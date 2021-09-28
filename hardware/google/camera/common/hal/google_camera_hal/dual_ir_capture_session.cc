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
#define LOG_TAG "GCH_DualIrCaptureSession"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include <set>
#include <vector>

#include "dual_ir_capture_session.h"
#include "dual_ir_request_processor.h"
#include "dual_ir_result_request_processor.h"
#include "hal_utils.h"
#include "multicam_realtime_process_block.h"

namespace android {
namespace google_camera_hal {

bool DualIrCaptureSession::IsStreamConfigurationSupported(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& stream_config) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr", __FUNCTION__);
    return false;
  }

  std::vector<uint32_t> physical_camera_ids =
      device_session_hwl->GetPhysicalCameraIds();
  if (physical_camera_ids.size() != 2) {
    ALOGD("%s: Only support two IR cameras but there are %zu cameras.",
          __FUNCTION__, physical_camera_ids.size());
    return false;
  }

  // Check the two physical cameras are IR cameras.
  for (auto id : physical_camera_ids) {
    std::unique_ptr<HalCameraMetadata> characteristics;
    status_t res = device_session_hwl->GetPhysicalCameraCharacteristics(
        id, &characteristics);
    if (res != OK) {
      ALOGE("%s: Cannot get physical camera characteristics for camera %u",
            __FUNCTION__, id);
      return false;
    }

    // TODO(b/129088371): Work around b/129088371 because current IR camera's
    // CFA is MONO instead of NIR.
    if (!hal_utils::IsIrCamera(characteristics.get()) &&
        !hal_utils::IsMonoCamera(characteristics.get())) {
      ALOGD("%s: camera %u is not an IR or MONO camera", __FUNCTION__, id);
      return false;
    }
  }

  uint32_t physical_stream_number = 0;
  uint32_t logical_stream_number = 0;
  for (auto& stream : stream_config.streams) {
    if (stream.is_physical_camera_stream) {
      physical_stream_number++;
    } else {
      logical_stream_number++;
    }
  }
  if (logical_stream_number > 0 && physical_stream_number > 0) {
    ALOGD("%s: can't support mixed logical and physical stream", __FUNCTION__);
    return false;
  }

  ALOGD("%s: DualIrCaptureSession supports the stream config", __FUNCTION__);
  return true;
}

std::unique_ptr<CaptureSession> DualIrCaptureSession::Create(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    HwlRequestBuffersFunc /*request_stream_buffers*/,
    std::vector<HalStream>* hal_configured_streams,
    CameraBufferAllocatorHwl* /*camera_allocator_hwl*/) {
  ATRACE_CALL();
  if (!IsStreamConfigurationSupported(device_session_hwl, stream_config)) {
    ALOGE("%s: stream configuration is not supported.", __FUNCTION__);
    return nullptr;
  }

  // TODO(b/129707250): Assume the first physical camera is the lead until
  // it's available in the static metadata.
  std::vector<uint32_t> physical_camera_ids =
      device_session_hwl->GetPhysicalCameraIds();
  uint32_t lead_camera_id = physical_camera_ids[0];

  // If stream configuration only contains follower physical streams, set
  // follower as lead.
  bool has_lead_camera_config = false;
  for (auto& stream : stream_config.streams) {
    if (!stream.is_physical_camera_stream ||
        (stream.is_physical_camera_stream &&
         stream.physical_camera_id == physical_camera_ids[0])) {
      has_lead_camera_config = true;
      break;
    }
  }
  if (!has_lead_camera_config) {
    lead_camera_id = physical_camera_ids[1];
  }

  auto session = std::unique_ptr<DualIrCaptureSession>(
      new DualIrCaptureSession(lead_camera_id));
  if (session == nullptr) {
    ALOGE("%s: Creating DualIrCaptureSession failed.", __FUNCTION__);
    return nullptr;
  }

  status_t res = session->Initialize(device_session_hwl, stream_config,
                                     process_capture_result, notify,
                                     hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Initializing DualIrCaptureSession failed: %s (%d).",
          __FUNCTION__, strerror(-res), res);
    return nullptr;
  }

  ALOGI("%s: Created a DualIrCaptureSession", __FUNCTION__);
  return session;
}

DualIrCaptureSession::DualIrCaptureSession(uint32_t lead_camera_id)
    : kLeadCameraId(lead_camera_id) {
}

DualIrCaptureSession::~DualIrCaptureSession() {
  ATRACE_CALL();
  if (device_session_hwl_ != nullptr) {
    device_session_hwl_->DestroyPipelines();
  }
}

bool DualIrCaptureSession::AreAllStreamsConfigured(
    const StreamConfiguration& stream_config,
    const StreamConfiguration& process_block_stream_config) const {
  ATRACE_CALL();
  // Check all streams are configured.
  if (stream_config.streams.size() !=
      process_block_stream_config.streams.size()) {
    ALOGE("%s: stream_config has %zu streams but only configured %zu streams",
          __FUNCTION__, stream_config.streams.size(),
          process_block_stream_config.streams.size());
    return false;
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
      return false;
    }
  }

  return true;
}

