#pragma once

#include <memory>
#include <utility>

/**
 * @brief Modern C++ Memory Management Utilities
 *
 * This header provides utilities for modernizing memory management in ViewTouch.
 * It offers smart pointer factory functions and ownership transfer utilities.
 *
 * Note: Factory functions for ViewTouch classes are provided in their respective
 * implementation files to avoid circular dependencies.
 */

namespace vt {

/**
 * @brief Safe ownership transfer utility
 *
 * Transfers ownership from a unique_ptr to a raw pointer, logging if the
 * transfer results in null. This is useful for interfacing with legacy
 * APIs that take ownership via raw pointers.
 *
 * @param ptr The unique_ptr to transfer from
 * @return Raw pointer, or nullptr if transfer failed
 */
template<typename T>
T* transfer_ownership(std::unique_ptr<T>& ptr) {
    T* raw = ptr.get();
    if (!raw) {
        // Log warning about null transfer
        // vt::Logger::warn("Attempted to transfer ownership of null pointer");
    }
    ptr.release();
    return raw;
}

/**
 * @brief RAII wrapper for legacy cleanup functions
 *
 * This template provides RAII management for resources that need custom
 * cleanup functions. Useful for interfacing with C APIs or legacy code.
 */
template<typename T, typename CleanupFunc>
class RaiiWrapper {
public:
    RaiiWrapper(T* ptr, CleanupFunc cleanup)
        : ptr_(ptr), cleanup_(cleanup) {}

    RaiiWrapper(const RaiiWrapper&) = delete;
    RaiiWrapper& operator=(const RaiiWrapper&) = delete;

    RaiiWrapper(RaiiWrapper&& other) noexcept
        : ptr_(other.ptr_), cleanup_(other.cleanup_) {
        other.ptr_ = nullptr;
    }

    RaiiWrapper& operator=(RaiiWrapper&& other) noexcept {
        if (this != &other) {
            cleanup_if_needed();
            ptr_ = other.ptr_;
            cleanup_ = other.cleanup_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    ~RaiiWrapper() {
        cleanup_if_needed();
    }

    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }

    T* release() {
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }

private:
    void cleanup_if_needed() {
        if (ptr_) {
            cleanup_(ptr_);
        }
    }

    T* ptr_;
    CleanupFunc cleanup_;
};

/**
 * @brief Creates a RAII wrapper for resources with custom cleanup
 * @param ptr The resource pointer
 * @param cleanup Function to call for cleanup (e.g., free, delete, etc.)
 * @return RaiiWrapper managing the resource
 */
template<typename T, typename CleanupFunc>
auto make_raii(T* ptr, CleanupFunc cleanup) {
    return RaiiWrapper<T, CleanupFunc>(ptr, cleanup);
}

} // namespace vt
