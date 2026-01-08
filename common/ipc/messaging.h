#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <cstring>
#include <stdexcept>
#include <optional>
#include "enum.h"
#include <iostream>

// On-wire structs (packed)
// Pack all struct fields with 1-byte alignment.
// Do not insert padding bytes between fields.
#pragma pack(push, 1)
#pragma pack(pop)

// [ MsgHeader ][ FieldHeader + value ][ FieldHeader + value ] ...
// ONE Message contains MANY fields
// ┌──────────────────────────┐
// │ MsgHeader                │ 16 bytes
// │  MsgType = NEW_ORDER     │
// │  fieldCount = 5          │ (after finalize())
// │  length = XXX            │ (sum of all field sections)
// │  seqNo = 0               │ (Sequencer sets later)
// └──────────────────────────┘

// ┌──────────────────────────┐
// │ FieldHeader(symbol)      │
// │  field_id = 1            │ FIELD_SYMBOL
// │  field_type = STRING     │
// │  value_len = 4           │
// │ value: "AAPL"            │
// └──────────────────────────┘

// ┌──────────────────────────┐
// │ FieldHeader(side)        │
// │  field_id = 2            │ FIELD_SIDE
// │  field_type = UINT64     │
// │  value_len = 8           │
// │ value: 0                 │ (BUY)
// └──────────────────────────┘

// ┌──────────────────────────┐
// │ FieldHeader(price)       │
// │  field_id = 3            │ FIELD_PRICE
// │  field_type = INT64      │
// │  value_len = 8           │
// │ value: 1234600           │
// └──────────────────────────┘

namespace Exchange::Ipc::Msg {

    struct MsgHeader {
        uint16_t MsgType;     // Message type
        uint16_t fieldCount;  // number of KV fields
        uint32_t length;      // bytes of all fields (not including this header)
        uint64_t seqNo;       // global sequence, -1 if unset
    };

    struct FieldHeader {
        int16_t fieldId;   // Field identifier
        uint8_t  fieldType; // Type of field
        uint32_t valueLen;  // size in bytes of the value
    };


    /**
    * @class IpcMessage
    * @brief Represents an IPC message with a header and multiple fields.
    * This class provides methods to construct, encode, decode, and access fields within the message.
    * It supports various field types including INT64, UINT64, DOUBLE, STRING, and BYTES.
    * Usage:
    * ```
    * IpcMessage msg;
    * msg.setMsgType(MsgType::NEW_ORDER);
    * msg.addString(FieldId::FIELD_SYMBOL, "AAPL");
    * msg.addUint64(FieldId::FIELD_QTY, 100);
    * msg.finalize();
    * ```
    */
    class IpcMessage {

    public:

        MsgHeader header{};

        std::vector<uint8_t> fields; // encoded (FieldHeader + value blobs)
        IpcMessage() {
            clear();
        }

        void clear() {
            // fills the entire header struct with zeros — byte-by-byte.
            std::memset(&header, 0, sizeof(header)); 
            header.MsgType = static_cast<uint16_t>(MsgType::NONE);
            fields.clear();
        }

        void setMsgType(MsgType t) {
            header.MsgType = static_cast<uint16_t>(t);
        }

        void setSeqNo(uint64_t seq) {
            header.seqNo = seq;
        }

        void addInt64(uint16_t fieldId, int64_t value) {
            addFieldHeader(fieldId, FieldType::INT64, sizeof(value));
            appendBytes(&value, sizeof(value));
        }

        void addUint64(uint16_t fieldId, uint64_t value) {
            addFieldHeader(fieldId, FieldType::UINT64, sizeof(value));
            appendBytes(&value, sizeof(value));
        }

        void addDouble(uint16_t fieldId, double value) {
            addFieldHeader(fieldId, FieldType::DOUBLE, sizeof(value));
            appendBytes(&value, sizeof(value));
        }

        void addString(uint16_t fieldId, std::string_view value) {
            addFieldHeader(fieldId, FieldType::STRING,
                        static_cast<uint32_t>(value.size()));
            appendBytes(value.data(), value.size());
        }

        void addBytes(uint16_t field_id, const void* data, uint32_t len) {
            addFieldHeader(field_id, FieldType::BYTES, len);
            appendBytes(data, len);
        }

        /**
         * @brief Finalizes the message by updating the header with the correct field count and length.
         * This method scans through the added fields to count them and calculate the total length.
         * 
         * @note It must be called before encoding or sending the message.'
         */
        void finalize() {
            uint16_t count = 0;
            const uint8_t* ptr = fields.data();
            const uint8_t* end = fields.data() + fields.size();
            while (ptr + sizeof(FieldHeader) <= end) {
                const auto* fh = reinterpret_cast<const FieldHeader*>(ptr);
                ptr += sizeof(FieldHeader);
                if (ptr + fh->valueLen > end) {
                    throw std::runtime_error("Message::finalize: corrupted internal buffer");
                }
                ptr += fh->valueLen;
                ++count;
            }
            if (ptr != end) {
                throw std::runtime_error("Message::finalize: misaligned field buffer");
            }
            header.fieldCount = count;
            header.length = static_cast<uint32_t>(fields.size());
        }

