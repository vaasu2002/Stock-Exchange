#include "SharedMemory.h"
#include <fstream>

namespace Exchange::Ipc {

    Consumer::Consumer(const Core::String& name, uint32_t capacity)
        : SharedMemory(name, capacity, false), mLock(name, false) {
        

        // Verify Magic
        if (std::strncmp(mHeader->signature, MAGIC, 32) != 0) {
            ENG_THROW("Invalid IPC Header Signature");
        }

        const Core::String uuidPath = "/tmp/" + mName +".uuid";
        std::ifstream f(uuidPath);
        if (!f.is_open()) {
            ENG_THROW("UUID file not found");
        }
        char expectedUuid[37] = {};
        f.getline(expectedUuid, sizeof(expectedUuid));
        
        if (std::strncmp(mHeader->uuid, expectedUuid, 36) != 0) {
            ENG_THROW("Stale shared memory session");
        }

        std::cout << "[Consumer] Attached. Session: " << mHeader->uuid << "\n";
    }

    uint32_t Consumer::read(void* buffer, uint32_t bufferSize) {

        // Pre-C++20: Use GCC/Clang built-ins
        //Load indices
        uint32_t currentRead = __atomic_load_n(&mHeader->readIdx, __ATOMIC_RELAXED);
        uint32_t currentWrite = __atomic_load_n(&mHeader->writeIdx, __ATOMIC_ACQUIRE);
        if (currentRead >= currentWrite) {
            return 0; // Empty
        }
        // Read Data
        uint32_t slotIdx = currentRead % mHeader->capacity;
        Slot& slot = mSlots[slotIdx];
        // Atomic load for length
        uint32_t msgLen = __atomic_load_n(&slot.len, __ATOMIC_RELAXED);
        if (msgLen > bufferSize) {
            // Handle buffer too small error locally or truncate
            msgLen = bufferSize; 
        }
        std::memcpy(buffer, slot.data, msgLen);
        // Commit: Release ordering
        __atomic_store_n(&mHeader->readIdx, currentRead + 1, __ATOMIC_RELEASE);
        return msgLen;
    }

    Core::String Consumer::getSessionUuid() const {
        return Core::String(mHeader->uuid);
    }

} // namespace Exchange::Ipc