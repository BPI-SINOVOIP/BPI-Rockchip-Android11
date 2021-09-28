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
#define LOG_TAG "GCH_DepthProcessBlock"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <cutils/properties.h>
#include <hardware/gralloc1.h>
#include <log/log.h>
#include <sys/mman.h>
#include <utils/Trace.h>

#include <dlfcn.h>

#include "depth_process_block.h"
#include "hal_types.h"
#include "hal_utils.h"
#include "result_processor.h"

namespace android {
namespace google_camera_hal {

static std::string kDepthGeneratorLib = "/vendor/lib64/libdepthgenerator.so";
using android::depth_generator::CreateDepthGenerator_t;
const float kSmallOffset = 0.01f;

std::unique_ptr<DepthProcessBlock> DepthProcessBlock::Create(
    CameraDeviceSessionHwl* device_session_hwl,
    HwlRequestBuffersFunc request_stream_buffers,
    const DepthProcessBlockCreateData& create_data) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr", __FUNCTION__);
    return nullptr;
  }

  auto block = std::unique_ptr<DepthProcessBlock>(
      new DepthProcessBlock(request_stream_buffers, create_data));
  if (block == nullptr) {
    ALOGE("%s: Creating DepthProcessBlock failed.", __FUNCTION__);
    return nullptr;
  }

  status_t res = block->InitializeBufferManagementStatus(device_session_hwl);
  if (res != OK) {
    ALOGE("%s: Failed to initialize HAL Buffer Management status.",
          __FUNCTION__);
    return nullptr;
  }

  res = block->CalculateActiveArraySizeRatio(device_session_hwl);
  if (res != OK) {
    ALOGE("%s: Calculating active array size ratio failed.", __FUNCTION__);
    return nullptr;
  }

  // TODO(b/128633958): remove this after FLL syncing is verified
  block->force_internal_stream_ =
      property_get_bool("persist.camera.rgbird.forceinternal", false);
  if (block->force_internal_stream_) {
    ALOGI("%s: Force creating internal streams for IR pipelines", __FUNCTION__);
  }

  block->pipelined_depth_engine_enabled_ =
      property_get_bool("persist.camera.frontdepth.enablepipeline", true);

  // TODO(b/129910835): Change the controlling prop into some deterministic
  // logic that controls when the front depth autocal will be triggered.
  // depth_process_block does not control autocal in current implementation.
  // Whenever there is a YUV buffer in the process block request, it will
  // trigger the AutoCal. So the condition is completely controlled by
  // rt_request_processor and result_request_processor.
  block->rgb_ir_auto_cal_enabled_ =
      property_get_bool("vendor.camera.frontdepth.enableautocal", true);

  return block;
}

status_t DepthProcessBlock::InitializeBufferManagementStatus(
    CameraDeviceSessionHwl* device_session_hwl) {
  // Query characteristics to check if buffer management supported
  std::unique_ptr<google_camera_hal::HalCameraMetadata> characteristics;
  status_t res = device_session_hwl->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: Get camera characteristics failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  camera_metadata_ro_entry entry = {};
  res = characteristics->Get(ANDROID_INFO_SUPPORTED_BUFFER_MANAGEMENT_VERSION,
                             &entry);
  if (res == OK && entry.count > 0) {
    buffer_management_supported_ =
        (entry.data.u8[0] >=
         ANDROID_INFO_SUPPORTED_BUFFER_MANAGEMENT_VERSION_HIDL_DEVICE_3_5);
  }

  return OK;
}

DepthProcessBlock::DepthProcessBlock(
    HwlRequestBuffersFunc request_stream_buffers,
    const DepthProcessBlockCreateData& create_data)
    : request_stream_buffers_(request_stream_buffers),
      rgb_internal_yuv_stream_id_(create_data.rgb_internal_yuv_stream_id),
      ir1_internal_raw_stream_id_(create_data.ir1_internal_raw_stream_id),
      ir2_internal_raw_stream_id_(create_data.ir2_internal_raw_stream_id) {
}

