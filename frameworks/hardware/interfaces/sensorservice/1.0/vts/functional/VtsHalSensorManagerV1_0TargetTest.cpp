/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "sensor_manager_hidl_hal_test"
#include <android-base/logging.h>

#include <sys/mman.h>

#include <chrono>
#include <thread>

#include <android-base/result.h>
#include <android/frameworks/sensorservice/1.0/ISensorManager.h>
#include <android/frameworks/sensorservice/1.0/types.h>
#include <android/hardware/sensors/1.0/types.h>
#include <android/hidl/allocator/1.0/IAllocator.h>
#include <android/sensor.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <hidl/GtestPrinter.h>
#include <hidl/ServiceManagement.h>
#include <sensors/convert.h>

using ::android::sp;
using ::android::frameworks::sensorservice::V1_0::ISensorManager;
using ::android::frameworks::sensorservice::V1_0::Result;
using ::android::hardware::hidl_memory;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::sensors::V1_0::Event;
using ::android::hardware::sensors::V1_0::RateLevel;
using ::android::hardware::sensors::V1_0::SensorFlagBits;
using ::android::hardware::sensors::V1_0::SensorFlagShift;
using ::android::hardware::sensors::V1_0::SensorInfo;
using ::android::hardware::sensors::V1_0::SensorsEventFormatOffset;
using ::android::hardware::sensors::V1_0::SensorType;
using ::android::hardware::sensors::V1_0::toString;
using ::android::hidl::allocator::V1_0::IAllocator;
using ::testing::Contains;

template <typename T>
static inline ::testing::AssertionResult isOk(const Return<T> &ret) {
  return (ret.isOk()
      ? ::testing::AssertionSuccess()
      : ::testing::AssertionFailure()) << ret.description();
}

template <>
__attribute__((__unused__))
inline ::testing::AssertionResult isOk(const Return<Result> &ret) {
  return ((ret.isOk() && ret == Result::OK)
      ? ::testing::AssertionSuccess()
      : ::testing::AssertionFailure())
      << ret.description() << ", "
      << (ret.isOk() ? toString(static_cast<Result>(ret)) : "");
}

static inline ::testing::AssertionResult isOk(Result result) {
  using ::android::frameworks::sensorservice::V1_0::toString;
  return (result == Result::OK
      ? ::testing::AssertionSuccess()
      : ::testing::AssertionFailure()) << toString(result);
}

template<typename I, typename F>
static ::testing::AssertionResult isIncreasing(I begin, I end, F getField) {
  typename std::iterator_traits<I>::pointer lastValue = nullptr;
  I iter;
  size_t pos;
  for (iter = begin, pos = 0; iter != end; ++iter, ++pos) {
    if (iter == begin) {
      lastValue = &(*iter);
      continue;
    }
    if (getField(*iter) < getField(*lastValue)) {
      return ::testing::AssertionFailure() << "Not an increasing sequence, pos = "
          << pos << ", " << getField(*iter) << " < " << getField(*lastValue);
    }
  }
  return ::testing::AssertionSuccess();
}

#define EXPECT_OK(__ret__) EXPECT_TRUE(isOk(__ret__))
#define ASSERT_OK(__ret__) ASSERT_TRUE(isOk(__ret__))

class SensorManagerTest : public ::testing::TestWithParam<std::string> {
 public:
  virtual void SetUp() override {
    manager_ = ISensorManager::getService(GetParam());
    ASSERT_NE(manager_, nullptr);
    ashmem_ = IAllocator::getService("ashmem");
    ASSERT_NE(ashmem_, nullptr);
  }

  // Call getSensorList. Filter result based on |pred| if it is provided.
  android::base::Result<std::vector<SensorInfo>> GetSensorList(
      const std::function<bool(SensorInfo)> &pred = nullptr) {
    Result out_result = Result::INVALID_OPERATION;
    std::vector<SensorInfo> out_info;

    auto ret = manager_->getSensorList([&](const auto &list, auto result) {
      out_result = result;
      if (result == Result::OK) {
        for (const auto &info : list) {
          if (!pred || pred(info)) {
            out_info.push_back(info);
          }
        }
      }
    });
    if (!ret.isOk()) {
      return android::base::Error() << ret.description();
    }
    if (out_result != Result::OK) {
      return android::base::Error() << "getSensorList returns " << toString(out_result);
    }
    return out_info;
  }

  sp<ISensorManager> manager_;
  sp<IAllocator> ashmem_;
};

using map_region = std::unique_ptr<void, std::function<void(void*)>>;

map_region map(const hidl_memory &mem) {
  if (mem.handle() == nullptr || mem.handle()->numFds != 1) {
    return nullptr;
  }
  size_t size = mem.size();
  void *buf = mmap(nullptr, size, PROT_READ, MAP_SHARED, mem.handle()->data[0], 0);
  return map_region{buf, [size](void *localBuf) {
    munmap(localBuf, size);
  }};
}

TEST_P(SensorManagerTest, List) {
  ASSERT_RESULT_OK(GetSensorList());
}

