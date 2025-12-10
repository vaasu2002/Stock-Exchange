#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>

#include "SharedMemory.h"
#include "messaging.h"

using namespace Exchange::Ipc;
using namespace Exchange::Ipc::Msg;

// ANSI color codes for better visibility
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

void log(const std::string& prefix, const std::string& msg, const std::string& color = RESET) {
    std::cout << color << "[" << prefix << "] " << msg << RESET << std::endl;
}

/**
 * @brief Test 1: Verify that producer and consumer connect to the same queue
 * 
 * GIVEN: A fresh producer creates a new shared memory queue with a UUID
 * WHEN:  A consumer connects to the same queue name
 * THEN:  
 *   - Consumer successfully connects without errors
 *   - Consumer reads the same UUID as the producer
 *   - Messages written by producer are readable by consumer
 *   - Data integrity is maintained across the shared memory boundary
 */
bool TEST1_sameQueueConnection() {
    log("TEST 1", "Testing same queue connection...", CYAN);
    
    const std::string queueName = "test_queue_same";
    
    try {
        // Create producer
        Producer producer(queueName, 64);
        log("Producer", "Created queue", GREEN);
        
        // Give it a moment to initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Create consumer
        Consumer consumer(queueName, 64);
        log("Consumer", "Connected to queue", GREEN);
        
        // Write a test message from producer
        IpcMessage msg;
        msg.setMsgType(MsgType::NEW_ORDER);
        msg.addString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL), "TEST");
        msg.addUint64(static_cast<uint16_t>(FieldId::FIELD_QTY), 100);
        msg.finalize();
        
        std::vector<uint8_t> encoded;
        msg.encode(encoded);
        
        if (!producer.write(encoded.data(), encoded.size())) {
            log("TEST 1", "Failed to write message", RED);
            return false;
        }
        log("Producer", "Wrote test message", GREEN);
        
        // Read from consumer
        uint8_t buffer[4096];
        uint32_t bytesRead = consumer.read(buffer, sizeof(buffer));
        
        if (bytesRead == 0) {
            log("TEST 1", "Consumer failed to read message", RED);
            return false;
        }
        
        IpcMessage receivedMsg;
        if (!IpcMessage::decode(buffer, bytesRead, receivedMsg)) {
            log("TEST 1", "Failed to decode message", RED);
            return false;
        }
        
        auto symbol = receivedMsg.getString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL));
        if (!symbol || *symbol != "TEST") {
            log("TEST 1", "Message content mismatch", RED);
            return false;
        }
        
        log("Consumer", "Successfully read message: " + *symbol, GREEN);
        log("TEST 1", "PASSED - Same queue connection verified", GREEN);
        return true;
        
    } catch (const std::exception& e) {
        log("TEST 1", std::string("FAILED - ") + e.what(), RED);
        return false;
    }
}

/**
 * @brief Test 2: Verify stale queue detection prevents reading from crashed sessions
 * 
 * GIVEN: A producer creates a queue and then terminates, leaving shared memory intact
 *        AND the UUID file is manually corrupted to simulate a stale/mismatched state
 * WHEN:  A new consumer attempts to connect to this existing shared memory
 * THEN:  
 *   - Consumer detects the UUID mismatch
 *   - Consumer throws an EngException with "Stale" in the error message
 *   - Consumer refuses to connect, preventing data corruption
 *   - The safety mechanism protects against reading from orphaned queues
 */
