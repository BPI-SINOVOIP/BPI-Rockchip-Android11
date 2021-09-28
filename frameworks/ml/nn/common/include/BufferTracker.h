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

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_BUFFER_TRACKER_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_BUFFER_TRACKER_H

#include <android-base/macros.h>

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stack>
#include <utility>
#include <vector>

#include "CpuExecutor.h"
#include "HalInterfaces.h"
#include "Utils.h"

namespace android::nn {

// This class manages a CPU buffer allocated on heap and provides validation methods.
class ManagedBuffer {
   public:
    static std::shared_ptr<ManagedBuffer> create(uint32_t size, std::set<PreparedModelRole> roles,
                                                 const hal::Operand& operand);

    // Prefer ManagedBuffer::create.
    ManagedBuffer(std::unique_ptr<uint8_t[]> buffer, uint32_t size,
                  std::set<PreparedModelRole> roles, const hal::Operand& operand);

    RunTimePoolInfo createRunTimePoolInfo() const {
        return RunTimePoolInfo::createFromExistingBuffer(kBuffer.get(), kSize);
    }

    // "poolIndex" is the index of this buffer in the request.pools.
    hal::ErrorStatus validateRequest(uint32_t poolIndex, const hal::Request& request,
                                     const hal::IPreparedModel* preparedModel) const;

    // "size" is the byte size of the hidl_memory provided to the copyFrom or copyTo method.
    hal::ErrorStatus validateCopyFrom(const std::vector<uint32_t>& dimensions, uint32_t size) const;
    hal::ErrorStatus validateCopyTo(uint32_t size) const;

    bool updateDimensions(const std::vector<uint32_t>& dimensions);
    void setInitialized(bool initialized);

   private:
    mutable std::mutex mMutex;
    const std::unique_ptr<uint8_t[]> kBuffer;
    const uint32_t kSize;
    const std::set<PreparedModelRole> kRoles;
    const hal::OperandType kOperandType;
    const std::vector<uint32_t> kInitialDimensions;
    std::vector<uint32_t> mUpdatedDimensions;
    bool mInitialized = false;
};

// Keep track of all ManagedBuffers and assign each with a unique token.
class BufferTracker : public std::enable_shared_from_this<BufferTracker> {
    DISALLOW_COPY_AND_ASSIGN(BufferTracker);

   public:
    // A RAII class to help manage the lifetime of the token.
    // It is only supposed to be constructed in BufferTracker::add.
    class Token {
        DISALLOW_COPY_AND_ASSIGN(Token);

       public:
        Token(uint32_t token, std::shared_ptr<BufferTracker> tracker)
            : kToken(token), kBufferTracker(std::move(tracker)) {}
        ~Token() { kBufferTracker->free(kToken); }
        uint32_t get() const { return kToken; }

       private:
        const uint32_t kToken;
        const std::shared_ptr<BufferTracker> kBufferTracker;
    };

    // The factory of BufferTracker. This ensures that the BufferTracker is always managed by a
    // shared_ptr.
    static std::shared_ptr<BufferTracker> create() { return std::make_shared<BufferTracker>(); }

    // Prefer BufferTracker::create.
    BufferTracker() : mTokenToBuffers(1) {}

    std::unique_ptr<Token> add(std::shared_ptr<ManagedBuffer> buffer);
    std::shared_ptr<ManagedBuffer> get(uint32_t token) const;

   private:
    void free(uint32_t token);

    mutable std::mutex mMutex;
    std::stack<uint32_t, std::vector<uint32_t>> mFreeTokens;

    // Since the tokens are allocated in a non-sparse way, we use a vector to represent the mapping.
    // The index of the vector is the token. When the token gets freed, the corresponding entry is
    // set to nullptr. mTokenToBuffers[0] is always set to nullptr because 0 is an invalid token.
    std::vector<std::shared_ptr<ManagedBuffer>> mTokenToBuffers;
};

}  // namespace android::nn

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_BUFFER_TRACKER_H
