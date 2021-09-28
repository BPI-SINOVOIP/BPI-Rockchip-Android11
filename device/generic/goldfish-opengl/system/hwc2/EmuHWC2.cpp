/*
 * Copyright 2018 The Android Open Source Project
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

#include "EmuHWC2.h"
//#define LOG_NDEBUG 0
//#define LOG_NNDEBUG 0
#undef LOG_TAG
#define LOG_TAG "EmuHWC2"

#include <errno.h>
#include <cutils/properties.h>
#include <log/log.h>
#include <sync/sync.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferAllocator.h>

#include "../egl/goldfish_sync.h"

#include "ThreadInfo.h"

#if defined(LOG_NNDEBUG) && LOG_NNDEBUG == 0
#define ALOGVV ALOGV
#else
#define ALOGVV(...) ((void)0)
#endif

template <typename PFN, typename T>
static hwc2_function_pointer_t asFP(T function)
{
    static_assert(std::is_same<PFN, T>::value, "Incompatible function pointer");
    return reinterpret_cast<hwc2_function_pointer_t>(function);
}

static HostConnection *sHostCon = nullptr;

static HostConnection* createOrGetHostConnection() {
    if (!sHostCon) {
        sHostCon = HostConnection::createUnique();
    }
    return sHostCon;
}

#define DEFINE_AND_VALIDATE_HOST_CONNECTION \
    HostConnection *hostCon = createOrGetHostConnection(); \
    if (!hostCon) { \
        ALOGE("EmuHWC2: Failed to get host connection\n"); \
        return Error::NoResources; \
    } \
    ExtendedRCEncoderContext *rcEnc = hostCon->rcEncoder(); \
    if (!rcEnc) { \
        ALOGE("EmuHWC2: Failed to get renderControl encoder context\n"); \
        return Error::NoResources; \
    }

using namespace HWC2;

namespace android {

EmuHWC2::EmuHWC2()
  : mStateMutex()
{
    common.tag = HARDWARE_DEVICE_TAG;
    common.version = HWC_DEVICE_API_VERSION_2_0;
    common.close = closeHook;
    getCapabilities = getCapabilitiesHook;
    getFunction = getFunctionHook;
    populateCapabilities();
    initDisplayParameters();
}

Error EmuHWC2::initDisplayParameters() {
    DEFINE_AND_VALIDATE_HOST_CONNECTION
    hostCon->lock();

    mDisplayWidth = rcEnc->rcGetFBParam(rcEnc, FB_WIDTH);
    mDisplayHeight = rcEnc->rcGetFBParam(rcEnc, FB_HEIGHT);
    mDisplayDpiX = rcEnc->rcGetFBParam(rcEnc, FB_XDPI);
    mDisplayDpiY = rcEnc->rcGetFBParam(rcEnc, FB_YDPI);

    hostCon->unlock();

    return HWC2::Error::None;
}

void EmuHWC2::doGetCapabilities(uint32_t* outCount, int32_t* outCapabilities) {
    if (outCapabilities == nullptr) {
        *outCount = mCapabilities.size();
        return;
    }

    auto capabilityIter = mCapabilities.cbegin();
    for (size_t i = 0; i < *outCount; ++i) {
        if (capabilityIter == mCapabilities.cend()) {
            return;
        }
        outCapabilities[i] = static_cast<int32_t>(*capabilityIter);
        ++capabilityIter;
    }
}

hwc2_function_pointer_t EmuHWC2::doGetFunction(
        FunctionDescriptor descriptor) {
    switch(descriptor) {
        case FunctionDescriptor::CreateVirtualDisplay:
            return asFP<HWC2_PFN_CREATE_VIRTUAL_DISPLAY>(
                    createVirtualDisplayHook);
        case FunctionDescriptor::DestroyVirtualDisplay:
            return asFP<HWC2_PFN_DESTROY_VIRTUAL_DISPLAY>(
                    destroyVirtualDisplayHook);
        case FunctionDescriptor::Dump:
            return asFP<HWC2_PFN_DUMP>(dumpHook);
        case FunctionDescriptor::GetMaxVirtualDisplayCount:
            return asFP<HWC2_PFN_GET_MAX_VIRTUAL_DISPLAY_COUNT>(
                    getMaxVirtualDisplayCountHook);
        case FunctionDescriptor::RegisterCallback:
            return asFP<HWC2_PFN_REGISTER_CALLBACK>(registerCallbackHook);

            // Display functions
        case FunctionDescriptor::AcceptDisplayChanges:
            return asFP<HWC2_PFN_ACCEPT_DISPLAY_CHANGES>(
                    displayHook<decltype(&Display::acceptChanges),
                    &Display::acceptChanges>);
        case FunctionDescriptor::CreateLayer:
            return asFP<HWC2_PFN_CREATE_LAYER>(
                    displayHook<decltype(&Display::createLayer),
                    &Display::createLayer, hwc2_layer_t*>);
        case FunctionDescriptor::DestroyLayer:
            return asFP<HWC2_PFN_DESTROY_LAYER>(
                    displayHook<decltype(&Display::destroyLayer),
                    &Display::destroyLayer, hwc2_layer_t>);
        case FunctionDescriptor::GetActiveConfig:
            return asFP<HWC2_PFN_GET_ACTIVE_CONFIG>(
                    displayHook<decltype(&Display::getActiveConfig),
                    &Display::getActiveConfig, hwc2_config_t*>);
        case FunctionDescriptor::GetChangedCompositionTypes:
            return asFP<HWC2_PFN_GET_CHANGED_COMPOSITION_TYPES>(
                    displayHook<decltype(&Display::getChangedCompositionTypes),
                    &Display::getChangedCompositionTypes, uint32_t*,
                    hwc2_layer_t*, int32_t*>);
        case FunctionDescriptor::GetColorModes:
            return asFP<HWC2_PFN_GET_COLOR_MODES>(
                    displayHook<decltype(&Display::getColorModes),
                    &Display::getColorModes, uint32_t*, int32_t*>);
        case FunctionDescriptor::GetDisplayAttribute:
            return asFP<HWC2_PFN_GET_DISPLAY_ATTRIBUTE>(
                    displayHook<decltype(&Display::getDisplayAttribute),
                    &Display::getDisplayAttribute, hwc2_config_t,
                    int32_t, int32_t*>);
        case FunctionDescriptor::GetDisplayConfigs:
            return asFP<HWC2_PFN_GET_DISPLAY_CONFIGS>(
                    displayHook<decltype(&Display::getConfigs),
                    &Display::getConfigs, uint32_t*, hwc2_config_t*>);
        case FunctionDescriptor::GetDisplayName:
            return asFP<HWC2_PFN_GET_DISPLAY_NAME>(
                    displayHook<decltype(&Display::getName),
                    &Display::getName, uint32_t*, char*>);
        case FunctionDescriptor::GetDisplayRequests:
            return asFP<HWC2_PFN_GET_DISPLAY_REQUESTS>(
                    displayHook<decltype(&Display::getRequests),
                    &Display::getRequests, int32_t*, uint32_t*, hwc2_layer_t*,
                    int32_t*>);
        case FunctionDescriptor::GetDisplayType:
            return asFP<HWC2_PFN_GET_DISPLAY_TYPE>(
                    displayHook<decltype(&Display::getType),
                    &Display::getType, int32_t*>);
        case FunctionDescriptor::GetDozeSupport:
            return asFP<HWC2_PFN_GET_DOZE_SUPPORT>(
                    displayHook<decltype(&Display::getDozeSupport),
                    &Display::getDozeSupport, int32_t*>);
        case FunctionDescriptor::GetHdrCapabilities:
            return asFP<HWC2_PFN_GET_HDR_CAPABILITIES>(
                    displayHook<decltype(&Display::getHdrCapabilities),
                    &Display::getHdrCapabilities, uint32_t*, int32_t*, float*,
                    float*, float*>);
        case FunctionDescriptor::GetReleaseFences:
            return asFP<HWC2_PFN_GET_RELEASE_FENCES>(
                    displayHook<decltype(&Display::getReleaseFences),
                    &Display::getReleaseFences, uint32_t*, hwc2_layer_t*,
                    int32_t*>);
        case FunctionDescriptor::PresentDisplay:
            return asFP<HWC2_PFN_PRESENT_DISPLAY>(
                    displayHook<decltype(&Display::present),
                    &Display::present, int32_t*>);
        case FunctionDescriptor::SetActiveConfig:
            return asFP<HWC2_PFN_SET_ACTIVE_CONFIG>(
                    displayHook<decltype(&Display::setActiveConfig),
                    &Display::setActiveConfig, hwc2_config_t>);
        case FunctionDescriptor::SetClientTarget:
            return asFP<HWC2_PFN_SET_CLIENT_TARGET>(
                    displayHook<decltype(&Display::setClientTarget),
                    &Display::setClientTarget, buffer_handle_t, int32_t,
                    int32_t, hwc_region_t>);
        case FunctionDescriptor::SetColorMode:
            return asFP<HWC2_PFN_SET_COLOR_MODE>(
                    displayHook<decltype(&Display::setColorMode),
                    &Display::setColorMode, int32_t>);
        case FunctionDescriptor::SetColorTransform:
            return asFP<HWC2_PFN_SET_COLOR_TRANSFORM>(
                    displayHook<decltype(&Display::setColorTransform),
                    &Display::setColorTransform, const float*, int32_t>);
        case FunctionDescriptor::SetOutputBuffer:
            return asFP<HWC2_PFN_SET_OUTPUT_BUFFER>(
                    displayHook<decltype(&Display::setOutputBuffer),
                    &Display::setOutputBuffer, buffer_handle_t, int32_t>);
        case FunctionDescriptor::SetPowerMode:
            return asFP<HWC2_PFN_SET_POWER_MODE>(
                    displayHook<decltype(&Display::setPowerMode),
                    &Display::setPowerMode, int32_t>);
        case FunctionDescriptor::SetVsyncEnabled:
            return asFP<HWC2_PFN_SET_VSYNC_ENABLED>(
                    displayHook<decltype(&Display::setVsyncEnabled),
                    &Display::setVsyncEnabled, int32_t>);
        case FunctionDescriptor::ValidateDisplay:
            return asFP<HWC2_PFN_VALIDATE_DISPLAY>(
                    displayHook<decltype(&Display::validate),
                    &Display::validate, uint32_t*, uint32_t*>);
        case FunctionDescriptor::GetClientTargetSupport:
            return asFP<HWC2_PFN_GET_CLIENT_TARGET_SUPPORT>(
                    displayHook<decltype(&Display::getClientTargetSupport),
                    &Display::getClientTargetSupport, uint32_t, uint32_t,
                                                      int32_t, int32_t>);
        // 2.3 required functions
        case FunctionDescriptor::GetDisplayIdentificationData:
            return asFP<HWC2_PFN_GET_DISPLAY_IDENTIFICATION_DATA>(
                    displayHook<decltype(&Display::getDisplayIdentificationData),
                    &Display::getDisplayIdentificationData, uint8_t*, uint32_t*, uint8_t*>);
        case FunctionDescriptor::GetDisplayCapabilities:
            return asFP<HWC2_PFN_GET_DISPLAY_CAPABILITIES>(
                    displayHook<decltype(&Display::getDisplayCapabilities),
                    &Display::getDisplayCapabilities, uint32_t*, uint32_t*>);
        case FunctionDescriptor::GetDisplayBrightnessSupport:
            return asFP<HWC2_PFN_GET_DISPLAY_BRIGHTNESS_SUPPORT>(
                    displayHook<decltype(&Display::getDisplayBrightnessSupport),
                    &Display::getDisplayBrightnessSupport, bool*>);
        case FunctionDescriptor::SetDisplayBrightness:
            return asFP<HWC2_PFN_SET_DISPLAY_BRIGHTNESS>(
                    displayHook<decltype(&Display::setDisplayBrightness),
                    &Display::setDisplayBrightness, float>);
        // Layer functions
        case FunctionDescriptor::SetCursorPosition:
            return asFP<HWC2_PFN_SET_CURSOR_POSITION>(
                    layerHook<decltype(&Layer::setCursorPosition),
                    &Layer::setCursorPosition, int32_t, int32_t>);
        case FunctionDescriptor::SetLayerBuffer:
            return asFP<HWC2_PFN_SET_LAYER_BUFFER>(
                    layerHook<decltype(&Layer::setBuffer), &Layer::setBuffer,
                    buffer_handle_t, int32_t>);
        case FunctionDescriptor::SetLayerSurfaceDamage:
            return asFP<HWC2_PFN_SET_LAYER_SURFACE_DAMAGE>(
                    layerHook<decltype(&Layer::setSurfaceDamage),
                    &Layer::setSurfaceDamage, hwc_region_t>);

        // Layer state functions
        case FunctionDescriptor::SetLayerBlendMode:
            return asFP<HWC2_PFN_SET_LAYER_BLEND_MODE>(
                    layerHook<decltype(&Layer::setBlendMode),
                    &Layer::setBlendMode, int32_t>);
        case FunctionDescriptor::SetLayerColor:
            return asFP<HWC2_PFN_SET_LAYER_COLOR>(
                    layerHook<decltype(&Layer::setColor), &Layer::setColor,
                    hwc_color_t>);
        case FunctionDescriptor::SetLayerCompositionType:
            return asFP<HWC2_PFN_SET_LAYER_COMPOSITION_TYPE>(
                    layerHook<decltype(&Layer::setCompositionType),
                    &Layer::setCompositionType, int32_t>);
        case FunctionDescriptor::SetLayerDataspace:
            return asFP<HWC2_PFN_SET_LAYER_DATASPACE>(
                    layerHook<decltype(&Layer::setDataspace),
                    &Layer::setDataspace, int32_t>);
        case FunctionDescriptor::SetLayerDisplayFrame:
            return asFP<HWC2_PFN_SET_LAYER_DISPLAY_FRAME>(
                    layerHook<decltype(&Layer::setDisplayFrame),
                    &Layer::setDisplayFrame, hwc_rect_t>);
        case FunctionDescriptor::SetLayerPlaneAlpha:
            return asFP<HWC2_PFN_SET_LAYER_PLANE_ALPHA>(
                    layerHook<decltype(&Layer::setPlaneAlpha),
                    &Layer::setPlaneAlpha, float>);
        case FunctionDescriptor::SetLayerSidebandStream:
            return asFP<HWC2_PFN_SET_LAYER_SIDEBAND_STREAM>(
                    layerHook<decltype(&Layer::setSidebandStream),
                    &Layer::setSidebandStream, const native_handle_t*>);
        case FunctionDescriptor::SetLayerSourceCrop:
            return asFP<HWC2_PFN_SET_LAYER_SOURCE_CROP>(
                    layerHook<decltype(&Layer::setSourceCrop),
                    &Layer::setSourceCrop, hwc_frect_t>);
        case FunctionDescriptor::SetLayerTransform:
            return asFP<HWC2_PFN_SET_LAYER_TRANSFORM>(
                    layerHook<decltype(&Layer::setTransform),
                    &Layer::setTransform, int32_t>);
        case FunctionDescriptor::SetLayerVisibleRegion:
            return asFP<HWC2_PFN_SET_LAYER_VISIBLE_REGION>(
                    layerHook<decltype(&Layer::setVisibleRegion),
                    &Layer::setVisibleRegion, hwc_region_t>);
        case FunctionDescriptor::SetLayerZOrder:
            return asFP<HWC2_PFN_SET_LAYER_Z_ORDER>(
                    displayHook<decltype(&Display::updateLayerZ),
                    &Display::updateLayerZ, hwc2_layer_t, uint32_t>);

        default:
            ALOGE("doGetFunction: Unknown function descriptor: %d (%s)",
                    static_cast<int32_t>(descriptor),
                    to_string(descriptor).c_str());
            return nullptr;
    }
}


// Device functions

Error EmuHWC2::createVirtualDisplay(uint32_t /*width*/, uint32_t /*height*/,
        int32_t* /*format*/, hwc2_display_t* /*outDisplay*/) {
    ALOGVV("%s", __FUNCTION__);
    //TODO: VirtualDisplay support
    return Error::None;
}

