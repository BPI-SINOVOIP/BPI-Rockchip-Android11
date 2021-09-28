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
#define LOG_TAG "GCH_RgbirdCaptureSession"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <cutils/properties.h>
#include <log/log.h>
#include <utils/Trace.h>

#include <inttypes.h>
#include <set>

#include "basic_result_processor.h"
#include "depth_process_block.h"
#include "hal_utils.h"
#include "hdrplus_process_block.h"
#include "hdrplus_request_processor.h"
#include "hdrplus_result_processor.h"
#include "multicam_realtime_process_block.h"
#include "rgbird_capture_session.h"
#include "rgbird_depth_result_processor.h"
#include "rgbird_result_request_processor.h"
#include "rgbird_rt_request_processor.h"

namespace android {
namespace google_camera_hal {

bool RgbirdCaptureSession::IsStreamConfigurationSupported(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& /*stream_config*/) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: device_session_hwl is nullptr", __FUNCTION__);
    return false;
  }

  std::vector<uint32_t> physical_camera_ids =
      device_session_hwl->GetPhysicalCameraIds();
  if (physical_camera_ids.size() != 3) {
    ALOGD("%s: RgbirdCaptureSession doesn't support %zu physical cameras",
          __FUNCTION__, physical_camera_ids.size());
    return false;
  }

  // Check if this is a logical camera containing two IR cameras.
  uint32_t num_ir_camera = 0;
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
    if (hal_utils::IsIrCamera(characteristics.get()) ||
        hal_utils::IsMonoCamera(characteristics.get())) {
      num_ir_camera++;
    }
  }

  if (num_ir_camera != 2) {
    ALOGD("%s: RgbirdCaptureSession only supports 2 ir cameras", __FUNCTION__);
    return false;
  }

  ALOGD("%s: RgbirdCaptureSession supports the stream config", __FUNCTION__);
  return true;
}

std::unique_ptr<CaptureSession> RgbirdCaptureSession::Create(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    HwlRequestBuffersFunc request_stream_buffers,
    std::vector<HalStream>* hal_configured_streams,
    CameraBufferAllocatorHwl* /*camera_allocator_hwl*/) {
  ATRACE_CALL();
  auto session =
      std::unique_ptr<RgbirdCaptureSession>(new RgbirdCaptureSession());
  if (session == nullptr) {
    ALOGE("%s: Creating RgbirdCaptureSession failed.", __FUNCTION__);
    return nullptr;
  }

  status_t res = session->Initialize(
      device_session_hwl, stream_config, process_capture_result, notify,
      request_stream_buffers, hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Initializing RgbirdCaptureSession failed: %s (%d).",
          __FUNCTION__, strerror(-res), res);
    return nullptr;
  }

  return session;
}

RgbirdCaptureSession::~RgbirdCaptureSession() {
  if (device_session_hwl_ != nullptr) {
    device_session_hwl_->DestroyPipelines();
  }

  rt_request_processor_ = nullptr;
  hdrplus_request_processor_ = nullptr;
  result_dispatcher_ = nullptr;
}

bool RgbirdCaptureSession::AreAllStreamsConfigured(
    const StreamConfiguration& stream_config,
    const StreamConfiguration& process_block_stream_config) const {
  ATRACE_CALL();
  // Check all streams are configured.
  if (stream_config.streams.size() > process_block_stream_config.streams.size()) {
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

status_t RgbirdCaptureSession::ConfigureStreams(
    const StreamConfiguration& stream_config,
    RequestProcessor* request_processor, ProcessBlock* process_block,
    StreamConfiguration* process_block_stream_config) {
  ATRACE_CALL();
  if (request_processor == nullptr || process_block == nullptr ||
      process_block_stream_config == nullptr) {
    ALOGE(
        "%s: request_processor(%p) or process_block(%p) or "
        "process_block_stream_config(%p) is nullptr",
        __FUNCTION__, request_processor, process_block,
        process_block_stream_config);
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
                                        stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring streams for ProcessBlock failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  return OK;
}

status_t RgbirdCaptureSession::SetDepthInternalStreamId(
    const StreamConfiguration& process_block_stream_config,
    const StreamConfiguration& stream_config) {
  // Assuming there is at most one internal YUV stream configured when this
  // function is called(i.e. when depth stream is configured).
  for (auto& configured_stream : process_block_stream_config.streams) {
    if (configured_stream.format == HAL_PIXEL_FORMAT_YCBCR_420_888) {
      bool matching_found = false;
      for (auto& framework_stream : stream_config.streams) {
        if (configured_stream.id == framework_stream.id) {
          matching_found = true;
          break;
        }
      }
      if (!matching_found) {
        rgb_internal_yuv_stream_id_ = configured_stream.id;
      }
    } else if (configured_stream.format == HAL_PIXEL_FORMAT_Y8) {
      if (configured_stream.physical_camera_id == ir1_camera_id_) {
        ir1_internal_raw_stream_id_ = configured_stream.id;
      } else if (configured_stream.physical_camera_id == ir2_camera_id_) {
        ir2_internal_raw_stream_id_ = configured_stream.id;
      } else {
        ALOGV("%s: Y8 stream found from non-IR sensors.", __FUNCTION__);
      }
    }
  }

  if (rgb_internal_yuv_stream_id_ == kInvalidStreamId ||
      ir1_internal_raw_stream_id_ == kInvalidStreamId ||
      ir2_internal_raw_stream_id_ == kInvalidStreamId) {
    ALOGE(
        "%s: Internal YUV or IR stream not found in "
        "process_block_stream_config.",
        __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  return OK;
}

status_t RgbirdCaptureSession::ConfigureHdrplusRawStreamId(
    const StreamConfiguration& process_block_stream_config) {
  ATRACE_CALL();
  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = device_session_hwl_->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
    return BAD_VALUE;
  }

  uint32_t active_array_width, active_array_height;
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

  for (auto& configured_stream : process_block_stream_config.streams) {
    if (configured_stream.format == kHdrplusRawFormat &&
        configured_stream.width == active_array_width &&
        configured_stream.height == active_array_height) {
      rgb_raw_stream_id_ = configured_stream.id;
      break;
    }
  }

  if (rgb_raw_stream_id_ == -1) {
    ALOGE("%s: Configuring stream fail due to wrong raw_stream_id",
          __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  return OK;
}

status_t RgbirdCaptureSession::AllocateInternalBuffers(
    const StreamConfiguration& framework_stream_config,
    std::vector<HalStream>* hal_configured_streams,
    ProcessBlock* hdrplus_process_block) {
  ATRACE_CALL();
  status_t res = OK;

  std::set<int32_t> framework_stream_id_set;
  for (auto& stream : framework_stream_config.streams) {
    framework_stream_id_set.insert(stream.id);
  }

  for (uint32_t i = 0; i < hal_configured_streams->size(); i++) {
    HalStream& hal_stream = hal_configured_streams->at(i);

    if (framework_stream_id_set.find(hal_stream.id) ==
        framework_stream_id_set.end()) {
      // hdrplus rgb raw stream buffers is allocated separately
      if (hal_stream.id == rgb_raw_stream_id_) {
        continue;
      }

      uint32_t additional_num_buffers =
          (hal_stream.max_buffers >= kDefaultInternalBufferCount)
              ? 0
              : (kDefaultInternalBufferCount - hal_stream.max_buffers);
      res = internal_stream_manager_->AllocateBuffers(
          hal_stream, hal_stream.max_buffers + additional_num_buffers);
      if (res != OK) {
        ALOGE("%s: Failed to allocate buffer for internal stream %d: %s(%d)",
              __FUNCTION__, hal_stream.id, strerror(-res), res);
        return res;
      } else {
        ALOGI("%s: Allocating %d internal buffers for stream %d", __FUNCTION__,
              additional_num_buffers + hal_stream.max_buffers, hal_stream.id);
      }
    }
  }

  if (is_hdrplus_supported_) {
    std::vector<HalStream> hdrplus_hal_configured_streams;
    res = hdrplus_process_block->GetConfiguredHalStreams(
        &hdrplus_hal_configured_streams);
    if (res != OK) {
      ALOGE("%s: Getting HDR+ HAL streams failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }

    res = ConfigureHdrplusUsageAndBuffers(hal_configured_streams,
                                          &hdrplus_hal_configured_streams);
    if (res != OK) {
      ALOGE("%s: ConfigureHdrplusUsageAndBuffer failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }
  }
  return res;
}

status_t RgbirdCaptureSession::PurgeHalConfiguredStream(
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

bool RgbirdCaptureSession::NeedDepthProcessBlock() const {
  // TODO(b/128633958): remove force flag after FLL syncing is verified
  return force_internal_stream_ || has_depth_stream_;
}

status_t RgbirdCaptureSession::CreateDepthChainSegment(
    std::unique_ptr<DepthProcessBlock>* depth_process_block,
    std::unique_ptr<RgbirdDepthResultProcessor>* depth_result_processor,
    RgbirdResultRequestProcessor* rt_result_processor,
    const StreamConfiguration& stream_config,
    const StreamConfiguration& overall_config,
    StreamConfiguration* depth_block_stream_config) {
  ATRACE_CALL();
  DepthProcessBlock::DepthProcessBlockCreateData data = {
      .rgb_internal_yuv_stream_id = rgb_internal_yuv_stream_id_,
      .ir1_internal_raw_stream_id = ir1_internal_raw_stream_id_,
      .ir2_internal_raw_stream_id = ir2_internal_raw_stream_id_};
  auto process_block = DepthProcessBlock::Create(device_session_hwl_,
                                                 request_stream_buffers_, data);
  if (process_block == nullptr) {
    ALOGE("%s: Creating DepthProcessBlock failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  auto result_processor =
      RgbirdDepthResultProcessor::Create(internal_stream_manager_.get());
  if (result_processor == nullptr) {
    ALOGE("%s: Creating RgbirdDepthResultProcessor", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  status_t res = rt_result_processor->ConfigureStreams(
      internal_stream_manager_.get(), stream_config, depth_block_stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring streams for ResultRequestProcessor failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  res = process_block->ConfigureStreams(*depth_block_stream_config,
                                        overall_config);
  if (res != OK) {
    ALOGE("%s: Configuring streams for DepthProcessBlock failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  *depth_process_block = std::move(process_block);
  *depth_result_processor = std::move(result_processor);

  return OK;
}

status_t RgbirdCaptureSession::SetupDepthChainSegment(
    const StreamConfiguration& stream_config,
    RgbirdResultRequestProcessor* realtime_result_processor,
    std::unique_ptr<ProcessBlock>* depth_process_block,
    std::unique_ptr<ResultProcessor>* depth_result_processor,
    StreamConfiguration* rt_process_block_stream_config) {
  ATRACE_CALL();
  // Create the depth segment of realtime process chain if need depth processing
  std::unique_ptr<DepthProcessBlock> d_process_block;
  std::unique_ptr<RgbirdDepthResultProcessor> d_result_processor;
  if (NeedDepthProcessBlock()) {
    StreamConfiguration depth_chain_segment_stream_config;
    status_t res =
        MakeDepthStreamConfig(*rt_process_block_stream_config, stream_config,
                              &depth_chain_segment_stream_config);
    if (res != OK) {
      ALOGE(
          "%s: Making depth chain segment stream configuration failed: "
          "%s(%d).",
          __FUNCTION__, strerror(-res), res);
      return res;
    }

    StreamConfiguration depth_block_stream_config;
    res = CreateDepthChainSegment(&d_process_block, &d_result_processor,
                                  realtime_result_processor,
                                  depth_chain_segment_stream_config,
                                  stream_config, &depth_block_stream_config);
    if (res != OK) {
      ALOGE("%s: Creating depth chain segment failed: %s(%d).", __FUNCTION__,
            strerror(-res), res);
      return res;
    }

    // process_block_stream_config may contain internal streams(some may be
    // duplicated as both input and output for bridging the rt and depth
    // segments of the realtime process chain.)
    rt_process_block_stream_config->streams.insert(
        rt_process_block_stream_config->streams.end(),
        depth_block_stream_config.streams.begin(),
        depth_block_stream_config.streams.end());

    *depth_process_block = std::move(d_process_block);
    *depth_result_processor = std::move(d_result_processor);
  }

  return OK;
}

status_t RgbirdCaptureSession::MakeDepthStreamConfig(
    const StreamConfiguration& rt_process_block_stream_config,
    const StreamConfiguration& stream_config,
    StreamConfiguration* depth_stream_config) {
  ATRACE_CALL();
  if (depth_stream_config == nullptr) {
    ALOGE("%s: depth_stream_config is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  if (!NeedDepthProcessBlock()) {
    ALOGE("%s: No need to create depth process chain segment stream config.",
          __FUNCTION__);
    return BAD_VALUE;
  }

  // Assuming all internal streams must be for depth process block as input,
  // if depth stream is configured by framework.
  depth_stream_config->operation_mode = stream_config.operation_mode;
  depth_stream_config->session_params =
      HalCameraMetadata::Clone(stream_config.session_params.get());
  depth_stream_config->stream_config_counter =
      stream_config.stream_config_counter;
  depth_stream_config->streams = stream_config.streams;
  for (auto& stream : rt_process_block_stream_config.streams) {
    bool is_internal_stream = true;
    for (auto& framework_stream : stream_config.streams) {
      if (stream.id == framework_stream.id) {
        is_internal_stream = false;
        break;
      }
    }

    // Change all internal streams to input streams and keep others untouched
    if (is_internal_stream) {
      Stream input_stream = stream;
      input_stream.stream_type = StreamType::kInput;
      depth_stream_config->streams.push_back(input_stream);
    }
  }

  return OK;
}

status_t RgbirdCaptureSession::SetupRealtimeProcessChain(
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    std::unique_ptr<ProcessBlock>* realtime_process_block,
    std::unique_ptr<RgbirdResultRequestProcessor>* realtime_result_processor,
    std::unique_ptr<ProcessBlock>* depth_process_block,
    std::unique_ptr<ResultProcessor>* depth_result_processor) {
  ATRACE_CALL();
  if (realtime_process_block == nullptr ||
      realtime_result_processor == nullptr) {
    ALOGE("%s: realtime_process_block(%p) or realtime_result_processor(%p) or ",
          __FUNCTION__, realtime_process_block, realtime_result_processor);
    return BAD_VALUE;
  }

  auto rt_process_block = MultiCameraRtProcessBlock::Create(device_session_hwl_);
  if (rt_process_block == nullptr) {
    ALOGE("%s: Creating RealtimeProcessBlock failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // TODO(b/128632740): Create and connect depth process block.
  rt_request_processor_ = RgbirdRtRequestProcessor::Create(
      device_session_hwl_, is_hdrplus_supported_);
  if (rt_request_processor_ == nullptr) {
    ALOGE("%s: Creating RealtimeZslsRequestProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  StreamConfiguration process_block_stream_config;
  status_t res =
      ConfigureStreams(stream_config, rt_request_processor_.get(),
                       rt_process_block.get(), &process_block_stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring stream failed: %s(%d)", __FUNCTION__, strerror(-res),
          res);
    return res;
  }

  if (is_hdrplus_supported_) {
    res = ConfigureHdrplusRawStreamId(process_block_stream_config);
    if (res != OK) {
      ALOGE("%s: ConfigureHdrplusRawStreamId failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }
  }

  if (has_depth_stream_) {
    res = SetDepthInternalStreamId(process_block_stream_config, stream_config);
    if (res != OK) {
      ALOGE("%s: ConfigureDepthOnlyRawStreamId failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }
  }

  // Create realtime result processor.
  RgbirdResultRequestProcessor::RgbirdResultRequestProcessorCreateData data = {
      .rgb_camera_id = rgb_camera_id_,
      .ir1_camera_id = ir1_camera_id_,
      .ir2_camera_id = ir2_camera_id_,
      .rgb_raw_stream_id = rgb_raw_stream_id_,
      .is_hdrplus_supported = is_hdrplus_supported_,
      .rgb_internal_yuv_stream_id = rgb_internal_yuv_stream_id_};
  auto rt_result_processor = RgbirdResultRequestProcessor::Create(data);
  if (rt_result_processor == nullptr) {
    ALOGE("%s: Creating RgbirdResultRequestProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  rt_result_processor->SetResultCallback(process_capture_result, notify);

  if (is_hdrplus_supported_) {
    res = rt_result_processor->ConfigureStreams(internal_stream_manager_.get(),
                                                stream_config,
                                                &process_block_stream_config);
    if (res != OK) {
      ALOGE("%s: Configuring streams for ResultRequestProcessor failed: %s(%d)",
            __FUNCTION__, strerror(-res), res);
      return res;
    }
  }

  res = SetupDepthChainSegment(stream_config, rt_result_processor.get(),
                               depth_process_block, depth_result_processor,
                               &process_block_stream_config);
  if (res != OK) {
    ALOGE("%s: Failed to setup depth chain segment.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // TODO(b/128632740): Remove force internal flag after depth block is in place
  //                    and the FLL sync is verified.
  //                    This should be done after depth process block stream
  //                    configuration.
  if (!AreAllStreamsConfigured(stream_config, process_block_stream_config) &&
      !force_internal_stream_) {
    // TODO(b/127322570): Handle the case where RT request processor configures
    // internal streams for depth.
    ALOGE("%s: Not all streams are configured.", __FUNCTION__);
    return INVALID_OPERATION;
  }

  *realtime_process_block = std::move(rt_process_block);
  *realtime_result_processor = std::move(rt_result_processor);

  return OK;
}

status_t RgbirdCaptureSession::SetupHdrplusProcessChain(
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    std::unique_ptr<ProcessBlock>* hdrplus_process_block,
    std::unique_ptr<ResultProcessor>* hdrplus_result_processor) {
  ATRACE_CALL();
  if (hdrplus_process_block == nullptr || hdrplus_result_processor == nullptr) {
    ALOGE(
        "%s: hdrplus_process_block(%p) or hdrplus_result_processor(%p) is "
        "nullptr",
        __FUNCTION__, hdrplus_process_block, hdrplus_result_processor);
    return BAD_VALUE;
  }

  // Create hdrplus process block.
  std::vector<uint32_t> physical_camera_ids =
      device_session_hwl_->GetPhysicalCameraIds();
  // TODO: Check the static metadata and determine which one is rgb camera
  auto process_block =
      HdrplusProcessBlock::Create(device_session_hwl_, physical_camera_ids[0]);
  if (process_block == nullptr) {
    ALOGE("%s: Creating HdrplusProcessBlock failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // Create hdrplus request processor.
  hdrplus_request_processor_ = HdrplusRequestProcessor::Create(
      device_session_hwl_, rgb_raw_stream_id_, physical_camera_ids[0]);
  if (hdrplus_request_processor_ == nullptr) {
    ALOGE("%s: Creating HdrplusRequestProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // Create hdrplus result processor.
  auto result_processor = HdrplusResultProcessor::Create(
      internal_stream_manager_.get(), rgb_raw_stream_id_);
  if (result_processor == nullptr) {
    ALOGE("%s: Creating HdrplusResultProcessor failed.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  result_processor->SetResultCallback(process_capture_result, notify);

  StreamConfiguration process_block_stream_config;
  status_t res =
      ConfigureStreams(stream_config, hdrplus_request_processor_.get(),
                       process_block.get(), &process_block_stream_config);
  if (res != OK) {
    ALOGE("%s: Configuring hdrplus stream failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  *hdrplus_process_block = std::move(process_block);
  *hdrplus_result_processor = std::move(result_processor);

  return OK;
}

status_t RgbirdCaptureSession::CreateProcessChain(
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    std::vector<HalStream>* hal_configured_streams) {
  ATRACE_CALL();
  // Setup realtime process chain
  std::unique_ptr<ProcessBlock> realtime_process_block;
  std::unique_ptr<RgbirdResultRequestProcessor> realtime_result_processor;
  std::unique_ptr<ProcessBlock> depth_process_block;
  std::unique_ptr<ResultProcessor> depth_result_processor;

  status_t res = SetupRealtimeProcessChain(
      stream_config, process_capture_result, notify, &realtime_process_block,
      &realtime_result_processor, &depth_process_block, &depth_result_processor);
  if (res != OK) {
    ALOGE("%s: SetupRealtimeProcessChain fail: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  // Setup hdrplus process chain
  std::unique_ptr<ProcessBlock> hdrplus_process_block;
  std::unique_ptr<ResultProcessor> hdrplus_result_processor;
  if (is_hdrplus_supported_) {
    res = SetupHdrplusProcessChain(stream_config, process_capture_result,
                                   notify, &hdrplus_process_block,
                                   &hdrplus_result_processor);
    if (res != OK) {
      ALOGE("%s: SetupHdrplusProcessChain fail: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }
  }
  // Realtime and HDR+ streams are configured
  // Start to build pipleline
  res = BuildPipelines(stream_config, realtime_process_block.get(),
                       depth_process_block.get(), hdrplus_process_block.get(),
                       hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Building pipelines failed: %s(%d)", __FUNCTION__, strerror(-res),
          res);
    return res;
  }

  // Connecting the depth segment of the realtime process chain.
  if (NeedDepthProcessBlock()) {
    depth_result_processor->SetResultCallback(process_capture_result, notify);

    res = ConnectProcessChain(realtime_result_processor.get(),
                              std::move(depth_process_block),
                              std::move(depth_result_processor));
    if (res != OK) {
      ALOGE("%s: Connecting depth segment of realtime chain failed: %s(%d)",
            __FUNCTION__, strerror(-res), res);
      return res;
    }
  }

  // Connect realtime process chain
  res = ConnectProcessChain(rt_request_processor_.get(),
                            std::move(realtime_process_block),
                            std::move(realtime_result_processor));
  if (res != OK) {
    ALOGE("%s: Connecting process chain failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  if (is_hdrplus_supported_) {
    // Connect HDR+ process chain
    res = ConnectProcessChain(hdrplus_request_processor_.get(),
                              std::move(hdrplus_process_block),
                              std::move(hdrplus_result_processor));
    if (res != OK) {
      ALOGE("%s: Connecting HDR+ process chain failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }
  }
  return OK;
}

status_t RgbirdCaptureSession::ConnectProcessChain(
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

status_t RgbirdCaptureSession::ConfigureHdrplusUsageAndBuffers(
    std::vector<HalStream>* hal_configured_streams,
    std::vector<HalStream>* hdrplus_hal_configured_streams) {
  ATRACE_CALL();
  if (hal_configured_streams == nullptr ||
      hdrplus_hal_configured_streams == nullptr) {
    ALOGE(
        "%s: hal_configured_streams (%p) or hdrplus_hal_configured_streams "
        "(%p) is nullptr",
        __FUNCTION__, hal_configured_streams, hdrplus_hal_configured_streams);
    return BAD_VALUE;
  }
  // Combine realtime and HDR+ hal stream.
  // Only usage of internal raw stream is different, so combine usage directly
  uint64_t consumer_usage = 0;
  for (uint32_t i = 0; i < (*hdrplus_hal_configured_streams).size(); i++) {
    if (hdrplus_hal_configured_streams->at(i).override_format ==
            kHdrplusRawFormat &&
        hdrplus_hal_configured_streams->at(i).id == rgb_raw_stream_id_) {
      consumer_usage = hdrplus_hal_configured_streams->at(i).consumer_usage;
      break;
    }
  }

  for (uint32_t i = 0; i < hal_configured_streams->size(); i++) {
    if (hal_configured_streams->at(i).override_format == kHdrplusRawFormat &&
        hal_configured_streams->at(i).id == rgb_raw_stream_id_) {
      hal_configured_streams->at(i).consumer_usage = consumer_usage;
      // Allocate internal raw stream buffers
      if (hal_configured_streams->at(i).max_buffers < kRgbMinRawBufferCount) {
        hal_configured_streams->at(i).max_buffers = kRgbMinRawBufferCount;
      }

      uint32_t additional_num_buffers =
          (hal_configured_streams->at(i).max_buffers >= kRgbRawBufferCount)
              ? 0
              : (kRgbRawBufferCount - hal_configured_streams->at(i).max_buffers);
      status_t res = internal_stream_manager_->AllocateBuffers(
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

status_t RgbirdCaptureSession::BuildPipelines(
    const StreamConfiguration& stream_config,
    ProcessBlock* realtime_process_block, ProcessBlock* depth_process_block,
    ProcessBlock* hdrplus_process_block,
    std::vector<HalStream>* hal_configured_streams) {
  ATRACE_CALL();
  if (realtime_process_block == nullptr) {
    ALOGE("%s: realtime_process_block (%p) is nullptr", __FUNCTION__,
          realtime_process_block);
    return BAD_VALUE;
  }

  if (depth_process_block == nullptr && has_depth_stream_) {
    ALOGE("%s: depth_process_block (%p) is nullptr", __FUNCTION__,
          depth_process_block);
    return BAD_VALUE;
  }

  if (hal_configured_streams == nullptr) {
    ALOGE("%s: hal_configured_streams (%p) is nullptr", __FUNCTION__,
          hal_configured_streams);
    return BAD_VALUE;
  }

  if (is_hdrplus_supported_ && hdrplus_process_block == nullptr) {
    ALOGE("%s: hdrplus_process_block is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res = device_session_hwl_->BuildPipelines();
  if (res != OK) {
    ALOGE("%s: Building pipelines failed: %s(%d)", __FUNCTION__, strerror(-res),
          res);
    return res;
  }

  res = realtime_process_block->GetConfiguredHalStreams(hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Getting HAL streams failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  res = AllocateInternalBuffers(stream_config, hal_configured_streams,
                                hdrplus_process_block);

  // Need to update hal_configured_streams if there is a depth stream
  std::vector<HalStream> depth_streams;
  if (has_depth_stream_) {
    res = depth_process_block->GetConfiguredHalStreams(&depth_streams);
    if (res != OK) {
      ALOGE("%s: Failed to get configured hal streams from DepthProcessBlock",
            __FUNCTION__);
      return UNKNOWN_ERROR;
    }

    // Depth Process Block can only configure one depth stream so far
    if (depth_streams.size() != 1) {
      ALOGE("%s: DepthProcessBlock configured more than one stream.",
            __FUNCTION__);
      return UNKNOWN_ERROR;
    }

    hal_configured_streams->push_back(depth_streams[0]);
  }

  if (res != OK) {
    ALOGE("%s: Allocating buffer for internal stream managers failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  hal_utils::DumpHalConfiguredStreams(*hal_configured_streams,
                                      "hal_configured_streams BEFORE purge");

  // TODO(b/128633958): cover the streams Depth PB processes
  res = PurgeHalConfiguredStream(stream_config, hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Removing internal streams from configured stream failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  hal_utils::DumpHalConfiguredStreams(*hal_configured_streams,
                                      "hal_configured_streams AFTER purge");

  return OK;
}

status_t RgbirdCaptureSession::InitializeCameraIds(
    CameraDeviceSessionHwl* device_session_hwl) {
  ATRACE_CALL();
  if (device_session_hwl == nullptr) {
    ALOGE("%s: Device session hwl is null.", __FUNCTION__);
    return BAD_VALUE;
  }

  std::vector<uint32_t> physical_camera_ids =
      device_session_hwl->GetPhysicalCameraIds();
  if (physical_camera_ids.size() != 3) {
    ALOGE("%s: Failed to initialize camera ids. Only support 3 cameras",
          __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // TODO(b/127322570): Figure out physical camera IDs from static metadata.
  rgb_camera_id_ = physical_camera_ids[0];
  ir1_camera_id_ = physical_camera_ids[1];
  ir2_camera_id_ = physical_camera_ids[2];
  return OK;
}

status_t RgbirdCaptureSession::Initialize(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    HwlRequestBuffersFunc request_stream_buffers,
    std::vector<HalStream>* hal_configured_streams) {
  ATRACE_CALL();
  if (!IsStreamConfigurationSupported(device_session_hwl, stream_config)) {
    ALOGE("%s: stream configuration is not supported.", __FUNCTION__);
    return BAD_VALUE;
  }

  // TODO(b/128633958): remove this after FLL syncing is verified
  force_internal_stream_ =
      property_get_bool("persist.camera.rgbird.forceinternal", false);
  if (force_internal_stream_) {
    ALOGI("%s: Force creating internal streams for IR pipelines", __FUNCTION__);
  }

  device_session_hwl_ = device_session_hwl;
  internal_stream_manager_ = InternalStreamManager::Create();
  if (internal_stream_manager_ == nullptr) {
    ALOGE("%s: Cannot create internal stream manager.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res = device_session_hwl->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: GetCameraCharacteristics failed.", __FUNCTION__);
    return BAD_VALUE;
  }

  is_hdrplus_supported_ = hal_utils::IsStreamHdrplusCompatible(
      stream_config, characteristics.get());

  if (is_hdrplus_supported_) {
    for (auto stream : stream_config.streams) {
      if (utils::IsPreviewStream(stream)) {
        hal_preview_stream_id_ = stream.id;
        break;
      }
    }
  }

  // Create result dispatcher
  result_dispatcher_ =
      ResultDispatcher::Create(kPartialResult, process_capture_result, notify);
  if (result_dispatcher_ == nullptr) {
    ALOGE("%s: Cannot create result dispatcher.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // Reroute callback functions
  device_session_notify_ = notify;
  process_capture_result_ =
      ProcessCaptureResultFunc([this](std::unique_ptr<CaptureResult> result) {
        ProcessCaptureResult(std::move(result));
      });
  notify_ = NotifyFunc(
      [this](const NotifyMessage& message) { NotifyHalMessage(message); });
  request_stream_buffers_ = request_stream_buffers;

  // Initialize physical camera ids
  res = InitializeCameraIds(device_session_hwl_);
  if (res != OK) {
    ALOGE("%s: Initializing camera ids failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  for (auto& stream : stream_config.streams) {
    if (utils::IsDepthStream(stream)) {
      ALOGI("%s: Depth stream exists in the stream config.", __FUNCTION__);
      has_depth_stream_ = true;
    }
  }

  // Finally create the process chains
  res = CreateProcessChain(stream_config, process_capture_result_, notify_,
                           hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Creating the process  chain failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  return OK;
}

status_t RgbirdCaptureSession::ProcessRequest(const CaptureRequest& request) {
  ATRACE_CALL();
  bool is_hdrplus_request = false;
  if (is_hdrplus_supported_) {
    is_hdrplus_request =
        hal_utils::IsRequestHdrplusCompatible(request, hal_preview_stream_id_);
    // TODO: Check if request is HDR+ request when contains a depth buffer
  }

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
      res = rt_request_processor_->ProcessRequest(request);
    }
  } else {
    res = rt_request_processor_->ProcessRequest(request);
  }

  if (res != OK) {
    ALOGE("%s: ProcessRequest (%d) fail and remove pending request",
          __FUNCTION__, request.frame_number);
    result_dispatcher_->RemovePendingRequest(request.frame_number);
  }
  return res;
}

status_t RgbirdCaptureSession::Flush() {
  ATRACE_CALL();
  return rt_request_processor_->Flush();
}

void RgbirdCaptureSession::ProcessCaptureResult(
    std::unique_ptr<CaptureResult> result) {
  ATRACE_CALL();
  std::lock_guard<std::mutex> lock(callback_lock_);
  status_t res = result_dispatcher_->AddResult(std::move(result));
  if (res != OK) {
    ALOGE("%s: fail to AddResult", __FUNCTION__);
    return;
  }
}

void RgbirdCaptureSession::NotifyHalMessage(const NotifyMessage& message) {
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
      ALOGE("%s: frame(%d) fail to AddShutter", __FUNCTION__,
            message.message.shutter.frame_number);
      return;
    }
  } else if (message.type == MessageType::kError) {
    // drop the error notifications for the internal streams
    auto error_stream_id = message.message.error.error_stream_id;
    if (has_depth_stream_ &&
        message.message.error.error_code == ErrorCode::kErrorBuffer &&
        error_stream_id != kInvalidStreamId &&
        (error_stream_id == rgb_internal_yuv_stream_id_ ||
         error_stream_id == ir1_internal_raw_stream_id_ ||
         error_stream_id == ir2_internal_raw_stream_id_)) {
      return;
    }

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
