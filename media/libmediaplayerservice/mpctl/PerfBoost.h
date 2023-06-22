/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef _PERF_BOOST_H_
#define _PERF_BOOST_H_

namespace android {

/*
 * int perf_lock_acq(int handle, int duration, int list[], int numArgs)
 *
 * handle – Used to track each distinct request;
 * duration – Time to hold the lock in ms; 0 for indefinite time.
 * list – Array of resource opcodes and value pairs.
 * numArgs – Number of elements in the list array.
 *
 * Return: A nonzero integer as the handle on success. -1 on failure.
 */
typedef int (*PerfLockAcquirePtr)(int, int, int*, int);

/*
 * int perf_lock_rel(int handle)
 *
 * handle – Used to track each distinct request. The user passes in the same handle
 * that was returned by perf_lock_acq to properly release the lock.
 *
 * Return: 0 on success, -1 on failure.
 */
typedef int (*PerfLockReleasePtr)(int);

/*
 * int perf_hint(int hint_id, const char *pkg, int duration, int type)
 *
 * hint_id – Perf hint ID required
 * pkg – pkg name (optional).
 * duration – Time to hold the perf params, in ms; 0 for indefinite time.
 * type – Perf Hint type.
 *
 * Return: A nonzero integer as the handle on success. -1 on failure.
 */
typedef int (*PerfHintPtr)(int, const char*, int, int);

/*
 * int perf_hint_acq_rel(int, int, const char *, int, int, int, int[]);
 *
 * hint_id – Perf hint ID required
 * pkg – pkg name (optional).
 * duration – Time to hold the perf params, in ms; 0 for indefinite time.
 * type – Perf Hint type.
 * num_args - number of arguments following
 * args - list of params
 *
 * Return: 0 on success, -1 on failure.
 */
typedef int (*PerfHintAcqRelPtr)(int handle, int hint_id, const char *pkg, \
                int duration, int type, int num_args, int args[]);


// Singleton class to load perf lib and symbols
// To avoid removal singleton instance of the MP-CTL library by perf class wrapper
class HeifMPCtl {
public:
    // Get singletone instance of MP-CTL which remains for entire life-cycle
    static HeifMPCtl& getMPCtlLibInstance() {
        static HeifMPCtl mpCtlLib;
        return mpCtlLib;
    }

    HeifMPCtl(const HeifMPCtl &) = delete;
    HeifMPCtl & operator = (const HeifMPCtl &) = delete;

    PerfLockAcquirePtr mPerfLockAcquire = nullptr;
    PerfLockReleasePtr mPerfLockRelease = nullptr;
    PerfHintPtr mPerfHint = nullptr;
    PerfHintAcqRelPtr mPerfHintAcqRel = nullptr;

private:
    void *mPerfLibHandle = nullptr;

    HeifMPCtl();
    ~HeifMPCtl();
    void init(); // function to open lib and load perf lib symbols
};

class HeifPerfBoost {
private:
    int mPerfLockHandle;
    bool mSync; // true specifies if it was requested for synchronous behavior

public:
    // Ctor that takes resource values, sync behavior boolean and duration in milliseconds
    HeifPerfBoost(bool sync = true, int durationMs = 0);

    ~HeifPerfBoost();

    HeifPerfBoost(const HeifPerfBoost &) = delete;
    HeifPerfBoost & operator = (const HeifPerfBoost &) = delete;
};

};  // namespace android
#endif  // _PERF_BOOST_H_
