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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_STREAM_BUFFER_CACHE_MANAGER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_STREAM_BUFFER_CACHE_MANAGER_H_

#include <utils/Errors.h>

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "gralloc_buffer_allocator.h"
#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// Function to request buffer for a specific stream. The size of buffers vector
// should be extended by the callee. The caller owns the acquire fences of the
// acquired StreamBuffer. The caller owns the std::vector that contain the
// allocated buffers. The buffers themselves are released through the buffer
// return function or through result processing functions.
using StreamBufferRequestFunc =
    std::function<status_t(uint32_t num_buffer, std::vector<StreamBuffer>* buffers,
                           StreamBufferRequestError* status)>;

// Function to return buffer for a specific stream
using StreamBufferReturnFunc =
    std::function<status_t(const std::vector<StreamBuffer>& buffers)>;

// Function to notify the manager for a new thread loop workload
using NotifyManagerThreadWorkloadFunc = std::function<void()>;
//
// StreamBufferCacheRegInfo
//
// Contains all information needed to register a StreamBufferCache into manager
//
struct StreamBufferCacheRegInfo {
  // Interface to request buffer for this cache
  StreamBufferRequestFunc request_func = nullptr;
  // Interface to return buffer from this cache
  StreamBufferReturnFunc return_func = nullptr;
  // Stream to be registered
  int32_t stream_id = -1;
  // Width of the stream
  uint32_t width = 0;
  // Height of the stream
  uint32_t height = 0;
  // Format of the stream
  android_pixel_format_t format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
  // Producer flags of the stream
  uint64_t producer_flags = 0;
  // Consumer flags of the stream
  uint64_t consumer_flags = 0;
  // Number of buffers that the manager needs to cache
  uint32_t num_buffers_to_cache = 1;
};

//
// StreamBufferRequestResult
//
// Contains all information returned to the client by GetStreamBuffer function.
//
struct StreamBufferRequestResult {
  // Whether the returned StreamBuffer is a dummy buffer or an actual buffer
  // obtained from the buffer provider. Client should return the buffer from
  // providers through the normal result processing functions. There is no need
  // for clients to return or recycle a dummy buffer returned.
  bool is_dummy_buffer = false;
  // StreamBuffer obtained
  StreamBuffer buffer;
};

//
// StreamBufferCacheManager
//
// A StreamBufferCacheManager manages a list of StreamBufferCache for registered
// streams. A client needs to register a stream first. It then needs to signal
// the manager to start caching buffers for that stream. It can then get stream
// buffers from the manager. The buffers obtained, not matter buffers from buf
// provider or a dummy buffer, do not need to be returned to the manager. The
// client should notify the manager to flush all buffers cached before a session
// can successfully end.
//
// The manager uses a dedicated thread to asynchronously request/return buffers
// while clients threads fetch buffers and notify for a change of state.
//
class StreamBufferCacheManager {
 public:
  // Create an instance of the StreamBufferCacheManager
  static std::unique_ptr<StreamBufferCacheManager> Create();

  virtual ~StreamBufferCacheManager();

  // Client calls this function to register the buffer caching service
  status_t RegisterStream(const StreamBufferCacheRegInfo& reg_info);

  // Client calls this function to signal the manager to start caching buffer of
  // the stream with stream_id. The manager will not cache stream buffer by
  // requesting from the provider for a stream until this function is invoked.
  status_t NotifyProviderReadiness(int32_t stream_id);

  // Client calls this function to request for buffer of stream with stream_id.
  // StreamBufferCacheManager only supports getting one buffer each time. Client
  // is responsible to call NotifyProviderReadiness before calling this func.
  // Caller owns the StreamBufferRequestResult and should keep it valid until
  // the function is returned. The ownership of the fences of the StreamBuffer
  // in the StreamBufferRequestResult is transferred to the caller after this
  // function is returned. In case dummy buffer is returned, the fences are all
  // nullptr.
  status_t GetStreamBuffer(int32_t stream_id, StreamBufferRequestResult* res);

  // Client calls this function to signal the manager to flush all buffers
  // cached for all streams registered. After this function is called, client
  // can still call GetStreamBuffer to trigger the stream buffer cache manager
  // to restart caching buffers for a specific stream.
  status_t NotifyFlushingAll();

