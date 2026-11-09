#ifndef SPARKDB_SERVER_H
#define SPARKDB_SERVER_H

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "KvStore.h"

namespace sparkDB {
    struct ServerConfig {
        std::size_t maxClients = 1000;
    };

    struct Connection;

    // Single-threaded, level-triggered epoll server speaking RESP2.
    class Server {
    public:
        explicit Server(KvStore &store, ServerConfig config = {});
        ~Server();

        Server(const Server &) = delete;
        Server &operator=(const Server &) = delete;

        bool Listen(std::uint16_t port); // port 0 picks a free port
        std::uint16_t Port() const; // the port actually bound
        void Run(); // runs the loop until Stop()
        void Stop(); // thread-safe; makes Run() return

    private:
        void AcceptClients();
        void HandleClient(int fd, std::uint32_t events);
        bool ReadFrom(int fd); // false if the connection should close
        bool Flush(int fd); // false if the connection should close
        void CloseClient(int fd);
        void CloseAll();

        KvStore &store_;
        ServerConfig config_;
        int epollFd_ = -1;
        int stopFd_ = -1;
        int listenFd_ = -1;
        std::uint16_t port_ = 0;
        std::unordered_map<int, Connection> connections_;
    };
}

#endif //SPARKDB_SERVER_H
