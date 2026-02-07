/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#pragma once

#include <stdio.h>
#include <mutex>
#include <atomic>
#include <string>
#include <condition_variable>

const int FD_COUNT = 2;
#define QSH_POWER_WAKE_lOCK_ACQUIRE_INDEX 0
#define QSH_POWER_WAKE_lOCK_RELEASE_INDEX 1

const char * const PATHS[] = {
    /*to hold wakelock*/
    "/sys/power/wake_lock",
    /*to release wakelock*/
    "/sys/power/wake_unlock",
};

/*Singleton class which will interact with qshWakelock
 * to acuire / release the wakelock based on the number of
 * wakeup events processed from FMQ */
class qshWakelock {
public:
  /**
   * @brief Static function to get instance of qshWakelock with file name
   */
  static qshWakelock* getInstance(const char* lockName);

  /**
   * @brief Number of wakeup samples pushed to framework
   */
  int32_t acquire(unsigned int count);

  /**
   * @brief Number of wakeup samples processed by framework
   */
  int32_t release(unsigned int count);

private:
  qshWakelock(const char* lockName);
  ~qshWakelock();

  /* @brief acquire the wakelock
   * */
  int32_t acquire();

  /* @brief release the wakelock based on processed count
   * */
  int32_t release();

  /* @brief Instance to singleton class
   * */
  static qshWakelock* mSelf;

  /* @brief Total number of unprocessed wakeup samples by framework
   * */
  unsigned int  mAcqCount;
  std::mutex mMutex;
  std::condition_variable mCondition;
  int mFds[FD_COUNT];
  bool mIsHeld;
  std::string mQshWakelockName;
};
