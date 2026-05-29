#include "crow.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

namespace {

struct StorageNode {
    std::string host;
    std::string port;
    bool alive = true;
};

class Socket {
public:
    explicit Socket(const int fd) : fd_(fd) {}

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                close(fd_);
            }

            fd_ = other.fd_;
            other.fd_ = -1;
        }

        return *this;
    }

    ~Socket() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    [[nodiscard]]
    int fd() const {
        return fd_;
    }

private:
    int fd_;
};

Socket connect_to_server(const std::string& host,
                         const std::string& port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* raw = nullptr;

    if (const auto rc = getaddrinfo(
            host.c_str(),
            port.c_str(),
            &hints,
            &raw
        ); rc != 0) {
        throw std::runtime_error(
            std::string("getaddrinfo failed: ") +
            gai_strerror(rc)
        );
    }

    const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>
        result(raw, freeaddrinfo);

    int fd = -1;

    for (auto rp = result.get(); rp != nullptr; rp = rp->ai_next) {
        fd = socket(
            rp->ai_family,
            rp->ai_socktype,
            rp->ai_protocol
        );

        if (fd == -1) {
            continue;
        }

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        close(fd);
        fd = -1;
    }

    if (fd == -1) {
        throw std::runtime_error(
            "cannot connect to " + host + ":" + port
        );
    }

    return Socket(fd);
}

void send_all(const int fd, const std::string& data) {
    auto* ptr = data.data();
    auto left = data.size();

    while (left > 0) {
        const auto sent = send(fd, ptr, left, 0);

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error(
                std::string("send failed: ") +
                std::strerror(errno)
            );
        }

        if (sent == 0) {
            throw std::runtime_error(
                "send failed: connection closed"
            );
        }

        ptr += sent;
        left -= static_cast<std::size_t>(sent);
    }
}

std::string recv_all(const int fd) {
    std::string response;

    char buffer[4096];

    for (;;) {
        const auto received = recv(fd, buffer, sizeof(buffer), 0);

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw std::runtime_error(
                std::string("recv failed: ") +
                std::strerror(errno)
            );
        }

        if (received == 0) {
            break;
        }

        response.append(
            buffer,
            static_cast<std::size_t>(received)
        );
    }

    return response;
}

std::string extract_body(const std::string& response) {
    const auto pos = response.find("\r\n\r\n");

    if (pos == std::string::npos) {
        return response;
    }

    return response.substr(pos + 4);
}

bool ping_node(const StorageNode& node) {
    try {
        auto socket = connect_to_server(node.host, node.port);

        std::string req =
            "GET /heartbeat HTTP/1.1\r\n"
            "Host: " + node.host + "\r\n"
            "Connection: close\r\n\r\n";

        send_all(socket.fd(), req);
        auto resp = recv_all(socket.fd());

        return resp.find("200") != std::string::npos;
    } catch (...) {
        return false;
    }
}

void restart_node(const StorageNode& node) {
    std::string cmd =
        "./dbms_server " + node.port + " ./data" + node.port +
        " > logs_" + node.port + ".txt 2>&1 &";

    std::cout << "[HEARTBEAT] restarting node: "
              << node.host << ":" << node.port << "\n";

    std::system(cmd.c_str());
}

void heartbeat_loop(std::vector<StorageNode>* nodes,
                    std::atomic<bool>* running)
{
    using namespace std::chrono_literals;

    while (running->load()) {
        for (auto& node : *nodes) {
            const bool was_alive = node.alive;
            const bool is_alive = ping_node(node);

            if (!is_alive && was_alive) {
                std::cout << "[HEARTBEAT] node died\n";
                restart_node(node);
            }
            if (is_alive && !was_alive) {
                std::cout << "[HEARTBEAT] node recovered\n";
            }
            
        }

        std::this_thread::sleep_for(2s);
    }
}

std::string send_with_failover(std::vector<StorageNode>& nodes,
                               const std::string& request)
{
    std::vector<StorageNode*> candidates;

    for (auto& node : nodes) {
        if (node.alive) {
            candidates.push_back(&node);
        }
    }

    if (candidates.empty()) {
        throw std::runtime_error("no alive storage nodes available");
    }

    for (auto* node : candidates) {
        try {
            auto socket = connect_to_server(node->host, node->port);

            send_all(socket.fd(), request);
            return recv_all(socket.fd());
        }
        catch (...) {
            node->alive = false;

            std::cout << "[FAILOVER] node failed: "
                      << node->host << ":" << node->port << "\n";
        }
    }

    throw std::runtime_error("all storage nodes failed");
}

} // namespace

int main() {
    crow::SimpleApp app;

    std::vector<StorageNode> nodes{
        {
            "127.0.0.1",
            "9001"
        }
    };
    
    CROW_ROUTE(app, "/query")
    .methods(crow::HTTPMethod::Post)
    ([&nodes](const crow::request& req) {

        if (nodes.empty()) {
            return crow::response(500, "no storage nodes available");
        }

        try {
            auto response = send_with_failover(nodes, req.body);
            return crow::response(
                200,
                extract_body(response)
            );
        }
        catch (const std::exception& e) {
            return crow::response(503, e.what());
        }
    });

    std::atomic<bool> running{true};
    std::thread hb_thread(
        heartbeat_loop,
        &nodes,
        &running
    );
    hb_thread.detach();

    std::cout
        << "entrypoint listening on port 8080\n";

    app.port(8080)
       .multithreaded()
       .run();
}