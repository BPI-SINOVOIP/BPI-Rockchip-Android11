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
#define LOG_TAG "GCH_HdrplusCaptureSession"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <cutils/properties.h>
#include <log/log.h>
#include <utils/Trace.h>

#include <inttypes.h>
#include <set>

#include "hal_utils.h"
#include "hdrplus_capture_session.h"
#include "hdrplus_process_block.h"
#include "hdrplus_request_processor.h"
#include "hdrplus_result_processor.h"
#include "realtime_process_block.h"
#include "realtime_zsl_request_processor.h"
#include "realtime_zsl_result_processor.h"
#include "vendor_tag_defs.h"

namespace android {
namespace google_camera_hal {
bool HdrplusCaptureSession::IsStreamConfigurationSupported(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& stream_config) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr", __FUNCTION__);
    return false;
  }

  uint32_t num_physical_cameras =
      device_session_hwl->GetPhysicalCameraIds().size();
  if (num_physical_cameras > 1) {
    ALOGD("%s: HdrplusCaptureSession doesn't support %u physical cameras",
          __FUNCTION__, num_physical_cameras);
    return false;
  }

  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = device_session_hwl->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (hal_utils::IsStreamHdrplusCompatible(stream_config,
                                           characteristics.get()) == false) {
    return false;
  }

  if (!hal_utils::IsBayerCamera(characteristics.get())) {
    ALOGD("%s: camera %d is not a bayer camera", __FUNCTION__,
          device_session_hwl->GetCameraId());
    return false;
  }

  ALOGI("%s: HDR+ is enabled", __FUNCTION__);
  ALOGD("%s: HdrplusCaptureSession supports the stream config", __FUNCTION__);
  return true;
}

std::unique_ptr<HdrplusCaptureSession> HdrplusCaptureSession::Create(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    HwlRequestBuffersFunc /*request_stream_buffers*/,
    std::vector<HalStream>* hal_configured_streams,
    CameraBufferAllocatorHwl* /*camera_allocator_hwl*/) {
  ATRACE_CALL();
  auto session =
      std::unique_ptr<HdrplusCaptureSession>(new HdrplusCaptureSession());
  if (session == nullptr) {
    ALOGE("%s: Creating HdrplusCaptureSession failed.", __FUNCTION__);
    return nullptr;
  }

  status_t res = session->Initialize(device_session_hwl, stream_config,
                                     process_capture_result, notify,
                                     hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Initializing HdrplusCaptureSession failed: %s (%d).",
          __FUNCTION__, strerror(-res), res);
    return nullptr;
  }

  return session;
}

HdrplusCaptureSession::~HdrplusCaptureSession() {
  ATRACE_CALL();
  if (device_session_hwl_ != nullptr) {
    device_session_hwl_->DestroyPipelines();
  }
}

