#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "String.h"
#include "Exception.h"
#include "Config.h"
#include "Scheduler/GatewayScheduler.h"
#include "BlockingQueue/MutexBlockingQueue.h"
#include "Network/TcpEpollListener.h"
#include "FixMessageDispatcher.h"

namespace Exchange::Gateway {
    class Gateway {
    public:
        explicit Gateway(const Core::String& name);

        void start();
        void stop();

    private:
        void setupSignalHandlers();
        static void signalHandler(int signum);

    private:
        Core::String mName;
        std::atomic<bool> mShutdownRequested{false};

        std::unique_ptr<GatewayScheduler> mScheduler;
        std::shared_ptr<Core::IBlockingQueue<Network::RawPacket>> mIngressQueue;
        std::unique_ptr<Network::TcpEpollListener> mListener;
        std::unique_ptr<FixMessageDispatcher> mDispatcher;
    };
}