bool TEST2_staleQueueDetection() {
    log("TEST 2", "Testing stale queue detection...", CYAN);
    
    const std::string queueName = "test_queue_stale";
    const std::string uuidPath = "/tmp//" + queueName + ".uuid";
    
    try {
        // Create producer with valid UUID
        {
            Producer producer(queueName, 64);
            log("Producer", "Created queue with UUID", GREEN);
        }
        // Producer destroyed but shared memory persists
        
        // Manually corrupt the UUID file to simulate stale queue
        std::ofstream corrupted(uuidPath, std::ios::trunc);
        corrupted << "00000000-0000-0000-0000-000000000000";
        corrupted.close();
        log("Test", "Corrupted UUID file to simulate stale queue", YELLOW);
        
        // Try to connect consumer - should fail
        try {
            Consumer consumer(queueName, 64);
            log("TEST 2", "FAILED - Consumer connected to stale queue (should have failed)", RED);
            return false;
        } catch (const Engine::EngException& e) {
            std::string errMsg = e.what();
            if (errMsg.find("Stale") != std::string::npos) {
                log("Consumer", "Correctly rejected stale queue", GREEN);
                log("TEST 2", "PASSED - Stale queue detection works", GREEN);
                return true;
            } else {
                log("TEST 2", "FAILED - Wrong exception: " + errMsg, RED);
                return false;
            }
        }
        
    } catch (const std::exception& e) {
        log("TEST 2", std::string("FAILED - Unexpected error: ") + e.what(), RED);
        return false;
    }
}

/**
 * @brief Test 3: Simulate producer crash and recovery
 * Expected: New producer creates new UUID, consumer detects change and reconnects
 */
