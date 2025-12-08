#pragma once

#include <random>

// Linux 
#include <sys/mman.h>

#include "const.h"
#include "ScopedFileLock.h"

namespace Exchange::Ipc {


    struct Slot {
        uint32_t len;  // Bytes of data currently in stored
        uint8_t data[MAX_MSG_SIZE]; // Raw message bytes stored in this slot
    };


    struct SharedHeader {
        // Magic signature to ensure we are looking at a valid queue
        char signature[32];

        // Session ID to detect stale queues after crashes
        char uuid[37]; 

        // Align to cache line to prevent false sharing between producer/consumer
        alignas(CACHE_LINE_SIZE) uint32_t writeIdx; // Producer writes here
        alignas(CACHE_LINE_SIZE) uint32_t readIdx;  // Consumer reads here

        uint32_t capacity;
        uint32_t maxMsgSize;
    }; // class SharedHeader

    // Helper to generate uuid
    inline std::string generateUuid() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static const char* v = "0123456789abcdef";
        std::string uuid;
        for (int i = 0; i < 36; ++i) {
            if (i == 8 || i == 13 || i == 18 || i == 23) {
                uuid += '-';
            }
            else {
                uuid += v[dis(gen)];
            }
        }
        return uuid;
    }

    class SharedMemory {
    protected:
        std::string mName;     ///> Name of the shared memory object
        int mFd;               ///> File descriptor for the shared memory object
        size_t mTotalSize;     ///> Total size of the shared memory segment
        void* mBasePtr;        ///> Base pointer to the mapped shared memory
        SharedHeader* mHeader; ///> Pointer to the shared header in the mapped memory
        Slot* mSlots;          ///> Pointer to the array of slots in the mapped memory
        bool mIsOwner;         ///> Whether this instance created the shared memory (producer) or opened existing (consumer)
        const char* MAGIC = "IPC_V1_MAGIC"; ///> Magic signature to identify valid ring buffer
    public:
        /**
         * @brief Constructor
         * @note
         * POSIX shared memory objects must start with '/'
         */
        SharedMemory(const std::string& name, uint32_t capacity, bool create);
            
    }; // class SharedMemory


    class Producer : public SharedMemory {
        ScopedFileLock mLock;   ///> File lock to enforce single producer
    public:
        /**
         * @brief Constructor
         * 
         * @note
         * SharedMemory(.., true) means this is the creator (producer) of the shared memory segment.
         * mLock(true) means this is a producer lock.
         */
        Producer(const std::string& name, uint32_t capacity = BUFFER_CAPACITY);

        /**
         * @brief Write data into the ring buffer. 
         * @param data Pointer to the data to write.
         * @param size Size of the data in bytes.
         * @return true if write succeeded, false if buffer is full or size exceeds max message size.
         * 
         * @details
         * This is lock free write operation. It uses atomic operations to manage the write and read indices.
         * In Shared Memory, the data exists as raw bytes (uint32_t). We cannot store a std::atomic object there
         * directly because std::atomic might have a constructor/destructor that doesn't play well with mmap 
         * (implicit lifetimes).
         */
        bool write(const void* data, uint32_t size);

    }; // class Producer
    

    class Consumer : public SharedMemory {
        ScopedFileLock mLock;   ///> File lock to enforce single producer
    public:
        /**
         * @brief Constructor
         * 
         * @note
         * SharedMemory(.., false) means this is the creator (producer) of the shared memory segment.
         * mLock(false) means this is a consumer lock.
         */
        Consumer(const std::string& name, uint32_t capacity = BUFFER_CAPACITY);

        // Returns bytes read, or 0 if empty
        uint32_t read(void* buffer, uint32_t bufferSize);
        
        std::string getSessionUuid() const;

    }; // class Consumer

} // namespace Exchange::Ipc