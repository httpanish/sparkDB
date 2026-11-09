#include <catch2/catch_test_macros.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>

#include "KvStore.h"
#include "Server.h"

using sparkDB::KvStore;
using sparkDB::Server;
using sparkDB::ServerConfig;

namespace {
    // Runs a Server on a free port in a background thread for the test's lifetime.
    class RunningServer {
    public:
        explicit RunningServer(ServerConfig config = {}) : server_(store_, config) {
            REQUIRE(server_.Listen(0));
            thread_ = std::thread([this] { server_.Run(); });
        }

        ~RunningServer() {
            Stop();
        }

        // Stops the server and waits for Run() to return.
        void Stop() {
            if (thread_.joinable()) {
                server_.Stop();
                thread_.join();
            }
        }

        std::uint16_t Port() const {
            return server_.Port();
        }

    private:
        KvStore store_;
        Server server_;
        std::thread thread_;
    };

    // A blocking client socket with a receive timeout, so a bug fails the test instead of hanging it.
    class Client {
    public:
        // A small receiveBuffer forces the server into partial writes.
        explicit Client(std::uint16_t port, int receiveBuffer = 0) {
            fd_ = socket(AF_INET, SOCK_STREAM, 0);
            REQUIRE(fd_ >= 0);
            timeval timeout{2, 0};
            setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            if (receiveBuffer > 0) {
                setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &receiveBuffer, sizeof(receiveBuffer));
            }

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            REQUIRE(connect(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);
        }

        ~Client() {
            Close();
        }

        void Close() {
            if (fd_ >= 0) {
                close(fd_);
                fd_ = -1;
            }
        }

        void Send(const std::string &data) {
            std::size_t sent = 0;
            while (sent < data.size()) {
                const ssize_t n = send(fd_, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
                REQUIRE(n > 0);
                sent += static_cast<std::size_t>(n);
            }
        }

        // Reads exactly size bytes, or fewer if the connection closes or times out.
        std::string Read(std::size_t size) {
            std::string data;
            char chunk[65536];
            while (data.size() < size) {
                const ssize_t n = recv(fd_, chunk, std::min(sizeof(chunk), size - data.size()), 0);
                if (n <= 0) {
                    break;
                }
                data.append(chunk, static_cast<std::size_t>(n));
            }
            return data;
        }

        // Sends a request and reads a reply of the expected length.
        std::string Roundtrip(const std::string &request, std::size_t replySize) {
            Send(request);
            return Read(replySize);
        }

        // True if the server closed the connection (recv returns 0) before the timeout.
        bool SeesEof() {
            char byte;
            return recv(fd_, &byte, 1, 0) == 0;
        }

    private:
        int fd_ = -1;
    };