Error EmuHWC2::destroyVirtualDisplay(hwc2_display_t /*displayId*/) {
    ALOGVV("%s", __FUNCTION__);
    //TODO: VirtualDisplay support
    return Error::None;
}

void EmuHWC2::dump(uint32_t* /*outSize*/, char* /*outBuffer*/) {
    ALOGVV("%s", __FUNCTION__);
    //TODO:
    return;
}

uint32_t EmuHWC2::getMaxVirtualDisplayCount() {
    ALOGVV("%s", __FUNCTION__);
    //TODO: VirtualDisplay support
    return 0;
}

static bool isValid(Callback descriptor) {
    switch (descriptor) {
        case Callback::Hotplug: // Fall-through
        case Callback::Refresh: // Fall-through
        case Callback::Vsync: return true;
        default: return false;
    }
}

Error EmuHWC2::registerCallback(Callback descriptor,
        hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer) {
    ALOGVV("%s", __FUNCTION__);
    if (!isValid(descriptor)) {
        ALOGE("registerCallback: Unkown function descriptor: %d",
                static_cast<int32_t>(descriptor));
        return Error::BadParameter;
    }
    ALOGV("registerCallback(%s, %p, %p)", to_string(descriptor).c_str(),
            callbackData, pointer);

    std::unique_lock<std::mutex> lock(mStateMutex);

    if (pointer != nullptr) {
        mCallbacks[descriptor] = {callbackData, pointer};
    }
    else {
        ALOGV("unregisterCallback(%s)", to_string(descriptor).c_str());
        mCallbacks.erase(descriptor);
        return Error::None;
    }

    // Callback without the state lock held
    if (descriptor == Callback::Hotplug) {
        lock.unlock();
        auto hotplug = reinterpret_cast<HWC2_PFN_VSYNC>(pointer);
        for (const auto& iter : mDisplays) {
            hotplug(callbackData, iter.first, static_cast<int32_t>(Connection::Connected));
        }
    }

    return Error::None;
}

