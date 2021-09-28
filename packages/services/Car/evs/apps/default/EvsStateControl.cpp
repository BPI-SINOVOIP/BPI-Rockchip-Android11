/*
 * Copyright (C) 2016 The Android Open Source Project
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
#include "EvsStateControl.h"
#include "RenderDirectView.h"
#include "RenderTopView.h"
#include "RenderPixelCopy.h"
#include "FormatConvert.h"

#include <stdio.h>
#include <string.h>

#include <android-base/logging.h>
#include <inttypes.h>
#include <utils/SystemClock.h>
#include <binder/IServiceManager.h>

using ::android::hardware::automotive::evs::V1_0::EvsResult;
using EvsDisplayState = ::android::hardware::automotive::evs::V1_0::DisplayState;
using BufferDesc_1_0  = ::android::hardware::automotive::evs::V1_0::BufferDesc;
using BufferDesc_1_1  = ::android::hardware::automotive::evs::V1_1::BufferDesc;

static bool isSfReady() {
    const android::String16 serviceName("SurfaceFlinger");
    return android::defaultServiceManager()->checkService(serviceName) != nullptr;
}

// TODO:  Seems like it'd be nice if the Vehicle HAL provided such helpers (but how & where?)
inline constexpr VehiclePropertyType getPropType(VehicleProperty prop) {
    return static_cast<VehiclePropertyType>(
            static_cast<int32_t>(prop)
            & static_cast<int32_t>(VehiclePropertyType::MASK));
}


EvsStateControl::EvsStateControl(android::sp <IVehicle>       pVnet,
                                 android::sp <IEvsEnumerator> pEvs,
                                 android::sp <IEvsDisplay>    pDisplay,
                                 const ConfigManager&         config) :
    mVehicle(pVnet),
    mEvs(pEvs),
    mDisplay(pDisplay),
    mConfig(config),
    mCurrentState(OFF) {

    // Initialize the property value containers we'll be updating (they'll be zeroed by default)
    static_assert(getPropType(VehicleProperty::GEAR_SELECTION) == VehiclePropertyType::INT32,
                  "Unexpected type for GEAR_SELECTION property");
    static_assert(getPropType(VehicleProperty::TURN_SIGNAL_STATE) == VehiclePropertyType::INT32,
                  "Unexpected type for TURN_SIGNAL_STATE property");

    mGearValue.prop       = static_cast<int32_t>(VehicleProperty::GEAR_SELECTION);
    mTurnSignalValue.prop = static_cast<int32_t>(VehicleProperty::TURN_SIGNAL_STATE);

    // This way we only ever deal with cameras which exist in the system
    // Build our set of cameras for the states we support
    LOG(DEBUG) << "Requesting camera list";
    mEvs->getCameraList_1_1(
        [this, &config](hidl_vec<CameraDesc> cameraList) {
            LOG(INFO) << "Camera list callback received " << cameraList.size() << "cameras.";
            for (auto&& cam: cameraList) {
                LOG(DEBUG) << "Found camera " << cam.v1.cameraId;
                bool cameraConfigFound = false;

                // Check our configuration for information about this camera
                // Note that a camera can have a compound function string
                // such that a camera can be "right/reverse" and be used for both.
                // If more than one camera is listed for a given function, we'll
                // list all of them and let the UX/rendering logic use one, some
                // or all of them as appropriate.
                for (auto&& info: config.getCameras()) {
                    if (cam.v1.cameraId == info.cameraId) {
                        // We found a match!
                        if (info.function.find("reverse") != std::string::npos) {
                            mCameraList[State::REVERSE].emplace_back(info);
                            mCameraDescList[State::REVERSE].emplace_back(cam);
                        }
                        if (info.function.find("right") != std::string::npos) {
                            mCameraList[State::RIGHT].emplace_back(info);
                            mCameraDescList[State::RIGHT].emplace_back(cam);
                        }
                        if (info.function.find("left") != std::string::npos) {
                            mCameraList[State::LEFT].emplace_back(info);
                            mCameraDescList[State::LEFT].emplace_back(cam);
                        }
                        if (info.function.find("park") != std::string::npos) {
                            mCameraList[State::PARKING].emplace_back(info);
                            mCameraDescList[State::PARKING].emplace_back(cam);
                        }
                        cameraConfigFound = true;
                        break;
                    }
                }
                if (!cameraConfigFound) {
                    LOG(WARNING) << "No config information for hardware camera "
                                 << cam.v1.cameraId;
                }
            }
        }
    );

    LOG(DEBUG) << "State controller ready";
}


bool EvsStateControl::startUpdateLoop() {
    // Create the thread and report success if it gets started
    mRenderThread = std::thread([this](){ updateLoop(); });
    return mRenderThread.joinable();
}


void EvsStateControl::terminateUpdateLoop() {
    // Join a rendering thread
    if (mRenderThread.joinable()) {
        mRenderThread.join();
    }
}


void EvsStateControl::postCommand(const Command& cmd, bool clear) {
    // Push the command onto the queue watched by updateLoop
    mLock.lock();
    if (clear) {
        std::queue<Command> emptyQueue;
        std::swap(emptyQueue, mCommandQueue);
    }

    mCommandQueue.push(cmd);
    mLock.unlock();

    // Send a signal to wake updateLoop in case it is asleep
    mWakeSignal.notify_all();
}


void EvsStateControl::updateLoop() {
    LOG(DEBUG) << "Starting EvsStateControl update loop";

    bool run = true;
    while (run) {
        // Process incoming commands
        {
            std::lock_guard <std::mutex> lock(mLock);
            while (!mCommandQueue.empty()) {
                const Command& cmd = mCommandQueue.front();
                switch (cmd.operation) {
                case Op::EXIT:
                    run = false;
                    break;
                case Op::CHECK_VEHICLE_STATE:
                    // Just running selectStateForCurrentConditions below will take care of this
                    break;
                case Op::TOUCH_EVENT:
                    // Implement this given the x/y location of the touch event
                    break;
                }
                mCommandQueue.pop();
            }
        }

        // Review vehicle state and choose an appropriate renderer
        if (!selectStateForCurrentConditions()) {
            LOG(ERROR) << "selectStateForCurrentConditions failed so we're going to die";
            break;
        }

        // If we have an active renderer, give it a chance to draw
        if (mCurrentRenderer) {
            // Get the output buffer we'll use to display the imagery
            BufferDesc_1_0 tgtBuffer = {};
            mDisplay->getTargetBuffer([&tgtBuffer](const BufferDesc_1_0& buff) {
                                          tgtBuffer = buff;
                                      }
            );

            if (tgtBuffer.memHandle == nullptr) {
                LOG(ERROR) << "Didn't get requested output buffer -- skipping this frame.";
            } else {
                // Generate our output image
                if (!mCurrentRenderer->drawFrame(convertBufferDesc(tgtBuffer))) {
                    // If drawing failed, we want to exit quickly so an app restart can happen
                    run = false;
                }

                // Send the finished image back for display
                mDisplay->returnTargetBufferForDisplay(tgtBuffer);
            }
        } else if (run) {
            // No active renderer, so sleep until somebody wakes us with another command
            // or exit if we received EXIT command
            std::unique_lock<std::mutex> lock(mLock);
            mWakeSignal.wait(lock);
        }
    }

    LOG(WARNING) << "EvsStateControl update loop ending";

    if (mCurrentRenderer) {
        // Deactive the renderer
        mCurrentRenderer->deactivate();
    }

    printf("Shutting down app due to state control loop ending\n");
    LOG(ERROR) << "Shutting down app due to state control loop ending";
}


bool EvsStateControl::selectStateForCurrentConditions() {
    static int32_t sDummyGear   = mConfig.getMockGearSignal();
    static int32_t sDummySignal = int32_t(VehicleTurnSignal::NONE);

    if (mVehicle != nullptr) {
        // Query the car state
        if (invokeGet(&mGearValue) != StatusCode::OK) {
            LOG(ERROR) << "GEAR_SELECTION not available from vehicle.  Exiting.";
            return false;
        }
        if ((mTurnSignalValue.prop == 0) || (invokeGet(&mTurnSignalValue) != StatusCode::OK)) {
            // Silently treat missing turn signal state as no turn signal active
            mTurnSignalValue.value.int32Values.setToExternal(&sDummySignal, 1);
            mTurnSignalValue.prop = 0;
        }
    } else {
        // While testing without a vehicle, behave as if we're in reverse for the first 20 seconds
        static const int kShowTime = 20;    // seconds

        // See if it's time to turn off the default reverse camera
        static std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > kShowTime) {
            // Switch to drive (which should turn off the reverse camera)
            sDummyGear = int32_t(VehicleGear::GEAR_DRIVE);
        }

        // Build the placeholder vehicle state values (treating single values as 1 element vectors)
        mGearValue.value.int32Values.setToExternal(&sDummyGear, 1);
        mTurnSignalValue.value.int32Values.setToExternal(&sDummySignal, 1);
    }

    // Choose our desired EVS state based on the current car state
    // TODO:  Update this logic, and consider user input when choosing if a view should be presented
    State desiredState = OFF;
    if (mGearValue.value.int32Values[0] == int32_t(VehicleGear::GEAR_REVERSE)) {
        desiredState = REVERSE;
    } else if (mTurnSignalValue.value.int32Values[0] == int32_t(VehicleTurnSignal::RIGHT)) {
        desiredState = RIGHT;
    } else if (mTurnSignalValue.value.int32Values[0] == int32_t(VehicleTurnSignal::LEFT)) {
        desiredState = LEFT;
    } else if (mGearValue.value.int32Values[0] == int32_t(VehicleGear::GEAR_PARK)) {
        desiredState = PARKING;
    }

    // Apply the desire state
    return configureEvsPipeline(desiredState);
}


StatusCode EvsStateControl::invokeGet(VehiclePropValue *pRequestedPropValue) {
    StatusCode status = StatusCode::TRY_AGAIN;

    // Call the Vehicle HAL, which will block until the callback is complete
    mVehicle->get(*pRequestedPropValue,
                  [pRequestedPropValue, &status]
                  (StatusCode s, const VehiclePropValue& v) {
                       status = s;
                       if (s == StatusCode::OK) {
                           *pRequestedPropValue = v;
                       }
                  }
    );

    return status;
}


bool EvsStateControl::configureEvsPipeline(State desiredState) {
    static bool isGlReady = false;

    if (mCurrentState == desiredState) {
        // Nothing to do here...
        return true;
    }

    LOG(DEBUG) << "Switching to state " << desiredState;
    LOG(DEBUG) << "  Current state " << mCurrentState
               << " has " << mCameraList[mCurrentState].size() << " cameras";
    LOG(DEBUG) << "  Desired state " << desiredState
               << " has " << mCameraList[desiredState].size() << " cameras";

    if (!isGlReady && !isSfReady()) {
        // Graphics is not ready yet; using CPU renderer.
        if (mCameraList[desiredState].size() >= 1) {
            mDesiredRenderer = std::make_unique<RenderPixelCopy>(mEvs,
                                                                 mCameraList[desiredState][0]);
            if (!mDesiredRenderer) {
                LOG(ERROR) << "Failed to construct Pixel Copy renderer.  Skipping state change.";
                return false;
            }
        } else {
            LOG(DEBUG) << "Unsupported, desiredState " << desiredState
                       << " has " << mCameraList[desiredState].size() << " cameras.";
        }
    } else {
        // Assumes that SurfaceFlinger is available always after being launched.

        // Do we need a new direct view renderer?
        if (mCameraList[desiredState].size() == 1) {
            // We have a camera assigned to this state for direct view.
            mDesiredRenderer = std::make_unique<RenderDirectView>(mEvs,
                                                                  mCameraDescList[desiredState][0],
                                                                  mConfig);
            if (!mDesiredRenderer) {
                LOG(ERROR) << "Failed to construct direct renderer.  Skipping state change.";
                return false;
            }
        } else if (mCameraList[desiredState].size() > 1 || desiredState == PARKING) {
            //TODO(b/140668179): RenderTopView needs to be updated to use new
            //                   ConfigManager.
            mDesiredRenderer = std::make_unique<RenderTopView>(mEvs,
                                                               mCameraList[desiredState],
                                                               mConfig);
            if (!mDesiredRenderer) {
                LOG(ERROR) << "Failed to construct top view renderer.  Skipping state change.";
                return false;
            }
        } else {
            LOG(DEBUG) << "Unsupported, desiredState " << desiredState
                       << " has " << mCameraList[desiredState].size() << " cameras.";
        }

        // GL renderer is now ready.
        isGlReady = true;
    }

    // Since we're changing states, shut down the current renderer
    if (mCurrentRenderer != nullptr) {
        mCurrentRenderer->deactivate();
        mCurrentRenderer = nullptr; // It's a smart pointer, so destructs on assignment to null
    }

    // Now set the display state based on whether we have a video feed to show
    if (mDesiredRenderer == nullptr) {
        LOG(DEBUG) << "Turning off the display";
        mDisplay->setDisplayState(EvsDisplayState::NOT_VISIBLE);
    } else {
        mCurrentRenderer = std::move(mDesiredRenderer);

        // Start the camera stream
        LOG(DEBUG) << "EvsStartCameraStreamTiming start time: "
                   << android::elapsedRealtime() << " ms.";
        if (!mCurrentRenderer->activate()) {
            LOG(ERROR) << "New renderer failed to activate";
            return false;
        }

        // Activate the display
        LOG(DEBUG) << "EvsActivateDisplayTiming start time: "
                   << android::elapsedRealtime() << " ms.";
        Return<EvsResult> result = mDisplay->setDisplayState(EvsDisplayState::VISIBLE_ON_NEXT_FRAME);
        if (result != EvsResult::OK) {
            LOG(ERROR) << "setDisplayState returned an error "
                       << result.description();
            return false;
        }
    }

    // Record our current state
    LOG(INFO) << "Activated state " << desiredState;
    mCurrentState = desiredState;

    return true;
}
