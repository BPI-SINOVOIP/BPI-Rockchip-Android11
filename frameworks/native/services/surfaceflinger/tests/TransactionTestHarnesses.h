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
#ifndef ANDROID_TRANSACTION_TEST_HARNESSES
#define ANDROID_TRANSACTION_TEST_HARNESSES

#include <ui/DisplayState.h>

#include "LayerTransactionTest.h"

namespace android {

using android::hardware::graphics::common::V1_1::BufferUsage;

class LayerRenderPathTestHarness {
public:
    LayerRenderPathTestHarness(LayerTransactionTest* delegate, RenderPath renderPath)
          : mDelegate(delegate), mRenderPath(renderPath) {}

    std::unique_ptr<ScreenCapture> getScreenCapture() {
        switch (mRenderPath) {
            case RenderPath::SCREENSHOT:
                return mDelegate->screenshot();
            case RenderPath::VIRTUAL_DISPLAY:

                const auto displayToken = SurfaceComposerClient::getInternalDisplayToken();

                ui::DisplayState displayState;
                SurfaceComposerClient::getDisplayState(displayToken, &displayState);

                DisplayConfig displayConfig;
                SurfaceComposerClient::getActiveDisplayConfig(displayToken, &displayConfig);
                const ui::Size& resolution = displayConfig.resolution;

                sp<IBinder> vDisplay;
                sp<IGraphicBufferProducer> producer;
                sp<IGraphicBufferConsumer> consumer;
                sp<BufferItemConsumer> itemConsumer;
                BufferQueue::createBufferQueue(&producer, &consumer);

                consumer->setConsumerName(String8("Virtual disp consumer"));
                consumer->setDefaultBufferSize(resolution.getWidth(), resolution.getHeight());

                itemConsumer = new BufferItemConsumer(consumer,
                                                      // Sample usage bits from screenrecord
                                                      GRALLOC_USAGE_HW_VIDEO_ENCODER |
                                                              GRALLOC_USAGE_SW_READ_OFTEN);

                vDisplay = SurfaceComposerClient::createDisplay(String8("VirtualDisplay"),
                                                                false /*secure*/);

                SurfaceComposerClient::Transaction t;
                t.setDisplaySurface(vDisplay, producer);
                t.setDisplayLayerStack(vDisplay, 0);
                t.setDisplayProjection(vDisplay, displayState.orientation,
                                       Rect(displayState.viewport), Rect(resolution));
                t.apply();
                SurfaceComposerClient::Transaction().apply(true);
                BufferItem item;
                itemConsumer->acquireBuffer(&item, 0, true);
                auto sc = std::make_unique<ScreenCapture>(item.mGraphicBuffer);
                itemConsumer->releaseBuffer(item);
                SurfaceComposerClient::destroyDisplay(vDisplay);
                return sc;
        }
    }

protected:
    LayerTransactionTest* mDelegate;
    RenderPath mRenderPath;
};

class LayerTypeTransactionHarness : public LayerTransactionTest {
public:
    LayerTypeTransactionHarness(uint32_t layerType) : mLayerType(layerType) {}

    sp<SurfaceControl> createLayer(const char* name, uint32_t width, uint32_t height,
                                   uint32_t flags = 0, SurfaceControl* parent = nullptr,
                                   uint32_t* outTransformHint = nullptr,
                                   PixelFormat format = PIXEL_FORMAT_RGBA_8888) {
        // if the flags already have a layer type specified, return an error
        if (flags & ISurfaceComposerClient::eFXSurfaceMask) {
            return nullptr;
        }
        return LayerTransactionTest::createLayer(name, width, height, flags | mLayerType, parent,
                                                 outTransformHint, format);
    }

    void fillLayerColor(const sp<SurfaceControl>& layer, const Color& color, int32_t bufferWidth,
                        int32_t bufferHeight) {
        ASSERT_NO_FATAL_FAILURE(LayerTransactionTest::fillLayerColor(mLayerType, layer, color,
                                                                     bufferWidth, bufferHeight));
    }

    void fillLayerQuadrant(const sp<SurfaceControl>& layer, int32_t bufferWidth,
                           int32_t bufferHeight, const Color& topLeft, const Color& topRight,
                           const Color& bottomLeft, const Color& bottomRight) {
        ASSERT_NO_FATAL_FAILURE(LayerTransactionTest::fillLayerQuadrant(mLayerType, layer,
                                                                        bufferWidth, bufferHeight,
                                                                        topLeft, topRight,
                                                                        bottomLeft, bottomRight));
    }

protected:
    uint32_t mLayerType;
};
} // namespace android
#endif
