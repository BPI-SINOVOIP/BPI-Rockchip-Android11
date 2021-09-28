//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <getopt.h>

#include <iostream>
#include <string>

#include <android-base/logging.h>
#include <binder/BinderService.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <libgsi/libgsi.h>
#include <libgsi/libgsid.h>

#include "gsi_service.h"

using android::ProcessState;
using android::sp;
using namespace std::literals;

static int DumpDeviceMapper() {
    auto service = android::gsi::GetGsiService();
    if (!service) {
        std::cerr << "Could not start IGsiService.\n";
        return 1;
    }
    std::string output;
    auto status = service->dumpDeviceMapperDevices(&output);
    if (!status.isOk()) {
        std::cerr << "Could not dump device-mapper devices: " << status.exceptionMessage().c_str()
                  << "\n";
        return 1;
    }
    std::cout << output;
    return 0;
}

int main(int argc, char** argv) {
    android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));

    if (argc > 1) {
        if (argv[1] == "run-startup-tasks"s) {
            android::gsi::GsiService::RunStartupTasks();
            exit(0);
        } else if (argv[1] == "dump-device-mapper"s) {
            int rc = DumpDeviceMapper();
            exit(rc);
        }
    }

    android::gsi::GsiService::Register();
    {
        sp<ProcessState> ps(ProcessState::self());
        ps->startThreadPool();
        ps->giveThreadPoolName();
    }
    android::IPCThreadState::self()->joinThreadPool();

    exit(0);
}
