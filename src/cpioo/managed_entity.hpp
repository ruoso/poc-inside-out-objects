#ifndef CPIOO_MANAGED_ENTITY_HPP
#define CPIOO_MANAGED_ENTITY_HPP

#include <cpioo/version.hpp>
#include <cpioo/thread_safe_queue.hpp>
#include <optional>

#include <type_traits>
#include <array>
#include <vector>
#include <sys/mman.h>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>
#include <map>
#include <tuple>
#include <atomic>
#include <thread>
#include <chrono>

namespace cpioo {
  namespace managed_entity {

    template <class STORAGE>
    class reference {
      // Wrap the pointer in an optional. When engaged the pointer is never null.
      std::optional<const typename STORAGE::type*> d_ptr;
      const typename STORAGE::index_type d_index;

    public:
      reference(typename STORAGE::type* ptr, typename STORAGE::index_type index)
        : d_ptr(ptr), d_index(index) {
        STORAGE::refcnt_add(d_index);
      }

      reference(const reference& other)
        : d_ptr(other.d_ptr), d_index(other.d_index) {
        STORAGE::refcnt_add(d_index);
      }

      reference(reference&& other) noexcept
        : d_ptr(other.d_ptr), d_index(other.d_index) {
        other.d_ptr.reset();
      }

      reference& operator=(const reference& other) = delete;
      reference& operator=(reference&& other) noexcept = delete;

      bool operator==(const reference& other) const {
        return d_ptr == other.d_ptr;
      }

      bool operator!=(const reference& other) const {
        return d_ptr != other.d_ptr;
      }

      ~reference() {
        if (d_ptr.has_value()) {
          STORAGE::refcnt_subtract(d_index);
        }
      }

      const typename STORAGE::type* operator->() const {
        return d_ptr.value();
      }
    };

    constexpr size_t buffer_count(int buffer_size_bits) {
      return 1 << buffer_size_bits;
    }

    template <typename INDEX_TYPE>
    constexpr size_t superbuffer_count(size_t buffer_size_bits) {
      return std::numeric_limits<INDEX_TYPE>::max() >> buffer_size_bits;
    }
      
    template <
      class T,
      std::size_t BUFFER_SIZE_BITS = 10,
      typename INDEX_TYPE = uint32_t,
      std::size_t SUPERBUFFER_COUNT = superbuffer_count<INDEX_TYPE>(BUFFER_SIZE_BITS),
      std::size_t BUFFER_COUNT = buffer_count(BUFFER_SIZE_BITS),
      typename REFCNT_TYPE = short,
      class DATA_ALLOCATOR = std::allocator<
        std::array<T, BUFFER_COUNT >
        >,
      class REFCNT_ALLOCATOR = std::allocator<
        std::array<
          std::atomic<REFCNT_TYPE>, BUFFER_COUNT
          >
        >
      >
    class storage {
    public:
      using refcntbuffer =
        std::array<std::atomic<REFCNT_TYPE>, BUFFER_COUNT >;
      using refcntsuperbuffer = std::array<refcntbuffer*, SUPERBUFFER_COUNT>;
      
      using buffer = std::array<T, BUFFER_COUNT>;
      using superbuffer = std::array<buffer*, SUPERBUFFER_COUNT>;

      using type = T;
      using ref_type = reference<storage>;
      using index_type = INDEX_TYPE;

    private:
      // Helper class to store the thread-local free pool and automatically 
      // return it when the thread exits
      struct ThreadFreePoolManager {
        std::queue<INDEX_TYPE> available_indices;
        
        ThreadFreePoolManager() = default;
        
        void push(INDEX_TYPE index) {
          available_indices.push(index);
        }
        
        bool empty() const {
          return available_indices.empty();
        }
        
        INDEX_TYPE front() const {
          return available_indices.front();
        }
        
        void pop() {
          available_indices.pop();
        }
        
        size_t size() const {
          return available_indices.size();
        }
        
        ~ThreadFreePoolManager() {
          // Return any remaining items to the global pool on thread exit
          if (!available_indices.empty()) {
            s_globally_available.push(std::move(available_indices));
          }
        }
      };

      inline static DATA_ALLOCATOR s_data_allocator;
      inline static REFCNT_ALLOCATOR s_refcnt_allocator;
      inline static superbuffer s_buffers;
      inline static refcntsuperbuffer s_refcntbuffers;

      // Thread-local manager that handles the free pool for this thread
      inline static thread_local ThreadFreePoolManager s_available_on_thread;

      // Global pool of freed memory from all threads
      inline static ThreadSafeQueue<std::queue<INDEX_TYPE>> s_globally_available;

      inline static std::atomic<INDEX_TYPE> s_elements_reserved = 0;
      inline static std::atomic<INDEX_TYPE> s_elements_capacity = 0;

      std::tuple<INDEX_TYPE, INDEX_TYPE>
      constexpr static split_index(INDEX_TYPE index) {
        INDEX_TYPE index_in_superbuffer = index >> BUFFER_SIZE_BITS;
        INDEX_TYPE index_in_buffer = index & ((1 << BUFFER_SIZE_BITS)-1);
        return {index_in_superbuffer, index_in_buffer};
      }
      
