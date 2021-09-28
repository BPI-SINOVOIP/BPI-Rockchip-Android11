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

#include <cinttypes>
#include <log/log.h>
#include <qemud.h>
#include <utils/SystemClock.h>
#include "multihal_sensors.h"
#include "sensor_list.h"

namespace goldfish {
using ahs21::SensorType;
using ahs10::SensorFlagBits;
using ahs10::MetaDataEventType;

MultihalSensors::MultihalSensors()
        : m_qemuSensorsFd(qemud_channel_open("sensors"))
        , m_batchInfo(getSensorNumber()) {
    if (!m_qemuSensorsFd.ok()) {
        ALOGE("%s:%d: m_qemuSensorsFd is not opened", __func__, __LINE__);
        ::abort();
    }

    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer),
                       "time:%" PRId64, ::android::elapsedRealtimeNano());
    if (qemud_channel_send(m_qemuSensorsFd.get(), buffer, len) < 0) {
        ALOGE("%s:%d: qemud_channel_send failed", __func__, __LINE__);
        ::abort();
    }

    using namespace std::literals;
    const std::string_view kListSensorsCmd = "list-sensors"sv;

    if (qemud_channel_send(m_qemuSensorsFd.get(),
                           kListSensorsCmd.data(),
                           kListSensorsCmd.size()) < 0) {
        ALOGE("%s:%d: qemud_channel_send failed", __func__, __LINE__);
        ::abort();
    }

    len = qemud_channel_recv(m_qemuSensorsFd.get(), buffer, sizeof(buffer) - 1);
    if (len < 0) {
        ALOGE("%s:%d: qemud_channel_recv failed", __func__, __LINE__);
        ::abort();
    }
    buffer[len] = 0;
    uint32_t hostSensorsMask = 0;
    if (sscanf(buffer, "%u", &hostSensorsMask) != 1) {
        ALOGE("%s:%d: Can't parse qemud response", __func__, __LINE__);
        ::abort();
    }
    m_availableSensorsMask =
        hostSensorsMask & ((1u << getSensorNumber()) - 1);

    ALOGI("%s:%d: host sensors mask=%x, available sensors mask=%x",
          __func__, __LINE__, hostSensorsMask, m_availableSensorsMask);

    if (!::android::base::Socketpair(AF_LOCAL, SOCK_STREAM, 0,
                                     &m_callersFd, &m_sensorThreadFd)) {
        ALOGE("%s:%d: Socketpair failed", __func__, __LINE__);
        ::abort();
    }

    m_sensorThread = std::thread(&MultihalSensors::qemuSensorListenerThread, this);
    m_batchThread = std::thread(&MultihalSensors::batchThread, this);
}

MultihalSensors::~MultihalSensors() {
    setAllQemuSensors(false);

    m_batchRunning = false;
    m_batchUpdated.notify_one();
    m_batchThread.join();

    qemuSensorThreadSendCommand(kCMD_QUIT);
    m_sensorThread.join();
}

const std::string MultihalSensors::getName() {
    return "hal_sensors_2_1_impl_ranchu";
}

Return<void> MultihalSensors::debug(const hidl_handle& fd, const hidl_vec<hidl_string>& args) {
    (void)fd;
    (void)args;
    return {};
}

Return<void> MultihalSensors::getSensorsList_2_1(getSensorsList_2_1_cb _hidl_cb) {
    std::vector<SensorInfo> sensors;

    uint32_t mask = m_availableSensorsMask;
    for (int i = 0; mask; ++i, mask >>= 1) {
        if (mask & 1) {
            sensors.push_back(*getSensorInfoByHandle(i));
        }
    }

    _hidl_cb(sensors);
    return {};
}

Return<Result> MultihalSensors::setOperationMode(const OperationMode mode) {
    std::unique_lock<std::mutex> lock(m_mtx);

    if (m_activeSensorsMask) {
        return Result::INVALID_OPERATION;
    } else {
        m_opMode = mode;
        return Result::OK;
    }
}

