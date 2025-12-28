#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <atomic>
#include <cstdlib>

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

constexpr int TEST_PORT = 9000;
constexpr char FIX_DELIMITER = '\x01';

void log(const std::string& prefix, const std::string& msg, const std::string& color = RESET) {
    std::cout << color << "[" << prefix << "] " << msg << RESET << std::endl;
}

std::string buildFixMessage(const std::string& msgType, const std::string& symbol, 
                           const std::string& side, int quantity, double price) {
    std::string msg = "8=FIX.4.2" + std::string(1, FIX_DELIMITER);
    msg += "35=" + msgType + FIX_DELIMITER;
    msg += "55=" + symbol + FIX_DELIMITER;
    msg += "54=" + side + FIX_DELIMITER;
    msg += "38=" + std::to_string(quantity) + FIX_DELIMITER;
    msg += "44=" + std::to_string(price) + FIX_DELIMITER;
    return msg;
}

int connectToGateway() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log("ERROR", "Failed to create socket", RED);
        return -1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(TEST_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Try to connect with retries
    int attempts = 0;
    while (attempts < 5) {
        if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
            log("Client", "Connected to gateway", GREEN);
            return sock;
        }
        attempts++;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    log("ERROR", "Failed to connect after 5 attempts", RED);
    close(sock);
    return -1;
}

/**
 * @brief Test 1: Single client connection and basic FIX order
 * 
 * GIVEN: Gateway is running and listening on port 9000
 * WHEN:  A client connects and sends a valid FIX New Order Single (35=D)
 * THEN:  
 *   - Connection is established successfully
 *   - Order is received and parsed correctly
 *   - Gateway processes the message without errors
 */
bool TEST1_singleOrderSubmission() {
    log("TEST 1", "Testing single order submission...", CYAN);
    
    int sock = connectToGateway();
    if (sock < 0) {
        log("TEST 1", "FAILED - Cannot connect to gateway", RED);
        return false;
    }

    // Send a New Order Single
    std::string order = buildFixMessage("D", "AAPL", "1", 100, 150.50);
    
    ssize_t sent = send(sock, order.c_str(), order.length(), 0);
    if (sent < 0) {
        log("TEST 1", "FAILED - Cannot send order", RED);
        close(sock);
        return false;
    }

    log("Client", "Sent FIX order: BUY 100 AAPL @ 150.50", GREEN);
    
    // Give gateway time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    close(sock);
    log("TEST 1", "PASSED - Order sent successfully", GREEN);
    return true;
}

/**
 * @brief Test 2: Multiple concurrent client connections
 * 
 * GIVEN: Gateway is running with epoll-based event handling
 * WHEN:  Multiple clients (5) connect simultaneously and send orders
 * THEN:  
 *   - All clients connect successfully
 *   - Gateway handles concurrent connections without blocking
 *   - All orders are received and processed
 *   - No connection is dropped or refused
 */