const native_handle_t* EmuHWC2::allocateDisplayColorBuffer() {
    const uint32_t layerCount = 1;
    const uint64_t graphicBufferId = 0; // not used

    buffer_handle_t h;
    uint32_t stride;

    if (GraphicBufferAllocator::get().allocate(
        mDisplayWidth, mDisplayHeight,
        PIXEL_FORMAT_RGBA_8888,
        layerCount,
        (GraphicBuffer::USAGE_HW_COMPOSER | GraphicBuffer::USAGE_HW_RENDER),
        &h, &stride,
        graphicBufferId, "EmuHWC2") == OK) {
        return static_cast<const native_handle_t*>(h);
    } else {
        return nullptr;
    }
}

void EmuHWC2::freeDisplayColorBuffer(const native_handle_t* h) {
    GraphicBufferAllocator::get().free(h);
}

// Display functions

#define VSYNC_PERIOD_PROP "ro.kernel.qemu.vsync"

static int getVsyncPeriodFromProperty() {
    char displaysValue[PROPERTY_VALUE_MAX] = "";
    property_get(VSYNC_PERIOD_PROP, displaysValue, "");
    bool isValid = displaysValue[0] != '\0';

    if (!isValid) return 60;

    long vsyncPeriodParsed = strtol(displaysValue, 0, 10);

    // On failure, strtol returns 0. Also, there's no reason to have 0
    // as the vsync period.
    if (!vsyncPeriodParsed) return 60;

    return static_cast<int>(vsyncPeriodParsed);
}

std::atomic<hwc2_display_t> EmuHWC2::Display::sNextId(0);

EmuHWC2::Display::Display(EmuHWC2& device, DisplayType type)
  : mDevice(device),
    mId(sNextId++),
    mHostDisplayId(0),
    mName(),
    mType(type),
    mPowerMode(PowerMode::Off),
    mVsyncEnabled(Vsync::Invalid),
    mVsyncPeriod(1000*1000*1000/getVsyncPeriodFromProperty()), // vsync is 60 hz
    mVsyncThread(*this),
    mClientTarget(),
    mChanges(),
    mLayers(),
    mReleaseLayerIds(),
    mReleaseFences(),
    mConfigs(),
    mActiveConfig(nullptr),
    mColorModes(),
    mSetColorTransform(false),
    mStateMutex() {
        mVsyncThread.run("", -19 /* ANDROID_PRIORITY_URGENT_AUDIO */);
        mTargetCb = device.allocateDisplayColorBuffer();
}

EmuHWC2::Display::~Display() {
    mDevice.freeDisplayColorBuffer(mTargetCb);
}

Error EmuHWC2::Display::acceptChanges() {
    ALOGVV("%s: displayId %u", __FUNCTION__, (uint32_t)mId);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (!mChanges) {
        ALOGW("%s: displayId %u acceptChanges failed, not validated",
              __FUNCTION__, (uint32_t)mId);
        return Error::NotValidated;
    }


    for (auto& change : mChanges->getTypeChanges()) {
        auto layerId = change.first;
        auto type = change.second;
        if (mDevice.mLayers.count(layerId) == 0) {
            // This should never happen but somehow does.
            ALOGW("Cannot accept change for unknown layer %u",
                  (uint32_t)layerId);
            continue;
        }
        auto layer = mDevice.mLayers[layerId];
        layer->setCompositionType((int32_t)type);
    }

    mChanges->clearTypeChanges();
    return Error::None;
}

Error EmuHWC2::Display::createLayer(hwc2_layer_t* outLayerId) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    auto layer = *mLayers.emplace(std::make_shared<Layer>(*this));
    mDevice.mLayers.emplace(std::make_pair(layer->getId(), layer));
    *outLayerId = layer->getId();
    ALOGV("%s: Display %u created layer %u", __FUNCTION__, (uint32_t)mId,
         (uint32_t)(*outLayerId));
    return Error::None;
}

Error EmuHWC2::Display::destroyLayer(hwc2_layer_t layerId) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    const auto mapLayer = mDevice.mLayers.find(layerId);
    if (mapLayer == mDevice.mLayers.end()) {
        ALOGW("%s failed: no such layer, displayId %u layerId %u",
             __FUNCTION__, (uint32_t)mId, (uint32_t)layerId);
        return Error::BadLayer;
    }
    const auto layer = mapLayer->second;
    mDevice.mLayers.erase(mapLayer);
    const auto zRange = mLayers.equal_range(layer);
    for (auto current = zRange.first; current != zRange.second; ++current) {
        if (**current == *layer) {
            current = mLayers.erase(current);
            break;
        }
    }
    ALOGV("%s: displayId %d layerId %d", __FUNCTION__, (uint32_t)mId,
         (uint32_t)layerId);
    return Error::None;
}

Error EmuHWC2::Display::getActiveConfig(hwc2_config_t* outConfig) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (!mActiveConfig) {
        ALOGW("%s: displayId %d %s", __FUNCTION__, (uint32_t)mId,
                to_string(Error::BadConfig).c_str());
        return Error::BadConfig;
    }
    auto configId = mActiveConfig->getId();
    ALOGV("%s: displayId %d configId %d", __FUNCTION__,
          (uint32_t)mId, (uint32_t)configId);
    *outConfig = configId;
    return Error::None;
}

Error EmuHWC2::Display::getDisplayAttribute(hwc2_config_t configId,
        int32_t attribute, int32_t* outValue) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (configId > mConfigs.size() || !mConfigs[configId]->isOnDisplay(*this)) {
        ALOGW("%s: bad config (%u %u)", __FUNCTION__, (uint32_t)mId, configId);
        return Error::BadConfig;
    }
    *outValue = mConfigs[configId]->getAttribute((Attribute)attribute);
    ALOGV("%s: (%d %d) %s --> %d", __FUNCTION__,
          (uint32_t)mId, (uint32_t)configId,
          to_string((Attribute)attribute).c_str(), *outValue);
    return Error::None;
}

Error EmuHWC2::Display::getChangedCompositionTypes(
        uint32_t* outNumElements, hwc2_layer_t* outLayers, int32_t* outTypes) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (!mChanges) {
        ALOGW("display %u getChangedCompositionTypes failed: not validated",
                (uint32_t)mId);
        return Error::NotValidated;
    }

    if ((outLayers == nullptr) || (outTypes == nullptr)) {
        *outNumElements = mChanges->getTypeChanges().size();
        return Error::None;
    }

    uint32_t numWritten = 0;
    for (const auto& element : mChanges->getTypeChanges()) {
        if (numWritten == *outNumElements) {
            break;
        }
        auto layerId = element.first;
        auto intType = static_cast<int32_t>(element.second);
        ALOGV("%s: Adding layer %u %s", __FUNCTION__, (uint32_t)layerId,
                to_string(element.second).c_str());
        outLayers[numWritten] = layerId;
        outTypes[numWritten] = intType;
        ++numWritten;
    }
    *outNumElements = numWritten;
    return Error::None;
}

