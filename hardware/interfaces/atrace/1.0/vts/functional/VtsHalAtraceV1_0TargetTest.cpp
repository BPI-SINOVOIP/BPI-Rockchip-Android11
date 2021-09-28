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

#include <unordered_set>

#include <android/hardware/atrace/1.0/IAtraceDevice.h>

#include <gtest/gtest.h>
#include <hidl/GtestPrinter.h>
#include <hidl/ServiceManagement.h>

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::atrace::V1_0::IAtraceDevice;
using ::android::hardware::atrace::V1_0::Status;

/**
 * There is no expected behaviour that can be tested so these tests check the
 * HAL doesn't crash with different execution orders.
 */
struct AtraceHidlTest : public ::testing::TestWithParam<std::string> {
    virtual void SetUp() override {
        atrace = IAtraceDevice::getService(GetParam());
        ASSERT_NE(atrace, nullptr);
    }

    sp<IAtraceDevice> atrace;
};

hidl_vec<hidl_string> getVendorCategoryName(sp<IAtraceDevice> atrace) {
    std::unordered_set<std::string> categories_set;
    Return<void> ret = atrace->listCategories([&](const auto& list) {
        for (const auto& category : list) {
            std::string name = category.name;
            if (categories_set.count(name)) {
                ADD_FAILURE() << "Duplicate category: " << name;
            } else {
                categories_set.emplace(name);
            }
        }
    });
    if (!ret.isOk()) {
        ADD_FAILURE();
    }
    hidl_vec<hidl_string> categories;
    categories.resize(categories_set.size());
    std::size_t i = 0;
    for (auto& c : categories_set) {
        categories[i++] = c;
    }
    return categories;
}

/* list categories from vendors. */
TEST_P(AtraceHidlTest, listCategories) {
    hidl_vec<hidl_string> vnd_categories = getVendorCategoryName(atrace);
    EXPECT_NE(0, vnd_categories.size());
}

/* enable categories. */
TEST_P(AtraceHidlTest, enableCategories) {
    hidl_vec<hidl_string> vnd_categories = getVendorCategoryName(atrace);
    // empty Category with ERROR_INVALID_ARGUMENT
    hidl_vec<hidl_string> empty_categories;
    auto ret = atrace->enableCategories(empty_categories);
    ASSERT_TRUE(ret.isOk());
    EXPECT_EQ(Status::ERROR_INVALID_ARGUMENT, ret);
    // non-empty categories SUCCESS
    ret = atrace->enableCategories(vnd_categories);
    ASSERT_TRUE(ret.isOk());
    EXPECT_EQ(Status::SUCCESS, ret);
}

/* enable categories. */
TEST_P(AtraceHidlTest, disableAllCategories) {
    auto ret = atrace->disableAllCategories();
    ASSERT_TRUE(ret.isOk());
    EXPECT_EQ(Status::SUCCESS, ret);
}

INSTANTIATE_TEST_SUITE_P(
        PerInstance, AtraceHidlTest,
        testing::ValuesIn(android::hardware::getAllHalInstanceNames(IAtraceDevice::descriptor)),
        android::hardware::PrintInstanceNameToString);
