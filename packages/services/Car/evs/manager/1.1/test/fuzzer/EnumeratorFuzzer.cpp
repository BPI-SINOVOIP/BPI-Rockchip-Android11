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
#include <hidl/HidlTransportSupport.h>
#include <stdlib.h>
#include <sys/time.h>
#include <iostream>
#include "Common.h"
#include "Enumerator.h"
#include "HalDisplay.h"
#include "MockHWEnumerator.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

namespace {

enum EvsFuzzFuncs {
    EVS_FUZZ_GET_CAMERA_LIST,             // verify getCameraList
    EVS_FUZZ_OPEN_CAMERA,                 // verify openCamera
    EVS_FUZZ_CLOSE_CAMERA,                // verify closeCamera
    EVS_FUZZ_OPEN_DISPLAY,                // verify openDisplay
    EVS_FUZZ_CLOSE_DISPLAY,               // verify closeDisplay
    EVS_FUZZ_GET_DISPLAY_STATE,           // verify getDisplayState
    EVS_FUZZ_GET_CAMERA_LIST_1_1,         // verify getCameraList_1_1
    EVS_FUZZ_OPEN_CAMERA_1_1,             // verify openCamera_1_1
    EVS_FUZZ_IS_HARDWARE,                 // verify isHardware
    EVS_FUZZ_GET_DISPLAY_LIST,            // verify getDisplayIdList
    EVS_FUZZ_OPEN_DISPLAY_1_1,            // verify openDisplay_1_1
    EVS_FUZZ_GET_ULTRASONICS_ARRAY_LIST,  // verify getUltrasonicsArrayList
    EVS_FUZZ_OPEN_ULTRASONICS_ARRAY,      // verify openUltrasonicsArray
    EVS_FUZZ_CLOSE_ULTRASONICS_ARRAY,     // verify closeUltrasonicsArray
    EVS_FUZZ_API_SUM
};

const int kMaxFuzzerConsumedBytes = 12;


static sp<IEvsEnumerator_1_1> sMockHWEnumerator;
static sp<Enumerator> sEnumerator;

bool DoInitialization() {
    setenv("TREBLE_TESTING_OVERRIDE", "true", true);
    configureRpcThreadpool(2, false /* callerWillNotJoin */);

    // Prepare for the HWEnumerator service
    sMockHWEnumerator = new MockHWEnumerator();
    status_t status = sMockHWEnumerator->registerAsService(kMockHWEnumeratorName);
    if (status != OK) {
        std::cerr << "Could not register service " << kMockHWEnumeratorName
                  << " status = " << status
                  << " - quitting from LLVMFuzzerInitialize" << std::endl;
        exit(2);
    }

    return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);

    vector<sp<IEvsCamera_1_0>> vVirtualCameras;
    vector<sp<IEvsDisplay_1_0>> vDisplays;

    // Inititialize the enumerator that we are going to test
    static bool initialized = DoInitialization();
    sEnumerator = new Enumerator();
    if (!initialized || !sEnumerator->init(kMockHWEnumeratorName)) {
        std::cerr << "Failed to connect to hardware service"
                  << "- quitting from LLVMFuzzerInitialize" << std::endl;
        exit(1);
    }

    while (fdp.remaining_bytes() > kMaxFuzzerConsumedBytes) {
        switch (fdp.ConsumeIntegralInRange<uint32_t>(0, EVS_FUZZ_API_SUM - 1)) {
            case EVS_FUZZ_GET_CAMERA_LIST: {
                sEnumerator->getCameraList([](auto list) {});
                break;
            }
            case EVS_FUZZ_OPEN_CAMERA: {
                uint64_t whichCam =
                            fdp.ConsumeIntegralInRange<uint64_t>(startMockHWCameraId,
                                                                 endMockHWCameraId-1);
                hidl_string camStr = std::to_string(whichCam);
                sp<IEvsCamera_1_0> virtualCam = sEnumerator->openCamera(camStr);
                if (virtualCam != nullptr) {
                    vVirtualCameras.emplace_back(virtualCam);
                }
                break;
            }
            case EVS_FUZZ_CLOSE_CAMERA: {
                if (!vVirtualCameras.empty()) {
                    sp<IEvsCamera_1_0> cam = vVirtualCameras.back();
                    sEnumerator->closeCamera(cam);
                    vVirtualCameras.pop_back();
                }
                break;
            }
            case EVS_FUZZ_OPEN_DISPLAY: {
                sp<IEvsDisplay_1_0> display = sEnumerator->openDisplay();
                if (display != nullptr) {
                    vDisplays.emplace_back(display);
                }
                break;
            }
            case EVS_FUZZ_CLOSE_DISPLAY: {
                if (!vDisplays.empty()) {
                    sp<IEvsDisplay_1_0> display = vDisplays.back();
                    sEnumerator->closeDisplay(display);
                    vDisplays.pop_back();
                }
                break;
            }
            case EVS_FUZZ_GET_DISPLAY_STATE: {
                sEnumerator->getDisplayState();
                break;
            }
            case EVS_FUZZ_GET_CAMERA_LIST_1_1: {
                sEnumerator->getCameraList_1_1([](auto cams) {});
                break;
            }
            case EVS_FUZZ_OPEN_CAMERA_1_1: {
                uint64_t whichCam =
                            fdp.ConsumeIntegralInRange<uint64_t>(startMockHWCameraId,
                                                                 endMockHWCameraId-1);
                hidl_string camStr = std::to_string(whichCam);
                Stream streamCfg = {};
                sp<IEvsCamera_1_1> virtualCam = sEnumerator->openCamera_1_1(camStr, streamCfg);
                if (virtualCam != nullptr) {
                    vVirtualCameras.emplace_back(virtualCam);
                }
                break;
            }
            case EVS_FUZZ_IS_HARDWARE: {
                sEnumerator->isHardware();
                break;
            }
            case EVS_FUZZ_GET_DISPLAY_LIST: {
                sEnumerator->getDisplayIdList([](auto list) {});
                break;
            }
            case EVS_FUZZ_OPEN_DISPLAY_1_1: {
                uint64_t whichDisp =
                            fdp.ConsumeIntegralInRange<uint64_t>(startMockHWDisplayId,
                                                                 endMockHWDisplayId-1);
                // port is the same as display in this test
                sp<IEvsDisplay_1_1> display = sEnumerator->openDisplay_1_1(
                                                static_cast<uint8_t>(whichDisp));
                if (display != nullptr) {
                    vDisplays.emplace_back(display);
                }
                break;
            }
            case EVS_FUZZ_GET_ULTRASONICS_ARRAY_LIST: {
                sEnumerator->getUltrasonicsArrayList([](auto list) {});
                break;
            }
            case EVS_FUZZ_OPEN_ULTRASONICS_ARRAY: {
                // TODO(b/162465548) replace this once implementation is ready
                sEnumerator->openUltrasonicsArray("");
                break;
            }
            case EVS_FUZZ_CLOSE_ULTRASONICS_ARRAY: {
                // TODO(b/162465548) replace this once implementation is ready
                sEnumerator->closeUltrasonicsArray(nullptr);
                break;
            }
            default:
                LOG(ERROR) << "Unexpected option, aborting...";
                break;
        }
    }
    // Explicitly destroy the Enumerator
    sEnumerator = nullptr;
    return 0;
}

}  // namespace
}  // namespace implementation
}  // namespace V1_1
}  // namespace evs
}  // namespace automotive
}  // namespace android
