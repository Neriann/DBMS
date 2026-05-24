#pragma once

#include <limits>
#include <memory>
#include <memory_resource>

template<typename T>
class pp_allocator {
public:
    using propagate_on_container_swap = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using value_type = T;

    pp_allocator(std::pmr::memory_resource *resource = std::pmr::get_default_resource()) noexcept
        : resource_(resource == nullptr ? std::pmr::get_default_resource() : resource) {
    }

    template<typename U>
    pp_allocator(const pp_allocator<U> &other) noexcept
        : resource_(other.resource()) {
    }

    [[nodiscard]] T *allocate(std::size_t n) {
        return static_cast<T *>(resource_->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T *p, const std::size_t n = 1) {
        resource_->deallocate(p, n * sizeof(T), alignof(T));
    }

    template<typename U, typename... Args>
    void construct(U *p, Args &&... args) {
        std::uninitialized_construct_using_allocator(p, *this, std::forward<Args>(args)...);
    }

    template<typename U>
    static void destroy(U *p) {
        p->~U();
    }

    [[nodiscard]] void *allocate_bytes(
        const std::size_t nbytes,
        const std::size_t alignment = alignof(std::max_align_t)) const {
        return resource_->allocate(nbytes, alignment);
    }

    void deallocate_bytes(
        void *p,
        const std::size_t bytes = 1,
        const std::size_t alignment = alignof(std::max_align_t)) const {
        resource_->deallocate(p, bytes, alignment);
    }

    template<typename U>
    [[nodiscard]] U *allocate_object(std::size_t n = 1) {
        if ((std::numeric_limits<std::size_t>::max() / sizeof(U)) < n) {
            throw std::bad_array_new_length();
        }
        return static_cast<U *>(allocate_bytes(n * sizeof(U), alignof(U)));
    }

    template<typename U>
    void deallocate_object(U *p, std::size_t n = 1) {
        deallocate_bytes(p, n * sizeof(U), alignof(U));
    }

    template<typename U, typename... Args>
    [[nodiscard]] U *new_object(Args &&... args) {
        U *p = allocate_object<U>();
        try {
            construct(p, std::forward<Args>(args)...);
        } catch (...) {
            deallocate_object(p);
            throw;
        }
        return p;
    }

    template<typename U>
    void delete_object(U *p) {
        destroy(p);
        deallocate_object(p);
    }

    pp_allocator select_on_container_copy_construction() const {
        return pp_allocator(resource_);
    }

    [[nodiscard]] std::pmr::memory_resource *resource() const {
        return resource_;
    }

private:
    std::pmr::memory_resource *resource_;
};

template<typename T, typename U>
bool operator==(const pp_allocator<T> &lhs, const pp_allocator<U> &rhs) noexcept {
    return lhs.resource()->is_equal(*rhs.resource());
}

template<typename T, typename U>
bool operator!=(const pp_allocator<T> &lhs, const pp_allocator<U> &rhs) noexcept {
    return !(lhs == rhs);
}
