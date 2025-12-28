#pragma once

#include "FIX.h"

namespace Exchange::Gateway {

    class FixMessageDispatcher {
    public:

        /** @brief Constructor */
        FixMessageDispatcher(auto q):mIngesssQueue(std::move(q)){}

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
        
        std::shared_ptr<Core::IBlockingQueue<Network::RawPacket>> mIngesssQueue;

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
            // TODO:
            // Send order to scheduler / matching engine via IPC
        }

        void handleLogon(const Network::RawPacket& packet) {
            LOG_INFO("LOGON request from client %d", packet.clientSocket);
            // TODO:
            // Initialize FIX session state
            // Send Logon response
        }
    };

} // namespace Exchange::Gateway::Fix