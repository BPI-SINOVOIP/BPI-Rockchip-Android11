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

#define LOG_NDEBUG 0
#define LOG_TAG "MediaMetricsTest"

#include <gtest/gtest.h>
#include <utils/Log.h>
#include <media/MediaMetrics.h>

//-----------------------------------------------------------------
class MediaMetricsTest : public ::testing::Test {

protected:
    MediaMetricsTest() { }

    virtual ~MediaMetricsTest() { }

    /* Test setup*/
    virtual void SetUp() {
        handle_ = mediametrics_create("foo");
    }

    virtual void TearDown() {
        mediametrics_delete(handle_);
    }

    mediametrics_handle_t handle_;
};

//-------------------------------------------------------------------------------------------------
TEST_F(MediaMetricsTest, testCreateDelete) {
    // Will be done with SetUp and TearDown.
}

TEST_F(MediaMetricsTest, testInt32) {
    mediametrics_setInt32(handle_, "attr1", 100);
    int32_t value;
    EXPECT_TRUE(mediametrics_getInt32(handle_, "attr1",  &value));
    EXPECT_EQ(100, value);

    mediametrics_addInt32(handle_, "attr1", 50);
    EXPECT_TRUE(mediametrics_getInt32(handle_, "attr1",  &value));
    EXPECT_EQ(150, value);
}

TEST_F(MediaMetricsTest, testInt64) {
    mediametrics_setInt64(handle_, "attr2", 1e10);
    int64_t value;
    EXPECT_TRUE(mediametrics_getInt64(handle_, "attr2",  &value));
    EXPECT_EQ(1e10, value);

    mediametrics_addInt64(handle_, "attr2", 50);
    EXPECT_TRUE(mediametrics_getInt64(handle_, "attr2",  &value));
    EXPECT_EQ(1e10 + 50, value);
}

TEST_F(MediaMetricsTest, testDouble) {
    mediametrics_setDouble(handle_, "attr3", 100.0);
    double value;
    EXPECT_TRUE(mediametrics_getDouble(handle_, "attr3",  &value));
    EXPECT_DOUBLE_EQ(100.0, value);

    mediametrics_addDouble(handle_, "attr3", 50.0);
    EXPECT_TRUE(mediametrics_getDouble(handle_, "attr3",  &value));
    EXPECT_DOUBLE_EQ(150.0, value);
}

TEST_F(MediaMetricsTest, testRate) {
    mediametrics_setRate(handle_, "attr4", 30, 1000);
    int64_t count;
    int64_t duration;
    double rate;
    EXPECT_TRUE(mediametrics_getRate(handle_, "attr4",  &count, &duration, &rate));
    EXPECT_EQ(30, count);
    EXPECT_EQ(1000, duration);
    EXPECT_DOUBLE_EQ(30/1000.0, rate);

    mediametrics_addRate(handle_, "attr4", 29, 1000);
    EXPECT_TRUE(mediametrics_getRate(handle_, "attr4",  &count, &duration, &rate));
    EXPECT_EQ(59, count);
    EXPECT_EQ(2000, duration);
    EXPECT_DOUBLE_EQ(59/2000.0, rate);
}

TEST_F(MediaMetricsTest, testCString) {
    mediametrics_setCString(handle_, "attr5", "test_string");
    char *value = nullptr;
    EXPECT_TRUE(mediametrics_getCString(handle_, "attr5",  &value));
    EXPECT_STREQ("test_string", value);
    mediametrics_freeCString(value);
}

TEST_F(MediaMetricsTest, testCount) {
    mediametrics_setInt32(handle_, "attr1", 100);
    EXPECT_EQ(1, mediametrics_count(handle_));
    mediametrics_setInt32(handle_, "attr2", 200);
    mediametrics_setInt32(handle_, "attr3", 300);
    EXPECT_EQ(3, mediametrics_count(handle_));
}

TEST_F(MediaMetricsTest, testReadable) {
    mediametrics_setInt32(handle_, "attr1", 1);
    mediametrics_setInt64(handle_, "attr2", 2);
    mediametrics_setDouble(handle_, "attr3", 3.0);
    mediametrics_setRate(handle_, "attr4", 4, 5);
    mediametrics_setCString(handle_, "attr5", "test_string");

    EXPECT_TRUE(strlen(mediametrics_readable(handle_)) > 0);
}

TEST_F(MediaMetricsTest, testSelfRecord) {
    mediametrics_setInt32(handle_, "attr1", 100);
    mediametrics_setInt64(handle_, "attr2", 1e10);
    mediametrics_setDouble(handle_, "attr3", 100.0);
    mediametrics_setRate(handle_, "attr4", 30, 1000);
    mediametrics_setCString(handle_, "attr5", "test_string");
    mediametrics_setUid(handle_, 10000);

    EXPECT_TRUE(mediametrics_selfRecord(handle_));
}

TEST_F(MediaMetricsTest, testIsEnabled) {
    EXPECT_TRUE(mediametrics_isEnabled());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

