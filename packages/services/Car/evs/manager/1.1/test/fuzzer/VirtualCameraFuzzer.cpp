/*
 * Copyright 2020 The Android Open Source Project
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

#include <fuzzer/FuzzedDataProvider.h>
#include <iostream>
#include "Common.h"
#include "Enumerator.h"
#include "MockHWCamera.h"
#include "VirtualCamera.h"

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

namespace {

using CameraDesc_1_0 = ::android::hardware::automotive::evs::V1_0::CameraDesc;

enum EvsFuzzFuncs {
    EVS_FUZZ_GET_ALLOWED_BUFFERS,       // verify getAllowedBuffers
    EVS_FUZZ_IS_STREAMING,              // verify isStreaming
    EVS_FUZZ_GET_VERSION,               // verify getVersion
    EVS_FUZZ_SET_DESCRIPTOR,            // verify setDescriptor
    EVS_FUZZ_GET_CAMERA_INFO,           // verify getCameraInfo
    EVS_FUZZ_SETMAX_FRAMES_IN_FLIGHT,   // verify setMaxFramesInFlight
    EVS_FUZZ_START_VIDEO_STREAM,        // verify startVideoStream
    EVS_FUZZ_STOP_VIDEO_STREAM,         // verify stopVideoStream
    EVS_FUZZ_GET_EXTENDED_INFO,         // verify getExtendedInfo
    EVS_FUZZ_SET_EXTENDED_INFO,         // verify setExtendedInfo
    EVS_FUZZ_GET_CAMERA_INFO_1_1,       // verify getCameraInfo_1_1
    EVS_FUZZ_GET_PHYSICAL_CAMERA_INFO,  // verify getPhysicalCameraInfo
    EVS_FUZZ_PAUSE_VIDEO_STREAM,        // verify pauseVideoStream
    EVS_FUZZ_RESUME_VIDEO_STREAM,       // verify resumeVideoStream
    EVS_FUZZ_GET_PARAMETER_LIST,        // verify getParameterList
    EVS_FUZZ_GET_INT_PARAMETER_RANGE,   // verify getIntParameterRange
    EVS_FUZZ_SET_EXTENDED_INFO_1_1,     // verify setExtendedInfo_1_1
    EVS_FUZZ_GET_EXTENDED_INFO_1_1,     // verify getExtendedInfo_1_1
    EVS_FUZZ_IMPORT_EXTERNAL_BUFFERS,   // verify importExternalBuffers
    EVS_FUZZ_BASE_ENUM                  // verify common functions
};

const int kMaxFuzzerConsumedBytes = 12;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);
    sp<IEvsCamera_1_1> mockHWCamera = new MockHWCamera();
    sp<HalCamera> halCamera = new HalCamera(mockHWCamera);
    sp<VirtualCamera> virtualCamera = halCamera->makeVirtualCamera();

    std::vector<BufferDesc_1_0> vBufferDesc_1_0;
    std::vector<BufferDesc_1_1> vBufferDesc_1_1;

    bool videoStarted = false;

    while (fdp.remaining_bytes() > kMaxFuzzerConsumedBytes) {
        switch (fdp.ConsumeIntegralInRange<uint32_t>(0, EVS_FUZZ_API_SUM - 1)) {
            case EVS_FUZZ_GET_ALLOWED_BUFFERS: {
                virtualCamera->getAllowedBuffers();
                break;
            }
            case EVS_FUZZ_IS_STREAMING: {
                virtualCamera->isStreaming();
                break;
            }
            case EVS_FUZZ_GET_VERSION: {
                virtualCamera->getVersion();
                break;
            }
            case EVS_FUZZ_GET_HW_CAMERA: {
                virtualCamera->getHalCameras();
                break;
            }
            case EVS_FUZZ_SET_DESCRIPTOR: {
                CameraDesc* desc = new CameraDesc();
                virtualCamera->setDescriptor(desc);
                break;
            }
            case EVS_FUZZ_NOTIFY: {
                if (videoStarted) {
                    EvsEventDesc event;
                    uint32_t type = fdp.ConsumeIntegralInRange<
                            uint32_t>(0, static_cast<uint32_t>(EvsEventType::STREAM_ERROR));
                    event.aType = static_cast<EvsEventType>(type);
                    virtualCamera->notify(event);
                }
                break;
            }
            case EVS_FUZZ_DELIVER_FRAME: {
                BufferDesc buffer;
                buffer.bufferId = fdp.ConsumeIntegral<int32_t>();
                virtualCamera->deliverFrame(buffer);
                vBufferDesc_1_1.emplace_back(buffer);
                break;
            }
            case EVS_FUZZ_GET_CAMERA_INFO: {
                virtualCamera->getCameraInfo([](CameraDesc_1_0 desc) {});
                break;
            }
            case EVS_FUZZ_SETMAX_FRAMES_IN_FLIGHT: {
                uint32_t delta = fdp.ConsumeIntegral<uint32_t>();
                virtualCamera->setMaxFramesInFlight(delta);
                break;
            }
            case EVS_FUZZ_START_VIDEO_STREAM: {
                if (!videoStarted) {
                    sp<IEvsCamera_1_1> mockHWCamera1 = new MockHWCamera();
                    sp<HalCamera> halCamera1 = new HalCamera(mockHWCamera1);
                    virtualCamera->startVideoStream(halCamera1);
                    videoStarted = true;
                }
                break;
            }
            case EVS_FUZZ_DONE_WITH_FRAME_1_0: {
                if (!vBufferDesc_1_0.empty()) {
                    uint32_t whichBuffer =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, vBufferDesc_1_0.size() - 1);
                    virtualCamera->doneWithFrame(vBufferDesc_1_0[whichBuffer]);
                }
                break;
            }
            case EVS_FUZZ_STOP_VIDEO_STREAM: {
                virtualCamera->stopVideoStream();
                videoStarted = false;
                break;
            }
            case EVS_FUZZ_GET_EXTENDED_INFO: {
                uint32_t opaqueIdentifier = fdp.ConsumeIntegral<uint32_t>();
                virtualCamera->getExtendedInfo(opaqueIdentifier);
                break;
            }
            case EVS_FUZZ_SET_EXTENDED_INFO: {
                uint32_t opaqueIdentifier = fdp.ConsumeIntegral<uint32_t>();
                int32_t opaqueValue = fdp.ConsumeIntegral<int32_t>();
                virtualCamera->setExtendedInfo(opaqueIdentifier, opaqueValue);
                break;
            }
            case EVS_FUZZ_GET_CAMERA_INFO_1_1: {
                virtualCamera->getCameraInfo_1_1([](CameraDesc desc) {});
                break;
            }
            case EVS_FUZZ_GET_PHYSICAL_CAMERA_INFO: {
                hidl_string deviceId("");
                virtualCamera->getPhysicalCameraInfo(deviceId, [](const CameraDesc& info) {});
                break;
            }
            case EVS_FUZZ_DONE_WITH_FRAME_1_1: {
                if (!vBufferDesc_1_1.empty()) {
                    hidl_vec<BufferDesc_1_1> buffers(vBufferDesc_1_1);
                    virtualCamera->doneWithFrame_1_1(buffers);
                }
                break;
            }
            case EVS_FUZZ_PAUSE_VIDEO_STREAM: {
                virtualCamera->pauseVideoStream();
                break;
            }
            case EVS_FUZZ_RESUME_VIDEO_STREAM: {
                virtualCamera->resumeVideoStream();
                break;
            }
            case EVS_FUZZ_SET_PRIMARY: {
                virtualCamera->setMaster();
                break;
            }
            case EVS_FUZZ_FORCE_PRIMARY: {
                // TODO(161388489) skip this until we finished fuzzing evs display
                break;
            }
            case EVS_FUZZ_UNSET_PRIMARY: {
                virtualCamera->unsetMaster();
                break;
            }
            case EVS_FUZZ_GET_PARAMETER_LIST: {
                virtualCamera->getParameterList([](hidl_vec<CameraParam> cmdList) {});
                break;
            }
            case EVS_FUZZ_GET_INT_PARAMETER_RANGE: {
                uint32_t whichParam =
                        fdp.ConsumeIntegralInRange<uint32_t>(0,
                                                             static_cast<uint32_t>(
                                                                     CameraParam::ABSOLUTE_ZOOM));
                virtualCamera->getIntParameterRange(static_cast<CameraParam>(whichParam),
                                                    [](int32_t val0, int32_t val1, int32_t val2) {
                                                    });
                break;
            }
            case EVS_FUZZ_SET_PARAMETER: {
                uint32_t whichParam =
                        fdp.ConsumeIntegralInRange<uint32_t>(0,
                                                             static_cast<uint32_t>(
                                                                     CameraParam::ABSOLUTE_ZOOM));
                int32_t val = fdp.ConsumeIntegral<int32_t>();
                virtualCamera->setIntParameter(static_cast<CameraParam>(whichParam), val,
                                               [](auto status, auto effectiveValues) {});
                break;
            }
            case EVS_FUZZ_GET_PARAMETER: {
                uint32_t whichParam =
                        fdp.ConsumeIntegralInRange<uint32_t>(0,
                                                             static_cast<uint32_t>(
                                                                     CameraParam::ABSOLUTE_ZOOM));
                virtualCamera->getIntParameter(static_cast<CameraParam>(whichParam),
                                               [](auto status, auto effectiveValues) {});
                break;
            }
            case EVS_FUZZ_SET_EXTENDED_INFO_1_1: {
                uint32_t opaqueIdentifier = fdp.ConsumeIntegral<uint32_t>();
                uint8_t opaqueValue = fdp.ConsumeIntegral<uint8_t>();
                vector<uint8_t> v;
                v.push_back(opaqueValue);
                hidl_vec<uint8_t> vec(v);
                virtualCamera->setExtendedInfo_1_1(opaqueIdentifier, vec);
                break;
            }
            case EVS_FUZZ_GET_EXTENDED_INFO_1_1: {
                uint32_t opaqueIdentifier = fdp.ConsumeIntegral<uint32_t>();
                virtualCamera->getExtendedInfo_1_1(opaqueIdentifier,
                                                   [](const auto& result, const auto& data) {});
                break;
            }
            case EVS_FUZZ_IMPORT_EXTERNAL_BUFFERS: {
                if (!vBufferDesc_1_1.empty()) {
                    hidl_vec<BufferDesc_1_1> buffers(vBufferDesc_1_1);
                    virtualCamera->importExternalBuffers(buffers, [](auto _result, auto _delta) {});
                }
                break;
            }
            default:
                LOG(ERROR) << "Unexpected option, aborting...";
                break;
        }
    }

    if (videoStarted) {
        // TODO(b/161762538) if we do not stop video stream manually here,
        // there will be crash at VirtualCamera.cpp::pHwCamera->unsetMaster(this);
        virtualCamera->stopVideoStream();
    }
    return 0;
}

}  // namespace
}  // namespace implementation
}  // namespace V1_1
}  // namespace evs
}  // namespace automotive
}  // namespace android
