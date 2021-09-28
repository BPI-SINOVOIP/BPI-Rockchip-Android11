/*
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

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>

#include <poll.h>
#include <pthread.h>
#include <stdlib.h>

#include <linux/input.h>

#include <cutils/atomic.h>
#include <math.h>

#include "nusensors.h"
#include "LightSensor.h"
#include "ProximitySensor.h"
#include "MmaSensor.h"
#include "AkmSensor.h"
#include "GyroSensor.h"
#include "PressureSensor.h"
#include "TemperatureSensor.h"

/*****************************************************************************/

struct sensors_poll_context_t {
    sensors_poll_device_1_t device; // must be first

        sensors_poll_context_t();
        ~sensors_poll_context_t();
    int activate(int handle, int enabled);
    int setDelay(int handle, int64_t ns);
    int pollEvents(sensors_event_t* data, int count);
    int batch(int handle, int flags, int64_t period_ns, int64_t timeout);
    int flush(int handle);
    bool getInitialized() { return mInitialized; };

private:
    bool mInitialized;

    enum {
        light           = 0,
        proximity       = 1,
        mma             = 2,
        akm             = 3,
        gyro            = 4,
        pressure        = 5,
        temperature		= 6,
        numSensorDrivers,
        numFds,
    };

    static const size_t flushPipe = numFds - 1;
    struct pollfd mPollFds[numFds];
    int mFlushWritePipeFd;
    SensorBase* mSensors[numSensorDrivers];

    int handleToDriver(int handle) const {
        switch (handle) {
            case ID_A:
                return mma;
            case ID_M:
                return akm;	
            case ID_P:
                return proximity;
            case ID_L:
                return light;	
            case ID_GY:
                return gyro;
            case ID_PR:
                return pressure;
            case ID_TMP:
                return temperature;
        }
        return -EINVAL;
    }
};

/*****************************************************************************/

sensors_poll_context_t::sensors_poll_context_t()
{
    mInitialized = false;
    /* Must clean this up early or else the destructor will make a mess */
    memset(mSensors, 0, sizeof(mSensors));

    mSensors[light] = new LightSensor();
    mPollFds[light].fd = mSensors[light]->getFd();
    mPollFds[light].events = POLLIN;
    mPollFds[light].revents = 0;

    mSensors[proximity] = new ProximitySensor();
    mPollFds[proximity].fd = mSensors[proximity]->getFd();
    mPollFds[proximity].events = POLLIN;
    mPollFds[proximity].revents = 0;

    mSensors[mma] = new MmaSensor();
    mPollFds[mma].fd = mSensors[mma]->getFd();
    mPollFds[mma].events = POLLIN;
    mPollFds[mma].revents = 0;

    mSensors[akm] = new AkmSensor();
    mPollFds[akm].fd = mSensors[akm]->getFd();
    mPollFds[akm].events = POLLIN;
    mPollFds[akm].revents = 0;

    mSensors[gyro] = new GyroSensor();
    mPollFds[gyro].fd = mSensors[gyro]->getFd();
    mPollFds[gyro].events = POLLIN;
    mPollFds[gyro].revents = 0;

    mSensors[pressure] = new PressureSensor();
    mPollFds[pressure].fd = mSensors[pressure]->getFd();
    mPollFds[pressure].events = POLLIN;
    mPollFds[pressure].revents = 0;

    mSensors[temperature] = new TemperatureSensor();
    mPollFds[temperature].fd = mSensors[temperature]->getFd();
    mPollFds[temperature].events = POLLIN;
    mPollFds[temperature].revents = 0;

    int flushFds[2];
    int result = pipe(flushFds);
    LOGE_IF(result<0, "error creating flush pipe (%s)", strerror(errno));
    result = fcntl(flushFds[0], F_SETFL, O_NONBLOCK);
    LOGE_IF(result<0, "error setting flushFds[0] access mode (%s)", strerror(errno));
    result = fcntl(flushFds[1], F_SETFL, O_NONBLOCK);
    LOGE_IF(result<0, "error setting flushFds[1] access mode (%s)", strerror(errno));
    mFlushWritePipeFd = flushFds[1];

    mPollFds[flushPipe].fd = flushFds[0];
    mPollFds[flushPipe].events = POLLIN;
    mPollFds[flushPipe].revents = 0;

    mInitialized = true;
}

