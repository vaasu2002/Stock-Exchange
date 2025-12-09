#pragma once

#include <cstdint>


namespace Exchange {
    
    // <======================== INTER PROCESS COMMUNICATION MESSAGE ========================>
    namespace Ipc::Msg {

        /**
         * @enum Types of message
         * NONE, NEW ORDER, CANCEL, TRADE, BOOK_DELTA
        **/
        enum class MsgType : uint16_t {
            NONE        = 0,
            NEW_ORDER   = 1, // client is submitting a new ordeR
            CANCEL      = 2, // Client wants to cancel an existing resting order
            TRADE       = 3, // trade occurred
            BOOK_DELTA  = 4, // incremental change to the order book
        };

        /**
         * @enum Types of fields
        **/
        enum class FieldType : uint8_t {
            INT64   = 1,
            UINT64  = 2,
            DOUBLE  = 3,
            STRING  = 4,
            BYTES   = 5,
        };

        /**
         * @enum Types of keys
        **/
        enum class FieldId : uint16_t {
            FIELD_SYMBOL        = 1,
            FIELD_SIDE          = 2,  // 0=buy, 1=sell
            FIELD_PRICE         = 3, 
            FIELD_QTY           = 4,
            FIELD_CLIENT_ID     = 5,
            FIELD_ORDER_ID      = 6,
            FIELD_TIF           = 7,
        };

    } // namespace namespace Ipc::Msg

    // <=================================== ORDER TYPES =====================================>

    namespace Order {

        /**
         * @enum Side which order belongs to
        **/
        enum class Side : uint8_t {
            BUY,
            SELL
        };

        /**
         * @enum Side which order belongs to
        **/
        enum class Type : uint8_t {
            MARKET,
            LIMIT,
            DEFAULT = MARKET
        };

        /**
         * @enum Status of order
        **/
        enum class Status : uint8_t {
            PENDING,                // Not yet executed, order stays active on book
            CANCELLED,              // No fills, just cancelled
            FULFILLED,              // Fully executed
            PARTIALLY_FILLED,       // Partial but order stays active on book
            PARTIAL_FILL_CANCELLED  // Partial fill + remainder cancelled (IOC, MKT)
        };

        /**
         * @enum Time In force configration of order
        **/
        enum class TIF : uint8_t {
            DAY = 0,                                            // 000 (0)
            ALL_OR_NONE = 1,                                    // 001 (1)
            IMMEDIATE_OR_CANCEL = 2,                            // 010 (2)
            FILL_OR_KILL = (ALL_OR_NONE | IMMEDIATE_OR_CANCEL), // 011 (3)
            GOOD_TILL_CANCEL = 4,                               // 110 (4)
            DEFAULT = DAY
        };
    } // namespace Order

} // namespace Engine