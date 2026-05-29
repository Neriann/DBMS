#include "auth/password_hasher.hpp"

#include <iomanip>
#include <random>
#include <sstream>

namespace auth {
    namespace {

        std::string to_hex(std::uint64_t value) {
            std::ostringstream out;
            out << std::hex << std::setw(16) << std::setfill('0') << value;
            return out.str();
        }

    } // namespace

    std::string PasswordHasher::generate_salt() const {
        std::random_device device;
        std::mt19937_64 engine(device());
        std::uniform_int_distribution<std::uint64_t> dist;
        return to_hex(dist(engine));
    }

    std::string PasswordHasher::hash_password(const std::string &password, const std::string &salt) const {
        const auto data = salt + ":" + password;
        const auto digest = std::hash<std::string>{}(data);
        return to_hex(digest);
    }

    bool PasswordHasher::verify_password(const std::string &password,
                                         const std::string &salt,
                                         const std::string &hash) const {
        return hash_password(password, salt) == hash;
    }

} // namespace auth
