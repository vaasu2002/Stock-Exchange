//
// Created by Vaasu Bisht on 19/10/25.
//

#include "Scheduler.h"

#include "Worker/Worker.h"
#include <iostream>

Scheduler::~Scheduler()
{
    shutdown();
}


void Scheduler::createWorker(const std::string& id)
{
    std::unique_lock wlk(mLock);

    // Check if worker with id already exists.
    if(mWorkers.contains(id))
    {
        throw std::runtime_error("Worker: "+id+" already exists");
    }
    mWorkers.emplace(id, std::make_unique<Worker>(id));
}

void Scheduler::createWorkers(const std::string& prefix, const size_t cnt)
{
    {
        std::shared_lock<std::shared_mutex> rlk(mLock);
        if(mWorkers.size())
        {
            mWorkers.clear();
        }
    }
    for(size_t i = 0 ; i < cnt ; i++)
    {
        createWorker(prefix+"_"+std::to_string(i));
    }
}

void Scheduler::start()
{
    std::unique_lock wlk(mLock);
    for (auto& [_, worker] : mWorkers)
    {
        worker->start();
    }
}

void Scheduler::shutdown()
{
    {
        std::unique_lock<std::shared_mutex> wlk(mLock);
        if(mShutdown)
        {
            return;
        }
        mShutdown = true;
        for(auto& [_,worker]:mWorkers)
        {
            worker->postStop();
        }
    }
    for(auto& [_,worker]:mWorkers)
    {
        worker->join();
    }
    std::unique_lock<std::shared_mutex> wlk(mLock);
    mWorkers.clear();
}

uint64_t Scheduler::submitTo(const std::string& wId, const TaskFn& func, const std::string& desc)
{
    const auto t = makeTask(func,desc);
    auto* w = getWorker(wId);
    w->postTask(t);
    return t.id;
}

Task Scheduler::makeTask(const TaskFn& fn, const std::string& desc)
{
    Task t;
    t.id = nextTaskId();
    t.func = fn;
    t.token = CancelToken();
    t.desc = desc;
    return t;
}

template <typename F, typename... Args>
auto Scheduler::submitToWithFuture(const std::string& wid, F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using R = std::invoke_result_t<F, Args...>; // Result type
    auto pkg = std::make_shared<std::promise<R>>();
    auto fut = pkg->get_future();

    auto wrapper = [pkg, fn = std::forward<F>(f), tup = std::make_tuple(std::forward<Args>(args)...)]
    (const CancelToken& token) mutable
    {
        // Optionally check token before running
        if(token.isCancelled())
        {
            // For promise types, set an exception or set_defaults
            try
            {
                throw std::runtime_error("Task cancelled");
            }
            catch (...)
            {
                pkg->set_exception(std::current_exception());
            }
            return;
        }
        try
        {
            if constexpr (std::is_void_v<R>)
            {
                std::apply(fn, tup);
                pkg->set_value();
            }
            else
            {
                R r = std::apply(fn, tup);
                pkg->set_value(std::move(r));
            }
        }
        catch (...)
        {
            pkg->set_exceeption(std::current_exception());
        }
    };
    auto t = makeTask(std::move(wrapper),"future_task");
    getWorker(wid)->postTask(t);
    return fut;
}

std::vector<std::string> Scheduler::workerIds() const
{
    std::vector<std::string> ids;
    std::shared_lock<std::shared_mutex> rlk(mLock);
    ids.reserve(mWorkers.size());
    for(auto const& [id,_] : mWorkers)
    {
        ids.push_back(id);
    }
    return ids;
}

bool Scheduler::hasWorker(const std::string& id) const
{
    std::shared_lock<std::shared_mutex> rlk(mLock);
    return mWorkers.count(id) != 0 ;
}