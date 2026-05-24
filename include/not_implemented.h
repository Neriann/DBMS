#pragma once

#include <stdexcept>
#include <string>

class not_implemented final : public std::logic_error {
public:
    explicit not_implemented(const std::string& method_name, const std::string& message)
        : std::logic_error("method `" + method_name + "` not implemented: \"" + message + "\"") {}
};
