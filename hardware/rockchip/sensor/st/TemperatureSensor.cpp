/*
 * Copyright (C) 2010 Motorola, Inc.
 * Copyright (C) 2008 The Android Open Source Project
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

#include "TemperatureSensor.h"

/*****************************************************************************/

TemperatureSensor::TemperatureSensor()
    : SensorBase(TMP_DEVICE_NAME, "temperature"),
      mEnabled(0),
      mInputReader(32),
      mHasPendingEvent(false)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_TMP;
    mPendingEvent.type = SENSOR_TYPE_TEMPERATURE;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

     open_device();

    int flags = 0;
    if ((dev_fd > 0) && (!ioctl(dev_fd, TEMPERATURE_IOCTL_GET_ENABLED, &flags))) {
        if (flags) {
            mEnabled = 1;
            setInitialState();
        }
    }
}

TemperatureSensor::~TemperatureSensor() {
    if (dev_fd > 0) {
        close(dev_fd);
        dev_fd = -1;
    }
}

int TemperatureSensor::setInitialState() {
    struct input_absinfo absinfo;
    if ((data_fd > 0) && !ioctl(data_fd, EVIOCGABS(EVENT_TYPE_TEMPERATURE), &absinfo)) {
        mHasPendingEvent = true;
        mPendingEvent.temperature = CONVERT_B * absinfo.value;
    }
    return 0;
}

int TemperatureSensor::enable(int32_t, int en) {
    int flags = en ? 1 : 0;
    int err = 0;
    if (flags != mEnabled) {
        if (dev_fd < 0) {
            open_device();
        }
        err = ioctl(dev_fd, TEMPERATURE_IOCTL_ENABLE, &flags);
        err = err<0 ? -errno : 0;
        LOGE_IF(err, "TEMPERATURE_IOCTL_ENABLE failed (%s)", strerror(-err));
        if (!err) {
            mEnabled = en ? 1 : 0;
            if (en) {
                setInitialState();
            }
        }
    }
    return err;
}

bool TemperatureSensor::hasPendingEvents() const {
    return mHasPendingEvent;
}


int TemperatureSensor::setDelay(int32_t handle, int64_t ns)
{
    if (ns < 0)
        return -EINVAL;
	
    if (dev_fd < 0) {
        open_device();
    }

    int delay = ns / 1000000;
    if (ioctl(dev_fd, TEMPERATURE_IOCTL_SET_DELAY, &delay)) {
        return -errno;
    }
    return 0;
}

int TemperatureSensor::isActivated(int /* handle */)
{
    return mEnabled;
}

int TemperatureSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    if (mHasPendingEvent) {
        mHasPendingEvent = false;
        mPendingEvent.timestamp = getTimestamp();
        *data = mPendingEvent;
        return mEnabled ? 1 : 0;
    }

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;
    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_ABS) {
            processEvent(event->code, event->value);
        } else if (type == EV_SYN) {
            int64_t time = timevalToNano(event->time);
            mPendingEvent.timestamp = time;
            if (mEnabled) {
                *data++ = mPendingEvent;
                count--;
                numEventReceived++;
            }
        } else {
            ALOGE("TemperatureSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }

    return numEventReceived;
}

void TemperatureSensor::processEvent(int code, int value)
{
    if (code == EVENT_TYPE_TEMPERATURE) {
            mPendingEvent.pressure = value * CONVERT_B ;
			//LOGD("%s:value=%d\n",__FUNCTION__, value);
    }
}
