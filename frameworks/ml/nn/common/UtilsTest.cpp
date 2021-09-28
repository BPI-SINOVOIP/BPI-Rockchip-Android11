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
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "OperationsUtils.cpp"
#include "QuantUtils.h"

namespace android {
namespace nn {
namespace wrapper {

namespace {
using ::testing::ElementsAreArray;
}  // namespace

TEST(CalculateBroadcastedShapeTest, Basic) {
    Shape shape1;
    Shape shape2;
    shape1.dimensions = {4, 3, 2, 1};
    shape2.dimensions = {3, 1, 5};

    Shape expectedOutputShape;
    expectedOutputShape.dimensions = {4, 3, 2, 5};

    Shape actualOutputShape;
    EXPECT_TRUE(calculateBroadcastedShape(shape1, shape2, &actualOutputShape));
    EXPECT_THAT(actualOutputShape.dimensions, ElementsAreArray(expectedOutputShape.dimensions));

    EXPECT_TRUE(calculateBroadcastedShape(shape2, shape1, &actualOutputShape));
    EXPECT_THAT(actualOutputShape.dimensions, ElementsAreArray(expectedOutputShape.dimensions));
}

TEST(CalculateBroadcastedShapeTest, FailsOnIncompatible) {
    Shape shape1;
    Shape shape2;
    shape1.dimensions = {5};
    shape2.dimensions = {3};

    Shape actualOutputShape;
    EXPECT_FALSE(calculateBroadcastedShape(shape1, shape2, &actualOutputShape));
    EXPECT_FALSE(calculateBroadcastedShape(shape2, shape1, &actualOutputShape));
}

static int32_t getExtensionType(uint16_t extensionPrefix, uint16_t typeWithinExtension) {
    constexpr uint8_t kLowBitsType = static_cast<uint8_t>(ExtensionTypeEncoding::LOW_BITS_TYPE);
    int32_t type = (extensionPrefix << kLowBitsType) | typeWithinExtension;
    EXPECT_TRUE(isExtensionOperandType(static_cast<OperandType>(type)));
    return type;
}

TEST(TensorHasUnspecifiedDimensionsTest, ExtensionTensorWithUnspecifiedRank) {
    // Regression test for b/124285861.
    EXPECT_TRUE(tensorHasUnspecifiedDimensions(getExtensionType(1, 0), /*dim=*/nullptr,
                                               /*dimCount=*/0));
}

TEST(ValidateOperandTypeTest, ExtensionTensorWithUnspecifiedRank) {
    // Regression test for b/124104123.
    constexpr uint16_t kExtensionPrefix = 1;
    constexpr uint16_t kTypeWithinExtension = 0;
    int32_t extensionType = getExtensionType(kExtensionPrefix, kTypeWithinExtension);
    ANeuralNetworksOperandType type = {
            .type = extensionType,
            .dimensionCount = 0,
            .dimensions = nullptr,
    };
    Extension::OperandTypeInformation info = {
            .type = kTypeWithinExtension,
            .isTensor = true,
            .byteSize = 4,
    };
    EXPECT_EQ(validateOperandType(type, &info, /*tag=*/"test", /*allowPartial=*/true),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(validateOperandType(type, &info, /*tag=*/"test", /*allowPartial=*/false),
              ANEURALNETWORKS_BAD_DATA);
}

TEST(ValidateOperandTypeTest, ExtensionTypeDimensionProductOverflow) {
    // Regression test for b/146044137.
    constexpr uint16_t kExtensionPrefix = 1;
    constexpr uint16_t kTypeWithinExtension = 0;
    int32_t extensionType = getExtensionType(kExtensionPrefix, kTypeWithinExtension);
    uint32_t dimensions[] = {5, 4, 4, 786433, 5, 3, 16777216, 4, 5};
    ANeuralNetworksOperandType type = {
            .type = extensionType,
            .dimensionCount = std::size(dimensions),
            .dimensions = dimensions,
    };
    Extension::OperandTypeInformation info = {
            .type = kTypeWithinExtension,
            .isTensor = true,
            .byteSize = 1,
    };
    EXPECT_EQ(validateOperandType(type, &info, /*tag=*/"test", /*allowPartial=*/true),
              ANEURALNETWORKS_BAD_DATA);
}

TEST(ValidateOperandTypeTest, TensorSizeDimensionProductOverflow) {
    // Regression test for b/146044137.
    uint32_t dimensions[] = {256, 256, 256, 256};
    ANeuralNetworksOperandType type = {
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = std::size(dimensions),
            .dimensions = dimensions,
    };
    EXPECT_EQ(validateOperandType(type, nullptr, /*tag=*/"test", /*allowPartial=*/true),
              ANEURALNETWORKS_BAD_DATA);
}

class CombineDimensionsTest : public ::testing::Test {
   protected:
    void testCompatible(const std::vector<uint32_t>& lhs, const std::vector<uint32_t>& rhs,
                        const std::vector<uint32_t>& expected) {
        SCOPED_TRACE("lhs = " + toString(lhs) + ", rhs = " + toString(rhs));
        const auto res = combineDimensions(lhs, rhs);
        ASSERT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), expected);
    }

