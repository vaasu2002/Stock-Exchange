//
// Created by Vaasu Bisht on 19/10/25.
//

#pragma once

#ifndef WORKER_H
#define WORKER_H

#include<queue>
#include <thread>
#include <unordered_set>

#include "Task.h"
#include "../Scheduler.h"

struct Task;

/**
 * @struct  Worker
 * @brief Internal class representing a single worker thread. This class is owner
 * of thread.
 *
 * Each worker has its own task queue and continuously processes tasks until explicitly
 * stopped. SSynchronization is managed via mutex and conditional variables.
 *
 * @details
 * `threadMutex` protects ownership changes of the std::thread object (start/join/move/destruction).
 * Without this, there can be data races — e.g., one thread calling start() while another calls join().
 */
struct Worker {
 using Id = std::string;
 using TokenMap = std::unordered_map<uint64_t, std::shared_ptr<std::atomic<bool>>>;
 using UnorderedTaskIdSet = std::unordered_set<uint64_t>;
    std::string mId;
    std::queue<Task> mQueue;
    std::mutex mQueueMutex;
    std::mutex mThreadMutex;
    std::condition_variable mCv;
    std::thread mThread;
    TokenMap mTokenMap; /// > Tasks which are cancelable, thought a cancel token.
 UnorderedTaskIdSet mPendingTasks; /// > Tasks that are pending (queued but not started).
 UnorderedTaskIdSet mRunningTasks; /// > Tasks that are currently running.


    bool mStop;
    /** @brief Constructor */
    explicit  Worker(const std::string& id);

    /**
     * @brief  Responsible for creating and launching work's dedicated thread
     *
     * @details
     * Has a dedicated thread-mutex as multiple threads in the system could
     * potentially call start()/shutdown() on the same worker concurrently.
     */
    void start();
    void shutdown();

    /**
     * @brief Worker's main loop.
     *
     * Continuously waits for a task to be enqueued and executes them one by one. The loop exists when
     * `mStop` is set and all pending tasks are processed.
     * @details
     * Conditional variable is used so thread avoids busy-waiting. Thread sleep until there is task
     * available to be executed.
     * Scheduler APIs like postTask(), tryRemoveOrCancel() etc. writes into the worker's queue while run()
     * reads from it. So queue is used by multiple thread that's why there is need of mQueueMutex.
     */
    void run();

 /**
  * @brief  Add new work (task) for the worker thread
  *
  * Locks the worker's task queue, add a Task object, and then releases the lock and wakes up the
  * sleeping thread(worker). The worker thread wakes up, pop the new task and runs it.
  * @param t
  */
 void postTask(const Task& t);

 /**
  * @brief Signals the worker to exit gracefully.
  *
  * Request that the worker thread finishes all pending work and then terminates clearly.
  */
 void postStop();

 /**
  * @brief
  * To optimize it's better to release the lock before actually calling .join() as std::thread::join()
  * is a blocking call — it can take a long time (the worker might still be finishing its run loop). Holding
  * a mutex while doing a blocking operation — that would block other threads (e.g. start(), shutdown())
  * from making progress. After std::move, this->thread becomes empty (non-joinable). The lock is then released.
  * Other code can now safely call start() or inspect state without deadlock risk.
  */
 void join() noexcept;
};



#endif //WORKER_H