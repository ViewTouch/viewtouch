/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * object_pool.hh - Object pooling for reduced allocation overhead
 * Optimized for resource-constrained systems like Raspberry Pi
 */

#ifndef VT_OBJECT_POOL_HH
#define VT_OBJECT_POOL_HH

#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <cassert>

namespace vt {

/**
 * @brief A simple, efficient object pool for reducing allocation overhead.
 * 
 * Benefits:
 * - Reduces memory fragmentation from frequent new/delete
 * - Faster allocation (reuses existing objects)
 * - Better cache locality (objects from same pool are nearby)
 * - Reduced pressure on system allocator
 * 
 * Usage:
 *   ObjectPool<MyClass> pool(16);  // Pre-allocate 16 objects
 *   auto obj = pool.acquire();     // Get object from pool
 *   // ... use obj ...
 *   pool.release(obj);             // Return to pool (or let destructor handle it)
 * 
 * @tparam T The type of object to pool (must be default-constructible)
 */
template<typename T>
class ObjectPool {
public:
    /**
     * @brief Construct pool with optional pre-allocation
     * @param initial_size Number of objects to pre-allocate (0 = grow on demand)
     * @param max_size Maximum pool size (0 = unlimited)
     */
    explicit ObjectPool(size_t initial_size = 0, size_t max_size = 0)
        : max_pool_size_(max_size)
        , total_allocated_(0)
        , total_reused_(0)
    {
        if (initial_size > 0) {
            pool_.reserve(initial_size);
            for (size_t i = 0; i < initial_size; ++i) {
                pool_.push_back(std::make_unique<T>());
            }
        }
    }

    // Non-copyable, non-movable (owns resources)
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

    ~ObjectPool() = default;

    /**
     * @brief Acquire an object from the pool
     * @return Pointer to object (caller must call release() when done)
     * 
     * If pool is empty, allocates a new object.
     * Object is NOT reset - caller should initialize as needed.
     */
    T* acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!pool_.empty()) {
            T* obj = pool_.back().release();
            pool_.pop_back();
            ++total_reused_;
            return obj;
        }
        
        // Pool empty - allocate new
        ++total_allocated_;
        return new T();
    }

    /**
     * @brief Acquire an object and reset it using provided function
     * @param reset_func Function to reset/initialize the object
     * @return Pointer to reset object
     */
    T* acquire(std::function<void(T&)> reset_func) {
        T* obj = acquire();
        if (obj && reset_func) {
            reset_func(*obj);
        }
        return obj;
    }

    /**
     * @brief Release an object back to the pool
     * @param obj Object to return (must have been acquired from this pool)
     * 
     * If pool is at max_size, object is deleted instead of pooled.
     */
    void release(T* obj) {
        if (obj == nullptr) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if we should pool or delete
        if (max_pool_size_ > 0 && pool_.size() >= max_pool_size_) {
            delete obj;
            return;
        }
        
        pool_.push_back(std::unique_ptr<T>(obj));
    }

    /**
     * @brief Get current number of objects in pool
     */
    [[nodiscard]] size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

    /**
     * @brief Get total allocations made (new objects created)
     */
    [[nodiscard]] size_t total_allocated() const {
        return total_allocated_;
    }

    /**
     * @brief Get total reuses (objects taken from pool)
     */
    [[nodiscard]] size_t total_reused() const {
        return total_reused_;
    }

    /**
     * @brief Get reuse ratio (higher = better pool efficiency)
     */
    [[nodiscard]] double reuse_ratio() const {
        size_t total = total_allocated_ + total_reused_;
        if (total == 0) return 0.0;
        return static_cast<double>(total_reused_) / static_cast<double>(total);
    }

    /**
     * @brief Clear all pooled objects (frees memory)
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.clear();
    }

    /**
     * @brief Pre-allocate additional objects
     * @param count Number of objects to add to pool
     */
    void reserve(size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < count; ++i) {
            if (max_pool_size_ > 0 && pool_.size() >= max_pool_size_) {
                break;
            }
            pool_.push_back(std::make_unique<T>());
        }
    }

