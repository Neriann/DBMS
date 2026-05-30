#include "distributed/rpc/rpc_client.hpp"

#include <cerrno>
#include <cstring>
#include <memory>
#include <netdb.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace distributed::rpc {
namespace {

class Socket {
public:
    explicit Socket(const int fd) : fd_(fd) {}
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    ~Socket() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }
    [[nodiscard]] int fd() const { return fd_; }

private:
    int fd_;
};

Socket connect_to_server(const cluster::NodeEndpoint &endpoint) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *raw = nullptr;
    const auto port = std::to_string(endpoint.port);
    if (const auto rc = getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &raw); rc != 0) {
        throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(rc));
    }
    const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> result(raw, freeaddrinfo);

    int fd = -1;
    for (auto rp = result.get(); rp != nullptr; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
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
        throw std::runtime_error("cannot connect to " + endpoint.host + ":" + port);
    }
    return Socket(fd);
}

void send_all(const int fd, const std::string &data) {
    auto *ptr = data.data();
    auto left = data.size();
    while (left > 0) {
        const auto sent = send(fd, ptr, left, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error(std::string("send failed: ") + std::strerror(errno));
        }
        if (sent == 0) {
            throw std::runtime_error("send failed: connection closed");
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
            throw std::runtime_error(std::string("recv failed: ") + std::strerror(errno));
        }
        if (received == 0) {
            break;
        }
        response.append(buffer, static_cast<std::size_t>(received));
    }
    return response;
}

int parse_status_code(const std::string &response) {
    std::istringstream line(response.substr(0, response.find("\r\n")));
    std::string version;
    int status = 0;
    line >> version >> status;
    return status;
}

std::string response_body(const std::string &response) {
    const auto header_end = response.find("\r\n\r\n");
    return header_end == std::string::npos ? response : response.substr(header_end + 4);
}

} // namespace

RpcClient::RpcClient(cluster::NodeEndpoint endpoint)
    : endpoint_(std::move(endpoint)) {
}

RpcResponse RpcClient::execute(const RpcRequest &request) const {
    const auto socket = connect_to_server(endpoint_);
    std::ostringstream http;
    http << "POST /query HTTP/1.1\r\n"
         << "Host: " << endpoint_.host << ':' << endpoint_.port << "\r\n"
         << "Content-Type: text/plain; charset=utf-8\r\n";
    if (!request.auth_token.empty()) {
        http << "Authorization: Bearer " << request.auth_token << "\r\n";
    }
    http << "Content-Length: " << request.sql.size() << "\r\n"
         << "Connection: close\r\n\r\n"
         << request.sql;

    send_all(socket.fd(), http.str());
    const auto raw = recv_all(socket.fd());
    return RpcResponse{parse_status_code(raw), response_body(raw)};
}

RpcResponse RpcClient::heartbeat(const std::string &node_id) const {
    (void)node_id;
    const auto socket = connect_to_server(endpoint_);
    std::ostringstream http;
    http << "GET /heartbeat HTTP/1.1\r\n"
         << "Host: " << endpoint_.host << ':' << endpoint_.port << "\r\n"
         << "Connection: close\r\n\r\n";
    send_all(socket.fd(), http.str());
    const auto raw = recv_all(socket.fd());
    return RpcResponse{parse_status_code(raw), response_body(raw)};
}

} // namespace distributed::rpc
