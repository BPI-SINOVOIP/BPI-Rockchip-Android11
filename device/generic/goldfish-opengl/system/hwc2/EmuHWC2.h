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

#ifndef ANDROID_HW_EMU_HWC2_H
#define ANDROID_HW_EMU_HWC2_H

#define HWC2_INCLUDE_STRINGIFICATION
#define HWC2_USE_CPP11
#include <hardware/hwcomposer2.h>
#undef HWC2_INCLUDE_STRINGIFICATION
#undef HWC2_USE_CPP11
#include <utils/Thread.h>

#include <android-base/unique_fd.h>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <set>

#include <cutils/native_handle.h>

#include "HostConnection.h"

namespace android {

class EmuHWC2 : public hwc2_device_t {
public:
    EmuHWC2();
    int populatePrimary();
    int populateSecondaryDisplays();

private:
    static inline EmuHWC2* getHWC2(hwc2_device_t* device) {
        return static_cast<EmuHWC2*>(device);
    }

    static int closeHook(hw_device_t* device) {
        EmuHWC2 *ctx = reinterpret_cast<EmuHWC2*>(device);
        delete ctx;
        return 0;
    }

    // getCapabilities
    void doGetCapabilities(uint32_t* outCount, int32_t* outCapabilities);
    static void getCapabilitiesHook(hwc2_device_t* device, uint32_t* outCount,
                                int32_t* outCapabilities) {
        getHWC2(device)->doGetCapabilities(outCount, outCapabilities);
    }

    // getFunction
    hwc2_function_pointer_t doGetFunction(HWC2::FunctionDescriptor descriptor);
    static hwc2_function_pointer_t getFunctionHook(hwc2_device_t* device,
            int32_t desc) {
        auto descriptor = static_cast<HWC2::FunctionDescriptor>(desc);
        return getHWC2(device)->doGetFunction(descriptor);
    }

    // Device functions
    HWC2::Error createVirtualDisplay(uint32_t width, uint32_t height,
            int32_t* format, hwc2_display_t* outDisplay);
    static int32_t createVirtualDisplayHook(hwc2_device_t* device,
            uint32_t width, uint32_t height, int32_t* format,
            hwc2_display_t* outDisplay) {
        auto error = getHWC2(device)->createVirtualDisplay(width, height,
                format, outDisplay);
        return static_cast<int32_t>(error);
    }

    HWC2::Error destroyVirtualDisplay(hwc2_display_t display);
    static int32_t destroyVirtualDisplayHook(hwc2_device_t* device,
            hwc2_display_t display) {
        auto error = getHWC2(device)->destroyVirtualDisplay(display);
        return static_cast<int32_t>(error);
    }

    std::string mDumpString;
    void dump(uint32_t* outSize, char* outBuffer);
    static void dumpHook(hwc2_device_t* device, uint32_t* outSize,
            char* outBuffer) {
        getHWC2(device)->dump(outSize, outBuffer);
    }

    uint32_t getMaxVirtualDisplayCount();
    static uint32_t getMaxVirtualDisplayCountHook(hwc2_device_t* device) {
        return getHWC2(device)->getMaxVirtualDisplayCount();
    }

    HWC2::Error registerCallback(HWC2::Callback descriptor,
            hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer);
    static int32_t registerCallbackHook(hwc2_device_t* device,
            int32_t intDesc, hwc2_callback_data_t callbackData,
            hwc2_function_pointer_t pointer) {
        auto descriptor = static_cast<HWC2::Callback>(intDesc);
        auto error = getHWC2(device)->registerCallback(descriptor,
                callbackData, pointer);
        return static_cast<int32_t>(error);
    }

    class Layer;
    class SortLayersByZ {
    public:
        bool operator()(const std::shared_ptr<Layer>& lhs,
                    const std::shared_ptr<Layer>& rhs) const;
    };

    // SurfaceFlinger sets the ColorBuffer and its Fence handler for each
    // layer. This class is a container for these two.
    class FencedBuffer {
        public:
            FencedBuffer() : mBuffer(nullptr) {}

            void setBuffer(buffer_handle_t buffer) { mBuffer = buffer; }
            void setFence(int fenceFd) {
                mFence = std::make_shared<base::unique_fd>(fenceFd);
            }

            buffer_handle_t getBuffer() const { return mBuffer; }
            int getFence() const { return mFence ? dup(mFence->get()) : -1; }

        private:
            buffer_handle_t mBuffer;
            std::shared_ptr<base::unique_fd> mFence;
    };