private:
    std::vector<std::unique_ptr<T>> pool_;
    mutable std::mutex mutex_;
    size_t max_pool_size_;
    size_t total_allocated_;
    size_t total_reused_;
};

/**
 * @brief RAII wrapper for pooled objects - auto-releases on destruction
 * 
 * Usage:
 *   ObjectPool<MyClass> pool;
 *   {
 *       PooledObject<MyClass> obj(pool);
 *       obj->doSomething();
 *   }  // Automatically released back to pool
 */
template<typename T>
class PooledObject {
public:
    explicit PooledObject(ObjectPool<T>& pool)
        : pool_(&pool)
        , obj_(pool.acquire())
    {}

    PooledObject(ObjectPool<T>& pool, std::function<void(T&)> reset_func)
        : pool_(&pool)
        , obj_(pool.acquire(reset_func))
    {}

    ~PooledObject() {
        if (obj_ && pool_) {
            pool_->release(obj_);
        }
    }

    // Move-only
    PooledObject(PooledObject&& other) noexcept
        : pool_(other.pool_)
        , obj_(other.obj_)
    {
        other.pool_ = nullptr;
        other.obj_ = nullptr;
    }

    PooledObject& operator=(PooledObject&& other) noexcept {
        if (this != &other) {
            if (obj_ && pool_) {
                pool_->release(obj_);
            }
            pool_ = other.pool_;
            obj_ = other.obj_;
            other.pool_ = nullptr;
            other.obj_ = nullptr;
        }
        return *this;
    }

    // Non-copyable
    PooledObject(const PooledObject&) = delete;
    PooledObject& operator=(const PooledObject&) = delete;

    // Accessors
    T* get() noexcept { return obj_; }
    const T* get() const noexcept { return obj_; }
    T* operator->() noexcept { return obj_; }
    const T* operator->() const noexcept { return obj_; }
    T& operator*() noexcept { return *obj_; }
    const T& operator*() const noexcept { return *obj_; }
    explicit operator bool() const noexcept { return obj_ != nullptr; }

    /**
     * @brief Release ownership without returning to pool
     */
    T* release() noexcept {
        T* tmp = obj_;
        obj_ = nullptr;
        pool_ = nullptr;
        return tmp;
    }

private:
    ObjectPool<T>* pool_;
    T* obj_;
};

/**
 * @brief Fixed-size buffer pool for stack-like allocations
 * 
 * More efficient than ObjectPool for fixed-size buffers (char arrays, etc.)
 * Uses a simple free-list instead of vector.
 */
template<size_t BufferSize>
class BufferPool {
public:
    explicit BufferPool(size_t initial_count = 4, size_t max_count = 32)
        : max_buffers_(max_count)
        , buffer_count_(0)
    {
        buffers_.reserve(initial_count);
        for (size_t i = 0; i < initial_count; ++i) {
            buffers_.push_back(std::make_unique<std::array<char, BufferSize>>());
        }
    }

    char* acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!buffers_.empty()) {
            char* buf = buffers_.back()->data();
            buffers_.pop_back();
            return buf;
        }
        
        // Allocate new buffer
        auto new_buf = std::make_unique<std::array<char, BufferSize>>();
        char* buf = new_buf->data();
        allocated_.push_back(std::move(new_buf));
        ++buffer_count_;
        return buf;
    }

    void release(char* buf) {
        if (buf == nullptr) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Find which allocation this came from
        for (auto& alloc : allocated_) {
            if (alloc->data() == buf) {
                if (buffers_.size() < max_buffers_) {
                    buffers_.push_back(std::move(alloc));
                }
                return;
            }
        }
        // Buffer not from this pool - ignore
    }

    [[nodiscard]] size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffers_.size();
    }

    static constexpr size_t buffer_size() { return BufferSize; }

private:
    std::vector<std::unique_ptr<std::array<char, BufferSize>>> buffers_;
    std::vector<std::unique_ptr<std::array<char, BufferSize>>> allocated_;
    mutable std::mutex mutex_;
    size_t max_buffers_;
    size_t buffer_count_;
};

} // namespace vt

#endif // VT_OBJECT_POOL_HH
