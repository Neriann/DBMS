#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
    constexpr auto DEFAULT_HOST = "127.0.0.1";
    constexpr auto DEFAULT_PORT = "8080";
    constexpr auto DEFAULT_TOKEN_FILE_NAME = ".dbms_client_token";

    enum class Mode {
        Query,
        Login,
        Register,
        CreateGroup,
        AddUserToGroup,
        GrantPermission,
        RevokePermission
    };

    class Socket {
    public:
        explicit Socket(const int fd) : fd_(fd) {
        }

        Socket(const Socket &) = delete;
        Socket &operator=(const Socket &) = delete;

        Socket(Socket &&other) noexcept : fd_(other.fd_) {
            other.fd_ = -1;
        }

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

    std::string default_token_file() {
        if (const char *home = std::getenv("HOME"); home != nullptr && *home != '\0') {
            return std::string(home) + "/" + DEFAULT_TOKEN_FILE_NAME;
        }
        return DEFAULT_TOKEN_FILE_NAME;
    }

    std::string read_token_file(const std::string &path) {
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error("missing token; run --login or pass --token");
        }

        std::string token;
        std::getline(file, token);
        if (token.empty()) {
            throw std::runtime_error("token file is empty: " + path);
        }
        return token;
    }

    void write_token_file(const std::string &path, const std::string &token) {
        std::ofstream file(path, std::ios::trunc);
        if (!file) {
            throw std::runtime_error("cannot write token file: " + path);
        }
        file << token << '\n';
    }

    std::string json_escape(const std::string &value) {
        std::string escaped;
        escaped.reserve(value.size());
        for (const char ch : value) {
            switch (ch) {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    escaped.push_back(ch);
                    break;
            }
        }
        return escaped;
    }

    std::string json_object(const std::initializer_list<std::pair<std::string, std::string>> fields) {
        std::ostringstream body;
        body << '{';
        bool first = true;
        for (const auto &[key, value] : fields) {
            if (!first) {
                body << ',';
            }
            first = false;
            body << '"' << json_escape(key) << "\":\"" << json_escape(value) << '"';
        }
        body << '}';
        return body.str();
    }

    std::string extract_json_string(const std::string &json, const std::string &key) {
        const auto quoted_key = "\"" + key + "\"";
        auto pos = json.find(quoted_key);
        if (pos == std::string::npos) {
            throw std::runtime_error("response does not contain '" + key + "'");
        }
        pos = json.find(':', pos + quoted_key.size());
        if (pos == std::string::npos) {
            throw std::runtime_error("invalid JSON response");
        }
        pos = json.find('"', pos + 1);
        if (pos == std::string::npos) {
            throw std::runtime_error("invalid JSON response");
        }

        std::string value;
        bool escaping = false;
        for (++pos; pos < json.size(); ++pos) {
            const char ch = json[pos];
            if (escaping) {
                switch (ch) {
                    case 'n':
                        value.push_back('\n');
                        break;
                    case 'r':
                        value.push_back('\r');
                        break;
                    case 't':
                        value.push_back('\t');
                        break;
                    default:
                        value.push_back(ch);
                        break;
                }
                escaping = false;
                continue;
            }
            if (ch == '\\') {
                escaping = true;
                continue;
            }
            if (ch == '"') {
                return value;
            }
            value.push_back(ch);
        }

        throw std::runtime_error("invalid JSON response");
    }

    Socket connect_to_server(const std::string &host, const std::string &port) {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo *raw = nullptr;
        if (const auto rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &raw); rc != 0) {
            throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(rc));
        }
        const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> result(raw, freeaddrinfo);

        int fd = -1;
        for (auto rp = result.get(); rp != nullptr; rp = rp->ai_next) {
            fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (fd == -1) continue;
            if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
            close(fd);
            fd = -1;
        }

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
                if (errno == EINTR) continue;
                throw std::runtime_error(std::string("recv failed: ") + std::strerror(errno));
            }
            if (received == 0) break;
            response.append(buffer, static_cast<std::size_t>(received));
        }
        return response;
    }

    std::string http_post(const std::string &host,
                          const std::string &port,
                          const std::string &path,
                          const std::string &content_type,
                          const std::string &body,
                          const std::string &token) {
        const auto socket = connect_to_server(host, port);

        std::ostringstream request;
        request << "POST " << path << " HTTP/1.1\r\n"
                << "Host: " << host << ':' << port << "\r\n"
                << "Content-Type: " << content_type << "\r\n";
        if (!token.empty()) {
            request << "Authorization: Bearer " << token << "\r\n";
        }
        request << "Content-Length: " << body.size() << "\r\n"
                << "Connection: close\r\n"
                << "\r\n"
                << body;

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
        std::cerr
                << "usage:\n"
                << "  " << program << " [--host HOST] [--port PORT] [--token TOKEN] [--token-file PATH] [file.sql]\n"
                << "  " << program << " --login USER PASSWORD [--host HOST] [--port PORT] [--token-file PATH]\n"
                << "  " << program << " --register USER PASSWORD [--host HOST] [--port PORT] [--token-file PATH]\n"
                << "  " << program << " --create-group NAME [--token TOKEN|--token-file PATH]\n"
                << "  " << program << " --add-user-to-group USER_ID GROUP_ID [--token TOKEN|--token-file PATH]\n"
                << "  " << program << " --grant-permission SUBJECT_TYPE SUBJECT_ID DB TABLE PERMISSION [--token TOKEN|--token-file PATH]\n"
                << "  " << program << " --revoke-permission SUBJECT_TYPE SUBJECT_ID DB TABLE PERMISSION [--token TOKEN|--token-file PATH]\n"
                << "\n"
                << "SQL is read from stdin when file.sql is omitted.\n";
    }
}

