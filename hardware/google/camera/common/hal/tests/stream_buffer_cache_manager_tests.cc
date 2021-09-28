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

#define LOG_TAG "StreamBufferCacheManagerTests"
#include <cutils/properties.h>
#include <gtest/gtest.h>
#include <log/log.h>

#include <chrono>
#include <map>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "stream_buffer_cache_manager.h"

namespace android {
namespace google_camera_hal {

using namespace std::chrono_literals;

class StreamBufferCacheManagerTests : public ::testing::Test {
 protected:
  // This is used to mock the framework callback latency
  static constexpr auto kAllocateBufferFuncLatency = 10ms;
  // The minimum interval two successful buffer acquisition must have, this
  // should be close to kAllocateBufferFuncLatency, but leave a safe gap(1ms)
  // in case of timing fluctuation.
  static constexpr auto kBufferAcquireMinLatency = 9ms;
  // The maximum latency that the cached buffer is returned to the framework
  static constexpr auto kBufferReturnMaxLatency = 5ms;
  static const uint32_t kDefaultRemainingFulfillment = 2;

  status_t AllocateBufferFunc(uint32_t num_buffer,
                              std::vector<StreamBuffer>* buffers,
                              StreamBufferRequestError* status) {
    *status = StreamBufferRequestError::kOk;
    buffers->clear();
    if (remaining_number_of_fulfillment_ != 0) {
      buffers->resize(num_buffer);
    } else {
      *status = StreamBufferRequestError::kStreamDisconnected;
      return OK;
    }

    // Mocking the framework callback latency
    std::this_thread::sleep_for(kAllocateBufferFuncLatency);
    remaining_number_of_fulfillment_--;
    return OK;
  }

  status_t ReturnBufferFunc(const std::vector<StreamBuffer>& /*buffers*/) {
    num_return_buffer_func_called++;
    return OK;
  }

  const StreamBufferCacheRegInfo kDummyCacheRegInfo{
      .request_func =
          [this](uint32_t num_buffer, std::vector<StreamBuffer>* buffers,
                 StreamBufferRequestError* status) {
            return this->AllocateBufferFunc(num_buffer, buffers, status);
          },
      .return_func =
          [this](const std::vector<StreamBuffer>& buffers) {
            return this->ReturnBufferFunc(buffers);
          },
      .stream_id = 1,
      .width = 640,
      .height = 480,
      .format = HAL_PIXEL_FORMAT_RAW10,
      .producer_flags = 0,
      .consumer_flags = 0,
      .num_buffers_to_cache = 1};

  void SetUp() override {
    // Skip test if product is unsupported.
    char product_name[PROPERTY_VALUE_MAX];
    // TODO(b/142732212): Flacky occurred,
    // Remove "blueline", "crosshatch", "flame", "coral"
    // from supported_product_list first.
    std::unordered_set<std::string> const supported_product_list{""};
    property_get("ro.build.product", product_name, "");
    bool product_support_test =
        supported_product_list.find(std::string{product_name}) !=
        supported_product_list.end();
    if (!product_support_test) {
      GTEST_SKIP();
    }

    cache_manager_ = StreamBufferCacheManager::Create();
    ASSERT_NE(cache_manager_, nullptr)
        << " Creating StreamBufferCacheManager failed";
  }

  // Set remaining_number_of_fulfillment_. This should be called before any
  // other operations if the test needs to control this.
  void SetRemainingFulfillment(uint32_t remaining_num) {
    remaining_number_of_fulfillment_ = remaining_num;
  }

  // StreamBufferCacheManager created by this test fixture
  std::unique_ptr<StreamBufferCacheManager> cache_manager_;

  // Counts the total number of buffers acquired by each stream id
  std::map<int32_t, uint32_t> buffer_allocation_cnt_;

  // Counts the total number of buffers returned by each stream id
  std::map<int32_t, uint32_t> buffer_return_cnt_;

