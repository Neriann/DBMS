#include <cerrno>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
    constexpr auto DEFAULT_HOST = "127.0.0.1";
    constexpr auto DEFAULT_PORT = "8080";

    class Socket {
    public:
        explicit Socket(const int fd) : fd_(fd) {
        }

        Socket(const Socket &) = delete;

        Socket &operator=(Socket &&other) noexcept {
            if (this != &other) {
                if (fd_ >= 0) close(fd_);
                fd_ = other.fd_;
                other.fd_ = -1;
            }
            return *this;
        }

        ~Socket() {
            if (fd_ >= 0) close(fd_);
        }

        [[nodiscard]] int fd() const {
            return fd_;
        }

    private:
        int fd_;
    };

    std::string read_all(const std::istream &in) {
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::string read_file(const char *path) {
        const std::ifstream file(path);
        if (!file) {
            throw std::runtime_error(std::string("cannot open file: ") + path);
        }
        return read_all(file);
    }

    Socket connect_to_server(const std::string &host, const std::string &port) {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo *result = nullptr;
        if (const auto rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &result); rc != 0) {
            throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(rc));
        }

        int fd = -1;
        for (const auto *rp = result; rp != nullptr; rp = rp->ai_next) {
            fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (fd == -1) continue;
            if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
            close(fd);
            fd = -1;
        }

        freeaddrinfo(result);

        if (fd == -1) throw std::runtime_error("cannot connect to " + host + ":" + port);

        return Socket(fd);
    }

    void send_all(const int fd, const std::string &data) {
        auto *ptr = data.data();
        auto left = data.size();
        while (left > 0) {
            const auto sent = send(fd, ptr, left, 0);
            if (sent < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("send failed: ") + std::strerror(errno));
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
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("recv failed: ") + std::strerror(errno));
            }
            if (received == 0) break;
            response.append(buffer, static_cast<std::size_t>(received));
        }
        return response;
    }

    std::string http_post_query(const std::string &host, const std::string &port, const std::string &sql) {
        const auto socket = connect_to_server(host, port);

        std::ostringstream request;
        request << "POST /query HTTP/1.1\r\n"
                << "Host: " << host << ':' << port << "\r\n"
                << "Content-Type: text/plain; charset=utf-8\r\n"
                << "Content-Length: " << sql.size() << "\r\n"
                << "Connection: close\r\n"
                << "\r\n"
                << sql;

        send_all(socket.fd(), request.str());
        return recv_all(socket.fd());
    }

    int parse_status_code(const std::string &response) {
        std::istringstream line(response.substr(0, response.find("\r\n")));
        std::string http_version;
        auto status = 0;
        line >> http_version >> status;
        return status;
    }

    std::string response_body(const std::string &response) {
        const auto header_end = response.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            return response;
        }
        return response.substr(header_end + 4);
    }

    void print_usage(const char *program) {
        std::cerr << "usage: " << program << " [--host HOST] [--port PORT] [file.sql]\n"
                << "       SQL is read from stdin when file.sql is omitted.\n";
    }
}

int main(const int argc, char **argv) {
    auto host = DEFAULT_HOST;
    auto port = DEFAULT_PORT;
    const char *file_path = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "--host") {
            if (++i >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            host = argv[i];
        } else if (arg == "--port") {
            if (++i >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            port = argv[i];
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (file_path == nullptr) {
            file_path = argv[i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    try {
        const auto sql = file_path == nullptr ? read_all(std::cin) : read_file(file_path);
        if (sql.empty()) return 0;

        const auto response = http_post_query(host, port, sql);
        const auto status = parse_status_code(response);

        if (const auto body = response_body(response); !body.empty()) {
            auto &out = status >= 200 && status < 300 ? std::cout : std::cerr;
            out << body;
            if (body.back() != '\n') out << '\n';
        }

        return status >= 200 && status < 300 ? 0 : 1;
    } catch (const std::exception &e) {
        std::cerr << "dbms_client: " << e.what() << "\n";
        return 1;
    }
}