  // Whether stream buffer cache manager can still acquire buffer from the
  // provider successfully(e.g. if a stream is abandoned by the framework, this
  // returns false). Once a stream is inactive, dummy buffer will be used in all
  // following GetStreamBuffer calling. Calling NotifyFlushingAll does not make
  // a change in this case.
  status_t IsStreamActive(int32_t stream_id, bool* is_active);

 protected:
  StreamBufferCacheManager();

 private:
  // Duration to wait for fence.
  static constexpr uint32_t kSyncWaitTimeMs = 5000;

  //
  // StreamBufferCache
  //
  // Contains all information and status of the stream buffer cache for a
  // specific stream with stream_id
  //
  class StreamBufferCache {
   public:
    // Create a StreamBufferCache from the StreamBufferCacheRegInfo
    // reg_info contains the basic information about the stream this cache is
    // for and interfaces for buffer return and request.
    // notify is the function for each stream buffer cache to notify the manager
    // for new thread loop work load.
    // dummy_buffer_allocator allocates the dummy buffer needed when buffer
    // provider can not fulfill a buffer request any more.
    static std::unique_ptr<StreamBufferCache> Create(
        const StreamBufferCacheRegInfo& reg_info,
        NotifyManagerThreadWorkloadFunc notify,
        IHalBufferAllocator* dummy_buffer_allocator);

    virtual ~StreamBufferCache() = default;

    // Flush the stream buffer cache if the forced_flushing flag is set or if
    // the stream buffer cache has been notified for flushing. Otherwise, check
    // if the stream buffer cache needs to be and can be refilled. Do so if that
    // is true.
    status_t UpdateCache(bool forced_flushing);

    // Get a buffer for the client. The buffer returned can be a dummy buffer,
    // in which case, the is_dummy_buffer field in res will be true.
    status_t GetBuffer(StreamBufferRequestResult* res);

    // Notify provider readiness. Client should call this function before
    // calling Refill and GetBuffer.
    void NotifyProviderReadiness();

    // Notify the stream buffer cache to flush all buffers it acquire from the
    // provider.
    void NotifyFlushing();

    // Return whether the stream that this cache is for has been deactivated
    bool IsStreamDeactivated();

   protected:
    StreamBufferCache(const StreamBufferCacheRegInfo& reg_info,
                      NotifyManagerThreadWorkloadFunc notify,
                      IHalBufferAllocator* dummy_buffer_allocator);

   private:
    // Flush all buffers acquired from the buffer provider. Return the acquired
    // buffers through the return_func.
    // The cache_access_mutex_ must be locked when calling this function.
    status_t FlushLocked(bool forced_flushing);

    // Refill the cached buffers by trying to acquire buffers from the buffer
    // provider using request_func. If the provider can not fulfill the request
    // by returning an empty buffer vector. The stream buffer cache will be
    // providing dummy buffer for all following requests.
    // TODO(b/136107942): Only one thread(currently the manager's workload thread)
    //                    should call this function to avoid unexpected racing
    //                    condition. This will be fixed by taking advantage of
    //                    a buffer requesting task queue from which the dedicated
    //                    thread fetches the task and refill the cache separately.
    status_t Refill();

    // Whether a stream buffer cache can be refilled.
    // The cache_access_mutex_ must be locked when calling this function.
    bool RefillableLocked() const;

    // Allocate dummy buffer for this stream buffer cache. The
    // cache_access_mutex_ needs to be locked before calling this function.
    status_t AllocateDummyBufferLocked();

    // Release allocated dummy buffer when StreamBufferCache exiting.
    // The cache_access_mutex_ needs to be locked before calling this function.
    void ReleaseDummyBufferLocked();