      inline static std::tuple<void*, INDEX_TYPE>
      get_new_storage() {
        // We try to consume any memory already available to this
        // thread before trying to do anything that would cause a
        // synchronization requirement.
        if (s_available_on_thread.empty()) {
          // Check if there's any globally available memory we can use
          auto global_queue = s_globally_available.try_pop();
          if (global_queue) {
            // Found available memory from another thread
            s_available_on_thread.available_indices = std::move(*global_queue);
          }
        }
          
        if (s_available_on_thread.empty()) {
          // No free memory available, allocate new
          // this will return the index of the desired element
          INDEX_TYPE old_index = s_elements_reserved.load();
          INDEX_TYPE index = s_elements_reserved.fetch_add(1);
          if (index < old_index) {
            std::cerr << "Ran out of memory." << std::endl;
            std::abort();
          }
          INDEX_TYPE index_in_superbuffer;
          INDEX_TYPE index_in_buffer;
          std::tie(index_in_superbuffer, index_in_buffer) =
            split_index(index);

          // There are three options here:
          //
          // 1) the index is equals to the current capacity, that
          // means it's our job to expand the capacity.
          //
          // 2) the index is smaller than the current capacity, that
          // means we can move forward.
          //
          // 3) the index is bigger than the current capacity, that
          // means we need to yield and wait for the thread in the
          // first case to do its allocation, and then we can proceed.
          if (index == s_elements_capacity) {

            buffer* bp = s_data_allocator.allocate(1);
            s_buffers[index_in_superbuffer] = bp;
            
            refcntbuffer* rcb =
              new(s_refcnt_allocator.allocate(1)) refcntbuffer;
            for ( auto i = rcb->begin(); i != rcb->end(); i++ ) {
              new(i) typename refcntbuffer::value_type;
              *i = 0;
            }

            s_refcntbuffers[index_in_superbuffer] = rcb;

            s_elements_capacity.fetch_add(1<<BUFFER_SIZE_BITS);

          } else {
            while (index > s_elements_capacity) {
              std::this_thread::yield();
            }
          }

          buffer* bp = s_buffers[index_in_superbuffer];
          void* s = &((*bp)[index_in_buffer]);
          return {
            s,
            index
          };
        } else {
          // Reuse memory from thread-local pool
          INDEX_TYPE index = s_available_on_thread.front();
          INDEX_TYPE index_in_superbuffer;
          INDEX_TYPE index_in_buffer;
          std::tie(index_in_superbuffer, index_in_buffer) =
            split_index(index);
          s_available_on_thread.pop();
          return {
            &((*(s_buffers[index_in_superbuffer]))[index_in_buffer]),
            index
          };
        }
      }
      
    public:

      storage() = default;

      inline static INDEX_TYPE get_elements_reserved() {
        return s_elements_reserved;
      }

      inline static INDEX_TYPE get_elements_capacity() {
        return s_elements_capacity;
      }

      inline static ref_type make_entity() {
        auto n = get_new_storage();
        type* initialized = new(std::get<0>(n)) T;
        INDEX_TYPE index = std::get<1>(n);
        return ref_type(initialized, index);
      }

      inline static ref_type make_entity(const T& other) {
        auto n = get_new_storage();
        T* initialized = new(std::get<0>(n)) T(other);
        return ref_type(initialized, std::get<1>(n));
      }

      inline static ref_type make_entity(T&& other) {
        auto n = get_new_storage();
        T* uninitialized = static_cast<T*>(std::get<0>(n));
        std::uninitialized_move_n(std::addressof(other), 1, uninitialized);
        return ref_type(uninitialized, std::get<1>(n));
      }

      inline static ref_type make_entity(std::initializer_list<T> init_list) {
        auto n = get_new_storage();
        T* initialized = new(std::get<0>(n)) T(init_list);
        return ref_type(initialized, std::get<1>(n));
      }

      inline static void refcnt_add(INDEX_TYPE index) {
        INDEX_TYPE index_in_superbuffer;
        INDEX_TYPE index_in_buffer;
        std::tie(index_in_superbuffer, index_in_buffer) =
          split_index(index);
        (*(s_refcntbuffers[index_in_superbuffer]))[index_in_buffer]
          .fetch_add(1);
      }
      
      inline static void refcnt_subtract(INDEX_TYPE index) {
        INDEX_TYPE index_in_superbuffer;
        INDEX_TYPE index_in_buffer;
        std::tie(index_in_superbuffer, index_in_buffer) =
          split_index(index);
        REFCNT_TYPE old =
          (*(s_refcntbuffers[index_in_superbuffer]))[index_in_buffer].
          fetch_sub(1);
        if (old == 1) {
          s_available_on_thread.push(index);
        }
      }

      inline static size_t return_free_pool_to_global() {
        if (s_available_on_thread.empty()) {
          return 0;
        }
        
        size_t count = s_available_on_thread.size();
        s_globally_available.push(std::move(s_available_on_thread.available_indices));
        
        // Create a new empty queue for this thread
        s_available_on_thread.available_indices = std::queue<INDEX_TYPE>();
        
        return count;
      }

    };

  }
}


#endif
