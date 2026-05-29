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

std::string forward_query(const StorageNode& node,
                          const std::string& sql) {
    const auto socket = connect_to_server(
        node.host,
        node.port
    );

    std::ostringstream request;

    request
        << "POST /query HTTP/1.1\r\n"
        << "Host: "
        << node.host
        << ':'
        << node.port
        << "\r\n"
        << "Content-Type: text/plain\r\n"
        << "Content-Length: "
        << sql.size()
        << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << sql;

    send_all(socket.fd(), request.str());

    return recv_all(socket.fd());
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

void heartbeat_loop(std::vector<StorageNode>* nodes,
                    std::atomic<bool>* running)
{
    using namespace std::chrono_literals;

    while (running->load()) {
        for (auto& node : *nodes) {
            const bool was_alive = node.alive;
            const bool is_alive = ping_node(node);

            node.alive = is_alive;

            if (node.alive && !was_alive) {
                std::cout << "[HEARTBEAT] node recovered\n";
            }

            if (!node.alive && was_alive) {
                std::cout << "[HEARTBEAT] node died\n";
            }
            
        }

        std::this_thread::sleep_for(2s);
    }
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

    std::size_t next_node = 0;

    CROW_ROUTE(app, "/query")
        .methods(crow::HTTPMethod::Post)
    ([&nodes, &next_node](const crow::request& req) {

        if (nodes.empty()) {
            return crow::response(
                500,
                "no storage nodes available"
            );
        }

        StorageNode* selected = nullptr;

        for (auto& node : nodes) {
            if (node.alive) {
                selected = &node;
                break;
            }
        }

        if (!selected) {
            return crow::response(503, "no alive storage nodes");
        }
        ++next_node;

        try {
            const auto response =
                forward_query(*selected, req.body);

            return crow::response(
                200,
                extract_body(response)
            );
        }
        catch (const std::exception& e) {
            return crow::response(
                500,
                e.what()
            );
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