DepthProcessBlock::~DepthProcessBlock() {
  ATRACE_CALL();
  depth_generator_ = nullptr;

  if (depth_generator_lib_handle_ != nullptr) {
    dlclose(depth_generator_lib_handle_);
    depth_generator_lib_handle_ = nullptr;
  }
}

status_t DepthProcessBlock::SetResultProcessor(
    std::unique_ptr<ResultProcessor> result_processor) {
  ATRACE_CALL();
  if (result_processor == nullptr) {
    ALOGE("%s: result_processor is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(result_processor_lock_);
  if (result_processor_ != nullptr) {
    ALOGE("%s: result_processor_ was already set.", __FUNCTION__);
    return ALREADY_EXISTS;
  }

  result_processor_ = std::move(result_processor);
  return OK;
}

status_t DepthProcessBlock::GetStreamBufferSize(const Stream& stream,
                                                int32_t* buffer_size) {
  ATRACE_CALL();
  // TODO(b/130764929): Use actual gralloc buffer stride instead of stream dim
  switch (stream.format) {
    case HAL_PIXEL_FORMAT_Y8:
      *buffer_size = stream.width * stream.height;
      break;
    case HAL_PIXEL_FORMAT_Y16:
      *buffer_size = stream.width * stream.height * 2;
      break;
    case HAL_PIXEL_FORMAT_YCBCR_420_888:
      *buffer_size = static_cast<int32_t>(stream.width * stream.height * 1.5);
      break;
    default:
      ALOGW("%s: Unsupported format:%d", __FUNCTION__, stream.format);
      *buffer_size = 0;
      break;
  }

  return OK;
}

status_t DepthProcessBlock::ConfigureStreams(
    const StreamConfiguration& stream_config,
    const StreamConfiguration& /*overall_config*/) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(configure_lock_);
  if (is_configured_) {
    ALOGE("%s: Already configured.", __FUNCTION__);
    return ALREADY_EXISTS;
  }

  // TODO(b/128633958): remove this after FLL syncing is verified
  if (force_internal_stream_) {
    // Nothing to configure if this is force internal mode
    ALOGV("%s: Force internal enabled, skip depth block config.", __FUNCTION__);
    is_configured_ = true;
    return OK;
  }

  uint32_t num_depth_stream = 0;
  for (auto& stream : stream_config.streams) {
    if (utils::IsDepthStream(stream)) {
      num_depth_stream++;
      // Save depth stream as HAL configured stream
      depth_stream_.id = stream.id;
      depth_stream_.override_format = stream.format;
      depth_stream_.producer_usage = GRALLOC1_PRODUCER_USAGE_CAMERA;
      depth_stream_.consumer_usage = 0;
      depth_stream_.max_buffers = kDepthStreamMaxBuffers;
      depth_stream_.override_data_space = stream.data_space;
      depth_stream_.is_physical_camera_stream = false;
      depth_stream_.physical_camera_id = 0;
    }

    // Save stream information for mapping purposes
    depth_io_streams_[stream.id] = stream;
    int32_t buffer_size = 0;
    status_t res = GetStreamBufferSize(stream, &buffer_size);
    if (res != OK) {
      ALOGE("%s: Failed to get stream buffer size.", __FUNCTION__);
      return res;
    }
    stream_buffer_sizes_[stream.id] = buffer_size;
  }

  if (num_depth_stream != 1) {
    ALOGE(
        "%s: Depth Process Block can only config 1 depth stream. There are "
        "%zu streams, including %u depth stream.",
        __FUNCTION__, stream_config.streams.size(), num_depth_stream);
    return BAD_VALUE;
  }

  if (depth_generator_ == nullptr) {
    status_t res = LoadDepthGenerator(&depth_generator_);
    if (res != OK) {
      ALOGE("%s: Creating DepthGenerator failed.", __FUNCTION__);
      return NO_INIT;
    }

    if (pipelined_depth_engine_enabled_ == true) {
      auto depth_result_callback =
          android::depth_generator::DepthResultCallbackFunction(
              [this](DepthResultStatus result_status, uint32_t frame_number) {
                status_t res = ProcessDepthResult(result_status, frame_number);
                if (res != OK) {
                  ALOGE("%s: Failed to process the depth result for frame %d.",
                        __FUNCTION__, frame_number);
                }
              });
      ALOGI("%s: Async depth api is used. Callback func is set.", __FUNCTION__);
      depth_generator_->SetResultCallback(depth_result_callback);
    } else {
      ALOGI("%s: Blocking depth api is used.", __FUNCTION__);
      depth_generator_->SetResultCallback(nullptr);
    }
  }

  is_configured_ = true;
  return OK;
}

status_t DepthProcessBlock::GetConfiguredHalStreams(
    std::vector<HalStream>* hal_streams) const {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(configure_lock_);
  if (hal_streams == nullptr) {
    ALOGE("%s: hal_streams is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (!is_configured_) {
    ALOGE("%s: Not configured yet.", __FUNCTION__);
    return NO_INIT;
  }

  hal_streams->push_back(depth_stream_);

  return OK;
}

status_t DepthProcessBlock::SubmitBlockingDepthRequest(
    const DepthRequestInfo& request_info) {
  ALOGV("%s: [ud] ExecuteProcessRequest for frame %d", __FUNCTION__,
        request_info.frame_number);

  status_t res = depth_generator_->ExecuteProcessRequest(request_info);
  if (res != OK) {
    ALOGE("%s: Depth generator fails to process frame %d.", __FUNCTION__,
          request_info.frame_number);
    return res;
  }

  res = ProcessDepthResult(DepthResultStatus::kOk, request_info.frame_number);
  if (res != OK) {
    ALOGE("%s: Failed to process depth result.", __FUNCTION__);
    return res;
  }

  return OK;
}

status_t DepthProcessBlock::SubmitAsyncDepthRequest(
    const DepthRequestInfo& request_info) {
  std::unique_lock<std::mutex> lock(depth_generator_api_lock_);
  ALOGV("%s: [ud] ExecuteProcessRequest for frame %d", __FUNCTION__,
        request_info.frame_number);
  status_t res = depth_generator_->EnqueueProcessRequest(request_info);
  if (res != OK) {
    ALOGE("%s: Failed to enqueue depth request.", __FUNCTION__);
    return res;
  }

  return OK;
}

status_t DepthProcessBlock::ProcessDepthResult(DepthResultStatus result_status,
                                               uint32_t frame_number) {
  std::unique_lock<std::mutex> lock(depth_generator_api_lock_);
  ALOGV("%s: [ud] Depth result for frame %u notified.", __FUNCTION__,
        frame_number);

  status_t res = UnmapDepthRequestBuffers(frame_number);
  if (res != OK) {
    ALOGE("%s: Failed to clean up the depth request info.", __FUNCTION__);
    return res;
  }

  auto capture_result = std::make_unique<CaptureResult>();
  if (capture_result == nullptr) {
    ALOGE("%s: Creating capture_result failed.", __FUNCTION__);
    return NO_MEMORY;
  }

  CaptureRequest request;
  {
    std::lock_guard<std::mutex> pending_request_lock(pending_requests_mutex_);
    if (pending_depth_requests_.find(frame_number) ==
        pending_depth_requests_.end()) {
      ALOGE("%s: Frame %u does not exist in pending requests list.",
            __FUNCTION__, frame_number);
    } else {
      auto& request = pending_depth_requests_[frame_number].request;
      capture_result->frame_number = frame_number;
      capture_result->output_buffers = request.output_buffers;

      // In case the depth engine fails to process a depth request, mark the
      // buffer as in error state.
      if (result_status != DepthResultStatus::kOk) {
        for (auto& stream_buffer : capture_result->output_buffers) {
          if (stream_buffer.stream_id == depth_stream_.id) {
            stream_buffer.status = BufferStatus::kError;
          }
        }
      }

      capture_result->input_buffers = request.input_buffers;
      pending_depth_requests_.erase(frame_number);
    }
  }

  ProcessBlockResult block_result = {.request_id = 0,
                                     .result = std::move(capture_result)};
  {
    std::lock_guard<std::mutex> lock(result_processor_lock_);
    result_processor_->ProcessResult(std::move(block_result));
  }

  return OK;
}

status_t DepthProcessBlock::ProcessRequests(
    const std::vector<ProcessBlockRequest>& process_block_requests,
    const CaptureRequest& remaining_session_request) {
  ATRACE_CALL();
  // TODO(b/128633958): remove this after FLL syncing is verified
  if (force_internal_stream_) {
    // Nothing to configure if this is force internal mode
    ALOGE("%s: Force internal ON, Depth PB should not process request.",
          __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  std::lock_guard<std::mutex> lock(configure_lock_);
  if (!is_configured_) {
    ALOGE("%s: block is not configured.", __FUNCTION__);
    return NO_INIT;
  }

  if (process_block_requests.size() != 1) {
    ALOGE("%s: Only a single request is supported but there are %zu",
          __FUNCTION__, process_block_requests.size());
    return BAD_VALUE;
  }

  {
    std::lock_guard<std::mutex> lock(result_processor_lock_);
    if (result_processor_ == nullptr) {
      ALOGE("%s: result processor was not set.", __FUNCTION__);
      return NO_INIT;
    }

    status_t res = result_processor_->AddPendingRequests(
        process_block_requests, remaining_session_request);
    if (res != OK) {
      ALOGE("%s: Adding a pending request to result processor failed: %s(%d)",
            __FUNCTION__, strerror(-res), res);
      return res;
    }
  }

  auto& request = process_block_requests[0].request;
  DepthRequestInfo request_info;
  request_info.frame_number = request.frame_number;
  std::unique_ptr<HalCameraMetadata> metadata = nullptr;
  if (request.settings != nullptr) {
    metadata = HalCameraMetadata::Clone(request.settings.get());
  }

  std::unique_ptr<HalCameraMetadata> color_metadata = nullptr;
  for (auto& metadata : request.input_buffer_metadata) {
    if (metadata != nullptr) {
      color_metadata = HalCameraMetadata::Clone(metadata.get());
    }
  }

  ALOGV("%s: [ud] Prepare depth request info for frame %u .", __FUNCTION__,
        request.frame_number);

  status_t res = PrepareDepthRequestInfo(request, &request_info, metadata.get(),
                                         color_metadata.get());
  if (res != OK) {
    ALOGE("%s: Failed to perpare the depth request info.", __FUNCTION__);
    return res;
  }

  if (pipelined_depth_engine_enabled_ == true) {
    res = SubmitAsyncDepthRequest(request_info);
    if (res != OK) {
      ALOGE("%s: Failed to submit asynchronized depth request.", __FUNCTION__);
    }
  } else {
    res = SubmitBlockingDepthRequest(request_info);
    if (res != OK) {
      ALOGE("%s: Failed to submit blocking depth request.", __FUNCTION__);
    }
  }

  return OK;
}

status_t DepthProcessBlock::Flush() {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(configure_lock_);
  if (!is_configured_) {
    return OK;
  }

  // TODO(b/127322570): Implement this method.
  return OK;
}

status_t DepthProcessBlock::LoadDepthGenerator(
    std::unique_ptr<DepthGenerator>* depth_generator) {
  ATRACE_CALL();
  CreateDepthGenerator_t create_depth_generator;

  ALOGI("%s: Loading library: %s", __FUNCTION__, kDepthGeneratorLib.c_str());
  depth_generator_lib_handle_ =
      dlopen(kDepthGeneratorLib.c_str(), RTLD_NOW | RTLD_NODELETE);
  if (depth_generator_lib_handle_ == nullptr) {
    ALOGE("Depth generator loading %s failed.", kDepthGeneratorLib.c_str());
    return NO_INIT;
  }

  create_depth_generator = (CreateDepthGenerator_t)dlsym(
      depth_generator_lib_handle_, "CreateDepthGenerator");
  if (create_depth_generator == nullptr) {
    ALOGE("%s: dlsym failed (%s).", __FUNCTION__, kDepthGeneratorLib.c_str());
    dlclose(depth_generator_lib_handle_);
    depth_generator_lib_handle_ = nullptr;
    return NO_INIT;
  }

  *depth_generator = std::unique_ptr<DepthGenerator>(create_depth_generator());
  if (*depth_generator == nullptr) {
    return NO_INIT;
  }

  return OK;
}

status_t DepthProcessBlock::MapBuffersForDepthGenerator(
    const StreamBuffer& stream_buffer, depth_generator::Buffer* buffer) {
  ATRACE_CALL();
  buffer_handle_t buffer_handle = stream_buffer.buffer;
  ALOGV("%s: Mapping FD=%d to CPU addr.", __FUNCTION__, buffer_handle->data[0]);

  int32_t stream_id = stream_buffer.stream_id;
  if (stream_buffer_sizes_.find(stream_id) == stream_buffer_sizes_.end() ||
      depth_io_streams_.find(stream_id) == depth_io_streams_.end()) {
    ALOGE("%s: Stream buffer stream id:%d not found.", __FUNCTION__, stream_id);
    return UNKNOWN_ERROR;
  }

  void* virtual_addr =
      mmap(NULL, stream_buffer_sizes_[stream_id], (PROT_READ | PROT_WRITE),
           MAP_SHARED, buffer_handle->data[0], 0);

  if (virtual_addr == nullptr || virtual_addr == reinterpret_cast<void*>(-1)) {
    ALOGE("%s: Failed to map the stream buffer to virtual addr.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  auto& stream = depth_io_streams_[stream_id];
  buffer->format = stream.format;
  buffer->width = stream.width;
  buffer->height = stream.height;
  depth_generator::BufferPlane buffer_plane = {};
  buffer_plane.addr = reinterpret_cast<uint8_t*>(virtual_addr);
  // TODO(b/130764929): Use actual gralloc buffer stride instead of stream dim
  buffer_plane.stride = stream.width;
  buffer_plane.scanline = stream.height;
  buffer->planes.push_back(buffer_plane);

  return OK;
}

status_t DepthProcessBlock::UnmapBuffersForDepthGenerator(
    const StreamBuffer& stream_buffer, uint8_t* addr) {
  ATRACE_CALL();
  if (addr == nullptr) {
    ALOGE("%s: Addr is null.", __FUNCTION__);
    return BAD_VALUE;
  }

  int32_t stream_id = stream_buffer.stream_id;
  if (stream_buffer_sizes_.find(stream_id) == stream_buffer_sizes_.end() ||
      depth_io_streams_.find(stream_id) == depth_io_streams_.end()) {
    ALOGE("%s: Stream buffer stream id:%d not found.", __FUNCTION__, stream_id);
    return UNKNOWN_ERROR;
  }

  munmap(addr, stream_buffer_sizes_[stream_id]);
  return OK;
}

status_t DepthProcessBlock::RequestDepthStreamBuffer(
    StreamBuffer* incomplete_buffer, uint32_t frame_number) {
  if (!buffer_management_supported_) {
    return OK;
  }

  if (request_stream_buffers_ == nullptr) {
    ALOGE("%s: request_stream_buffers_ is nullptr", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  std::vector<StreamBuffer> buffers;
  {
    status_t res = request_stream_buffers_(
        incomplete_buffer->stream_id,
        /* request one depth buffer each time */ 1, &buffers, frame_number);
    if (res != OK) {
      ALOGE("%s: Failed to request stream buffers from camera device session.",
            __FUNCTION__);
      return UNKNOWN_ERROR;
    }
  }

  *incomplete_buffer = buffers[0];
  return OK;
}

status_t DepthProcessBlock::UpdateCropRegion(const CaptureRequest& request,
                                             DepthRequestInfo* depth_request_info,
                                             HalCameraMetadata* metadata) {
  if (request.settings != nullptr && metadata != nullptr) {
    camera_metadata_ro_entry_t entry_crop_region_user = {};
    if (request.settings->Get(ANDROID_SCALER_CROP_REGION,
                              &entry_crop_region_user) == OK) {
      const int32_t* crop_region = entry_crop_region_user.data.i32;
      ALOGV("%s: Depth PB crop region[%d %d %d %d]", __FUNCTION__,
            crop_region[0], crop_region[1], crop_region[2], crop_region[3]);

      int32_t resized_crop_region[4] = {};
      // top
      resized_crop_region[0] = crop_region[1] / logical_to_ir_ratio_;
      if (resized_crop_region[0] < 0) {
        resized_crop_region[0] = 0;
      }
      // left
      resized_crop_region[1] = crop_region[0] / logical_to_ir_ratio_;
      if (resized_crop_region[1] < 0) {
        resized_crop_region[1] = 0;
      }
      // bottom
      resized_crop_region[2] =
          (crop_region[3] / logical_to_ir_ratio_) + resized_crop_region[0];
      if (resized_crop_region[2] > ir_active_array_height_) {
        resized_crop_region[2] = ir_active_array_height_;
      }
      // right
      resized_crop_region[3] =
          (crop_region[2] / logical_to_ir_ratio_) + resized_crop_region[1];
      if (resized_crop_region[3] > ir_active_array_width_) {
        resized_crop_region[3] = ir_active_array_width_;
      }
      metadata->Set(ANDROID_SCALER_CROP_REGION, resized_crop_region,
                    sizeof(resized_crop_region) / sizeof(int32_t));

      depth_request_info->settings = metadata->GetRawCameraMetadata();
    }
  }
  return OK;
}

status_t DepthProcessBlock::MapDepthRequestBuffers(
    const CaptureRequest& request, DepthRequestInfo* depth_request_info) {
  status_t res = OK;
  depth_request_info->ir_buffer.resize(2);
  for (auto& input_buffer : request.input_buffers) {
    // If the stream id is invalid. The input buffer is only a place holder
    // corresponding to the input buffer metadata for the rgb pipeline.
    if (input_buffer.stream_id == kInvalidStreamId) {
      ALOGV("%s: Skipping input buffer place holder for frame %u.",
            __FUNCTION__, depth_request_info->frame_number);
      continue;
    }

    depth_generator::Buffer buffer = {};
    res = MapBuffersForDepthGenerator(input_buffer, &buffer);
    if (res != OK) {
      ALOGE("%s: Mapping buffer for depth generator failed.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }
    const int32_t stream_id = input_buffer.stream_id;
    if (stream_id == rgb_internal_yuv_stream_id_) {
      // TODO(b/129910835): Triggering Condition
      // Adjust the condition according to how rt_request_processor and
      // result_request_processor handles the triggering condition. If they have
      // full control of the logic and decide to pass yuv buffer only when
      // autocal should be triggered, then the logic here can be as simple as
      // this.
      depth_request_info->color_buffer.push_back(buffer);
    } else if (stream_id == ir1_internal_raw_stream_id_) {
      depth_request_info->ir_buffer[0].push_back(buffer);
    } else if (stream_id == ir2_internal_raw_stream_id_) {
      depth_request_info->ir_buffer[1].push_back(buffer);
    }
  }

  res = MapBuffersForDepthGenerator(request.output_buffers[0],
                                    &depth_request_info->depth_buffer);
  if (res != OK) {
    ALOGE("%s: Mapping depth buffer for depth generator failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  return OK;
}

status_t DepthProcessBlock::PrepareDepthRequestInfo(
    const CaptureRequest& request, DepthRequestInfo* depth_request_info,
    HalCameraMetadata* metadata, const HalCameraMetadata* color_metadata) {
  ATRACE_CALL();

  if (depth_request_info == nullptr) {
    ALOGE("%s: depth_request_info is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res = UpdateCropRegion(request, depth_request_info, metadata);
  if (res != OK) {
    ALOGE("%s: Failed to update crop region.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  if (color_metadata != nullptr) {
    depth_request_info->color_buffer_metadata =
        color_metadata->GetRawCameraMetadata();
  }

  if (request.input_buffers.size() < 2 || request.input_buffers.size() > 3 ||
      request.output_buffers.size() != 1) {
    ALOGE(
        "%s: Cannot prepare request info, input buffer size is not 2 or 3(is"
        " %zu) or output buffer size is not 1(is %zu).",
        __FUNCTION__, request.input_buffers.size(),
        request.output_buffers.size());
    return BAD_VALUE;
  }

  if (buffer_management_supported_) {
    res = RequestDepthStreamBuffer(
        &(const_cast<CaptureRequest&>(request).output_buffers[0]),
        request.frame_number);
    if (res != OK) {
      ALOGE("%s: Failed to request depth stream buffer.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }
  }

  res = MapDepthRequestBuffers(request, depth_request_info);
  if (res != OK) {
    ALOGE("%s: Failed to map buffers for depth request.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  {
    uint32_t frame_number = request.frame_number;
    std::lock_guard<std::mutex> lock(pending_requests_mutex_);
    if (pending_depth_requests_.find(frame_number) !=
        pending_depth_requests_.end()) {
      ALOGE("%s: Frame %u already exists in pending requests.", __FUNCTION__,
            request.frame_number);
      return UNKNOWN_ERROR;
    } else {
      pending_depth_requests_[frame_number] = {};
      auto& pending_request = pending_depth_requests_[frame_number].request;
      pending_request.frame_number = frame_number;
      pending_request.input_buffers = request.input_buffers;
      pending_request.output_buffers = request.output_buffers;
      auto& pending_depth_request =
          pending_depth_requests_[frame_number].depth_request;
      pending_depth_request = *depth_request_info;
    }
  }

  return OK;
}

status_t DepthProcessBlock::UnmapDepthRequestBuffers(uint32_t frame_number) {
  std::lock_guard<std::mutex> lock(pending_requests_mutex_);
  if (pending_depth_requests_.find(frame_number) ==
      pending_depth_requests_.end()) {
    ALOGE("%s: Can not find frame %u in pending requests list.", __FUNCTION__,
          frame_number);
    return BAD_VALUE;
  }

  auto& request = pending_depth_requests_[frame_number].request;
  auto& depth_request_info = pending_depth_requests_[frame_number].depth_request;

  ATRACE_CALL();
  if (request.input_buffers.size() < 2 || request.input_buffers.size() > 3 ||
      request.output_buffers.size() != 1) {
    ALOGE(
        "%s: Cannot prepare request info, input buffer size is not 2 or 3(is "
        "%zu) or output buffer size is not 1(is %zu).",
        __FUNCTION__, request.input_buffers.size(),
        request.output_buffers.size());
    return BAD_VALUE;
  }

  status_t res = OK;
  for (auto& input_buffer : request.input_buffers) {
    uint8_t* addr = nullptr;
    int32_t stream_id = input_buffer.stream_id;
    if (stream_id == kInvalidStreamId) {
      ALOGV("%s: input buffer place holder found for frame %u", __FUNCTION__,
            frame_number);
      continue;
    }

    if (stream_id == rgb_internal_yuv_stream_id_) {
      addr = depth_request_info.color_buffer[0].planes[0].addr;
    } else if (stream_id == ir1_internal_raw_stream_id_) {
      addr = depth_request_info.ir_buffer[0][0].planes[0].addr;
    } else if (stream_id == ir2_internal_raw_stream_id_) {
      addr = depth_request_info.ir_buffer[1][0].planes[0].addr;
    }

    res = UnmapBuffersForDepthGenerator(input_buffer, addr);
    if (res != OK) {
      ALOGE("%s: Unmapping input buffer for depth generator failed.",
            __FUNCTION__);
      return UNKNOWN_ERROR;
    }
  }

  res = UnmapBuffersForDepthGenerator(
      request.output_buffers[0], depth_request_info.depth_buffer.planes[0].addr);
  if (res != OK) {
    ALOGE("%s: Unmapping depth buffer for depth generator failed.",
          __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  return OK;
}

status_t DepthProcessBlock::CalculateActiveArraySizeRatio(
    CameraDeviceSessionHwl* device_session_hwl) {
  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = device_session_hwl->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  uint32_t active_array_width = 0;
  uint32_t active_array_height = 0;
  camera_metadata_ro_entry entry;
  res = characteristics->Get(
      ANDROID_SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE, &entry);
  if (res == OK) {
    active_array_width = entry.data.i32[2];
    active_array_height = entry.data.i32[3];
    ALOGI("%s Active size (%d x %d).", __FUNCTION__, active_array_width,
          active_array_height);
  } else {
    ALOGE("%s Get active size failed: %s (%d).", __FUNCTION__, strerror(-res),
          res);
    return UNKNOWN_ERROR;
  }

  std::vector<uint32_t> physical_camera_ids =
      device_session_hwl->GetPhysicalCameraIds();
  if (physical_camera_ids.size() != 3) {
    ALOGE("%s: Only support 3 cameras", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  uint32_t ir_active_array_width = 0;
  uint32_t ir_active_array_height = 0;
  std::unique_ptr<HalCameraMetadata> ir_characteristics;
  for (auto camera_id : physical_camera_ids) {
    res = device_session_hwl->GetPhysicalCameraCharacteristics(
        camera_id, &ir_characteristics);
    if (res != OK) {
      ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
      return UNKNOWN_ERROR;
    }

    // assuming both IR camera are of the same size
    if (hal_utils::IsIrCamera(ir_characteristics.get())) {
      camera_metadata_ro_entry entry;
      res = ir_characteristics->Get(
          ANDROID_SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE, &entry);
      if (res == OK) {
        ir_active_array_width = entry.data.i32[2];
        ir_active_array_height = entry.data.i32[3];
        ALOGI("%s IR active size (%dx%d).", __FUNCTION__, ir_active_array_width,
              ir_active_array_height);
      } else {
        ALOGE("%s Get ir active size failed: %s (%d).", __FUNCTION__,
              strerror(-res), res);
        return UNKNOWN_ERROR;
      }
      break;
    }
  }

  if (active_array_width == 0 || active_array_height == 0 ||
      ir_active_array_width == 0 || ir_active_array_height == 0) {
    ALOGE(
        "%s: One dimension of the logical camera active array size or the "
        "IR camera active array size is 0.",
        __FUNCTION__);
    return INVALID_OPERATION;
  }

  float logical_aspect_ratio = 1.0;
  float ir_aspect_ratio = 1.0;
  if (active_array_width > active_array_height) {
    logical_aspect_ratio = active_array_width / active_array_height;
    ir_aspect_ratio = ir_active_array_width / ir_active_array_height;
  } else {
    logical_aspect_ratio = active_array_height / active_array_width;
    ir_aspect_ratio = ir_active_array_height / ir_active_array_width;
  }

  ir_active_array_height_ = ir_active_array_height;
  ir_active_array_width_ = ir_active_array_width;

  float aspect_ratio_diff = logical_aspect_ratio - ir_aspect_ratio;
  if (aspect_ratio_diff > kSmallOffset || aspect_ratio_diff < -kSmallOffset) {
    ALOGE(
        "%s: Logical camera aspect ratio and IR camera aspect ratio are "
        "different from each other.",
        __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  logical_to_ir_ratio_ = float(active_array_height) / ir_active_array_height;

  ALOGI("%s: logical_to_ir_ratio_ = %f", __FUNCTION__, logical_to_ir_ratio_);

  return OK;
}

}  // namespace google_camera_hal
}  // namespace android
