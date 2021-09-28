/*
 * Copyright (C) 2013-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#define LOG_TAG "V4L2DevBase"

#include "LogHelper.h"
#include "v4l2device.h"
#include "UtilityMacros.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////
//                          PUBLIC METHODS
////////////////////////////////////////////////////////////////////

NAMESPACE_DECLARATION {
V4L2DeviceBase::V4L2DeviceBase(const char *name): mName(name),
                                                  mFd(-1)
{
}

V4L2DeviceBase::~V4L2DeviceBase()
{
    LOGI("@%s", __FUNCTION__);
    if (mFd != -1) {
        LOGW("Destroying a device object not closed, closing first");
        close();
    }
}

status_t V4L2DeviceBase::open()
{
    LOGI("@%s %s", __FUNCTION__, mName.c_str());
    status_t ret = NO_ERROR;
    struct stat st;
    CLEAR(st);
    if (mFd != -1) {
        LOGE("Trying to open a device already open");
        return NO_ERROR; //INVALID_OPERATION;
    }

    if (stat (mName.c_str(), &st) == -1) {
        LOGE("Error stat video device %s: %s",
             mName.c_str(), strerror(errno));
        return UNKNOWN_ERROR;
    }

    if (!S_ISCHR (st.st_mode)) {
        LOGE("%s is not a device", mName.c_str());
        return UNKNOWN_ERROR;
    }

    PERFORMANCE_ATRACE_NAME_SNPRINTF("Open - %s", mName.c_str());
    perfopen(mName.c_str(), O_RDWR, mFd);

    if (mFd < 0) {
        LOGE("Error opening video device %s: %s",
              mName.c_str(), strerror(errno));
        return UNKNOWN_ERROR;
    }

    return ret;
}

status_t V4L2DeviceBase::close()
{
    LOGI("@%s device : %s", __FUNCTION__, mName.c_str());

    if (mFd == -1) {
        LOGW("Device not opened!");
        return INVALID_OPERATION;
    }

    if (perfclose(mName.c_str(), mFd) < 0) {
        LOGE("Close video device failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    mFd = -1;
    return NO_ERROR;
}

int V4L2DeviceBase::xioctl(int request, void *arg, int *errnoCopy) const
{
    int ret(0);
    if (mFd == -1) {
        LOGE("%s invalid device closed!",__FUNCTION__);
        return INVALID_OPERATION;
    }

    do {
        ret = pioctl (mFd, request, arg, mName.c_str());
    } while (-1 == ret && EINTR == errno);

    if (ret < 0) {
        if (errnoCopy != nullptr)
            *errnoCopy = errno;
        LOGW ("%s: Request 0x%x failed: %s", __FUNCTION__, request, strerror(errno));
    }

    return ret;
}

/**
 * Waits for frame data to be available
 *
 * \param timeout time to wait for data (in ms), timeout of -1
 *        means to wait indefinitely for data
 *
 * \return 0: timeout, -1: error happened, positive number: success
 */
int V4L2DeviceBase::poll(int timeout)
{
    struct pollfd pfd = {0,0,0};
    int ret(0);

    if (mFd == -1) {
        LOGW("Device %s already closed. Do nothing.", mName.c_str());
        return -1;
    }

    pfd.fd = mFd;
    pfd.events = POLLPRI | POLLIN | POLLERR;

    ret = perfpoll(&pfd, 1, timeout);

    if (ret < 0) {
        LOGE("poll error ret=%d, mFd=%d, error:%s", ret, mFd, strerror(errno));
        return ret;
    }

    if (pfd.revents & POLLERR) {
        LOGE("%s received POLLERR", __FUNCTION__);
        return -1;
    }

    return ret;
}

int V4L2DeviceBase::subscribeEvent(int event)
{
    LOGI("@%s", __FUNCTION__);
    int ret(0);
    struct v4l2_event_subscription sub;

    if (mFd == -1) {
        LOGW("Device %s already closed. cannot subscribe.", mName.c_str());
        return -1;
    }

    CLEAR(sub);
    sub.type = event;

    ret = pioctl(mFd, VIDIOC_SUBSCRIBE_EVENT, &sub, mName.c_str());
    if (ret < 0) {
        LOGE("error subscribing event %x: %s", event, strerror(errno));
        return ret;
    }

    return ret;
}

