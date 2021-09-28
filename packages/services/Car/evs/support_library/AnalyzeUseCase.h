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
#ifndef CAR_LIB_EVS_SUPPORT_ANALYZE_USECASE_H
#define CAR_LIB_EVS_SUPPORT_ANALYZE_USECASE_H

#include <thread>

#include "BaseUseCase.h"
#include "StreamHandler.h"
#include "BaseAnalyzeCallback.h"
#include "ResourceManager.h"

using ::android::sp;
using ::android::hardware::Return;
using ::std::string;

namespace android {
namespace automotive {
namespace evs {
namespace support {

class AnalyzeUseCase : public BaseUseCase {
public:
    AnalyzeUseCase(string cameraId, BaseAnalyzeCallback* analyzeCallback);
    virtual ~AnalyzeUseCase();
    virtual bool startVideoStream() override;
    virtual void stopVideoStream() override;

    static AnalyzeUseCase createDefaultUseCase(string cameraId,
                                               BaseAnalyzeCallback* cb = nullptr);

private:
    bool initialize();

    bool mIsInitialized = false;
    BaseAnalyzeCallback* mAnalyzeCallback = nullptr;

    sp<StreamHandler>           mStreamHandler;
    sp<ResourceManager>         mResourceManager;
};

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif // CAR_LIB_EVS_SUPPORT_ANALYZE_USECASE_H
