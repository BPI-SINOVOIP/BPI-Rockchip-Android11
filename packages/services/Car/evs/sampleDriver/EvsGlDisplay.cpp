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

#include "EvsGlDisplay.h"

#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>
#include <utils/SystemClock.h>

using ::android::frameworks::automotive::display::V1_0::HwDisplayConfig;
using ::android::frameworks::automotive::display::V1_0::HwDisplayState;

namespace android {
namespace hardware {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

#ifdef EVS_DEBUG
static bool sDebugFirstFrameDisplayed = false;
#endif


EvsGlDisplay::EvsGlDisplay(sp<IAutomotiveDisplayProxyService> pDisplayProxy, uint64_t displayId)
    : mDisplayProxy(pDisplayProxy),
      mDisplayId(displayId) {
    LOG(DEBUG) << "EvsGlDisplay instantiated";

    // Set up our self description
    // NOTE:  These are arbitrary values chosen for testing
    mInfo.displayId             = "Mock Display";
    mInfo.vendorFlags           = 3870;
}


EvsGlDisplay::~EvsGlDisplay() {
    LOG(DEBUG) << "EvsGlDisplay being destroyed";
    forceShutdown();
}


/**
 * This gets called if another caller "steals" ownership of the display
 */
void EvsGlDisplay::forceShutdown()
{
    LOG(DEBUG) << "EvsGlDisplay forceShutdown";
    std::lock_guard<std::mutex> lock(mAccessLock);

    // If the buffer isn't being held by a remote client, release it now as an
    // optimization to release the resources more quickly than the destructor might
    // get called.
    if (mBuffer.memHandle) {
        // Report if we're going away while a buffer is outstanding
        if (mFrameBusy) {
            LOG(ERROR) << "EvsGlDisplay going down while client is holding a buffer";
        }

        // Drop the graphics buffer we've been using
        GraphicBufferAllocator& alloc(GraphicBufferAllocator::get());
        alloc.free(mBuffer.memHandle);
        mBuffer.memHandle = nullptr;

        mGlWrapper.hideWindow(mDisplayProxy, mDisplayId);
        mGlWrapper.shutdown();
    }

    // Put this object into an unrecoverable error state since somebody else
    // is going to own the display now.
    mRequestedState = EvsDisplayState::DEAD;
}


/**
 * Returns basic information about the EVS display provided by the system.
 * See the description of the DisplayDesc structure for details.
 */
Return<void> EvsGlDisplay::getDisplayInfo(getDisplayInfo_cb _hidl_cb)  {
    LOG(DEBUG) << __FUNCTION__;

    // Send back our self description
    _hidl_cb(mInfo);
    return Void();
}


/**
 * Clients may set the display state to express their desired state.
 * The HAL implementation must gracefully accept a request for any state
 * while in any other state, although the response may be to ignore the request.
 * The display is defined to start in the NOT_VISIBLE state upon initialization.
 * The client is then expected to request the VISIBLE_ON_NEXT_FRAME state, and
 * then begin providing video.  When the display is no longer required, the client
 * is expected to request the NOT_VISIBLE state after passing the last video frame.
 */
Return<EvsResult> EvsGlDisplay::setDisplayState(EvsDisplayState state) {
    LOG(DEBUG) << __FUNCTION__;
    std::lock_guard<std::mutex> lock(mAccessLock);

    if (mRequestedState == EvsDisplayState::DEAD) {
        // This object no longer owns the display -- it's been superceeded!
        return EvsResult::OWNERSHIP_LOST;
    }

    // Ensure we recognize the requested state so we don't go off the rails
    if (state >= EvsDisplayState::NUM_STATES) {
        return EvsResult::INVALID_ARG;
    }

    switch (state) {
    case EvsDisplayState::NOT_VISIBLE:
        mGlWrapper.hideWindow(mDisplayProxy, mDisplayId);
        break;
    case EvsDisplayState::VISIBLE:
        mGlWrapper.showWindow(mDisplayProxy, mDisplayId);
        break;
    default:
        break;
    }

    // Record the requested state
    mRequestedState = state;

    return EvsResult::OK;
}


/**
 * The HAL implementation should report the actual current state, which might
 * transiently differ from the most recently requested state.  Note, however, that
 * the logic responsible for changing display states should generally live above
 * the device layer, making it undesirable for the HAL implementation to
 * spontaneously change display states.
 */
Return<EvsDisplayState> EvsGlDisplay::getDisplayState()  {
    LOG(DEBUG) << __FUNCTION__;
    std::lock_guard<std::mutex> lock(mAccessLock);

    return mRequestedState;
}


/**
 * This call returns a handle to a frame buffer associated with the display.
 * This buffer may be locked and written to by software and/or GL.  This buffer
 * must be returned via a call to returnTargetBufferForDisplay() even if the
 * display is no longer visible.
 */
Return<void> EvsGlDisplay::getTargetBuffer(getTargetBuffer_cb _hidl_cb)  {
    LOG(DEBUG) << __FUNCTION__;
    std::lock_guard<std::mutex> lock(mAccessLock);

    if (mRequestedState == EvsDisplayState::DEAD) {
        LOG(ERROR) << "Rejecting buffer request from object that lost ownership of the display.";
        _hidl_cb({});
        return Void();
    }

    // If we don't already have a buffer, allocate one now
    if (!mBuffer.memHandle) {
        // Initialize our display window
        // NOTE:  This will cause the display to become "VISIBLE" before a frame is actually
        // returned, which is contrary to the spec and will likely result in a black frame being
        // (briefly) shown.
        if (!mGlWrapper.initialize(mDisplayProxy, mDisplayId)) {
            // Report the failure
            LOG(ERROR) << "Failed to initialize GL display";
            _hidl_cb({});
            return Void();
        }

        // Assemble the buffer description we'll use for our render target
        mBuffer.width       = mGlWrapper.getWidth();
        mBuffer.height      = mGlWrapper.getHeight();
        mBuffer.format      = HAL_PIXEL_FORMAT_RGBA_8888;
        mBuffer.usage       = GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER;
        mBuffer.bufferId    = 0x3870;  // Arbitrary magic number for self recognition
        mBuffer.pixelSize   = 4;

        // Allocate the buffer that will hold our displayable image
        buffer_handle_t handle = nullptr;
        GraphicBufferAllocator& alloc(GraphicBufferAllocator::get());
        status_t result = alloc.allocate(mBuffer.width, mBuffer.height,
                                         mBuffer.format, 1,
                                         mBuffer.usage, &handle,
                                         &mBuffer.stride,
                                         0, "EvsGlDisplay");
        if (result != NO_ERROR) {
            LOG(ERROR) << "Error " << result
                       << " allocating " << mBuffer.width << " x " << mBuffer.height
                       << " graphics buffer.";
            _hidl_cb({});
            mGlWrapper.shutdown();
            return Void();
        }
        if (!handle) {
            LOG(ERROR) << "We didn't get a buffer handle back from the allocator";
            _hidl_cb({});
            mGlWrapper.shutdown();
            return Void();
        }

        mBuffer.memHandle = handle;
        LOG(DEBUG) << "Allocated new buffer " << mBuffer.memHandle.getNativeHandle()
                   << " with stride " <<  mBuffer.stride;
        mFrameBusy = false;
    }

    // Do we have a frame available?
    if (mFrameBusy) {
        // This means either we have a 2nd client trying to compete for buffers
        // (an unsupported mode of operation) or else the client hasn't returned
        // a previously issued buffer yet (they're behaving badly).
        // NOTE:  We have to make the callback even if we have nothing to provide
        LOG(ERROR) << "getTargetBuffer called while no buffers available.";
        _hidl_cb({});
        return Void();
    } else {
        // Mark our buffer as busy
        mFrameBusy = true;

        // Send the buffer to the client
        LOG(VERBOSE) << "Providing display buffer handle " << mBuffer.memHandle.getNativeHandle()
                     << " as id " << mBuffer.bufferId;
        _hidl_cb(mBuffer);
        return Void();
    }
}


/**
 * This call tells the display that the buffer is ready for display.
 * The buffer is no longer valid for use by the client after this call.
 */
Return<EvsResult> EvsGlDisplay::returnTargetBufferForDisplay(const BufferDesc_1_0& buffer)  {
    LOG(VERBOSE) << __FUNCTION__ << " " << buffer.memHandle.getNativeHandle();
    std::lock_guard<std::mutex> lock(mAccessLock);

    // Nobody should call us with a null handle
    if (!buffer.memHandle.getNativeHandle()) {
        LOG(ERROR) << __FUNCTION__
                   << " called without a valid buffer handle.";
        return EvsResult::INVALID_ARG;
    }
    if (buffer.bufferId != mBuffer.bufferId) {
        LOG(ERROR) << "Got an unrecognized frame returned.";
        return EvsResult::INVALID_ARG;
    }
    if (!mFrameBusy) {
        LOG(ERROR) << "A frame was returned with no outstanding frames.";
        return EvsResult::BUFFER_NOT_AVAILABLE;
    }

    mFrameBusy = false;

    // If we've been displaced by another owner of the display, then we can't do anything else
    if (mRequestedState == EvsDisplayState::DEAD) {
        return EvsResult::OWNERSHIP_LOST;
    }

    // If we were waiting for a new frame, this is it!
    if (mRequestedState == EvsDisplayState::VISIBLE_ON_NEXT_FRAME) {
        mRequestedState = EvsDisplayState::VISIBLE;
        mGlWrapper.showWindow(mDisplayProxy, mDisplayId);
    }

    // Validate we're in an expected state
    if (mRequestedState != EvsDisplayState::VISIBLE) {
        // Not sure why a client would send frames back when we're not visible.
        LOG(WARNING) << "Got a frame returned while not visible - ignoring.";
    } else {
        // Update the texture contents with the provided data
// TODO:  Why doesn't it work to pass in the buffer handle we got from HIDL?
//        if (!mGlWrapper.updateImageTexture(buffer)) {
        if (!mGlWrapper.updateImageTexture(mBuffer)) {
            return EvsResult::UNDERLYING_SERVICE_ERROR;
        }

        // Put the image on the screen
        mGlWrapper.renderImageToScreen();
#ifdef EVS_DEBUG
        if (!sDebugFirstFrameDisplayed) {
            LOG(DEBUG) << "EvsFirstFrameDisplayTiming start time: "
                       << elapsedRealtime() << " ms.";
            sDebugFirstFrameDisplayed = true;
        }
#endif

    }

    return EvsResult::OK;
}


Return<void> EvsGlDisplay::getDisplayInfo_1_1(getDisplayInfo_1_1_cb _info_cb) {
    if (mDisplayProxy != nullptr) {
        return mDisplayProxy->getDisplayInfo(mDisplayId, _info_cb);
    } else {
        _info_cb({}, {});
        return Void();
    }
}


} // namespace implementation
} // namespace V1_1
} // namespace evs
} // namespace automotive
} // namespace hardware
} // namespace android