Error EmuHWC2::Display::getColorModes(uint32_t* outNumModes,
        int32_t* outModes) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (!outModes) {
        *outNumModes = mColorModes.size();
        return Error::None;
    }

    // we only support HAL_COLOR_MODE_NATIVE so far
    uint32_t numModes = std::min(*outNumModes,
            static_cast<uint32_t>(mColorModes.size()));
    std::copy_n(mColorModes.cbegin(), numModes, outModes);
    *outNumModes = numModes;
    return Error::None;
}

Error EmuHWC2::Display::getConfigs(uint32_t* outNumConfigs,
        hwc2_config_t* outConfigs) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (!outConfigs) {
        *outNumConfigs = mConfigs.size();
        return Error::None;
    }
    uint32_t numWritten = 0;
    for (const auto config : mConfigs) {
        if (numWritten == *outNumConfigs) {
            break;
        }
        outConfigs[numWritten] = config->getId();
        ++numWritten;
    }
    *outNumConfigs = numWritten;
    return Error::None;
}

Error EmuHWC2::Display::getDozeSupport(int32_t* outSupport) {
    ALOGVV("%s", __FUNCTION__);
    // We don't support so far
    *outSupport = 0;
    return Error::None;
}

Error EmuHWC2::Display::getHdrCapabilities(uint32_t* outNumTypes,
        int32_t* /*outTypes*/, float* /*outMaxLuminance*/,
        float* /*outMaxAverageLuminance*/, float* /*outMinLuminance*/) {
    ALOGVV("%s", __FUNCTION__);
    // We don't support so far
    *outNumTypes = 0;
    return Error::None;
}

Error EmuHWC2::Display::getName(uint32_t* outSize, char* outName) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (!outName) {
        *outSize = mName.size();
        return Error::None;
    }
    auto numCopied = mName.copy(outName, *outSize);
    *outSize = numCopied;
    return Error::None;
}

Error EmuHWC2::Display::getReleaseFences(uint32_t* outNumElements,
        hwc2_layer_t* outLayers, int32_t* outFences) {
    ALOGVV("%s", __FUNCTION__);

    *outNumElements = mReleaseLayerIds.size();

    ALOGVV("%s. Got %u elements", __FUNCTION__, *outNumElements);

    if (*outNumElements && outLayers) {
        ALOGVV("%s. export release layers", __FUNCTION__);
        memcpy(outLayers, mReleaseLayerIds.data(),
               sizeof(hwc2_layer_t) * (*outNumElements));
    }

    if (*outNumElements && outFences) {
        ALOGVV("%s. export release fences", __FUNCTION__);
        memcpy(outFences, mReleaseFences.data(),
               sizeof(int32_t) * (*outNumElements));
    }

    return Error::None;
}

Error EmuHWC2::Display::getRequests(int32_t* outDisplayRequests,
        uint32_t* outNumElements, hwc2_layer_t* outLayers,
        int32_t* outLayerRequests) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (!mChanges) {
        return Error::NotValidated;
    }

    if (outLayers == nullptr || outLayerRequests == nullptr) {
        *outNumElements = mChanges->getNumLayerRequests();
        return Error::None;
    }

    //TODO
    // Display requests (HWC2::DisplayRequest) are not supported so far:
    *outDisplayRequests = 0;

    uint32_t numWritten = 0;
    for (const auto& request : mChanges->getLayerRequests()) {
        if (numWritten == *outNumElements) {
            break;
        }
        outLayers[numWritten] = request.first;
        outLayerRequests[numWritten] = static_cast<int32_t>(request.second);
        ++numWritten;
    }

    return Error::None;
}

Error EmuHWC2::Display::getType(int32_t* outType) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    *outType = (int32_t)mType;
    return Error::None;
}

Error EmuHWC2::Display::present(int32_t* outRetireFence) {
    ALOGVV("%s", __FUNCTION__);

    *outRetireFence = -1;

    std::unique_lock<std::mutex> lock(mStateMutex);

    if (!mChanges || (mChanges->getNumTypes() > 0)) {
        ALOGE("%s display(%u) set failed: not validated", __FUNCTION__,
              (uint32_t)mId);
        return Error::NotValidated;
    }
    mChanges.reset();

    DEFINE_AND_VALIDATE_HOST_CONNECTION
    hostCon->lock();
    bool hostCompositionV1 = rcEnc->hasHostCompositionV1();
    bool hostCompositionV2 = rcEnc->hasHostCompositionV2();
    hostCon->unlock();

    // if we supports v2, then discard v1
    if (hostCompositionV2) {
        hostCompositionV1 = false;
    }

    if (hostCompositionV2 || hostCompositionV1) {
        uint32_t numLayer = 0;
        for (auto layer: mLayers) {
            if (layer->getCompositionType() == Composition::Device ||
                layer->getCompositionType() == Composition::SolidColor) {
                numLayer++;
            }
        }

        ALOGVV("present %d layers total %u layers",
              numLayer, (uint32_t)mLayers.size());

        mReleaseLayerIds.clear();
        mReleaseFences.clear();

        if (numLayer == 0) {
            ALOGW("No layers, exit, buffer %p", mClientTarget.getBuffer());
            if (mClientTarget.getBuffer()) {
                post(hostCon, rcEnc, mClientTarget.getBuffer());
                *outRetireFence = mClientTarget.getFence();
            }
            return Error::None;
        }

        if (hostCompositionV1) {
            if (mComposeMsg == nullptr || mComposeMsg->getLayerCnt() < numLayer) {
                mComposeMsg.reset(new ComposeMsg(numLayer));
            }
        } else {
            if (mComposeMsg_v2 == nullptr || mComposeMsg_v2->getLayerCnt() < numLayer) {
                mComposeMsg_v2.reset(new ComposeMsg_v2(numLayer));
            }
        }

        // Handle the composition
        ComposeDevice* p;
        ComposeDevice_v2* p2;
        ComposeLayer* l;

        if (hostCompositionV1) {
            p = mComposeMsg->get();
            l = p->layer;
        } else {
            p2 = mComposeMsg_v2->get();
            l = p2->layer;
        }

        for (auto layer: mLayers) {
            if (layer->getCompositionType() != Composition::Device &&
                layer->getCompositionType() != Composition::SolidColor) {
                ALOGE("%s: Unsupported composition types %d layer %u",
                      __FUNCTION__, layer->getCompositionType(),
                      (uint32_t)layer->getId());
                continue;
            }
            // send layer composition command to host
            if (layer->getCompositionType() == Composition::Device) {
                int fence = layer->getLayerBuffer().getFence();
                mReleaseLayerIds.push_back(layer->getId());
                if (fence != -1) {
                    int err = sync_wait(fence, 3000);
                    if (err < 0 && errno == ETIME) {
                        ALOGE("%s waited on fence %d for 3000 ms",
                            __FUNCTION__, fence);
                    }
                    close(fence);
                }
                else {
                    ALOGV("%s: acquire fence not set for layer %u",
                          __FUNCTION__, (uint32_t)layer->getId());
                }
                const native_handle_t *cb =
                    layer->getLayerBuffer().getBuffer();
                if (cb != nullptr) {
                    l->cbHandle = hostCon->grallocHelper()->getHostHandle(cb);
                }
                else {
                    ALOGE("%s null buffer for layer %d", __FUNCTION__,
                          (uint32_t)layer->getId());
                }
            }
            else {
                // solidcolor has no buffer
                l->cbHandle = 0;
            }
            l->composeMode = (hwc2_composition_t)layer->getCompositionType();
            l->displayFrame = layer->getDisplayFrame();
            l->crop = layer->getSourceCrop();
            l->blendMode = layer->getBlendMode();
            l->alpha = layer->getPlaneAlpha();
            l->color = layer->getColor();
            l->transform = layer->getTransform();
            ALOGV("   cb %d blendmode %d alpha %f %d %d %d %d z %d"
                  " composeMode %d, transform %d",
                  l->cbHandle, l->blendMode, l->alpha,
                  l->displayFrame.left, l->displayFrame.top,
                  l->displayFrame.right, l->displayFrame.bottom,
                  layer->getZ(), l->composeMode, l->transform);
            l++;
        }
        if (hostCompositionV1) {
            p->version = 1;
            p->targetHandle = hostCon->grallocHelper()->getHostHandle(mTargetCb);
            p->numLayers = numLayer;
        } else {
            p2->version = 2;
            p2->displayId = mHostDisplayId;
            p2->targetHandle = hostCon->grallocHelper()->getHostHandle(mTargetCb);
            p2->numLayers = numLayer;
        }

        hostCon->lock();
        if (hostCompositionV1) {
            rcEnc->rcCompose(rcEnc,
                             sizeof(ComposeDevice) + numLayer * sizeof(ComposeLayer),
                             (void *)p);
        } else {
            rcEnc->rcCompose(rcEnc,
                             sizeof(ComposeDevice_v2) + numLayer * sizeof(ComposeLayer),
                             (void *)p2);
        }

        hostCon->unlock();

        // Send a retire fence and use it as the release fence for all layers,
        // since media expects it
        EGLint attribs[] = { EGL_SYNC_NATIVE_FENCE_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID };

        uint64_t sync_handle, thread_handle;
        int retire_fd;

        hostCon->lock();
        rcEnc->rcCreateSyncKHR(rcEnc, EGL_SYNC_NATIVE_FENCE_ANDROID,
                attribs, 2 * sizeof(EGLint), true /* destroy when signaled */,
                &sync_handle, &thread_handle);
        hostCon->unlock();

        goldfish_sync_queue_work(mSyncDeviceFd,
                sync_handle, thread_handle, &retire_fd);

        for (size_t i = 0; i < mReleaseLayerIds.size(); ++i) {
            mReleaseFences.push_back(dup(retire_fd));
        }

        *outRetireFence = dup(retire_fd);
        close(retire_fd);
        hostCon->lock();
        rcEnc->rcDestroySyncKHR(rcEnc, sync_handle);
        hostCon->unlock();
    } else {
        // we set all layers Composition::Client, so do nothing.
        post(hostCon, rcEnc, mClientTarget.getBuffer());
        *outRetireFence = mClientTarget.getFence();
        ALOGV("%s fallback to post, returns outRetireFence %d",
              __FUNCTION__, *outRetireFence);
    }

    return Error::None;
}

