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

#include <log/log.h>
#include <utils/SystemClock.h>
#include <math.h>
#include <qemud.h>
#include "multihal_sensors.h"
#include "sensor_list.h"

namespace goldfish {
using ahs10::EventPayload;
using ahs21::SensorType;
using ahs10::SensorStatus;

namespace {
const char* testPrefix(const char* i, const char* end, const char* v, const char sep) {
    while (i < end) {
        if (*v == 0) {
            return (*i == sep) ? (i + 1) : nullptr;
        } else if (*v == *i) {
            ++v;
            ++i;
        } else {
            return nullptr;
        }
    }

    return nullptr;
}

bool approximatelyEqual(double a, double b, double eps) {
    return fabs(a - b) <= std::max(fabs(a), fabs(b)) * eps;
}

int64_t weigthedAverage(const int64_t a, int64_t aw, int64_t b, int64_t bw) {
    return (a * aw + b * bw) / (aw + bw);
}

}  // namespace

bool MultihalSensors::activateQemuSensorImpl(const int pipe,
                                             const int sensorHandle,
                                             const bool enabled) {
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer),
                       "set:%s:%d",
                       getQemuSensorNameByHandle(sensorHandle),
                       (enabled ? 1 : 0));

    if (qemud_channel_send(pipe, buffer, len) < 0) {
        ALOGE("%s:%d: qemud_channel_send failed", __func__, __LINE__);
        return false;
    } else {
        return true;
    }
}

bool MultihalSensors::setAllQemuSensors(const bool enabled) {
    uint32_t mask = m_availableSensorsMask;
    for (int i = 0; mask; ++i, mask >>= 1) {
        if (mask & 1) {
            if (!activateQemuSensorImpl(m_qemuSensorsFd.get(), i, enabled)) {
                return false;
            }
        }
    }

    return true;
}