int V4L2DeviceBase::unsubscribeEvent(int event)
{
    LOGI("@%s", __FUNCTION__);
    int ret(0);
    struct v4l2_event_subscription sub;

    if (mFd == -1) {
        LOGW("Device %s closed. cannot unsubscribe.", mName.c_str());
        return -1;
    }

    CLEAR(sub);
    sub.type = event;

    ret = pioctl(mFd, VIDIOC_UNSUBSCRIBE_EVENT, &sub, mName.c_str());
    if (ret < 0) {
        LOGE("error unsubscribing event %x :%s",event,strerror(errno));
        return ret;
    }

    return ret;
}

int V4L2DeviceBase::dequeueEvent(struct v4l2_event *event)
{
    LOGD("@%s", __FUNCTION__);
    int ret(0);

    if (mFd == -1) {
        LOGW("Device %s closed. cannot dequeue event.", mName.c_str());
        return -1;
    }

    ret = pioctl(mFd, VIDIOC_DQEVENT, event, mName.c_str());
    if (ret < 0) {
        LOGE("error dequeuing event");
        return ret;
    }

    return ret;
}

status_t V4L2DeviceBase::setControl(int aControlNum, const int value, const char *name)
{
    LOGD("@%s", __FUNCTION__);

    struct v4l2_control control;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control ext_control;

    CLEAR(control);
    CLEAR(controls);
    CLEAR(ext_control);

    LOGD("setting attribute [%s] to %d", name, value);

    if (mFd == -1) {
        LOGE("%s: Invalid device state (CLOSED)", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    control.id = aControlNum;
    control.value = value;
    controls.ctrl_class = V4L2_CTRL_ID2CLASS(control.id);
    controls.count = 1;
    controls.controls = &ext_control;
    ext_control.id = aControlNum;
    ext_control.value = value;

    if (pioctl(mFd, VIDIOC_S_EXT_CTRLS, &controls, mName.c_str()) == 0)
        return NO_ERROR;

    if (pioctl(mFd, VIDIOC_S_CTRL, &control, mName.c_str()) == 0)
        return NO_ERROR;

    LOGE("Failed to set value %d for control %s (%d) on device '%s', %s",
        value, name, aControlNum, mName.c_str(), strerror(errno));

    return UNKNOWN_ERROR;
}

status_t V4L2DeviceBase::getControl(int aControlNum, int *value)
{
    LOGD("@%s", __FUNCTION__);

    struct v4l2_control control;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control ext_control;

    CLEAR(control);
    CLEAR(controls);
    CLEAR(ext_control);

    if (mFd == -1) {
        LOGE("%s: Invalid state device (CLOSED)", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    control.id = aControlNum;
    controls.ctrl_class = V4L2_CTRL_ID2CLASS(control.id);
    controls.count = 1;
    controls.controls = &ext_control;
    ext_control.id = aControlNum;

    if (pioctl(mFd, VIDIOC_G_EXT_CTRLS, &controls, mName.c_str()) == 0) {
       *value = ext_control.value;
       return NO_ERROR;
    }

    if (pioctl(mFd, VIDIOC_G_CTRL, &control, mName.c_str()) == 0) {
       *value = control.value;
       return NO_ERROR;
    }

    LOGE("Failed to get value for control (%d) on device '%s', %s",
            aControlNum, mName.c_str(), strerror(errno));
    return UNKNOWN_ERROR;
}

status_t V4L2DeviceBase::queryMenu(v4l2_querymenu &menu)
{
    LOGD("@%s", __FUNCTION__);

    if (mFd == -1) {
        LOGE("%s: Invalid state device (CLOSED)", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    if (pioctl(mFd, VIDIOC_QUERYMENU, &menu, mName.c_str()) == 0) {
       return NO_ERROR;
    }

    LOGE("Failed to get values for query menu (%d) on device '%s', %s",
            menu.id, mName.c_str(), strerror(errno));
    return UNKNOWN_ERROR;
}

status_t V4L2DeviceBase::queryControl(v4l2_queryctrl &control)
{
    LOGD("@%s", __FUNCTION__);

    if (mFd == -1) {
        LOGE("%s: Invalid state device (CLOSED)", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    if (pioctl(mFd, VIDIOC_QUERYCTRL, &control, mName.c_str()) == 0) {
       return NO_ERROR;
    }
    LOGE("Failed to get values for query control (%d) on device '%s', %s",
            control.id, mName.c_str(), strerror(errno));
    return UNKNOWN_ERROR;
}


/**
 * \brief Poll V4L2 devices
 *
 * \param devices         [IN]  devices to poll()
 * \param activeDevices   [OUT] devices that had data available
 * \param inactiveDevices [OUT] devices that did not have data available
 * \param timeOut         [IN]  poll timeout
 * \param flushFd         [IN]  file descriptor of the pipe device that will be used
 *                              to return from poll() in case of flush request,
 *                              i.e., to abort poll before timeout
 */
int V4L2DeviceBase::pollDevices(const std::vector<std::shared_ptr<V4L2DeviceBase>> &devices,
                                std::vector<std::shared_ptr<V4L2DeviceBase>> &activeDevices,
                                std::vector<std::shared_ptr<V4L2DeviceBase>> &inactiveDevices,
                                int timeOut, int flushFd, int events)
{
    LOGD("@%s", __FUNCTION__);
    int numFds = devices.size();
    int totalNumFds = (flushFd != -1) ? numFds + 1 : numFds; //adding one more fd if flushfd given.
    struct pollfd pollFds[totalNumFds];
    int ret = 0;

    for (int i = 0; i < numFds; i++) {
        pollFds[i].fd = devices[i]->mFd;
        pollFds[i].events = events | POLLERR; // we always poll for errors, asked or not
        pollFds[i].revents = 0;
    }

    if (flushFd != -1) {
        pollFds[numFds].fd = flushFd;
        pollFds[numFds].events = POLLPRI | POLLIN;
        pollFds[numFds].revents = 0;
    }

    ret = perfpoll(pollFds, totalNumFds, timeOut);
    if (ret <= 0) {
        for (uint32_t i = 0; i < devices.size(); i++) {
            LOGE("Device %s poll failed (%s)", devices[i]->name(),
                                              (ret == 0) ? "timeout" : "error");
            if (pollFds[i].revents & POLLERR) {
                LOGE("%s: device %s received POLLERR", __FUNCTION__, devices[i]->name());
            }

        }
        return ret;
    }

    activeDevices.clear();
    inactiveDevices.clear();

    //check first the flush
    if (flushFd != -1) {
        if ((pollFds[numFds].revents & POLLIN) || (pollFds[numFds].revents & POLLPRI)) {
            LOGI("%s: Poll returning from flush", __FUNCTION__);
            return ret;
        }
    }

    // check other active devices.
    for (int i = 0; i < numFds; i++) {
        if (pollFds[i].revents & POLLERR) {
            LOGE("%s: received POLLERR", __FUNCTION__);
            return -1;
        }
        // return nodes that have data available
        if (pollFds[i].revents & events) {
            activeDevices.push_back(devices[i]);
        } else
            inactiveDevices.push_back(devices[i]);
    }
    return ret;
}

int V4L2DeviceBase::frmsizeWidth(struct v4l2_frmsizeenum &size)
{
    return size.type == V4L2_FRMSIZE_TYPE_DISCRETE ?
           size.discrete.width :
           size.stepwise.max_width;
}

int V4L2DeviceBase::frmsizeHeight(struct v4l2_frmsizeenum &size)
{
    return size.type == V4L2_FRMSIZE_TYPE_DISCRETE ?
           size.discrete.height :
           size.stepwise.max_height;
}

void V4L2DeviceBase::frmivalIval(struct v4l2_frmivalenum &frmival, struct v4l2_fract &ival)
{
    if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
        ival = frmival.discrete;
    } else {
        ival = frmival.stepwise.min;
    }
}

int V4L2DeviceBase::cmpFract(struct v4l2_fract &f1, struct v4l2_fract &f2)
{
    return f1.numerator * f2.denominator - f2.numerator * f1.denominator;
}

int V4L2DeviceBase::cmpIval(struct v4l2_frmivalenum &i1, struct v4l2_frmivalenum &i2)
{
    struct v4l2_fract f1, f2;
    frmivalIval(i1, f1);
    frmivalIval(i2, f2);
    return cmpFract(f1, f2);
}
} NAMESPACE_DECLARATION_END

////////////////////////////////////////////////////////////////////
//                          PRIVATE METHODS
////////////////////////////////////////////////////////////////////

