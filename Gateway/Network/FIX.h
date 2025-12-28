#pragma once

#include <sstream>
#include "String.h"

namespace Exchange::Gateway::Network {

    // Start of Heading (SOH) character standard in FIX (Financial Information eXchange) protocol
    // Example: 8=FIX.4.4\x019=176\x0135=D\x0149=BUYER\x0156=SELLER\x0110=128\x01
    constexpr char gFixDelimiter = '\x01';

    class Fix {
    public:
        /**
         * @struct FixMsg
         * @brief Represents a simplified FIX message.
         */
        struct FixMsg {
            Core::String msgType; // Tag 35: Message type (e.g., "D" = New Order Single)
            Core::String symbol;  // Tag 55: Financial instrument symbol
            Core::String side;    // Tag 54: Order side ("1" = Buy, "2" = Sell)
            double price;        // Tag 44: Order price
            int quantity;        // Tag 38: Order quantity
            bool isValid;        // Indicates whether the FIX message passed validation
        };

        /**
         * @brief Parses a raw FIX message string into a FixMessage structure.
         * 
         * @details 
         * The message is split using the SOH (0x01) field delimiter and each tag=value
         * pair is extracted and mapped to known FIX tags. A zero-copy parser would avoid 
         * string allocations and be faster, but this approach is easier to understand 
         * and maintain.
         * @param raw Raw string received from network
         * @return 
         */
        static FixMsg parseFix(const Core::String& raw) {
            LOG_TRACE("Raw Fix: %ls", raw);

            FixMsg msg;
            msg.isValid = false;
            std::stringstream ss(raw.toString());
            std::string segment;
            
            // Split by SOH delimiter 
            while (std::getline(ss, segment, gFixDelimiter)) {
                size_t eq_pos = segment.find('=');
                if (eq_pos == std::string::npos) continue;

                std::string tag = segment.substr(0, eq_pos);
                std::string value = segment.substr(eq_pos + 1);

                if (tag == "35") msg.msgType = value;
                else if (tag == "55") msg.symbol = value;
                else if (tag == "54") msg.side = value; // 1=Buy, 2=Sell
                else if (tag == "44") msg.price = std::stod(value);
                else if (tag == "38") msg.quantity = std::stoi(value);
            }
            
            // Basic validation
            // todo: Add more validation logic
            if (!msg.msgType.empty()) {
                msg.isValid = true;
            }

            return msg;
        }
    };
}