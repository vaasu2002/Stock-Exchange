//
// Created by Vaasu Bisht on 19/10/25.
// File contains: CancelToken, Task
//

#pragma once

#ifndef TASK_H
#define TASK_H
#include <cstdint>
#include <functional>
#include <memory>

// <================================ Cancel Token ================================>

struct CancelToken
{
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
    bool isCancelled() const noexcept
    {
        return cancelled && cancelled->load(std::memory_order_acquire);
    }

    void cancel() const noexcept
    {
        if(cancelled)
        {
            cancelled->store(true,std::memory_order_relaxed);
        }
    }
};


// <================================ Task ================================>


using TaskFn = std::function<void(const CancelToken&)>;

/**
 * @struct Task
 * @brief Task is a wrapper of all callable (function, lambda, functor). It is a functor
 * itself.
 */
struct Task
{
    uint64_t id; // unique identification
    TaskFn func;
    CancelToken token;
    std::string desc;

    void operator()() const
    {
        func(token);
    }
};

/**
 * @brief Global unique task id generator
 * @return unique task id
 */
inline uint64_t nextTaskId()
{
    static std::atomic<uint64_t> i{1};
    return i.fetch_add(1, std::memory_order_relaxed);
}

#endif //TASK_H