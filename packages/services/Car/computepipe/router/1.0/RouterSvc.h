/**
 * Copyright 2019 The Android Open Source Project
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
#ifndef ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_ROUTERSVC
#define ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_V1_0_ROUTERSVC

#include "PipeQuery.h"
#include "PipeRegistration.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace router {
namespace V1_0 {
namespace implementation {

namespace router = android::automotive::computepipe::router;

class RouterRegistry : public router::PipeRegistry<PipeRunner> {
    std ::unique_ptr<PipeHandle<PipeRunner>> getDebuggerPipeHandle(const std::string& name) {
        return getPipeHandle(name, nullptr);
    }
    Error RemoveEntry(const std::string& name) {
        return DeletePipeHandle(name);
    }
};

class RouterSvc {
  public:
    router::Error initSvc();
    router::Error parseArgs(int argc, char** argv);
    std::string getSvcName() {
        return mSvcName;
    }

  private:
    router::Error initQueryEngine();
    router::Error initRegistrationEngine();

    std::string mSvcName = "ComputePipeRouter";
    std::shared_ptr<PipeQuery> mQueryEngine;
    std::shared_ptr<PipeRegistration> mRegisterEngine;
    std::shared_ptr<RouterRegistry> mRegistry;
};
}  // namespace implementation
}  // namespace V1_0
}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
