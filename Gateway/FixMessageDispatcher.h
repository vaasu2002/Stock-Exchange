#pragma once

#include "FIX.h"
#include "Config.h"
#include "SharedMemory.h"
#include "messaging.h"

namespace Exchange::Gateway {

    class FixMessageDispatcher {
    public:

        /** @brief Constructor */
        FixMessageDispatcher(auto q): 
            mIngesssQueue(std::move(q)), mSchedulerInjector(Config::instance().ipcQueueScheduler(), 4096) {}

        /**
         * @brief Main consumer loop. Continuously consumes raw packets, 
         * parses FIX messages, and dispatches them based on MsgType.
         */
        void run() {
            LOG_INFO("Fix message dispatcher started");
            while (true) {
                Network::RawPacket packet;
                // Blocks until data is available or queue is closed
                if (!mIngesssQueue->pop(packet)) {
                    LOG_INFO("Ingress queue closed and empty, dispatcher exiting");
                    break;
                }
                dispatch(packet);
            }
        }

    private:
        // Queue carrying raw network packets received from client connections.
        // This dispatcher class consumes packets from it.
        std::shared_ptr<Core::IBlockingQueue<Network::RawPacket>> mIngesssQueue;

        // IPC producer to events to downstream components scheduler via shared memory.
        Ipc::Producer mSchedulerInjector;

        void dispatch(const Network::RawPacket& packet) {
            Network::Fix::FixMsg fix = Network::Fix::parseFix(packet.data.toString());

            if (!fix.isValid) {
                LOG_WARN("Invalid or partial FIX message from client %d", packet.clientSocket);
                return;
            }

            // New Order Single
            if (fix.msgType == "D") { 
                handleNewOrder(packet, fix);
            }
            // Logon: message that establishes a FIX session between two counterparties.
            else if (fix.msgType == "A") {
                handleLogon(packet);
            }
            else {
                LOG_WARN("Unhandled FIX MsgType=%ls from client %d", fix.msgType.get(), packet.clientSocket);
            }
        }

        void handleNewOrder(const Network::RawPacket& packet, const Network::Fix::FixMsg& fix) {
            LOG_TRACE(
                "ORDER RECEIVED Client=%d Side=%s Qty=%d Symbol=%s Price=%.2f",
                packet.clientSocket,
                fix.side.get(),
                fix.quantity,
                fix.symbol.get(),
                fix.price
            );

            // Build IPC New Order message
            Ipc::Msg::IpcMessage newOrder;
            newOrder.setMsgType(Ipc::Msg::MsgType::NEW_ORDER);

            // Populate fields from FIX message
            newOrder.addString(
                static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_SYMBOL),
                fix.symbol.get()
            );

            newOrder.addUint64(
                static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_SIDE),
                static_cast<uint64_t>(
                    fix.side == "1" ? Exchange::Order::Side::BUY
                                    : Exchange::Order::Side::SELL
                )
            );

            // Price converted to integer (e.g., 2 decimal fixed-point)
            int64_t priceInt = static_cast<int64_t>(fix.price * 10000);
            newOrder.addInt64(
                static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_PRICE),
                priceInt
            );

            newOrder.addUint64(
                static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_QTY),
                fix.quantity
            );

            // Use socket as client identifier (todo: replace with FIX CompID later)
            newOrder.addUint64(
                static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_CLIENT_ID),
                static_cast<uint64_t>(packet.clientSocket)
            );

            // todo: Assign a unique order id
            uint64_t tempOrderId = 1;
            newOrder.addUint64(
                static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_ORDER_ID),
                tempOrderId
            );

            // Default Time-In-Force (adjust if FIX tag 59 exists)
            newOrder.addUint64(
                static_cast<uint16_t>(Exchange::Ipc::Msg::FieldId::FIELD_TIF),
                static_cast<uint64_t>(Exchange::Order::TIF::DAY)
            );

            newOrder.finalize();

            // Encode and publish over shared memory IPC
            std::vector<uint8_t> buf;
            newOrder.encode(buf);

            bool success = mSchedulerInjector.write(
                buf.data(),
                static_cast<uint32_t>(buf.size())
            );

            if (success) {
                LOG_DEBUG("NEW_ORDER forwarded to IPC (OrderID=%lu)", tempOrderId);
            }
            else {
                LOG_ERROR("Failed to publish NEW_ORDER to IPC");
            }
        }

        void handleLogon(const Network::RawPacket& packet) {
            LOG_INFO("LOGON request from client %d", packet.clientSocket);
            // TODO:
            // Initialize FIX session state
            // Send Logon response
        }
    };

} // namespace Exchange::Gateway::Fix