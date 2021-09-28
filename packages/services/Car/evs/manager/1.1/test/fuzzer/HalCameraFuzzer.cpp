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
#include <sys/time.h>
#include <iostream>
#include "Common.h"
#include "Enumerator.h"
#include "HalCamera.h"
#include "MockHWCamera.h"

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

namespace {

enum EvsFuzzFuncs {
    EVS_FUZZ_MAKE_VIRTUAL_CAMERA = 0,    // verify makeVirtualCamera
    EVS_FUZZ_OWN_VIRTUAL_CAMERA,         // verify ownVirtualCamera
    EVS_FUZZ_DISOWN_VIRTUAL_CAMERA,      // verify disownVirtualCamera
    EVS_FUZZ_GET_CLIENT_COUNT,           // verify getClientCount
    EVS_FUZZ_GET_ID,                     // verify getId
    EVS_FUZZ_GET_STREAM_CONFIG,          // verify getStreamConfig
    EVS_FUZZ_CHANGE_FRAMES_IN_FLIGHT,    // verify changeFramesInFlight
    EVS_FUZZ_CHANGE_FRAMES_IN_FLIGHT_1,  // verify overloaded changeFramesInFlight
    EVS_FUZZ_REQUEST_NEW_FRAME,          // verify requestNewFrame
    EVS_FUZZ_CLIENT_STREAM_STARTING,     // verify clientStreamStarting
    EVS_FUZZ_CLIENT_STREAM_ENDING,       // verify clientStreamEnding
    EVS_FUZZ_GET_STATS,                  // verify getStats
    EVS_FUZZ_GET_STREAM_CONFIGURATION,   // verify getStreamConfiguration
    EVS_FUZZ_DELIVER_FRAME_1_1,          // verify deliverFrame_1_1
    EVS_FUZZ_BASE_ENUM                   // verify common functions
};

int64_t getCurrentTimeStamp() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    int64_t ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    return ms;
}