Error EmuHWC2::Display::setActiveConfig(hwc2_config_t configId) {
    ALOGVV("%s %u", __FUNCTION__, (uint32_t)configId);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (configId > mConfigs.size() || !mConfigs[configId]->isOnDisplay(*this)) {
        ALOGW("%s: bad config (%u %u)", __FUNCTION__, (uint32_t)mId,
              (uint32_t)configId);
        return Error::BadConfig;
    }
    auto config = mConfigs[configId];
    if (config == mActiveConfig) {
        return Error::None;
    }

    mActiveConfig = config;
    return Error::None;
}

Error EmuHWC2::Display::setClientTarget(buffer_handle_t target,
        int32_t acquireFence, int32_t /*dataspace*/, hwc_region_t /*damage*/) {
    ALOGVV("%s", __FUNCTION__);

    std::unique_lock<std::mutex> lock(mStateMutex);
    mClientTarget.setBuffer(target);
    mClientTarget.setFence(acquireFence);
    return Error::None;
}

Error EmuHWC2::Display::setColorMode(int32_t intMode) {
    ALOGVV("%s %d", __FUNCTION__, intMode);
    std::unique_lock<std::mutex> lock(mStateMutex);

    auto mode = static_cast<android_color_mode_t>(intMode);
    ALOGV("%s: (display %u mode %d)", __FUNCTION__, (uint32_t)mId, intMode);
    if (mode == mActiveColorMode) {
        return Error::None;
    }
    if (mColorModes.count(mode) == 0) {
        ALOGE("%s: display %d Mode %d not found in mColorModes",
             __FUNCTION__, (uint32_t)mId, intMode);
        return Error::Unsupported;
    }
    mActiveColorMode = mode;
    return Error::None;
}

Error EmuHWC2::Display::setColorTransform(const float* /*matrix*/,
                                          int32_t hint) {
    ALOGVV("%s hint %d", __FUNCTION__, hint);
    std::unique_lock<std::mutex> lock(mStateMutex);
    //we force client composition if this is set
    if (hint == 0 ) {
        mSetColorTransform = false;
    }
    else {
        mSetColorTransform = true;
    }
    return Error::None;
}

Error EmuHWC2::Display::setOutputBuffer(buffer_handle_t /*buffer*/,
        int32_t /*releaseFence*/) {
    ALOGVV("%s", __FUNCTION__);
    //TODO: for virtual display
    return Error::None;
}

static bool isValid(PowerMode mode) {
    switch (mode) {
        case PowerMode::Off: // Fall-through
        case PowerMode::DozeSuspend: // Fall-through
        case PowerMode::Doze: // Fall-through
        case PowerMode::On: return true;
        default: return false;
    }
}

Error EmuHWC2::Display::setPowerMode(int32_t intMode) {
    ALOGVV("%s", __FUNCTION__);
    // Emulator always set screen ON
    PowerMode mode = static_cast<PowerMode>(intMode);
    if (!isValid(mode)) {
        return Error::BadParameter;
    }
    if (mode == mPowerMode) {
        return Error::None;
    }
    std::unique_lock<std::mutex> lock(mStateMutex);

    ALOGV("%s: (display %u mode %s)", __FUNCTION__,
          (uint32_t)mId, to_string(mode).c_str());
    mPowerMode = mode;
    return Error::None;
}

static bool isValid(Vsync enable) {
    switch (enable) {
        case Vsync::Enable: // Fall-through
        case Vsync::Disable: return true;
        case Vsync::Invalid: return false;
    }
}

Error EmuHWC2::Display::setVsyncEnabled(int32_t intEnable) {
    ALOGVV("%s %d", __FUNCTION__, intEnable);
    Vsync enable = static_cast<Vsync>(intEnable);
    if (!isValid(enable)) {
        return Error::BadParameter;
    }
    if (enable == mVsyncEnabled) {
        return Error::None;
    }

    std::unique_lock<std::mutex> lock(mStateMutex);

    mVsyncEnabled = enable;
    return Error::None;
}

Error EmuHWC2::Display::validate(uint32_t* outNumTypes,
        uint32_t* outNumRequests) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (!mChanges) {
        mChanges.reset(new Changes);
        DEFINE_AND_VALIDATE_HOST_CONNECTION
        hostCon->lock();
        bool hostCompositionV1 = rcEnc->hasHostCompositionV1();
        bool hostCompositionV2 = rcEnc->hasHostCompositionV2();
        hostCon->unlock();

        if (hostCompositionV1 || hostCompositionV2) {
            // Support Device and SolidColor, otherwise, fallback all layers
            // to Client
            bool fallBack = false;
            for (auto& layer : mLayers) {
                if (layer->getCompositionType() == Composition::Invalid) {
                    // Log error for unused layers, layer leak?
                    ALOGE("%s layer %u CompositionType(%d) not set",
                          __FUNCTION__, (uint32_t)layer->getId(),
                          layer->getCompositionType());
                    continue;
                }
                if (layer->getCompositionType() == Composition::Client ||
                    layer->getCompositionType() == Composition::Cursor ||
                    layer->getCompositionType() == Composition::Sideband) {
                    ALOGW("%s: layer %u CompositionType %d, fallback", __FUNCTION__,
                         (uint32_t)layer->getId(), layer->getCompositionType());
                    fallBack = true;
                    break;
                }
            }
            if (mSetColorTransform) {
                fallBack = true;
            }
            if (fallBack) {
                for (auto& layer : mLayers) {
                    if (layer->getCompositionType() == Composition::Invalid) {
                        continue;
                    }
                    if (layer->getCompositionType() != Composition::Client) {
                        mChanges->addTypeChange(layer->getId(),
                                                Composition::Client);
                    }
                }
            }
       }
       else {
            for (auto& layer : mLayers) {
                if (layer->getCompositionType() != Composition::Client) {
                    mChanges->addTypeChange(layer->getId(),
                                            Composition::Client);
                }
            }
        }
    }
    else {
        ALOGE("Validate was called more than once!");
    }

    *outNumTypes = mChanges->getNumTypes();
    *outNumRequests = mChanges->getNumLayerRequests();
    ALOGV("%s: displayId %u types %u, requests %u", __FUNCTION__,
          (uint32_t)mId, *outNumTypes, *outNumRequests);
    return *outNumTypes > 0 ? Error::HasChanges : Error::None;
}

Error EmuHWC2::Display::updateLayerZ(hwc2_layer_t layerId, uint32_t z) {
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    const auto mapLayer = mDevice.mLayers.find(layerId);
    if (mapLayer == mDevice.mLayers.end()) {
        ALOGE("%s failed to find layer %u", __FUNCTION__, (uint32_t)mId);
        return Error::BadLayer;
    }

    const auto layer = mapLayer->second;
    const auto zRange = mLayers.equal_range(layer);
    bool layerOnDisplay = false;
    for (auto current = zRange.first; current != zRange.second; ++current) {
        if (**current == *layer) {
            if ((*current)->getZ() == z) {
                // Don't change anything if the Z hasn't changed
                return Error::None;
            }
            current = mLayers.erase(current);
            layerOnDisplay = true;
            break;
        }
    }

    if (!layerOnDisplay) {
        ALOGE("%s failed to find layer %u on display", __FUNCTION__,
              (uint32_t)mId);
        return Error::BadLayer;
    }

    layer->setZ(z);
    mLayers.emplace(std::move(layer));
    return Error::None;
}