    typedef struct compose_layer {
        uint32_t cbHandle;
        hwc2_composition_t composeMode;
        hwc_rect_t displayFrame;
        hwc_frect_t crop;
        int32_t blendMode;
        float alpha;
        hwc_color_t color;
        hwc_transform_t transform;
    } ComposeLayer;
    typedef struct compose_device {
        uint32_t version;
        uint32_t targetHandle;
        uint32_t numLayers;
        struct compose_layer layer[0];
    } ComposeDevice;
    typedef struct compose_device_v2 {
        uint32_t version;
        uint32_t displayId;
        uint32_t targetHandle;
        uint32_t numLayers;
        struct compose_layer layer[0];
    } ComposeDevice_v2;

    class ComposeMsg {
    public:
        ComposeMsg(uint32_t layerCnt = 0) :
          mData(sizeof(ComposeDevice) + layerCnt * sizeof(ComposeLayer))
        {
            mComposeDevice = reinterpret_cast<ComposeDevice*>(mData.data());
            mLayerCnt = layerCnt;
        }

        ComposeDevice* get() { return mComposeDevice; }

        uint32_t getLayerCnt() { return mLayerCnt; }

    private:
        std::vector<uint8_t> mData;
        uint32_t mLayerCnt;
        ComposeDevice* mComposeDevice;
    };

    class ComposeMsg_v2 {
    public:
        ComposeMsg_v2(uint32_t layerCnt = 0) :
          mData(sizeof(ComposeDevice_v2) + layerCnt * sizeof(ComposeLayer))
        {
            mComposeDevice = reinterpret_cast<ComposeDevice_v2*>(mData.data());
            mLayerCnt = layerCnt;
        }

        ComposeDevice_v2* get() { return mComposeDevice; }

        uint32_t getLayerCnt() { return mLayerCnt; }

    private:
        std::vector<uint8_t> mData;
        uint32_t mLayerCnt;
        ComposeDevice_v2* mComposeDevice;
    };

    class Display {
    public:
        Display(EmuHWC2& device, HWC2::DisplayType type);
        ~Display();
        hwc2_display_t getId() const {return mId;}

        // HWC2 Display functions
        HWC2::Error acceptChanges();
        HWC2::Error createLayer(hwc2_layer_t* outLayerId);
        HWC2::Error destroyLayer(hwc2_layer_t layerId);
        HWC2::Error getActiveConfig(hwc2_config_t* outConfigId);
        HWC2::Error getDisplayAttribute(hwc2_config_t configId,
                int32_t attribute, int32_t* outValue);
        HWC2::Error getChangedCompositionTypes(uint32_t* outNumElements,
                hwc2_layer_t* outLayers, int32_t* outTypes);
        HWC2::Error getColorModes(uint32_t* outNumModes, int32_t* outModes);
        HWC2::Error getConfigs(uint32_t* outNumConfigs,
                hwc2_config_t* outConfigIds);
        HWC2::Error getDozeSupport(int32_t* outSupport);
        HWC2::Error getHdrCapabilities(uint32_t* outNumTypes,
                int32_t* outTypes, float* outMaxLuminance,
                float* outMaxAverageLuminance, float* outMinLuminance);
        HWC2::Error getName(uint32_t* outSize, char* outName);
        HWC2::Error getReleaseFences(uint32_t* outNumElements,
                hwc2_layer_t* outLayers, int32_t* outFences);
        HWC2::Error getRequests(int32_t* outDisplayRequests,
                uint32_t* outNumElements, hwc2_layer_t* outLayers,
                int32_t* outLayerRequests);
        HWC2::Error getType(int32_t* outType);
        HWC2::Error present(int32_t* outRetireFence);
        HWC2::Error setActiveConfig(hwc2_config_t configId);
        HWC2::Error setClientTarget(buffer_handle_t target,
                int32_t acquireFence, int32_t dataspace,
                hwc_region_t damage);
        HWC2::Error setColorMode(int32_t mode);
        HWC2::Error setColorTransform(const float* matrix,
                int32_t hint);
        HWC2::Error setOutputBuffer(buffer_handle_t buffer,
                int32_t releaseFence);
        HWC2::Error setPowerMode(int32_t mode);
        HWC2::Error setVsyncEnabled(int32_t enabled);
        HWC2::Error validate(uint32_t* outNumTypes,
                uint32_t* outNumRequests);
        HWC2::Error updateLayerZ(hwc2_layer_t layerId, uint32_t z);
        HWC2::Error getClientTargetSupport(uint32_t width, uint32_t height,
                 int32_t format, int32_t dataspace);
        // 2.3 required functions
        HWC2::Error getDisplayIdentificationData(uint8_t* outPort,
                 uint32_t* outDataSize, uint8_t* outData);
        HWC2::Error getDisplayCapabilities(uint32_t* outNumCapabilities,
                 uint32_t* outCapabilities);
        HWC2::Error getDisplayBrightnessSupport(bool *out_support);
        HWC2::Error setDisplayBrightness(float brightness);

