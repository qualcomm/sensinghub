/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once
/* This is platform specific code, so make sure that we are compiling
   for correct architecture */
#if defined(__aarch64__)
#define TARGET_ARM64
#elif defined(__arm__)
#define TARGET_ARM
#else
#error "target cpu architecture not supported"
#endif
#include <cstdint>
#include <cinttypes>
#include <atomic>
#include <limits>
#ifdef __ANDROID_API__
#include "utils/SystemClock.h"
#else
#include <time.h>
#endif
#include "qshLog.h"

/**
 * @brief Sensors time utilities
 *
 * Provides utility functions for
 *  - reading QTimer ticks/frequency and timestamps
 *  - converting QTimer timestamps to android system clock
 *    timestamps and vice versa.
 */
class qshTimeUtil
{
public:

    /**
     * @brief get singleton instance of the timeutil class
     * @return qshTimeUtil&
     */
    static qshTimeUtil& getInstance()
    {
        static qshTimeUtil inst;
        return inst;
    }

    /**
     * @brief reads the current QTimer count value
     * @return uint64_t QTimer tick-count
     */
    uint64_t qtimerGetTicks()
    {
    #if defined(TARGET_ARM64)
        unsigned long long val = 0;
        asm volatile("mrs %0, cntvct_el0" : "=r" (val));
        return val;
    #else
        uint64_t val;
        unsigned long lsb = 0, msb = 0;
        asm volatile("mrrc p15, 1, %[lsb], %[msb], c14"
                     : [lsb] "=r" (lsb), [msb] "=r" (msb));
        val = ((uint64_t)msb << 32) | lsb;
        return val;
    #endif
    }

    /**
     * @brief get QTimer frequency in Hz
     * @return uint64_t
     */
    uint64_t qtimerGetFreq()
    {
    #if defined(TARGET_ARM64)
        uint64_t val = 0;
        asm volatile("mrs %0, cntfrq_el0" : "=r" (val));
        return val;
    #else
        uint32_t val = 0;
        asm volatile("mrc p15, 0, %[val], c14, c0, 0" : [val] "=r" (val));
        return val;
    #endif
    }

    /**
     * @brief get time in nanoseconds since boot-up
     * @return uint64_t nanoseconds
     */
    uint64_t qtimerGetTimeNs()
    {
        return qtimerTicksToNs(qtimerGetTicks());
    }

    /**
     * @brief convert the qtimer tick value of nanoseconds
     * @param ticks
     * @return uint64_t qtimer time in nanoseconds
     */
    uint64_t qtimerTicksToNs(uint64_t ticks)
    {
        return uint64_t(double(ticks) * (double(NSEC_PER_SEC) / double(mQtimerFreq)));
    }

    /**
     * @brief convert qtimer timestamp (in ns) to android system
     *        time (in ns)
     * @param qtimer_ts
     * @return int64_t
     */
    int64_t qtimerNsToElapsedRealtimeNano(uint64_t qtimerTS)
    {
        int64_t realtimeNs = int64_t(qtimerTS) + mOffsetNs;
        if (updateQtimerToRealtimeOffset(realtimeNs)) {
            realtimeNs = int64_t(qtimerTS) + mOffsetNs;
        }
        return realtimeNs;
    }

    /**
     * @brief convert android system time (in ns) to qtimer
     *        timestamp (in ns)
     * @param androidTS
     * @return uint64_t
     */
    uint64_t elapsedRealtimeNanoToQtimerNs(int64_t androidTS)
    {
        updateQtimerToRealtimeOffset(androidTS);
        return (androidTS - mOffsetNs);
    }

    /**
     * @brief convert qtimer timestamp (in ticks) to android
     *        timestamp (in ns)
     * @param qtimer_ticks
     * @return int64_t
     */
    int64_t qtimerTicksToElapsedRealtimeNano(uint64_t qtimerTicks)
    {
        return qtimerNsToElapsedRealtimeNano(qtimerTicksToNs(qtimerTicks));
    }
    /**
     * @brief get offset between sensor time system and Android time system (in ns)
     * @return int64_t
     */
    int64_t getElapsedRealtimeNanoOffset()
    {
        return mOffsetNs;
    }

    /**
     * @brief kick in the logic to recalculate offset for drift
     * @param force_update if true, always update the offset (even if the
     *       previous update was more recent than the last update)
     * @return true if offset changed, false otherwise

     */