TEST_P(SensorManagerTest, Ashmem) {
  auto ashmem_sensors = GetSensorList(
      [](const auto &info) { return info.flags & SensorFlagBits::DIRECT_CHANNEL_ASHMEM; });
  ASSERT_RESULT_OK(ashmem_sensors);
  if (ashmem_sensors->empty()) {
    GTEST_SKIP() << "DIRECT_CHANNEL_ASHMEM not supported by HAL, skipping";
  }
  auto testOne = [this](uint64_t memSize, uint64_t intendedSize,
        ISensorManager::createAshmemDirectChannel_cb callback) {
    ASSERT_OK(ashmem_->allocate(memSize, [&](bool success, const auto &mem) {
      ASSERT_TRUE(success);
      ASSERT_NE(mem.handle(), nullptr);
      ASSERT_OK(manager_->createAshmemDirectChannel(mem, intendedSize, callback));
    }));
  };

  testOne(16, 16, [](const auto &chan, Result result) {
    EXPECT_EQ(result, Result::BAD_VALUE) << "unexpected result when memory size is too small";
    EXPECT_EQ(chan, nullptr);
  });

  testOne(1024, 1024, [](const auto &chan, Result result) {
    EXPECT_OK(result);
    EXPECT_NE(chan, nullptr);
  });

  testOne(1024, 2048, [](const auto &chan, Result result) {
    EXPECT_EQ(result, Result::BAD_VALUE) << "unexpected result when intended size is too big";
    EXPECT_EQ(chan, nullptr);
  });

  testOne(1024, 16, [](const auto &chan, Result result) {
    EXPECT_EQ(result, Result::BAD_VALUE) << "unexpected result when intended size is too small";
    EXPECT_EQ(chan, nullptr);
  });
}

static std::vector<Event> parseEvents(uint8_t *buf, size_t memSize) {
  using O = SensorsEventFormatOffset;
  using ::android::hardware::sensors::V1_0::implementation::convertFromSensorEvent;
  size_t offset = 0;
  int64_t lastCounter = -1;
  std::vector<Event> events;
  Event event;

  while(offset + (size_t)O::TOTAL_LENGTH <= memSize) {
    uint8_t *start = buf + offset;
    int64_t atomicCounter = *reinterpret_cast<uint32_t *>(start + (size_t)O::ATOMIC_COUNTER);
    if (atomicCounter <= lastCounter) {
      break;
    }
    int32_t size = *reinterpret_cast<int32_t *>(start + (size_t)O::SIZE_FIELD);
    if (size != (size_t)O::TOTAL_LENGTH) {
      // unknown error, events parsed may be wrong, remove all
      events.clear();
      break;
    }

    convertFromSensorEvent(*reinterpret_cast<const sensors_event_t *>(start), &event);
    events.push_back(event);
    lastCounter = atomicCounter;
    offset += (size_t)O::TOTAL_LENGTH;
  }
  return events;
}

TEST_P(SensorManagerTest, GetDefaultAccelerometer) {
  auto accelerometer_ashmem_sensors =
      GetSensorList([](const auto &info) { return info.type == SensorType::ACCELEROMETER; });
  ASSERT_RESULT_OK(accelerometer_ashmem_sensors);

  ASSERT_OK(
      manager_->getDefaultSensor(SensorType::ACCELEROMETER, [&](const auto &info, auto result) {
        if (accelerometer_ashmem_sensors->empty()) {
          ASSERT_EQ(Result::NOT_EXIST, result);
        } else {
          ASSERT_OK(result);
          ASSERT_THAT(*accelerometer_ashmem_sensors, Contains(info));
        }
      }));
}

TEST_P(SensorManagerTest, Accelerometer) {
  using std::literals::chrono_literals::operator""ms;
  using ::android::hardware::sensors::V1_0::implementation::convertFromRateLevel;

  auto accelerometer_ashmem_sensors = GetSensorList([](const auto &info) {
    if (info.type != SensorType::ACCELEROMETER) return false;
    if (!(info.flags & SensorFlagBits::DIRECT_CHANNEL_ASHMEM)) return false;
    int maxLevel =
        (info.flags & SensorFlagBits::MASK_DIRECT_REPORT) >> (int)SensorFlagShift::DIRECT_REPORT;
    return maxLevel >= convertFromRateLevel(RateLevel::FAST);
  });
  ASSERT_RESULT_OK(accelerometer_ashmem_sensors);

  if (accelerometer_ashmem_sensors->empty()) {
    GTEST_SKIP() << "No accelerometer sensor that supports DIRECT_CHANNEL_ASHMEM and fast report "
                 << "rate, skipping";
  }

  for (const auto &info : *accelerometer_ashmem_sensors) {
    int32_t handle = info.sensorHandle;
    const size_t memSize = (size_t)SensorsEventFormatOffset::TOTAL_LENGTH * 300;
    ASSERT_OK(ashmem_->allocate(memSize, [&](bool success, const auto &mem) {
      ASSERT_TRUE(success);
      map_region buf = map(mem);
      ASSERT_NE(buf, nullptr);
      ASSERT_OK(
          manager_->createAshmemDirectChannel(mem, memSize, [&](const auto &chan, Result result) {
            ASSERT_OK(result);
            ASSERT_NE(chan, nullptr);

            int32_t returnedToken;
            ASSERT_OK(chan->configure(handle, RateLevel::FAST, [&](auto token, auto res) {
              ASSERT_OK(res);
              ASSERT_GT(token, 0);
              returnedToken = token;
            }));  // ~200Hz
            std::this_thread::sleep_for(500ms);
            ASSERT_OK(chan->configure(handle, RateLevel::STOP, [](auto token, auto res) {
              ASSERT_OK(res);
              ASSERT_EQ(token, 0);
            }));

            auto events = parseEvents(static_cast<uint8_t *>(buf.get()), memSize);

            EXPECT_TRUE(isIncreasing(events.begin(), events.end(), [](const auto &event) {
              return event.timestamp;
            })) << "timestamp is not monotonically increasing";
            for (const auto &event : events) {
              EXPECT_EQ(returnedToken, event.sensorHandle)
                  << "configure token and sensor handle don't match.";
            }
          }));
    }));
  }
}

INSTANTIATE_TEST_SUITE_P(
        PerInstance, SensorManagerTest,
        testing::ValuesIn(android::hardware::getAllHalInstanceNames(ISensorManager::descriptor)),
        android::hardware::PrintInstanceNameToString);
