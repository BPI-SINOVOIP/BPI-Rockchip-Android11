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

#include <compositionengine/CompositionRefreshArgs.h>
#include <compositionengine/LayerFE.h>
#include <compositionengine/LayerFECompositionState.h>
#include <compositionengine/OutputLayer.h>
#include <compositionengine/impl/CompositionEngine.h>
#include <compositionengine/impl/Display.h>

#include <renderengine/RenderEngine.h>
#include <utils/Trace.h>

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

#include "DisplayHardware/HWComposer.h"

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic pop // ignored "-Wconversion"

namespace android::compositionengine {

CompositionEngine::~CompositionEngine() = default;

namespace impl {

std::unique_ptr<compositionengine::CompositionEngine> createCompositionEngine() {
    return std::make_unique<CompositionEngine>();
}

CompositionEngine::CompositionEngine() = default;
CompositionEngine::~CompositionEngine() = default;

std::shared_ptr<compositionengine::Display> CompositionEngine::createDisplay(
        const DisplayCreationArgs& args) {
    return compositionengine::impl::createDisplay(*this, args);
}

std::unique_ptr<compositionengine::LayerFECompositionState>
CompositionEngine::createLayerFECompositionState() {
    return std::make_unique<compositionengine::LayerFECompositionState>();
}

HWComposer& CompositionEngine::getHwComposer() const {
    return *mHwComposer.get();
}

void CompositionEngine::setHwComposer(std::unique_ptr<HWComposer> hwComposer) {
    mHwComposer = std::move(hwComposer);
}

renderengine::RenderEngine& CompositionEngine::getRenderEngine() const {
    return *mRenderEngine.get();
}

void CompositionEngine::setRenderEngine(std::unique_ptr<renderengine::RenderEngine> renderEngine) {
    mRenderEngine = std::move(renderEngine);
}

TimeStats& CompositionEngine::getTimeStats() const {
    return *mTimeStats.get();
}

void CompositionEngine::setTimeStats(const std::shared_ptr<TimeStats>& timeStats) {
    mTimeStats = timeStats;
}

bool CompositionEngine::needsAnotherUpdate() const {
    return mNeedsAnotherUpdate;
}

nsecs_t CompositionEngine::getLastFrameRefreshTimestamp() const {
    return mRefreshStartTime;
}

void CompositionEngine::present(CompositionRefreshArgs& args) {
    ATRACE_CALL();
    ALOGV(__FUNCTION__);

    preComposition(args);

    {
        // latchedLayers is used to track the set of front-end layer state that
        // has been latched across all outputs for the prepare step, and is not
        // needed for anything else.
        LayerFESet latchedLayers;

        for (const auto& output : args.outputs) {
            output->prepare(args, latchedLayers);
        }
    }

    updateLayerStateFromFE(args);

#if USE_HWC2ON1ADAPTER
    /*
     * For HWC2 adapter to HWC1 on RK platform.
     * Android 11.0 SurfaceFlinger call HWC by the following software process :
     *   1. Primary display: updateInfo(Primary display) -> prepareFrame -> hwc-perpare -> postFramebuffer -> hwc-set
     *   2. Extend  display: updateInfo(Extend display) -> prepareFrame -> hwc-perpare -> postFramebuffer -> hwc-set
     * It's not suitable for HWC1 version,so amended to the following process:
     *   1. updateInfo(Primary) -> updateInfo(Extend)
     *   2. prepareFrame -> hwc-prepare (Primary and Extend display)
     *   3. postFramebuffer -> hwc-set (Primary and Extend display)
     */
    for (const auto& output : args.outputs) {
        output->updateInfoForHwc2On1Adapter(args);
    }
    for (const auto& output : args.outputs) {
        output->presentForHwc2On1Adapter(args);
    }
    for (const auto& output : args.outputs) {
        output->postBufferForHwc2On1Adapter();
    }
#else
    for (const auto& output : args.outputs) {
        output->present(args);
    }
#endif /* #if USE_HWC2ON1ADAPTER */
}

void CompositionEngine::updateCursorAsync(CompositionRefreshArgs& args) {
    std::unordered_map<compositionengine::LayerFE*, compositionengine::LayerFECompositionState*>
            uniqueVisibleLayers;

    for (const auto& output : args.outputs) {
        for (auto* layer : output->getOutputLayersOrderedByZ()) {
            if (layer->isHardwareCursor()) {
                // Latch the cursor composition state from each front-end layer.
                layer->getLayerFE().prepareCompositionState(LayerFE::StateSubset::Cursor);
                layer->writeCursorPositionToHWC();
            }
        }
    }
}

void CompositionEngine::preComposition(CompositionRefreshArgs& args) {
    ATRACE_CALL();
    ALOGV(__FUNCTION__);

    bool needsAnotherUpdate = false;

    mRefreshStartTime = systemTime(SYSTEM_TIME_MONOTONIC);

    for (auto& layer : args.layers) {
        if (layer->onPreComposition(mRefreshStartTime)) {
            needsAnotherUpdate = true;
        }
    }

    mNeedsAnotherUpdate = needsAnotherUpdate;
}

void CompositionEngine::dump(std::string&) const {
    // The base class has no state to dump, but derived classes might.
}

void CompositionEngine::setNeedsAnotherUpdateForTest(bool value) {
    mNeedsAnotherUpdate = value;
}

void CompositionEngine::updateLayerStateFromFE(CompositionRefreshArgs& args) {
    // Update the composition state from each front-end layer
    for (const auto& output : args.outputs) {
        output->updateLayerStateFromFE(args);
    }
}

} // namespace impl
} // namespace android::compositionengine
