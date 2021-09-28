/*
 *
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#define TEEUI_DO_LOG_DEBUG

#include <teeui/error.h>
#include <teeui/log.h>
#include <teeui/utils.h>

namespace teeui {

namespace test {

TEST(TeeUIUtilsTest, intersectTest) {
    PxVec a(0.0, 1.0);
    PxPoint b(0.0, -2.0);
    PxVec c(1.0, 0.0);
    PxPoint d(3.0, 0.0);
    auto result = intersect(a, b, c, d);
    ASSERT_TRUE(result);
    ASSERT_EQ(PxPoint(0.0, 0.0), *result);

    // one of the directional vectors is (0,0)
    a = {0.0, 1.0};
    b = {0.0, -2.0};
    c = {0.0, 0.0};
    d = {3.0, 0.0};
    result = intersect(a, b, c, d);
    ASSERT_FALSE(result);
    result = intersect(c, d, a, b);
    ASSERT_FALSE(result);

    a = {0.0, 0.0};
    b = {0.0, -2.0};
    c = {1.0, 0.0};
    d = {3.0, 0.0};
    result = intersect(a, b, c, d);
    ASSERT_FALSE(result);
    result = intersect(c, d, a, b);
    ASSERT_FALSE(result);

    a = {0.0, 0.0};
    b = {0.0, -2.0};
    c = {0.0, 0.0};
    d = {3.0, 0.0};
    result = intersect(a, b, c, d);
    ASSERT_FALSE(result);
    result = intersect(c, d, a, b);
    ASSERT_FALSE(result);

    // lines are parallel
    a = {0.0, 1.0};
    b = {0.0, -2.0};
    c = {0.0, 2.0};
    d = {3.0, 0.0};
    result = intersect(a, b, c, d);
    ASSERT_FALSE(result);
    result = intersect(c, d, a, b);
    ASSERT_FALSE(result);

    a = {3.0, 1.0};
    b = {0.0, -2.0};
    c = {6.0, 2.0};
    d = {0.0, 4.0};
    result = intersect(a, b, c, d);
    ASSERT_FALSE(result);
    result = intersect(c, d, a, b);
    ASSERT_FALSE(result);

    a = {1.0, 1.0};
    b = {0.0, -0.5};
    c = {1.0, 0.0};
    d = {0.0, 0.0};
    result = intersect(a, b, c, d);
    ASSERT_TRUE(result);
    ASSERT_EQ(PxPoint(0.5, 0.0), *result);
    result = intersect(c, d, a, b);
    ASSERT_TRUE(result);
    ASSERT_EQ(PxPoint(0.5, 0.0), *result);

    a = {-1.0, -1.0};
    b = {0.0, -0.5};
    c = {1.0, 0.0};
    d = {0.0, 0.0};
    result = intersect(a, b, c, d);
    ASSERT_TRUE(result);
    ASSERT_EQ(PxPoint(0.5, 0.0), *result);
    result = intersect(c, d, a, b);
    ASSERT_TRUE(result);
    ASSERT_EQ(PxPoint(0.5, 0.0), *result);

    a = {1.0, -1.0};
    b = {0.0, 1.0};
    c = {1.0, 1.0};
    d = {0.0, 0.0};
    result = intersect(a, b, c, d);
    ASSERT_TRUE(result);
    ASSERT_EQ(PxPoint(0.5, 0.5), *result);
    result = intersect(c, d, a, b);
    ASSERT_TRUE(result);
    ASSERT_EQ(PxPoint(0.5, 0.5), *result);
}

TEST(TeeUIUtilsTest, ConvexObjectConstruction) {
    constexpr ConvexObject<10> o{{.0, .0}, {1.0, .0}, {1.0, 1.0}, {.0, 1.0}};
    ASSERT_EQ(size_t(4), o.size());
}

TEST(TeeUIUtilsTest, ConvexObjectLineIntersection) {
    constexpr ConvexObject<10> o{{.0, .0}, {1.0, .0}, {1.0, 1.0}, {.0, 1.0}};
    ASSERT_EQ(size_t(4), o.size());

    // diagonally through the corners
    auto o2 = o.intersect<10>({.0, .0}, {1.0, 1.0});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(3), o2->size());
    ASSERT_EQ(pxs(.5), o2->area());

    // diagonally through the corners reversed
    o2 = o.intersect<10>({1.0, 1.0}, {.0, .0});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(3), o2->size());
    ASSERT_EQ(pxs(.5), o2->area());

    // diagonally through the top right corner
    o2 = o.intersect<10>({.0, 2.0}, {2.0, .0});
    ASSERT_FALSE(o2);

    // diagonally through the top right corner reversed
    o2 = o.intersect<10>({2.0, .0}, {.0, 2.0});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(4), o2->size());
    ASSERT_EQ(pxs(1.0), o2->area());

    // diagonally through the top left corner
    o2 = o.intersect<10>({-1.0, .0}, {1.0, 2.0});
    ASSERT_FALSE(o2);

    // diagonally through the top left corner reversed
    o2 = o.intersect<10>({1.0, 2.0}, {-1.0, .0});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(4), o2->size());
    ASSERT_EQ(pxs(1.0), o2->area());

    // diagonally through the bottom right corner
    o2 = o.intersect<10>({2.0, 1.0}, {.0, -1.0});
    ASSERT_FALSE(o2);

    // diagonally through the bottom right corner reversed
    o2 = o.intersect<10>({.0, -1.0}, {2.0, 1.0});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(4), o2->size());
    ASSERT_EQ(pxs(1.0), o2->area());

    // diagonally through the bottom left corner
    o2 = o.intersect<10>({1.0, -1.0}, {-1.0, 1.0});
    ASSERT_FALSE(o2);

    // diagonally through the top right corner reversed
    o2 = o.intersect<10>({-1.0, 1.0}, {1.0, -1.0});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(4), o2->size());
    ASSERT_EQ(pxs(1.0), o2->area());

    // through two corners
    o2 = o.intersect<10>({-1.0, 1.0}, {2.0, 1.0});
    ASSERT_FALSE(o2);

    // through two corners reversed
    o2 = o.intersect<10>({2.0, 1.0}, {-1.0, 1.0});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(4), o2->size());
    ASSERT_EQ(pxs(1.0), o2->area());

    o2 = o.intersect<10>({.0, -.5}, {.5, .0});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(5), o2->size());
    ASSERT_EQ(pxs(.875), o2->area());

    o2 = o.intersect<10>({.0, .5}, {.5, .0});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(5), o2->size());
    ASSERT_EQ(pxs(.875), o2->area());

    ConvexObject<10> o3{{-1.3845, 23.0}, {-0.384501, 23}, {-0.384501, 24}, {-1.3845, 24}};
    o2 = o3.intersect<10>({-3.3845, 25.3339}, {7.59022, 14.3592});
    ASSERT_TRUE(o2);
    ASSERT_EQ(size_t(5), o2->size());
}

TEST(TeeUIUtilsTest, ErrorOperatorOrOverloadTest) {
    // This expression should evaluate to the first (non OK) error code in the sequence.
    ASSERT_EQ(Error::NotInitialized,
              Error::OK || Error::NotInitialized || Error::FaceNotLoaded || Error::OK);
}

}  // namespace test

}  // namespace teeui
