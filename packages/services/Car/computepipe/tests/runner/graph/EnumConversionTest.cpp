/*
 * Copyright 2020 The Android Open Source Project
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
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "prebuilt_interface.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {
namespace {

TEST(EnumConversionTest, StatusToErrorCodeEnums) {
    EXPECT_EQ(static_cast<int>(PrebuiltComputepipeRunner_ErrorCode::SUCCESS),
              static_cast<int>(Status::SUCCESS));
    EXPECT_EQ(static_cast<int>(PrebuiltComputepipeRunner_ErrorCode::INTERNAL_ERROR),
              static_cast<int>(Status::INTERNAL_ERROR));
    EXPECT_EQ(PrebuiltComputepipeRunner_ErrorCode::INVALID_ARGUMENT,
              static_cast<int>(Status::INVALID_ARGUMENT));
    EXPECT_EQ(PrebuiltComputepipeRunner_ErrorCode::ILLEGAL_STATE,
              static_cast<int>(Status::ILLEGAL_STATE));
    EXPECT_EQ(PrebuiltComputepipeRunner_ErrorCode::NO_MEMORY, static_cast<int>(Status::NO_MEMORY));
    EXPECT_EQ(PrebuiltComputepipeRunner_ErrorCode::FATAL_ERROR,
              static_cast<int>(Status::FATAL_ERROR));
    EXPECT_EQ(PrebuiltComputepipeRunner_ErrorCode::ERROR_CODE_MAX,
              static_cast<int>(Status::STATUS_MAX));
}
enum PrebuiltComputepipeRunner_PixelDataFormat {
    RGB = 0,
    RGBA = 1,
    GRAY = 2,
    PIXEL_DATA_FORMAT_MAX = 3,
};
TEST(EnumConversionTest, PixelFormatEnums) {
    EXPECT_EQ(static_cast<int>(PrebuiltComputepipeRunner_PixelDataFormat::RGB),
              static_cast<int>(PixelFormat::RGB));
    EXPECT_EQ(static_cast<int>(PrebuiltComputepipeRunner_PixelDataFormat::RGBA),
              static_cast<int>(PixelFormat::RGBA));
    EXPECT_EQ(PrebuiltComputepipeRunner_PixelDataFormat::GRAY, static_cast<int>(PixelFormat::GRAY));
    EXPECT_EQ(PrebuiltComputepipeRunner_PixelDataFormat::PIXEL_DATA_FORMAT_MAX,
              static_cast<int>(PixelFormat::PIXELFORMAT_MAX));
}

}  // namespace
}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