bool TEST3_producerCrashRecovery() {
    log("TEST 3", "Testing producer crash and recovery...", CYAN);
    
    const std::string queueName = "test_queue_crash";
    
    pid_t consumerPid = fork();
    
    if (consumerPid == 0) {
        // Child process - Consumer
        try {
            log("Consumer[Child]", "Waiting for initial producer...", YELLOW);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            // Connect to first producer
            Exchange::Core::String firstUuid;
            {
                Consumer consumer(queueName, 64);
                firstUuid = consumer.getSessionUuid();
                log("Consumer[Child]", "Connected to producer 1, UUID: " + firstUuid.toString(), GREEN);
                
                // Read a message
                uint8_t buffer[4096];
                int attempts = 0;
                while (attempts < 10) {
                    uint32_t bytes = consumer.read(buffer, sizeof(buffer));
                    if (bytes > 0) {
                        IpcMessage msg;
                        IpcMessage::decode(buffer, bytes, msg);
                        auto sym = msg.getString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL));
                        log("Consumer[Child]", "Received from producer 1: " + *sym, GREEN);
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    attempts++;
                }
            }
            
            // Wait for producer crash and restart
            log("Consumer[Child]", "Waiting for producer crash and restart...", YELLOW);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Try to connect to new producer (should fail with stale queue first)
            log("Consumer[Child]", "Attempting to reconnect...", YELLOW);
            bool detectedStale = false;
            try {
                Consumer consumer(queueName, 64);
                // If we get here, either the UUID matches (bad) or we're connecting to new session (good)
                Exchange::Core::String secondUuid = consumer.getSessionUuid();
                if (secondUuid == firstUuid) {
                    log("Consumer[Child]", "ERROR: Still on old UUID!", RED);
                    exit(1);
                }
            } catch (const Engine::EngException& e) {
                Exchange::Core::String errMsg = e.what();
                if (errMsg.find("Stale") != std::string::npos) {
                    detectedStale = true;
                    log("Consumer[Child]", "Detected stale queue as expected", GREEN);
                }
            }
            
            // Now connect to new producer
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            {
                Consumer consumer(queueName, 64);
                Exchange::Core::String secondUuid = consumer.getSessionUuid();
                log("Consumer[Child]", "Connected to producer 2, UUID: " + secondUuid.toString(), GREEN);
                
                if (secondUuid == firstUuid) {
                    log("Consumer[Child]", "ERROR: UUIDs should be different!", RED);
                    exit(1);
                }
                
                // Read message from new producer
                uint8_t buffer[4096];
                int attempts = 0;
                while (attempts < 10) {
                    uint32_t bytes = consumer.read(buffer, sizeof(buffer));
                    if (bytes > 0) {
                        IpcMessage msg;
                        IpcMessage::decode(buffer, bytes, msg);
                        auto sym = msg.getString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL));
                        log("Consumer[Child]", "Received from producer 2: " + *sym, GREEN);
                        exit(0); // Success
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    attempts++;
                }
                
                log("Consumer[Child]", "ERROR: Failed to read from new producer", RED);
                exit(1);
            }
            
        } catch (const std::exception& e) {
            log("Consumer[Child]", std::string("ERROR: ") + e.what(), RED);
            exit(1);
        }
        exit(1); // Should not reach here
    }
    
    // Parent process - Producer
    try {
        // First producer
        log("Producer[Parent]", "Starting producer 1...", YELLOW);
        {
            Producer producer(queueName, 64);
            log("Producer[Parent]", "Producer 1 created", GREEN);
            
            // Send a message
            IpcMessage msg;
            msg.setMsgType(MsgType::NEW_ORDER);
            msg.addString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL), "PROD1");
            msg.addUint64(static_cast<uint16_t>(FieldId::FIELD_QTY), 100);
            msg.finalize();
            
            std::vector<uint8_t> encoded;
            msg.encode(encoded);
            producer.write(encoded.data(), encoded.size());
            log("Producer[Parent]", "Sent message from producer 1", GREEN);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        // Producer 1 "crashes" (destructor called, but shared memory persists)
        log("Producer[Parent]", "Producer 1 crashed (simulated)", RED);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // Second producer (recovery)
        log("Producer[Parent]", "Starting producer 2 (recovery)...", YELLOW);
        {
            Producer producer(queueName, 64);
            log("Producer[Parent]", "Producer 2 created with new UUID", GREEN);
            
            // Send a message from new producer
            IpcMessage msg;
            msg.setMsgType(MsgType::NEW_ORDER);
            msg.addString(static_cast<uint16_t>(FieldId::FIELD_SYMBOL), "PROD2");
            msg.addUint64(static_cast<uint16_t>(FieldId::FIELD_QTY), 200);
            msg.finalize();
            
            std::vector<uint8_t> encoded;
            msg.encode(encoded);
            producer.write(encoded.data(), encoded.size());
            log("Producer[Parent]", "Sent message from producer 2", GREEN);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Wait for consumer to finish
        int status;
        waitpid(consumerPid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            log("TEST 3", "PASSED - Crash recovery verified", GREEN);
            return true;
        } else {
            log("TEST 3", "FAILED - Consumer reported error", RED);
            return false;
        }
        
    } catch (const std::exception& e) {
        log("TEST 3", std::string("FAILED - ") + e.what(), RED);
        kill(consumerPid, SIGKILL);
        return false;
    }
}

int main() {
    std::cout << "\n" << MAGENTA << "========================================" << RESET << std::endl;
    std::cout << MAGENTA << "  IPC Queue Connection & Crash Recovery" << RESET << std::endl;
    std::cout << MAGENTA << "========================================" << RESET << "\n" << std::endl;
    
    int passed = 0;
    int total = 3;
    
    // Test 1: Same queue connection
    if (TEST1_sameQueueConnection()) {
        passed++;
    }
    std::cout << std::endl;
    
    // Test 2: Stale queue detection
    if (TEST2_staleQueueDetection()) {
        passed++;
    }
    std::cout << std::endl;
    
    // Test 3: Producer crash and recovery
    if (TEST3_producerCrashRecovery) {
        passed++;
    }
    std::cout << std::endl;
    
    // Summary
    std::cout << MAGENTA << "========================================" << RESET << std::endl;
    std::cout << "Test Results: " << (passed == total ? GREEN : RED) 
              << passed << "/" << total << " passed" << RESET << std::endl;
    std::cout << MAGENTA << "========================================" << RESET << std::endl;
    
    return (passed == total) ? 0 : 1;
}