    // Any access to the cache content must be guarded by this mutex.
    std::mutex cache_access_mutex_;
    // Condition variable used in timed wait for refilling
    std::condition_variable cache_access_cv_;
    // Basic information about this stream buffer cache
    const StreamBufferCacheRegInfo cache_info_;
    // Cached StreamBuffers
    std::vector<StreamBuffer> cached_buffers_;
    // Whether the stream this cache is for has been deactived. The stream is
    // labeled as deactived when kStreamDisconnected or kUnknownError is
    // returned by a request_func_. In this case, all following request_func_ is
    // expected to raise the same error. So dummy buffer will be used directly
    // without wasting the effort to call request_func_ again. Error code
    // kNoBufferAvailable and kMaxBufferExceeded should not cause this to be
    // labeled as true. The next UpdateCache status should still try to refill
    // the cache.
    bool stream_deactived_ = false;
    // Dummy StreamBuffer reserved for errorneous situation. In case there is
    // not available cached buffers, this dummy buffer is used to allow the
    // client to continue its ongoing work without crashing. This dummy buffer
    // is reused and should not be returned to the buf provider. If this buffer
    // is returned, the is_dummy_buffer_ flag in the BufferRequestResult must be
    // set to true.
    StreamBuffer dummy_buffer_;
    // Whether this stream has been notified by the client for flushing. This
    // flag should be cleared after StreamBufferCache is flushed. Once this is
    // flagged by the client, StreamBufferCacheManager will return all buffers
    // acquired from provider for this cache the next time the dedicated thread
    // processes any request/return workload.
    bool notified_flushing_ = false;
    // StreamBufferCacheManager does not refill a StreamBufferCache until this is
    // notified by the client. Client should notify this after the buffer provider
    // (e.g. framework) is ready to handle buffer requests. Usually, this is set
    // once in a session and will not be cleared in the same session.
    bool notified_provider_readiness_ = false;
    // Interface to notify the parent manager for new threadloop workload.
    NotifyManagerThreadWorkloadFunc notify_for_workload_ = nullptr;
    // Allocator of the dummy buffer for this stream. The stream buffer cache
    // manager owns this throughout the life cycle of this stream buffer cahce.
    IHalBufferAllocator* dummy_buffer_allocator_ = nullptr;
  };

  // Add stream buffer cache. Lock caches_map_mutex_ before calling this func.
  status_t AddStreamBufferCacheLocked(const StreamBufferCacheRegInfo& reg_info);

  // Procedure running in the dedicated thread loop
  void WorkloadThreadLoop();

  // Notifies the dedicated thread for new processing request. This can only be
  // invoked after any change to the cache state is done and cache_access_mutex
  // has been unlocked.
  void NotifyThreadWorkload();

  // Fetch the actual StreamBufferCache given a stream_id
  status_t GetStreamBufferCache(int32_t stream_id,
                                StreamBufferCache** stream_buffer_cache);

  // Guards the stream_buffer_caches_
  std::mutex caches_map_mutex_;
  // Mapping from a stream_id to the StreamBufferCache for that stream. Any
  // access to this map must be guarded by the caches_map_mutex.
  std::map<int32_t, std::unique_ptr<StreamBufferCache>> stream_buffer_caches_;

  // Guards the thread dedicated for StreamBuffer request and return
  std::mutex workload_mutex_;
  // Thread dedicated for stream buffer request and return
  std::thread workload_thread_;
  // CV for dedicated thread guarding
  std::condition_variable workload_cv_;
  // Whether the dedicated thread has been notified for exiting. Change to this
  // must be guarded by request_return_mutex_.
  bool workload_thread_exiting_ = false;
  // Whether a processing request has been notified. Change to this must be
  // guarded by request_return_mutex_;
  bool has_new_workload_ = false;
  // The dummy buffer allocator allocates the dummy buffer. It only allocates
  // the dummy buffer when a stream buffer cache is NotifyProviderReadiness.
  std::unique_ptr<IHalBufferAllocator> dummy_buffer_allocator_;

  // Guards NotifyFlushingAll. In case the workload thread is processing workload,
  // the NotifyFlushingAll calling should wait until workload loop is done. This
  // avoids the false trigger of another caching request after a stream buffer
  // cache is flushed.
  // For example, there are two stream cache in manager. Without using the mutex,
  // there is a chance the following could happen:
  //
  // Workload thread waiting for signal
  // |
  // Workload thread triggered by GetBuffer
  // |
  // c1 request new buffer
  // |
  // NotifyFlushingAll (workload thread bypasses next wait due to this)
  // |
  // c2 return cached buffer
  // |
  // Next workload thread loop starts immediately
  // |
  // c1 return buffer
  // |
  // c2 request buffer <-- this should not happen and is avoid by the mutex
  // |
  // WaitUntilDrain
  std::mutex flush_mutex_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_STREAM_BUFFER_CACHE_MANAGER_H_