void MultihalSensors::parseQemuSensorEvent(const int pipe,
                                           QemuSensorsProtocolState* state) {
    char buf[256];
    const int len = qemud_channel_recv(pipe, buf, sizeof(buf) - 1);
    if (len < 0) {
        ALOGE("%s:%d: qemud_channel_recv failed", __func__, __LINE__);
    }
    const int64_t nowNs = ::android::elapsedRealtimeNano();
    buf[len] = 0;
    const char* end = buf + len;

    bool parsed = false;
    Event event;
    EventPayload* payload = &event.u;
    ahs10::Vec3* vec3 = &payload->vec3;
    ahs10::Uncal* uncal = &payload->uncal;

    if (const char* values = testPrefix(buf, end, "acceleration", ':')) {
        if (sscanf(values, "%f:%f:%f",
                   &vec3->x, &vec3->y, &vec3->z) == 3) {
            vec3->status = SensorStatus::ACCURACY_MEDIUM;
            event.timestamp = nowNs + state->timeBiasNs;
            event.sensorHandle = kSensorHandleAccelerometer;
            event.sensorType = SensorType::ACCELEROMETER;
            postSensorEvent(event);
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "gyroscope", ':')) {
        if (sscanf(values, "%f:%f:%f",
                   &vec3->x, &vec3->y, &vec3->z) == 3) {
            vec3->status = SensorStatus::ACCURACY_MEDIUM;
            event.timestamp = nowNs + state->timeBiasNs;
            event.sensorHandle = kSensorHandleGyroscope;
            event.sensorType = SensorType::GYROSCOPE;
            postSensorEvent(event);
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "gyroscope-uncalibrated", ':')) {
        if (sscanf(values, "%f:%f:%f",
                   &uncal->x, &uncal->y, &uncal->z) == 3) {
            uncal->x_bias = 0.0;
            uncal->y_bias = 0.0;
            uncal->z_bias = 0.0;
            event.timestamp = nowNs + state->timeBiasNs;
            event.sensorHandle = kSensorHandleGyroscopeFieldUncalibrated;
            event.sensorType = SensorType::GYROSCOPE_UNCALIBRATED;
            postSensorEvent(event);
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "orientation", ':')) {
        if (sscanf(values, "%f:%f:%f",
                   &vec3->x, &vec3->y, &vec3->z) == 3) {
            vec3->status = SensorStatus::ACCURACY_HIGH;
            event.timestamp = nowNs + state->timeBiasNs;
            event.sensorHandle = kSensorHandleOrientation;
            event.sensorType = SensorType::ORIENTATION;
            postSensorEvent(event);
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "magnetic", ':')) {
        if (sscanf(values, "%f:%f:%f",
                   &vec3->x, &vec3->y, &vec3->z) == 3) {
            vec3->status = SensorStatus::ACCURACY_HIGH;
            event.timestamp = nowNs + state->timeBiasNs;
            event.sensorHandle = kSensorHandleMagneticField;
            event.sensorType = SensorType::MAGNETIC_FIELD;
            postSensorEvent(event);
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "magnetic-uncalibrated", ':')) {
        if (sscanf(values, "%f:%f:%f",
                   &uncal->x, &uncal->y, &uncal->z) == 3) {
            uncal->x_bias = 0.0;
            uncal->y_bias = 0.0;
            uncal->z_bias = 0.0;
            event.timestamp = nowNs + state->timeBiasNs;
            event.sensorHandle = kSensorHandleMagneticFieldUncalibrated;
            event.sensorType = SensorType::MAGNETIC_FIELD_UNCALIBRATED;
            postSensorEvent(event);
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "temperature", ':')) {
        if (sscanf(values, "%f", &payload->scalar) == 1) {
            if (!approximatelyEqual(state->lastAmbientTemperatureValue,
                                    payload->scalar, 0.001)) {
                event.timestamp = nowNs + state->timeBiasNs;
                event.sensorHandle = kSensorHandleAmbientTemperature;
                event.sensorType = SensorType::AMBIENT_TEMPERATURE;
                postSensorEvent(event);
                state->lastAmbientTemperatureValue = payload->scalar;
            }
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "proximity", ':')) {
        if (sscanf(values, "%f", &payload->scalar) == 1) {
            if (!approximatelyEqual(state->lastProximityValue,
                                    payload->scalar, 0.001)) {
                event.timestamp = nowNs + state->timeBiasNs;
                event.sensorHandle = kSensorHandleProximity;
                event.sensorType = SensorType::PROXIMITY;
                postSensorEvent(event);
                state->lastProximityValue = payload->scalar;
            }
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "light", ':')) {
        if (sscanf(values, "%f", &payload->scalar) == 1) {
            if (!approximatelyEqual(state->lastLightValue,
                                    payload->scalar, 0.001)) {
                event.timestamp = nowNs + state->timeBiasNs;
                event.sensorHandle = kSensorHandleLight;
                event.sensorType = SensorType::LIGHT;
                postSensorEvent(event);
                state->lastLightValue = payload->scalar;
            }
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "pressure", ':')) {
        if (sscanf(values, "%f", &payload->scalar) == 1) {
            event.timestamp = nowNs + state->timeBiasNs;
            event.sensorHandle = kSensorHandlePressure;
            event.sensorType = SensorType::PRESSURE;
            postSensorEvent(event);
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "humidity", ':')) {
        if (sscanf(values, "%f", &payload->scalar) == 1) {
            if (!approximatelyEqual(state->lastRelativeHumidityValue,
                                    payload->scalar, 0.001)) {
                event.timestamp = nowNs + state->timeBiasNs;
                event.sensorHandle = kSensorHandleRelativeHumidity;
                event.sensorType = SensorType::RELATIVE_HUMIDITY;
                postSensorEvent(event);
                state->lastRelativeHumidityValue = payload->scalar;
            }
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "hinge-angle0", ':')) {
        if (sscanf(values, "%f", &payload->scalar) == 1) {
            if (!approximatelyEqual(state->lastHingeAngle0Value,
                                    payload->scalar, 0.001)) {
                event.timestamp = nowNs + state->timeBiasNs;
                event.sensorHandle = kSensorHandleHingeAngle0;
                event.sensorType = SensorType::HINGE_ANGLE;
                postSensorEvent(event);
                state->lastHingeAngle0Value = payload->scalar;
            }
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "hinge-angle1", ':')) {
        if (sscanf(values, "%f", &payload->scalar) == 1) {
            if (!approximatelyEqual(state->lastHingeAngle1Value,
                                    payload->scalar, 0.001)) {
                event.timestamp = nowNs + state->timeBiasNs;
                event.sensorHandle = kSensorHandleHingeAngle1;
                event.sensorType = SensorType::HINGE_ANGLE;
                postSensorEvent(event);
                state->lastHingeAngle1Value = payload->scalar;
            }
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "hinge-angle2", ':')) {
        if (sscanf(values, "%f", &payload->scalar) == 1) {
            if (!approximatelyEqual(state->lastHingeAngle2Value,
                                    payload->scalar, 0.001)) {
                event.timestamp = nowNs + state->timeBiasNs;
                event.sensorHandle = kSensorHandleHingeAngle2;
                event.sensorType = SensorType::HINGE_ANGLE;
                postSensorEvent(event);
                state->lastHingeAngle2Value = payload->scalar;
            }
            parsed = true;
        }
     } else if (const char* values = testPrefix(buf, end, "guest-sync", ':')) {
        long long value;
        if ((sscanf(values, "%lld", &value) == 1) && (value >= 0)) {
            const int64_t guestTimeNs = static_cast<int64_t>(value * 1000ll);
            const int64_t timeBiasNs = guestTimeNs - nowNs;
            state->timeBiasNs =
                std::min(int64_t(0),
                         weigthedAverage(state->timeBiasNs, 3, timeBiasNs, 1));
            parsed = true;
        }
    } else if (const char* values = testPrefix(buf, end, "sync", ':')) {
        parsed = true;
    }

    if (!parsed) {
        ALOGW("%s:%d: don't know how to parse '%s'", __func__, __LINE__, buf);
    }
}

}  // namespace