  // Max number of requests that AllocateBufferFunc can fulfill. This is used to
  // mock the failure of buffer provider from the framework.
  uint32_t remaining_number_of_fulfillment_ = kDefaultRemainingFulfillment;

  // Number of times the ReturnBufferFunc is called.
  int32_t num_return_buffer_func_called = 0;
};

// Test RegisterStream
TEST_F(StreamBufferCacheManagerTests, RegisterStream) {
  // RegisterStream should succeed
  status_t res = cache_manager_->RegisterStream(kDummyCacheRegInfo);
  ASSERT_EQ(res, OK) << " RegisterStream failed!" << strerror(res);

  // RegisterStream should fail when registering the same stream twice
  res = cache_manager_->RegisterStream(kDummyCacheRegInfo);
  ASSERT_NE(res, OK) << " RegisterStream succeeded when registering the same "
                        "stream for more than once!";

  // RegisterStream should succeed when registering another stream
  StreamBufferCacheRegInfo another_reg_info = kDummyCacheRegInfo;
  another_reg_info.stream_id = kDummyCacheRegInfo.stream_id + 1;
  res = cache_manager_->RegisterStream(another_reg_info);
  ASSERT_EQ(res, OK) << " RegisterStream another stream failed!"
                     << strerror(res);
}

// Test NotifyProviderReadiness
TEST_F(StreamBufferCacheManagerTests, NotifyProviderReadiness) {
  // Need to register stream before notifying provider readiness
  status_t res =
      cache_manager_->NotifyProviderReadiness(kDummyCacheRegInfo.stream_id);
  ASSERT_NE(res, OK) << " NotifyProviderReadiness succeeded without reigstering"
                        " the stream.";

  res = cache_manager_->RegisterStream(kDummyCacheRegInfo);
  ASSERT_EQ(res, OK) << " RegisterStream failed!" << strerror(res);

  // Notify ProviderReadiness should succeed after the stream is registered
  res = cache_manager_->NotifyProviderReadiness(kDummyCacheRegInfo.stream_id);
  ASSERT_EQ(res, OK) << " NotifyProviderReadiness failed!" << strerror(res);
}

// Test the correct order of calling GetStreamBuffer
TEST_F(StreamBufferCacheManagerTests, BasicGetStreamBuffer) {
  StreamBufferRequestResult req_result;
  // GetStreamBuffer should fail before the stream is registered.
  status_t res = cache_manager_->GetStreamBuffer(kDummyCacheRegInfo.stream_id,
                                                 &req_result);
  ASSERT_NE(res, OK) << " GetStreamBuffer should fail before stream is "
                        "registered and provider readiness is notified.";

  res = cache_manager_->RegisterStream(kDummyCacheRegInfo);
  ASSERT_EQ(res, OK) << " RegisterStream failed!" << strerror(res);

  // GetStreamBuffer should fail before the stream's provider is notified for
  // readiness.
  res = cache_manager_->GetStreamBuffer(kDummyCacheRegInfo.stream_id,
                                        &req_result);
  ASSERT_NE(res, OK) << " GetStreamBuffer should fail before stream is "
                        "registered and provider readiness is notified.";

  res = cache_manager_->NotifyProviderReadiness(kDummyCacheRegInfo.stream_id);
  ASSERT_EQ(res, OK) << " NotifyProviderReadiness failed!" << strerror(res);

  // GetStreamBuffer should succeed after the stream is registered and its
  // provider's readiness is notified.
  res = cache_manager_->GetStreamBuffer(kDummyCacheRegInfo.stream_id,
                                        &req_result);
  ASSERT_EQ(res, OK) << " Getting stream buffer failed!" << strerror(res);
}

// Test sequence of function call to GetStreamBuffer
TEST_F(StreamBufferCacheManagerTests, SequenceOfGetStreamBuffer) {
  const uint32_t kValidBufferRequests = 2;
  SetRemainingFulfillment(kValidBufferRequests);
  status_t res = cache_manager_->RegisterStream(kDummyCacheRegInfo);
  ASSERT_EQ(res, OK) << " RegisterStream failed!" << strerror(res);

  res = cache_manager_->NotifyProviderReadiness(kDummyCacheRegInfo.stream_id);
  ASSERT_EQ(res, OK) << " NotifyProviderReadiness failed!" << strerror(res);

  // Allow enough time for the buffer allocator to refill the cache
  std::this_thread::sleep_for(kAllocateBufferFuncLatency);

  // First GetStreamBuffer should succeed immediately with a non-dummy buffer
  StreamBufferRequestResult req_result;
  auto t_start = std::chrono::high_resolution_clock::now();
  res = cache_manager_->GetStreamBuffer(kDummyCacheRegInfo.stream_id,
                                        &req_result);
  auto t_end = std::chrono::high_resolution_clock::now();
  ASSERT_EQ(res, OK) << " GetStreamBuffer failed!" << strerror(res);
  ASSERT_EQ(true, t_end - t_start < kBufferAcquireMinLatency)
      << " First buffer request should be fulfilled immediately.";
  ASSERT_EQ(req_result.is_dummy_buffer, false)
      << " First buffer request got dummy buffer.";

  // Second GetStreamBuffer should succeed with a non-dummy buffer, but should
  // happen after a gap longer than kBufferAcquireMinLatency.
  t_start = std::chrono::high_resolution_clock::now();
  res = cache_manager_->GetStreamBuffer(kDummyCacheRegInfo.stream_id,
                                        &req_result);
  t_end = std::chrono::high_resolution_clock::now();
  ASSERT_EQ(res, OK) << " GetStreamBuffer failed!" << strerror(res);
  ASSERT_EQ(true, t_end - t_start > kBufferAcquireMinLatency)
      << " Buffer acquisition gap between two consecutive reqs is too small.";
  ASSERT_EQ(req_result.is_dummy_buffer, false)
      << " Second buffer request got dummy buffer.";

  // Allow enough time for the buffer allocator to refill the cache
  std::this_thread::sleep_for(kAllocateBufferFuncLatency);
  // No more remaining fulfilment so StreamBufferCache should be either deactive
  // or inactive.
  bool is_active = false;
  res = cache_manager_->IsStreamActive(kDummyCacheRegInfo.stream_id, &is_active);
  ASSERT_EQ(res, OK) << " IsStreamActive failed!" << strerror(res);
  ASSERT_EQ(is_active, false)
      << " StreamBufferCache should be either deactive or inactive!";

  // Third GetStreamBuffer should succeed with a dummy buffer immediately
  t_start = std::chrono::high_resolution_clock::now();
  res = cache_manager_->GetStreamBuffer(kDummyCacheRegInfo.stream_id,
                                        &req_result);
  t_end = std::chrono::high_resolution_clock::now();
  ASSERT_EQ(res, OK) << " GetStreamBuffer failed!" << strerror(res);
  ASSERT_EQ(true, t_end - t_start < kBufferAcquireMinLatency)
      << " Buffer acquisition gap for a dummy return should be negligible.";
  ASSERT_EQ(req_result.is_dummy_buffer, true)
      << " Third buffer request did not get dummy buffer.";
}

// Test NotifyFlushingAll
TEST_F(StreamBufferCacheManagerTests, NotifyFlushingAll) {
  // One before the first GetStreamBuffer. One after that. One for the
  // GetStreamBuffer happens after the NotifyFlushingAll.
  const uint32_t kValidBufferRequests = 3;
  SetRemainingFulfillment(kValidBufferRequests);
  status_t res = cache_manager_->RegisterStream(kDummyCacheRegInfo);
  ASSERT_EQ(res, OK) << " RegisterStream failed!" << strerror(res);

  res = cache_manager_->NotifyProviderReadiness(kDummyCacheRegInfo.stream_id);
  ASSERT_EQ(res, OK) << " NotifyProviderReadiness failed!" << strerror(res);

  // Allow enough time for the buffer allocator to refill the cache
  std::this_thread::sleep_for(kAllocateBufferFuncLatency);

  // First GetStreamBuffer should succeed immediately with a non-dummy buffer
  StreamBufferRequestResult req_result;
  auto t_start = std::chrono::high_resolution_clock::now();
  res = cache_manager_->GetStreamBuffer(kDummyCacheRegInfo.stream_id,
                                        &req_result);
  auto t_end = std::chrono::high_resolution_clock::now();
  ASSERT_EQ(res, OK) << " GetStreamBuffer failed!" << strerror(res);
  ASSERT_EQ(true, t_end - t_start < kBufferAcquireMinLatency)
      << " First buffer request should be fulfilled immediately.";
  ASSERT_EQ(req_result.is_dummy_buffer, false)
      << " First buffer request got dummy buffer.";

  // Allow enough time for the buffer allocator to refill the cache
  std::this_thread::sleep_for(kAllocateBufferFuncLatency);
  // NotifyFlushingAll should succeed
  ASSERT_EQ(num_return_buffer_func_called, 0)
      << " ReturnBufferFunc should not be called before NotifyFlushingAll!";
  res = cache_manager_->NotifyFlushingAll();
  ASSERT_EQ(res, OK) << " NotifyFlushingAll failed!" << strerror(res);
  std::this_thread::sleep_for(kBufferReturnMaxLatency);
  ASSERT_EQ(num_return_buffer_func_called, 1)
      << " ReturnBufferFunc was not called after NotifyFlushingAll is invoked!";

  // GetStreamBuffer should still be able to re-trigger cache to refill after
  // NotifyFlushingAll is called.
  res = cache_manager_->GetStreamBuffer(kDummyCacheRegInfo.stream_id,
                                        &req_result);
  ASSERT_EQ(res, OK) << " GetStreamBuffer failed!" << strerror(res);
  ASSERT_EQ(req_result.is_dummy_buffer, false)
      << " Buffer request got dummy buffer.";
}

// Test IsStreamActive
TEST_F(StreamBufferCacheManagerTests, IsStreamActive) {
  const uint32_t kValidBufferRequests = 1;
  SetRemainingFulfillment(kValidBufferRequests);
  status_t res = cache_manager_->RegisterStream(kDummyCacheRegInfo);
  ASSERT_EQ(res, OK) << " RegisterStream failed!" << strerror(res);

  res = cache_manager_->NotifyProviderReadiness(kDummyCacheRegInfo.stream_id);
  ASSERT_EQ(res, OK) << " NotifyProviderReadiness failed!" << strerror(res);

  // Allow enough time for the buffer allocator to refill the cache
  std::this_thread::sleep_for(kAllocateBufferFuncLatency);

  // StreamBufferCache should be valid before dummy buffer is used.
  bool is_active = false;
  res = cache_manager_->IsStreamActive(kDummyCacheRegInfo.stream_id, &is_active);
  ASSERT_EQ(res, OK) << " IsStreamActive failed!" << strerror(res);
  ASSERT_EQ(is_active, true) << " StreamBufferCache should be active!";

  StreamBufferRequestResult req_result;
  res = cache_manager_->GetStreamBuffer(kDummyCacheRegInfo.stream_id,
                                        &req_result);

  // Allow enough time for buffer provider to finish its job
  std::this_thread::sleep_for(kAllocateBufferFuncLatency);
  // There is only one valid buffer request. So the stream will be deactive
  // after the GetStreamBuffer(when the cache tries the second buffer request).
  res = cache_manager_->IsStreamActive(kDummyCacheRegInfo.stream_id, &is_active);
  ASSERT_EQ(res, OK) << " IsStreamActive failed!" << strerror(res);
  ASSERT_EQ(is_active, false) << " StreamBufferCache should be deactived!";
}

}  // namespace google_camera_hal
}  // namespace android
