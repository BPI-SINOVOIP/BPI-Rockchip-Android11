/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "Callbacks"

#include "Callbacks.h"

#include <android-base/logging.h>
#include <limits>
#include <utility>
#include <vector>

namespace android::nn {

using namespace hal;

constexpr Timing kNoTiming = {.timeOnDevice = std::numeric_limits<uint64_t>::max(),
                              .timeInDriver = std::numeric_limits<uint64_t>::max()};

// PreparedModelCallback methods begin here

Return<void> PreparedModelCallback::notifyInternal(bool deadObject, ErrorStatus errorStatus,
                                                   const sp<V1_0::IPreparedModel>& preparedModel) {
    {
        std::lock_guard<std::mutex> hold(mMutex);

        // quick-return if object has already been notified
        if (mNotified) {
            return Void();
        }

        // store results and mark as notified
        mDeadObject = deadObject;
        mErrorStatus = errorStatus;
        mPreparedModel = preparedModel;
        mNotified = true;
    }

    mCondition.notify_all();
    return Void();
}

Return<void> PreparedModelCallback::notify(V1_0::ErrorStatus errorStatus,
                                           const sp<V1_0::IPreparedModel>& preparedModel) {
    return notifyInternal(false, static_cast<ErrorStatus>(errorStatus), preparedModel);
}

Return<void> PreparedModelCallback::notify_1_2(V1_0::ErrorStatus errorStatus,
                                               const sp<V1_2::IPreparedModel>& preparedModel) {
    return notifyInternal(false, static_cast<ErrorStatus>(errorStatus), preparedModel);
}

Return<void> PreparedModelCallback::notify_1_3(ErrorStatus errorStatus,
                                               const sp<V1_3::IPreparedModel>& preparedModel) {
    return notifyInternal(false, errorStatus, preparedModel);
}

void PreparedModelCallback::notifyAsDeadObject() {
    notifyInternal(true, ErrorStatus::GENERAL_FAILURE, nullptr);
}

void PreparedModelCallback::wait() const {
    std::unique_lock<std::mutex> lock(mMutex);
    mCondition.wait(lock, [this] { return mNotified; });
}

ErrorStatus PreparedModelCallback::getStatus() const {
    wait();
    return mErrorStatus;
}

sp<V1_0::IPreparedModel> PreparedModelCallback::getPreparedModel() const {
    wait();
    return mPreparedModel;
}

bool PreparedModelCallback::isDeadObject() const {
    wait();
    return mDeadObject;
}

// ExecutionCallback methods begin here

Return<void> ExecutionCallback::notify(V1_0::ErrorStatus errorStatus) {
    return notifyInternal(false, static_cast<ErrorStatus>(errorStatus), {}, kNoTiming);
}

Return<void> ExecutionCallback::notify_1_2(V1_0::ErrorStatus errorStatus,
                                           const hidl_vec<OutputShape>& outputShapes,
                                           const Timing& timing) {
    return notifyInternal(false, static_cast<ErrorStatus>(errorStatus), outputShapes, timing);
}

Return<void> ExecutionCallback::notify_1_3(V1_3::ErrorStatus errorStatus,
                                           const hidl_vec<OutputShape>& outputShapes,
                                           const Timing& timing) {
    return notifyInternal(false, errorStatus, outputShapes, timing);
}

void ExecutionCallback::notifyAsDeadObject() {
    notifyInternal(true, ErrorStatus::GENERAL_FAILURE, {}, kNoTiming);
}

void ExecutionCallback::wait() const {
    std::unique_lock<std::mutex> lock(mMutex);
    mCondition.wait(lock, [this] { return mNotified; });

    /*
     * Note that we cannot call std::thread::join from ExecutionCallback's
     * destructor: ExecutionCallback is intended to be reference counted, and it
     * is possible that the reference count drops to zero in the bound thread,
     * causing the bound thread to call this destructor. If a thread tries to
     * join itself, it throws an exception, producing a message like the
     * following:
     *
     *     terminating with uncaught exception of type std::__1::system_error:
     *     thread::join failed: Resource deadlock would occur
     */
    if (mThread.joinable()) {
        mThread.join();
    }
}

ErrorStatus ExecutionCallback::getStatus() const {
    wait();
    return mErrorStatus;
}

const std::vector<OutputShape>& ExecutionCallback::getOutputShapes() const {
    wait();
    return mOutputShapes;
}

Timing ExecutionCallback::getTiming() const {
    wait();
    return mTiming;
}

bool ExecutionCallback::isDeadObject() const {
    wait();
    return mDeadObject;
}

bool ExecutionCallback::bindThread(std::thread asyncThread) {
    std::lock_guard<std::mutex> lock(mMutex);

    // Ensure ExecutionCallback object does not already have a thread bound
    if (mThread.joinable()) {
        LOG(ERROR) << "ExecutionCallback::bindThread -- a thread has already been bound to this "
                      "callback object";
        return false;
    }

    // Ensure the new thread is valid
    if (!asyncThread.joinable()) {
        LOG(ERROR) << "ExecutionCallback::bindThread -- the new thread is not joinable";
        return false;
    }

    mThread = std::move(asyncThread);
    return true;
}

void ExecutionCallback::setOnFinish(const ExecutionFinish& finish) {
    std::lock_guard<std::mutex> hold(mMutex);

    // Ensure ExecutionCallback object does not already have a "finish" callback
    if (mOnFinish != nullptr) {
        LOG(ERROR) << "ExecutionCallback::setOnFinish -- object already has a \"finish\" callback";
        return;
    }

    // Ensure new "finish" callback is valid
    if (finish == nullptr) {
        LOG(ERROR) << "ExecutionCallback::setOnFinish -- \"finish\" callback is invalid";
        return;
    }

    // Essure ExecutionCallback object has not already been notified
    if (mNotified) {
        LOG(ERROR) << "ExecutionCallback::setOnFinish -- ExecutionCallback has already been "
                      "notified with results";
        return;
    }

    mOnFinish = finish;
}

Return<void> ExecutionCallback::notifyInternal(bool deadObject, ErrorStatus errorStatus,
                                               std::vector<OutputShape> outputShapes,
                                               Timing timing) {
    // check results
    if (!deadObject) {
        if (errorStatus == ErrorStatus::OUTPUT_INSUFFICIENT_SIZE) {
            // outputShapes must not be empty if OUTPUT_INSUFFICIENT_SIZE.
            if (outputShapes.size() == 0) {
                LOG(ERROR)
                        << "Notified with empty output shape vector when OUTPUT_INSUFFICIENT_SIZE";
                errorStatus = ErrorStatus::GENERAL_FAILURE;
                outputShapes = {};
                timing = kNoTiming;
            }
        } else if (errorStatus != ErrorStatus::NONE) {
            // outputShapes must be empty if errorStatus is neither NONE nor
            // OUTPUT_INSUFFICIENT_SIZE.
            if (outputShapes.size() != 0) {
                LOG(ERROR) << "Notified with non-empty output shape vector when error status is "
                              "neither NONE nor OUTPUT_INSUFFICIENT_SIZE";
                errorStatus = ErrorStatus::GENERAL_FAILURE;
                outputShapes = {};
                timing = kNoTiming;
            }
        }
    }

    // store results
    {
        std::lock_guard<std::mutex> hold(mMutex);

        // quick-return if object has already been notified
        if (mNotified) {
            return Void();
        }

        mDeadObject = deadObject;
        mErrorStatus = errorStatus;
        mOutputShapes = std::move(outputShapes);
        mTiming = timing;
        mNotified = true;

        if (mOnFinish != nullptr) {
            ErrorStatus status = mOnFinish(mErrorStatus, mOutputShapes);
            mOnFinish = nullptr;
            if (status != ErrorStatus::NONE) {
                mErrorStatus = status;
            }
        }
    }
    mCondition.notify_all();
    return Void();
}

}  // namespace android::nn
