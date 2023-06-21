/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <utils/Trace.h>
#include <utils/Log.h>
#include <dlfcn.h>
#include <vector>
#include "PerfBoost.h"
#include <string>

#define VENDOR_HINT_HEIF_DECODE_ENCODE_BOOST 0x000010A7

namespace android {

void HeifMPCtl::init() {
        std::string perfLibPath("libqti-perfd-client_system.so");

        if (!mPerfLibHandle &&
                (mPerfLibHandle = dlopen(perfLibPath.c_str(), RTLD_NOW)) == NULL) {
                ALOGE("Failed to open %s, Error: \"%s\"",
                        perfLibPath.c_str(), dlerror());
                return;
        }

        auto resetHandles = [this] () -> void {
                if (mPerfLibHandle) {
                        dlclose(mPerfLibHandle);
                }
                mPerfLibHandle = nullptr;
                mPerfLockAcquire = nullptr;
                mPerfLockRelease = nullptr;
                mPerfHint = nullptr;
                mPerfHintAcqRel = nullptr;
        };

        if (!mPerfLockAcquire &&
                (mPerfLockAcquire = (PerfLockAcquirePtr)dlsym(mPerfLibHandle,
                                                        "perf_lock_acq")) == NULL) {
                ALOGE("Could not find symbol \"perf_lock_acq\" in %s",
                        perfLibPath.c_str());
                resetHandles();
                return;
        }

        if (!mPerfLockRelease &&
                (mPerfLockRelease = (PerfLockReleasePtr)dlsym(mPerfLibHandle,
                                                        "perf_lock_rel")) == NULL) {
                ALOGE("Could not find symbol \"perf_lock_rel\" in %s",
                        perfLibPath.c_str());
                resetHandles();
                return;
        }

        if (!mPerfHint &&
                (mPerfHint = (PerfHintPtr)dlsym(mPerfLibHandle,
                                                    "perf_hint")) == NULL) {
                ALOGE("Could not find symbol \"perf_hint\" in %s",
                        perfLibPath.c_str());
                resetHandles();
                return;
        }

        if (!mPerfHintAcqRel &&
                (mPerfHintAcqRel = (PerfHintAcqRelPtr)dlsym(mPerfLibHandle,
                                                    "perf_hint_acq_rel")) == NULL) {
                ALOGE("Could not find symbol \"perf_hint_acq_rel\" in %s",
                        perfLibPath.c_str());
                resetHandles();
                return;
        }

    ALOGI("MP Control is enabled");
}

HeifMPCtl::HeifMPCtl() {
        init();
        if (mPerfLibHandle == nullptr) {
            ALOGW("%s: MP-CTL lib open or symbol load failed!", __func__);
        }
}

HeifMPCtl::~HeifMPCtl() {
        if (mPerfLibHandle) {
                dlclose(mPerfLibHandle);
        }
}

HeifPerfBoost::HeifPerfBoost(bool sync, int durationMs): mSync(sync) {
        if (HeifMPCtl::getMPCtlLibInstance().mPerfHint) {
            ALOGV("Requesting %s for duration [%u] ms", __func__, durationMs);
            mPerfLockHandle = HeifMPCtl::getMPCtlLibInstance().mPerfHint(
                    VENDOR_HINT_HEIF_DECODE_ENCODE_BOOST, nullptr, durationMs, 1);
            if (mPerfLockHandle < 0) {
                ALOGI("%s Perf lock acquistion failed [%u] ms", __func__, durationMs);
                mPerfLockHandle = 0;
            } else {
                ALOGI("%s Perf Lock acquired for duration [%u] ms for perf handle [%d]",
                        __func__, durationMs, mPerfLockHandle);
            }
        }
}

HeifPerfBoost::~HeifPerfBoost() {
        if (mSync) {
            if (HeifMPCtl::getMPCtlLibInstance().mPerfLockRelease) {
                if (mPerfLockHandle > 0) {
                    ALOGV("Requesting %s release", __func__);
                    if (0 == HeifMPCtl::getMPCtlLibInstance().mPerfLockRelease(mPerfLockHandle)) {
                        ALOGI("%s Perf Lock released successfully ", __func__);
                    } else {
                        ALOGW("%s Perf Lock release failed for perf handle %d",
                                __func__, mPerfLockHandle);
                    }
                    mPerfLockHandle = 0;
                }
            }
        }
}

} // namespace android