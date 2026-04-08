#include "buffer_pool.hh"
#include <cstdlib>

namespace vt {

BufferPool& BufferPool::Instance() {
    static BufferPool inst;
    return inst;
}

void* BufferPool::Acquire(std::size_t size) {
    if (size == 0) return nullptr;
    std::lock_guard<std::mutex> lk(mu_);
    auto it = pool_.find(size);
    if (it != pool_.end() && !it->second.empty()) {
        void* p = it->second.back();
        it->second.pop_back();
        return p;
    }
    return std::malloc(size);
}

void BufferPool::Release(void* ptr, std::size_t size) {
    if (!ptr || size == 0) return;
    std::lock_guard<std::mutex> lk(mu_);
    pool_[size].push_back(ptr);
}

BufferPool::~BufferPool() {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto &kv : pool_) {
        for (void* p : kv.second) {
            std::free(p);
        }
    }
}

} // namespace vt