sensors_poll_context_t::~sensors_poll_context_t() {
    for (int i=0 ; i<numSensorDrivers ; i++) {
        delete mSensors[i];
    }
    close(mPollFds[flushPipe].fd);
    close(mFlushWritePipeFd);
    mInitialized = false;
}

int sensors_poll_context_t::activate(int handle, int enabled) {
    if (!mInitialized) return -EINVAL;
    int index = handleToDriver(handle);
    if (index < 0) return index;
    return mSensors[index]->enable(handle, enabled);
}

int sensors_poll_context_t::setDelay(int handle, int64_t ns) {

    int index = handleToDriver(handle);
    if (index < 0) return index;
    return mSensors[index]->setDelay(handle, ns);
}

int sensors_poll_context_t::flush(int handle)
{
    int result;
    sensors_event_t flush_event_data;

    int index = handleToDriver(handle);
    if (index < 0) return index;

    result = mSensors[index]->isActivated(handle);
    if (!result)
        return -EINVAL;

    flush_event_data.sensor = 0;
    flush_event_data.timestamp = 0;
    flush_event_data.meta_data.sensor = handle;
    flush_event_data.meta_data.what = META_DATA_FLUSH_COMPLETE;
    flush_event_data.type = SENSOR_TYPE_META_DATA;
    flush_event_data.version = META_DATA_VERSION;

    result = write(mFlushWritePipeFd, &flush_event_data, sizeof(sensors_event_t));
    ALOGE_IF(result<0, "error sending flush event data (%s)", strerror(errno));

    return (result >= 0 ? 0 : result);
}

static int64_t tm_min=0;
static int64_t tm_max=0;
static int64_t tm_sum=0;
static int64_t tm_last_print=0;
static int64_t tm_count=0;

static int debug_time = 0;
static int debug_lvl = 0;

#define NSEC_PER_SEC            1000000000
#include <cutils/properties.h>

static inline int64_t timespec_to_ns(const struct timespec *ts)
{
	return ((int64_t) ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

static int64_t get_time_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return timespec_to_ns(&ts);
}

int sensors_poll_context_t::pollEvents(sensors_event_t* data, int count)
{
    int nbEvents = 0;
    int nb, polltime = -1;

    // look for new events
    nb = poll(mPollFds, numFds, polltime);

    /* flush event data */
    if ((count > 0) && (nb > 0)) {
        if (mPollFds[flushPipe].revents & POLLIN) {
            int n = read(mPollFds[flushPipe].fd, data, count * sizeof(sensors_event_t));
            if (n < 0) {
                LOGE("error reading from flush pipe (%s)", strerror(errno));
                return 0;
            }
            nb = n / sizeof(sensors_event_t);
            mPollFds[flushPipe].revents = 0;
            count -= nb;
            nbEvents += nb;
            data += nb;
            LOGI("report %d flush event\n", nbEvents);
            return nbEvents;
        }
    }

    if (nb > 0) {
        for (int i=0 ; count && i < numSensorDrivers ; i++) {
            SensorBase* const sensor(mSensors[i]);
            if (mPollFds[i].revents & POLLIN) {
                nb = 0;
                nb = sensor->readEvents(data, count);
                mPollFds[i].revents = 0;

                //LOGI("count = %d, nbEvents = %d, nb = %d\n", count, nbEvents, nb);
                if (nb > 0) {
                    if (debug_time) {
                        int64_t tm_cur = get_time_ns();
                        int64_t tm_delta = tm_cur - data->timestamp;
                        if (tm_min==0 && tm_max==0)
                            tm_min = tm_max = tm_delta;
                        else if (tm_delta < tm_min)
                            tm_min = tm_delta;
                        else if (tm_delta > tm_max)
                            tm_max = tm_delta;
                        tm_sum += tm_delta;
                        tm_count++;
                        //LOGI("tm_count = %d\n", tm_count);
                        if ((tm_cur-tm_last_print) > 1000000000) {
                            LOGD("ST HAL report rate[%4lld]: %8lld, %8lld, %8lld\n", (long long)tm_count, (long long)tm_min, (long long)(tm_sum/tm_count), (long long)tm_max);
                            tm_last_print = tm_cur;
                            tm_min = tm_max = tm_count = tm_sum = 0;
                        }
                    }
    
                    if (debug_lvl > 0) {
                        for (int j=0; j<nb; j++) {
                            if ((debug_lvl&1) && data[j].sensor==ID_GY) {
                                LOGD("GYRO: %+f %+f %+f - %lld", data[j].gyro.x, data[j].gyro.y, data[j].gyro.z, (long long)data[j].timestamp);
                            }
                            if ((debug_lvl&2) && data[j].sensor==ID_A) {
                                LOGD("ACCL: %+f %+f %+f - %lld", data[j].acceleration.x, data[j].acceleration.y, data[j].acceleration.z, (long long)data[j].timestamp);
                            }
                            if ((debug_lvl&4) && (data[j].sensor==ID_M)) {
                                LOGD("MAG: %+f %+f %+f - %lld", data[j].magnetic.x, data[j].magnetic.y, data[j].magnetic.z, (long long)data[j].timestamp);
                            }
                        }
                    }
    
                    count -= nb;
                    nbEvents += nb;
                    data += nb;
                }
            }
        }
    }

    return nbEvents;
}

/*****************************************************************************/

static int poll__close(struct hw_device_t *dev)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    if (ctx) {
        delete ctx;
    }
    return 0;
}

