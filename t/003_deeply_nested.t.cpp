#include <cpioo/managed_entity.hpp>
#include "gtest/gtest.h"

struct TestStruct1 {
  double a;
  double b;
  int c;
  int d;
};

using TestStruct1_Storage =
  cpioo::managed_entity::storage<TestStruct1>;
using TestStruct1_Ref =
  TestStruct1_Storage::ref_type;

struct TestStruct2 {
  double e;
  const TestStruct1_Storage::ref_type ts1;
};
using TestStruct2_Storage =
  cpioo::managed_entity::storage<TestStruct2>;
using TestStruct2_Ref =
  TestStruct2_Storage::ref_type;

struct TestStruct3 {
  double f;
  const TestStruct2_Storage::ref_type ts2;
};
using TestStruct3_Storage =
  cpioo::managed_entity::storage<TestStruct3>;
using TestStruct3_Ref =
  TestStruct3_Storage::ref_type;

TEST(t_003_deply_nested, initialize) {
  TestStruct1_Storage s1;
  TestStruct2_Storage s2;
  TestStruct3_Storage s3;

  TestStruct1_Ref r1 = s1.make_entity({ 1.0, 2.0, 3, 4 });
  TestStruct2_Ref r2 = s2.make_entity({ 5.0, r1 });
  TestStruct3_Ref r3 = s3.make_entity({ 6.0, r2 });

  ASSERT_EQ(4, r3->ts2->ts1->d);
}
