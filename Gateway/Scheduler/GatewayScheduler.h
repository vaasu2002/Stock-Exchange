#pragma once

#include "Scheduler.h"
#include "String.h"
#include "Network/TcpEpollListener.h"
#include "FixMessageDispatcher.h"

namespace Exchange::Gateway {

    enum Threads {
        Listener,
        Dispatcher
    };

    class GatewayScheduler : public Scheduler {
        Core::String mWorkerPrefix;
        size_t mWorkerCnt;
        std::map<Threads, std::string> mThreads;
        std::shared_ptr<std::atomic<bool>> mStopNetwork;

    public:
        GatewayScheduler(const Core::String prefix) : mWorkerPrefix(std::move(prefix)),
            mStopNetwork(std::make_shared<std::atomic<bool>>(false))
        {
            mThreads.insert({Threads::Listener, (mWorkerPrefix + "_listener").toString()});
            mThreads.insert({Threads::Dispatcher, (mWorkerPrefix + "_dispatcher").toString()});

            createWorker(mThreads[Threads::Listener]);
            createWorker(mThreads[Threads::Dispatcher]);
        }

        void start(Network::TcpEpollListener& listener, Exchange::Gateway::FixMessageDispatcher& dispatcher) {
            LOG_INFO("Starting Gateway Scheduler workers...");
            Scheduler::start();

            auto stopNet = mStopNetwork;
            
            submitTo(
                mThreads[Threads::Listener], 
                [stopNet, &listener](const CancelToken& token) {
                    listener.run(stopNet.get());
                }, 
                "This thread listens to network request from clients."
            );
            
            submitTo(
                mThreads[Threads::Dispatcher], 
                [&dispatcher](const CancelToken& token) {
                    dispatcher.run();
                }, 
                "This thread dispatches the valid requests to sequence process"
            );
            
            LOG_INFO("Gateway loops submitted to workers");
        }

        void shutdown() {
            LOG_INFO("Initiating Gateway shutdown...");
            mStopNetwork->store(true, std::memory_order_release);
            LOG_INFO("Network stop signal sent, waiting for clean shutdown...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            Scheduler::shutdown();
            LOG_INFO("Gateway shutdown complete");
        }
    };
}