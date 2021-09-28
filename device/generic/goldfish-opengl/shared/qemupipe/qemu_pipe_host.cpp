// Copyright 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <qemu_pipe_bp.h>

#include "android/emulation/hostdevices/HostGoldfishPipe.h"

#if PLATFORM_SDK_VERSION < 26
#include <cutils/log.h>
#else
#include <log/log.h>
#endif

#include <errno.h>

using android::HostGoldfishPipeDevice;

QEMU_PIPE_HANDLE qemu_pipe_open(const char* pipeName) {
    return HostGoldfishPipeDevice::get()->connect(pipeName);
}

void qemu_pipe_close(QEMU_PIPE_HANDLE pipe) {
    HostGoldfishPipeDevice::get()->close(pipe);
}

int qemu_pipe_read(QEMU_PIPE_HANDLE pipe, void* buffer, int len) {
    return HostGoldfishPipeDevice::get()->read(pipe, buffer, len);
}

int qemu_pipe_write(QEMU_PIPE_HANDLE pipe, const void* buffer, int len) {
    return HostGoldfishPipeDevice::get()->write(pipe, buffer, len);
}

int qemu_pipe_try_again(int ret) {
    if (ret < 0) {
        int err = HostGoldfishPipeDevice::get()->getErrno();
        return err == EINTR || err == EAGAIN;
    } else {
        return false;
    }
}

void qemu_pipe_print_error(QEMU_PIPE_HANDLE pipe) {
    int err = HostGoldfishPipeDevice::get()->getErrno();
    ALOGE("pipe error: pipe %p err %d", pipe, err);
}
