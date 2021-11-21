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

namespace cpioo {
  namespace managed_entity {

    template < class STORAGE >
    struct reference {
      STORAGE& d_storage;
      const typename STORAGE::superbuffer::size_type d_superbuffer_index;
      const typename STORAGE::buffer::size_type d_buffer_index;

      reference
      (STORAGE& storage,
       const typename STORAGE::superbuffer::size_type superbuffer_index,
       const typename STORAGE::buffer::size_type buffer_index)
        :d_storage(storage),
         d_superbuffer_index(superbuffer_index),
         d_buffer_index(buffer_index) {
        d_storage.refcnt_add(superbuffer_index, buffer_index);
      }

      reference(const reference& other)
        :d_storage(other.d_storage),
         d_superbuffer_index(other.d_superbuffer_index),
         d_buffer_index(other.d_buffer_index) {
        d_storage.refcnt_add(d_superbuffer_index, d_buffer_index);
      }

      reference(reference&&) = default;

      ~reference() {
        d_storage.refcnt_subtract(d_superbuffer_index, d_buffer_index);
      }

      typename STORAGE::type* operator->() {
        return d_storage.access(d_superbuffer_index, d_buffer_index);
      }

      const typename STORAGE::type * operator->() const {
        return d_storage.access_const(d_superbuffer_index, d_buffer_index);
      }

    };
      
    template <
      class T,
      std::size_t BUFFER_SIZE = 1024,
      typename REFCNT_TYPE = short,
      class DATA_ALLOCATOR = std::allocator< std::array<T, BUFFER_SIZE> >,
      class REFCNT_ALLOCATOR = std::allocator<
        std::array< REFCNT_TYPE, BUFFER_SIZE >
        >
      >
    class storage {
    public:
     
      using refcntbuffer = std::array<REFCNT_TYPE, BUFFER_SIZE>;
      using refcntsuperbuffer = std::vector<refcntbuffer*>;
      
      using buffer = std::array<T, BUFFER_SIZE>;
      using superbuffer = std::vector<buffer*>;

      using type = T;
      using ref_type = reference<storage>;

    private:
      DATA_ALLOCATOR d_data_allocator;
      REFCNT_ALLOCATOR d_refcnt_allocator;
      superbuffer d_buffers;
      refcntsuperbuffer d_refcntbuffers;

      // memory freed on one thread is only available on that thread
      inline static thread_local std::map<
        storage*,
        std::queue<
          std::pair<
            typename superbuffer::size_type,
            typename buffer::size_type
            >
          >
        > s_available_on_thread;

      typename superbuffer::size_type d_buffer_count = 0;
      typename buffer::size_type d_top_buffer_count = BUFFER_SIZE;

      std::tuple<void*,
                 typename superbuffer::size_type,
                 typename buffer::size_type>
      get_new_storage() {
        // TODO: make thread-safe
        if (s_available_on_thread[this].empty()) {
          if (d_top_buffer_count >= BUFFER_SIZE) {
            buffer* bp = d_data_allocator.allocate(1);
            d_buffers.push_back(bp);
            
            refcntbuffer* rcb =
              new(d_refcnt_allocator.allocate(1)) refcntbuffer;
          
            std::fill(rcb->begin(), rcb->end(), 0);
            d_refcntbuffers.push_back(rcb);
          
            d_top_buffer_count = 1;
            d_buffer_count++;
            return {
              &(bp[d_top_buffer_count - 1]),
              d_buffer_count - 1,
              d_top_buffer_count - 1
            };
          } else {
            buffer* bp = d_buffers[d_buffer_count - 1];
            d_top_buffer_count++;
            return {
              &(bp[d_top_buffer_count - 1]),
              d_buffer_count - 1,
              d_top_buffer_count -1
            };
          }
        } else {
          // TODO: 
          auto entry = s_available_on_thread[this].front();
          s_available_on_thread[this].pop();
          return {
            &((*(d_buffers[entry.first]))[entry.second]),
            entry.first,
            entry.second
          };
        }
      }
      
    public:

      storage() = default;

      inline typename superbuffer::size_type get_buffer_count() {
        return d_buffer_count;
      }

      inline typename buffer::size_type get_top_buffer_count() {
        return d_top_buffer_count;
      }

      ref_type make_entity() {
        auto n = get_new_storage();
        T* initialized = new(std::get<0>(n)) T;
        return reference(*this, std::get<1>(n), std::get<2>(n));
      }

      ref_type make_entity(const T& other) {
        auto n = get_new_storage();
        T* initialized = new(std::get<0>(n)) T(other);
        return reference(*this, std::get<1>(n), std::get<2>(n));
      }

      ref_type make_entity(T&& other) {
        auto n = get_new_storage();
        T* uninitialized = static_cast<T*>(std::get<0>(n));
        std::uninitialized_move_n(std::addressof(other), 1, uninitialized);
        return reference(*this, std::get<1>(n), std::get<2>(n));
      }

      void refcnt_add(typename superbuffer::size_type superbuffer_index,
                      typename buffer::size_type buffer_index) {
        (*(d_refcntbuffers[superbuffer_index]))[buffer_index]++;
      }
      
      void refcnt_subtract(typename superbuffer::size_type superbuffer_index,
                           typename buffer::size_type buffer_index) {
        if (--(*(d_refcntbuffers[superbuffer_index]))[buffer_index] <= 0) {
          s_available_on_thread[this].push({superbuffer_index, buffer_index});
        }
      }

      type* access
      (typename superbuffer::size_type superbuffer_index,
       typename buffer::size_type buffer_index) {
        return &((*(d_buffers[superbuffer_index]))[buffer_index]);
      }

      const type* access_const
      (
       typename superbuffer::size_type superbuffer_index,
       typename buffer::size_type buffer_index) const {
        return &((*(d_buffers[superbuffer_index]))[buffer_index]);
      }

    };

  }
}


#endif