    void testIncompatible(const std::vector<uint32_t>& lhs, const std::vector<uint32_t>& rhs) {
        SCOPED_TRACE("lhs = " + toString(lhs) + ", rhs = " + toString(rhs));
        const auto res = combineDimensions(lhs, rhs);
        EXPECT_FALSE(res.has_value());
    }
};

TEST_F(CombineDimensionsTest, Rank) {
    testCompatible({}, {1, 2, 3, 4}, {1, 2, 3, 4});
    testCompatible({1, 2, 3, 4}, {}, {1, 2, 3, 4});
    testCompatible({}, {}, {});
    testIncompatible({1, 2, 3}, {1, 2, 3, 4});
    testIncompatible({1, 2, 3, 4}, {1, 2, 3});
}

TEST_F(CombineDimensionsTest, Dimensions) {
    testCompatible({0, 0, 0, 0}, {1, 2, 3, 4}, {1, 2, 3, 4});
    testCompatible({1, 2, 3, 4}, {0, 0, 0, 0}, {1, 2, 3, 4});
    testCompatible({0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0});
    testIncompatible({1, 2, 3, 4}, {2, 2, 3, 4});
    testIncompatible({1, 2, 3, 4}, {1, 2, 3, 3});
}

TEST(QuantizationUtilsTest, QuantizeMultiplierSmallerThanOneExp) {
    auto checkInvalidQuantization = [](double value) {
        int32_t q;
        int s;
        EXPECT_FALSE(QuantizeMultiplierSmallerThanOneExp(value, &q, &s));
    };

    checkInvalidQuantization(-0.1);
    checkInvalidQuantization(0.0);
    // If we get close enough to 1.0 it crashes and dies in one of two ways:
    // Either the shift becomes negative or we trigger the 'less-than-one' CHECK.
    checkInvalidQuantization(1 - 1e-15);
    checkInvalidQuantization(1 - 1e-17);
    checkInvalidQuantization(1.0);

    auto checkQuantization = [](double value, int32_t goldenQuantized, int goldenShift) {
        int32_t q;
        int s;
        EXPECT_TRUE(QuantizeMultiplierSmallerThanOneExp(value, &q, &s));
        EXPECT_EQ(q, goldenQuantized);
        EXPECT_EQ(s, goldenShift);
    };

    checkQuantization(0.25, 1073741824, -1);
    checkQuantization(0.50 - 5e-9, 2147483627, -1);
    checkQuantization(0.50 - 1e-10, 1073741824, 0);
    checkQuantization(0.50, 1073741824, 0);
    checkQuantization(0.75, 1610612736, 0);
    checkQuantization(1 - 1e-9, 2147483646, 0);
}

TEST(QuantizationUtilsTest, QuantizeMultiplierGreaterThanOne) {
    auto checkInvalidQuantization = [](double value) {
        int32_t q;
        int s;
        EXPECT_FALSE(QuantizeMultiplierGreaterThanOne(value, &q, &s));
    };

    checkInvalidQuantization(1 + 1e-16);

    auto checkQuantization = [](double value, int32_t goldenQuantized, int goldenShift) {
        int32_t q;
        int s;
        EXPECT_TRUE(QuantizeMultiplierGreaterThanOne(value, &q, &s));
        EXPECT_EQ(q, goldenQuantized);
        EXPECT_EQ(s, goldenShift);
    };

    checkQuantization(1 + 1e-11, 1073741824, 1);
    checkQuantization(1.25, 1342177280, 1);
    checkQuantization(1.50, 1610612736, 1);
    checkQuantization(1.50, 1610612736, 1);
    checkQuantization(1.75, 1879048192, 1);
    checkQuantization(2 - 1e-9, 2147483647, 1);
    checkQuantization(2 - 1e-11, 1073741824, 2);
    checkQuantization(2, 1073741824, 2);
}

TEST(QuantizationUtilTest, QuantizeMultiplier) {
    auto checkQuantization = [](double value, int32_t goldenQuantized, int goldenShift) {
        int32_t q;
        int s;
        EXPECT_TRUE(QuantizeMultiplier(value, &q, &s));
        EXPECT_EQ(q, goldenQuantized);
        EXPECT_EQ(s, goldenShift);
    };

    checkQuantization(-4, -1073741824, 3);
    checkQuantization(-2, -1073741824, 2);
    checkQuantization(-1, -1073741824, 1);
    checkQuantization(-0.5, -1073741824, 0);
    checkQuantization(-0.25, -1073741824, -1);
    checkQuantization(-0.125, -1073741824, -2);
    checkQuantization(0, 0, 0);
    checkQuantization(0.125, 1073741824, -2);
    checkQuantization(0.25, 1073741824, -1);
    checkQuantization(0.5, 1073741824, 0);
    checkQuantization(1, 1073741824, 1);
    checkQuantization(2, 1073741824, 2);
    checkQuantization(4, 1073741824, 3);
}

TEST(QuantizationUtilTest, QuantizeMultiplierUnderflow) {
    auto checkQuantization = [](double value, int32_t goldenQuantized, int goldenShift) {
        int32_t q;
        int s;
        EXPECT_TRUE(QuantizeMultiplier(value, &q, &s));
        EXPECT_EQ(q, goldenQuantized);
        EXPECT_EQ(s, goldenShift);
    };

    checkQuantization(std::ldexp(1.0f, -31), 1073741824, -30);
    checkQuantization(std::ldexp(1.0f, -32), 1073741824, -31);
    checkQuantization(std::ldexp(0.99f, -32), 0, 0);
    checkQuantization(std::ldexp(1.0f, -33), 0, 0);
}

TEST(QuantizationUtilTest, GetInvSqrtQuantizedMultiplierExp) {
    auto checkInvSqrtQuantization = [](int32_t input, int32_t goldenInvSqrt, int goldenShift) {
        int32_t q;
        int s;
        EXPECT_TRUE(GetInvSqrtQuantizedMultiplierExp(input, 1, &q, &s));
        EXPECT_EQ(q, goldenInvSqrt);
        EXPECT_EQ(s, goldenShift);
    };

    const auto kInt32Max = std::numeric_limits<std::int32_t>::max();
    checkInvSqrtQuantization(0, kInt32Max, 0);
    checkInvSqrtQuantization(1, kInt32Max, 0);
    checkInvSqrtQuantization(2, 1518498372, 0);
    checkInvSqrtQuantization(3, 1239850284, 0);
    checkInvSqrtQuantization(4, 1073741828, 0);
    checkInvSqrtQuantization(100, 214748363, 0);
    checkInvSqrtQuantization(10000, 343597361, 4);
    checkInvSqrtQuantization(1000000, 274877901, 7);
    checkInvSqrtQuantization(100000000, 219902323, 10);
    checkInvSqrtQuantization((1 << 30), 268435457, 12);
    checkInvSqrtQuantization(kInt32Max, 189812531, 12);
}

}  // namespace wrapper
}  // namespace nn
}  // namespace android
