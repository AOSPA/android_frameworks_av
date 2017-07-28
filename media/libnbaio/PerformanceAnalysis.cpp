/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#define LOG_TAG "PerformanceAnalysis"
// #define LOG_NDEBUG 0

#include <algorithm>
#include <climits>
#include <deque>
#include <iostream>
#include <math.h>
#include <numeric>
#include <vector>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <new>
#include <audio_utils/roundup.h>
#include <media/nbaio/NBLog.h>
#include <media/nbaio/PerformanceAnalysis.h>
#include <media/nbaio/ReportPerformance.h>
#include <utils/Log.h>
#include <utils/String8.h>

#include <queue>
#include <utility>

namespace android {

namespace ReportPerformance {

PerformanceAnalysis::PerformanceAnalysis() {
    // These variables will be (FIXME) learned from the data
    kPeriodMs = 4; // typical buffer period (mode)
    // average number of Ms spent processing buffer
    kPeriodMsCPU = static_cast<int>(kPeriodMs * kRatio);
}

static int widthOf(int x) {
    int width = 0;
    while (x > 0) {
        ++width;
        x /= 10;
    }
    return width;
}

// Given a series of audio processing wakeup timestamps,
// buckets the time intervals into a histogram, searches for
// outliers, analyzes the outlier series for unexpectedly
// small or large values and stores these as peaks, and flushes
// the timestamp series from memory.
void PerformanceAnalysis::processAndFlushTimeStampSeries() {
    if (mTimeStampSeries.empty()) {
        ALOGD("Timestamp series is empty");
        return;
    }

    // mHists is empty if thread/hash pair is sending data for the first time
    if (mHists.empty()) {
        mHists.emplace_front(static_cast<uint64_t>(mTimeStampSeries[0]),
                            std::map<int, int>());
    }

    // 1) analyze the series to store all outliers and their exact timestamps:
    storeOutlierData(mTimeStampSeries);

    // 2) detect peaks in the outlier series
    detectPeaks();

    // if the current histogram has spanned its maximum time interval,
    // insert a new empty histogram to the front of mHists
    if (deltaMs(mHists[0].first, mTimeStampSeries[0]) >= kMaxLength.HistTimespanMs) {
        mHists.emplace_front(static_cast<uint64_t>(mTimeStampSeries[0]),
                             std::map<int, int>());
        // When memory is full, delete oldest histogram
        if (mHists.size() >= kMaxLength.Hists) {
            mHists.resize(kMaxLength.Hists);
        }
    }

    // 3) add current time intervals to histogram
    for (size_t i = 1; i < mTimeStampSeries.size(); ++i) {
        ++mHists[0].second[deltaMs(
                mTimeStampSeries[i - 1], mTimeStampSeries[i])];
    }

    // clear the timestamps
    mTimeStampSeries.clear();
}

// forces short-term histogram storage to avoid adding idle audio time interval
// to buffer period data
void PerformanceAnalysis::handleStateChange() {
    ALOGD("handleStateChange");
    processAndFlushTimeStampSeries();
    return;
}

// Takes a single buffer period timestamp entry information and stores it in a
// temporary series of timestamps. Once the series is full, the data is analyzed,
// stored, and emptied.
void PerformanceAnalysis::logTsEntry(int64_t ts) {
    // TODO might want to filter excessively high outliers, which are usually caused
    // by the thread being inactive.
    // Store time series data for each reader in order to bucket it once there
    // is enough data. Then, write to recentHists as a histogram.
    mTimeStampSeries.push_back(ts);
    // if length of the time series has reached kShortHistSize samples,
    // analyze the data and flush the timestamp series from memory
    if (mTimeStampSeries.size() >= kMaxLength.TimeStamps) {
        processAndFlushTimeStampSeries();
    }
}

// Given a series of outlier intervals (mOutlier data),
// looks for changes in distribution (peaks), which can be either positive or negative.
// The function sets the mean to the starting value and sigma to 0, and updates
// them as long as no peak is detected. When a value is more than 'threshold'
// standard deviations from the mean, a peak is detected and the mean and sigma
// are set to the peak value and 0.
void PerformanceAnalysis::detectPeaks() {
    if (mOutlierData.empty()) {
        return;
    }

    // compute mean of the distribution. Used to check whether a value is large
    const double kTypicalDiff = std::accumulate(
        mOutlierData.begin(), mOutlierData.end(), 0,
        [](auto &a, auto &b){return a + b.first;}) / mOutlierData.size();
    // ALOGD("typicalDiff %f", kTypicalDiff);

    // iterator at the beginning of a sequence, or updated to the most recent peak
    std::deque<std::pair<uint64_t, uint64_t>>::iterator start = mOutlierData.begin();
    // the mean and standard deviation are updated every time a peak is detected
    // initialize first time. The mean from the previous sequence is stored
    // for the next sequence. Here, they are initialized for the first time.
    if (mOutlierDistribution.Mean < 0) {
        mOutlierDistribution.Mean = static_cast<double>(start->first);
        mOutlierDistribution.Sd = 0;
    }
    auto sqr = [](auto x){ return x * x; };
    for (auto it = mOutlierData.begin(); it != mOutlierData.end(); ++it) {
        // no surprise occurred:
        // the new element is a small number of standard deviations from the mean
        if ((fabs(it->first - mOutlierDistribution.Mean) <
             mOutlierDistribution.kMaxDeviation * mOutlierDistribution.Sd) ||
             // or: right after peak has been detected, the delta is smaller than average
            (mOutlierDistribution.Sd == 0 &&
                     fabs(it->first - mOutlierDistribution.Mean) < kTypicalDiff)) {
            // update the mean and sd:
            // count number of elements (distance between start interator and current)
            const int kN = std::distance(start, it) + 1;
            // usual formulas for mean and sd
            mOutlierDistribution.Mean = std::accumulate(start, it + 1, 0.0,
                                   [](auto &a, auto &b){return a + b.first;}) / kN;
            mOutlierDistribution.Sd = sqrt(std::accumulate(start, it + 1, 0.0,
                    [=](auto &a, auto &b){
                    return a + sqr(b.first - mOutlierDistribution.Mean);})) /
                    ((kN > 1)? kN - 1 : kN); // kN - 1: mean is correlated with variance
        }
        // surprising value: store peak timestamp and reset mean, sd, and start iterator
        else {
            mPeakTimestamps.emplace_front(it->second);
            // TODO: turn this into a circular buffer
            if (mPeakTimestamps.size() >= kMaxLength.Peaks) {
                mPeakTimestamps.resize(kMaxLength.Peaks);
            }
            mOutlierDistribution.Mean = static_cast<double>(it->first);
            mOutlierDistribution.Sd = 0;
            start = it;
        }
    }
    return;
}

// Called by LogTsEntry. The input is a vector of timestamps.
// Finds outliers and writes to mOutlierdata.
// Each value in mOutlierdata consists of: <outlier timestamp,
// time elapsed since previous outlier>.
// e.g. timestamps (ms) 1, 4, 5, 16, 18, 28 will produce pairs (4, 5), (13, 18).
// This function is applied to the time series before it is converted into a histogram.
void PerformanceAnalysis::storeOutlierData(const std::vector<int64_t> &timestamps) {
    if (timestamps.size() < 1) {
        return;
    }
    // first pass: need to initialize
    if (mOutlierDistribution.Elapsed == 0) {
        mOutlierDistribution.PrevNs = timestamps[0];
    }
    for (const auto &ts: timestamps) {
        const uint64_t diffMs = static_cast<uint64_t>(deltaMs(mOutlierDistribution.PrevNs, ts));
        if (diffMs >= static_cast<uint64_t>(kOutlierMs)) {
            mOutlierData.emplace_front(mOutlierDistribution.Elapsed,
                                      static_cast<uint64_t>(mOutlierDistribution.PrevNs));
            // Remove oldest value if the vector is full
            // TODO: remove pop_front once circular buffer is in place
            // FIXME: make sure kShortHistSize is large enough that that data will never be lost
            // before being written to file or to a FIFO
            if (mOutlierData.size() >= kMaxLength.Outliers) {
                mOutlierData.resize(kMaxLength.Outliers);
            }
            mOutlierDistribution.Elapsed = 0;
        }
        mOutlierDistribution.Elapsed += diffMs;
        mOutlierDistribution.PrevNs = ts;
    }
}

// TODO Make it return a std::string instead of modifying body --> is this still relevant?
// TODO consider changing all ints to uint32_t or uint64_t
// TODO: move this to ReportPerformance, probably make it a friend function of PerformanceAnalysis
void PerformanceAnalysis::reportPerformance(String8 *body, int maxHeight) {
    // Add any new data
    processAndFlushTimeStampSeries();

    if (mHists.empty()) {
        ALOGD("reportPerformance: mHists is empty");
        return;
    }
    ALOGD("reportPerformance: hists size %d", static_cast<int>(mHists.size()));
    // TODO: more elaborate data analysis
    std::map<int, int> buckets;
    for (const auto &shortHist: mHists) {
        for (const auto &countPair : shortHist.second) {
            buckets[countPair.first] += countPair.second;
        }
    }

    // underscores and spaces length corresponds to maximum width of histogram
    static const int kLen = 40;
    std::string underscores(kLen, '_');
    std::string spaces(kLen, ' ');

    auto it = buckets.begin();
    int maxDelta = it->first;
    int maxCount = it->second;
    // Compute maximum values
    while (++it != buckets.end()) {
        if (it->first > maxDelta) {
            maxDelta = it->first;
        }
        if (it->second > maxCount) {
            maxCount = it->second;
        }
    }
    int height = log2(maxCount) + 1; // maxCount > 0, safe to call log2
    const int leftPadding = widthOf(1 << height);
    const int colWidth = std::max(std::max(widthOf(maxDelta) + 1, 3), leftPadding + 2);
    int scalingFactor = 1;
    // scale data if it exceeds maximum height
    if (height > maxHeight) {
        scalingFactor = (height + maxHeight) / maxHeight;
        height /= scalingFactor;
    }
    // TODO: print reader (author) ID
    body->appendFormat("\n%*s", leftPadding + 11, "Occurrences");
    // write histogram label line with bucket values
    body->appendFormat("\n%s", " ");
    body->appendFormat("%*s", leftPadding, " ");
    for (auto const &x : buckets) {
        body->appendFormat("%*d", colWidth, x.second);
    }
    // write histogram ascii art
    body->appendFormat("\n%s", " ");
    for (int row = height * scalingFactor; row >= 0; row -= scalingFactor) {
        const int value = 1 << row;
        body->appendFormat("%.*s", leftPadding, spaces.c_str());
        for (auto const &x : buckets) {
          body->appendFormat("%.*s%s", colWidth - 1,
                             spaces.c_str(), x.second < value ? " " : "|");
        }
        body->appendFormat("\n%s", " ");
    }
    // print x-axis
    const int columns = static_cast<int>(buckets.size());
    body->appendFormat("%*c", leftPadding, ' ');
    body->appendFormat("%.*s", (columns + 1) * colWidth, underscores.c_str());
    body->appendFormat("\n%s", " ");

    // write footer with bucket labels
    body->appendFormat("%*s", leftPadding, " ");
    for (auto const &x : buckets) {
        body->appendFormat("%*d", colWidth, x.first);
    }
    body->appendFormat("%.*s%s", colWidth, spaces.c_str(), "ms\n");

    // Now report glitches
    body->appendFormat("\ntime elapsed between glitches and glitch timestamps\n");
    for (const auto &outlier: mOutlierData) {
        body->appendFormat("%lld: %lld\n", static_cast<long long>(outlier.first),
                           static_cast<long long>(outlier.second));
    }

}

// TODO: decide whether to use this or whether it is overkill, and it is enough
// to only treat as glitches single wakeup call intervals which are too long.
// Ultimately, glitch detection will be directly on the audio signal.
// Produces a log warning if the timing of recent buffer periods caused a glitch
// Computes sum of running window of three buffer periods
// Checks whether the buffer periods leave enough CPU time for the next one
// e.g. if a buffer period is expected to be 4 ms and a buffer requires 3 ms of CPU time,
// here are some glitch cases:
// 4 + 4 + 6 ; 5 + 4 + 5; 2 + 2 + 10
void PerformanceAnalysis::alertIfGlitch(const std::vector<int64_t> &samples) {
    std::deque<int> periods(kNumBuff, kPeriodMs);
    for (size_t i = 2; i < samples.size(); ++i) { // skip first time entry
        periods.push_front(deltaMs(samples[i - 1], samples[i]));
        periods.pop_back();
        // TODO: check that all glitch cases are covered
        if (std::accumulate(periods.begin(), periods.end(), 0) > kNumBuff * kPeriodMs +
            kPeriodMs - kPeriodMsCPU) {
                ALOGW("A glitch occurred");
                periods.assign(kNumBuff, kPeriodMs);
        }
    }
    return;
}

//------------------------------------------------------------------------------

// writes summary of performance into specified file descriptor
void dump(int fd, int indent, PerformanceAnalysisMap &threadPerformanceAnalysis) {
    String8 body;
    const char* const kDirectory = "/data/misc/audioserver/";
    for (auto & thread : threadPerformanceAnalysis) {
        for (auto & hash: thread.second) {
            PerformanceAnalysis& curr = hash.second;
            curr.processAndFlushTimeStampSeries();
            // write performance data to console
            curr.reportPerformance(&body);
            // write to file
            writeToFile(curr.mHists, curr.mOutlierData, curr.mPeakTimestamps,
                        kDirectory, false, thread.first, hash.first);
        }
    }
    if (!body.isEmpty()) {
        dumpLine(fd, indent, body);
        body.clear();
    }
}

// Writes a string into specified file descriptor
void dumpLine(int fd, int indent, const String8 &body) {
    dprintf(fd, "%.*s%s \n", indent, "", body.string());
}

} // namespace ReportPerformance

}   // namespace android