Return<Result> MultihalSensors::activate(const int32_t sensorHandle,
                                         const bool enabled) {
    if (!isSensorHandleValid(sensorHandle)) {
        return Result::BAD_VALUE;
    }

    std::unique_lock<std::mutex> lock(m_mtx);
    BatchInfo& batchInfo = m_batchInfo[sensorHandle];

    if (enabled) {
        const SensorInfo* sensor = getSensorInfoByHandle(sensorHandle);
        LOG_ALWAYS_FATAL_IF(!sensor);
        if (!(sensor->flags & static_cast<uint32_t>(SensorFlagBits::ON_CHANGE_MODE))) {
            if (batchInfo.samplingPeriodNs <= 0) {
                return Result::BAD_VALUE;
            }

            BatchEventRef batchEventRef;
            batchEventRef.timestamp =
                ::android::elapsedRealtimeNano() + batchInfo.samplingPeriodNs;
            batchEventRef.sensorHandle = sensorHandle;
            batchEventRef.generation = ++batchInfo.generation;

            m_batchQueue.push(batchEventRef);
            m_batchUpdated.notify_one();
        }

        m_activeSensorsMask = m_activeSensorsMask | (1u << sensorHandle);
    } else {
        m_activeSensorsMask = m_activeSensorsMask & ~(1u << sensorHandle);
    }
    return Result::OK;
}

Return<Result> MultihalSensors::batch(const int32_t sensorHandle,
                                      const int64_t samplingPeriodNs,
                                      const int64_t maxReportLatencyNs) {
    (void)maxReportLatencyNs;

    if (!isSensorHandleValid(sensorHandle)) {
        return Result::BAD_VALUE;
    }

    const SensorInfo* sensor = getSensorInfoByHandle(sensorHandle);
    LOG_ALWAYS_FATAL_IF(!sensor);

    if (samplingPeriodNs < sensor->minDelay) {
        return Result::BAD_VALUE;
    }

    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_opMode == OperationMode::NORMAL) {
        m_batchInfo[sensorHandle].samplingPeriodNs = samplingPeriodNs;
    }

    return Result::OK;
}

Return<Result> MultihalSensors::flush(const int32_t sensorHandle) {
    if (!isSensorHandleValid(sensorHandle)) {
        return Result::BAD_VALUE;
    }

    const SensorInfo* sensor = getSensorInfoByHandle(sensorHandle);
    LOG_ALWAYS_FATAL_IF(!sensor);

    std::unique_lock<std::mutex> lock(m_mtx);
    if (!isSensorActive(sensorHandle)) {
        return Result::BAD_VALUE;
    }

    Event event;
    event.sensorHandle = sensorHandle;
    event.sensorType = SensorType::META_DATA;
    event.u.meta.what = MetaDataEventType::META_DATA_FLUSH_COMPLETE;

    doPostSensorEventLocked(*sensor, event);
    return Result::OK;
}

Return<Result> MultihalSensors::injectSensorData_2_1(const Event& event) {
    if (!isSensorHandleValid(event.sensorHandle)) {
        return Result::BAD_VALUE;
    }
    if (event.sensorType == SensorType::ADDITIONAL_INFO) {
        return Result::OK;
    }

    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_opMode != OperationMode::DATA_INJECTION) {
        return Result::INVALID_OPERATION;
    }
    const SensorInfo* sensor = getSensorInfoByHandle(event.sensorHandle);
    LOG_ALWAYS_FATAL_IF(!sensor);
    if (sensor->type != event.sensorType) {
        return Result::BAD_VALUE;
    }

    doPostSensorEventLocked(*sensor, event);
    return Result::OK;
}

Return<Result> MultihalSensors::initialize(const sp<IHalProxyCallback>& halProxyCallback) {
    std::unique_lock<std::mutex> lock(m_mtx);
    setAllQemuSensors(true);   // we need to start sampling sensors for batching
    m_opMode = OperationMode::NORMAL;
    m_halProxyCallback = halProxyCallback;
    return Result::OK;
}

