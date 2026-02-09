/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once
#include <atomic>
#include <thread>
#include <atomic>
#include <queue>
#include <functional>
#include <memory>
#include <string.h>
#include <mutex>
#include <condition_variable>
#ifdef USE_GLIB
#include <glib.h>
#define strlcpy g_strlcpy
#endif
#include "qshLog.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#include <condition_variable>

#define UNUSED(x) (void)(x)
/**
 * type alias for worker's task function
 */
using workerTask = std::function<void()>;

/**
 * Implementation of a worker thread with its own task-queue.
 * Worker thread starts running when constructed and stops when
 * destroyed. Tasks can be assigned to the worker
 * asynchronously and they will be performed in order.
 */
class qshWorker
{
public:
    /**
     * @brief creates a worker thread and starts processing tasks
     */
    qshWorker(): mAlive(true)
    {
        mThread = std::thread([this] { run(); });
    }

    /**
     * @brief terminates the worker thread, waits until all
     *        outstanding tasks are finished.
     *
     * @note this destructor should never be called from the worker
     *       thread itself.
     */
    ~qshWorker()
    {
        std::unique_lock<std::mutex> lk(mMutex);
        mAlive = false;
        mConditionVar.notify_one();
        lk.unlock();
        mThread.join();
        size_t num_of_task = mTaskQueue.size();
    }

    void setName(const char *name) {
#ifndef _WIN32
        pthread_setname_np(mThread.native_handle() , name);
#endif
    }

     /**
     * @brief add a new task for the worker to do
     *
     * Tasks are performed in order in which they are added
     *
     * @param task task to perform
     */
    void addTask(const workerTask& task)
    {
        std::lock_guard<std::mutex> lk(mMutex);
        try {
            mTaskQueue.push(task);
        } catch (std::exception& e) {
            sns_loge("failed to add new task, %s", e.what());
            return;
        }
        mConditionVar.notify_one();
    }

private:

    /* worker thread's mainloop */
    void run()
    {
        while (mAlive) {
            std::unique_lock<std::mutex> lk(mMutex);
            while (mTaskQueue.empty() && mAlive) {
                mConditionVar.wait(lk);
            }
            if (mAlive && !mTaskQueue.empty()) {
                auto task = std::move(mTaskQueue.front());
                mTaskQueue.pop();
                lk.unlock();
                try {
                    if(nullptr != task) {
                        task();
                    }
                } catch (const std::exception& e) {
                    /* if an unhandled exception happened when running
                       the task, just log it and move on */
                    sns_loge("task failed, %s", e.what());
                }
                lk.lock();
            }
            lk.unlock();
        }
    }

    std::atomic<bool> mAlive;
    std::queue<workerTask> mTaskQueue;
    std::mutex mMutex;
    std::condition_variable mConditionVar;
    std::thread mThread;
    /* flag to make the worker thread prevent target to go to suspend */
};
