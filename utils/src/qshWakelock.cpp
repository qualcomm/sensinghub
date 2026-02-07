/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>        // EXIT_SUCCESS/EXIT_FAILURE
#include <time.h>
#include <cerrno>
#include "qshWakelock.h"
#include "qshLog.h"

qshWakelock* qshWakelock::mSelf= nullptr;

qshWakelock* qshWakelock::getInstance(const char* lockName)
{
  sns_logi("qshWakelock::getInstance");
  if(nullptr != mSelf) {
    sns_logi("return an exsiting lock: %s", mSelf->mQshWakelockName.c_str());
    return mSelf;
  } else {
    if (nullptr == lockName) {
      sns_loge("input lock name is null! Will use the default name.");
      return nullptr;
    }
    mSelf = new qshWakelock(lockName);
    if (nullptr == mSelf) {
        sns_loge("create unique lock instance failed!");
        return nullptr;
    }
    return mSelf;
  }
}

qshWakelock::qshWakelock(const char* lockName)
{
  mIsHeld = false;
  mAcqCount = 0;
  for (int i=0; i<FD_COUNT; i++) {
    mFds[i] = -1;
  }

  for (int i=0; i<FD_COUNT; i++) {
    int fd = open(PATHS[i], O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      sns_loge("fatal error opening \"%s\": %s\n", PATHS[i], strerror(errno));
      // Clean up previously opened fds
      for (int j=0; j<i; j++) {
        if (mFds[j] >= 0) {
          close(mFds[j]);
          mFds[j] = -1;
        }
      }
      return;
    }
    mFds[i] = fd;
  }
  mQshWakelockName = lockName;
}

qshWakelock::~qshWakelock()
{
  std::lock_guard<std::mutex> lock(mMutex);
  if(true == mIsHeld) {
    mAcqCount = 0;
    release();
  }

  for (int i=0; i<FD_COUNT; i++) {
    if (mFds[i] >= 0)
      close(mFds[i]);
  }
}

int32_t qshWakelock::acquire(unsigned int inCount)
{
  int32_t ret = -1;
  sns_logi("qshWakelock::acquire %d", inCount);
  std::lock_guard<std::mutex> lock(mMutex);
  if (inCount > std::numeric_limits<unsigned int>::max() - mAcqCount) {
    sns_loge("_in_count %d will overflow max of mAcqCount %u", inCount, mAcqCount);
    return ret;
  }
  if(mAcqCount == 0 && false == mIsHeld) {
    ret = acquire();
    if (ret != 0) {
      sns_loge("fail to acquire wakelock");
      return ret;
    }
  }
  mAcqCount = mAcqCount + inCount;
  sns_logi("number of acquired wakelock %d", mAcqCount);
  return ret;
}

int32_t qshWakelock::release(unsigned int inCount)
{
  int32_t ret = -1;
  sns_logi("qshWakelock::release %d", inCount);
  std::lock_guard<std::mutex> lock(mMutex);
  if (inCount > mAcqCount) {
      sns_loge("_in_count %d is larger than mAcqCount %u", inCount, mAcqCount);
      return ret;
  }
  mAcqCount = mAcqCount - inCount;
  if(mAcqCount == 0 && true ==mIsHeld) {
    ret = release();
    if (ret != 0) {
      sns_loge("fail to release wakelock");
    }
  } else {
    sns_logi("number of remaining wakelock %d", mAcqCount);
  }
  return ret;
}

int32_t qshWakelock::acquire()
{
  int32_t ret = -1;
  if (mFds[QSH_POWER_WAKE_lOCK_ACQUIRE_INDEX] >= 0) {
    if (write( mFds[QSH_POWER_WAKE_lOCK_ACQUIRE_INDEX], mQshWakelockName.c_str(), mQshWakelockName.length()+1) > 0) {
      sns_logv("sucess wakelock acquire:%s", mQshWakelockName.c_str());
      mIsHeld = true;
      mCondition.notify_one();
      ret = 0;
    } else {
      sns_loge("write fail wakelock acquire:%s", mQshWakelockName.c_str());
    }
  } else {
    sns_loge("Not able to open fd for wakeup:%s", mQshWakelockName.c_str());
  }
  return ret;
}

int32_t qshWakelock::release()
{
  int32_t ret = -1;
  if (mFds[QSH_POWER_WAKE_lOCK_RELEASE_INDEX] >= 0) {
    if (write( mFds[QSH_POWER_WAKE_lOCK_RELEASE_INDEX],mQshWakelockName.c_str(), mQshWakelockName.length()+1) > 0) {
      sns_logv("sucess release %s wakelock", mQshWakelockName.c_str());
      mIsHeld = false;
      ret = 0;
    } else {
      sns_loge("failed in write:%s wake unlock", mQshWakelockName.c_str());
    }
  } else {
    sns_loge("Not able to open fd for wakeup:%s", mQshWakelockName.c_str());
  }
  return ret;
}

