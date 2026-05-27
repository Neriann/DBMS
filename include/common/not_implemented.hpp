#pragma once

#include <stdexcept>

namespace common {

template <class... Ts>
[[noreturn]] inline void not_implemented(Ts &&...) {
    throw std::logic_error("not_implemented");
}

} // namespace common