Error EmuHWC2::Display::getClientTargetSupport(uint32_t width, uint32_t height,
                                      int32_t format, int32_t dataspace){
    ALOGVV("%s", __FUNCTION__);
    std::unique_lock<std::mutex> lock(mStateMutex);

    if (mActiveConfig == nullptr) {
        return Error::Unsupported;
    }

    if (width == (uint32_t)mActiveConfig->getAttribute(Attribute::Width) &&
        height == (uint32_t)mActiveConfig->getAttribute(Attribute::Height) &&
        format == HAL_PIXEL_FORMAT_RGBA_8888 &&
        dataspace == HAL_DATASPACE_UNKNOWN) {
        return Error::None;
    }

    return Error::None;
}

// thess EDIDs are carefully generated according to the EDID spec version 1.3, more info
// can be found from the following file:
//   frameworks/native/services/surfaceflinger/DisplayHardware/DisplayIdentification.cpp
// approved pnp ids can be found here: https://uefi.org/pnp_id_list
// pnp id: GGL, name: EMU_display_0, last byte is checksum
// display id is local:8141603649153536
static const uint8_t sEDID0[] = {
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1c, 0xec, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x1b, 0x10, 0x01, 0x03, 0x80, 0x50, 0x2d, 0x78, 0x0a, 0x0d, 0xc9, 0xa0, 0x57, 0x47, 0x98, 0x27,
    0x12, 0x48, 0x4c, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
    0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc,
    0x00, 0x45, 0x4d, 0x55, 0x5f, 0x64, 0x69, 0x73, 0x70, 0x6c, 0x61, 0x79, 0x5f, 0x30, 0x00, 0x4b
};

// pnp id: GGL, name: EMU_display_1
// display id is local:8140900251843329
static const uint8_t sEDID1[] = {
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1c, 0xec, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x1b, 0x10, 0x01, 0x03, 0x80, 0x50, 0x2d, 0x78, 0x0a, 0x0d, 0xc9, 0xa0, 0x57, 0x47, 0x98, 0x27,
    0x12, 0x48, 0x4c, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
    0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc,
    0x00, 0x45, 0x4d, 0x55, 0x5f, 0x64, 0x69, 0x73, 0x70, 0x6c, 0x61, 0x79, 0x5f, 0x31, 0x00, 0x3b
};

