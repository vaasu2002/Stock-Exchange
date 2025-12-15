#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include "messaging.h"
#include "enum.h"
#include "Exception.h"
#include "ipc/SharedMemory.h"
#include "Logger/Logger.h"
#include "Fix.h"

// Producer
int main(int argc, char* argv[]) {

    int port = 8001;  // Default port
    // Parse command line argument for port
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }



    try{
        LOG_INFO("Launching producer...");
        
        // // Initialize the queue
        // Exchange::Ipc::Producer producer("queue"); // name: "queue"
        // Exchange::Ipc::Msg::IpcMessage newOrder;
        // newOrder.setMsgType(Exchange::Ipc::Msg::MsgType::NEW_ORDER);
        // newOrder.addString(static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_SYMBOL), "RIL");
        // newOrder.addUint64(static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_SIDE), static_cast<uint64_t>(Exchange::Order::Side::BUY));
        // newOrder.addInt64(static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_PRICE), 123450); 
        // newOrder.addUint64(static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_QTY), 100);
        // newOrder.addUint64(static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_CLIENT_ID), 42);
        // newOrder.addUint64(static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_ORDER_ID), 1001);
        // newOrder.addUint64(static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_TIF), static_cast<uint64_t>(Exchange::Order::TIF::DAY));
        // newOrder.finalize();

        // std::vector<uint8_t> buf;
        // newOrder.encode(buf);

        // bool success = producer.write(buf.data(), static_cast<uint32_t>(buf.size()));
        // if (success) {
        //     LOG_INFO("Message sent...");
        //     printMessage(newOrder);
        // }
        // else {
        //     LOG_WARN("IPC Queue Full! Waiting...");
        // }
    } 
    catch (Engine::EngException& ex) {
        ex.log();
        return 1;
    }

    // Start Threads
    std::thread net_thread(Exchange::Gateway::networkLoop);
    std::thread app_thread(Exchange::Gateway::consumerLoop);
    // Wait forever
    net_thread.join();
    app_thread.join();
    return 0;
}
