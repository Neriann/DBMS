#include "auth/password_hasher.hpp"

#include <random>

#include "common/not_implemented.hpp"

namespace auth {

std::string PasswordHasher::generate_salt() const {
    common::not_implemented();
}

std::string PasswordHasher::hash_password(const std::string &password, const std::string &salt) const {
    common::not_implemented();
}

bool PasswordHasher::verify_password(const std::string &password,
                                     const std::string &salt,
                                     const std::string &hash) const {
    common::not_implemented();
}

} // namespace auth
