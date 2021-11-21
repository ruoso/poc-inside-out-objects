#include <cpioo/managed_entity.hpp>
#include "gtest/gtest.h"

struct TestStruct {
  double a;
  double b;
  int c;
  int d;
};

using storage_t = cpioo::managed_entity::storage<TestStruct, 2>;
using reference_t = cpioo::managed_entity::reference<storage_t>;
using immutable_t = const cpioo::managed_entity::reference<storage_t>;

TEST(t_002_managed_array, initialize) {

  storage_t storage ;
  reference_t o1 = storage.make_entity();
  o1->a = 42;

  immutable_t imm1 = o1;
  EXPECT_EQ(42, imm1->a);

  {
    // create and destroy objects, make sure we clear them
    reference_t o2 = storage.make_entity();
    reference_t o3 = storage.make_entity();
    reference_t o4 = storage.make_entity();
    reference_t o5 = storage.make_entity();
  
    immutable_t imm2 = imm1;
    
    EXPECT_EQ(3, storage.get_buffer_count());
    EXPECT_EQ(1, storage.get_top_buffer_count());
    EXPECT_EQ(42, imm2->a);
  }

  {
    // this should reuse objects we freed above and consumed the queue
    // without adding a new buffer.
    reference_t o2 = storage.make_entity();
    reference_t o3 = storage.make_entity();
    reference_t o4 = storage.make_entity();
    reference_t o5 = storage.make_entity();
    reference_t o6 = storage.make_entity();
    EXPECT_EQ(3, storage.get_buffer_count());
    EXPECT_EQ(2, storage.get_top_buffer_count());
  }
}
