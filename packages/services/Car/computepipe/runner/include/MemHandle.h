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

#ifndef COMPUTEPIPE_RUNNER_STREAM_MANAGER_MEMHANDLE_H
#define COMPUTEPIPE_RUNNER_STREAM_MANAGER_MEMHANDLE_H

#include <OutputConfig.pb.h>
#include <vndk/hardware_buffer.h>

namespace android {
namespace automotive {
namespace computepipe {

class MemHandle {
  public:
    /* Retrieve stream Id */
    virtual int getStreamId() const = 0;
    /* Retrieves the buffer id */
    virtual int getBufferId() const = 0;
    /* Retrieve packet type */
    virtual proto::PacketType getType() const = 0;
    /* Retrieve packet time stamp */
    virtual uint64_t getTimeStamp() const = 0;
    /* Get size */
    virtual uint32_t getSize() const = 0;
    /* Get data, raw pointer. Only implemented for copy semantics */
    virtual const char* getData() const = 0;
    /* Get native handle. data with zero copy semantics */
    virtual AHardwareBuffer* getHardwareBuffer() const = 0;

    virtual ~MemHandle() {
    }
};

}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_STREAM_MANAGER_MEMHANDLE_H
