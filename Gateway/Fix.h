#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <string>
#include <cstring>
#include <sstream>

// Linux specific headers for networking and threading
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "Logger/Logger.h"
#include "BlockingQueue/MutexBlockingQueue.h"
#include "Exception.h"

namespace Exchange::Gateway {

    constexpr char FIX_DELIMITER = '\x01'; // SOH character standard in FIX
    constexpr int MAX_EVENTS = 100;
    constexpr int PORT = 9000;
    constexpr int BUFFER_SIZE = 4096;

    // void pinThreadToCpuCore(std::thread &t, int coreId) {
    //     cpu_set_t cpuset;
    //     CPU_ZERO(&cpuset);
    //     CPU_SET(coreId, &cpuset);
    //     pthread_t current_thread = pthread_self();
    //     pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
    //     LOG_INFO("Thread pinned to Core %d",coreId);
    // }

    void pinThreadToCore(int coreId) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(coreId, &cpuset);
        pthread_t current_thread = pthread_self();
        pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
        LOG_INFO("Thread pinned to Core %d",coreId);
    }
    // Global queue to pass raw data from Network to Logic
    struct RawPacket {
        int clientSocket;
        Core::String data;
    };

    Core::IBlockingQueue<RawPacket>* gIngressQueue = new Core::MutexBlockingQueue<RawPacket>(4096);

    struct FixMessage {
        std::string msgType; // Tag 35
        std::string symbol;  // Tag 55
        std::string side;    // Tag 54
        double price;        // Tag 44
        int quantity;        // Tag 38
        bool isValid;
    };

    // A "Zero-Copy" style parser would be faster, but string split is easier to understand
    FixMessage parseFix(const std::string& raw) {

        LOG_TRACE(static_cast<Core::String>(raw));

        FixMessage msg;
        msg.isValid = false;
        
        std::stringstream ss(raw);
        std::string segment;
        
        // Split by SOH delimiter (\x01)
        while (std::getline(ss, segment, FIX_DELIMITER)) {
            size_t eq_pos = segment.find('=');
            if (eq_pos == std::string::npos) continue;

            std::string tag = segment.substr(0, eq_pos);
            std::string value = segment.substr(eq_pos + 1);

            if (tag == "35") msg.msgType = value;
            else if (tag == "55") msg.symbol = value;
            else if (tag == "54") msg.side = value; // 1=Buy, 2=Sell
            else if (tag == "44") msg.price = std::stod(value);
            else if (tag == "38") msg.quantity = std::stoi(value);
        }
        
        // Basic validation: Must have MsgType
        if (!msg.msgType.empty()) {
            msg.isValid = true;
        }
        return msg;
    }

    void networkLoop() {
        // Force this specific thread to run only on CPU Core #1
        // If the Operating System moves this thread to a different core (context switch), 
        // the CPU cache has to be reloaded, costing microseconds. In trading, we want 
        // "Hot Cache" at all times.
        // pinThreadToCore(1);

        // Address Family: AF_INET, tells OS we are using IPv4
        // Socket Type: SOCK_STREAM, tells OS we want TCP
        // Protocol: 0, lets OS choose the default protocol for given family and type
        int serverFd = socket(AF_INET, SOCK_STREAM, 0);

        // Setting the strcuture that holds the configration
        // - sockaddr_in: A struct specifically designed for IPv4 addresses.
        // - INADDR_ANY: This is a shortcut. It tells the server to listen on all available network interfaces.
        //   If your server has a Local IP (127.0.0.1) and a Public Wi-Fi IP (192.168.1.5), 
        //   INADDR_ANY will accept connections arriving at either IP.
        // - htons(PORT): Host to Network Short
        //   Computers store numbers in different orders (Endianness). Intel CPUs are "Little Endian" (start with 
        //   the smallest byte). The Internet is "Big Endian" (start with the largest byte). If you don't use htons,
        //   Port 9000 might be interpreted by the network as Port 36899. This function flips the bytes so the network
        //   understands the number correctly.
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        // Sets REUSEADDR
        // When you stop a server (or it crashes), the OS puts the port (e.g., 9000) into a TIME_WAIT state for 
        // about 60 seconds to ensure all lingering packets are flushed. During this time, you cannot restart
        // your server; you will get an "Address already in use" error.
        // SO_REUSEADDR tells the OS: "I know this port is technically busy cleaning up, but let me bind to 
        // it anyway immediately."
        // Without this, every time you re-compile and run your code, you'd have to wait 1 minute for the port
        // to free up.
        int opt = 1;
        setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // Connects the abstract socket ID (serverFd) to the specific IP and Port configuration (address)
        if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            // Another program is already using that port. (as bind returns < 0)
            // Or You are trying to use a restricted port (0–1023) without 
            // Root/Administrator privileges.
            perror("Bind failed"); return;
        }

        // When the socket is made the socket could be used to make outgoing calls.
        // listen flips the switch telling the OS "This socket is not making calls, but waiting for them."
        // Puts the socket in passive mode, ready to accept incoming connection requests.
        // Backlog queue size: 10
        // If 15 clients try to connect at the exact same nanosecond, The first 10 go into a holding queue.
        // The 11th through 15th receive a "Connection Refused" error immediately.
        listen(serverFd, 10);

        /*
        This block sets up the "Traffic Control Tower" for your network thread. Instead of checking
        every connection one by one to see if they have data (which is slow), you are asking the Linux
        Kernel to watch them for you.
        You can think of this ID as a handle to a "To-Do List" managed by the Operating System.
        */
        // Create Epoll Instance
        int epollFd = epoll_create1(0); // ID of that epoll instance
        
        // This declares two variables:
        // - event (Singular): This is a Configuration Form. We use this temporarily to describe what we want to
        //   watch (e.g., "Watch Socket 5 for incoming data").
        // - events (Array): This is the Inbox. When we later call epoll_wait(), the Kernel will 
        //   fill this array with the list of sockets that actually have activity. If 5 clients
        //   send orders at once, this array will contain 5 items.
        epoll_event event{}, events[MAX_EVENTS];
        
        // This stands for "Epoll Input". It tells the Kernel: "Wake me up if there is data to read."
        // - For a Client Socket: "Data to read" means they sent a FIX message.
        // - For the Server Socket: "Data to read" means a new user is trying to connect (the TCP 3-way handshake is complete).
        event.events = EPOLLIN; // Read available

        // We attach the ID of the socket (serverFd) to this event. When the event
        // fires later, the Kernel will hand this ID back to us so we know who triggered the event.
        event.data.fd = serverFd;

        // Registering with the Kernel
        // This is the actual system call that submits the form.
        //  - EPOLL_CTL_ADD: The Command. We are Adding a new socket to the watch list.
        //  - epollFd: The Traffic Tower instance we created in step 1.
        //  - serverFd: The target socket we want to watch.
        //  - &event: The rules we defined in step 3 (Watch for EPOLLIN).
        // "Hey Linux (epollFd), please add (ADD) the Main Server Phone (serverFd) to your
        // watch list. Use the rules written on this form (&event), which say you should notify
        // me whenever it rings (EPOLLIN)."
        epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &event);

        LOG_INFO("Gateway is listening on port, %d", PORT);

        // This creates a raw memory array (usually 4KB or 8KB) on the stack.
        // When data eventually arrives, we will use read() to pour the raw bytes from the
        // network card into this bucket so our application can process them.
        // Without epoll, you would have to loop through every client constantly:
        // Are you sending data? No. Are you sending data? No. Are you sending data? No.
        // With epoll, your code does nothing (consumes 0 CPU) until the Kernel taps it on the shoulder and says:
        // - "Socket 7 and Socket 12 have data. Here is the list."
        char buffer[BUFFER_SIZE];

        // Main event loop (Engine of your network thread)
        // This loop runs millions of times a day. It sleeps when the market is quiet and
        // wakes up instantly when activity happens.
        while (true) {
            // Wait for events
            // Blocking call, wakes up on network activity
            // The pause button
            // The Linux Kernel holds the thread here until at least one interesting thing happens (e.g., a client sends an order, or a new user tries to connect).

            int eventcount = epoll_wait(epollFd, events, MAX_EVENTS, -1);

            for (int i = 0; i < eventcount; i++) {

                // New Connection
                if (events[i].data.fd == serverFd) {
                    int newSocket = accept(serverFd, nullptr, nullptr);
                    
                    // Set non-blocking for new client
                    fcntl(newSocket, F_SETFL, O_NONBLOCK);
                    
                    event.events = EPOLLIN | EPOLLET; // Edge Triggered
                    event.data.fd = newSocket;
                    epoll_ctl(epollFd, EPOLL_CTL_ADD, newSocket, &event);
                    LOG_INFO("New Client Connected:, %d", newSocket);
                }
                // Data arrived 
                else {
                    int clientFd = events[i].data.fd;
                    int bytesRead = read(clientFd, buffer, BUFFER_SIZE);

                    if (bytesRead <= 0) {
                        // Client disconnected
                        close(clientFd);
                        epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
                    } 
                    else {
                        std::string rawData(buffer, bytesRead);
                        gIngressQueue->push({clientFd, rawData});
                    }
                }
            }
        }
    }


    void consumerLoop() {
        // pinThreadToCore(2);
        LOG_INFO("Consumer started..");

        while(true) {
            RawPacket packet;
            gIngressQueue->pop(packet);
            FixMessage fix = parseFix(packet.data.toString());

            if (fix.isValid) {
                if (fix.msgType == "D") { // New Order Single
                    LOG_INFO("ORDER RECEIVED Client: %d BUY: %d %s @ %.2f", packet.clientSocket, fix.quantity, fix.symbol.c_str(), fix.price);
                    // todo: send to scheduler process using IPC   
                } 
                else if (fix.msgType == "A") { // Logon
                    LOG_INFO("LOGON Request from Client %d", packet.clientSocket);
                }
            } 
            else {
                LOG_WARN("Noise or Partial Packet");
            }
        }
    }
}
