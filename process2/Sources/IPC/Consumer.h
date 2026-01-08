#pragma once

#include "SharedMemory.h"
#include "Config/Config.h"
#include "messaging.h"
namespace Exchange::Sequencer::Ipc {
    
    /**
     * @class Consumer
     * @brief Consumer for reading order messages from the Gateway process via 
     * shared memory queue.
     */
    class Consumer {

        // Provides read-only access to the shared memory queue in which gateway
        // process sends messages
        Exchange::Ipc::Consumer mFromGatewayQueue;

    public:
        /** @brief Constructor */
        Consumer(): mFromGatewayQueue(Config::instance().IPC_QUEUE_GATEWAY, 4096) {}

        void run() {
            std::vector<uint8_t> buf(Exchange::Ipc::MAX_MSG_SIZE);
            while (true) {
                uint32_t n = mFromGatewayQueue.read(buf.data(), static_cast<uint32_t>(buf.size()));
                if (n == 0) {
                    // no message -- sleep/yield or continue polling
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                Exchange::Ipc::Msg::IpcMessage msg;
                if (!Exchange::Ipc::Msg::IpcMessage::decode(buf.data(), n, msg)) {
                    // handle decode error
                    continue;
                }
                std::cout<<"Message received"<<std::endl;
            }
        }
    }; // class Consumer

} // namespace Exchange::Sequencer::Ipc