    std::string Command(std::initializer_list<std::string> args) {
        std::string out = "*" + std::to_string(args.size()) + "\r\n";
        for (const auto &arg: args) {
            out += "$" + std::to_string(arg.size()) + "\r\n" + arg + "\r\n";
        }
        return out;
    }
}

TEST_CASE("Server replies to PING") {
    RunningServer server;
    Client client(server.Port());
    const std::string reply = "+PONG\r\n";
    REQUIRE(client.Roundtrip(Command({"PING"}), reply.size()) == reply);
}

TEST_CASE("Server runs SET and GET, and skips *0") {
    RunningServer server;
    Client client(server.Port());
    REQUIRE(client.Roundtrip(Command({"SET", "k", "v"}), 5) == "+OK\r\n");
    REQUIRE(client.Roundtrip("*0\r\n" + Command({"GET", "k"}), 7) == "$1\r\nv\r\n");
}

TEST_CASE("Server serves a client while another is idle") {
    RunningServer server;
    Client idle(server.Port());
    Client active(server.Port());
    REQUIRE(active.Roundtrip(Command({"SET", "k", "v"}), 5) == "+OK\r\n");
    REQUIRE(active.Roundtrip(Command({"GET", "k"}), 7) == "$1\r\nv\r\n");
}

TEST_CASE("Server shares the store between clients") {
    RunningServer server;
    Client a(server.Port());
    Client b(server.Port());
    REQUIRE(a.Roundtrip(Command({"SET", "k", "one"}), 5) == "+OK\r\n");
    REQUIRE(b.Roundtrip(Command({"GET", "k"}), 9) == "$3\r\none\r\n");
    REQUIRE(b.Roundtrip(Command({"SET", "k", "two"}), 5) == "+OK\r\n");
    REQUIRE(a.Roundtrip(Command({"GET", "k"}), 9) == "$3\r\ntwo\r\n");
}

TEST_CASE("Server answers a large pipeline sent before any reply is read") {
    RunningServer server;
    Client client(server.Port());
    std::string request;
    std::string expected;
    for (int i = 0; i < 10000; ++i) {
        request += Command({"PING"});
        expected += "+PONG\r\n";
    }
    client.Send(request);
    REQUIRE(client.Read(expected.size()) == expected);
}

TEST_CASE("Server sends a large reply to a slow reader intact") {
    RunningServer server;
    Client client(server.Port(), 4096);
    const std::string value(1024 * 1024, 'x');
    REQUIRE(client.Roundtrip(Command({"SET", "big", value}), 5) == "+OK\r\n");

    // 8 MiB of replies is more than the kernel buffers, so the server must wait for EPOLLOUT.
    std::string request;
    std::string expected;
    for (int i = 0; i < 8; ++i) {
        request += Command({"GET", "big"});
        expected += "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    }
    client.Send(request);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(client.Read(expected.size()) == expected);
}

TEST_CASE("Server flushes replies and the protocol error before closing") {
    RunningServer server;
    Client bad(server.Port());
    Client other(server.Port());
    const std::string expected = "+PONG\r\n-ERR Protocol error: expected '*'\r\n";
    REQUIRE(bad.Roundtrip(Command({"PING"}) + "garbage\r\n", expected.size()) == expected);
    REQUIRE(bad.SeesEof());
    REQUIRE(other.Roundtrip(Command({"PING"}), 7) == "+PONG\r\n");
}

TEST_CASE("Server rejects clients over the cap and accepts again after a disconnect") {
    RunningServer server(ServerConfig{.maxClients = 2});
    auto first = std::make_unique<Client>(server.Port());
    Client second(server.Port());
    REQUIRE(first->Roundtrip(Command({"PING"}), 7) == "+PONG\r\n");
    REQUIRE(second.Roundtrip(Command({"PING"}), 7) == "+PONG\r\n");

    Client third(server.Port());
    const std::string error = "-ERR max number of clients reached\r\n";
    REQUIRE(third.Read(error.size()) == error);
    REQUIRE(third.SeesEof());

    first.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Client fourth(server.Port());
    REQUIRE(fourth.Roundtrip(Command({"PING"}), 7) == "+PONG\r\n");
}

TEST_CASE("Server closes idle clients but keeps active ones") {
    using namespace std::chrono;
    RunningServer server(ServerConfig{.idleTimeout = milliseconds(100)});
    Client idle(server.Port());
    Client active(server.Port());

    const auto start = steady_clock::now();
    for (int i = 0; i < 10; ++i) {
        REQUIRE(active.Roundtrip(Command({"PING"}), 7) == "+PONG\r\n");
        std::this_thread::sleep_for(milliseconds(50));
    }

    REQUIRE(idle.SeesEof());
    REQUIRE(steady_clock::now() - start < milliseconds(1500));
    REQUIRE(active.Roundtrip(Command({"PING"}), 7) == "+PONG\r\n");
}

TEST_CASE("Stop makes Run return and closes connected clients") {
    RunningServer server;
    Client client(server.Port());
    REQUIRE(client.Roundtrip(Command({"PING"}), 7) == "+PONG\r\n");
    server.Stop();
    REQUIRE(client.SeesEof());
}

TEST_CASE("Stop before Run makes Run return straight away") {
    KvStore store;
    Server server(store);
    REQUIRE(server.Listen(0));
    server.Stop();
    server.Run();
}