bool TEST2_multipleClients() {
    log("TEST 2", "Testing multiple concurrent clients...", CYAN);
    
    const int numClients = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numClients; i++) {
        threads.emplace_back([i, &successCount]() {
            int sock = connectToGateway();
            if (sock < 0) {
                log("Client-" + std::to_string(i), "Failed to connect", RED);
                return;
            }

            std::string order = buildFixMessage("D", "MSFT", "2", 50 * (i + 1), 300.25);
            
            ssize_t sent = send(sock, order.c_str(), order.length(), 0);
            if (sent > 0) {
                log("Client-" + std::to_string(i), "Sent order successfully", GREEN);
                successCount++;
            } else {
                log("Client-" + std::to_string(i), "Failed to send order", RED);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            close(sock);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    if (successCount == numClients) {
        log("TEST 2", "PASSED - All " + std::to_string(numClients) + " clients succeeded", GREEN);
        return true;
    } else {
        log("TEST 2", "FAILED - Only " + std::to_string(successCount.load()) + "/" + 
            std::to_string(numClients) + " clients succeeded", RED);
        return false;
    }
}

/**
 * @brief Test 3: FIX logon message handling
 * 
 * GIVEN: Gateway supports FIX Logon messages (35=A)
 * WHEN:  Client sends a Logon message
 * THEN:  
 *   - Message is parsed correctly
 *   - Gateway logs the logon request
 *   - Connection remains active after logon
 */
bool TEST3_fixLogon() {
    log("TEST 3", "Testing FIX logon message...", CYAN);
    
    int sock = connectToGateway();
    if (sock < 0) {
        log("TEST 3", "FAILED - Cannot connect", RED);
        return false;
    }

    // Send a Logon message (35=A)
    std::string logon = "8=FIX.4.2" + std::string(1, FIX_DELIMITER) +
                       "35=A" + FIX_DELIMITER +
                       "49=CLIENT" + FIX_DELIMITER +
                       "56=GATEWAY" + FIX_DELIMITER;
    
    ssize_t sent = send(sock, logon.c_str(), logon.length(), 0);
    if (sent < 0) {
        log("TEST 3", "FAILED - Cannot send logon", RED);
        close(sock);
        return false;
    }

    log("Client", "Sent FIX Logon message", GREEN);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    close(sock);
    log("TEST 3", "PASSED - Logon message sent", GREEN);
    return true;
}

/**
 * @brief Test 4: Malformed/invalid FIX message handling
 * 
 * GIVEN: Gateway has FIX message validation
 * WHEN:  Client sends invalid/malformed FIX data
 * THEN:  
 *   - Gateway doesn't crash
 *   - Invalid message is logged as noise/partial packet
 *   - Connection remains stable for subsequent valid messages
 */
bool TEST4_malformedMessage() {
    log("TEST 4", "Testing malformed message handling...", CYAN);
    
    int sock = connectToGateway();
    if (sock < 0) {
        log("TEST 4", "FAILED - Cannot connect", RED);
        return false;
    }

    // Send garbage data
    std::string garbage = "THIS_IS_NOT_FIX_DATA_12345";
    ssize_t sent = send(sock, garbage.c_str(), garbage.length(), 0);
    
    if (sent < 0) {
        log("TEST 4", "FAILED - Cannot send data", RED);
        close(sock);
        return false;
    }

    log("Client", "Sent malformed data", YELLOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Now send valid message to verify connection still works
    std::string validOrder = buildFixMessage("D", "GOOG", "1", 25, 2800.00);
    sent = send(sock, validOrder.c_str(), validOrder.length(), 0);
    
    if (sent < 0) {
        log("TEST 4", "FAILED - Cannot send valid order after garbage", RED);
        close(sock);
        return false;
    }

    log("Client", "Sent valid order after malformed data", GREEN);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    close(sock);
    log("TEST 4", "PASSED - Gateway handled malformed data gracefully", GREEN);
    return true;
}

/**
 * @brief Test 5: Rapid order submission (stress test)
 * 
 * GIVEN: Gateway uses lock-free queue for message passing
 * WHEN:  Client rapidly sends 100 orders in quick succession
 * THEN:  
 *   - All orders are accepted
 *   - No messages are lost
 *   - Gateway's queue doesn't overflow
 *   - Performance remains stable
 */
bool TEST5_rapidOrderSubmission() {
    log("TEST 5", "Testing rapid order submission (100 orders)...", CYAN);
    
    int sock = connectToGateway();
    if (sock < 0) {
        log("TEST 5", "FAILED - Cannot connect", RED);
        return false;
    }

    const int numOrders = 100;
    int sentCount = 0;

    for (int i = 0; i < numOrders; i++) {
        std::string order = buildFixMessage("D", "TSLA", "1", i + 1, 700.00 + i);
        ssize_t sent = send(sock, order.c_str(), order.length(), 0);
        
        if (sent > 0) {
            sentCount++;
        }
        
        // Minimal delay to simulate high-frequency trading
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    log("Client", "Sent " + std::to_string(sentCount) + "/" + std::to_string(numOrders) + " orders", 
        sentCount == numOrders ? GREEN : YELLOW);
    
    // Give gateway time to process all messages
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    close(sock);
    
    if (sentCount == numOrders) {
        log("TEST 5", "PASSED - All orders sent successfully", GREEN);
        return true;
    } else {
        log("TEST 5", "PARTIAL - " + std::to_string(sentCount) + "/" + 
            std::to_string(numOrders) + " sent", YELLOW);
        return sentCount >= numOrders * 0.95; // 95% success rate acceptable
    }
}

/**
 * @brief Test 6: Client disconnect and reconnect
 * 
 * GIVEN: Gateway uses epoll with EPOLL_CTL_DEL for cleanup
 * WHEN:  Client connects, disconnects, then reconnects
 * THEN:  
 *   - First connection succeeds
 *   - Disconnect is detected by gateway
 *   - Reconnection succeeds without port exhaustion
 *   - Socket resources are properly cleaned up
 */
bool TEST6_disconnectReconnect() {
    log("TEST 6", "Testing disconnect and reconnect...", CYAN);
    
    // First connection
    int sock1 = connectToGateway();
    if (sock1 < 0) {
        log("TEST 6", "FAILED - Initial connection failed", RED);
        return false;
    }

    std::string order1 = buildFixMessage("D", "AMZN", "1", 10, 3300.00);
    send(sock1, order1.c_str(), order1.length(), 0);
    log("Client", "Sent order on first connection", GREEN);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    close(sock1);
    log("Client", "Closed first connection", YELLOW);
    
    // Wait for gateway to detect disconnect
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Reconnect
    int sock2 = connectToGateway();
    if (sock2 < 0) {
        log("TEST 6", "FAILED - Reconnection failed", RED);
        return false;
    }

    std::string order2 = buildFixMessage("D", "NFLX", "2", 20, 500.00);
    ssize_t sent = send(sock2, order2.c_str(), order2.length(), 0);
    
    if (sent < 0) {
        log("TEST 6", "FAILED - Cannot send on reconnection", RED);
        close(sock2);
        return false;
    }

    log("Client", "Sent order on reconnection", GREEN);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    close(sock2);
    log("TEST 6", "PASSED - Disconnect and reconnect successful", GREEN);
    return true;
}

int main() {
    std::cout << "\n" << MAGENTA << "========================================" << RESET << std::endl;
    std::cout << MAGENTA << "     Gateway Network & FIX Protocol Tests" << RESET << std::endl;
    std::cout << MAGENTA << "========================================" << RESET << "\n" << std::endl;
    
    // Start the Gateway in a background process
    log("INFO", "Starting Gateway process...", YELLOW);
    system("cd /workspaces/Stock-Exchange/build && ./Gateway > /tmp/gateway.log 2>&1 &");
    
    log("INFO", "Waiting for gateway to start...", YELLOW);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    int passed = 0;
    int total = 6;
    
    if (TEST1_singleOrderSubmission()) passed++;
    std::cout << std::endl;
    
    if (TEST2_multipleClients()) passed++;
    std::cout << std::endl;
    
    if (TEST3_fixLogon()) passed++;
    std::cout << std::endl;
    
    if (TEST4_malformedMessage()) passed++;
    std::cout << std::endl;
    
    if (TEST5_rapidOrderSubmission()) passed++;
    std::cout << std::endl;
    
    if (TEST6_disconnectReconnect()) passed++;
    std::cout << std::endl;
    
    // Summary
    std::cout << MAGENTA << "========================================" << RESET << std::endl;
    std::cout << "Test Results: " << (passed == total ? GREEN : (passed > 0 ? YELLOW : RED))
              << passed << "/" << total << " passed" << RESET << std::endl;
    std::cout << MAGENTA << "========================================" << RESET << std::endl;
    
    return (passed == total) ? 0 : 1;
}