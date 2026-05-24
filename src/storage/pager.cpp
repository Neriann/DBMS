#include "storage/pager.hpp"

#include <utility>

#include "not_implemented.h"

namespace storage {

Pager::Pager(std::filesystem::path  path): path_(std::move(path)) {
    throw not_implemented("storage::Pager::Pager(const std::filesystem::path&)", "disk page storage skeleton");
}

page_id_t Pager::allocate_page() {
    throw not_implemented("storage::Pager::allocate_page()", "disk page allocation skeleton");
}

void Pager::read_page(const page_id_t /*id*/, const std::span<std::byte, page_size> /*out*/) {
    throw not_implemented("storage::Pager::read_page(page_id_t, std::span<std::byte, page_size>)", "disk page read skeleton");
}

void Pager::write_page(const page_id_t /*id*/, const std::span<const std::byte, page_size> /*data*/) {
    throw not_implemented("storage::Pager::write_page(page_id_t, std::span<const std::byte, page_size>)", "disk page write skeleton");
}

const std::filesystem::path& Pager::path() const noexcept {
    return path_;
}

} // namespace storage
