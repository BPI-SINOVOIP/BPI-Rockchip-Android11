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

#include "VhalHandler.h"

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>

#include <android-base/logging.h>
#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>
#include <time.h>
#include <utils/SystemClock.h>
#include <utils/Timers.h>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

using vehicle::V2_0::IVehicle;
using vehicle::V2_0::StatusCode;
using vehicle::V2_0::VehiclePropertyType;
using vehicle::V2_0::VehiclePropValue;

bool VhalHandler::initialize(UpdateMethod updateMethod, int rate) {
    LOG(DEBUG) << __FUNCTION__;
    std::scoped_lock<std::mutex> lock(mAccessLock);

    if (mIsInitialized) {
        LOG(ERROR) << "Vehicle Handler is already initialized.";
        return false;
    }

    LOG(INFO) << "Connecting to Vehicle HAL";
    mVhalServicePtr = IVehicle::getService();
    if (mVhalServicePtr.get() == nullptr) {
        LOG(ERROR) << "Vehicle HAL getService failed.";
        return false;
    }

    if (rate < 1 || rate > 100) {
        LOG(ERROR) << "Rate must be in the range [1, 100].";
        return false;
    }

    if (mUpdateMethod == UpdateMethod::SUBSCRIBE) {
        LOG(ERROR) << "Update method Subscribe is not currently implemented.";
        return false;
    }

    mUpdateMethod = updateMethod;
    mRate = rate;
    mIsInitialized = true;
    mIsUpdateActive = false;

    return true;
}

void VhalHandler::pollProperties() {
    LOG(DEBUG) << "Polling thread started.";
    while (true) {
        nsecs_t startTime = elapsedRealtimeNano();

        // Copy properties to read.
        std::vector<VehiclePropValue> propertiesToRead;
        int rate;
        {
            std::scoped_lock<std::mutex> lock(mAccessLock);
            if (!mIsUpdateActive) {
                LOG(DEBUG) << "Exiting polling thread.";
                break;
            }
            propertiesToRead = mPropertiesToRead;
            rate = mRate;
        }

        // Make get call for each VHAL property.
        // Write to back property values, note lock is not needed as only this thread uses it.
        std::vector<VehiclePropValue> vehiclePropValuesUpdated;
        for (auto& propertyToRead : propertiesToRead) {
            StatusCode statusResult;
            VehiclePropValue propValueResult;
            mVhalServicePtr->get(propertyToRead,
                                 [&statusResult,
                                  &propValueResult](StatusCode status,
                                                    const VehiclePropValue& propValue) {
                                     statusResult = status;
                                     propValueResult = propValue;
                                 });
            if (statusResult != StatusCode::OK) {
                LOG(WARNING) << "Failed to read vhal property: " << propertyToRead.prop
                             << ", with status code: " << static_cast<int32_t>(statusResult);
            } else {
                vehiclePropValuesUpdated.push_back(propValueResult);
            }
        }

        // Update property values by swapping with updated property values.
        {
            std::scoped_lock<std::mutex> lock(mAccessLock);
            std::swap(mPropertyValues, vehiclePropValuesUpdated);
        }

        std::unique_lock<std::mutex> sleepLock(mPollThreadSleepMutex);
        // Sleep to generate frames at kTargetFrameRate.
        // rate is number of updates per seconds,
        // Target time period between two updates in nano-seconds = (10 ^ 9) / rate.
        const nsecs_t kTargetRateNs = std::pow(10, 9) / mRate;
        const nsecs_t now = elapsedRealtimeNano();
        const nsecs_t workTimeNs = now - startTime;
        const nsecs_t sleepDurationNs = kTargetRateNs - workTimeNs;
        if (sleepDurationNs > 0) {
            // Sleep for sleepDurationNs or until a stop signal is received.
            mPollThreadCondition.wait_for(sleepLock, std::chrono::nanoseconds(sleepDurationNs),
                                          [this]() { return mPollStopSleeping; });
        }
    }
}

bool VhalHandler::startPropertiesUpdate() {
    LOG(DEBUG) << __FUNCTION__;
    std::scoped_lock<std::mutex> lock(mAccessLock);

    // Check Vhal service is initialized.
    if (!mIsInitialized) {
        LOG(ERROR) << "VHAL handler not initialized.";
        return false;
    }

    if (mIsUpdateActive) {
        LOG(ERROR) << "Polling is already started.";
        return false;
    }

    mIsUpdateActive = true;

    {
        std::scoped_lock<std::mutex> sleepLock(mPollThreadSleepMutex);
        mPollStopSleeping = false;
    }

    // Start polling thread if updated method is GET.
    if (mUpdateMethod == UpdateMethod::GET) {
        mPollingThread = std::thread([this]() { pollProperties(); });
    }

    return true;
}

bool VhalHandler::setPropertiesToRead(const std::vector<VehiclePropValue>& propertiesToRead) {
    LOG(DEBUG) << __FUNCTION__;
    std::scoped_lock<std::mutex> lock(mAccessLock);

    // Replace property ids to read.
    mPropertiesToRead = propertiesToRead;

    return true;
}

bool VhalHandler::setPropertiesToRead(const std::vector<uint64_t>& propertiesToRead) {
    LOG(DEBUG) << __FUNCTION__;
    std::vector<VehiclePropValue> vhalPropValues;
    for (const auto& property : propertiesToRead) {
        VehiclePropValue propValue;
        // Higher 32 bits = property id, lower 32 bits = area id.
        propValue.areaId = property & 0xFFFFFFFF;
        propValue.prop = (property >> 32) & 0xFFFFFFFF;
        vhalPropValues.push_back(propValue);
    }
    return setPropertiesToRead(vhalPropValues);
}

bool VhalHandler::getPropertyValues(std::vector<VehiclePropValue>* property_values) {
    LOG(DEBUG) << __FUNCTION__;
    std::scoped_lock<std::mutex> lock(mAccessLock);

    // Check Vhal service is initialized.
    if (!mIsInitialized) {
        LOG(ERROR) << "VHAL handler not initialized.";
        return false;
    }

    // Copy current property values to argument.
    *property_values = mPropertyValues;

    return true;
}

bool VhalHandler::stopPropertiesUpdate() {
    LOG(DEBUG) << __FUNCTION__;
    {
        std::scoped_lock<std::mutex> lock(mAccessLock);

        // Check Vhal service is initialized.
        if (!mIsInitialized) {
            LOG(ERROR) << "VHAL handler not initialized.";
            return false;
        }

        if (!mIsUpdateActive) {
            LOG(ERROR) << "Polling is already stopped.";
            return false;
        }

        mIsUpdateActive = false;
    }

    // Wake up the polling thread.
    {
        std::scoped_lock<std::mutex> sleepLock(mPollThreadSleepMutex);
        mPollStopSleeping = true;
    }
    mPollThreadCondition.notify_one();

    // Wait for polling thread to exit.
    mPollingThread.join();

    return true;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