static int poll__activate(struct sensors_poll_device_t *dev,
        int handle, int enabled) {
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;

    LOGI("set active: handle = %d, enable = %d\n", handle, enabled);

    char propbuf[PROPERTY_VALUE_MAX];
    property_get("vendor.sensor.debug.level", propbuf, "0");
    debug_lvl = atoi(propbuf);

    memset(propbuf, 0, sizeof(propbuf));
    property_get("vendor.sensor.debug.time", propbuf, "0");
    debug_time = atoi(propbuf);

    return ctx->activate(handle, enabled);
}

static int poll__setDelay(struct sensors_poll_device_t *dev,
        int handle, int64_t ns) {
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;

    LOGI("set delay: handle = %d, delay = %dns\n", handle, (int)ns);

    return ctx->setDelay(handle, ns);
}

static int poll__poll(struct sensors_poll_device_t *dev,
        sensors_event_t* data, int count) {
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->pollEvents(data, count);
}

static int poll__batch(struct sensors_poll_device_1 *dev,
                      int handle, int flags, int64_t period_ns, int64_t timeout)
{
    LOGI("set batch: handle = %d, period_ns = %dns, timeout = %dns\n", handle, (int)period_ns, (int)timeout);

    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->setDelay(handle, period_ns);
}

static int poll__flush(struct sensors_poll_device_1 *dev,
                      int handle)
{
    LOGI("set flush: handle = %d\n", handle);
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->flush(handle);
}

/*****************************************************************************/

int init_nusensors(hw_module_t const* module, hw_device_t** device)
{
	LOGD("%s\n",SENSOR_VERSION_AND_TIME);
    int status = -EINVAL;

    sensors_poll_context_t *dev = new sensors_poll_context_t();
    if (!dev->getInitialized()) {
        LOGE("Failed to open the sensors");
        return status;
    }
    memset(&dev->device, 0, sizeof(sensors_poll_device_1));

    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.version  = SENSORS_DEVICE_API_VERSION_1_3;
    dev->device.common.module   = const_cast<hw_module_t*>(module);
    dev->device.common.close    = poll__close;
    dev->device.activate        = poll__activate;
    dev->device.setDelay        = poll__setDelay;
    dev->device.poll            = poll__poll;

    /* Batch processing */
    dev->device.batch           = poll__batch;
    dev->device.flush           = poll__flush;

    *device = &dev->device.common;

    status = 0;
    return status;
}