// pnp id: GGL, name: EMU_display_2
// display id is local:8140940453066754
static const uint8_t sEDID2[] = {
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1c, 0xec, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x1b, 0x10, 0x01, 0x03, 0x80, 0x50, 0x2d, 0x78, 0x0a, 0x0d, 0xc9, 0xa0, 0x57, 0x47, 0x98, 0x27,
    0x12, 0x48, 0x4c, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
    0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc,
    0x00, 0x45, 0x4d, 0x55, 0x5f, 0x64, 0x69, 0x73, 0x70, 0x6c, 0x61, 0x79, 0x5f, 0x32, 0x00, 0x49
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

Error EmuHWC2::Display::getDisplayIdentificationData(uint8_t* outPort,
        uint32_t* outDataSize, uint8_t* outData) {
    ALOGVV("%s DisplayId %u", __FUNCTION__, (uint32_t)mId);
    if (outPort == nullptr || outDataSize == nullptr)
        return Error::BadParameter;

    uint32_t len = std::min(*outDataSize, (uint32_t)ARRAY_SIZE(sEDID0));
    if (outData != nullptr && len < (uint32_t)ARRAY_SIZE(sEDID0)) {
        ALOGW("%s DisplayId %u, small buffer size: %u is specified",
                __FUNCTION__, (uint32_t)mId, len);
    }
    *outDataSize = ARRAY_SIZE(sEDID0);
    switch (mId) {
        case 0:
            *outPort = 0;
            if (outData)
                memcpy(outData, sEDID0, len);
            break;

        case 1:
            *outPort = 1;
            if (outData)
                memcpy(outData, sEDID1, len);
            break;

        case 2:
            *outPort = 2;
            if (outData)
                memcpy(outData, sEDID2, len);
            break;

        default:
            *outPort = (uint8_t)mId;
            if (outData) {
                memcpy(outData, sEDID2, len);
                uint32_t size = ARRAY_SIZE(sEDID0);
                // change the name to EMU_display_<mID>
                // note the 3rd char from back is the number, _0, _1, _2, etc.
                if (len >= size - 2)
                    outData[size-3] = '0' + (uint8_t)mId;
                if (len >= size) {
                    // update the last byte, which is checksum byte
                    uint8_t checksum = -(uint8_t)std::accumulate(
                            outData, outData + size - 1, static_cast<uint8_t>(0));
                    outData[size - 1] = checksum;
                }
            }
            break;
    }

    return Error::None;
}

Error EmuHWC2::Display::getDisplayCapabilities(uint32_t* outNumCapabilities,
        uint32_t* outCapabilities) {
    if (outNumCapabilities == nullptr) {
        return Error::None;
    }

    bool brightness_support = true;
    bool doze_support = true;

    uint32_t count = 1  + static_cast<uint32_t>(doze_support) + (brightness_support ? 1 : 0);
    int index = 0;
    if (outCapabilities != nullptr && (*outNumCapabilities >= count)) {
        outCapabilities[index++] = HWC2_DISPLAY_CAPABILITY_SKIP_CLIENT_COLOR_TRANSFORM;
        if (doze_support) {
            outCapabilities[index++] = HWC2_DISPLAY_CAPABILITY_DOZE;
        }
        if (brightness_support) {
            outCapabilities[index++] = HWC2_DISPLAY_CAPABILITY_BRIGHTNESS;
       }
    }

    *outNumCapabilities = count;
    return Error::None;
}

Error EmuHWC2::Display::getDisplayBrightnessSupport(bool *out_support) {
    *out_support = false;
    return Error::None;
}

Error EmuHWC2::Display::setDisplayBrightness(float brightness) {
    ALOGW("TODO: setDisplayBrightness() is not implemented yet: brightness=%f", brightness);
    return Error::None;
}

int EmuHWC2::Display::populatePrimaryConfigs(int width, int height, int dpiX, int dpiY) {
    ALOGVV("%s DisplayId %u", __FUNCTION__, (uint32_t)mId);
    std::unique_lock<std::mutex> lock(mStateMutex);

    auto newConfig = std::make_shared<Config>(*this);
    // vsync is 60 hz;
    newConfig->setAttribute(Attribute::VsyncPeriod, mVsyncPeriod);
    newConfig->setAttribute(Attribute::Width, width);
    newConfig->setAttribute(Attribute::Height, height);
    newConfig->setAttribute(Attribute::DpiX, dpiX * 1000);
    newConfig->setAttribute(Attribute::DpiY, dpiY * 1000);

    newConfig->setId(static_cast<hwc2_config_t>(mConfigs.size()));
    ALOGV("Found new config %d: %s", (uint32_t)newConfig->getId(),
            newConfig->toString().c_str());
    mConfigs.emplace_back(std::move(newConfig));

    // Only have single config so far, it is activeConfig
    mActiveConfig = mConfigs[0];
    mActiveColorMode = HAL_COLOR_MODE_NATIVE;
    mColorModes.emplace((android_color_mode_t)HAL_COLOR_MODE_NATIVE);

    mSyncDeviceFd = goldfish_sync_open();

    return 0;
}

void EmuHWC2::Display::post(HostConnection *hostCon,
                            ExtendedRCEncoderContext *rcEnc,
                            buffer_handle_t h) {
    assert(cb && "native_handle_t::from(h) failed");

    hostCon->lock();
    rcEnc->rcFBPost(rcEnc, hostCon->grallocHelper()->getHostHandle(h));
    hostCon->flush();
    hostCon->unlock();
}

HWC2::Error EmuHWC2::Display::populateSecondaryConfigs(uint32_t width, uint32_t height,
        uint32_t dpi, uint32_t idx) {
    ALOGVV("%s DisplayId %u, width %u, height %u, dpi %u",
            __FUNCTION__, (uint32_t)mId, width, height, dpi);
    std::unique_lock<std::mutex> lock(mStateMutex);

    auto newConfig = std::make_shared<Config>(*this);
    // vsync is 60 hz;
    newConfig->setAttribute(Attribute::VsyncPeriod, mVsyncPeriod);
    newConfig->setAttribute(Attribute::Width, width);
    newConfig->setAttribute(Attribute::Height, height);
    newConfig->setAttribute(Attribute::DpiX, dpi*1000);
    newConfig->setAttribute(Attribute::DpiY, dpi*1000);

    newConfig->setId(static_cast<hwc2_config_t>(mConfigs.size()));
    ALOGV("Found new secondary config %d: %s", (uint32_t)newConfig->getId(),
            newConfig->toString().c_str());
    mConfigs.emplace_back(std::move(newConfig));

    // we need to reset these values after populatePrimaryConfigs()
    mActiveConfig = mConfigs[0];
    mActiveColorMode = HAL_COLOR_MODE_NATIVE;
    mColorModes.emplace((android_color_mode_t)HAL_COLOR_MODE_NATIVE);

    uint32_t displayId = hostDisplayIdStart + idx;
    DEFINE_AND_VALIDATE_HOST_CONNECTION

    hostCon->lock();
    rcEnc->rcDestroyDisplay(rcEnc, displayId);
    rcEnc->rcCreateDisplay(rcEnc, &displayId);
    rcEnc->rcSetDisplayPose(rcEnc, displayId, -1, -1, width, height);
    hostCon->unlock();

    if (displayId != hostDisplayIdStart + idx) {
        ALOGE("Something wrong with host displayId allocation, want %d "
              "allocated %d", hostDisplayIdStart + idx, displayId);
    }
    mHostDisplayId = displayId;
    ALOGVV("%s: mHostDisplayId=%d", __FUNCTION__, mHostDisplayId);

    return HWC2::Error::None;
}


// Config functions

void EmuHWC2::Display::Config::setAttribute(Attribute attribute,
       int32_t value) {
    mAttributes[attribute] = value;
}

int32_t EmuHWC2::Display::Config::getAttribute(Attribute attribute) const {
    if (mAttributes.count(attribute) == 0) {
        return -1;
    }
    return mAttributes.at(attribute);
}

std::string EmuHWC2::Display::Config::toString() const {
    std::string output;

    const size_t BUFFER_SIZE = 100;
    char buffer[BUFFER_SIZE] = {};
    auto writtenBytes = snprintf(buffer, BUFFER_SIZE,
            "%u x %u", mAttributes.at(HWC2::Attribute::Width),
            mAttributes.at(HWC2::Attribute::Height));
    output.append(buffer, writtenBytes);

    if (mAttributes.count(HWC2::Attribute::VsyncPeriod) != 0) {
        std::memset(buffer, 0, BUFFER_SIZE);
        writtenBytes = snprintf(buffer, BUFFER_SIZE, " @ %.1f Hz",
                1e9 / mAttributes.at(HWC2::Attribute::VsyncPeriod));
        output.append(buffer, writtenBytes);
    }

    if (mAttributes.count(HWC2::Attribute::DpiX) != 0 &&
            mAttributes.at(HWC2::Attribute::DpiX) != -1) {
        std::memset(buffer, 0, BUFFER_SIZE);
        writtenBytes = snprintf(buffer, BUFFER_SIZE,
                ", DPI: %.1f x %.1f",
                mAttributes.at(HWC2::Attribute::DpiX) / 1000.0f,
                mAttributes.at(HWC2::Attribute::DpiY) / 1000.0f);
        output.append(buffer, writtenBytes);
    }

    return output;
}

// VsyncThread function
bool EmuHWC2::Display::VsyncThread::threadLoop() {
    struct timespec rt;
    if (clock_gettime(CLOCK_MONOTONIC, &rt) == -1) {
        ALOGE("%s: error in vsync thread clock_gettime: %s",
              __FUNCTION__, strerror(errno));
        return true;
    }
    const int logInterval = 60;
    int64_t lastLogged = rt.tv_sec;
    int sent = 0;
    int lastSent = 0;
    bool vsyncEnabled = false;

    struct timespec wait_time;
    wait_time.tv_sec = 0;
    wait_time.tv_nsec = mDisplay.mVsyncPeriod;
    const int64_t kOneRefreshNs = mDisplay.mVsyncPeriod;
    const int64_t kOneSecondNs = 1000ULL * 1000ULL * 1000ULL;
    int64_t lastTimeNs = -1;
    int64_t phasedWaitNs = 0;
    int64_t currentNs = 0;

    while (true) {
        clock_gettime(CLOCK_MONOTONIC, &rt);
        currentNs = rt.tv_nsec + rt.tv_sec * kOneSecondNs;

        if (lastTimeNs < 0) {
            phasedWaitNs = currentNs + kOneRefreshNs;
        } else {
            phasedWaitNs = kOneRefreshNs *
                (( currentNs - lastTimeNs) / kOneRefreshNs + 1) +
                lastTimeNs;
        }

        wait_time.tv_sec = phasedWaitNs / kOneSecondNs;
        wait_time.tv_nsec = phasedWaitNs - wait_time.tv_sec * kOneSecondNs;

        int ret;
        do {
            ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wait_time, NULL);
        } while (ret == -1 && errno == EINTR);

        lastTimeNs = phasedWaitNs;

        std::unique_lock<std::mutex> lock(mDisplay.mStateMutex);
        vsyncEnabled = (mDisplay.mVsyncEnabled == Vsync::Enable);
        lock.unlock();

        if (!vsyncEnabled) {
            continue;
        }

        lock.lock();
        const auto& callbackInfo = mDisplay.mDevice.mCallbacks[Callback::Vsync];
        auto vsync = reinterpret_cast<HWC2_PFN_VSYNC>(callbackInfo.pointer);
        lock.unlock();

        if (vsync) {
            vsync(callbackInfo.data, mDisplay.mId, lastTimeNs);
        }

        if (rt.tv_sec - lastLogged >= logInterval) {
            ALOGVV("sent %d syncs in %ds", sent - lastSent, rt.tv_sec - lastLogged);
            lastLogged = rt.tv_sec;
            lastSent = sent;
        }
        ++sent;
    }
    return false;
}


// Layer functions
bool EmuHWC2::SortLayersByZ::operator()(const std::shared_ptr<Layer>& lhs,
        const std::shared_ptr<Layer>& rhs) const {
    return lhs->getZ() < rhs->getZ();
}

std::atomic<hwc2_layer_t> EmuHWC2::Layer::sNextId(1);

EmuHWC2::Layer::Layer(Display& display)
  : mId(sNextId++),
    mDisplay(display),
    mBuffer(),
    mSurfaceDamage(),
    mBlendMode(BlendMode::None),
    mColor({0, 0, 0, 0}),
    mCompositionType(Composition::Invalid),
    mDisplayFrame({0, 0, -1, -1}),
    mPlaneAlpha(0.0f),
    mSidebandStream(nullptr),
    mSourceCrop({0.0f, 0.0f, -1.0f, -1.0f}),
    mTransform(Transform::None),
    mVisibleRegion(),
    mZ(0)
    {}

Error EmuHWC2::Layer::setBuffer(buffer_handle_t buffer,
        int32_t acquireFence) {
    ALOGVV("%s: Setting acquireFence %d for layer %u", __FUNCTION__,
          acquireFence, (uint32_t)mId);
    mBuffer.setBuffer(buffer);
    mBuffer.setFence(acquireFence);
    return Error::None;
}

Error EmuHWC2::Layer::setCursorPosition(int32_t /*x*/,
                                        int32_t /*y*/) {
    ALOGVV("%s layer %u", __FUNCTION__, (uint32_t)mId);
    if (mCompositionType != Composition::Cursor) {
        ALOGE("%s: CompositionType not Cursor type", __FUNCTION__);
        return Error::BadLayer;
    }
   //TODO
    return Error::None;
}

Error EmuHWC2::Layer::setSurfaceDamage(hwc_region_t /*damage*/) {
    // Emulator redraw whole layer per frame, so ignore this.
    ALOGVV("%s", __FUNCTION__);
    return Error::None;
}

// Layer state functions

Error EmuHWC2::Layer::setBlendMode(int32_t mode) {
    ALOGVV("%s %d for layer %u", __FUNCTION__, mode, (uint32_t)mId);
    mBlendMode = static_cast<BlendMode>(mode);
    return Error::None;
}

