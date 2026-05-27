#pragma once

#include <string>

namespace auth {

class PasswordHasher {
public:
    [[nodiscard]] std::string generate_salt() const;
    [[nodiscard]] std::string hash_password(const std::string &password, const std::string &salt) const;
    [[nodiscard]] bool verify_password(const std::string &password,
                                       const std::string &salt,
                                       const std::string &hash) const;
};

} // namespace auth

