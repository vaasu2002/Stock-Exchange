#pragma once

// Linux System Headers
#include <unistd.h>
#include <sys/file.h>

#include "exception.h"
#include "const.h"
#include "String.h"

namespace Exchange::Ipc {

    /**
     * @class ScopedFileLock
     * @brief 
     * It is an implementation of an Advisory File Lock using the RAII pattern.
     * Enforces Single Producer / Single Consumer logic via OS Kernel i.e. "Highlander Principle"
     */
    class ScopedFileLock {
        int mFd;
        Core::String mPath; 
    public:
        /**
         * The constructor ensures that Producers only fight other Producers, and Consumers only fight other Consumers
         */
        ScopedFileLock(const Core::String& name, bool isProducer) {

            mPath = Core::String(LOCK_BASE_PATH) + name + (isProducer ? ".prod.lock" : ".cons.lock");

            // Open lock file (create if needs be)
            // O_RDWR: read and write
            // O_CREAT: create if file does not exsits
            // O_CLOEXEC: close the file desciptoo when process exists
            mFd = open(mPath.get(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);

            if (mFd < 0) {
                ENG_THROW("Failed to open lock file: %s", mPath.get());
            }

            // LOCK_EX (Exclusive): This tells the Linux Kernel: "Only one file descriptor can hold a 
            // lock on this file at any time."
            // LOCK_NB (Non-Blocking): This tells the Kernel: "If someone else already has it, do not wait. 
            // Fail immediately."
            if (flock(mFd, LOCK_EX | LOCK_NB) < 0) {
                close(mFd);
                ENG_THROW("Highlander Rule Violation: Another process holds the lock %s", mPath.get());
            }
        }

        ~ScopedFileLock() {
            if (mFd >= 0) {
                // Automatically releases the lock when the FD closes
                // But done explicitly for clarity
                flock(mFd, LOCK_UN);
                close(mFd);
                // NOTE: Do not unlink the file, as other processes 
                // might be racing to open it.
            }
        }

    }; // class ScopedFileLock

} // namespace Exchange::Ipc