const int kMaxFuzzerConsumedBytes = 12;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);
    sp<IEvsCamera_1_1> mockHWCamera = new MockHWCamera();
    sp<HalCamera> halCamera = new HalCamera(mockHWCamera);
    std::vector<sp<VirtualCamera>> virtualCameras;
    std::vector<BufferDesc_1_0> vBufferDesc_1_0;
    std::vector<BufferDesc_1_1> vBufferDesc_1_1;

    while (fdp.remaining_bytes() > kMaxFuzzerConsumedBytes) {
        switch (fdp.ConsumeIntegralInRange<uint32_t>(0, EVS_FUZZ_API_SUM - 1)) {
            case EVS_FUZZ_MAKE_VIRTUAL_CAMERA: {
                sp<VirtualCamera> virtualCamera = halCamera->makeVirtualCamera();
                virtualCameras.emplace_back(virtualCamera);
                break;
            }
            case EVS_FUZZ_OWN_VIRTUAL_CAMERA: {
                if (!virtualCameras.empty()) {
                    uint32_t whichCam =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, virtualCameras.size() - 1);
                    halCamera->ownVirtualCamera(virtualCameras[whichCam]);
                }
                break;
            }
            case EVS_FUZZ_DISOWN_VIRTUAL_CAMERA: {
                if (!virtualCameras.empty()) {
                    uint32_t whichCam =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, virtualCameras.size() - 1);
                    halCamera->disownVirtualCamera(virtualCameras[whichCam]);
                }
                break;
            }
            case EVS_FUZZ_GET_HW_CAMERA: {
                halCamera->getHwCamera();
                break;
            }
            case EVS_FUZZ_GET_CLIENT_COUNT: {
                halCamera->getClientCount();
                break;
            }
            case EVS_FUZZ_GET_ID: {
                halCamera->getId();
                break;
            }
            case EVS_FUZZ_GET_STREAM_CONFIG: {
                halCamera->getStreamConfig();
                break;
            }
            case EVS_FUZZ_CHANGE_FRAMES_IN_FLIGHT: {
                uint32_t delta = fdp.ConsumeIntegral<int32_t>();
                halCamera->changeFramesInFlight(delta);
                break;
            }
            case EVS_FUZZ_CHANGE_FRAMES_IN_FLIGHT_1: {
                hidl_vec<BufferDesc_1_1> buffers;
                int32_t delta = 0;
                halCamera->changeFramesInFlight(buffers, &delta);
                break;
            }
            case EVS_FUZZ_REQUEST_NEW_FRAME: {
                if (!virtualCameras.empty()) {
                    uint32_t whichCam =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, virtualCameras.size() - 1);
                    halCamera->requestNewFrame(virtualCameras[whichCam], getCurrentTimeStamp());
                }
                break;
            }
            case EVS_FUZZ_CLIENT_STREAM_STARTING: {
                halCamera->clientStreamStarting();
                break;
            }
            case EVS_FUZZ_CLIENT_STREAM_ENDING: {
                if (!virtualCameras.empty()) {
                    uint32_t whichCam =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, virtualCameras.size() - 1);
                    halCamera->clientStreamEnding(virtualCameras[whichCam].get());
                }
                break;
            }
            case EVS_FUZZ_DONE_WITH_FRAME_1_0: {
                if (!vBufferDesc_1_0.empty()) {
                    uint32_t whichBuffer =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, vBufferDesc_1_0.size() - 1);
                    halCamera->doneWithFrame(vBufferDesc_1_0[whichBuffer]);
                }
                break;
            }
            case EVS_FUZZ_DONE_WITH_FRAME_1_1: {
                if (!vBufferDesc_1_1.empty()) {
                    uint32_t whichBuffer =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, vBufferDesc_1_1.size() - 1);
                    halCamera->doneWithFrame(vBufferDesc_1_1[whichBuffer]);
                }
                break;
            }
            case EVS_FUZZ_SET_PRIMARY: {
                if (!virtualCameras.empty()) {
                    uint32_t whichCam =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, virtualCameras.size() - 1);
                    halCamera->setMaster(virtualCameras[whichCam]);
                }
                break;
            }
            case EVS_FUZZ_FORCE_PRIMARY: {
                if (!virtualCameras.empty()) {
                    uint32_t whichCam =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, virtualCameras.size() - 1);
                    halCamera->forceMaster(virtualCameras[whichCam]);
                }
                break;
            }
            case EVS_FUZZ_UNSET_PRIMARY: {
                if (!virtualCameras.empty()) {
                    uint32_t whichCam =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, virtualCameras.size() - 1);
                    halCamera->unsetMaster(virtualCameras[whichCam]);
                }
                break;
            }
            case EVS_FUZZ_SET_PARAMETER: {
                if (!virtualCameras.empty()) {
                    uint32_t whichCam =
                            fdp.ConsumeIntegralInRange<uint32_t>(0, virtualCameras.size() - 1);
                    uint32_t whichParam = fdp.ConsumeIntegralInRange<
                            uint32_t>(0, static_cast<uint32_t>(CameraParam::ABSOLUTE_ZOOM));
                    int32_t value = fdp.ConsumeIntegral<int32_t>();
                    halCamera->setParameter(virtualCameras[whichCam],
                                            static_cast<CameraParam>(whichParam), value);
                }
                break;
            }
            case EVS_FUZZ_GET_PARAMETER: {
                uint32_t whichParam =
                        fdp.ConsumeIntegralInRange<uint32_t>(0,
                                                             static_cast<uint32_t>(
                                                                     CameraParam::ABSOLUTE_ZOOM));
                int32_t value = fdp.ConsumeIntegral<int32_t>();
                halCamera->getParameter(static_cast<CameraParam>(whichParam), value);
                break;
            }
            case EVS_FUZZ_GET_STATS: {
                halCamera->getStats();
                break;
            }
            case EVS_FUZZ_GET_STREAM_CONFIGURATION: {
                halCamera->getStreamConfiguration();
                break;
            }
            case EVS_FUZZ_DELIVER_FRAME: {
                BufferDesc_1_0 buffer;
                buffer.bufferId = fdp.ConsumeIntegral<int32_t>();
                halCamera->deliverFrame(buffer);
                vBufferDesc_1_0.emplace_back(buffer);
                break;
            }
            case EVS_FUZZ_DELIVER_FRAME_1_1: {
                std::vector<BufferDesc_1_1> vec;
                BufferDesc_1_1 buffer;
                buffer.bufferId = fdp.ConsumeIntegral<int32_t>();
                vec.push_back(buffer);
                hardware::hidl_vec<BufferDesc_1_1> hidl_vec(vec);
                halCamera->deliverFrame_1_1(hidl_vec);
                vBufferDesc_1_1.emplace_back(buffer);
                break;
            }
            case EVS_FUZZ_NOTIFY: {
                EvsEventDesc event;
                uint32_t type =
                        fdp.ConsumeIntegralInRange<uint32_t>(0,
                                                             static_cast<uint32_t>(
                                                                     EvsEventType::STREAM_ERROR));
                event.aType = static_cast<EvsEventType>(type);
                // TODO(b/160824438) let's comment this for now because of the failure.
                // If virtualCamera does not call startVideoStream, and notify(1) is called
                // it will fail.
                // halCamera->notify(event);
                break;
            }
            default:
                LOG(ERROR) << "Unexpected option, aborting...";
                break;
        }
    }
    return 0;
}

}  // namespace
}  // namespace implementation
}  // namespace V1_1
}  // namespace evs
}  // namespace automotive
}  // namespace android
