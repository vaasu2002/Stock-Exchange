//
// Created by Vaasu Bisht on 19/10/25.
//

#pragma once

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <map>
#include <shared_mutex>
#include <future>
#include <variant>
#include "Worker/Task.h"
#include "Worker/Worker.h"

struct Worker;
class Order;


namespace Msg
{
 struct AddBook {std::string symbol;};
 struct NewOrder {std::string symbol; Order* order;};
 struct ControlStop {};
 using Msg = std::variant<AddBook,NewOrder,ControlStop>;
 }

/**
 * @class Scheduler
 * @brief Lightweight multithreaded task scheduler that manages multiple worker threads
 * and task queues.
 *
 * It provides a framework for async task execution across a configurable number of worker threads.
 * Each worker maintains its own task queue and continuously processes task in FIFO order.  Task can
 * be submitted to specific workers using `submitTo()` or with a returnable `std::future` via
 * `submitToWithFuture()`.
 */
class Scheduler
{
 // using WorkerMap = std::map<std::string, std::unique_ptr<Worker>>;

 // <====== Data Members ======>
 std::map<std::string, std::unique_ptr<Worker>> mWorkers; ///< Holds all the workers currently active by their id
 mutable  std::shared_mutex mLock; ///< Mutex for WorkerMap
 bool mShutdown{false}; ///> Indicates that all workers are shutdown

public:
 /* @brief Default constructor. Initializes an empty scheduler */
 Scheduler() = default;

 /* @brief Delete copy constructor to prevent copying of v instances. */
 Scheduler(const Scheduler&) = delete;

 /* @brief Delete copy constructor to prevent copying of v instances. */
 Scheduler& operator=(const Scheduler&) = delete;

 /* @brief Destructor that ensures all workers are stopped and joined */
 virtual ~Scheduler();

 /**
  * @brief Start all the workers
  */
 void start();

 /**
  * @brief Gracefully stops all workers and joins their threads.
  */
 void shutdown();

 /**
   * @brief  Creates and reserves a single worker with a unique identifier.
   * @param id The string identifier for the worker (e.g., "worker_1")
   * @throws std::runtime_error If a worker with the same id already exists.
   */
 void createWorker(const std::string& id);

 /**
  * @brief Reserves multiple workers using a naming prefix.
  * @param prefix The prefix for worker's names.
  * @param cnt The number of workers to create.
  *
  * @example
  * prefix = "workers", count = 3 -> creates 'worker_0', 'worker_1', 'worker_2'
  */
 void createWorkers(const std::string& prefix, size_t cnt);

 /**
  * @brief Submits a fire-and-forget task to a worker
  * @param wId Unique identified of workers
  * @param func Callable which the task will call
  * @param desc
  * @return Task Id
  */
 uint64_t submitTo(const std::string& wId, const TaskFn& func, const std::string& desc = "");

 /**
  * Helper method to create a task.
  * @param fn Callable which will be called during execution.
  * @param desc Description of task (optional)
  * @return
  */
 static Task makeTask(const TaskFn& fn, const std::string& desc = "");

 /**
  * @brief Locates the worker pointer
  * @param id Identifier of thread needed to be located
  * @return Worker pointer3
  */
 Worker* getWorker(const std::string& id)
 {
  std::shared_lock<std::shared_mutex> rlk(mLock); // read only lock
  const auto it = mWorkers.find(id);
   if(it==mWorkers.end())
   {
    throw std::runtime_error("Worker not found: " + id);
   }
   return it->second.get();
  }

 /**
  *
  * @tparam F The callable type (lambda, function, or functor).
  * @tparam Args The types of arguments to pass to the callable.
  * @param wid The identifier of the worker thread that should run the task
  * @param f The callable to execute asynchronously.
  * @param args Arguments to pass to the callable.
  * @return A future that can be used to retrieve the callable's
  * return value.
  */
 template<typename  F,typename ...Args>
 auto submitToWithFuture(const std::string& wid, F&& f, Args&&... args)
 ->std::future<std::invoke_result_t<F,Args...>>;

 /**
  * @brief Get all active worker's id.
  */
 std::vector<std::string> workerIds() const;

 bool hasWorker(const std::string& id) const;

};



#endif //SCHEDULER_H