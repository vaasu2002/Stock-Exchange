#pragma once

#include <memory>

// Linux specific headers for networking and threading
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "Exception.h"
#include "BlockingQueue/IBlockingQueue.h"
#include "../Config.h"

namespace Exchange::Gateway::Network {

    extern Config& gc;

    struct RawPacket {
        int clientSocket;
        Core::String data;
    };

    class TcpEpollListener {
    public: 
        using BlockingQueue = std::shared_ptr<Core::IBlockingQueue<RawPacket>>;

        /** @brief Constructor */
        TcpEpollListener(BlockingQueue q):mIngesssQueue(std::move(q)){}

        void run(std::atomic<bool>* stopFlag);

    private:
        BlockingQueue mIngesssQueue;
        int mServerFd{-1};
        int mEpollFd{-1};

        void setupServer();
        void eventLoop(std::atomic<bool>* stopFlag);

        void handleAccept();
        void handleRead(int clientFd);

        void shutdown();
        // /**
        //  * @brief Runs the main TCP networking event loop using epoll.
        //  * 
        //  * @details
        //  * This function initializes a TCP server socket, binds it to the configured
        //  * port, and registers it with the Linux epoll subsystem to efficiently monitor
        //  * network activity. The server socket accepts incoming client connections,
        //  * while connected client sockets are monitored for incoming data.
        //  *
        //  * The loop blocks inside epoll_wait(), consuming no CPU when idle, and wakes
        //  * only when meaningful network events occur (new connections or incoming data).
        //  * Incoming bytes are read in non-blocking mode and forwarded to a shared
        //  * ingress queue for downstream processing (e.g., FIX parsing and validation).
        //  *
        //  * The design prioritizes low latency and scalability by avoiding per-connection
        //  * polling, minimizing context switches, and delegating protocol handling to
        //  * downstream components.
        //  *
        //  * The loop continues running until the provided stop flag is set, after which
        //  * all sockets are closed and the listener shuts down cleanly.
        //  */
        // void networkLoop() {
        //     // Config& config = Config::instance();
        //     // Create a TCP socket using IPv4
        //     // AF_INET      -> IPv4
        //     // SOCK_STREAM  -> TCP
        //     // Protocol 0   -> Let the OS select the default protocol
        //     int serverFd = socket(AF_INET, SOCK_STREAM, 0);
        //     if (serverFd < 0) {
        //         ENG_THROW("Socket creation failed")
        //         return;
        //     }

        //     // Enable SO_REUSEADDR to allow immediate reuse of the port after shutdown.
        //     // Without this option, the OS may keep the port in TIME_WAIT state for 60 sec,
        //     // preventing quick restarts during development or recovery.
        //     int opt = 1;
        //     setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        //     // Configure the server address
        //     // - sockaddr_in is the IPv4-specific socket structure
        //     // - INADDR_ANY allows listening on all available network interfaces
        //     // - htons() converts the port from host byte order to network byte order
        //     //   (network byte order is big-endian)
        //     sockaddr_in address{};
        //     address.sin_family = AF_INET;
        //     address.sin_addr.s_addr = INADDR_ANY;
        //     address.sin_port = htons(PORT);

        //     // Bind the socket to the configured IP address and port
        //     if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        //         ENG_THROW("Bind failed");
        //         return;
        //     }

        //     // Put the socket into passive mode so it can accept incoming connections.
        //     // The backlog defines how many pending connections can wait in the queue
        //     // before new connection attempts are rejected.
        //     listen(serverFd, 10);

        //     /**
        //      * Create an epoll instance to efficiently monitor multiple sockets.
        //      * epoll allows the kernel to notify us only when meaningful events occur,
        //      * avoiding inefficient polling and unnecessary CPU usage.
        //     */
        //     int epollFd = epoll_create1(0);

        //     // epoll_event:
        //     // - event  → configuration used when registering a socket
        //     // - events → array filled by the kernel with sockets that have activity
        //     epoll_event event{}, events[MAX_EVENTS];

        //     // EPOLLIN means "data available to read"
        //     // - For the server socket: a new client connection is ready to accept
        //     // - For client sockets: incoming data is available
        //     event.events = EPOLLIN;
        //     event.data.fd = serverFd;

        //     // Register the server socket with epoll
        //     epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &event);

        //     LOG_INFO("Gateway is listening on port %d", PORT);

        //     // Buffer used to read raw bytes from client sockets
        //     char buffer[BUFFER_SIZE];
        // }
    };
}