        // Read configs from PRIMARY Display
        int populatePrimaryConfigs(int width, int height, int dpiX, int dpiY);
        HWC2::Error populateSecondaryConfigs(uint32_t width, uint32_t height,
                 uint32_t dpi, uint32_t idx);

    private:
        void post(HostConnection *hostCon, ExtendedRCEncoderContext *rcEnc,
                  buffer_handle_t h);

        class Config {
        public:
            Config(Display& display)
              : mDisplay(display),
                mId(0),
                mAttributes() {}

            bool isOnDisplay(const Display& display) const {
                return display.getId() == mDisplay.getId();
            }
            void setAttribute(HWC2::Attribute attribute, int32_t value);
            int32_t getAttribute(HWC2::Attribute attribute) const;
            void setId(hwc2_config_t id) {mId = id; }
            hwc2_config_t getId() const {return mId; }
            std::string toString() const;

        private:
            Display& mDisplay;
            hwc2_config_t mId;
            std::unordered_map<HWC2::Attribute, int32_t> mAttributes;
        };

        // Stores changes requested from the device upon calling prepare().
        // Handles change request to:
        //   - Layer composition type.
        //   - Layer hints.
        class Changes {
            public:
                uint32_t getNumTypes() const {
                    return static_cast<uint32_t>(mTypeChanges.size());
                }

                uint32_t getNumLayerRequests() const {
                    return static_cast<uint32_t>(mLayerRequests.size());
                }

                const std::unordered_map<hwc2_layer_t, HWC2::Composition>&
                        getTypeChanges() const {
                    return mTypeChanges;
                }

                const std::unordered_map<hwc2_layer_t, HWC2::LayerRequest>&
                        getLayerRequests() const {
                    return mLayerRequests;
                }

                void addTypeChange(hwc2_layer_t layerId,
                        HWC2::Composition type) {
                    mTypeChanges.insert({layerId, type});
                }

                void clearTypeChanges() { mTypeChanges.clear(); }

                void addLayerRequest(hwc2_layer_t layerId,
                        HWC2::LayerRequest request) {
                    mLayerRequests.insert({layerId, request});
                }

            private:
                std::unordered_map<hwc2_layer_t, HWC2::Composition>
                        mTypeChanges;
                std::unordered_map<hwc2_layer_t, HWC2::LayerRequest>
                        mLayerRequests;
        };

        // Generate sw vsync signal
        class VsyncThread : public Thread {
        public:
            VsyncThread(Display& display)
              : mDisplay(display) {}
            virtual ~VsyncThread() {}
        private:
            Display& mDisplay;
            bool threadLoop() final;
        };

    private:
        EmuHWC2& mDevice;
        // Display ID generator.
        static std::atomic<hwc2_display_t> sNextId;
        static const uint32_t hostDisplayIdStart = 6;
        const hwc2_display_t mId;
        // emulator side displayId
        uint32_t mHostDisplayId;
        std::string mName;
        HWC2::DisplayType mType;
        HWC2::PowerMode mPowerMode;
        HWC2::Vsync mVsyncEnabled;
        uint32_t mVsyncPeriod;
        VsyncThread mVsyncThread;
        FencedBuffer mClientTarget;
        // Will only be non-null after the Display has been validated and
        // before it has been presented
        std::unique_ptr<Changes> mChanges;
        // All layers this Display is aware of.
        std::multiset<std::shared_ptr<Layer>, SortLayersByZ> mLayers;
        std::vector<hwc2_display_t> mReleaseLayerIds;
        std::vector<int32_t> mReleaseFences;
        std::vector<std::shared_ptr<Config>> mConfigs;
        std::shared_ptr<const Config> mActiveConfig;
        std::set<android_color_mode_t> mColorModes;
        android_color_mode_t mActiveColorMode;
        bool mSetColorTransform;
        // The state of this display should only be modified from
        // SurfaceFlinger's main loop, with the exception of when dump is
        // called. To prevent a bad state from crashing us during a dump
        // call, all public calls into Display must acquire this mutex.
        mutable std::mutex mStateMutex;
        std::unique_ptr<ComposeMsg> mComposeMsg;
        std::unique_ptr<ComposeMsg_v2> mComposeMsg_v2;
        int mSyncDeviceFd;
        const native_handle_t* mTargetCb;
    };

