#ifndef CPIOO_THREAD_SAFE_QUEUE_HPP
#define CPIOO_THREAD_SAFE_QUEUE_HPP

#include <cpioo/version.hpp>

#include <queue>
#include <mutex>
#include <optional>
#include <condition_variable>

namespace cpioo {

/**
 * @brief A thread-safe wrapper around std::queue
 * 
 * This class provides a thread-safe interface to a standard queue
 * by protecting all operations with a mutex.
 */
template <typename T>
class ThreadSafeQueue {
private:
    mutable std::mutex d_mutex;
    std::queue<T> d_queue;
    std::condition_variable d_cond;

public:
    ThreadSafeQueue() = default;
    
    /**
     * @brief Push a new element to the queue
     * @param value The value to be added
     */
    void push(T value) {
        std::lock_guard<std::mutex> lock(d_mutex);
        d_queue.push(std::move(value));
        d_cond.notify_one();
    }
    
    /**
     * @brief Try to pop an element from the queue
     * @return The element if queue is not empty, std::nullopt otherwise
     */
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(d_mutex);
        if (d_queue.empty()) {
            return std::nullopt;
        }
        
        T value = std::move(d_queue.front());
        d_queue.pop();
        return value;
    }
    
    /**
     * @brief Wait and pop an element when available
     * @return The popped element
     */
    T wait_and_pop() {
        std::unique_lock<std::mutex> lock(d_mutex);
        d_cond.wait(lock, [this]{ return !d_queue.empty(); });
        
        T value = std::move(d_queue.front());
        d_queue.pop();
        return value;
    }
    
    /**
     * @brief Check if the queue is empty
     * @return true if empty, false otherwise
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(d_mutex);
        return d_queue.empty();
    }
    
    /**
     * @brief Get the current size of the queue
     * @return Number of elements in the queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(d_mutex);
        return d_queue.size();
    }
    
    /**
     * @brief Clear all elements from the queue
     */
    void clear() {
        std::lock_guard<std::mutex> lock(d_mutex);
        std::queue<T> empty;
        std::swap(d_queue, empty);
    }
};

} // namespace cpioo

#endif // CPIOO_THREAD_SAFE_QUEUE_HPP
