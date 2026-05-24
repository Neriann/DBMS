#include "storage/page_io.hpp"

#include <algorithm>
#include <stdexcept>

namespace storage {
    PageWriter::PageWriter(page_buffer &page, const std::size_t offset) noexcept : page_(page), offset_(offset) {
    }

    void PageWriter::write_bytes(const std::span<const std::byte> bytes) {
        check_available(bytes.size());
        std::ranges::copy(bytes, page_.begin() + static_cast<std::ptrdiff_t>(offset_));
        offset_ += bytes.size();
    }

    std::size_t PageWriter::offset() const noexcept {
        return offset_;
    }

    void PageWriter::seek(const std::size_t offset) {
        if (offset > page_size) throw std::out_of_range("PageWriter offset is outside the page");

        offset_ = offset;
    }

    void PageWriter::check_available(const std::size_t bytes) const {
        if (bytes > page_size || offset_ > page_size - bytes) {
            throw std::out_of_range("PageWriter write exceeds page boundary");
        }
    }

    PageReader::PageReader(const page_buffer &page, const std::size_t offset) noexcept
        : page_(page),
          offset_(offset) {
    }

    void PageReader::read_bytes(const std::span<std::byte> out) {
        ensure_available(out.size());
        std::ranges::copy(std::span<const std::byte>(page_).subspan(offset_, out.size()), out.begin());
        offset_ += out.size();
    }

    std::size_t PageReader::offset() const noexcept {
        return offset_;
    }

    void PageReader::seek(const std::size_t offset) {
        if (offset > page_size) throw std::out_of_range("PageReader offset is outside the page");
        offset_ = offset;
    }

    void PageReader::ensure_available(const std::size_t bytes) const {
        if (bytes > page_size || offset_ > page_size - bytes) {
            throw std::out_of_range("PageReader read exceeds page boundary");
        }
    }
} // namespace storage