int main(const int argc, char **argv) {
    auto host = DEFAULT_HOST;
    auto port = DEFAULT_PORT;
    auto token_file = default_token_file();
    std::string token;
    bool token_explicit = false;
    Mode mode = Mode::Query;
    std::string username;
    std::string password;
    std::string user_id;
    std::string group_id;
    std::string group_name;
    std::string subject_type;
    std::string subject_id;
    std::string database_name;
    std::string table_name;
    std::string permission;
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
        } else if (arg == "--token") {
            if (++i >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            token = argv[i];
            token_explicit = true;
        } else if (arg == "--token-file") {
            if (++i >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            token_file = argv[i];
        } else if (arg == "--login") {
            if (mode != Mode::Query || i + 2 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            mode = Mode::Login;
            username = argv[++i];
            password = argv[++i];
        } else if (arg == "--register") {
            if (mode != Mode::Query || i + 2 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            mode = Mode::Register;
            username = argv[++i];
            password = argv[++i];
        } else if (arg == "--create-group") {
            if (mode != Mode::Query || i + 1 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            mode = Mode::CreateGroup;
            group_name = argv[++i];
        } else if (arg == "--add-user-to-group") {
            if (mode != Mode::Query || i + 2 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            mode = Mode::AddUserToGroup;
            user_id = argv[++i];
            group_id = argv[++i];
        } else if (arg == "--grant-permission") {
            if (mode != Mode::Query || i + 5 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            mode = Mode::GrantPermission;
            subject_type = argv[++i];
            subject_id = argv[++i];
            database_name = argv[++i];
            table_name = argv[++i];
            permission = argv[++i];
        } else if (arg == "--revoke-permission") {
            if (mode != Mode::Query || i + 5 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            mode = Mode::RevokePermission;
            subject_type = argv[++i];
            subject_id = argv[++i];
            database_name = argv[++i];
            table_name = argv[++i];
            permission = argv[++i];
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
        if (mode == Mode::Login || mode == Mode::Register) {
            const auto body = json_object({{"username", username}, {"password", password}});
            const auto response = http_post(host,
                                            port,
                                            mode == Mode::Login ? "/login" : "/register",
                                            "application/json",
                                            body,
                                            "");
            const auto status = parse_status_code(response);
            const auto body_text = response_body(response);
            if (status < 200 || status >= 300) {
                std::cerr << body_text << (body_text.ends_with('\n') ? "" : "\n");
                return 1;
            }

            token = extract_json_string(body_text, "token");
            write_token_file(token_file, token);
            std::cout << "token saved to " << token_file << '\n';
            return 0;
        }

        if (!token_explicit) {
            token = read_token_file(token_file);
        }

        if (mode == Mode::CreateGroup) {
            const auto body = json_object({{"name", group_name}});
            const auto response = http_post(host, port, "/admin/groups", "application/json", body, token);
            const auto status = parse_status_code(response);
            const auto body_text = response_body(response);
            auto &out = status >= 200 && status < 300 ? std::cout : std::cerr;
            out << body_text << (body_text.ends_with('\n') ? "" : "\n");
            return status >= 200 && status < 300 ? 0 : 1;
        }

        if (mode == Mode::AddUserToGroup) {
            const auto body = json_object({{"user_id", user_id}});
            const auto response = http_post(host,
                                            port,
                                            "/admin/groups/" + group_id + "/users",
                                            "application/json",
                                            body,
                                            token);
            const auto status = parse_status_code(response);
            const auto body_text = response_body(response);
            auto &out = status >= 200 && status < 300 ? std::cout : std::cerr;
            out << body_text << (body_text.ends_with('\n') ? "" : "\n");
            return status >= 200 && status < 300 ? 0 : 1;
        }

        if (mode == Mode::GrantPermission || mode == Mode::RevokePermission) {
            const auto body = json_object({{"subject_type", subject_type},
                                           {"subject_id", subject_id},
                                           {"database_name", database_name},
                                           {"table_name", table_name},
                                           {"permission", permission}});
            const auto response = http_post(host,
                                            port,
                                            mode == Mode::GrantPermission
                                                ? "/admin/permissions/grant"
                                                : "/admin/permissions/revoke",
                                            "application/json",
                                            body,
                                            token);
            const auto status = parse_status_code(response);
            const auto body_text = response_body(response);
            auto &out = status >= 200 && status < 300 ? std::cout : std::cerr;
            out << body_text << (body_text.ends_with('\n') ? "" : "\n");
            return status >= 200 && status < 300 ? 0 : 1;
        }

        const auto sql = file_path == nullptr ? read_all(std::cin) : read_file(file_path);
        if (sql.empty()) return 0;

        const auto response = http_post(host, port, "/query", "text/plain; charset=utf-8", sql, token);
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
