#include "Server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <system_error>

#include "Commands.h"
#include "RespParser.h"

namespace sparkDB {
    struct Connection {
        std::string readBuffer;
        std::string writeBuffer;
        std::chrono::steady_clock::time_point lastActivity;
        bool closing = false; // protocol error sent; close once writeBuffer is empty
    };

    namespace {
        void Check(bool ok, const char *what) {
            if (!ok) {
                throw std::system_error(errno, std::generic_category(), what);
            }
        }

        void UpdateEpoll(int epollFd, int op, int fd, std::uint32_t events) {
            epoll_event event{};
            event.events = events;
            event.data.fd = fd;
            Check(epoll_ctl(epollFd, op, fd, &event) == 0, "epoll_ctl");
        }
    }

    Server::Server(KvStore &store, ServerConfig config) : store_(store), config_(config) {
        epollFd_ = epoll_create1(EPOLL_CLOEXEC);
        Check(epollFd_ >= 0, "epoll_create1");
        stopFd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        Check(stopFd_ >= 0, "eventfd");
        UpdateEpoll(epollFd_, EPOLL_CTL_ADD, stopFd_, EPOLLIN);
    }

    Server::~Server() {
        CloseAll();
        if (listenFd_ >= 0) {
            close(listenFd_);
        }
        close(stopFd_);
        close(epollFd_);
    }

    bool Server::Listen(std::uint16_t port) {
        const int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            return false;
        }

        const int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t length = sizeof(addr);
        if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
            listen(fd, SOMAXCONN) != 0 ||
            getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &length) != 0) {
            close(fd);
            return false;
        }

        listenFd_ = fd;
        port_ = ntohs(addr.sin_port);
        UpdateEpoll(epollFd_, EPOLL_CTL_ADD, listenFd_, EPOLLIN);
        return true;
    }

    std::uint16_t Server::Port() const {
        return port_;
    }

    void Server::Run() {
        const auto waitMs = static_cast<int>(std::min<std::chrono::milliseconds::rep>(
            1000, std::max<std::chrono::milliseconds::rep>(1, config_.idleTimeout.count())));
        epoll_event events[64];

        while (true) {
            const int count = epoll_wait(epollFd_, events, std::size(events), waitMs);
            Check(count >= 0 || errno == EINTR, "epoll_wait");

            for (int i = 0; i < count; ++i) {
                const int fd = events[i].data.fd;
                if (fd == stopFd_) {
                    std::uint64_t value;
                    (void) read(stopFd_, &value, sizeof(value));
                    CloseAll();
                    return;
                }
                if (fd == listenFd_) {
                    AcceptClients();
                } else {
                    HandleClient(fd, events[i].events);
                }
            }

            CloseIdleClients();
        }
    }

    void Server::Stop() {
        const std::uint64_t one = 1;
        (void) write(stopFd_, &one, sizeof(one));
    }

    void Server::AcceptClients() {
        while (true) {
            const int fd = accept4(listenFd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (fd < 0) {
                if (errno == EINTR || errno == ECONNABORTED) {
                    continue;
                }
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    std::cerr << "accept: " << std::strerror(errno) << '\n';
                }
                return;
            }

            if (connections_.size() >= config_.maxClients) {
                const std::string reply = ToResp({ResponseType::Error, "ERR max number of clients reached"});
                (void) send(fd, reply.data(), reply.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
                close(fd);
                continue;
            }

            UpdateEpoll(epollFd_, EPOLL_CTL_ADD, fd, EPOLLIN);
            connections_[fd].lastActivity = std::chrono::steady_clock::now();
        }
    }

    void Server::HandleClient(int fd, std::uint32_t events) {
        if (events & (EPOLLERR | EPOLLHUP)) {
            CloseClient(fd);
            return;
        }
        if ((events & EPOLLIN) && !ReadFrom(fd)) {
            CloseClient(fd);
            return;
        }
        if (!Flush(fd)) {
            CloseClient(fd);
        }
    }

    bool Server::ReadFrom(int fd) {
        Connection &connection = connections_.at(fd);
        if (connection.closing) {
            return true;
        }

        char chunk[4096];
        const ssize_t n = read(fd, chunk, sizeof(chunk));
        if (n == 0) {
            return false;
        }
        if (n < 0) {
            return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
        }
        connection.readBuffer.append(chunk, static_cast<std::size_t>(n));
        connection.lastActivity = std::chrono::steady_clock::now();

        std::size_t offset = 0;
        while (true) {
            ParseResult result = ParseRequest(std::string_view(connection.readBuffer).substr(offset));
            if (result.status == ParseStatus::Incomplete) {
                break;
            }
            if (result.status == ParseStatus::Error) {
                connection.writeBuffer += ToResp({ResponseType::Error, result.error});
                connection.readBuffer.clear();
                connection.closing = true;
                return true;
            }
            offset += result.consumed;
            if (!result.args.empty()) {
                connection.writeBuffer += ToResp(Execute(store_, result.args));
            }
        }
        connection.readBuffer.erase(0, offset);
        return true;
    }

    bool Server::Flush(int fd) {
        Connection &connection = connections_.at(fd);
        while (!connection.writeBuffer.empty()) {
            const ssize_t n = send(fd, connection.writeBuffer.data(), connection.writeBuffer.size(), MSG_NOSIGNAL);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    return false;
                }
                break;
            }
            connection.writeBuffer.erase(0, static_cast<std::size_t>(n));
            connection.lastActivity = std::chrono::steady_clock::now();
        }

        if (connection.writeBuffer.empty() && connection.closing) {
            return false;
        }
        std::uint32_t events = connection.closing ? 0u : EPOLLIN;
        if (!connection.writeBuffer.empty()) {
            events |= EPOLLOUT;
        }
        UpdateEpoll(epollFd_, EPOLL_CTL_MOD, fd, events);
        return true;
    }

    void Server::CloseIdleClients() {
        const auto now = std::chrono::steady_clock::now();
        for (auto it = connections_.begin(); it != connections_.end();) {
            const int fd = it->first;
            const bool idle = now - it->second.lastActivity > config_.idleTimeout;
            ++it;
            if (idle) {
                CloseClient(fd);
            }
        }
    }

    void Server::CloseClient(int fd) {
        epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        connections_.erase(fd);
    }

    void Server::CloseAll() {
        while (!connections_.empty()) {
            CloseClient(connections_.begin()->first);
        }
    }
}
