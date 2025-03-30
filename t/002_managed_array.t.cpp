#include <cpioo/managed_entity.hpp>
#include "gtest/gtest.h"
#include <future>

struct TestStruct {
  double a;
  double b;
  int c;
  int d;
};

using storage_t = cpioo::managed_entity::storage<TestStruct, 1, short>;
using reference_t = cpioo::managed_entity::reference<storage_t>;
using immutable_t = const cpioo::managed_entity::reference<storage_t>;

TEST(t_002_managed_array, initialize) {
  // new storage is empty
  storage_t storage;
  EXPECT_EQ(0, storage.get_elements_reserved());
  EXPECT_EQ(0, storage.get_elements_capacity());

  // making an entity should create the capacity, and reserve just one.
  reference_t o1 = storage.make_entity({42, 0.0, 1, 2});
  EXPECT_EQ(42, o1->a);
  EXPECT_EQ(1, storage.get_elements_reserved());
  EXPECT_EQ(2, storage.get_elements_capacity());

  // getting a const reference should not change storage, and value
  // should resolve the same way.
  immutable_t imm1 = o1;
  EXPECT_EQ(1, storage.get_elements_reserved());
  EXPECT_EQ(2, storage.get_elements_capacity());
  EXPECT_EQ(42, imm1->a);

  {
    // create and destroy objects, make sure we clear them
    reference_t o2 = storage.make_entity();
    EXPECT_EQ(2, storage.get_elements_reserved());
    EXPECT_EQ(2, storage.get_elements_capacity());
    reference_t o3 = storage.make_entity();
    EXPECT_EQ(3, storage.get_elements_reserved());
    EXPECT_EQ(4, storage.get_elements_capacity());
    reference_t o4 = storage.make_entity();
    EXPECT_EQ(4, storage.get_elements_reserved());
    EXPECT_EQ(4, storage.get_elements_capacity());
    reference_t o5 = storage.make_entity();
    EXPECT_EQ(5, storage.get_elements_reserved());
    EXPECT_EQ(6, storage.get_elements_capacity());

    // the copy of the reference shouldn't change it
    immutable_t imm2 = imm1;
    EXPECT_EQ(5, storage.get_elements_reserved());
    EXPECT_EQ(6, storage.get_elements_capacity());
    EXPECT_EQ(42, imm2->a);
  }

  {
    // this should reuse objects we freed above and consumed the queue
    // without adding a new buffer.
    reference_t o2 = storage.make_entity();
    EXPECT_EQ(5, storage.get_elements_reserved());
    EXPECT_EQ(6, storage.get_elements_capacity());
    reference_t o3 = storage.make_entity();
    EXPECT_EQ(5, storage.get_elements_reserved());
    EXPECT_EQ(6, storage.get_elements_capacity());
    reference_t o4 = storage.make_entity();
    EXPECT_EQ(5, storage.get_elements_reserved());
    EXPECT_EQ(6, storage.get_elements_capacity());
    reference_t o5 = storage.make_entity();
    EXPECT_EQ(5, storage.get_elements_reserved());
    EXPECT_EQ(6, storage.get_elements_capacity());
    reference_t o6 = storage.make_entity();
    EXPECT_EQ(6, storage.get_elements_reserved());
    EXPECT_EQ(6, storage.get_elements_capacity());
  }
}

TEST(t_002_managed_array, max_capacity) {
  using max_storage_t = cpioo::managed_entity::storage<TestStruct, 2, short>;
  max_storage_t storage;

  // Fill the storage to its maximum capacity
  for (int i = 0; i < 4; ++i) {
    auto ref = storage.make_entity({1.0 * i, 2.0 * i, i, i});
    EXPECT_EQ(i, ref->c);
  }

  // Attempting to allocate beyond capacity should still work due to dynamic expansion
  auto ref = storage.make_entity({4.0, 8.0, 4, 4});
  EXPECT_EQ(4, ref->c);
}

TEST(t_002_managed_array, reuse_freed_memory) {
  storage_t storage;

  // Create and destroy entities
  {
    auto ref1 = storage.make_entity({1.0, 2.0, 3, 4});
    auto ref2 = storage.make_entity({5.0, 6.0, 7, 8});
    EXPECT_EQ(2, storage.get_elements_reserved());
    // ref1 and ref2 go out of scope here
  }

  EXPECT_EQ(2, storage.get_elements_reserved());  // Reserved count remains

  // Reuse freed memory
  auto ref3 = storage.make_entity({9.0, 10.0, 11, 12});
  EXPECT_EQ(2, storage.get_elements_reserved());
  EXPECT_EQ(12, ref3->d);
}

TEST(t_002_managed_array, multi_threaded_access) {
  storage_t storage;

  constexpr int thread_count = 4;
  constexpr int entities_per_thread = 10;

  // Use a fixed-size array of promises for references
  std::array<std::array<std::promise<reference_t>, entities_per_thread>, thread_count> promises;

  // Initialize futures from promises
  auto thread_func = [&storage, &promises](int thread_id) {
    for (int i = 0; i < entities_per_thread; ++i) {
      std::cerr << "Thread " << thread_id << " creating entity " << i << std::endl;
      promises[thread_id][i].set_value(storage.make_entity({1.0, 1.0, i, i}));
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < thread_count; ++i) {
    threads.emplace_back(thread_func, i);
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Step 1: Ensure all elements are reserved
  EXPECT_EQ(thread_count * entities_per_thread, storage.get_elements_reserved());

  // Step 2: Validate that all references are released
  {
    for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
      for (int i = 0; i < entities_per_thread; ++i) {
        auto ref = promises[thread_id][i].get_future().get(); // Get the reference
        EXPECT_EQ(i, ref->c);
      }
    }
  }

  // After the inner scope, all references should be released
  auto released = storage.return_free_pool_to_global();
  EXPECT_EQ(thread_count * entities_per_thread, released);
}

TEST(t_002_managed_array, create_from_initializer_list) {
  storage_t storage;

  // Create an entity using an initializer list
  auto ref = storage.make_entity({1.0, 2.0, 3, 4});
  EXPECT_EQ(1.0, ref->a);
  EXPECT_EQ(2.0, ref->b);
  EXPECT_EQ(3, ref->c);
  EXPECT_EQ(4, ref->d);

  // Ensure storage reflects the correct state
  EXPECT_EQ(1, storage.get_elements_reserved());
  EXPECT_EQ(2, storage.get_elements_capacity());
}
