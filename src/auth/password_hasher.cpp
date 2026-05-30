#include "auth/password_hasher.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace auth {
    namespace {

        constexpr std::size_t salt_bytes = 16;
        constexpr std::size_t hash_bytes = 32;
        constexpr int pbkdf2_iterations = 100000; // for safety from brute force

        std::string to_hex(const unsigned char *data, const std::size_t size) {
            std::ostringstream out;
            out << std::hex << std::setfill('0');
            for (std::size_t i = 0; i < size; ++i) {
                out << std::setw(2) << static_cast<unsigned int>(data[i]);
            }
            return out.str();
        }

    } // namespace

    std::string PasswordHasher::generate_salt() const {
        std::vector<unsigned char> salt(salt_bytes);
        if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
            throw std::runtime_error("failed to generate random salt");
        }
        return to_hex(salt.data(), salt.size());
    }

    std::string PasswordHasher::hash_password(const std::string &password, const std::string &salt) const {
        std::vector<unsigned char> digest(hash_bytes);
        if (PKCS5_PBKDF2_HMAC(password.data(),
                              static_cast<int>(password.size()),
                              reinterpret_cast<const unsigned char *>(salt.data()),
                              static_cast<int>(salt.size()),
                              pbkdf2_iterations,
                              EVP_sha256(),
                              static_cast<int>(digest.size()),
                              digest.data()) != 1) {
            throw std::runtime_error("failed to hash password");
        }
        return to_hex(digest.data(), digest.size());
    }

    bool PasswordHasher::verify_password(const std::string &password,
                                         const std::string &salt,
                                         const std::string &hash) const {
        const auto candidate = hash_password(password, salt);
        return candidate.size() == hash.size()
            && CRYPTO_memcmp(candidate.data(), hash.data(), hash.size()) == 0;
    }

} // namespace auth
