#include "storage/pager.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace storage {
    namespace {
        [[nodiscard]] std::streamoff page_offset(const page_id_t id) {
            return static_cast<std::streamoff>(id * page_size);
        }
    } // namespace

    Pager::Pager(std::filesystem::path path)
        : path_(std::move(path)) {
        if (const auto parent = path_.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        file_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
        if (!file_.is_open()) {
            std::ofstream create_file(path_, std::ios::binary);
            if (!create_file) {
                throw std::runtime_error("Failed to create pager file: " + path_.string());
            }
            create_file.close();

            file_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
        }

        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open pager file: " + path_.string());
        }
    }

    page_id_t Pager::allocate_page() const {
        const auto file_size = std::filesystem::file_size(path_);
        if (file_size % page_size != 0) {
            throw std::runtime_error("Corrupted pager file: size is not page-aligned");
        }

        const auto id = static_cast<page_id_t>(file_size / page_size);
        page_buffer empty_page{};
        std::ranges::fill(empty_page, std::byte{0});
        write_page(id, empty_page);
        return id;
    }

    void Pager::read_page(const page_id_t id, const std::span<std::byte, page_size> out) const {
        const auto offset = page_offset(id);
        if (const auto file_size = std::filesystem::file_size(path_);
            static_cast<std::uintmax_t>(offset) + page_size > file_size) {
            throw std::out_of_range("Pager page id is outside the file");
        }

        file_.clear();
        file_.seekg(offset);
        if (!file_.read(reinterpret_cast<char *>(out.data()), out.size())) {
            throw std::runtime_error("Failed to read pager page");
        }
    }

    void Pager::write_page(const page_id_t id, const std::span<const std::byte, page_size> data) const {
        const auto offset = page_offset(id);
        file_.clear();
        file_.seekp(offset);
        if (!file_.write(reinterpret_cast<const char *>(data.data()), data.size())) {
            throw std::runtime_error("Failed to write pager page");
        }
        file_.flush();
        if (!file_) {
            throw std::runtime_error("Failed to flush pager page");
        }
    }

    const std::filesystem::path &Pager::path() const noexcept {
        return path_;
    }
} // namespace storage
