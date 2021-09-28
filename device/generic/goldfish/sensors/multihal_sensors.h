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
#include <V2_1/SubHal.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <queue>
#include <thread>
#include <vector>

namespace goldfish {
namespace ahs = ::android::hardware::sensors;
namespace ahs21 = ahs::V2_1;
namespace ahs10 = ahs::V1_0;

using ahs21::implementation::IHalProxyCallback;
using ahs21::SensorInfo;
using ahs21::Event;
using ahs10::OperationMode;
using ahs10::RateLevel;
using ahs10::Result;
using ahs10::SharedMemInfo;

using ::android::base::unique_fd;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::sp;

struct MultihalSensors : public ahs21::implementation::ISensorsSubHal {
    MultihalSensors();
    ~MultihalSensors();

    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& args) override;
    Return<void> getSensorsList_2_1(getSensorsList_2_1_cb _hidl_cb) override;
    Return<Result> setOperationMode(OperationMode mode) override;
    Return<Result> activate(int32_t sensorHandle, bool enabled) override;
    Return<Result> batch(int32_t sensorHandle,
                           int64_t samplingPeriodNs,
                           int64_t maxReportLatencyNs) override;
    Return<Result> flush(int32_t sensorHandle) override;
    Return<Result> injectSensorData_2_1(const Event& event) override;


    Return<void> registerDirectChannel(const SharedMemInfo& mem,
                                       registerDirectChannel_cb _hidl_cb) override;
    Return<Result> unregisterDirectChannel(int32_t channelHandle) override;
    Return<void> configDirectReport(int32_t sensorHandle,
                                    int32_t channelHandle,
                                    RateLevel rate,
                                    configDirectReport_cb _hidl_cb) override;

    const std::string getName() override;
    Return<Result> initialize(const sp<IHalProxyCallback>& halProxyCallback) override;

private:
    struct QemuSensorsProtocolState {
        int64_t timeBiasNs = -500000000;

        static constexpr float kSensorNoValue = -1e+30;

        // on change sensors (host does not support them)
        float lastAmbientTemperatureValue = kSensorNoValue;
        float lastProximityValue = kSensorNoValue;
        float lastLightValue = kSensorNoValue;
        float lastRelativeHumidityValue = kSensorNoValue;
        float lastHingeAngle0Value = kSensorNoValue;
        float lastHingeAngle1Value = kSensorNoValue;
        float lastHingeAngle2Value = kSensorNoValue;
    };

    bool isSensorHandleValid(int sensorHandle) const;
    bool isSensorActive(int sensorHandle) const {
        return m_activeSensorsMask & (1u << sensorHandle);  // m_mtx required
    }
    static bool activateQemuSensorImpl(int pipe, int sensorHandle, bool enabled);
    bool setAllQemuSensors(bool enabled);
    void parseQemuSensorEvent(const int pipe, QemuSensorsProtocolState* state);
    void postSensorEvent(const Event& event);
    void doPostSensorEventLocked(const SensorInfo& sensor, const Event& event);

    void qemuSensorListenerThread();
    void batchThread();

    static constexpr char kCMD_QUIT = 'q';
    bool qemuSensorThreadSendCommand(char cmd) const;

    // set in ctor, never change
    const unique_fd     m_qemuSensorsFd;
    uint32_t            m_availableSensorsMask = 0;
    // a pair of connected sockets to talk to the worker thread
    unique_fd           m_callersFd;        // a caller writes here
    unique_fd           m_sensorThreadFd;   // the worker thread listens from here
    std::thread         m_sensorThread;

    // changed by API
    uint32_t                m_activeSensorsMask = 0;
    OperationMode           m_opMode = OperationMode::NORMAL;
    sp<IHalProxyCallback>   m_halProxyCallback;

    // batching
    struct BatchEventRef {
        int64_t  timestamp = -1;
        int      sensorHandle = -1;
        int      generation = 0;

        bool operator<(const BatchEventRef &rhs) const {
            // not a typo, we want m_batchQueue.top() to be the smallest timestamp
            return timestamp > rhs.timestamp;
        }
    };

    struct BatchInfo {
        Event       event;
        int64_t     samplingPeriodNs = 0;
        int         generation = 0;
    };

    std::priority_queue<BatchEventRef>      m_batchQueue;
    std::vector<BatchInfo>                  m_batchInfo;
    std::condition_variable                 m_batchUpdated;
    std::thread                             m_batchThread;
    std::atomic<bool>                       m_batchRunning = true;

    mutable std::mutex                      m_mtx;
};

}  // namespace goldfish
