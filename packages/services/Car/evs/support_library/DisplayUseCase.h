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
#ifndef CAR_LIB_EVS_SUPPORT_DISPLAY_USECASE_H
#define CAR_LIB_EVS_SUPPORT_DISPLAY_USECASE_H

#include <android/hardware/automotive/evs/1.0/IEvsDisplay.h>

#include <string>
#include <thread>

#include "BaseRenderCallback.h"
#include "BaseUseCase.h"
#include "ConfigManager.h"
#include "RenderBase.h"
#include "StreamHandler.h"
#include "ResourceManager.h"

namespace android {
namespace automotive {
namespace evs {
namespace support {

using namespace ::android::hardware::automotive::evs::V1_0;
using ::android::sp;
using ::android::hardware::Return;
using ::std::string;

// TODO(b/130246434): Think about multi-camera situation.
class DisplayUseCase : public BaseUseCase {
  public:
    ~DisplayUseCase();
    bool startVideoStream() override;
    void stopVideoStream() override;

    // TODO(b/130246434): Add configuration class to create more use case.
    static DisplayUseCase createDefaultUseCase(string cameraId,
                                               BaseRenderCallback* cb = nullptr);

  private:
    DisplayUseCase(string cameraId, BaseRenderCallback* renderCallback);

    // TODO(b/130246434): Think about whether we should make init public so
    // users can call it.
    bool initialize();
    bool streamFrame();

    bool mIsInitialized = false;
    BaseRenderCallback* mRenderCallback = nullptr;
    std::unique_ptr<RenderBase> mCurrentRenderer;

    sp<IEvsDisplay> mDisplay;
    sp<StreamHandler> mStreamHandler;
    sp<ResourceManager> mResourceManager;
    bool mIsReadyToRun;
    std::thread mWorkerThread;
    BufferDesc mImageBuffer;
};

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif  // CAR_LIB_EVS_SUPPORT_DISPLAY_USECASE_H
