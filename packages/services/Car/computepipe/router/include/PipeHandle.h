/*
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
#ifndef ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_PIPE_HANDLE
#define ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_PIPE_HANDLE

#include <memory>
#include <string>
#include <utility>

namespace android {
namespace automotive {
namespace computepipe {
namespace router {

/**
 * This abstracts the runner interface object and hides its
 * details from the inner routing logic.
 */
template <typename T>
class PipeHandle {
  public:
    explicit PipeHandle(std::unique_ptr<T> intf) : mInterface(std::move(intf)) {
    }
    // Check if runner process is still alive
    virtual bool isAlive() = 0;
    // Start the monitor for the pipe
    virtual bool startPipeMonitor() = 0;
    // Any successful client lookup, clones this handle
    // The implementation must handle refcounting of remote objects
    // accordingly.
    virtual PipeHandle<T>* clone() const = 0;
    // Retrieve the underlying remote IPC object
    std::shared_ptr<T> getInterface() {
        return mInterface;
    }
    virtual ~PipeHandle() = default;

  protected:
    explicit PipeHandle(std::shared_ptr<T> intf) : mInterface(intf){};
    // Interface object
    std::shared_ptr<T> mInterface;
};

}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif
