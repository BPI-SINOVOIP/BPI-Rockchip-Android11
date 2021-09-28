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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_CAMERA_VENDOR_TAG_DEFS_H
#define HARDWARE_GOOGLE_CAMERA_HAL_CAMERA_VENDOR_TAG_DEFS_H

#include <vector>

#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// TODO(b/127998029): it is more suitable to reserve a section before
// VENDOR_SECTION_START in the framework for private use and update this range.
//
// Android vendor tags start at 0x80000000 according to VENDOR_SECTION_START.
// Reserve the upper range of that for HAL. The vendor HWL cannot have any tags
// overlapping with this range up
constexpr uint32_t kHalVendorTagSectionStart = 0x84000000;

// Camera HAL vendor tag IDs. Items should not be removed or rearranged
enum VendorTagIds : uint32_t {
  kLogicalCamDefaultPhysicalId = kHalVendorTagSectionStart,
  kHybridAeEnabled,
  kHdrPlusDisabled,
  kHdrplusPayloadFrames,
  kProcessingMode,
  kThermalThrottling,
  kOutputIntent,
  kAvailableNonWarpedYuvSizes,
  kNonWarpedYuvStreamId,
  kSensorModeFullFov,
  kNonWarpedCropRegion,
  kHdrUsageMode,
  // This should not be used as a vendor tag ID on its own, but as a placeholder
  // to indicate the end of currently defined vendor tag IDs
  kEndMarker
};

enum class SmoothyMode : uint32_t {
  // Stablizes frames while moves with user's intentional motion, e.g. panning.
  // Similar to normal EIS.
  kSteadyCamMode = 0,

  // Fixes the viewport as if videos are captured on a tripod.
  kTripodMode,

  // Tracks an object of interest and keeps it at frame's salient position, e.g.
  // center.
  kTrackingMode,

  // Uses UW camera with a larger margin. In this way, we get a better video
  // stabilization quality, while preserving a similar FoV as the main camera.
  kSuperstabMode
};

// Logical camera vendor tags
static const std::vector<VendorTag> kLogicalCameraVendorTags = {
    // Logical camera default physical camera ID
    //
    // Indicates the camera ID for the physical camera that should be streamed on
    // as the default camera of a logical camera device
    //
    // Present in: Characteristics
    // Payload: framework camera ID
    {.tag_id = VendorTagIds::kLogicalCamDefaultPhysicalId,
     .tag_name = "DefaultPhysicalCamId",
     .tag_type = CameraMetadataType::kInt32},
};

// Experimental 2016 API tags
static const std::vector<VendorTag> kExperimental2016Tags = {
    // Hybrid AE enabled toggle
    //
    // Indicates whether Hybrid AE should be enabled in HAL or not
    //
    // Present in: request, and result keys
    // Payload: integer treated as a boolean toggle flag
    {.tag_id = VendorTagIds::kHybridAeEnabled,
     .tag_name = "3a.hybrid_ae_enable",
     .tag_type = CameraMetadataType::kInt32},
};

// Experimental 2017 API tags
static const std::vector<VendorTag> kExperimental2017Tags = {
    // HDR+ disabled toggle
    //
    // Indicates whether HDR+ should be disabled in HAL or not
    //
    // Present in: request, result, and session keys
    // Payload: 1 byte boolean flag
    {.tag_id = VendorTagIds::kHdrPlusDisabled,
     .tag_name = "request.disable_hdrplus",
     .tag_type = CameraMetadataType::kByte},
};

// Experimental 2019 API tags
static const std::vector<VendorTag> kExperimental2019Tags = {
    // Select sensor mode which has Full FOV
    //
    // Indicates whether full FOV sensor mode is requested
    //
    // Present in: request, result, and session keys
    // Payload: 1 byte boolean flag
    {.tag_id = VendorTagIds::kSensorModeFullFov,
     .tag_name = "SensorModeFullFov",
     .tag_type = CameraMetadataType::kByte},
};

// Internal vendor tags
static const std::vector<VendorTag> kInternalVendorTags = {
    // Hdrplus playload frames
    //
    // Indicates the number of HDR+ input buffers
    //
    // Present in: Characteristics
    // Payload: integer for HDR+ input buffers
    {.tag_id = VendorTagIds::kHdrplusPayloadFrames,
     .tag_name = "hdrplus.PayloadFrames",
     .tag_type = CameraMetadataType::kInt32},
    // Capture request processing mode
    //
    // Indicates whether the capture request is intended for intermediate
    // processing, or if it's the final capture request to be sent back to
    // the camera framework. Absense of this tag should imply final processing.
    // When indermediate processing is specified, HAL will need to explicitly
    // filter HWL's private metadata by calling
    //   CameraDeviceSessionHwl::FilterResultMetadata()
    //
    // Present in: request
    // Payload: ProcessingMode
    {.tag_id = VendorTagIds::kProcessingMode,
     .tag_name = "ProcessingMode",
     .tag_type = CameraMetadataType::kByte},
    // Thermal throttled
    //
    // Indicates whether thermal throttling is triggered.
    //
    // Present in: request
    // Payload: 1 byte boolean flag
    {.tag_id = VendorTagIds::kThermalThrottling,
     .tag_name = "thermal_throttling",
     .tag_type = CameraMetadataType::kByte},
    // Capture request output intent
    //
    // Indicates whether the capture request is intended for preview, snapshot,
    // video, zsl, or video snapshot, etc. This information can be used to
    // indicate different tuning usecases.
    //
    // Present in: request
    // Payload: OutputIntent
    {.tag_id = VendorTagIds::kOutputIntent,
     .tag_name = "OutputIntent",
     .tag_type = CameraMetadataType::kByte},
    // Supported stream sizes for non-warped yuv
    //
    // List supported dimensions if HAL request non-warped YUV_420_888.
    //
    // Present in: Characteristics
    // Payload: n * 2 integers for supported dimensions(w*h)
    {.tag_id = VendorTagIds::kAvailableNonWarpedYuvSizes,
     .tag_name = "AvailableNonWarpedYuvSizes",
     .tag_type = CameraMetadataType::kInt32},
    // Non-warped YUV stream id
    //
    // Used by GCH to specify one YUV stream through its stream id to which no
    // warping should be applied except for certain level of cropping. The
    // cropping should be specified in VendorTagIds::kNonWarpedCropRegion.
    // Present in: session parameter
    // Payload: one int32_t
    {.tag_id = VendorTagIds::kNonWarpedYuvStreamId,
     .tag_name = "NonWarpedYuvStreamId",
     .tag_type = CameraMetadataType::kInt32},
    // Non-warped crop region
    //
    // This specifies how the NonWarpedYuvStream is cropped relative to
    // android.sensor.info.preCorrectionActiveArraySize.
    //
    // Present in: request and result parameter
    // Payload: Four int32_t in the order of [left, right, width, height]
    {.tag_id = VendorTagIds::kNonWarpedCropRegion,
     .tag_name = "NonWarpedCropRegion",
     .tag_type = CameraMetadataType::kInt32},
    // Hdrplus usage mode
    //
    // Indicates the usage mode of hdrplus
    //
    // Present in: Characteristics
    // Payload: HdrUsageMode
    {.tag_id = VendorTagIds::kHdrUsageMode,
     .tag_name = "hdr.UsageMode",
     .tag_type = CameraMetadataType::kByte},
};

// Google Camera HAL vendor tag sections
static const std::vector<VendorTagSection> kHalVendorTagSections = {
    {.section_name = "com.google.hal.logicalcamera",
     .tags = kLogicalCameraVendorTags},
    {.section_name = "com.google.pixel.experimental2016",
     .tags = kExperimental2016Tags},
    {.section_name = "com.google.pixel.experimental2017",
     .tags = kExperimental2017Tags},
    {.section_name = "com.google.pixel.experimental2019",
     .tags = kExperimental2019Tags},
    {.section_name = "com.google.internal", .tags = kInternalVendorTags},
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_CAMERA_VENDOR_TAG_DEFS_H