Error EmuHWC2::Layer::setColor(hwc_color_t color) {
    ALOGVV("%s layer %u %d", __FUNCTION__, (uint32_t)mId, color);
    mColor = color;
    return Error::None;
}

Error EmuHWC2::Layer::setCompositionType(int32_t type) {
    ALOGVV("%s layer %u %u", __FUNCTION__, (uint32_t)mId, type);
    mCompositionType = static_cast<Composition>(type);
    return Error::None;
}

Error EmuHWC2::Layer::setDataspace(int32_t) {
    ALOGVV("%s", __FUNCTION__);
    return Error::None;
}

Error EmuHWC2::Layer::setDisplayFrame(hwc_rect_t frame) {
    ALOGVV("%s layer %u", __FUNCTION__, (uint32_t)mId);
    mDisplayFrame = frame;
    return Error::None;
}

Error EmuHWC2::Layer::setPlaneAlpha(float alpha) {
    ALOGVV("%s layer %u %f", __FUNCTION__, (uint32_t)mId, alpha);
    mPlaneAlpha = alpha;
    return Error::None;
}

Error EmuHWC2::Layer::setSidebandStream(const native_handle_t* stream) {
    ALOGVV("%s layer %u", __FUNCTION__, (uint32_t)mId);
    mSidebandStream = stream;
    return Error::None;
}

Error EmuHWC2::Layer::setSourceCrop(hwc_frect_t crop) {
    ALOGVV("%s layer %u", __FUNCTION__, (uint32_t)mId);
    mSourceCrop = crop;
    return Error::None;
}

Error EmuHWC2::Layer::setTransform(int32_t transform) {
    ALOGVV("%s layer %u", __FUNCTION__, (uint32_t)mId);
    mTransform = static_cast<Transform>(transform);
    return Error::None;
}

static bool compareRects(const hwc_rect_t& rect1, const hwc_rect_t& rect2) {
    return rect1.left == rect2.left &&
            rect1.right == rect2.right &&
            rect1.top == rect2.top &&
            rect1.bottom == rect2.bottom;
}

Error EmuHWC2::Layer::setVisibleRegion(hwc_region_t visible) {
    ALOGVV("%s", __FUNCTION__);
    if ((getNumVisibleRegions() != visible.numRects) ||
        !std::equal(mVisibleRegion.begin(), mVisibleRegion.end(), visible.rects,
                    compareRects)) {
        mVisibleRegion.resize(visible.numRects);
        std::copy_n(visible.rects, visible.numRects, mVisibleRegion.begin());
    }
    return Error::None;
}

Error EmuHWC2::Layer::setZ(uint32_t z) {
    ALOGVV("%s layer %u %d", __FUNCTION__, (uint32_t)mId, z);
    mZ = z;
    return Error::None;
}

// Adaptor Helpers

void EmuHWC2::populateCapabilities() {
    //TODO: add Capabilities
    // support virtualDisplay
    // support sideBandStream
    // support backGroundColor
    // we should not set this for HWC2, TODO: remove
    // mCapabilities.insert(Capability::PresentFenceIsNotReliable);
}

int EmuHWC2::populatePrimary() {
    int ret = 0;
    auto display = std::make_shared<Display>(*this, HWC2::DisplayType::Physical);
    ret = display->populatePrimaryConfigs(mDisplayWidth, mDisplayHeight,
                                          mDisplayDpiX, mDisplayDpiY);
    if (ret != 0) {
        return ret;
    }
    mDisplays.emplace(display->getId(), std::move(display));
    return ret;
}

// Note "hwservicemanager." is used to avoid selinux issue
#define EXTERANL_DISPLAY_PROP "hwservicemanager.external.displays"

// return 0 for successful, 1 if no external displays are specified
// return < 0 if failed
int EmuHWC2::populateSecondaryDisplays() {
    // this guest property, hwservicemanager.external.displays,
    // specifies multi-display info, with comma (,) as separator
    // each display has the following info:
    //   physicalId,width,height,dpi,flags
    // serveral displays can be provided, e.g., following has 2 displays:
    // setprop hwservicemanager.external.displays 1,1200,800,120,0,2,1200,800,120,0
    std::vector<uint64_t> values;
    char displaysValue[PROPERTY_VALUE_MAX] = "";
    property_get(EXTERANL_DISPLAY_PROP, displaysValue, "");
    bool isValid = displaysValue[0] != '\0';
    if (isValid) {
        char *p = displaysValue;
        while (*p) {
            if (!isdigit(*p) && *p != ',' && *p != ' ') {
                isValid = false;
                break;
            }
            p ++;
        }
        if (!isValid) {
            ALOGE("Invalid syntax for the value of system prop: %s", EXTERANL_DISPLAY_PROP);
        }
    }
    if (!isValid) {
        // no external displays are specified
        return 1;
    }
    // parse all int values to a vector
    std::istringstream stream(displaysValue);
    for (uint64_t id; stream >> id;) {
        values.push_back(id);
        if (stream.peek() == ',')
            stream.ignore();
    }
    // each display has 5 values
    if ((values.size() % 5) != 0) {
        ALOGE("%s: invalid value for system property: %s", __FUNCTION__, EXTERANL_DISPLAY_PROP);
        return -1;
    }
    uint32_t idx = 0;
    while (!values.empty()) {
        // uint64_t physicalId = values[0];
        uint32_t width = values[1];
        uint32_t height = values[2];
        uint32_t dpi = values[3];
        // uint32_t flags = values[4];
        values.erase(values.begin(), values.begin() + 5);

        Error ret = Error::None;
        auto display = std::make_shared<Display>(*this, HWC2::DisplayType::Physical);
        ret = display->populateSecondaryConfigs(width, height, dpi, idx++);
        if (ret != Error::None) {
            return -2;
        }
        mDisplays.emplace(display->getId(), std::move(display));
    }
    return 0;
}

EmuHWC2::Display* EmuHWC2::getDisplay(hwc2_display_t id) {
    auto display = mDisplays.find(id);
    if (display == mDisplays.end()) {
        ALOGE("Failed to get display for id=%d", (uint32_t)id);
        return nullptr;
    }
    return display->second.get();
}

std::tuple<EmuHWC2::Layer*, Error> EmuHWC2::getLayer(
        hwc2_display_t displayId, hwc2_layer_t layerId) {
    auto display = getDisplay(displayId);
    if (!display) {
        ALOGE("%s: Fail to find display %d", __FUNCTION__, (uint32_t)displayId);
        return std::make_tuple(static_cast<Layer*>(nullptr), Error::BadDisplay);
    }

    auto layerEntry = mLayers.find(layerId);
    if (layerEntry == mLayers.end()) {
        ALOGE("%s: Fail to find layer %d", __FUNCTION__, (uint32_t)layerId);
        return std::make_tuple(static_cast<Layer*>(nullptr), Error::BadLayer);
    }

    auto layer = layerEntry->second;
    if (layer->getDisplay().getId() != displayId) {
        ALOGE("%s: layer %d not belongs to display %d", __FUNCTION__,
              (uint32_t)layerId, (uint32_t)displayId);
        return std::make_tuple(static_cast<Layer*>(nullptr), Error::BadLayer);
    }
    return std::make_tuple(layer.get(), Error::None);
}

static int hwc2DevOpen(const struct hw_module_t *module, const char *name,
        struct hw_device_t **dev) {
    ALOGVV("%s ", __FUNCTION__);
    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        ALOGE("Invalid module name- %s", name);
        return -EINVAL;
    }

    EmuHWC2* ctx = new EmuHWC2();
    if (!ctx) {
        ALOGE("Failed to allocate EmuHWC2");
        return -ENOMEM;
    }
    int ret = ctx->populatePrimary();
    if (ret != 0) {
        ALOGE("Failed to populate primary display");
        return ret;
    }

    ret = ctx->populateSecondaryDisplays();
    if (ret < 0) {
        ALOGE("Failed to populate secondary displays");
        return ret;
    }

    ctx->common.module = const_cast<hw_module_t *>(module);
    *dev = &ctx->common;
    return 0;
}
}

static struct hw_module_methods_t hwc2_module_methods = {
    .open = android::hwc2DevOpen
};

hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 2,
    .version_minor = 0,
    .id = HWC_HARDWARE_MODULE_ID,
    .name = "goldfish HWC2 module",
    .author = "The Android Open Source Project",
    .methods = &hwc2_module_methods,
    .dso = NULL,
    .reserved = {0},
};
