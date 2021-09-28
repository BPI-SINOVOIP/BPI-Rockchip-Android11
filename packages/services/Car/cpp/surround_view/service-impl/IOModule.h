/*
 * Copyright 2020 The Android Open Source Project
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
#ifndef SURROUND_VIEW_SERVICE_IMPL_IOMODULE_H_
#define SURROUND_VIEW_SERVICE_IMPL_IOMODULE_H_

#include "IOModuleCommon.h"

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

// I/O Module class processing all I/O related operations.
class IOModule {
public:
    // Constructor with file name( and path) of config file.
    IOModule(const std::string& svConfigFile);

    // Reads all config files and stores parsed results in mIOModuleConfig.
    IOStatus initialize();

    // Gets config data read from files. initialize must be called this.
    bool getConfig(IOModuleConfig* ioModuleConfig);

private:
    // Config string filename.
    std::string mSvConfigFile;

    // Indicates initialize success/fail.
    bool mIsInitialized;

    // Stores the parsed config.
    IOModuleConfig mIOModuleConfig;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
#endif  // SURROUND_VIEW_SERVICE_IMPL_IOMODULE_H_