void MultihalSensors::postSensorEvent(const Event& event) {
    const SensorInfo* sensor = getSensorInfoByHandle(event.sensorHandle);
    LOG_ALWAYS_FATAL_IF(!sensor);

    std::unique_lock<std::mutex> lock(m_mtx);
    if (sensor->flags & static_cast<uint32_t>(SensorFlagBits::ON_CHANGE_MODE)) {
        if (isSensorActive(event.sensorHandle)) {
            doPostSensorEventLocked(*sensor, event);
        }
    } else {    // CONTINUOUS_MODE
        m_batchInfo[event.sensorHandle].event = event;
    }
}

void MultihalSensors::doPostSensorEventLocked(const SensorInfo& sensor,
                                              const Event& event) {
    const bool isWakeupEvent =
        sensor.flags & static_cast<uint32_t>(SensorFlagBits::WAKE_UP);

    m_halProxyCallback->postEvents(
        {event},
        m_halProxyCallback->createScopedWakelock(isWakeupEvent));
}

bool MultihalSensors::qemuSensorThreadSendCommand(const char cmd) const {
    return TEMP_FAILURE_RETRY(write(m_callersFd.get(), &cmd, 1)) == 1;
}

bool MultihalSensors::isSensorHandleValid(int sensorHandle) const {
    if (!goldfish::isSensorHandleValid(sensorHandle)) {
        return false;
    }

    if (!(m_availableSensorsMask & (1u << sensorHandle))) {
        return false;
    }

    return true;
}

void MultihalSensors::batchThread() {
    using high_resolution_clock = std::chrono::high_resolution_clock;

    while (m_batchRunning) {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (m_batchQueue.empty()) {
            m_batchUpdated.wait(lock);
        } else {
            const int64_t t = m_batchQueue.top().timestamp;
            const auto d = std::chrono::nanoseconds(t);
            high_resolution_clock::time_point waitUntil(d);
            m_batchUpdated.wait_until(lock, waitUntil);
        }

        const int64_t nowNs = ::android::elapsedRealtimeNano();
        while (!m_batchQueue.empty() && (nowNs >= m_batchQueue.top().timestamp)) {
            BatchEventRef evRef = m_batchQueue.top();
            m_batchQueue.pop();

            const int sensorHandle = evRef.sensorHandle;
            LOG_ALWAYS_FATAL_IF(!goldfish::isSensorHandleValid(sensorHandle));
            if (!isSensorActive(sensorHandle)) {
                continue;
            }

            BatchInfo &batchInfo = m_batchInfo[sensorHandle];
            if (batchInfo.event.sensorType == SensorType::META_DATA) {
                ALOGW("%s:%d the host has not provided value yet for sensorHandle=%d",
                      __func__, __LINE__, sensorHandle);
            } else {
                batchInfo.event.timestamp = evRef.timestamp;
                const SensorInfo* sensor = getSensorInfoByHandle(sensorHandle);
                LOG_ALWAYS_FATAL_IF(!sensor);
                doPostSensorEventLocked(*sensor, batchInfo.event);
            }

            if (evRef.generation == batchInfo.generation) {
                const int64_t samplingPeriodNs = batchInfo.samplingPeriodNs;
                LOG_ALWAYS_FATAL_IF(samplingPeriodNs <= 0);

                evRef.timestamp += samplingPeriodNs;
                m_batchQueue.push(evRef);
            }
        }
    }
}

/// not supported //////////////////////////////////////////////////////////////
Return<void> MultihalSensors::registerDirectChannel(const SharedMemInfo& mem,
                                                    registerDirectChannel_cb _hidl_cb) {
    (void)mem;
    _hidl_cb(Result::INVALID_OPERATION, -1);
    return {};
}

Return<Result> MultihalSensors::unregisterDirectChannel(int32_t channelHandle) {
    (void)channelHandle;
    return Result::INVALID_OPERATION;
}

Return<void> MultihalSensors::configDirectReport(int32_t sensorHandle,
                                                 int32_t channelHandle,
                                                 RateLevel rate,
                                                 configDirectReport_cb _hidl_cb) {
    (void)sensorHandle;
    (void)channelHandle;
    (void)rate;
    _hidl_cb(Result::INVALID_OPERATION, 0 /* reportToken */);
    return {};
}

}  // namespace goldfish
