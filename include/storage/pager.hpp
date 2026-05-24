#pragma once

#include "page.hpp"

#include <filesystem>
#include <fstream>
#include <span>

namespace storage {

class Pager {
public:
    explicit Pager(std::filesystem::path  path);

    static page_id_t allocate_page();

    static void read_page(page_id_t id, std::span<std::byte, page_size> out) ;
    static void write_page(page_id_t id, std::span<const std::byte, page_size> data);

    [[nodiscard]] const std::filesystem::path& path() const noexcept;

private:
    std::filesystem::path path_;
    mutable std::fstream file_;
};

} // namespace storage
