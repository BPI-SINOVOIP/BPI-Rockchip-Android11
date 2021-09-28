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
#ifndef ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_PIPE_CONTEXT
#define ANDROID_AUTOMOTIVE_COMPUTEPIPE_ROUTER_PIPE_CONTEXT

#include <memory>
#include <string>
#include <utility>

#include "ClientHandle.h"
#include "PipeHandle.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace router {

/**
 * This is the context of a registered pipe.
 * It tracks assignments to clients and availability.
 * It also owns the handle to the runner interface.
 * This is utilized by the registry to track every registered pipe
 */
template <typename T>
class PipeContext {
  public:
    // Check if associated runner is alive
    bool isAlive() const {
        return mPipeHandle->isAlive();
    }
    // Retrieve the graph name
    std::string getGraphName() const {
        return mGraphName;
    }
    /**
     * Check if its available for clients
     *
     * If no client is assigned mClientHandle is null.
     * If a client is assigned use it to determine if its still alive,
     * If its not then the runner is available
     */
    bool isAvailable() {
        if (!mClientHandle) {
            return true;
        }
        if (!mClientHandle->isAlive()) {
            mClientHandle = nullptr;
            return true;
        }
        return false;
    }
    // Mark availability. True if available
    void setClient(std::unique_ptr<ClientHandle> clientHandle) {
        mClientHandle.reset(clientHandle.release());
    }
    // Set the name of the graph
    void setGraphName(std::string name) {
        mGraphName = name;
    }
    // Duplicate the pipehandle for retrieval by clients.
    std::unique_ptr<PipeHandle<T>> dupPipeHandle() {
        return std::unique_ptr<PipeHandle<T>>(mPipeHandle->clone());
    }
    // Setup pipecontext
    PipeContext(std::unique_ptr<PipeHandle<T>> h, std::string name)
        : mGraphName(name), mPipeHandle(std::move(h)) {
    }
    ~PipeContext() {
        if (mPipeHandle) {
            mPipeHandle = nullptr;
        }
        if (mClientHandle) {
            mClientHandle = nullptr;
        }
    }

  private:
    std::string mGraphName;
    std::unique_ptr<PipeHandle<T>> mPipeHandle;
    std::unique_ptr<ClientHandle> mClientHandle;
    bool hasClient;
};

}  // namespace router
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
