
#include "tests/gtest/gtest.h"
#include "pbrt.h"
#include "mathutil.h"
#include <vector>

using namespace pbrt;

TEST(GetIntArrayerval, Basics) {
  std::vector<float> a{0,1,2,3,4,5,6,7,8,9};

  // Check clamping for out of range
  EXPECT_EQ(0, GetIntArrayerval(a.size(),
                            [&](int index) { return a[index] <= -1; }));
  EXPECT_EQ(a.size() - 2,
            GetIntArrayerval(a.size(),
                         [&](int index) { return a[index] <= 100; }));

  for (size_t i = 0; i < a.size() - 1; ++i) {
    EXPECT_EQ(i, GetIntArrayerval(a.size(),
                              [&](int index) { return a[index] <= i; }));
    EXPECT_EQ(i, GetIntArrayerval(a.size(),
                              [&](int index) { return a[index] <= i + 0.5; }));
    if (i > 0)
      EXPECT_EQ(i - 1,
                GetIntArrayerval(a.size(),
                             [&](int index) { return a[index] <= i - 0.5; }));
  }
}
