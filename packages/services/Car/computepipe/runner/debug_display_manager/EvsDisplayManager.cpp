// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <android/hardware/automotive/evs/1.1/IEvsDisplay.h>

#include <android-base/logging.h>
#include "EvsDisplayManager.h"

#include "PixelFormatUtils.h"
#include "RenderDirectView.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace debug_display_manager {
namespace {

constexpr char kServiceName[] = "default";

using android::hardware::automotive::evs::V1_1::IEvsEnumerator;
using android::hardware::automotive::evs::V1_1::IEvsDisplay;
using android::hardware::automotive::evs::V1_0::EvsResult;
using android::hardware::automotive::evs::V1_0::DisplayState;
using android::hardware::automotive::evs::V1_0::BufferDesc;

using android::automotive::evs::support::RenderDirectView;

BufferDesc getBufferDesc(const std::shared_ptr<MemHandle>& frame) {
    AHardwareBuffer_Desc hwDesc;
    AHardwareBuffer_describe(frame->getHardwareBuffer(), &hwDesc);

    BufferDesc buffer;
    buffer.width = hwDesc.width;
    buffer.height = hwDesc.height;
    buffer.stride = hwDesc.stride;
    buffer.pixelSize = numBytesPerPixel(static_cast<AHardwareBuffer_Format>(hwDesc.format));
    buffer.format = hwDesc.format;
    buffer.usage = hwDesc.usage;
    buffer.memHandle = AHardwareBuffer_getNativeHandle(frame->getHardwareBuffer());
    return buffer;
}

}  // namespace

EvsDisplayManager::~EvsDisplayManager() {
    stopThread();
}

Status EvsDisplayManager::setArgs(std::string displayManagerArgs) {
    auto pos = displayManagerArgs.find(kDisplayId);
    if (pos == std::string::npos) {
        return Status::SUCCESS;
    }
    mDisplayId = std::stoi(displayManagerArgs.substr(pos + strlen(kDisplayId)));
    mOverrideDisplayId = true;
    return Status::SUCCESS;
}

void EvsDisplayManager::stopThread() {
    {
        std::lock_guard<std::mutex> lk(mLock);
        mStopThread = true;
        mWait.notify_one();
    }
    if (mThread.joinable()) {
        mThread.join();
    }
}

void EvsDisplayManager::threadFn() {
    sp<IEvsEnumerator> evsEnumerator = IEvsEnumerator::getService(std::string() + kServiceName);

    if (!mOverrideDisplayId) {
        evsEnumerator->getDisplayIdList([this] (auto ids) {
                this->mDisplayId = ids[ids.size() - 1];
        });
    }

    android::sp <IEvsDisplay> evsDisplay = evsEnumerator->openDisplay_1_1(mDisplayId);

    if (evsDisplay != nullptr) {
        LOG(INFO) << "Computepipe runner opened debug display.";
    } else {
        mStopThread = true;
        LOG(ERROR) << "EVS Display unavailable.  Exiting thread.";
        return;
    }

    RenderDirectView evsRenderer;
    EvsResult result = evsDisplay->setDisplayState(DisplayState::VISIBLE_ON_NEXT_FRAME);
    if (result != EvsResult::OK) {
        mStopThread = true;
        LOG(ERROR) <<  "Set display state returned error - " << static_cast<int>(result);
        evsEnumerator->closeDisplay(evsDisplay);
        return;
    }

    if (!evsRenderer.activate()) {
        mStopThread = true;
        LOG(ERROR) <<  "Unable to activate evs renderer.";
        evsEnumerator->closeDisplay(evsDisplay);
        return;
    }

    std::unique_lock<std::mutex> lk(mLock);
    while (true) {
        mWait.wait(lk, [this]() { return mNextFrame != nullptr || mStopThread; });

        if (mStopThread) {
            // Free unused frame.
            if (mFreePacketCallback && mNextFrame) {
                mFreePacketCallback(mNextFrame->getBufferId());
            }
            break;
        }

        BufferDesc tgtBuffer = {};
        evsDisplay->getTargetBuffer([&tgtBuffer](const BufferDesc& buff) {
                tgtBuffer = buff;
                }
            );

        BufferDesc srcBuffer = getBufferDesc(mNextFrame);
        if (!evsRenderer.drawFrame(tgtBuffer, srcBuffer)) {
            LOG(ERROR) << "Error in rendering a frame.";
            mStopThread = true;
        }

        evsDisplay->returnTargetBufferForDisplay(tgtBuffer);
        if (mFreePacketCallback) {
            mFreePacketCallback(mNextFrame->getBufferId());
        }
        mNextFrame = nullptr;
    }

    LOG(INFO) << "Computepipe runner closing debug display.";
    evsRenderer.deactivate();
    (void)evsDisplay->setDisplayState(DisplayState::NOT_VISIBLE);
    evsEnumerator->closeDisplay(evsDisplay);
}

void EvsDisplayManager::setFreePacketCallback(
            std::function<Status(int bufferId)> freePacketCallback) {
    std::lock_guard<std::mutex> lk(mLock);
    mFreePacketCallback = freePacketCallback;
}

Status EvsDisplayManager::displayFrame(const std::shared_ptr<MemHandle>& dataHandle) {
    std::lock_guard<std::mutex> lk(mLock);
    Status status = Status::SUCCESS;
    if (mStopThread) {
        return Status::ILLEGAL_STATE;
    }
    if (mNextFrame != nullptr && mFreePacketCallback) {
        status = mFreePacketCallback(mNextFrame->getBufferId());
    }
    mNextFrame = dataHandle;
    mWait.notify_one();
    return status;
}

Status EvsDisplayManager::handleExecutionPhase(const RunnerEvent& e) {
    if (e.isPhaseEntry()) {
        std::lock_guard<std::mutex> lk(mLock);
        mStopThread = false;
        mThread = std::thread(&EvsDisplayManager::threadFn, this);
    } else if (e.isAborted()) {
        stopThread();
    }
    return Status::SUCCESS;
}

Status EvsDisplayManager::handleStopWithFlushPhase(const RunnerEvent& /* e */) {
    stopThread();
    return Status::SUCCESS;
}

Status EvsDisplayManager::handleStopImmediatePhase(const RunnerEvent& /* e */) {
    stopThread();
    return Status::SUCCESS;
}

Status EvsDisplayManager::handleResetPhase(const RunnerEvent& /* e */) {
    stopThread();
    return Status::SUCCESS;
}

}  // namespace debug_display_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
