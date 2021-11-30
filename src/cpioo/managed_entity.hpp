#ifndef CPIOO_MANAGED_ENTITY_HPP
#define CPIOO_MANAGED_ENTITY_HPP

#include <cpioo/version.hpp>

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

    template < class STORAGE >
    class reference {
      typename STORAGE::type* const d_ptr;
      const typename STORAGE::index_type d_index;

    public:
      reference
      (typename STORAGE::type* ptr,
       const typename STORAGE::index_type index)
        :d_ptr(ptr),
         d_index(index) {
        STORAGE::refcnt_add(index);
      }

      reference(const reference& other)
        :d_ptr(other.d_ptr),
         d_index(other.d_index) {
        STORAGE::refcnt_add(d_index);
      }

      reference(reference&&) = default;

      ~reference() {
        STORAGE::refcnt_subtract(d_index);
      }

      typename STORAGE::type* operator->() {
        return d_ptr;
      }

      const typename STORAGE::type * const operator->() const {
        return d_ptr;
      }

    };

    constexpr int buffer_size(int buffer_size_bits) {
      return 1 << buffer_size_bits;
    }

    constexpr int buffer_count(int buffer_size_bits, int sizeof_index) {
      return (1 << ((sizeof_index*8)+1)) - 1 - buffer_size(buffer_size_bits);
    }
      
    template <
      class T,
      std::size_t BUFFER_SIZE_BITS = 10,
      typename INDEX_TYPE = size_t,
      std::size_t BUFFER_COUNT = buffer_count(BUFFER_SIZE_BITS,
                                              sizeof(INDEX_TYPE)),
      typename REFCNT_TYPE = short,
      class DATA_ALLOCATOR = std::allocator<
        std::array<T, buffer_size(BUFFER_SIZE_BITS) >
        >,
      class REFCNT_ALLOCATOR = std::allocator<
        std::array<
          std::atomic<REFCNT_TYPE>, buffer_size(BUFFER_SIZE_BITS)
          >
        >
      >
    class storage {
    public:
      using refcntbuffer =
        std::array<std::atomic<REFCNT_TYPE>, buffer_size(BUFFER_SIZE_BITS) >;
      using refcntsuperbuffer = std::array<refcntbuffer*, BUFFER_COUNT>;
      
      using buffer = std::array<T, buffer_size(BUFFER_SIZE_BITS)>;
      using superbuffer = std::array<buffer*, BUFFER_COUNT>;

      using type = T;
      using ref_type = reference<storage>;
      using index_type = INDEX_TYPE;

    private:
      inline static DATA_ALLOCATOR s_data_allocator;
      inline static REFCNT_ALLOCATOR s_refcnt_allocator;
      inline static superbuffer s_buffers;
      inline static refcntsuperbuffer s_refcntbuffers;

      // memory freed on one thread is only available on that thread
      inline static thread_local
      std::queue<INDEX_TYPE> s_available_on_thread;

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

          // this will return the index of the desired element
          INDEX_TYPE index = s_elements_reserved.fetch_add(1);
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
            }

            s_refcntbuffers[index_in_superbuffer] = rcb;
            s_elements_capacity.fetch_add(1<<BUFFER_SIZE_BITS);

          } else {
            while (index > s_elements_capacity) {
              std::this_thread::yield();
            }
          }

          return {
            &(s_buffers[index_in_superbuffer][index_in_buffer]),
            index
          };
        } else {
          // TODO: opportunistically release memory from the thread
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

    };

  }
}


#endif
