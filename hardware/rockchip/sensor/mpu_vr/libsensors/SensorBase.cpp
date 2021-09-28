/*
* Copyright (C) 2014 Invensense, Inc.
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
#include <stdlib.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <linux/input.h>
#include <utils/SystemClock.h>
#include <cutils/properties.h>

#include "SensorBase.h"

/*****************************************************************************/

// static vars
bool SensorBase::PROCESS_VERBOSE = false;
bool SensorBase::EXTRA_VERBOSE = false;
bool SensorBase::SYSFS_VERBOSE = false;

bool SensorBase::FUNC_ENTRY = false;
bool SensorBase::HANDLER_ENTRY = false;
bool SensorBase::ENG_VERBOSE = false;
bool SensorBase::INPUT_DATA = false;
bool SensorBase::HANDLER_DATA = false;
bool SensorBase::DEBUG_BATCHING = false;
bool SensorBase::DEBUG_DELAY = false;

SensorBase::SensorBase(const char* dev_name,
                       const char* data_name) 
                        : dev_name(dev_name),
                          data_name(data_name),
                          dev_fd(-1),
                          data_fd(-1)
{
    if (data_name) {
        data_fd = openInput(data_name);
    }

    ALOGD("dev_name=%s, data_name=%s, data_fd=%d\n", dev_name, data_name, data_fd);

    char value[PROPERTY_VALUE_MAX];
    property_get("invn.hal.verbose.basic", value, "0");
    if (atoi(value)) {
        PROCESS_VERBOSE = true;
    }
    property_get("invn.hal.verbose.extra", value, "0");
    if (atoi(value)) {
        EXTRA_VERBOSE = true;
    }
    property_get("invn.hal.verbose.sysfs", value, "0");
    if (atoi(value)) {
        SYSFS_VERBOSE = true;
    }
    property_get("invn.hal.verbose.engineering", value, "0");
    if (atoi(value)) {
        ENG_VERBOSE = true;
    }
    property_get("invn.hal.entry.function", value, "0");
    if (atoi(value)) {
        FUNC_ENTRY = true;
    }
    property_get("invn.hal.entry.handler", value, "0");
    if (atoi(value)) {
        HANDLER_ENTRY = true;
    }
    property_get("invn.hal.data.input", value, "0");
    if (atoi(value)) {
        INPUT_DATA = true;
    }
    property_get("invn.hal.data.handler", value, "0");
    if (atoi(value)) {
        HANDLER_DATA = true;
    }
    property_get("invn.hal.debug.batching", value, "0");
    if (atoi(value)) {
        DEBUG_BATCHING = true;
    }
}

SensorBase::~SensorBase()
{
    if (data_fd >= 0) {
        close(data_fd);
    }
    if (dev_fd >= 0) {
        close(dev_fd);
    }
}

int SensorBase::open_device()
{
    if (dev_fd<0 && dev_name) {
        dev_fd = open(dev_name, O_RDONLY);
        LOGE_IF(dev_fd<0, "Couldn't open %s (%s)", dev_name, strerror(errno));
    }
    return 0;
}

int SensorBase::close_device()
{
    if (dev_fd >= 0) {
        close(dev_fd);
        dev_fd = -1;
    }
    return 0;
}

int SensorBase::getFd() const
{
    if (!data_name) {
        return dev_fd;
    }
    return data_fd;
}

int SensorBase::setDelay(int32_t handle, int64_t ns)
{
    return 0;
}

bool SensorBase::hasPendingEvents() const
{
    return false;
}

int64_t SensorBase::getTimestamp()
{
#ifndef SENSOR_MPU_PAD
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return int64_t(t.tv_sec) * 1000000000LL + t.tv_nsec;
#else
    return android::elapsedRealtimeNano();
#endif
}

/*
 * 1: is target
 * 0: is not target
 * other: error
 */
static int is_target_dev(const char* fname, const char *inputName)
{
    int ret = 0;
    int fd = -1;
    char devname[PATH_MAX];
    char name[80];

    sprintf(devname, "/sys/class/input/%s/device/name", fname);
    fd = open(devname, O_RDONLY);

    if (fd >= 0) {
        if (read(fd, name, 79) < 1)
            name[0] = '\0';

        if (!strncmp(name, inputName, strlen(inputName)))
            ret = 1;

        close(fd);
    }

    return ret;
}

int SensorBase::openInput(const char *inputName)
{
    int fd = -1;
    const char *dirname = "/dev/input";
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
                (de->d_name[1] == '\0' ||
                        (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        if (!is_target_dev(de->d_name, inputName))
            continue;
        strcpy(filename, de->d_name);
        fd = open(devname, O_RDONLY);
        LOGV_IF(EXTRA_VERBOSE, "path open %s", devname);
        LOGI("path open %s", devname);
        if (fd >= 0) {
            strcpy(input_name, filename);
            break;
        }
    }
    closedir(dir);
    LOGE_IF(fd < 0, "couldn't find '%s' input device", inputName);

    return fd;
}

int SensorBase::enable(int32_t handle, int enabled)
{
    return 0;
}

int SensorBase::query(int what, int* value)
{
    return 0;
}

int SensorBase::batch(int handle, int flags, int64_t period_ns, int64_t timeout)
{
    return 0;
}
