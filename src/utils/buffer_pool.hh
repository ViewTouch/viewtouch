// Simple thread-safe buffer pool to reuse malloc'd blocks for image buffers
#pragma once

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace vt {

class BufferPool {
public:
    static BufferPool& Instance();

    // Acquire a buffer of at least `size` bytes. Caller must call Release(ptr,size)
    void* Acquire(std::size_t size);

    // Return a buffer previously acquired via Acquire.
    void Release(void* ptr, std::size_t size);

    ~BufferPool();

private:
    BufferPool() = default;
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    std::mutex mu_;
    std::unordered_map<std::size_t, std::vector<void*>> pool_;
};

} // namespace vt
