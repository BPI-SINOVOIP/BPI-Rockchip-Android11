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
#include <cutils/fs.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>

#include "Hardware.h"

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

using ::testing::Test;
using ::testing::TestParamInfo;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

class HwApiTest : public Test {
  private:
    static constexpr const char *FILE_NAMES[]{
            "device/f0_stored",
            "device/redc_stored",
            "device/q_stored",
            "activate",
            "duration",
            "state",
            "device/cp_trigger_duration",
            "device/cp_trigger_index",
            "device/cp_trigger_queue",
            "device/cp_dig_scale",
            "device/dig_scale",
            "device/asp_enable",
            "device/gpio1_fall_index",
            "device/gpio1_fall_dig_scale",
            "device/gpio1_rise_index",
            "device/gpio1_rise_dig_scale",
            "device/num_waves",
    };

  public:
    void SetUp() override {
        std::string prefix;
        for (auto n : FILE_NAMES) {
            auto name = std::filesystem::path(n);
            auto path = std::filesystem::path(mFilesDir.path) / name;
            fs_mkdirs(path.c_str(), S_IRWXU);
            std::ofstream touch{path};
            mFileMap[name] = path;
        }
        prefix = std::filesystem::path(mFilesDir.path) / "";
        setenv("HWAPI_PATH_PREFIX", prefix.c_str(), true);
        mHwApi = std::make_unique<HwApi>();

        for (auto n : FILE_NAMES) {
            auto name = std::filesystem::path(n);
            auto path = std::filesystem::path(mEmptyDir.path) / name;
        }
        prefix = std::filesystem::path(mEmptyDir.path) / "";
        setenv("HWAPI_PATH_PREFIX", prefix.c_str(), true);
        mNoApi = std::make_unique<HwApi>();
    }

    void TearDown() override { verifyContents(); }

    static auto ParamNameFixup(std::string str) {
        std::replace(str.begin(), str.end(), '/', '_');
        return str;
    }

  protected:
    // Set expected file content for a test.
    template <typename T>
    void expectContent(const std::string &name, const T &value) {
        mExpectedContent[name] << value << std::endl;
    }

    // Set actual file content for an input test.
    template <typename T>
    void updateContent(const std::string &name, const T &value) {
        std::ofstream(mFileMap[name]) << value << std::endl;
    }

    template <typename T>
    void expectAndUpdateContent(const std::string &name, const T &value) {
        expectContent(name, value);
        updateContent(name, value);
    }

    // Compare all file contents against expected contents.
    void verifyContents() {
        for (auto &a : mFileMap) {
            std::ifstream file{a.second};
            std::string expect = mExpectedContent[a.first].str();
            std::string actual = std::string(std::istreambuf_iterator<char>(file),
                                             std::istreambuf_iterator<char>());
            EXPECT_EQ(expect, actual) << a.first;
        }
    }

  protected:
    std::unique_ptr<Vibrator::HwApi> mHwApi;
    std::unique_ptr<Vibrator::HwApi> mNoApi;
    std::map<std::string, std::string> mFileMap;
    TemporaryDir mFilesDir;
    TemporaryDir mEmptyDir;
    std::map<std::string, std::stringstream> mExpectedContent;
};

template <typename T>
class HwApiTypedTest : public HwApiTest,
                       public WithParamInterface<std::tuple<std::string, std::function<T>>> {
  public:
    static auto PrintParam(const TestParamInfo<typename HwApiTypedTest::ParamType> &info) {
        return ParamNameFixup(std::get<0>(info.param));
    }
    static auto MakeParam(std::string name, std::function<T> func) {
        return std::make_tuple(name, func);
    }
};

using HasTest = HwApiTypedTest<bool(Vibrator::HwApi &)>;

TEST_P(HasTest, success_returnsTrue) {
    auto param = GetParam();
    auto func = std::get<1>(param);

    EXPECT_TRUE(func(*mHwApi));
}

TEST_P(HasTest, success_returnsFalse) {
    auto param = GetParam();
    auto func = std::get<1>(param);

    EXPECT_FALSE(func(*mNoApi));
}

INSTANTIATE_TEST_CASE_P(
        HwApiTests, HasTest,
        ValuesIn({
                HasTest::MakeParam("device/cp_dig_scale", &Vibrator::HwApi::hasEffectScale),
                HasTest::MakeParam("device/asp_enable", &Vibrator::HwApi::hasAspEnable),
        }),
        HasTest::PrintParam);

using GetBoolTest = HwApiTypedTest<bool(Vibrator::HwApi &, bool *)>;

TEST_P(GetBoolTest, success_returnsTrue) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    bool expect = true;
    bool actual = !expect;

    expectAndUpdateContent(name, "1");

    EXPECT_TRUE(func(*mHwApi, &actual));
    EXPECT_EQ(expect, actual);
}

TEST_P(GetBoolTest, success_returnsFalse) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    bool expect = false;
    bool actual = !expect;

    expectAndUpdateContent(name, "0");

    EXPECT_TRUE(func(*mHwApi, &actual));
    EXPECT_EQ(expect, actual);
}

TEST_P(GetBoolTest, failure) {
    auto param = GetParam();
    auto func = std::get<1>(param);
    bool value;

    EXPECT_FALSE(func(*mNoApi, &value));
}