        // ----- Encode / decode -----
        // Serialize header+fields to a contiguous buffer
        void encode(std::vector<uint8_t>& out) const {
            out.resize(sizeof(MsgHeader) + fields.size());
            std::memcpy(out.data(), &header, sizeof(MsgHeader));
            if (!fields.empty()) {
                std::memcpy(out.data() + sizeof(MsgHeader),
                            fields.data(),
                            fields.size());
            }
        }

        // Decode from a contiguous buffer (e.g. data read from ring buffer)
        // size must be at least sizeof(MsgHeader)
        static bool decode(const uint8_t* data, size_t size, IpcMessage& out) {
            if (size < sizeof(MsgHeader)) return false;
            MsgHeader hdr{};
            std::memcpy(&hdr, data, sizeof(MsgHeader));
            if (size < sizeof(MsgHeader) + hdr.length) {
                return false; // incomplete frame
            }
            out.header = hdr;
            out.fields.resize(hdr.length);
            if (hdr.length > 0) {
                std::memcpy(out.fields.data(),
                            data + sizeof(MsgHeader),
                            hdr.length);
            }
            // optional: validate internal structure now
            return out.validateFields();
        }

        // Raw size of the fully encoded message (for ring buffer slot size checks)
        size_t encodedSize() const {
            return sizeof(MsgHeader) + fields.size();
        }

        // Raw pointer for direct write (if you want)
        // (you still need to allocate a buffer and call encode() into it)
        const MsgHeader& getHeader() const { return header; }

        // ----- Getters -----
        // If field not found or type mismatch -> returns std::nullopt

        std::optional<int64_t> getInt64(uint16_t field_id)  const {
            int64_t out;
            if (findField(field_id, FieldType::INT64, &out, sizeof(out)))
                return out;
            return std::nullopt;
        }

        std::optional<uint64_t> getUint64(uint16_t field_id) const {
            uint64_t out;
            if (findField(field_id, FieldType::UINT64, &out, sizeof(out)))
                return out;
            return std::nullopt;
        }

        std::optional<double> getDouble(uint16_t field_id) const {
            double out;
            if (findField(field_id, FieldType::DOUBLE, &out, sizeof(out)))
                return out;
            return std::nullopt;
        }

        std::optional<std::string> getString(uint16_t field_id) const {
            const uint8_t* val;
            uint32_t len;
            if (!findFieldRaw(field_id, FieldType::STRING, &val, &len))
                return std::nullopt;
            return std::string(reinterpret_cast<const char*>(val), len);
        }

        std::optional<std::vector<uint8_t>> getBytes(uint16_t field_id) const {
            const uint8_t* val;
            uint32_t len;
            if (!findFieldRaw(field_id, FieldType::BYTES, &val, &len))
                return std::nullopt;
            return std::vector<uint8_t>(val, val + len);
        }
    private:
        void addFieldHeader(uint16_t fieldId, FieldType type, uint32_t valueLen) {
            FieldHeader fh;
            fh.fieldId   = fieldId;
            fh.fieldType = static_cast<uint8_t>(type);
            fh.valueLen  = valueLen;
            appendBytes(&fh, sizeof(fh));
        }

        void appendBytes(const void* src, size_t len) {
            size_t old = fields.size();
            fields.resize(old + len);
            std::memcpy(fields.data() + old, src, len);
        }

        bool validateFields() const {
            const uint8_t* ptr = fields.data();
            const uint8_t* end = fields.data() + fields.size();
            uint16_t count = 0;
            while (ptr + sizeof(FieldHeader) <= end) {
                const auto* fh = reinterpret_cast<const FieldHeader*>(ptr);
                ptr += sizeof(FieldHeader);
                if (ptr + fh->valueLen > end) return false;
                ptr += fh->valueLen;
                ++count;
            }
            if (ptr != end) return false;
            // we can also check count == header.field_count if you want strict
            return true;
        }

        bool findFieldRaw(uint16_t field_id,
                        FieldType expectedType,
                        const uint8_t** valOut,
                        uint32_t* lenOut) const
        {
            const uint8_t* ptr = fields.data();
            const uint8_t* end = fields.data() + fields.size();
            while (ptr + sizeof(FieldHeader) <= end) {
                const auto* fh = reinterpret_cast<const FieldHeader*>(ptr);
                ptr += sizeof(FieldHeader);
                if (ptr + fh->valueLen > end) {
                    return false; // corrupted
                }
                const uint8_t* valPtr = ptr;
                if (fh->fieldId == field_id &&
                    fh->fieldType == static_cast<uint8_t>(expectedType)) {
                    *valOut = valPtr;
                    *lenOut = fh->valueLen;
                    return true;
                }
                // skip value
                ptr += fh->valueLen;
            }
            return false;
        }