    bool recalculateOffset(bool forceUpdate)
    {
#ifdef __ANDROID_API__
       return updateQtimerToRealtimeOffset(android::elapsedRealtimeNano(), forceUpdate);
#else
       struct timespec timeElapsed;
       clock_gettime(CLOCK_REALTIME, &timeElapsed);
       return updateQtimerToRealtimeOffset((uint64_t)(timeElapsed.tv_sec*NSEC_PER_SEC+timeElapsed.tv_nsec), forceUpdate);
#endif
    }

    uint64_t getOffsetUpdateScheduleNs() const
    {
       return OFFSET_UPDATE_SCHEDULE_NS;
    }

private:
    /**
     * @brief update offset for qtimer timestamp (in ticks) to android
     *        timestamp (in ns)
     * @param androidTS realtime timestamp (in ns)
     * @param force if true, recalculate offset even if last update was more
     *        recent than the usual update schedule
     * @return bool true if offset is changed, flase if offset is not changed
     */
    bool updateQtimerToRealtimeOffset(int64_t androidTS, bool force=false)
    {
        bool changed = false;
        int64_t lastAndroidTS = mLastRealtimeNs;

        if (androidTS > 0 &&
            (force ||
            androidTS < lastAndroidTS ||
            androidTS - lastAndroidTS >= OFFSET_UPDATE_SCHEDULE_NS)) {
            uint64_t ns = 0, ns_end = 0;
            int iter = 0;
            int64_t oldmOffsetNs = mOffsetNs;
            do {
                ns = qtimerGetTimeNs();
#ifdef __ANDROID_API__
                int64_t curr_androidTS = android::elapsedRealtimeNano();
#else
                struct timespec _curr_androidTS;
                clock_gettime(CLOCK_REALTIME, &_curr_androidTS);
                int64_t curr_androidTS = (int64_t)(_curr_androidTS.tv_sec*NSEC_PER_SEC +_curr_androidTS.tv_nsec);
#endif
                if (ns <= std::numeric_limits<int64_t>::max() && curr_androidTS >= 0) {
                    mOffsetNs = curr_androidTS - int64_t(ns);
                } else {
                    sns_loge("invalid time: ts = %" PRIu64 " curr_androidTS = %" PRId64,
                        ns, curr_androidTS);
                }
                ns_end = qtimerGetTimeNs();
                iter++;
            } while ((ns_end - ns > QTIMER_GAP_THRESHOLD_NS) &&
                        (iter < QTIMER_GAP_MAX_ITERATION));
            mLastRealtimeNs = androidTS;
            sns_logd("updating qtimer-realtime offset, offset_diff = %" PRId64 \
                     " time_diff = %" PRId64, mOffsetNs - oldmOffsetNs,
                     androidTS - lastAndroidTS);
            changed = true;
        }
        return changed;
    }

    qshTimeUtil() :
        mQtimerFreq(qtimerGetFreq())
    {
#ifdef __ANDROID_API__
        mLastRealtimeNs = android::elapsedRealtimeNano();
#else
        struct timespec time_elapsed;
        clock_gettime(CLOCK_REALTIME, &time_elapsed);
        mLastRealtimeNs = (int64_t)(time_elapsed.tv_sec*NSEC_PER_SEC+time_elapsed.tv_nsec);
#endif
        mOffsetNs = mLastRealtimeNs - int64_t(qtimerGetTimeNs());
    }
    ~qshTimeUtil() = default;
    /* delete copy constructors/assignment for singleton operation */
    qshTimeUtil(const qshTimeUtil&) = delete;
    qshTimeUtil& operator=(const qshTimeUtil&) = delete;

    /* QTimer frequency in Hz */
    const uint64_t mQtimerFreq;

    /* constant offset between the android elapsedRealTimeNano clock
       and QTimer clock in nanoseconds */
    // PEND: Move this variable back into the private section (see PEND below)
//    const int64_t mOffsetNs;

    const uint64_t NSEC_PER_SEC = 1000000000ull;

    /* the last realtime timestamp converted or passed */
    std::atomic<std::int64_t> mLastRealtimeNs;
    /* how often offset needs to be updated */
    const int64_t OFFSET_UPDATE_SCHEDULE_NS = 60000000000ll;
    /* THRESHOLD to get minimum offset */
    const uint64_t QTIMER_GAP_THRESHOLD_NS = 10000;
    /* Max Interation to get minimum offset */
    const int QTIMER_GAP_MAX_ITERATION = 20;

public:
    // For some unknown reason, the Android and QTimer timestamps used to
    // calculate the mOffsetNs only make sense if mOffsetNs is a public
    // variable. I suspect that our method for reading the QTimer value may
    // have some unaccounted caveats.
    // PEND: Make this variable private again once we fix the root-cause for
    // the invalid QTimer values read during bootup
    std::atomic<std::int64_t> mOffsetNs;
};
