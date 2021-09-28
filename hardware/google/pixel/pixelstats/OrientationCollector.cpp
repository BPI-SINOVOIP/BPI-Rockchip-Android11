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

#define LOG_TAG "pixelstats-orientation"

#include <android/sensor.h>
#include <log/log.h>
#include <pixelstats/OrientationCollector.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

#define GET_EVENT_TIMEOUT_MILLIS 200
#define SENSOR_TYPE_DEVICE_ORIENTATION 27
#define ORIENTATION_UNKNOWN -1

sp<OrientationCollector> OrientationCollector::createOrientationCollector() {
    sp<OrientationCollector> orientationCollector;
    orientationCollector = new OrientationCollector();
    if (orientationCollector != nullptr && orientationCollector->init() < 0) {
        orientationCollector->disableOrientationSensor();
        return nullptr;
    }
    return orientationCollector;
}

/* pollOrientation
 * orientation: [out] orientation value from sensor Hal.
 * Return: OK, or error code from sensor Hal
 */
int32_t OrientationCollector::pollOrientation(int *orientation) {
    int err = OK;
    int eventCount = 0;
    ASensorEvent sensorEvent;

    *orientation = ORIENTATION_UNKNOWN;
    /* Get sensor events. */
    /* Get sensor sample events. */
    err = getEvents(&sensorEvent, 1, &eventCount);
    if (eventCount > 0) {
        ALOGV("%s: ##event data: %f,%f,%f", __func__, sensorEvent.data[0], sensorEvent.data[1],
              sensorEvent.data[2]);
        *orientation = sensorEvent.data[0];
    }
    return err;
}

/*
 * Collects sensor samples and returns them in the sensor sample event list
 * specified by event_list. Limits the number of returned sample events to the
 * value specified by event_list_size. Returns the number of returned sample
 * events in p_event_count.
 *
 * event_list               List of collected sample events.
 * event_list_size          Size of sample event list.
 * event_count              Returned count of the number of collected sample
 *                          events.
 */
int OrientationCollector::getEvents(ASensorEvent *event_list, size_t event_list_size,
                                    int *event_count) {
    int rv;
    int err = OK;

    /* Wait for a sensor event to be available */
    /* The timeout is used to prevent blocking for long time, when sensor pool is
     * empty, for example the device is put on a horizontal wireless charger.
     */
    rv = ALooper_pollOnce(GET_EVENT_TIMEOUT_MILLIS, nullptr, nullptr, nullptr);
    if (rv == ALOOPER_POLL_ERROR) {
        ALOGI("Sensor event looper returned a poll error.\n");
        err = UNKNOWN_ERROR;
        goto out;
    }

    /* Get sensor events. */
    *event_count = ASensorEventQueue_getEvents(mQueue, event_list, event_list_size);

out:
    return err;
}

int32_t OrientationCollector::init() {
    int err = OK;
    ALooper *looper = nullptr;

    // Get orientation sensor events from the NDK
    mSensorManager = ASensorManager_getInstanceForPackage(nullptr);
    if (mSensorManager == nullptr) {
        ALOGE("%s: Unable to get sensorManager.\n", __func__);
        return UNKNOWN_ERROR;
    }
    looper = ALooper_forThread();
    if (looper == nullptr) {
        looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    }
    if (looper == nullptr) {
        ALOGE("%s: Failed to prepare an event looper.\n", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    mQueue = ASensorManager_createEventQueue(mSensorManager, looper, 0, nullptr, nullptr);
    mOrientationSensor =
            ASensorManager_getDefaultSensor(mSensorManager, SENSOR_TYPE_DEVICE_ORIENTATION);
    if (mOrientationSensor == nullptr) {
        ALOGE("%s: Unable to get orientation sensor.\n", __func__);
        return UNKNOWN_ERROR;
    }
    err = ASensorEventQueue_registerSensor(mQueue, mOrientationSensor,
                                           ASensor_getMinDelay(mOrientationSensor), 0);
    if (err < 0) {
        ALOGE("%s: Unable to register for orientation sensor events\n", __func__);
    }
    return err;
}

void OrientationCollector::disableOrientationSensor() {
    if (mSensorManager != nullptr && mQueue != nullptr) {
        ASensorEventQueue_disableSensor(mQueue, mOrientationSensor);
        ASensorManager_destroyEventQueue(mSensorManager, mQueue);
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
