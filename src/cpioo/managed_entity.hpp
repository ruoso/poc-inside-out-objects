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
    struct reference {
      STORAGE& d_storage;
      const size_t d_index;

      reference
      (STORAGE& storage,
       const size_t index)
        :d_storage(storage),
         d_index(index) {
        d_storage.refcnt_add(index);
      }

      reference(const reference& other)
        :d_storage(other.d_storage),
         d_index(other.d_index) {
        d_storage.refcnt_add(d_index);
      }

      reference(reference&&) = default;

      ~reference() {
        d_storage.refcnt_subtract(d_index);
      }

      typename STORAGE::type* operator->() {
        return d_storage.access(d_index);
      }

      const typename STORAGE::type * operator->() const {
        return d_storage.access_const(d_index);
      }

    };

    constexpr int buffer_size(int buffer_size_bits) {
      return 1 << buffer_size_bits;
    }
      
    template <
      class T,
      std::size_t BUFFER_SIZE_BITS = 10,
      std::size_t BUFFER_COUNT = 1024 * 1024,
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

    private:
      DATA_ALLOCATOR d_data_allocator;
      REFCNT_ALLOCATOR d_refcnt_allocator;
      superbuffer d_buffers;
      refcntsuperbuffer d_refcntbuffers;

      // memory freed on one thread is only available on that thread
      inline static thread_local
      std::map<
        storage*,
        std::queue<size_t>
        > s_available_on_thread;

      std::atomic<size_t> d_elements_reserved = 0;
      std::atomic<size_t> d_elements_capacity = 0;

      std::tuple<size_t, size_t>
      constexpr split_index(size_t index) const {
        size_t index_in_superbuffer = index >> BUFFER_SIZE_BITS;
        size_t index_in_buffer = index & ((1 << BUFFER_SIZE_BITS)-1);
        return {index_in_superbuffer, index_in_buffer};
      }
      
      std::tuple<void*, size_t>
      get_new_storage() {
        // We try to consume any memory already available to this
        // thread before trying to do anything that would cause a
        // synchronization requirement.
        if (s_available_on_thread[this].empty()) {

          // this will return the index of the desired element
          size_t index = d_elements_reserved.fetch_add(1);
          size_t index_in_superbuffer;
          size_t index_in_buffer;
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
          if (index == d_elements_capacity) {

            buffer* bp = d_data_allocator.allocate(1);
            d_buffers[index_in_superbuffer] = bp;
            
            refcntbuffer* rcb =
              new(d_refcnt_allocator.allocate(1)) refcntbuffer;
            for ( auto i = rcb->begin(); i != rcb->end(); i++ ) {
              new(i) typename refcntbuffer::value_type;
            }

            d_refcntbuffers[index_in_superbuffer] = rcb;
            d_elements_capacity.fetch_add(1<<BUFFER_SIZE_BITS);

          } else {
            while (index > d_elements_capacity) {
              std::this_thread::yield();
            }
          }

          return {
            &(d_buffers[index_in_superbuffer][index_in_buffer]),
            index
          };
        } else {
          // TODO: opportunistically release memory from the thread
          size_t index = s_available_on_thread[this].front();
          size_t index_in_superbuffer;
          size_t index_in_buffer;
          std::tie(index_in_superbuffer, index_in_buffer) =
            split_index(index);
          s_available_on_thread[this].pop();
          return {
            &((*(d_buffers[index_in_superbuffer]))[index_in_buffer]),
            index
          };
        }
      }
      
    public:

      storage() = default;

      inline size_t get_elements_reserved() {
        return d_elements_reserved;
      }

      inline size_t get_elements_capacity() {
        return d_elements_capacity;
      }

      ref_type make_entity() {
        auto n = get_new_storage();
        T* initialized = new(std::get<0>(n)) T;
        return reference(*this, std::get<1>(n));
      }

      ref_type make_entity(const T& other) {
        auto n = get_new_storage();
        T* initialized = new(std::get<0>(n)) T(other);
        return reference(*this, std::get<1>(n));
      }

      ref_type make_entity(T&& other) {
        auto n = get_new_storage();
        T* uninitialized = static_cast<T*>(std::get<0>(n));
        std::uninitialized_move_n(std::addressof(other), 1, uninitialized);
        return reference(*this, std::get<1>(n));
      }

      void refcnt_add(size_t index) {
        size_t index_in_superbuffer;
        size_t index_in_buffer;
        std::tie(index_in_superbuffer, index_in_buffer) =
          split_index(index);
        (*(d_refcntbuffers[index_in_superbuffer]))[index_in_buffer]
          .fetch_add(1);
      }
      
      void refcnt_subtract(size_t index) {
        size_t index_in_superbuffer;
        size_t index_in_buffer;
        std::tie(index_in_superbuffer, index_in_buffer) =
          split_index(index);
        REFCNT_TYPE old =
          (*(d_refcntbuffers[index_in_superbuffer]))[index_in_buffer].
          fetch_sub(1);
        if (old == 1) {
          s_available_on_thread[this].push(index);
        }
      }

      type* access(size_t index) {
        size_t index_in_superbuffer;
        size_t index_in_buffer;
        std::tie(index_in_superbuffer, index_in_buffer) =
          split_index(index);
        return &((*(d_buffers[index_in_superbuffer]))[index_in_buffer]);
      }

      const type* access_const(size_t index) const {
        size_t index_in_superbuffer;
        size_t index_in_buffer;
        std::tie(index_in_superbuffer, index_in_buffer) =
          split_index(index);
        return &((*(d_buffers[index_in_superbuffer]))[index_in_buffer]);
      }

    };

  }
}


#endif
