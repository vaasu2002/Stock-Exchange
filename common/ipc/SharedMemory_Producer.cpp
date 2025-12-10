#include "SharedMemory.h"
#include <fstream>

namespace Exchange::Ipc {

    Producer::Producer(const Core::String& name, uint32_t capacity) : SharedMemory(name, capacity, true), mLock(name, true) {
        // Initialize header
        std::memset(mHeader, 0, sizeof(SharedHeader));
        std::strncpy(mHeader->signature, MAGIC, 32); // Set magic signature

        // Generate and set UUID
        Core::String sessionUuid = generateUuid();

        const Core::String uuidPath = "/tmp/" + mName + ".uuid";
        std::ofstream f(uuidPath.get(), std::ios::trunc);
        f << sessionUuid;
        f.close();

        std::strncpy(mHeader->uuid, sessionUuid.get(), 37);
        mHeader->capacity = capacity;
        mHeader->maxMsgSize  = MAX_MSG_SIZE;
        mHeader->writeIdx = 0;
        mHeader->readIdx = 0;
    }


    bool Producer::write(const void* data, uint32_t size) {
        if (size > mHeader->maxMsgSize) {
            // Prevent buffer overflow
            return false;
        }
        
        // Load Indices using GCC/Clang Intrinsics (works on raw uint32_t)
        // __ATOMIC_RELAXED, __ATOMIC_ACQUIRE, __ATOMIC_RELEASE are standard macr
        uint32_t currentWrite = __atomic_load_n(&mHeader->writeIdx, __ATOMIC_RELAXED);
        uint32_t currentRead  = __atomic_load_n(&mHeader->readIdx,  __ATOMIC_ACQUIRE);
        // std::atomic_ref<uint32_t> writeRef(header_->write_idx); cpp 20 // Creating atomic views
        // std::atomic_ref<uint32_t> readRef(header_->read_idx);
        // uint32_t currentWrite = writeRef.load(std::memory_order_relaxed); // only done by producer, so we trust its value
        // uint32_t currentRead = readRef.load(std::memory_order_acquire); // acquire to see consumer's updates coz it writes before reading thee
        // Check if full
        if (currentWrite - currentRead >= mHeader->capacity) {
            return false; 
        }
        // Write Data
        uint32_t slotIdx = currentWrite % mHeader->capacity;
        Slot& slot = mSlots[slotIdx];
        
        // Data copy
        std::memcpy(slot.data, data, size);
        
        // Length write
        __atomic_store_n(&slot.len, size, __ATOMIC_RELAXED);
        // slot.len = size; cpp 20
        
        // Commit (Release Fence)
        // This ensures all previous writes (data & len) are visible before the 
        // index updates.
        __atomic_store_n(&mHeader->writeIdx, currentWrite + 1, __ATOMIC_RELEASE);
        // writeRef.store(currentWrite + 1, std::memory_order_release); // cpp 20
        return true;
    }

} // namespace Exchange::Ipc