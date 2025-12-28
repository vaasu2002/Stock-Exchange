
#include "TcpEpollListener.h"

namespace Exchange::Gateway::Network {

    static Config& gCfg() { return Config::instance(); }

    void TcpEpollListener::run(std::atomic<bool>* stopFlag) {
        setupServer();
        eventLoop(stopFlag);
        shutdown();
    }

    void TcpEpollListener::setupServer() {
        mServerFd = socket(AF_INET, SOCK_STREAM, 0);

        int opt = 1;
        setsockopt(mServerFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(gCfg().port());

        if (bind(mServerFd, (sockaddr*)&address, sizeof(address)) < 0) {
            perror("bind");
            throw std::runtime_error("Server bind failed");
        }

        listen(mServerFd, 10);

        mEpollFd = epoll_create1(0);

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = mServerFd;
        epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mServerFd, &event);
    }

    void TcpEpollListener::eventLoop(std::atomic<bool>* stopFlag) {
        epoll_event events[gCfg().maxFixEventSize()];
        char buffer[1000];

        while (!stopFlag->load(std::memory_order_acquire)) {

            int count = epoll_wait(mEpollFd, events, gCfg().maxFixEventSize(), 1000);
            if (count <= 0) continue;

            for (int i = 0; i < count; ++i) {
                int fd = events[i].data.fd;

                if (fd == mServerFd) {
                    handleAccept();
                } else {
                    handleRead(fd);
                }
            }
        }
    }

    void TcpEpollListener::handleAccept() {
        int clientFd = accept(mServerFd, nullptr, nullptr);
        if (clientFd < 0) return;

        fcntl(clientFd, F_SETFL, O_NONBLOCK);

        epoll_event event{};
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = clientFd;

        epoll_ctl(mEpollFd, EPOLL_CTL_ADD, clientFd, &event);
    }

    void TcpEpollListener::handleRead(int clientFd) {
        char buffer[1000];
        int bytesRead = read(clientFd, buffer, sizeof(buffer));

        if (bytesRead <= 0) {
            close(clientFd);
            epoll_ctl(mEpollFd, EPOLL_CTL_DEL, clientFd, nullptr);
            return;
        }

        mIngesssQueue->push({clientFd, std::string(buffer, bytesRead)});
    }

    void TcpEpollListener::shutdown() {
        close(mEpollFd);
        close(mServerFd);
        mIngesssQueue->close();
    }
}