        bool findField(uint16_t field_id,
                    FieldType expectedType,
                    void* dst,
                    size_t dstSize) const
        {
            const uint8_t* val;
            uint32_t len;
            if (!findFieldRaw(field_id, expectedType, &val, &len))
                return false;
            if (len != dstSize) // size mismatch
                return false;
            std::memcpy(dst, val, dstSize);
            return true;
        }

    }; // class IpcMessage
    
    
        // For debuging
    static void printMessage(const Msg::IpcMessage &m) {
        using namespace Msg;
        const auto &h = m.getHeader();
        std::cout << "--- Message ---\n";
        std::cout << "MsgType: " << static_cast<uint16_t>(h.MsgType)
                << ", FieldCount: " << h.fieldCount
                << ", Length: " << h.length
                << ", SeqNo: " << h.seqNo << std::endl;

        if (auto s = m.getString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL)))
            std::cout << "symbol: " << *s << std::endl;
        if (auto side = m.getUint64(static_cast<uint16_t>(FieldId::FIELD_SIDE)))
            std::cout << "side: " << *side << std::endl;
        if (auto price = m.getInt64(static_cast<uint16_t>(FieldId::FIELD_PRICE)))
            std::cout << "price: " << *price << std::endl;
        if (auto qty = m.getUint64(static_cast<uint16_t>(FieldId::FIELD_QTY)))
            std::cout << "qty: " << *qty << std::endl;
        if (auto cid = m.getUint64(static_cast<uint16_t>(FieldId::FIELD_CLIENT_ID)))
            std::cout << "client_id: " << *cid << std::endl;
        if (auto oid = m.getUint64(static_cast<uint16_t>(FieldId::FIELD_ORDER_ID)))
            std::cout << "order_id: " << *oid << std::endl;
        if (auto tif = m.getUint64(static_cast<uint16_t>(FieldId::FIELD_TIF)))
            std::cout << "tif: " << *tif << std::endl;
    }

} // namespace Exchange::Ipc::Msg



// int main(){
//     using namespace Msg;

//     // 1) NEW_ORDER
//     IpcMessage newOrder;
//     newOrder.setMsgType(MsgType::NEW_ORDER);
//     newOrder.addString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL), "AAPL");
//     newOrder.addUint64(static_cast<uint16_t>(FieldId::FIELD_SIDE), static_cast<uint64_t>(ORDER::Side::BUY));
//     newOrder.addInt64(static_cast<uint16_t>(FieldId::FIELD_PRICE), 123450); // price expressed in cents
//     newOrder.addUint64(static_cast<uint16_t>(FieldId::FIELD_QTY), 100);
//     newOrder.addUint64(static_cast<uint16_t>(FieldId::FIELD_CLIENT_ID), 42);
//     newOrder.addUint64(static_cast<uint16_t>(FieldId::FIELD_ORDER_ID), 1001);
//     newOrder.addUint64(static_cast<uint16_t>(FieldId::FIELD_TIF), static_cast<uint64_t>(ORDER::TIF::DAY));
//     newOrder.finalize();

//     printMessage(newOrder);

//     // Encode/decode roundtrip demonstration
//     std::vector<uint8_t> buf;
//     newOrder.encode(buf);
//     IpcMessage decoded;
//     if (IpcMessage::decode(buf.data(), buf.size(), decoded)) {
//         std::cout << "Decoded NEW_ORDER successfully\n";
//         printMessage(decoded);
//     }

//     // 2) CANCEL
//     IpcMessage cancel;
//     cancel.setMsgType(MsgType::CANCEL);
//     cancel.addUint64(static_cast<uint16_t>(FieldId::FIELD_ORDER_ID), 1001);
//     cancel.addUint64(static_cast<uint16_t>(FieldId::FIELD_CLIENT_ID), 42);
//     cancel.finalize();
//     printMessage(cancel);

//     // 3) TRADE (report of an execution)
//     IpcMessage trade;
//     trade.setMsgType(MsgType::TRADE);
//     trade.addString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL), "AAPL");
//     trade.addInt64(static_cast<uint16_t>(FieldId::FIELD_PRICE), 123500);
//     trade.addUint64(static_cast<uint16_t>(FieldId::FIELD_QTY), 50);
//     trade.addUint64(static_cast<uint16_t>(FieldId::FIELD_ORDER_ID), 1001);
//     trade.finalize();
//     printMessage(trade);

//     // 4) BOOK_DELTA (incremental change)
//     IpcMessage delta;
//     delta.setMsgType(MsgType::BOOK_DELTA);
//     delta.addString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL), "AAPL");
//     delta.addUint64(static_cast<uint16_t>(FieldId::FIELD_SIDE), static_cast<uint64_t>(ORDER::Side::SELL));
//     delta.addInt64(static_cast<uint16_t>(FieldId::FIELD_PRICE), 123600);
//     delta.addUint64(static_cast<uint16_t>(FieldId::FIELD_QTY), 25);
//     delta.finalize();
//     printMessage(delta);

//     return 0;
// }