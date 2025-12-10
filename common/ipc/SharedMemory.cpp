#include "SharedMemory.h"


namespace Exchange::Ipc {


    SharedMemory::SharedMemory(const Core::String& name, uint32_t capacity, bool create)
            : mName("/" + name), mIsOwner(create) {
        
        // Calculate total size
        mTotalSize = sizeof(SharedHeader) + (capacity * sizeof(Slot));

        if (create) {
            // Producer: Unlink old, Create new
            // shm_unlink removes any previously existing shared memory with the same name.
            // Prevents leftover shared memory from old runs.
            // shm_unlink operates on the system-wide level, not just within your process's virtual memory.
            // This removes the name from the operating system's list (specifically from /dev/shm on Linux).
            // It prevents any new processes from connecting to that old memory segment.
            // This is what allows the Producer to say, "I am discarding the old, stale queue and starting fresh."
            // Data Persistence: Even though shm_unlink removes the global name immediately, the Operating System uses
            // reference counting. The physical RAM for that segment is not actually freed until every process (including
            // any lingering Consumers) has closed it or exited. However, since the name is gone, the Producer is free to
            // create a brand new segment with the same name immediately.
            shm_unlink(name.get());
            
            // Creates a new shared memory object.
            // O_RDWR — open read/write.
            // 0666 — permissions: read/write for all users.
            // Returns a file descriptor in shmFd.
            mFd = shm_open(name.get(), O_CREAT | O_RDWR, 0666);
            if (mFd == -1) {
                ENG_THROW("Producer: shm_open failed (create)");
            }
            // Sets the size of the shared memory object.
            // Newly created shared memory has size 0, so without ftruncate it cannot be mmap'ed properly.
            // ftruncate() sets or changes the size of a file — in this case, a POSIX shared-memory object created with shm_open.
            if (ftruncate(mFd, mTotalSize) == -1) {
                ENG_THROW("Producer: ftruncate failed");
            }
        } 
        else {
            // Consumer: Open existing
            mFd = shm_open(name.get(), O_RDWR, 0666);
            if (mFd == -1) {
                ENG_THROW("Consumer: shm_open failed (open existing)");
            }
        }
        mBasePtr = mmap(0, mTotalSize, PROT_READ | PROT_WRITE, MAP_SHARED, mFd, 0);
        if (mBasePtr == MAP_FAILED) {
            ENG_THROW("mmap failed");
        }
        // Calculate offsets
        mHeader = static_cast<SharedHeader*>(mBasePtr);
        
        // Pointer arithmetic to find start of slots
        uint8_t* rawPtr = static_cast<uint8_t*>(mBasePtr);
        mSlots = reinterpret_cast<Slot*>(rawPtr + sizeof(SharedHeader));
    }
    
} // namespace Exchange::Ipc