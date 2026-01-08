#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include "messaging.h"
#include "enum.h"
#include "Exception.h"
#include "ipc/SharedMemory.h"
#include "Config/Config.h"
#include "IPC/Consumer.h"

int main(int argc, char* argv[]) {

    int port = 8002;  // Default port
    // Parse command line argument for port
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    try {
        Exchange::Core::XMLReader reader("../config.xml");
        Exchange::Sequencer::Config::init(reader.getNode("Sequencer"));
        std::cout<<Exchange::Sequencer::Config::instance().PORT<<std::endl;
        std::cout<<Exchange::Sequencer::Config::instance().BLOCKING_QUEUE_SIZE<<std::endl;
        std::cout<<Exchange::Sequencer::Config::instance().IPC_QUEUE_GATEWAY.toString()<<std::endl;
        std::cout<<Exchange::Sequencer::Config::instance().IPC_QUEUE_ENGINE.toString()<<std::endl;
        std::cout << "[Consumer] Launching consumer... \n";
        Exchange::Sequencer::Ipc::Consumer sequencerConsumer;
        sequencerConsumer.run();
        // // Attempt to Connect
        // // This will THROW if the Producer hasn't started yet (shm_open fails)
        // Exchange::Ipc::Consumer consumer("queue");
        // std::cout << "Connected! Session UUID: " << consumer.getSessionUuid() << "\n";

        // std::vector<uint8_t> buf(Exchange::Ipc::MAX_MSG_SIZE);
        // while (true) {
        //     uint32_t n = consumer.read(buf.data(), static_cast<uint32_t>(buf.size()));
        //     if (n == 0) {
        //         // no message -- sleep/yield or continue polling
        //         std::this_thread::sleep_for(std::chrono::milliseconds(1));
        //         continue;
        //     }
        //     Exchange::Ipc::Msg::IpcMessage msg;
        //     if (!Exchange::Ipc::Msg::IpcMessage::decode(buf.data(), n, msg)) {
        //         // handle decode error
        //         continue;
        //     }

        //     // Process message
        //     Exchange::Ipc::Msg::printMessage(msg);
        // }
        
    } catch(Engine::EngException& ex) {
        // Producer likely hasn't started yet, or crashed and we lost the link.
        ex.log();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
