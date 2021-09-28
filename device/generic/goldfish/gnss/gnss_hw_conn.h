/*
 * Copyright (C) 2020 The Android Open Source Project
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
#include <android-base/unique_fd.h>
#include <mutex>
#include <thread>
#include "data_sink.h"

namespace goldfish {
using ::android::base::unique_fd;

class GnssHwConn {
public:
    explicit GnssHwConn(const DataSink* sink);
    ~GnssHwConn();

    bool ok() const;
    bool start();
    bool stop();

private:
    static void workerThread(int devFd, int threadsFd, const DataSink* sink);
    static int workerThreadRcvCommand(int fd);
    bool sendWorkerThreadCommand(char cmd) const;

    unique_fd m_devFd;      // Goldfish GPS QEMU device
    // a pair of connected sockets to talk to the worker thread
    unique_fd m_callersFd;  // a caller writes here
    unique_fd m_threadsFd;  // the worker thread listens from here
    std::thread m_thread;
};

}  // namespace goldfish
