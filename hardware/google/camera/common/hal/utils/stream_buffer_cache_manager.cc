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

//#define LOG_NDEBUG 0
#define LOG_TAG "StreamBufferCacheManager"
#define ATRACE_TAG ATRACE_TAG_CAMERA

#include <cutils/native_handle.h>
#include <log/log.h>
#include <sync/sync.h>
#include <utils/Trace.h>

#include <chrono>

#include "stream_buffer_cache_manager.h"
#include "utils.h"

using namespace std::chrono_literals;

namespace android {
namespace google_camera_hal {

// For CTS testCameraDeviceCaptureFailure, it holds image buffers and hal hits
// refill buffer timeout. Large timeout time also results in close session time
// is larger than 5 second in this test case. Typical buffer request from
// provider(e.g. framework) usually takes 1~2 ms. Small timeout time here may
// cause more framedrop in certain cases. But large timeout time can lead to
// extra long delay of traffic(in both ways) between the framework and the layer
// below HWL.
static constexpr auto kBufferWaitingTimeOutSec = 400ms;

StreamBufferCacheManager::StreamBufferCacheManager() {
  workload_thread_ = std::thread([this] { this->WorkloadThreadLoop(); });
  if (utils::SupportRealtimeThread()) {
    status_t res = utils::SetRealtimeThread(workload_thread_.native_handle());
    if (res != OK) {
      ALOGE("%s: SetRealtimeThread fail", __FUNCTION__);
    } else {
      ALOGI("%s: SetRealtimeThread OK", __FUNCTION__);
    }
  }
}

StreamBufferCacheManager::~StreamBufferCacheManager() {
  ALOGI("%s: Destroying stream buffer cache manager.", __FUNCTION__);
  {
    std::lock_guard<std::mutex> lock(workload_mutex_);
    workload_thread_exiting_ = true;
  }
  workload_cv_.notify_one();
  workload_thread_.join();
}

std::unique_ptr<StreamBufferCacheManager> StreamBufferCacheManager::Create() {
  ATRACE_CALL();

  auto manager =
      std::unique_ptr<StreamBufferCacheManager>(new StreamBufferCacheManager());
  if (manager == nullptr) {
    ALOGE("%s: Failed to create stream buffer cache manager.", __FUNCTION__);
    return nullptr;
  }

  manager->dummy_buffer_allocator_ = GrallocBufferAllocator::Create();
  if (manager->dummy_buffer_allocator_ == nullptr) {
    ALOGE("%s: Failed to create gralloc buffer allocator", __FUNCTION__);
    return nullptr;
  }

  ALOGI("%s: Created StreamBufferCacheManager.", __FUNCTION__);

  return manager;
}

status_t StreamBufferCacheManager::RegisterStream(
    const StreamBufferCacheRegInfo& reg_info) {
  ATRACE_CALL();
  if (reg_info.request_func == nullptr || reg_info.return_func == nullptr) {
    ALOGE("%s: Can't register stream, request or return function is nullptr.",
          __FUNCTION__);
    return BAD_VALUE;
  }

  if (reg_info.num_buffers_to_cache != 1) {
    ALOGE("%s: Only support caching one buffer.", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(caches_map_mutex_);
  if (stream_buffer_caches_.find(reg_info.stream_id) !=
      stream_buffer_caches_.end()) {
    ALOGE("%s: Stream %d has been registered.", __FUNCTION__,
          reg_info.stream_id);
    return INVALID_OPERATION;
  }

  status_t res = AddStreamBufferCacheLocked(reg_info);
  if (res != OK) {
    ALOGE("%s: Failed to add stream buffer cache.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  return OK;
}

status_t StreamBufferCacheManager::GetStreamBuffer(
    int32_t stream_id, StreamBufferRequestResult* res) {
  ATRACE_CALL();

  StreamBufferCache* stream_buffer_cache = nullptr;
  status_t result = GetStreamBufferCache(stream_id, &stream_buffer_cache);
  if (result != OK) {
    ALOGE("%s: Querying stream buffer cache failed.", __FUNCTION__);
    return result;
  }

  result = stream_buffer_cache->GetBuffer(res);
  if (result != OK) {
    ALOGE("%s: Get buffer for stream %d failed.", __FUNCTION__, stream_id);
    return UNKNOWN_ERROR;
  }

  {
    int fence_status = 0;
    if (res->buffer.acquire_fence != nullptr) {
      native_handle_t* fence_handle =
          const_cast<native_handle_t*>(res->buffer.acquire_fence);
      if (fence_handle->numFds == 1) {
        fence_status = sync_wait(fence_handle->data[0], kSyncWaitTimeMs);
      }
      if (0 != fence_status) {
        ALOGE("%s: Fence check failed.", __FUNCTION__);
      }
      native_handle_close(fence_handle);
      native_handle_delete(fence_handle);
      res->buffer.acquire_fence = nullptr;
    }
  }

  NotifyThreadWorkload();
  return OK;
}

status_t StreamBufferCacheManager::NotifyProviderReadiness(int32_t stream_id) {
  StreamBufferCache* stream_buffer_cache = nullptr;
  status_t res = GetStreamBufferCache(stream_id, &stream_buffer_cache);
  if (res != OK) {
    ALOGE("%s: Querying stream buffer cache failed.", __FUNCTION__);
    return res;
  }

  stream_buffer_cache->NotifyProviderReadiness();

  NotifyThreadWorkload();
  return OK;
}

status_t StreamBufferCacheManager::NotifyFlushingAll() {
  // Mark all StreamBufferCache as need to be flushed
  std::vector<StreamBufferCache*> stream_buffer_caches;
  {
    std::lock_guard<std::mutex> map_lock(caches_map_mutex_);
    for (auto& [stream_id, stream_buffer_cache] : stream_buffer_caches_) {
      stream_buffer_caches.push_back(stream_buffer_cache.get());
    }
  }

  {
    std::unique_lock<std::mutex> flush_lock(flush_mutex_);
    for (auto& stream_buffer_cache : stream_buffer_caches) {
      stream_buffer_cache->NotifyFlushing();
    }
  }

  NotifyThreadWorkload();
  return OK;
}

status_t StreamBufferCacheManager::IsStreamActive(int32_t stream_id,
                                                  bool* is_active) {
  StreamBufferCache* stream_buffer_cache = nullptr;
  status_t res = GetStreamBufferCache(stream_id, &stream_buffer_cache);
  if (res != OK) {
    ALOGE("%s: Querying stream buffer cache failed.", __FUNCTION__);
    return res;
  }

  *is_active = !stream_buffer_cache->IsStreamDeactivated();
  return OK;
}

status_t StreamBufferCacheManager::AddStreamBufferCacheLocked(
    const StreamBufferCacheRegInfo& reg_info) {
  auto stream_buffer_cache = StreamBufferCacheManager::StreamBufferCache::Create(
      reg_info, [this] { this->NotifyThreadWorkload(); },
      dummy_buffer_allocator_.get());
  if (stream_buffer_cache == nullptr) {
    ALOGE("%s: Failed to create StreamBufferCache for stream %d", __FUNCTION__,
          reg_info.stream_id);
    return UNKNOWN_ERROR;
  }

  stream_buffer_caches_[reg_info.stream_id] = std::move(stream_buffer_cache);
  return OK;
}

void StreamBufferCacheManager::WorkloadThreadLoop() {
  while (1) {
    bool exiting = false;
    {
      std::unique_lock<std::mutex> thread_lock(workload_mutex_);
      workload_cv_.wait(thread_lock, [this] {
        return has_new_workload_ || workload_thread_exiting_;
      });
      has_new_workload_ = false;
      exiting = workload_thread_exiting_;
    }

    std::vector<StreamBufferCacheManager::StreamBufferCache*> stream_buffer_caches;
    {
      std::unique_lock<std::mutex> map_lock(caches_map_mutex_);
      for (auto& [stream_id, cache] : stream_buffer_caches_) {
        stream_buffer_caches.push_back(cache.get());
      }
    }

    {
      std::unique_lock<std::mutex> flush_lock(flush_mutex_);
      for (auto& stream_buffer_cache : stream_buffer_caches) {
        status_t res = stream_buffer_cache->UpdateCache(exiting);
        if (res != OK) {
          ALOGE("%s: Updating(flush/refill) cache failed.", __FUNCTION__);
        }
      }
    }

    if (exiting) {
      ALOGI("%s: Exiting stream buffer cache manager workload thread.",
            __FUNCTION__);
      return;
    }
  }
}

void StreamBufferCacheManager::NotifyThreadWorkload() {
  {
    std::lock_guard<std::mutex> lock(workload_mutex_);
    has_new_workload_ = true;
  }
  workload_cv_.notify_one();
}

std::unique_ptr<StreamBufferCacheManager::StreamBufferCache>
StreamBufferCacheManager::StreamBufferCache::Create(
    const StreamBufferCacheRegInfo& reg_info,
    NotifyManagerThreadWorkloadFunc notify,
    IHalBufferAllocator* dummy_buffer_allocator) {
  if (notify == nullptr || dummy_buffer_allocator == nullptr) {
    ALOGE("%s: notify is nullptr or dummy_buffer_allocator is nullptr.",
          __FUNCTION__);
    return nullptr;
  }

  auto cache = std::unique_ptr<StreamBufferCacheManager::StreamBufferCache>(
      new StreamBufferCacheManager::StreamBufferCache(reg_info, notify,
                                                      dummy_buffer_allocator));
  if (cache == nullptr) {
    ALOGE("%s: Failed to create stream buffer cache.", __FUNCTION__);
    return nullptr;
  }

  return cache;
}

StreamBufferCacheManager::StreamBufferCache::StreamBufferCache(
    const StreamBufferCacheRegInfo& reg_info,
    NotifyManagerThreadWorkloadFunc notify,
    IHalBufferAllocator* dummy_buffer_allocator)
    : cache_info_(reg_info) {
  std::lock_guard<std::mutex> lock(cache_access_mutex_);
  notify_for_workload_ = notify;
  dummy_buffer_allocator_ = dummy_buffer_allocator;
}

status_t StreamBufferCacheManager::StreamBufferCache::UpdateCache(
    bool forced_flushing) {
  status_t res = OK;
  std::unique_lock<std::mutex> cache_lock(cache_access_mutex_);
  if (forced_flushing || notified_flushing_) {
    res = FlushLocked(forced_flushing);
    if (res != OK) {
      ALOGE("%s: Failed to flush stream buffer cache for stream %d",
            __FUNCTION__, cache_info_.stream_id);
      return res;
    }
  } else if (RefillableLocked()) {
    cache_lock.unlock();
    res = Refill();
    if (res != OK) {
      ALOGE("%s: Failed to refill stream buffer cache for stream %d",
            __FUNCTION__, cache_info_.stream_id);
      return res;
    }
  }
  return OK;
}

status_t StreamBufferCacheManager::StreamBufferCache::GetBuffer(
    StreamBufferRequestResult* res) {
  std::unique_lock<std::mutex> cache_lock(cache_access_mutex_);

  // 0. the provider of the stream for this cache must be ready
  if (!notified_provider_readiness_) {
    ALOGW("%s: The provider of stream %d is not ready.", __FUNCTION__,
          cache_info_.stream_id);
    return INVALID_OPERATION;
  }

  // 1. check if the cache is deactived or the stream has been notified for
  // flushing.
  if (stream_deactived_ || notified_flushing_) {
    res->is_dummy_buffer = true;
    res->buffer = dummy_buffer_;
    return OK;
  }

  // 2. check if there is any buffer available in the cache. If not, try
  // to wait for a short period and check again. In case of timeout, use the
  // dummy buffer instead.
  if (cached_buffers_.empty()) {
    // In case the GetStreamBufer is called after NotifyFlushingAll, this will
    // be the first event that should trigger the dedicated thread to restart
    // and refill the caches. An extra notification of thread workload is
    // harmless and will be bypassed very quickly.
    cache_lock.unlock();
    notify_for_workload_();
    cache_lock.lock();
    // Need to check this again since the state may change after the lock is
    // acquired for the second time.
    if (cached_buffers_.empty()) {
      // Wait for a certain amount of time for the cache to be refilled
      if (cache_access_cv_.wait_for(cache_lock, kBufferWaitingTimeOutSec) ==
          std::cv_status::timeout) {
        ALOGW("%s: StreamBufferCache for stream %d waiting for refill timeout.",
              __FUNCTION__, cache_info_.stream_id);
      }
    }
  }

  // 3. use dummy buffer if the cache is still empty
  if (cached_buffers_.empty()) {
    // Only allocate dummy buffer for the first time
    if (dummy_buffer_.buffer == nullptr) {
      status_t result = AllocateDummyBufferLocked();
      if (result != OK) {
        ALOGE("%s: Allocate dummy buffer failed.", __FUNCTION__);
        return UNKNOWN_ERROR;
      }
    }
    res->is_dummy_buffer = true;
    res->buffer = dummy_buffer_;
    return OK;
  } else {
    res->is_dummy_buffer = false;
    res->buffer = cached_buffers_.back();
    cached_buffers_.pop_back();
  }

  return OK;
}

bool StreamBufferCacheManager::StreamBufferCache::IsStreamDeactivated() {
  std::unique_lock<std::mutex> lock(cache_access_mutex_);
  return stream_deactived_;
}

void StreamBufferCacheManager::StreamBufferCache::NotifyProviderReadiness() {
  std::unique_lock<std::mutex> lock(cache_access_mutex_);
  notified_provider_readiness_ = true;
}

void StreamBufferCacheManager::StreamBufferCache::NotifyFlushing() {
  std::unique_lock<std::mutex> lock(cache_access_mutex_);
  notified_flushing_ = true;
}

status_t StreamBufferCacheManager::StreamBufferCache::FlushLocked(
    bool forced_flushing) {
  if (notified_flushing_ != true && !forced_flushing) {
    ALOGI("%s: Stream buffer cache is not notified for flushing.", __FUNCTION__);
    return INVALID_OPERATION;
  }

  notified_flushing_ = false;
  if (cache_info_.return_func == nullptr) {
    ALOGE("%s: return_func is nullptr.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  if (cached_buffers_.empty()) {
    ALOGV("%s: Stream buffer cache is already empty.", __FUNCTION__);
    ReleaseDummyBufferLocked();
    return OK;
  }

  status_t res = cache_info_.return_func(cached_buffers_);
  if (res != OK) {
    ALOGE("%s: Failed to return buffers.", __FUNCTION__);
    return res;
  }

  cached_buffers_.clear();
  ReleaseDummyBufferLocked();

  return OK;
}

status_t StreamBufferCacheManager::StreamBufferCache::Refill() {
  int32_t num_buffers_to_acquire = 0;
  {
    std::unique_lock<std::mutex> cache_lock(cache_access_mutex_);
    if (cache_info_.request_func == nullptr) {
      ALOGE("%s: request_func is nullptr.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }

    if (!notified_provider_readiness_) {
      ALOGI("%s: Provider is not ready.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }

    if (stream_deactived_ || notified_flushing_) {
      ALOGI("%s: Already notified for flushing or stream already deactived.",
            __FUNCTION__);
      return OK;
    }

    if (cached_buffers_.size() >= cache_info_.num_buffers_to_cache) {
      ALOGV("%s: Stream buffer cache is already full.", __FUNCTION__);
      return INVALID_OPERATION;
    }

    num_buffers_to_acquire =
        cache_info_.num_buffers_to_cache - cached_buffers_.size();
  }

  // Requesting buffer from the provider can take long(e.g. even > 1sec),
  // consumer should not be blocked by this procedure and can get dummy buffer
  // to unblock other pipelines. Thus, cache_access_mutex_ doesn't need to be
  // locked here.
  std::vector<StreamBuffer> buffers;
  StreamBufferRequestError req_status = StreamBufferRequestError::kOk;
  status_t res =
      cache_info_.request_func(num_buffers_to_acquire, &buffers, &req_status);

  std::unique_lock<std::mutex> cache_lock(cache_access_mutex_);
  if (res != OK) {
    status_t result = AllocateDummyBufferLocked();
    if (result != OK) {
      ALOGE("%s: Allocate dummy buffer failed.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }
  }

  if (buffers.empty() || res != OK) {
    ALOGW("%s: Failed to acquire buffer for stream %d, error %d", __FUNCTION__,
          cache_info_.stream_id, req_status);
    switch (req_status) {
      case StreamBufferRequestError::kNoBufferAvailable:
      case StreamBufferRequestError::kMaxBufferExceeded:
        ALOGI(
            "%s: No buffer available or max buffer exceeded for stream %d. "
            "Will retry for next request or when refilling other streams.",
            __FUNCTION__, cache_info_.stream_id);
        break;
      case StreamBufferRequestError::kStreamDisconnected:
      case StreamBufferRequestError::kUnknownError:
        ALOGW(
            "%s: Stream %d is disconnected or unknown error observed."
            "This stream is marked as inactive.",
            __FUNCTION__, cache_info_.stream_id);
        ALOGI("%s: Stream %d begin to use dummy buffer.", __FUNCTION__,
              cache_info_.stream_id);
        stream_deactived_ = true;
        break;
      default:
        ALOGE("%s: Unknown error code: %d", __FUNCTION__, req_status);
        break;
    }
  } else {
    for (auto& buffer : buffers) {
      cached_buffers_.push_back(buffer);
    }
  }

  cache_access_cv_.notify_one();

  return OK;
}

bool StreamBufferCacheManager::StreamBufferCache::RefillableLocked() const {
  // No need to refill if the provider is not ready
  if (!notified_provider_readiness_) {
    return false;
  }

  // No need to refill if the stream buffer cache is notified for flushing
  if (notified_flushing_) {
    return false;
  }

  // Need to refill if the cache is not full
  return cached_buffers_.size() < cache_info_.num_buffers_to_cache;
}

status_t StreamBufferCacheManager::StreamBufferCache::AllocateDummyBufferLocked() {
  if (dummy_buffer_.buffer != nullptr) {
    ALOGW("%s: Dummy buffer has already been allocated.", __FUNCTION__);
    return OK;
  }

  HalBufferDescriptor hal_buffer_descriptor{
      .stream_id = cache_info_.stream_id,
      .width = cache_info_.width,
      .height = cache_info_.height,
      .format = cache_info_.format,
      .producer_flags = cache_info_.producer_flags,
      .consumer_flags = cache_info_.consumer_flags,
      .immediate_num_buffers = 1,
      .max_num_buffers = 1,
  };
  std::vector<buffer_handle_t> buffers;

  status_t res =
      dummy_buffer_allocator_->AllocateBuffers(hal_buffer_descriptor, &buffers);
  if (res != OK) {
    ALOGE("%s: Dummy buffer allocator AllocateBuffers failed.", __FUNCTION__);
    return res;
  }

  if (buffers.size() != hal_buffer_descriptor.immediate_num_buffers) {
    ALOGE("%s: Not enough buffers allocated.", __FUNCTION__);
    return NO_MEMORY;
  }
  dummy_buffer_.stream_id = cache_info_.stream_id;
  dummy_buffer_.buffer = buffers[0];
  ALOGI("%s: [sbc] Dummy buffer allocated: strm %d buffer %p", __FUNCTION__,
        dummy_buffer_.stream_id, dummy_buffer_.buffer);

  return OK;
}

void StreamBufferCacheManager::StreamBufferCache::ReleaseDummyBufferLocked() {
  // Release dummy buffer if ever acquired from the dummy_buffer_allocator_.
  if (dummy_buffer_.buffer != nullptr) {
    std::vector<buffer_handle_t> buffers(1, dummy_buffer_.buffer);
    dummy_buffer_allocator_->FreeBuffers(&buffers);
    dummy_buffer_.buffer = nullptr;
  }
}

status_t StreamBufferCacheManager::GetStreamBufferCache(
    int32_t stream_id, StreamBufferCache** stream_buffer_cache) {
  std::unique_lock<std::mutex> map_lock(caches_map_mutex_);
  if (stream_buffer_caches_.find(stream_id) == stream_buffer_caches_.end()) {
    ALOGE("%s: Sream %d can not be found.", __FUNCTION__, stream_id);
    return BAD_VALUE;
  }

  *stream_buffer_cache = stream_buffer_caches_[stream_id].get();
  if (*stream_buffer_cache == nullptr) {
    ALOGE("%s: Get null cache pointer.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  return OK;
}

}  // namespace google_camera_hal
}  // namespace android
