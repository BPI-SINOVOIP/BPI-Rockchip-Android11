/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "utils/variant.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

TEST(VariantTest, GetType) {
  EXPECT_EQ(Variant().GetType(), Variant::TYPE_EMPTY);
  EXPECT_EQ(Variant(static_cast<int8_t>(9)).GetType(),
            Variant::TYPE_INT8_VALUE);
  EXPECT_EQ(Variant(static_cast<uint8_t>(9)).GetType(),
            Variant::TYPE_UINT8_VALUE);
  EXPECT_EQ(Variant(static_cast<int>(9)).GetType(), Variant::TYPE_INT_VALUE);
  EXPECT_EQ(Variant(static_cast<uint>(9)).GetType(), Variant::TYPE_UINT_VALUE);
  EXPECT_EQ(Variant(static_cast<int64>(9)).GetType(),
            Variant::TYPE_INT64_VALUE);
  EXPECT_EQ(Variant(static_cast<uint64>(9)).GetType(),
            Variant::TYPE_UINT64_VALUE);
  EXPECT_EQ(Variant(static_cast<float>(9)).GetType(),
            Variant::TYPE_FLOAT_VALUE);
  EXPECT_EQ(Variant(static_cast<double>(9)).GetType(),
            Variant::TYPE_DOUBLE_VALUE);
  EXPECT_EQ(Variant(true).GetType(), Variant::TYPE_BOOL_VALUE);
  EXPECT_EQ(Variant("hello").GetType(), Variant::TYPE_STRING_VALUE);
}

TEST(VariantTest, HasValue) {
  EXPECT_FALSE(Variant().HasValue());
  EXPECT_TRUE(Variant(static_cast<int8_t>(9)).HasValue());
  EXPECT_TRUE(Variant(static_cast<uint8_t>(9)).HasValue());
  EXPECT_TRUE(Variant(static_cast<int>(9)).HasValue());
  EXPECT_TRUE(Variant(static_cast<uint>(9)).HasValue());
  EXPECT_TRUE(Variant(static_cast<int64>(9)).HasValue());
  EXPECT_TRUE(Variant(static_cast<uint64>(9)).HasValue());
  EXPECT_TRUE(Variant(static_cast<float>(9)).HasValue());
  EXPECT_TRUE(Variant(static_cast<double>(9)).HasValue());
  EXPECT_TRUE(Variant(true).HasValue());
  EXPECT_TRUE(Variant("hello").HasValue());
}

TEST(VariantTest, Value) {
  EXPECT_EQ(Variant(static_cast<int8_t>(9)).Value<int8>(), 9);
  EXPECT_EQ(Variant(static_cast<uint8_t>(9)).Value<uint8>(), 9);
  EXPECT_EQ(Variant(static_cast<int>(9)).Value<int>(), 9);
  EXPECT_EQ(Variant(static_cast<uint>(9)).Value<uint>(), 9);
  EXPECT_EQ(Variant(static_cast<int64>(9)).Value<int64>(), 9);
  EXPECT_EQ(Variant(static_cast<uint64>(9)).Value<uint64>(), 9);
  EXPECT_EQ(Variant(static_cast<float>(9)).Value<float>(), 9);
  EXPECT_EQ(Variant(static_cast<double>(9)).Value<double>(), 9);
  EXPECT_EQ(Variant(true).Value<bool>(), true);
  EXPECT_EQ(Variant("hello").ConstRefValue<std::string>(), "hello");
}

}  // namespace
}  // namespace libtextclassifier3
