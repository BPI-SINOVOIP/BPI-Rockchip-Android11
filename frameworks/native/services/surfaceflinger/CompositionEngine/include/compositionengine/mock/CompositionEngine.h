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

#pragma once

#include <compositionengine/CompositionEngine.h>
#include <compositionengine/CompositionRefreshArgs.h>
#include <compositionengine/DisplayCreationArgs.h>
#include <compositionengine/LayerFECompositionState.h>
#include <gmock/gmock.h>
#include <renderengine/RenderEngine.h>

#include "DisplayHardware/HWComposer.h"

namespace android::compositionengine::mock {

class CompositionEngine : public compositionengine::CompositionEngine {
public:
    CompositionEngine();
    ~CompositionEngine() override;

    MOCK_METHOD1(createDisplay, std::shared_ptr<Display>(const DisplayCreationArgs&));
    MOCK_METHOD0(createLayerFECompositionState,
                 std::unique_ptr<compositionengine::LayerFECompositionState>());

    MOCK_CONST_METHOD0(getHwComposer, HWComposer&());
    MOCK_METHOD1(setHwComposer, void(std::unique_ptr<HWComposer>));

    MOCK_CONST_METHOD0(getRenderEngine, renderengine::RenderEngine&());
    MOCK_METHOD1(setRenderEngine, void(std::unique_ptr<renderengine::RenderEngine>));

    MOCK_CONST_METHOD0(getTimeStats, TimeStats&());
    MOCK_METHOD1(setTimeStats, void(const std::shared_ptr<TimeStats>&));

    MOCK_CONST_METHOD0(needsAnotherUpdate, bool());
    MOCK_CONST_METHOD0(getLastFrameRefreshTimestamp, nsecs_t());

    MOCK_METHOD1(present, void(CompositionRefreshArgs&));
    MOCK_METHOD1(updateCursorAsync, void(CompositionRefreshArgs&));

    MOCK_METHOD1(preComposition, void(CompositionRefreshArgs&));

    MOCK_CONST_METHOD1(dump, void(std::string&));
};

} // namespace android::compositionengine::mock
