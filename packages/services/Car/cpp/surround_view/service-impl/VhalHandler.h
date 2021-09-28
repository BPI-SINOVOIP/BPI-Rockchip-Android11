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

#ifndef SURROUND_VIEW_SERVICE_IMPL_VHALHANDLER_H_
#define SURROUND_VIEW_SERVICE_IMPL_VHALHANDLER_H_

#include <mutex>
#include <thread>
#include <vector>

#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>

using android::sp;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

// Vhal handler cache vhal properties needed and updates them at a fixed rate.
class VhalHandler {
public:
    // Enumeration for the method to use for updating the VHAL properties,
    enum UpdateMethod {
        // Makes a periodic get call in a polling thread.
        // Use when VHAL implementation does not support multiple clients in subscribe calls.
        GET = 0,

        // Subscribes to the VHAL properties, to receive values periodically in a callback.
        // Use when VHAL implementation support multiple clients in subscribe calls.
        // NOTE: Currently not implemented.
        SUBSCRIBE
    };

    // Empty vhal handler constructor.
    VhalHandler() : mIsInitialized(false), mUpdateMethod(GET), mRate(0), mIsUpdateActive(false) {}

    // Initializes the VHAL handler.
    // Valid range of rate is [1, 100] Hz.
    // For subscribe the rate must be within each properties min and maximum sampling rate.
    // For get, higher rate may result in excessive binder calls and increased latency.
    bool initialize(UpdateMethod updateMethod, int rate);

    // List of VHAL properties to read, can include vendor specific VHAL properties.
    // The updated method determines if properties are updated using get or subscribe calls.
    bool setPropertiesToRead(const std::vector<vehicle::V2_0::VehiclePropValue>& propertiesToRead);

    // Convenience function to set vhal properties in a format returned from IO Module.
    // uint64_t = (32 bits vhal property id) | (32 bits area id).
    bool setPropertiesToRead(const std::vector<uint64_t>& propertiesToRead);

    // Starts updating the VHAL properties with the specified rate.
    bool startPropertiesUpdate();

    // Gets the last updated VHAL property values.
    // property_values is empty if startPropertiesUpdate() has not been called.
    bool getPropertyValues(std::vector<vehicle::V2_0::VehiclePropValue>* property_values);

    // Stops updating the VHAL properties.
    // For Get method, waits for the polling thread to exit.
    bool stopPropertiesUpdate();

private:
    // Thread function to poll properties.
    void pollProperties();

    // Pointer to VHAL service.
    sp<vehicle::V2_0::IVehicle> mVhalServicePtr;

    // Mutex for locking VHAL properties data.
    std::mutex mAccessLock;

    // Initialized parameters.
    bool mIsInitialized;
    UpdateMethod mUpdateMethod;
    int mRate;
    bool mIsUpdateActive;

    // GET method related data members.
    std::thread mPollingThread;
    std::mutex mPollThreadSleepMutex;
    std::condition_variable mPollThreadCondition;
    bool mPollStopSleeping;

    // List of properties to read.
    std::vector<vehicle::V2_0::VehiclePropValue> mPropertiesToRead;

    // Updated list of property values.
    std::vector<vehicle::V2_0::VehiclePropValue> mPropertyValues;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // SURROUND_VIEW_SERVICE_IMPL_VHALHANDLER_H_