INSTANTIATE_TEST_CASE_P(HwApiTests, GetBoolTest,
                        ValuesIn({
                                GetBoolTest::MakeParam("device/asp_enable",
                                                       &Vibrator::HwApi::getAspEnable),
                        }),
                        GetBoolTest::PrintParam);

using GetUint32Test = HwApiTypedTest<bool(Vibrator::HwApi &, uint32_t *)>;

TEST_P(GetUint32Test, success) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    uint32_t expect = std::rand();
    uint32_t actual = ~expect;

    expectAndUpdateContent(name, expect);

    EXPECT_TRUE(func(*mHwApi, &actual));
    EXPECT_EQ(expect, actual);
}

TEST_P(GetUint32Test, failure) {
    auto param = GetParam();
    auto func = std::get<1>(param);
    uint32_t value;

    EXPECT_FALSE(func(*mNoApi, &value));
}

INSTANTIATE_TEST_CASE_P(HwApiTests, GetUint32Test,
                        ValuesIn({
                                GetUint32Test::MakeParam("device/num_waves",
                                                         &Vibrator::HwApi::getEffectCount),
                                GetUint32Test::MakeParam("device/cp_trigger_duration",
                                                         &Vibrator::HwApi::getEffectDuration),
                        }),
                        GetUint32Test::PrintParam);

using SetBoolTest = HwApiTypedTest<bool(Vibrator::HwApi &, bool)>;

TEST_P(SetBoolTest, success_returnsTrue) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);

    expectContent(name, "1");

    EXPECT_TRUE(func(*mHwApi, true));
}

TEST_P(SetBoolTest, success_returnsFalse) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);

    expectContent(name, "0");

    EXPECT_TRUE(func(*mHwApi, false));
}

TEST_P(SetBoolTest, failure) {
    auto param = GetParam();
    auto func = std::get<1>(param);

    EXPECT_FALSE(func(*mNoApi, true));
    EXPECT_FALSE(func(*mNoApi, false));
}

INSTANTIATE_TEST_CASE_P(HwApiTests, SetBoolTest,
                        ValuesIn({
                                SetBoolTest::MakeParam("activate", &Vibrator::HwApi::setActivate),
                                SetBoolTest::MakeParam("state", &Vibrator::HwApi::setState),
                                SetBoolTest::MakeParam("device/asp_enable",
                                                       &Vibrator::HwApi::setAspEnable),
                        }),
                        SetBoolTest::PrintParam);

using SetUint32Test = HwApiTypedTest<bool(Vibrator::HwApi &, uint32_t)>;

TEST_P(SetUint32Test, success) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    uint32_t value = std::rand();

    expectContent(name, value);

    EXPECT_TRUE(func(*mHwApi, value));
}

TEST_P(SetUint32Test, failure) {
    auto param = GetParam();
    auto func = std::get<1>(param);
    uint32_t value = std::rand();

    EXPECT_FALSE(func(*mNoApi, value));
}

INSTANTIATE_TEST_CASE_P(
        HwApiTests, SetUint32Test,
        ValuesIn({
                SetUint32Test::MakeParam("device/f0_stored", &Vibrator::HwApi::setF0),
                SetUint32Test::MakeParam("device/redc_stored", &Vibrator::HwApi::setRedc),
                SetUint32Test::MakeParam("device/q_stored", &Vibrator::HwApi::setQ),
                SetUint32Test::MakeParam("duration", &Vibrator::HwApi::setDuration),
                SetUint32Test::MakeParam("device/cp_trigger_index",
                                         &Vibrator::HwApi::setEffectIndex),
                SetUint32Test::MakeParam("device/cp_dig_scale", &Vibrator::HwApi::setEffectScale),
                SetUint32Test::MakeParam("device/dig_scale", &Vibrator::HwApi::setGlobalScale),
                SetUint32Test::MakeParam("device/gpio1_fall_index",
                                         &Vibrator::HwApi::setGpioFallIndex),
                SetUint32Test::MakeParam("device/gpio1_fall_dig_scale",
                                         &Vibrator::HwApi::setGpioFallScale),
                SetUint32Test::MakeParam("device/gpio1_rise_index",
                                         &Vibrator::HwApi::setGpioRiseIndex),
                SetUint32Test::MakeParam("device/gpio1_rise_dig_scale",
                                         &Vibrator::HwApi::setGpioRiseScale),
        }),
        SetUint32Test::PrintParam);

using SetStringTest = HwApiTypedTest<bool(Vibrator::HwApi &, std::string)>;

TEST_P(SetStringTest, success) {
    auto param = GetParam();
    auto name = std::get<0>(param);
    auto func = std::get<1>(param);
    std::string value = TemporaryFile().path;

    expectContent(name, value);

    EXPECT_TRUE(func(*mHwApi, value));
}

TEST_P(SetStringTest, failure) {
    auto param = GetParam();
    auto func = std::get<1>(param);
    std::string value = TemporaryFile().path;

    EXPECT_FALSE(func(*mNoApi, value));
}

INSTANTIATE_TEST_CASE_P(HwApiTests, SetStringTest,
                        ValuesIn({
                                SetStringTest::MakeParam("device/cp_trigger_queue",
                                                         &Vibrator::HwApi::setEffectQueue),
                        }),
                        SetStringTest::PrintParam);

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
