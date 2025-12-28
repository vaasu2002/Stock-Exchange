#include "Gateway.h"

#include <csignal>
#include <cstdlib>
#include <unistd.h>



namespace Exchange::Gateway {

    static Gateway* gInstance = nullptr;

    Gateway::Gateway(const Core::String& name)
        : mName(name) {
        gInstance = this;
    }

    void Gateway::setupSignalHandlers() {
        struct sigaction sa{};
        sa.sa_handler = Gateway::signalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        sigaction(SIGINT,  &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        LOG_INFO("Signal handlers registered (Ctrl+C to shutdown)");
    }

    void Gateway::signalHandler(int signum) {
        if (!gInstance) return;

        if (signum == SIGINT || signum == SIGTERM) {
            LOG_INFO("Shutdown signal received");
            gInstance->stop();
        }
    }

    void Gateway::start() {
        LOG_INFO("Launching Gateway...");
        setupSignalHandlers();

        // Load configuration
        Core::XMLReader reader("../config.xml");
        Config::init(reader.getNode(mName));

        // Create components
        mScheduler = std::make_unique<GatewayScheduler>(mName);

        mIngressQueue =
            std::make_shared<Core::MutexBlockingQueue<Network::RawPacket>>(
                Config::instance().blockingQueueSize()
            );

        mListener   = std::make_unique<Network::TcpEpollListener>(mIngressQueue);
        mDispatcher = std::make_unique<FixMessageDispatcher>(mIngressQueue);

        LOG_INFO("Starting Gateway Scheduler...");
        mScheduler->start(*mListener, *mDispatcher);

        LOG_INFO("Gateway is running. Press Ctrl+C to shutdown.");

        // Main wait loop
        while (!mShutdownRequested.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        LOG_INFO("Shutdown initiated, exiting in 1 second...");
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::exit(0);
    }

    void Gateway::stop() {
        mShutdownRequested.store(true, std::memory_order_release);

        // Safety net: force exit if shutdown hangs
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            LOG_WARN("Force exit after timeout");
            std::exit(0);
        }).detach();
    }

} // namespace Exchange::Gateway
