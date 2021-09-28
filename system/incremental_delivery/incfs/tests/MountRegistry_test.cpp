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

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <gtest/gtest.h>
#include <sys/select.h>
#include <unistd.h>

#include <optional>
#include <thread>

#include "MountRegistry.h"
#include "path.h"

using namespace android::incfs;
using namespace std::literals;

class MountRegistryTest : public ::testing::Test {
protected:
    virtual void SetUp() {}
    virtual void TearDown() {}

    MountRegistry::Mounts mounts_;

    MountRegistry::Mounts& r() { return mounts_; }
};

TEST_F(MountRegistryTest, RootForRoot) {
    r().addRoot("/root", "/backing");
    ASSERT_STREQ("/root", r().rootFor("/root").data());
    ASSERT_STREQ("/root", r().rootFor("/root/1").data());
    ASSERT_STREQ("/root", r().rootFor("/root/1/2").data());
    ASSERT_STREQ(nullptr, r().rootFor("/root1/1/2").data());
    ASSERT_STREQ(nullptr, r().rootFor("/1/root").data());
    ASSERT_STREQ(nullptr, r().rootFor("root").data());
}

TEST_F(MountRegistryTest, OneBind) {
    r().addRoot("/root", "/backing");
    r().addBind("/root/1", "/bind");
    ASSERT_STREQ("/root", r().rootFor("/root").data());
    ASSERT_STREQ("/root", r().rootFor("/bind").data());
    ASSERT_STREQ("/root", r().rootFor("/bind/1").data());
    ASSERT_STREQ("/root", r().rootFor("/root/1").data());
    ASSERT_STREQ(nullptr, r().rootFor("/1/bind").data());
    ASSERT_STREQ(nullptr, r().rootFor("bind").data());
    ASSERT_STREQ(nullptr, r().rootFor("/bind1").data());
    ASSERT_STREQ(nullptr, r().rootFor("/.bind").data());
}

TEST_F(MountRegistryTest, MultiBind) {
    r().addRoot("/root", "/backing");
    r().addBind("/root/1", "/bind");
    r().addBind("/root/2/3", "/bind2");
    r().addBind("/root/2/3", "/other/bind");
    ASSERT_STREQ("/root", r().rootFor("/root").data());
    ASSERT_STREQ("/root", r().rootFor("/bind").data());
    ASSERT_STREQ("/root", r().rootFor("/bind2").data());
    ASSERT_STREQ("/root", r().rootFor("/other/bind/dir").data());
    ASSERT_EQ(std::pair("/root"sv, ""s), r().rootAndSubpathFor("/root"));
    ASSERT_EQ(std::pair("/root"sv, "1"s), r().rootAndSubpathFor("/bind"));
    ASSERT_EQ(std::pair("/root"sv, "2/3"s), r().rootAndSubpathFor("/bind2"));
    ASSERT_EQ(std::pair("/root"sv, "2/3/blah"s), r().rootAndSubpathFor("/bind2/blah"));
    ASSERT_EQ(std::pair("/root"sv, "2/3/blah"s), r().rootAndSubpathFor("/other/bind/blah"));
}
