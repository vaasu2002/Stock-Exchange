#pragma once

#include "IBlockingQueue.h"
#include "Exception.h"

namespace Exchange::Core {

    /**
     * @class MutexBlockingQueue 
     * @brief A thread-safe blocking queue with a fixed capacity. It automates synchronization between producer and consumer threads.
     *
     * All are write operations  The condition check is not a pure read, because it controls a write Even the act of deciding whether to block is tied to shared state so always unique lock.
     */
    template <typename T>
    class MutexBlockingQueue : public IBlockingQueue<T> {
        std::queue<T> m_Queue; ///> Internal queue to hold elements 
        std::size_t m_Capacity; ///> Maximum capacity of the queue 
        bool m_Closed; ///> Flag indicating if the queue is closed for pushing new elements

        mutable std::mutex m_Mutex; ///> Mutex for synchronizing access to the queue
        std::condition_variable m_NotFullCv; ///> Condition variable to signal when the queue is not full
        std::condition_variable m_NotEmptyCv; ///> Condition variable to signal when the queue is not empty
    public:

        explicit MutexBlockingQueue (std::size_t capacity) : m_Capacity(capacity) {
            if (m_Capacity == 0) {
                ENG_THROW("Capacity must be > 0");
            }
        }
        // Disabling copy constructor
        MutexBlockingQueue (const MutexBlockingQueue &) = delete;
        // Disabling copy operation
        MutexBlockingQueue & operator=(const MutexBlockingQueue &) = delete;

        /**
         * @brief Puts an item(l value) into a queue. blocks if the queue is full
         * @throws std::runtime_error if queue is closed.
         */
        void push(const T& value) override {

            std::unique_lock<std::mutex> lock(m_Mutex);

            // Wait until there is space or closed is triggered
            m_NotFullCv.wait(lock, [&] {
                return (m_Queue.size() < m_Capacity) || m_Closed; // wake-up condition
            });

            if (m_Closed) {
                ENG_THROW("Push on closed.");
            }

            m_Queue.push(value);
            m_NotEmptyCv.notify_one();
        }

        /**
         * @brief Puts an item(r value) into a queue. blocks if the queue is full
         * @throws std::runtime_error if queue is closed.
         */
        void push(T&& value) override {
            std::unique_lock<std::mutex> lock(m_Mutex);

            // Wait until there is space or closed is triggered
            m_NotFullCv.wait(lock, [&] {
                return (m_Queue.size() < m_Capacity) || m_Closed; // wake-up condition
            });

            if (m_Closed) {
                ENG_THROW("Push on closed.");
            }

            m_Queue.push(std::move(value));
            m_NotEmptyCv.notify_one();
        }


        /**
         * @brief Closes the queue. No further push operations are allowed.
         *        Notifies all waiting threads.
         * @param out The variable to store the popped item.
         * @return true if an item was successfully popped, false if the queue is closed and empty.
         */
        bool pop(T& out) override {
            std::unique_lock<std::mutex> lock(m_Mutex);

            // Wait until here is data in the ueue or closed is triggered
            m_NotEmptyCv.wait(lock, [&] {
                return (!m_Queue.empty() || m_Closed);
            });

            // To check if we have data to pop after being notified coz of closed
            if (m_Queue.empty()) {
                // closed and empty
                return false;
            }

            out = std::move(m_Queue.front());
            m_Queue.pop();
            m_NotFullCv.notify_one();
            return true;
        }

        T pop() override {
            T value;
            if (!pop(value)) {
                ENG_THROW("pop on closed and empty");
            }
            return value;
        }

        void close() override {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Closed = true;
            m_NotEmptyCv.notify_all();
            m_NotFullCv.notify_all();
        }

        bool isClosed() const override {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Closed;
        }
    };
}