status_t HdrplusCaptureSession::ConfigureStreams(
    const StreamConfiguration& stream_config,
    RequestProcessor* request_processor, ProcessBlock* process_block,
    int32_t* raw_stream_id) {
  ATRACE_CALL();
  if (request_processor == nullptr || process_block == nullptr ||
      raw_stream_id == nullptr) {
    ALOGE(
        "%s: request_processor (%p) or process_block (%p) is nullptr or "
        "raw_stream_id (%p) is nullptr",
        __FUNCTION__, request_processor, process_block, raw_stream_id);
    return BAD_VALUE;
  }

  StreamConfiguration process_block_stream_config;
  // Configure streams for request processor
  status_t res = request_processor->ConfigureStreams(
      internal_stream_manager_.get(), stream_config,
      &process_block_stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring stream for request processor failed.", __FUNCTION__);
    return res;
  }

  // Check all streams are configured.
  if (stream_config.streams.size() > process_block_stream_config.streams.size()) {
    ALOGE("%s: stream_config has %zu streams but only configured %zu streams",
          __FUNCTION__, stream_config.streams.size(),
          process_block_stream_config.streams.size());
    return UNKNOWN_ERROR;
  }

  for (auto& stream : stream_config.streams) {
    bool found = false;
    for (auto& configured_stream : process_block_stream_config.streams) {
      if (stream.id == configured_stream.id) {
        found = true;
        break;
      }
    }

    if (!found) {
      ALOGE("%s: Cannot find stream %u in configured streams.", __FUNCTION__,
            stream.id);
      return UNKNOWN_ERROR;
    }
  }

  for (auto& configured_stream : process_block_stream_config.streams) {
    if (configured_stream.format == kHdrplusRawFormat) {
      *raw_stream_id = configured_stream.id;
      break;
    }
  }

  if (*raw_stream_id == -1) {
    ALOGE("%s: Configuring stream fail due to wrong raw_stream_id",
          __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // Configure streams for process block.
  res = process_block->ConfigureStreams(process_block_stream_config,
                                        stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring stream for process block failed.", __FUNCTION__);
    return res;
  }

  return OK;
}

status_t HdrplusCaptureSession::ConfigureHdrplusStreams(
    const StreamConfiguration& stream_config,
    RequestProcessor* hdrplus_request_processor,
    ProcessBlock* hdrplus_process_block) {
  ATRACE_CALL();
  if (hdrplus_process_block == nullptr || hdrplus_request_processor == nullptr) {
    ALOGE("%s: hdrplus_process_block or hdrplus_request_processor is nullptr",
          __FUNCTION__);
    return BAD_VALUE;
  }

  StreamConfiguration process_block_stream_config;
  // Configure streams for request processor
  status_t res = hdrplus_request_processor->ConfigureStreams(
      internal_stream_manager_.get(), stream_config,
      &process_block_stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring stream for request processor failed.", __FUNCTION__);
    return res;
  }

  // Check all streams are configured.
  if (stream_config.streams.size() > process_block_stream_config.streams.size()) {
    ALOGE("%s: stream_config has %zu streams but only configured %zu streams",
          __FUNCTION__, stream_config.streams.size(),
          process_block_stream_config.streams.size());
    return UNKNOWN_ERROR;
  }

  for (auto& stream : stream_config.streams) {
    bool found = false;
    for (auto& configured_stream : process_block_stream_config.streams) {
      if (stream.id == configured_stream.id) {
        found = true;
        break;
      }
    }

    if (!found) {
      ALOGE("%s: Cannot find stream %u in configured streams.", __FUNCTION__,
            stream.id);
      return UNKNOWN_ERROR;
    }
  }

  // Configure streams for HDR+ process block.
  res = hdrplus_process_block->ConfigureStreams(process_block_stream_config,
                                                stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring hdrplus stream for process block failed.",
          __FUNCTION__);
    return res;
  }

  return OK;
}

status_t HdrplusCaptureSession::BuildPipelines(
    ProcessBlock* process_block, ProcessBlock* hdrplus_process_block,
    std::vector<HalStream>* hal_configured_streams) {
  ATRACE_CALL();
  if (process_block == nullptr || hal_configured_streams == nullptr) {
    ALOGE("%s: process_block (%p) or hal_configured_streams (%p) is nullptr",
          __FUNCTION__, process_block, hal_configured_streams);
    return BAD_VALUE;
  }

  status_t res = device_session_hwl_->BuildPipelines();
  if (res != OK) {
    ALOGE("%s: Building pipelines failed: %s(%d)", __FUNCTION__, strerror(-res),
          res);
    return res;
  }

  res = process_block->GetConfiguredHalStreams(hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Getting HAL streams failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  std::vector<HalStream> hdrplus_hal_configured_streams;
  res = hdrplus_process_block->GetConfiguredHalStreams(
      &hdrplus_hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Getting HDR+ HAL streams failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  // Combine realtime and HDR+ hal stream.
  // Only usage of internal raw stream is different, so combine usage directly
  uint64_t consumer_usage = 0;
  for (uint32_t i = 0; i < hdrplus_hal_configured_streams.size(); i++) {
    if (hdrplus_hal_configured_streams[i].override_format == kHdrplusRawFormat) {
      consumer_usage = hdrplus_hal_configured_streams[i].consumer_usage;
      break;
    }
  }

  for (uint32_t i = 0; i < hal_configured_streams->size(); i++) {
    if (hal_configured_streams->at(i).override_format == kHdrplusRawFormat) {
      hal_configured_streams->at(i).consumer_usage = consumer_usage;
      if (hal_configured_streams->at(i).max_buffers < kRawMinBufferCount) {
        hal_configured_streams->at(i).max_buffers = kRawMinBufferCount;
      }
      // Allocate internal raw stream buffers
      uint32_t additional_num_buffers =
          (hal_configured_streams->at(i).max_buffers >= kRawBufferCount)
              ? 0
              : (kRawBufferCount - hal_configured_streams->at(i).max_buffers);
      res = internal_stream_manager_->AllocateBuffers(
          hal_configured_streams->at(i), additional_num_buffers);
      if (res != OK) {
        ALOGE("%s: AllocateBuffers failed.", __FUNCTION__);
        return UNKNOWN_ERROR;
      }
      break;
    }
  }

  return OK;
}

status_t HdrplusCaptureSession::ConnectProcessChain(
    RequestProcessor* request_processor,
    std::unique_ptr<ProcessBlock> process_block,
    std::unique_ptr<ResultProcessor> result_processor) {
  ATRACE_CALL();
  if (request_processor == nullptr) {
    ALOGE("%s: request_processor is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res = process_block->SetResultProcessor(std::move(result_processor));
  if (res != OK) {
    ALOGE("%s: Setting result process in process block failed.", __FUNCTION__);
    return res;
  }

  res = request_processor->SetProcessBlock(std::move(process_block));
  if (res != OK) {
    ALOGE(
        "%s: Setting process block for HdrplusRequestProcessor failed: %s(%d)",
        __FUNCTION__, strerror(-res), res);
    return res;
  }

  return OK;
}

status_t HdrplusCaptureSession::SetupRealtimeProcessChain(
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    std::unique_ptr<ProcessBlock>* realtime_process_block,
    std::unique_ptr<ResultProcessor>* realtime_result_processor,
    int32_t* raw_stream_id) {
  ATRACE_CALL();
  if (realtime_process_block == nullptr ||
      realtime_result_processor == nullptr || raw_stream_id == nullptr) {
    ALOGE(
        "%s: realtime_process_block(%p) or realtime_result_processor(%p) or "
        "raw_stream_id(%p) is nullptr",
        __FUNCTION__, realtime_process_block, realtime_result_processor,
        raw_stream_id);
    return BAD_VALUE;
  }
  // Create realtime process block.
  auto process_block = RealtimeProcessBlock::Create(device_session_hwl_);
  if (process_block == nullptr) {
    ALOGE("%s: Creating RealtimeProcessBlock failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // Create realtime request processor.
  request_processor_ = RealtimeZslRequestProcessor::Create(device_session_hwl_);
  if (request_processor_ == nullptr) {
    ALOGE("%s: Creating RealtimeZslsRequestProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  status_t res = ConfigureStreams(stream_config, request_processor_.get(),
                                  process_block.get(), raw_stream_id);
  if (res != OK) {
    ALOGE("%s: Configuring stream failed: %s(%d)", __FUNCTION__, strerror(-res),
          res);
    return res;
  }

  // Create realtime result processor.
  auto result_processor = RealtimeZslResultProcessor::Create(
      internal_stream_manager_.get(), *raw_stream_id);
  if (result_processor == nullptr) {
    ALOGE("%s: Creating RealtimeZslResultProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  result_processor->SetResultCallback(process_capture_result, notify);

  *realtime_process_block = std::move(process_block);
  *realtime_result_processor = std::move(result_processor);

  return OK;
}

status_t HdrplusCaptureSession::SetupHdrplusProcessChain(
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    std::unique_ptr<ProcessBlock>* hdrplus_process_block,
    std::unique_ptr<ResultProcessor>* hdrplus_result_processor,
    int32_t raw_stream_id) {
  ATRACE_CALL();
  if (hdrplus_process_block == nullptr || hdrplus_result_processor == nullptr) {
    ALOGE(
        "%s: hdrplus_process_block(%p) or hdrplus_result_processor(%p) is "
        "nullptr",
        __FUNCTION__, hdrplus_process_block, hdrplus_result_processor);
    return BAD_VALUE;
  }

  // Create hdrplus process block.
  auto process_block = HdrplusProcessBlock::Create(
      device_session_hwl_, device_session_hwl_->GetCameraId());
  if (process_block == nullptr) {
    ALOGE("%s: Creating HdrplusProcessBlock failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // Create hdrplus request processor.
  hdrplus_request_processor_ = HdrplusRequestProcessor::Create(
      device_session_hwl_, raw_stream_id, device_session_hwl_->GetCameraId());
  if (hdrplus_request_processor_ == nullptr) {
    ALOGE("%s: Creating HdrplusRequestProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // Create hdrplus result processor.
  auto result_processor = HdrplusResultProcessor::Create(
      internal_stream_manager_.get(), raw_stream_id);
  if (result_processor == nullptr) {
    ALOGE("%s: Creating HdrplusResultProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  result_processor->SetResultCallback(process_capture_result, notify);

  status_t res = ConfigureHdrplusStreams(
      stream_config, hdrplus_request_processor_.get(), process_block.get());
  if (res != OK) {
    ALOGE("%s: Configuring hdrplus stream failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  *hdrplus_process_block = std::move(process_block);
  *hdrplus_result_processor = std::move(result_processor);

  return OK;
}

status_t HdrplusCaptureSession::Initialize(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    std::vector<HalStream>* hal_configured_streams) {
  ATRACE_CALL();
  if (!IsStreamConfigurationSupported(device_session_hwl, stream_config)) {
    ALOGE("%s: stream configuration is not supported.", __FUNCTION__);
    return BAD_VALUE;
  }

  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = device_session_hwl->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  res = characteristics->Get(VendorTagIds::kHdrUsageMode, &entry);
  if (res == OK) {
    hdr_mode_ = static_cast<HdrMode>(entry.data.u8[0]);
  }

  for (auto stream : stream_config.streams) {
    if (utils::IsPreviewStream(stream)) {
      hal_preview_stream_id_ = stream.id;
      break;
    }
  }
  device_session_hwl_ = device_session_hwl;
  internal_stream_manager_ = InternalStreamManager::Create();
  if (internal_stream_manager_ == nullptr) {
    ALOGE("%s: Cannot create internal stream manager.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // Create result dispatcher
  result_dispatcher_ =
      ResultDispatcher::Create(kPartialResult, process_capture_result, notify);
  if (result_dispatcher_ == nullptr) {
    ALOGE("%s: Cannot create result dispatcher.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  device_session_notify_ = notify;
  process_capture_result_ =
      ProcessCaptureResultFunc([this](std::unique_ptr<CaptureResult> result) {
        ProcessCaptureResult(std::move(result));
      });
  notify_ = NotifyFunc(
      [this](const NotifyMessage& message) { NotifyHalMessage(message); });

  // Setup realtime process chain
  int32_t raw_stream_id = -1;
  std::unique_ptr<ProcessBlock> realtime_process_block;
  std::unique_ptr<ResultProcessor> realtime_result_processor;

  res = SetupRealtimeProcessChain(stream_config, process_capture_result_,
                                  notify_, &realtime_process_block,
                                  &realtime_result_processor, &raw_stream_id);
  if (res != OK) {
    ALOGE("%s: SetupRealtimeProcessChain fail: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  // Setup hdrplus process chain
  std::unique_ptr<ProcessBlock> hdrplus_process_block;
  std::unique_ptr<ResultProcessor> hdrplus_result_processor;

  res = SetupHdrplusProcessChain(stream_config, process_capture_result_,
                                 notify_, &hdrplus_process_block,
                                 &hdrplus_result_processor, raw_stream_id);
  if (res != OK) {
    ALOGE("%s: SetupHdrplusProcessChain fail: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  // Realtime and HDR+ streams are configured
  // Start to build pipleline
  res = BuildPipelines(realtime_process_block.get(),
                       hdrplus_process_block.get(), hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Building pipelines failed: %s(%d)", __FUNCTION__, strerror(-res),
          res);
    return res;
  }

  res = PurgeHalConfiguredStream(stream_config, hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Removing internal streams from configured stream failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  // Connect realtime process chain
  res = ConnectProcessChain(request_processor_.get(),
                            std::move(realtime_process_block),
                            std::move(realtime_result_processor));
  if (res != OK) {
    ALOGE("%s: Connecting process chain failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  // Connect HDR+ process chain
  res = ConnectProcessChain(hdrplus_request_processor_.get(),
                            std::move(hdrplus_process_block),
                            std::move(hdrplus_result_processor));
  if (res != OK) {
    ALOGE("%s: Connecting HDR+ process chain failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  return OK;
}

status_t HdrplusCaptureSession::ProcessRequest(const CaptureRequest& request) {
  ATRACE_CALL();
  bool is_hdrplus_request =
      hal_utils::IsRequestHdrplusCompatible(request, hal_preview_stream_id_);

  status_t res = result_dispatcher_->AddPendingRequest(request);
  if (res != OK) {
    ALOGE("%s: frame(%d) fail to AddPendingRequest", __FUNCTION__,
          request.frame_number);
    return BAD_VALUE;
  }

  if (is_hdrplus_request) {
    ALOGI("%s: hdrplus snapshot (%d), output stream size:%zu", __FUNCTION__,
          request.frame_number, request.output_buffers.size());
    res = hdrplus_request_processor_->ProcessRequest(request);
    if (res != OK) {
      ALOGI("%s: hdrplus snapshot frame(%d) request to realtime process",
            __FUNCTION__, request.frame_number);
      res = request_processor_->ProcessRequest(request);
    }
  } else {
    res = request_processor_->ProcessRequest(request);
  }

  if (res != OK) {
    ALOGE("%s: ProcessRequest (%d) fail and remove pending request",
          __FUNCTION__, request.frame_number);
    result_dispatcher_->RemovePendingRequest(request.frame_number);
  }
  return res;
}

status_t HdrplusCaptureSession::Flush() {
  ATRACE_CALL();
  return request_processor_->Flush();
}

void HdrplusCaptureSession::ProcessCaptureResult(
    std::unique_ptr<CaptureResult> result) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  if (result == nullptr) {
    return;
  }

  if (result->result_metadata && hdr_mode_ != HdrMode::kHdrplusMode) {
    device_session_hwl_->FilterResultMetadata(result->result_metadata.get());
  }

  status_t res = result_dispatcher_->AddResult(std::move(result));
  if (res != OK) {
    ALOGE("%s: fail to AddResult", __FUNCTION__);
    return;
  }
}

status_t HdrplusCaptureSession::PurgeHalConfiguredStream(
    const StreamConfiguration& stream_config,
    std::vector<HalStream>* hal_configured_streams) {
  if (hal_configured_streams == nullptr) {
    ALOGE("%s: HAL configured stream list is null.", __FUNCTION__);
    return BAD_VALUE;
  }

  std::set<int32_t> framework_stream_id_set;
  for (auto& stream : stream_config.streams) {
    framework_stream_id_set.insert(stream.id);
  }

  std::vector<HalStream> configured_streams;
  for (auto& hal_stream : *hal_configured_streams) {
    if (framework_stream_id_set.find(hal_stream.id) !=
        framework_stream_id_set.end()) {
      configured_streams.push_back(hal_stream);
    }
  }
  *hal_configured_streams = configured_streams;
  return OK;
}

void HdrplusCaptureSession::NotifyHalMessage(const NotifyMessage& message) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  if (device_session_notify_ == nullptr) {
    ALOGE("%s: device_session_notify_ is nullptr. Dropping a message.",
          __FUNCTION__);
    return;
  }

  if (message.type == MessageType::kShutter) {
    status_t res =
        result_dispatcher_->AddShutter(message.message.shutter.frame_number,
                                       message.message.shutter.timestamp_ns);
    if (res != OK) {
      ALOGE("%s: AddShutter for frame %u failed: %s (%d).", __FUNCTION__,
            message.message.shutter.frame_number, strerror(-res), res);
      return;
    }
  } else if (message.type == MessageType::kError) {
    status_t res = result_dispatcher_->AddError(message.message.error);
    if (res != OK) {
      ALOGE("%s: AddError for frame %u failed: %s (%d).", __FUNCTION__,
            message.message.error.frame_number, strerror(-res), res);
      return;
    }
  } else {
    ALOGW("%s: Unsupported message type: %u", __FUNCTION__, message.type);
    device_session_notify_(message);
  }
}
}  // namespace google_camera_hal
}  // namespace android