status_t DualIrCaptureSession::ConfigureStreams(
    RequestProcessor* request_processor, ProcessBlock* process_block,
    const StreamConfiguration& overall_config,
    const StreamConfiguration& stream_config,
    StreamConfiguration* process_block_stream_config) {
  ATRACE_CALL();
  if (request_processor == nullptr || process_block == nullptr) {
    ALOGE("%s: request_processor(%p) or process_block(%p) is nullptr",
          __FUNCTION__, request_processor, process_block);
    return BAD_VALUE;
  }

  status_t res = request_processor->ConfigureStreams(
      internal_stream_manager_.get(), stream_config,
      process_block_stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring stream for RequestProcessor failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  res = process_block->ConfigureStreams(*process_block_stream_config,
                                        overall_config);
  if (res != OK) {
    ALOGE("%s: Configuring streams for ProcessBlock failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  return OK;
}

status_t DualIrCaptureSession::ConnectProcessChain(
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
    ALOGE("%s: Setting process block for request processor failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  return OK;
}

status_t DualIrCaptureSession::PurgeHalConfiguredStream(
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

status_t DualIrCaptureSession::MakeDepthChainSegmentStreamConfig(
    const StreamConfiguration& /*stream_config*/,
    StreamConfiguration* rt_process_block_stream_config,
    StreamConfiguration* depth_chain_segment_stream_config) {
  if (depth_chain_segment_stream_config == nullptr ||
      rt_process_block_stream_config == nullptr) {
    ALOGE(
        "%s: depth_chain_segment_stream_config is nullptr or "
        "rt_process_block_stream_config is nullptr.",
        __FUNCTION__);
    return BAD_VALUE;
  }
  // TODO(b/131618554):
  // Actually implement this function to form a depth chain segment stream
  // config from the overall stream config and the streams mutli-camera realtime
  // process block configured.
  // This function signature may need to be changed.

  return OK;
}

status_t DualIrCaptureSession::SetupRealtimeSegment(
    const StreamConfiguration& stream_config,
    StreamConfiguration* process_block_stream_config,
    std::unique_ptr<MultiCameraRtProcessBlock>* rt_process_block,
    std::unique_ptr<DualIrResultRequestProcessor>* rt_result_request_processor) {
  request_processor_ =
      DualIrRequestProcessor::Create(device_session_hwl_, kLeadCameraId);
  if (request_processor_ == nullptr) {
    ALOGE("%s: Creating DualIrRtRequestProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  auto process_block = MultiCameraRtProcessBlock::Create(device_session_hwl_);
  if (process_block == nullptr) {
    ALOGE("%s: Creating MultiCameraRtProcessBlock failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  auto result_request_processor = DualIrResultRequestProcessor::Create(
      device_session_hwl_, stream_config, kLeadCameraId);
  if (result_request_processor == nullptr) {
    ALOGE("%s: Creating DualIrResultRequestProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  status_t res = ConfigureStreams(request_processor_.get(), process_block.get(),
                                  stream_config, stream_config,
                                  process_block_stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring streams failed: %s(%d).", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  *rt_process_block = std::move(process_block);
  *rt_result_request_processor = std::move(result_request_processor);
  return OK;
}

status_t DualIrCaptureSession::SetupDepthSegment(
    const StreamConfiguration& stream_config,
    StreamConfiguration* process_block_stream_config,
    DualIrResultRequestProcessor* rt_result_request_processor,
    std::unique_ptr<DepthProcessBlock>* depth_process_block,
    std::unique_ptr<DualIrDepthResultProcessor>* depth_result_processor) {
  DepthProcessBlock::DepthProcessBlockCreateData data = {};
  auto process_block =
      DepthProcessBlock::Create(device_session_hwl_, nullptr, data);
  if (process_block == nullptr) {
    ALOGE("%s: Creating DepthProcessBlock failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  auto result_processor =
      DualIrDepthResultProcessor::Create(internal_stream_manager_.get());
  if (result_processor == nullptr) {
    ALOGE("%s: Creating DualIrDepthResultProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  StreamConfiguration depth_pb_stream_config;
  StreamConfiguration depth_chain_segment_stream_config;
  status_t res = MakeDepthChainSegmentStreamConfig(
      stream_config, process_block_stream_config,
      &depth_chain_segment_stream_config);
  if (res != OK) {
    ALOGE("%s: Failed to make depth chain segment stream configuration: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  res = ConfigureStreams(rt_result_request_processor, process_block.get(),
                         stream_config, depth_chain_segment_stream_config,
                         &depth_pb_stream_config);
  if (res != OK) {
    ALOGE("%s: Failed to configure streams for the depth segment.",
          __FUNCTION__);
    return res;
  }

  // Append the streams configured by depth process block. So
  // process_block_stream_config contains all streams configured by both
  // realtime and depth process blocks
  process_block_stream_config->streams.insert(
      process_block_stream_config->streams.end(),
      depth_pb_stream_config.streams.begin(),
      depth_pb_stream_config.streams.end());

  *depth_process_block = std::move(process_block);
  *depth_result_processor = std::move(result_processor);

  return OK;
}

status_t DualIrCaptureSession::BuildPipelines(
    const StreamConfiguration& stream_config,
    std::vector<HalStream>* hal_configured_streams,
    MultiCameraRtProcessBlock* rt_process_block,
    DepthProcessBlock* depth_process_block) {
  status_t res = device_session_hwl_->BuildPipelines();
  if (res != OK) {
    ALOGE("%s: Building pipelines failed: %s(%d)", __FUNCTION__, strerror(-res),
          res);
    return res;
  }

  res = rt_process_block->GetConfiguredHalStreams(hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Getting HAL streams failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  if (has_depth_stream_) {
    std::vector<HalStream> depth_pb_configured_streams;
    res = depth_process_block->GetConfiguredHalStreams(
        &depth_pb_configured_streams);
    if (res != OK) {
      ALOGE("%s: Failed to get configured hal streams from DepthProcessBlock",
            __FUNCTION__);
      return UNKNOWN_ERROR;
    }
    // Depth Process Block can only configure one depth stream so far
    if (depth_pb_configured_streams.size() != 1) {
      ALOGE("%s: DepthProcessBlock configured more than one stream.",
            __FUNCTION__);
      return UNKNOWN_ERROR;
    }
    hal_configured_streams->push_back(depth_pb_configured_streams[0]);
  }

  res = PurgeHalConfiguredStream(stream_config, hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Removing internal streams from configured stream failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  return OK;
}

status_t DualIrCaptureSession::CreateProcessChain(
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    std::vector<HalStream>* hal_configured_streams) {
  ATRACE_CALL();

  // process_block_stream_config is used to collect all streams configured by
  // both realtime and the depth process blocks. This is used to verify if all
  // framework streams have been configured.
  StreamConfiguration process_block_stream_config;

  std::unique_ptr<MultiCameraRtProcessBlock> rt_process_block;
  std::unique_ptr<DualIrResultRequestProcessor> rt_result_request_processor;
  status_t res =
      SetupRealtimeSegment(stream_config, &process_block_stream_config,
                           &rt_process_block, &rt_result_request_processor);
  if (res != OK) {
    ALOGE("%s: Failed to setup the realtime segment of the process chain.",
          __FUNCTION__);
    return res;
  }

  // Create process block and result processor for Depth Process Chain Segment
  std::unique_ptr<DepthProcessBlock> depth_process_block;
  std::unique_ptr<DualIrDepthResultProcessor> depth_result_processor;
  if (has_depth_stream_) {
    status_t res =
        SetupDepthSegment(stream_config, &process_block_stream_config,
                          rt_result_request_processor.get(),
                          &depth_process_block, &depth_result_processor);
    if (res != OK) {
      ALOGE("%s: Failed to setup the depth segment of the process chain.",
            __FUNCTION__);
      return res;
    }
  }

  if (!AreAllStreamsConfigured(stream_config, process_block_stream_config)) {
    ALOGE("%s: Not all streams are configured!", __FUNCTION__);
    return INVALID_OPERATION;
  }

  res = BuildPipelines(stream_config, hal_configured_streams,
                       rt_process_block.get(), depth_process_block.get());
  if (res != OK) {
    ALOGE("%s: Failed to build pipelines.", __FUNCTION__);
    return res;
  }

  // Only connect the depth segment of the realtime process chain when depth
  // stream is configured
  if (has_depth_stream_) {
    depth_result_processor->SetResultCallback(process_capture_result, notify);
    res = ConnectProcessChain(rt_result_request_processor.get(),
                              std::move(depth_process_block),
                              std::move(depth_result_processor));
    if (res != OK) {
      ALOGE("%s: Connecting depth segment of realtime chain failed: %s(%d)",
            __FUNCTION__, strerror(-res), res);
      return res;
    }
  }

  rt_result_request_processor->SetResultCallback(process_capture_result, notify);
  res =
      ConnectProcessChain(request_processor_.get(), std::move(rt_process_block),
                          std::move(rt_result_request_processor));
  if (res != OK) {
    ALOGE("%s: Connecting process chain failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  return OK;
}

status_t DualIrCaptureSession::Initialize(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    std::vector<HalStream>* hal_configured_streams) {
  ATRACE_CALL();
  device_session_hwl_ = device_session_hwl;

  internal_stream_manager_ = InternalStreamManager::Create();
  if (internal_stream_manager_ == nullptr) {
    ALOGE("%s: Cannot create internal stream manager.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  for (auto& stream : stream_config.streams) {
    if (utils::IsDepthStream(stream)) {
      ALOGI("%s: Depth stream found in the stream config.", __FUNCTION__);
      has_depth_stream_ = true;
    }
  }

  status_t res = CreateProcessChain(stream_config, process_capture_result,
                                    notify, hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Creating the process  chain failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  return OK;
}

status_t DualIrCaptureSession::ProcessRequest(const CaptureRequest& request) {
  ATRACE_CALL();
  return request_processor_->ProcessRequest(request);
}

status_t DualIrCaptureSession::Flush() {
  ATRACE_CALL();
  return request_processor_->Flush();
}

}  // namespace google_camera_hal
}  // namespace android