    template<typename MF, MF memFunc, typename ...Args>
    static int32_t displayHook(hwc2_device_t* device, hwc2_display_t displayId,
            Args... args) {
        auto display = getHWC2(device)->getDisplay(displayId);
        if (!display) {
            return static_cast<int32_t>(HWC2::Error::BadDisplay);
        }
        auto error = ((*display).*memFunc)(std::forward<Args>(args)...);
        return static_cast<int32_t>(error);
    }

    class Layer {
    public:
        explicit Layer(Display& display);
        Display& getDisplay() const {return mDisplay;}
        hwc2_layer_t getId() const {return mId;}
        bool operator==(const Layer& other) { return mId == other.mId; }
        bool operator!=(const Layer& other) { return !(*this == other); }

        // HWC2 Layer functions
        HWC2::Error setBuffer(buffer_handle_t buffer, int32_t acquireFence);
        HWC2::Error setCursorPosition(int32_t x, int32_t y);
        HWC2::Error setSurfaceDamage(hwc_region_t damage);

        // HWC2 Layer state functions
        HWC2::Error setBlendMode(int32_t mode);
        HWC2::Error setColor(hwc_color_t color);
        HWC2::Error setCompositionType(int32_t type);
        HWC2::Error setDataspace(int32_t dataspace);
        HWC2::Error setDisplayFrame(hwc_rect_t frame);
        HWC2::Error setPlaneAlpha(float alpha);
        HWC2::Error setSidebandStream(const native_handle_t* stream);
        HWC2::Error setSourceCrop(hwc_frect_t crop);
        HWC2::Error setTransform(int32_t transform);
        HWC2::Error setVisibleRegion(hwc_region_t visible);
        HWC2::Error setZ(uint32_t z);

        HWC2::Composition getCompositionType() const {
            return mCompositionType;
        }
        hwc_color_t getColor() {return mColor; }
        uint32_t getZ() {return mZ; }
        std::size_t getNumVisibleRegions() {return mVisibleRegion.size(); }
        FencedBuffer& getLayerBuffer() {return mBuffer; }
        int32_t getBlendMode() {return (int32_t)mBlendMode; }
        float getPlaneAlpha() {return mPlaneAlpha; }
        hwc_frect_t getSourceCrop() {return mSourceCrop; }
        hwc_rect_t getDisplayFrame() {return mDisplayFrame; }
        hwc_transform_t getTransform() {return (hwc_transform_t)mTransform; }
    private:
        static std::atomic<hwc2_layer_t> sNextId;
        const hwc2_layer_t mId;
        Display& mDisplay;
        FencedBuffer mBuffer;
        std::vector<hwc_rect_t> mSurfaceDamage;

        HWC2::BlendMode mBlendMode;
        hwc_color_t mColor;
        HWC2::Composition mCompositionType;
        hwc_rect_t mDisplayFrame;
        float mPlaneAlpha;
        const native_handle_t* mSidebandStream;
        hwc_frect_t mSourceCrop;
        HWC2::Transform mTransform;
        std::vector<hwc_rect_t> mVisibleRegion;
        uint32_t mZ;
    };

    template <typename MF, MF memFunc, typename ...Args>
    static int32_t layerHook(hwc2_device_t* device, hwc2_display_t displayId,
            hwc2_layer_t layerId, Args... args) {
        auto result = getHWC2(device)->getLayer(displayId, layerId);
        auto error = std::get<HWC2::Error>(result);
        if (error == HWC2::Error::None) {
            auto layer = std::get<Layer*>(result);
            error = ((*layer).*memFunc)(std::forward<Args>(args)...);
        }
        return static_cast<int32_t>(error);
    }

    // helpers
    void populateCapabilities();
    Display* getDisplay(hwc2_display_t id);
    std::tuple<Layer*, HWC2::Error> getLayer(hwc2_display_t displayId,
            hwc2_layer_t layerId);

    HWC2::Error initDisplayParameters();
    const native_handle_t* allocateDisplayColorBuffer();
    void freeDisplayColorBuffer(const native_handle_t* h);

    std::unordered_set<HWC2::Capability> mCapabilities;

    // These are potentially accessed from multiple threads, and are protected
    // by this mutex.
    std::mutex mStateMutex;

    struct CallbackInfo {
        hwc2_callback_data_t data;
        hwc2_function_pointer_t pointer;
    };
    std::unordered_map<HWC2::Callback, CallbackInfo> mCallbacks;

    // use map so displays can be pluged in by order of ID, 0, 1, 2, 3, etc.
    std::map<hwc2_display_t, std::shared_ptr<Display>> mDisplays;
    std::unordered_map<hwc2_layer_t, std::shared_ptr<Layer>> mLayers;

    int mDisplayWidth;
    int mDisplayHeight;
    int mDisplayDpiX;
    int mDisplayDpiY;
};

}
#endif
