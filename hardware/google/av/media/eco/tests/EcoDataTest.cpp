/*
 * Copyright (C) 2019 The Android Open Source Project
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

// Unit Test for EcoData.

//#define LOG_NDEBUG 0
#define LOG_TAG "ECODataTest"

#include <android-base/unique_fd.h>
#include <binder/Parcel.h>
#include <binder/Parcelable.h>
#include <cutils/ashmem.h>
#include <gtest/gtest.h>
#include <math.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <utils/Log.h>

#include "eco/ECOData.h"
#include "eco/ECODataKey.h"

namespace android {
namespace media {
namespace eco {

TEST(EcoDataTest, TestConstructor1) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>();
    EXPECT_EQ(data->getDataType(), ECOData::DATA_TYPE_UNKNOWN);
    EXPECT_EQ(data->getDataTimeUs(), -1);
}

TEST(EcoDataTest, TestConstructor2) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS);
    EXPECT_EQ(data->getDataType(), ECOData::DATA_TYPE_STATS);
    EXPECT_EQ(data->getDataTimeUs(), -1);
}

TEST(EcoDataTest, TestConstructor3) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);
    EXPECT_EQ(data->getDataType(), ECOData::DATA_TYPE_STATS);
    EXPECT_EQ(data->getDataTimeUs(), 1000);
}

TEST(EcoDataTest, TestNormalSetAndFindString) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    data->setString(ENCODER_TYPE, "avc");
    std::string testValue;
    EXPECT_TRUE(data->findString(ENCODER_TYPE, &testValue) == ECODataStatus::OK);
    EXPECT_EQ(testValue, "avc");

    // Override existing key.
    data->setString(ENCODER_TYPE, "hevc");
    EXPECT_EQ(data->findString(ENCODER_TYPE, &testValue), ECODataStatus::OK);
    EXPECT_EQ(testValue, "hevc");
}

TEST(EcoDataTest, TestSetAndFindMultipleString) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    std::unordered_map<std::string, std::string> inputEntries = {
            {"name1", "avc"},  {"name2", "avc2"},   {"name3", "avc3"},   {"name4", "avc4"},
            {"name5", "avc5"}, {"name6", "avc6"},   {"name7", "avc7"},   {"name8", "avc8"},
            {"name9", "avc9"}, {"name10", "avc10"}, {"name11", "avc11"}, {"name12", "avc12"}};
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        data->setString(it->first, it->second);
    }

    // Checks if the string exist in the ECOData.
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        std::string testValue;
        EXPECT_TRUE(data->findString(it->first, &testValue) == ECODataStatus::OK);
        EXPECT_EQ(testValue, it->second);
    }
}

TEST(EcoDataTest, TestSetAndFindInvalidString) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    // Test read to null ptr and expect failure
    EXPECT_TRUE(data->findString("encoder-name", nullptr) != ECODataStatus::OK);

    // Test find non-existing key and expect failure.
    std::string testValue;
    EXPECT_TRUE(data->findString("encoder-name", &testValue) != ECODataStatus::OK);

    // Test set empty key and expect failure
    EXPECT_TRUE(data->setString("", "avc") != ECODataStatus::OK);

    // Test read empty key and expect failure
    EXPECT_TRUE(data->findString("", &testValue) != ECODataStatus::OK);
}

TEST(EcoDataTest, TestNormalSetAndFindInt32) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    data->setInt32(ENCODER_TARGET_BITRATE_BPS, 2000000);
    int32_t testValue;
    EXPECT_TRUE(data->findInt32(ENCODER_TARGET_BITRATE_BPS, &testValue) == ECODataStatus::OK);
    EXPECT_EQ(testValue, 2000000);

    // Override existing key.
    data->setInt32(ENCODER_TARGET_BITRATE_BPS, 2200000);
    EXPECT_EQ(data->findInt32(ENCODER_TARGET_BITRATE_BPS, &testValue), ECODataStatus::OK);
    EXPECT_EQ(testValue, 2200000);
}

TEST(EcoDataTest, TestSetAndFindMultipleInt32) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    std::unordered_map<std::string, int32_t> inputEntries = {
            {"name1", 100}, {"name2", 200},    {"name3", 300},     {"name4", 400},
            {"name5", 500}, {"name6", 600},    {"name7", 700},     {"name8", 800},
            {"name9", 900}, {"name10", 10000}, {"name11", 110000}, {"name12", 120000}};
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        data->setInt32(it->first, it->second);
    }

    // Checks if the string exist in the ECOData.
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        int32_t testValue;
        EXPECT_TRUE(data->findInt32(it->first, &testValue) == ECODataStatus::OK);
        EXPECT_EQ(testValue, it->second);
    }
}

TEST(EcoDataTest, TestSetAndFindInvalidInt32) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    // Test read to null ptr and expect failure
    EXPECT_TRUE(data->findInt32("encoder-name", nullptr) != ECODataStatus::OK);

    // Test find non-existing key and expect failure.
    int32_t testValue;
    EXPECT_TRUE(data->findInt32("encoder-name", &testValue) != ECODataStatus::OK);

    // Test set empty key and expect failure
    EXPECT_TRUE(data->setInt32("", 1000) != ECODataStatus::OK);

    // Test read empty key and expect failure
    EXPECT_TRUE(data->findInt32("", &testValue) != ECODataStatus::OK);
}

TEST(EcoDataTest, TestNormalSetAndFindInt64) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    data->setInt64(ENCODER_TARGET_BITRATE_BPS, 2000000);
    int64_t testValue;
    EXPECT_TRUE(data->findInt64(ENCODER_TARGET_BITRATE_BPS, &testValue) == ECODataStatus::OK);
    EXPECT_EQ(testValue, 2000000);

    // Override existing key.
    data->setInt64(ENCODER_TARGET_BITRATE_BPS, 2200000);
    EXPECT_EQ(data->findInt64(ENCODER_TARGET_BITRATE_BPS, &testValue), ECODataStatus::OK);
    EXPECT_EQ(testValue, 2200000);
}

TEST(EcoDataTest, TestNormalSetAndFindMultipleInt64) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    std::unordered_map<std::string, int64_t> inputEntries = {
            {"name1", 100}, {"name2", 200},    {"name3", 300},     {"name4", 400},
            {"name5", 500}, {"name6", 600},    {"name7", 700},     {"name8", 800},
            {"name9", 900}, {"name10", 10000}, {"name11", 110000}, {"name12", 120000}};
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        data->setInt64(it->first, it->second);
    }

    // Checks if the string exist in the ECOData.
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        int64_t testValue;
        EXPECT_TRUE(data->findInt64(it->first, &testValue) == ECODataStatus::OK);
        EXPECT_EQ(testValue, it->second);
    }
}

TEST(EcoDataTest, TestSetAndFindInvalidInt64) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    // Test read to null ptr and expect failure
    EXPECT_TRUE(data->findInt64("encoder-name", nullptr) != ECODataStatus::OK);

    // Test find non-existing key and expect failure.
    int64_t testValue;
    EXPECT_TRUE(data->findInt64("encoder-name", &testValue) != ECODataStatus::OK);

    // Test set empty key and expect failure
    EXPECT_TRUE(data->setInt64("", 1000) != ECODataStatus::OK);

    // Test read empty key and expect failure
    EXPECT_TRUE(data->findInt64("", &testValue) != ECODataStatus::OK);
}

TEST(EcoDataTest, TestNormalSetAndFindFloat) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    data->setFloat(ENCODER_TARGET_BITRATE_BPS, 2000000.0);
    float testValue;
    EXPECT_TRUE(data->findFloat(ENCODER_TARGET_BITRATE_BPS, &testValue) == ECODataStatus::OK);
    EXPECT_FLOAT_EQ(testValue, 2000000.0);

    // Override existing key.
    data->setFloat(ENCODER_TARGET_BITRATE_BPS, 2200000.0);
    EXPECT_TRUE(data->findFloat(ENCODER_TARGET_BITRATE_BPS, &testValue) == ECODataStatus::OK);
    EXPECT_FLOAT_EQ(testValue, 2200000.0);
}

TEST(EcoDataTest, TestNormalSetAndFindMultipleFloat) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    std::unordered_map<std::string, float> inputEntries = {
            {"name1", 100.0}, {"name2", 200.0},    {"name3", 300.0},     {"name4", 400.0},
            {"name5", 500.0}, {"name6", 600.0},    {"name7", 700.0},     {"name8", 800.0},
            {"name9", 900.0}, {"name10", 10000.0}, {"name11", 110000.0}, {"name12", 120000.0}};
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        data->setFloat(it->first, it->second);
    }

    // Checks if the string exist in the ECOData.
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        float testValue;
        EXPECT_TRUE(data->findFloat(it->first, &testValue) == ECODataStatus::OK);
        EXPECT_FLOAT_EQ(testValue, it->second);
    }
}

TEST(EcoDataTest, TestSetAndFindInvalidFloat) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    // Test read to null ptr and expect failure
    EXPECT_TRUE(data->findFloat("encoder-name", nullptr) != ECODataStatus::OK);

    // Test find non-existing key and expect failure.
    float testValue;
    EXPECT_TRUE(data->findFloat("encoder-name", &testValue) != ECODataStatus::OK);

    // Test set empty key and expect failure
    EXPECT_TRUE(data->setFloat("", 1000.0) != ECODataStatus::OK);

    // Test read empty key and expect failure
    EXPECT_TRUE(data->findFloat("", &testValue) != ECODataStatus::OK);
}

TEST(EcoDataTest, TestNormalSetAndFindMixedDataType) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    std::unordered_map<std::string, ECOData::ECODataValueType> inputEntries = {
            {"name1", "google-encoder"}, {"name2", "avc"}, {"profile", 1}, {"level", 2},
            {"framerate", 4.1},          {"kfi", 30}};
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        data->set(it->first, it->second);
    }

    // Checks if the string exist in the ECOData.
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        ECOData::ECODataValueType testValue;
        EXPECT_TRUE(data->find(it->first, &testValue) == ECODataStatus::OK);
        EXPECT_EQ(testValue, it->second);
    }
}

TEST(EcoDataTest, TestSetAndFindInvalidDataType) {
    std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);

    // Test read to null ptr and expect failure
    EXPECT_TRUE(data->find("encoder-name", nullptr) != ECODataStatus::OK);

    // Test find non-existing key and expect failure.
    ECOData::ECODataValueType testValue;
    EXPECT_TRUE(data->find("encoder-name2", &testValue) != ECODataStatus::OK);

    // Test set empty key and expect failure
    EXPECT_TRUE(data->set("", 1000) != ECODataStatus::OK);

    // Test read empty key and expect failure
    EXPECT_TRUE(data->find("", &testValue) != ECODataStatus::OK);
}

TEST(EcoDataTest, TestNormalWriteReadParcel) {
    constexpr int32_t kDataType = ECOData::DATA_TYPE_STATS;
    constexpr int64_t kDataTimeUs = 1000;

    std::unique_ptr<ECOData> sourceData = std::make_unique<ECOData>(kDataType, kDataTimeUs);

    std::unordered_map<std::string, ECOData::ECODataValueType> inputEntries = {
            {"name1", "google-encoder"}, {"name2", "avc"}, {"profile", 1}, {"level", 2},
            {"framerate", 4.1},          {"kfi", 30}};
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        sourceData->set(it->first, it->second);
    }

    std::unique_ptr<Parcel> parcel = std::make_unique<Parcel>();
    EXPECT_TRUE(sourceData->writeToParcel(parcel.get()) == NO_ERROR);

    // Rewind the data position of the parcel for this test. Otherwise, the following read will not
    // start from the beginning.
    parcel->setDataPosition(0);

    // Reads the parcel back into a new ECOData
    std::unique_ptr<ECOData> dstData = std::make_unique<ECOData>();
    EXPECT_TRUE(dstData->readFromParcel(parcel.get()) == NO_ERROR);

    // Checks the data type, time and number of entries.
    EXPECT_EQ(sourceData->getNumOfEntries(), dstData->getNumOfEntries());
    EXPECT_EQ(dstData->getDataType(), kDataType);
    EXPECT_EQ(dstData->getDataTimeUs(), kDataTimeUs);

    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        ECOData::ECODataValueType testValue;
        EXPECT_TRUE(dstData->find(it->first, &testValue) == ECODataStatus::OK);
        EXPECT_EQ(testValue, it->second);
    }
}

TEST(EcoDataTest, TestWriteInvalidParcel) {
    constexpr int32_t kDataType = ECOData::DATA_TYPE_STATS;
    constexpr int64_t kDataTimeUs = 1000;

    std::unique_ptr<ECOData> sourceData = std::make_unique<ECOData>(kDataType, kDataTimeUs);

    std::unique_ptr<Parcel> parcel = std::make_unique<Parcel>();
    EXPECT_TRUE(sourceData->writeToParcel(nullptr) != NO_ERROR);
}

TEST(EcoDataTest, TestReadInvalidParcel) {
    constexpr int32_t kDataType = ECOData::DATA_TYPE_STATS;
    constexpr int64_t kDataTimeUs = 1000;

    std::unique_ptr<ECOData> sourceData = std::make_unique<ECOData>(kDataType, kDataTimeUs);

    std::unordered_map<std::string, ECOData::ECODataValueType> inputEntries = {
            {"name1", "google-encoder"}, {"name2", "avc"}, {"profile", 1}, {"level", 2},
            {"framerate", 4.1},          {"kfi", 30}};
    for (auto it = inputEntries.begin(); it != inputEntries.end(); ++it) {
        sourceData->set(it->first, it->second);
    }

    std::unique_ptr<Parcel> parcel = std::make_unique<Parcel>();
    EXPECT_TRUE(sourceData->writeToParcel(parcel.get()) == NO_ERROR);

    // Corrupt the parcel by write random data to the beginning.
    parcel->setDataPosition(4);
    parcel->writeCString("invalid-data");

    parcel->setDataPosition(0);

    // Reads the parcel back into a new ECOData
    std::unique_ptr<ECOData> dstData = std::make_unique<ECOData>();
    EXPECT_TRUE(dstData->readFromParcel(parcel.get()) != NO_ERROR);
}

}  // namespace eco
}  // namespace media
}  // namespace android