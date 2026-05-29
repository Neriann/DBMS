#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace storage {

using page_id_t = std::uint64_t;

inline constexpr std::size_t page_size = 4096;
inline constexpr page_id_t invalid_page_id = 0;
inline constexpr page_id_t header_page_id = 0;
inline constexpr page_id_t first_data_page_id = 1;

using page_buffer = std::array<std::byte, page_size>;

struct page_header {
    std::uint32_t magic{};
    std::uint16_t version{};
    std::uint16_t flags{};
};

} // namespace storage
