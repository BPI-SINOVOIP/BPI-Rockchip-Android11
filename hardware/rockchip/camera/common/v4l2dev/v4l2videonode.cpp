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

#define LOG_TAG "V4L2VideoNode"

#include "LogHelper.h"

#include "v4l2device.h"
#include <linux/media.h>
#include "Camera3V4l2Format.h"
#include <limits.h>
#include <fcntl.h>
#include "UtilityMacros.h"
#include "PlatformData.h"
#include "CameraMetadataHelper.h"
#if defined(ANDROID_VERSION_ABOVE_12_X)
#include <hardware/hardware_rockchip.h>
#endif


////////////////////////////////////////////////////////////////////
//                          PUBLIC METHODS
////////////////////////////////////////////////////////////////////

#define MAX_CAMERA_BUFFERS_NUM  32  //MAX_CAMERA_BUFFERS_NUM

NAMESPACE_DECLARATION {

V4L2Buffer::V4L2Buffer()
{
    CLEAR(vbuf);
}

V4L2Buffer::V4L2Buffer(const struct v4l2_buffer &buf)
{
    memset(&vbuf, 0, sizeof(vbuf));
}

void V4L2Buffer::setType(uint32_t type)
{
    CheckError(!V4L2_TYPE_IS_VALID(type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, type);

    vbuf.type = type;
    if (V4L2_TYPE_IS_MULTIPLANAR(vbuf.type)) {
        /* Init fields required by multi-planar buffers */
        setNumPlanes(1);
    }
}

uint32_t V4L2Buffer::offset(int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);

    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), 0,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    return mp ? vbuf.m.planes[plane].m.mem_offset : vbuf.m.offset;
}

void V4L2Buffer::setOffset(uint32_t offset, int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);

    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), VOID_VALUE,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    if (mp)
        vbuf.m.planes[plane].m.mem_offset = offset;
    else
        vbuf.m.offset = offset;
}

unsigned long V4L2Buffer::userptr(int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);

    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), 0,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    return mp ? vbuf.m.planes[plane].m.userptr : vbuf.m.userptr;
}

void V4L2Buffer::setUserptr(unsigned long userptr, int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);
    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), VOID_VALUE,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    if (mp)
        vbuf.m.planes[plane].m.userptr = userptr;
    else
        vbuf.m.userptr = userptr;
}

int V4L2Buffer::fd(int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);

    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), -1,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    return mp ? vbuf.m.planes[plane].m.fd : vbuf.m.fd;
}

void V4L2Buffer::setFd(int fd, int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);
    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), VOID_VALUE,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    if (mp)
        vbuf.m.planes[plane].m.fd = fd;
    else
        vbuf.m.fd = fd;
}

uint32_t V4L2Buffer::bytesused(int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);

    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), 0,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    return mp ? vbuf.m.planes[plane].bytesused : vbuf.bytesused;
}

void V4L2Buffer::setBytesused(uint32_t bytesused, int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);
    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), VOID_VALUE,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    if (mp)
        vbuf.m.planes[plane].bytesused = bytesused;
    else
        vbuf.bytesused = bytesused;
}

uint32_t V4L2Buffer::length(int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);

    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), 0,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    return mp ? vbuf.m.planes[plane].length : vbuf.length;
}

void V4L2Buffer::setLength(uint32_t length, int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);
    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vbuf.type);
    CheckError(((!mp && plane) || (mp && plane >= planes.size())), VOID_VALUE,
               "@%s: invalid plane %d", __FUNCTION__, plane);

    if (mp)
        vbuf.m.planes[plane].length = length;
    else
        vbuf.length = length;
}

uint32_t V4L2Buffer::getNumPlanes()
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);

    if (V4L2_TYPE_IS_MULTIPLANAR(vbuf.type))
        return planes.size();
    else
        return 1;
}

void V4L2Buffer::setNumPlanes(int numPlanes)
{
    CheckError(!V4L2_TYPE_IS_VALID(vbuf.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vbuf.type);

    CheckError(!V4L2_TYPE_IS_MULTIPLANAR(vbuf.type), VOID_VALUE,
               "@%s: setting plane number for single plane buffer is not allowed", __FUNCTION__);

    if (numPlanes != planes.size()) {
        planes.clear();
        for (int i = 0; i < numPlanes; i++) {
            struct v4l2_plane plane;
            CLEAR(plane);
            planes.push_back(plane);
        }
    }
    vbuf.m.planes = planes.data();
    vbuf.length = numPlanes;
}

const V4L2Buffer &V4L2Buffer::operator=(const V4L2Buffer &buf)
{
    vbuf = buf.vbuf;
    if (V4L2_TYPE_IS_MULTIPLANAR(vbuf.type)) {
        planes = buf.planes;
        vbuf.m.planes = planes.data();
    }
    return *this;
}

void V4L2Format::setType(uint32_t type)
{
    CheckError(!V4L2_TYPE_IS_VALID(type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, type);

    vfmt.type = type;
    if (V4L2_TYPE_IS_MULTIPLANAR(vfmt.type))
        vfmt.fmt.pix_mp.num_planes = 1;
}

uint32_t V4L2Format::width()
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type))
        return vfmt.fmt.meta.buffersize;
    else
        return V4L2_TYPE_IS_MULTIPLANAR(vfmt.type) ? vfmt.fmt.pix_mp.width : vfmt.fmt.pix.width;
}

void V4L2Format::setWidth(uint32_t width)
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type)) {
        LOGE("@%s: setting width for meta format is not allowed.", __FUNCTION__);
    }
    else if (V4L2_TYPE_IS_MULTIPLANAR(vfmt.type))
        vfmt.fmt.pix_mp.width = width;
    else
        vfmt.fmt.pix.width = width;
}

uint32_t V4L2Format::height()
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type))
        return 1;
    else
        return V4L2_TYPE_IS_MULTIPLANAR(vfmt.type) ? vfmt.fmt.pix_mp.height : vfmt.fmt.pix.height;
}

void V4L2Format::setHeight(uint32_t height)
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type)) {
        LOGE("@%s: setting height for meta format is not allowed.", __FUNCTION__);
    }
    else if (V4L2_TYPE_IS_MULTIPLANAR(vfmt.type))
        vfmt.fmt.pix_mp.height = height;
    else
        vfmt.fmt.pix.height = height;
}

uint32_t V4L2Format::pixelformat()
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type))
        return vfmt.fmt.meta.dataformat;
    else
        return V4L2_TYPE_IS_MULTIPLANAR(vfmt.type) ?
            vfmt.fmt.pix_mp.pixelformat : vfmt.fmt.pix.pixelformat;
}

void V4L2Format::setPixelformat(uint32_t format)
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type))
        vfmt.fmt.meta.dataformat = format;
    else if (V4L2_TYPE_IS_MULTIPLANAR(vfmt.type))
        vfmt.fmt.pix_mp.pixelformat = format;
    else
        vfmt.fmt.pix.pixelformat = format;
}

uint32_t V4L2Format::field()
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type))
        return V4L2_FIELD_NONE;
    else
        return V4L2_TYPE_IS_MULTIPLANAR(vfmt.type) ? vfmt.fmt.pix_mp.field : vfmt.fmt.pix.field;
}

void V4L2Format::setField(uint32_t field)
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type)) {
        LOGE("@%s: setting field for meta format is not allowed.", __FUNCTION__);
    }
    else if (V4L2_TYPE_IS_MULTIPLANAR(vfmt.type))
        vfmt.fmt.pix_mp.field = field;
    else
        vfmt.fmt.pix.field = field;
}

uint32_t V4L2Format::bytesperline(int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type))
        return vfmt.fmt.meta.buffersize;

    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vfmt.type);

    if ((!mp && plane) ||
        (mp && plane >= vfmt.fmt.pix_mp.num_planes)) {
        LOGE("@%s: invalid plane %d", __FUNCTION__, plane);
        plane = 0;
    }

    return mp ? vfmt.fmt.pix_mp.plane_fmt[plane].bytesperline : vfmt.fmt.pix.bytesperline;
}

void V4L2Format::setBytesperline(uint32_t bytesperline, int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type)) {
        LOGE("@%s: setting bytesperline for meta format is not allowed.", __FUNCTION__);
    }
    else if (V4L2_TYPE_IS_MULTIPLANAR(vfmt.type))
        vfmt.fmt.pix_mp.plane_fmt[plane].bytesperline = bytesperline;
    else
        vfmt.fmt.pix.bytesperline = bytesperline;
}

uint32_t V4L2Format::sizeimage(int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), BAD_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type))
        return vfmt.fmt.meta.buffersize;

    bool mp = V4L2_TYPE_IS_MULTIPLANAR(vfmt.type);

    if ((!mp && plane) ||
        (mp && plane && plane >= vfmt.fmt.pix_mp.num_planes)) {
        LOGE("@%s: invalid plane %d", __FUNCTION__, plane);
        plane = 0;
    }

    return mp ? vfmt.fmt.pix_mp.plane_fmt[plane].sizeimage : vfmt.fmt.pix.sizeimage;
}

void V4L2Format::setSizeimage(uint32_t size, int plane)
{
    CheckError(!V4L2_TYPE_IS_VALID(vfmt.type), VOID_VALUE, \
               "@%s: invalid buffer type: %d.", __FUNCTION__, vfmt.type);

    if (V4L2_TYPE_IS_META(vfmt.type))
        vfmt.fmt.meta.buffersize = size;
    else if (V4L2_TYPE_IS_MULTIPLANAR(vfmt.type))
        vfmt.fmt.pix_mp.plane_fmt[plane].sizeimage = size;
    else
        vfmt.fmt.pix.sizeimage = size;
}

const V4L2Format &V4L2Format::operator=(const V4L2Format &fmt)
{
    vfmt = fmt.vfmt;
    return *this;
}

V4L2BufferInfo::V4L2BufferInfo():
    data(NULL),
    length(0),
    width(0),
    height(0),
    format(0),
    cache_flags(0)
{
}

V4L2VideoNode::V4L2VideoNode(const char *name):
                             V4L2DeviceBase(name),
                             mState(DEVICE_CLOSED),
                             mBuffersInDevice(0),
                             mFrameCounter(0),
                             mInitialSkips(0),
                             mBufType(V4L2_BUF_TYPE_VIDEO_CAPTURE),
                             mMemoryType(V4L2_MEMORY_USERPTR)
{
    LOGI("%s: @%s", mName.c_str(), __FUNCTION__);
    mBufferPool.reserve(MAX_CAMERA_BUFFERS_NUM);
    mSetBufferPool.reserve(MAX_CAMERA_BUFFERS_NUM);
    CLEAR(mConfig);
}

V4L2VideoNode::~V4L2VideoNode()
{
    LOGI("%s: @%s", mName.c_str(), __FUNCTION__);

    /**
     * Do something for the buffer pool ?
     */
}

status_t V4L2VideoNode::open()
{
    struct v4l2_capability cap;
    status_t status(NO_ERROR);

    status = V4L2DeviceBase::open();
    CheckError((status != NO_ERROR), status, "@%s: failed to open video device node", __FUNCTION__);
    mState = DEVICE_OPEN;
    status = queryCap(&cap);
    CheckError((status != NO_ERROR), status, "@%s: query device caps failed", __FUNCTION__);
    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        mBufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        mBufType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    else if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)
        mBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    else if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
        mBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    else if (cap.capabilities & V4L2_CAP_META_CAPTURE)
        mBufType = V4L2_BUF_TYPE_META_CAPTURE;
    else if (cap.capabilities & V4L2_CAP_META_OUTPUT)
        mBufType = V4L2_BUF_TYPE_META_OUTPUT;
    else {
        LOGE("@%s: unsupported buffer type.", __FUNCTION__);
        return DEAD_OBJECT;
    }

    mBuffersInDevice = 0;
    return status;
}

status_t V4L2VideoNode::close()
{
    status_t status(NO_ERROR);

    if (mState == DEVICE_STARTED) {
        stop();
    }
    if (!mBufferPool.empty()) {
        destroyBufferPool();
    }

    status = V4L2DeviceBase::close();
    if (status == NO_ERROR)
        mState = DEVICE_CLOSED;

    mBuffersInDevice = 0;
    return status;
}

status_t V4L2VideoNode::setBlock(bool block)
{
    int ret, flags;

    flags = fcntl(mFd, F_GETFL, 0);
    if (flags < 0)
        return UNKNOWN_ERROR;

    if (block)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;

    ret = fcntl(mFd, F_SETFL, flags);
    if (ret < 0)
        return UNKNOWN_ERROR;

    return NO_ERROR;
}

/**
 * queries the capabilities of the device and it does some basic sanity checks
 * based on the direction of the video device node
 *
 * \param cap: [OUT] V4L2 capability structure
 *
 * \return NO_ERROR  if everything went ok
 * \return INVALID_OPERATION if the device was not in correct state
 * \return UNKNOWN_ERROR if IOCTL operation failed
 * \return DEAD_OBJECT if the basic checks for this object failed
 */
status_t V4L2VideoNode::queryCap(struct v4l2_capability *cap)
{
    int ret(0);

    if (mState != DEVICE_OPEN) {
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    PERFORMANCE_ATRACE_NAME("VIDIOC_QUERYCAP");
    ret = pioctl(mFd, VIDIOC_QUERYCAP, cap, mName.c_str());

    if (ret < 0) {
        LOGE("VIDIOC_QUERYCAP returned: %d (%s)", ret, strerror(errno));
        return UNKNOWN_ERROR;
    }

    LOGI("%s: driver:       '%s'", mName.c_str(), cap->driver);
    LOGI("%s: card:         '%s'", mName.c_str(), cap->card);
    LOGI("%s: bus_info:     '%s'", mName.c_str(), cap->bus_info);
    LOGI("%s: version:      %x", mName.c_str(), cap->version);
    LOGI("%s: capabilities: %x", mName.c_str(), cap->capabilities);
    LOGI("%s: device caps:  %x", mName.c_str(), cap->device_caps);
    LOGI("%s: buffer type   %s", mName.c_str(), ENUM2STR(v4l2_buf_type_enum,mBufType));

    return NO_ERROR;
}

status_t V4L2VideoNode::enumerateInputs(struct v4l2_input *anInput)
{
    int ret(0);

    if (mState == DEVICE_CLOSED) {
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    PERFORMANCE_ATRACE_NAME("VIDIOC_ENUMINPUT");
    ret = pioctl(mFd, VIDIOC_ENUMINPUT, anInput, mName.c_str());
    int errno_copy = errno;

    if (ret < 0) {
        LOGE("VIDIOC_ENUMINPUT failed returned: %d (%s)", ret, strerror(errno_copy));
        if (errno_copy == EINVAL)
            return BAD_INDEX;
        else
            return UNKNOWN_ERROR;
    }
    LOGI("%s: VIDIOC_ENUMINPUT", mName.c_str());

    return NO_ERROR;
}

status_t V4L2VideoNode::queryCapturePixelFormats(std::vector<v4l2_fmtdesc> &formats)
{
    struct v4l2_fmtdesc aFormat;

    if (mState == DEVICE_CLOSED) {
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    formats.clear();
    CLEAR(aFormat);

    aFormat.index = 0;
    aFormat.type = mBufType;

    PERFORMANCE_ATRACE_NAME("VIDIOC_ENUM_FMT");
    while (pioctl(mFd, VIDIOC_ENUM_FMT , &aFormat, mName.c_str()) == 0) {
        formats.push_back(aFormat);
        aFormat.index++;
    };

    aFormat.index = 0;
    aFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    while (pioctl(mFd, VIDIOC_ENUM_FMT , &aFormat, mName.c_str()) == 0) {
        formats.push_back(aFormat);
        aFormat.index++;
    };

    LOGI("%s: VIDIOC_ENUM_FMT, %zu format retrieved", mName.c_str(), formats.size());
    return NO_ERROR;
}

int V4L2VideoNode::getMemoryType()
{
    return mMemoryType;
}

status_t V4L2VideoNode::setInput(int index)
{
    struct v4l2_input input;
    status_t status = NO_ERROR;
    int ret(0);

    if (mState == DEVICE_CLOSED) {
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    input.index = index;

    PERFORMANCE_ATRACE_NAME("VIDIOC_S_INPUT");
    ret = pioctl(mFd, VIDIOC_S_INPUT, &input, mName.c_str());

    if (ret < 0) {
       LOGE("VIDIOC_S_INPUT index %d returned: %d (%s)",
           input.index, ret, strerror(errno));
       status = UNKNOWN_ERROR;
    }
    LOGI("%s: VIDIOC_S_INPUT, input index:%d", mName.c_str(), input.index);

    return status;
}

/**
 * Stop the streaming of buffers of a video device
 * This method is basically a stream-off IOCTL but it has a parameter to
 * stop and destroy the current active-buffer-pool
 *
 * After this method completes the device is in state DEVICE_PREPARED
 *
 * \param leavePopulated: boolean to control the state change
 * \return   0 on success
 *          -1 on failure
 */
int V4L2VideoNode::stop(bool keepBuffers)
{
    int ret = 0;

    if (mState == DEVICE_STARTED) {
        //stream off
        PERFORMANCE_ATRACE_NAME("VIDIOC_STREAMOFF");
        ret = pioctl(mFd, VIDIOC_STREAMOFF, &mBufType, mName.c_str());
        if (ret < 0) {
            LOGE("VIDIOC_STREAMOFF returned: %d (%s)", ret, strerror(errno));
            return ret;
        }
        mState = DEVICE_PREPARED;
    }
    LOGI("%s: VIDIOC_STREAMOFF: BufType:%s", mName.c_str(), ENUM2STR(v4l2_buf_type_enum,mBufType));

    if (mState == DEVICE_PREPARED) {
        if (!keepBuffers) {
            destroyBufferPool();
            mState = DEVICE_CONFIGURED;
        }
   } else {
        LOGW("Trying to stop a device not started");
        ret = -1;
    }

    return ret;
}

/**
 * Start the streaming of buffers in a video device
 *
 * This called is allowed in the following states:
 * - PREPARED
 *
 * This method just calls  call the stream on IOCTL
 */
int V4L2VideoNode::start(int initialSkips)
{
    int ret(0);

    if (mState != DEVICE_PREPARED) {
        LOGE("%s: Invalid state to start %d", __FUNCTION__, mState);
        return -1;
    }

    //stream on
    PERFORMANCE_ATRACE_NAME("VIDIOC_STREAMON");
    ret = pioctl(mFd, VIDIOC_STREAMON, &mBufType, mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_STREAMON returned: %d (%s)", ret, strerror(errno));
        return ret;
    }
    LOGI("%s: VIDIOC_STREAMON: BufType:%s", mName.c_str(), ENUM2STR(v4l2_buf_type_enum,mBufType));

    mFrameCounter = 0;
    mState = DEVICE_STARTED;
    mInitialSkips = initialSkips;

    return ret;
}

/**
 * Update the current device node configuration
 *
 * This called is allowed in the following states:
 * - OPEN
 * - CONFIGURED
 * - PREPARED
 *
 * This method is a convenience method for use in the context of video capture
 * (INPUT_VIDEO_NODE)
 * It makes use of the more detailed method that uses as input parameter the
 * v4l2_format structure
 * This method queries first the current format and updates capture format.
 *
 *
 * \param aConfig:[IN/OUT] reference to the new configuration.
 *                 This structure contains new values for width,height and format
 *                 parameters, but the stride value is not known by the caller
 *                 of this method. The stride value is retrieved from the ISP
 *                 and the value updated, so aConfig.stride is an OUTPUT parameter
 *                 The same applies for the expected size of the buffer
 *                 aConfig.size is also an OUTPUT parameter
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOW_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::setFormat(FrameInfo &aConfig)
{
    int ret(0);
    V4L2Format v4l2_fmt;

    if ((mState != DEVICE_OPEN) &&
        (mState != DEVICE_CONFIGURED) &&
        (mState != DEVICE_PREPARED) ){
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    PERFORMANCE_ATRACE_NAME("VIDIOC_G_FMT");
    v4l2_fmt.setType(mBufType);
    ret = pioctl (mFd, VIDIOC_G_FMT, v4l2_fmt.get(), mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_G_FMT failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    if (V4L2_TYPE_IS_META(mBufType)) {
        v4l2_fmt.setPixelformat(aConfig.format);
        v4l2_fmt.setSizeimage(0);

        ret = setMetaFormat(v4l2_fmt);
        CheckError(ret != NO_ERROR, ret, "@%s set meta format failed", __FUNCTION__);
        // update config info
        aConfig.size = mConfig.size;
    } else {
        v4l2_fmt.setType(mBufType);
        v4l2_fmt.setWidth(aConfig.width);
        v4l2_fmt.setHeight(aConfig.height);
        v4l2_fmt.setPixelformat(aConfig.format);
        v4l2_fmt.setBytesperline((unsigned int)pixelsToBytes(aConfig.format, aConfig.stride));
        v4l2_fmt.setSizeimage(0);
        v4l2_fmt.setField(aConfig.field);

        // Update current configuration with the new one
        ret = setPixFormat(v4l2_fmt);
        CheckError(ret != NO_ERROR, ret, "@%s set pixel format failed", __FUNCTION__);
        // update config info
        aConfig.stride = mConfig.stride;
        aConfig.width = mConfig.width;
        aConfig.height = mConfig.height;
        aConfig.field = mConfig.field;
        aConfig.size = mConfig.size;
    }

    return NO_ERROR;
}

/**
 * Update the current device node configuration (low-level)
 *
 * This called is allowed in the following states:
 * - OPEN
 * - CONFIGURED
 * - PREPARED
 *
 * This methods allows more detailed control of the format than the previous one
 * It updates the internal configuration used to check for discrepancies between
 * configuration and buffer pool properties
 *
 * \param aFormat:[IN] reference to the new v4l2 format .
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOW_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::setPixFormat(V4L2Format &aFormat)
{
    int ret(0);

    if ((mState != DEVICE_OPEN) &&
        (mState != DEVICE_CONFIGURED) &&
        (mState != DEVICE_PREPARED) ){
        LOGE("%s invalid device state %d", __FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    PERFORMANCE_ATRACE_NAME("VIDIOC_S_FMT");
    aFormat.setType(mBufType);
    LOGI("%s: VIDIOC_S_FMT: %s width: %d, height: %d, bpl: %d, fourcc: %s, field: %d", mName.c_str(),
         mName.c_str(),
         aFormat.width(),
         aFormat.height(),
         aFormat.bytesperline(),
         v4l2Fmt2Str(aFormat.pixelformat()),
         aFormat.field());

    ret = pioctl(mFd, VIDIOC_S_FMT, aFormat.get(), mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_S_FMT failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    // Update current configuration with the new one
    mConfig.format = aFormat.pixelformat();
    mConfig.width = aFormat.width();
    mConfig.height = aFormat.height();
    mConfig.stride = bytesToPixels(mConfig.format, aFormat.bytesperline());
    mConfig.size = frameSize(mConfig.format, mConfig.stride, mConfig.height);

    if (mConfig.stride != mConfig.width)
        LOGI("%s: stride: %d from ISP width: %d", mName.c_str(), mConfig.stride,mConfig.width);

    mState = DEVICE_CONFIGURED;
    mSetBufferPool.clear();
    return NO_ERROR;
}

status_t V4L2VideoNode::setMetaFormat(V4L2Format &aFormat)
{
    LOGI("%s: @%s", mName.c_str(), __FUNCTION__);
    int ret(0);

    if ((mState != DEVICE_OPEN) &&
        (mState != DEVICE_CONFIGURED) &&
        (mState != DEVICE_PREPARED) ){
        LOGE("%s invalid device state %d", __FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    aFormat.setType(mBufType);
    LOGI("%s: VIDIOC_S_FMT: fourcc: %s, size: %d",
        mName.c_str(),
        v4l2Fmt2Str(aFormat.pixelformat()),
        aFormat.sizeimage());

    PERFORMANCE_ATRACE_NAME("VIDIOC_S_FMT");
    ret = pioctl(mFd, VIDIOC_S_FMT, aFormat.get(), mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_S_FMT failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    // Update current configuration with the new one
    mConfig.format = aFormat.pixelformat();
    mConfig.size = aFormat.sizeimage();

    mState = DEVICE_CONFIGURED;
    mSetBufferPool.clear();
    return NO_ERROR;
}

status_t V4L2VideoNode::setSelection(const struct v4l2_selection &aSelection)
{
    struct v4l2_selection *sel = const_cast<struct v4l2_selection *>(&aSelection);
    int ret = 0;

    if ((mState != DEVICE_OPEN) &&
        (mState != DEVICE_CONFIGURED)){
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    PERFORMANCE_ATRACE_NAME("VIDIOC_S_SELECTION");
    sel->type = mBufType;
    LOGI("%s: VIDIOC_S_SELECTION, type: %u, target: 0x%x, flags: 0x%x, rect left: %d, rect top: %d, width: %d, height: %d",
        mName.c_str(),
        aSelection.type,
        aSelection.target,
        aSelection.flags,
        aSelection.r.left,
        aSelection.r.top,
        aSelection.r.width,
        aSelection.r.height);

    ret = pbxioctl(VIDIOC_S_SELECTION, sel);
    if (ret < 0) {
        LOGE("VIDIOC_S_SELECTION failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}


int V4L2VideoNode::grabFrame(V4L2BufferInfo *buf)
{
    int ret(0);

    CheckError((mState != DEVICE_STARTED), -1, "@%s %s invalid device state %d",
        __FUNCTION__, mName.c_str(), mState);
    CheckError((buf == nullptr), -1, "@%s %s invalid parameter buf is nullptr",
        __FUNCTION__, mName.c_str());

    ret = dqbuf(buf);

    if (ret < 0)
        return ret;

    // inc frame counter but do no wrap to negative numbers
    mFrameCounter++;
    mFrameCounter &= INT_MAX;

    /* printBufferInfo(__FUNCTION__, buf->vbuffer); */
    return buf->vbuffer.index();
}

/*
 * In some cases like in timeout situation there is no need to add buffer to
 * traced buffers list because it is already there.
 */
status_t V4L2VideoNode::putFrame(const V4L2Buffer &buf)
{
    unsigned int index = buf.index();

    CheckError((index >= mBufferPool.size()), BAD_INDEX, "@%s %s Invalid index %d pool size %zu",
        __FUNCTION__, mName.c_str(), index, mBufferPool.size());

    mBufferPool.at(index).vbuffer = buf;
    if (putFrame(index) < 0)
        return UNKNOWN_ERROR;

    return NO_ERROR;
}

/*
 * In some cases like in timeout situation there is no need to add buffer to
 * traced buffers list because it is already there.
 */
int V4L2VideoNode::putFrame(unsigned int index)
{
    int ret(0);

    CheckError((index >= mBufferPool.size()), BAD_INDEX, "@%s %s Invalid index %d pool size %zu",
        __FUNCTION__, mName.c_str(), index, mBufferPool.size());
    V4L2BufferInfo vbuf = mBufferPool.at(index);
    ret = qbuf(&vbuf);
    /* printBufferInfo(__FUNCTION__, vbuf.vbuffer); */

    return ret;
}

int V4L2VideoNode::exportFrame(unsigned int index)
{
    int ret(0);

    if (mMemoryType != V4L2_MEMORY_MMAP) {
        LOGE("@%s %s Cannot export non-mmap buffers", __FUNCTION__, mName.c_str());
        return BAD_VALUE;
    }

    if (index >= mBufferPool.size()) {
        LOGE("@%s %s Invalid index %d pool size %zu",
            __FUNCTION__, mName.c_str(), index, mBufferPool.size());
        return BAD_INDEX;
    }

    PERFORMANCE_ATRACE_NAME("VIDIOC_EXPBUF");
    V4L2BufferInfo vbuf = mBufferPool.at(index);
    struct v4l2_exportbuffer ebuf;
    CLEAR(ebuf);
    ebuf.type = vbuf.vbuffer.type();
    ebuf.index = index;
    ret = pioctl(mFd, VIDIOC_EXPBUF, &ebuf, mName.c_str());
    if (ret < 0) {
        LOGE("@%s %s VIDIOC_EXPBUF failed ret %d : %s",
            __FUNCTION__, mName.c_str(), ret, strerror(errno));
        return ret;
    }
    LOGI("%s: @%s, idx %d fd %d", mName.c_str(), __FUNCTION__, index, ebuf.fd);
    return ebuf.fd;
}

status_t V4L2VideoNode::setParameter (struct v4l2_streamparm *aParam)
{
    LOGI("%s: @%s", mName.c_str(), __FUNCTION__);
    status_t ret = NO_ERROR;

    if (mState == DEVICE_CLOSED)
        return INVALID_OPERATION;

    PERFORMANCE_ATRACE_NAME("VIDIOC_S_PARM");
    ret = pioctl(mFd, VIDIOC_S_PARM, aParam, mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_S_PARM failed ret %d : %s", ret, strerror(errno));
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

/**
 * Get the maximum rectangle for cropping.
 *
 * This call is allowed in all other states except in DEVICE_CLOSED.
 *
 * \param crop:[OUT] pointer to the maximum crop rectangle.
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOW_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::getMaxCropRectangle(struct v4l2_rect *crop)
{
    LOGI("%s: @%s", mName.c_str(), __FUNCTION__);
    int ret;
    struct v4l2_cropcap cropcap;

    if (!crop)
        return UNKNOWN_ERROR;

    if (mState == DEVICE_CLOSED)
        return INVALID_OPERATION;

    PERFORMANCE_ATRACE_NAME("VIDIOC_CROPCAP");
    CLEAR(cropcap);
    cropcap.type = mBufType;
    ret = pioctl(mFd, VIDIOC_CROPCAP, &cropcap, mName.c_str());
    if (ret != NO_ERROR)
        return ret;

    *crop = cropcap.defrect;
    return NO_ERROR;
}

/**
 * Update the current device node cropping settings.
 *
 * This call is allowed in all other states except in DEVICE_CLOSED.
 *
 * It makes use of the more detailed method that uses as input parameter the
 * v4l2_format structure.
 *
 *
 * \param crop:[IN] pointer to the new cropping settings.
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOW_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::setCropRectangle(struct v4l2_rect *crop)
{
    LOGI("%s: @%s", mName.c_str(), __FUNCTION__);
    int ret;
    struct v4l2_crop v4l2_crop;
    CLEAR(v4l2_crop);

    if (!crop)
        return UNKNOWN_ERROR;

    if (mState == DEVICE_CLOSED)
        return INVALID_OPERATION;

    PERFORMANCE_ATRACE_NAME("VIDIOC_S_CROP");
    v4l2_crop.type     = mBufType;
    v4l2_crop.c.left   = crop->left;
    v4l2_crop.c.top    = crop->top;
    v4l2_crop.c.width  = crop->width;
    v4l2_crop.c.height = crop->height;

    // Update current configuration with the new one
    ret = pioctl(mFd, VIDIOC_S_CROP, &v4l2_crop, mName.c_str());
    if (ret != NO_ERROR)
        return ret;

    return NO_ERROR;
}

/**
 * Get the current device node cropping settings.
 *
 * This call is allowed in all other states except in DEVICE_CLOSED.
 *
 * \param[out] crop pointer to the cropping settings read from driver.
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOWN_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::getCropRectangle(struct v4l2_rect *crop)
{
    LOGI("%s: @%s", mName.c_str(), __FUNCTION__);
    int ret;
    struct v4l2_crop v4l2_crop;
    CLEAR(v4l2_crop);

    if (!crop)
        return BAD_VALUE;

    if (mState == DEVICE_CLOSED)
        return INVALID_OPERATION;

    v4l2_crop.type = mBufType;

    PERFORMANCE_ATRACE_NAME("VIDIOC_G_CROP");
    // Update current configuration with the new one
    ret = pioctl(mFd, VIDIOC_G_CROP, &v4l2_crop, mName.c_str());
    if (ret != NO_ERROR)
        return ret;

    crop->left   = v4l2_crop.c.left;
    crop->top    = v4l2_crop.c.top;
    crop->width  = v4l2_crop.c.width;
    crop->height = v4l2_crop.c.height;

    return NO_ERROR;
}

int V4L2VideoNode::getFramerate(float * framerate, int width, int height, int pix_fmt)
{
    LOGI("%s: @%s", mName.c_str(), __FUNCTION__);
    int ret(0);
    struct v4l2_frmivalenum frm_interval;

    if (nullptr == framerate)
        return BAD_VALUE;

    if (mState == DEVICE_CLOSED) {
        LOGE("Invalid state (%d) to set an attribute",mState);
        return UNKNOWN_ERROR;
    }

    CLEAR(frm_interval);
    frm_interval.pixel_format = pix_fmt;
    frm_interval.width = width;
    frm_interval.height = height;
    *framerate = -1.0;

    PERFORMANCE_ATRACE_NAME("VIDIOC_ENUM_FRAMEINTERVALS");
    ret = pioctl(mFd, VIDIOC_ENUM_FRAMEINTERVALS, &frm_interval, mName.c_str());
    if (ret < 0) {
        LOGW("ioctl VIDIOC_ENUM_FRAMEINTERVALS failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    } else if (0 == frm_interval.discrete.denominator) {
        LOGE("ioctl VIDIOC_ENUM_FRAMEINTERVALS get invalid denominator value");
        *framerate = 0;
        return UNKNOWN_ERROR;
    }

    *framerate = 1.0 / (1.0 * frm_interval.discrete.numerator / frm_interval.discrete.denominator);

    return NO_ERROR;
}

/**
 * setBufferPool
 * updates the set buffer pool with externally allocated memory
 *
 * The device has to be at least in CONFIGURED state but once configured
 * it the buffer pool can be reset in PREPARED state.
 *
 * This pool will become active after calling start()
 * \param pool: array of void* where the memory is available
 * \param poolSize: amount of buffers in the pool
 * \param aFrameInfo: description of the properties of the buffers
 *                   it should match the configuration passed during setFormat
 * \param cached: boolean to detect whether the buffers are cached or not
 *                A cached buffer in this context means that the buffer
 *                memory may be accessed through some system caches, and
 *                the V4L2 driver must do cache invalidation in case
 *                the image data source is not updating system caches
 *                in hardware.
 *                When false (not cached), V4L2 driver can assume no cache
 *                invalidation/flushes are needed for this buffer.
 */
status_t V4L2VideoNode::setBufferPool(void **pool, unsigned int poolSize,
                                     FrameInfo *aFrameInfo, bool cached)
{
    V4L2BufferInfo vinfo;
    uint32_t cacheflags = V4L2_BUF_FLAG_NO_CACHE_INVALIDATE |
                          V4L2_BUF_FLAG_NO_CACHE_CLEAN;

    if ((mState != DEVICE_CONFIGURED) && (mState != DEVICE_PREPARED)) {
        LOGE("%s:Invalid operation, device %s not configured (state = %d)",
                __FUNCTION__, mName.c_str(), mState);
        return INVALID_OPERATION;
    }

    if (pool == nullptr || aFrameInfo == nullptr) {
        LOGE("Invalid parameters, pool %p frameInfo %p",pool, aFrameInfo);
        return BAD_TYPE;
    }

    /**
     * check that the configuration of these buffers matches what we have already
     * told the driver.
     */
    if ((aFrameInfo->width != mConfig.width) ||
        (aFrameInfo->height != mConfig.height) ||
        (aFrameInfo->stride != mConfig.stride) ||
        (aFrameInfo->format != mConfig.format) ) {
        LOGE("Pool configuration does not match device configuration: (%dx%d) s:%d f:%s Pool is: (%dx%d) s:%d f:%s ",
                mConfig.width, mConfig.height, mConfig.stride, v4l2Fmt2Str(mConfig.format),
                aFrameInfo->width, aFrameInfo->height, aFrameInfo->stride, v4l2Fmt2Str(aFrameInfo->format));
        return BAD_VALUE;
    }

    mSetBufferPool.clear();

    for (unsigned int i = 0; i < poolSize; i++) {
        vinfo.data = pool[i];
        vinfo.width = aFrameInfo->stride;
        vinfo.height = aFrameInfo->height;
        vinfo.format = aFrameInfo->format;
        vinfo.length = aFrameInfo->size;
        if (cached)
           vinfo.cache_flags = 0;
       else
           vinfo.cache_flags = cacheflags;

        mSetBufferPool.push_back(vinfo);
    }

    mState = DEVICE_PREPARED;
    return NO_ERROR;
}

/**
 * setBufferPool
 * Presents the pool of buffers to the device.
 *
 * The device has to be  in CONFIGURED state.
 *
 * In this stage we request the buffer slots to the device and present
 * them to the driver assigning one index to each buffer.
 *
 * After this method the device is PREPARED and ready to Q
 * buffers
 * The structures in the pool are empty.
 * After this call the buffers have been presented to the device an index is assigned.
 * The structure is updated with this index and other details  (this is the output)
 *
 * \param pool: [IN/OUT]std::vector of v4l2_buffers structures
 * \param cached: [IN]boolean to detect whether the buffers are cached or not
 *                A cached buffer in this context means that the buffer
 *                memory may be accessed through some system caches, and
 *                the V4L2 driver must do cache invalidation in case
 *                the image data source is not updating system caches
 *                in hardware.
 *                When false (not cached), V4L2 driver can assume no cache
 *                invalidation/flushes are needed for this buffer.
 *\return  INVALID_OPERATION if the device was not configured
 *\return  UNKNOWN_ERROR if any of the v4l2 commands fails
 *\return  NO_ERROR if everything went AOK
 */
status_t V4L2VideoNode::setBufferPool(std::vector<V4L2Buffer> &pool,
                                      bool cached, int memType)
{
    V4L2BufferInfo vinfo;
    int ret;
    uint32_t cacheflags = V4L2_BUF_FLAG_NO_CACHE_INVALIDATE |
                         V4L2_BUF_FLAG_NO_CACHE_CLEAN;

    if ((mState != DEVICE_CONFIGURED)) {
        LOGE("%s:Invalid operation, device %s not configured (state = %d)",
                __FUNCTION__, mName.c_str(), mState);
        return INVALID_OPERATION;
    }

    mBufferPool.clear();
    int num_buffers = requestBuffers(pool.size(), memType);
    if (num_buffers <= 0) {
        LOGE("%s: Could not complete buffer request",__FUNCTION__);
        return UNKNOWN_ERROR;
    }

    vinfo.width = mConfig.stride;
    vinfo.height = mConfig.height;
    vinfo.format = mConfig.format;
    vinfo.length = mConfig.size;

    for (unsigned int i = 0; i < pool.size(); i++) {
        if (cached)
            vinfo.cache_flags = 0;
        else
            vinfo.cache_flags = cacheflags;

        vinfo.vbuffer = pool.at(i);
        if (memType == V4L2_MEMORY_USERPTR) {
            vinfo.data = (void*)(pool[i].userptr());
        }
        ret = newBuffer(i, vinfo, memType);
        if (ret < 0) {
            LOGE("Error querying buffers status");
            mBufferPool.clear();
            mState = DEVICE_ERROR;
            return UNKNOWN_ERROR;
        }
        pool.at(i) = vinfo.vbuffer;
        mBufferPool.push_back(vinfo);
    }

    mMemoryType = memType;
    mState = DEVICE_PREPARED;
    return NO_ERROR;
}

status_t V4L2VideoNode::enumModes(std::vector<struct v4l2_sensor_mode> &modes)
{
    static const int MAX_ENUMS = 100000;
    static const __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_sensor_mode mode;
    int r, id, is, ii;

    for (id = 0; id < MAX_ENUMS; id++) {
        CLEAR(mode.fmt);
        mode.fmt.index = id;
        mode.fmt.type  = type;
        r = pioctl(mFd, VIDIOC_ENUM_FMT, &mode.fmt, mName.c_str());
        if (r < 0 && errno == EINVAL)
            break;
        if (r < 0)
            return UNKNOWN_ERROR;
        for (is = 0; is < MAX_ENUMS; is++) {
            int width = 0, height = 0;
            CLEAR(mode.size);
            mode.size.index = is;
            mode.size.pixel_format = mode.fmt.pixelformat;
            r = pioctl(mFd, VIDIOC_ENUM_FRAMESIZES, &mode.size, mName.c_str());
            if (r < 0 && errno == EINVAL)
                break;
            if (r < 0)
                return UNKNOWN_ERROR;
            switch (mode.size.type) {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                width  = mode.size.discrete.width;
                height = mode.size.discrete.height;
                break;
            case V4L2_FRMSIZE_TYPE_CONTINUOUS:
            case V4L2_FRMSIZE_TYPE_STEPWISE:
                width  = mode.size.stepwise.min_width;
                height = mode.size.stepwise.min_height;
                break;
            }
            for (ii = 0; ii < MAX_ENUMS; ii++) {
                CLEAR(mode.ival);
                mode.ival.index = ii;
                mode.ival.pixel_format = mode.fmt.pixelformat;
                mode.ival.width  = width;
                mode.ival.height = height;
                r = pioctl(mFd, VIDIOC_ENUM_FRAMEINTERVALS, &mode.ival, mName.c_str());
                if (r < 0 && errno == EINVAL)
                    break;
                if (r < 0)
                    return UNKNOWN_ERROR;
                modes.push_back(mode);
            }
            if (ii >= MAX_ENUMS)
                LOGE("%s too many frame intervals", __FUNCTION__);
        }
        if (is >= MAX_ENUMS)
            LOGE("%s too many frame sizes", __FUNCTION__);
    }
    if (id >= MAX_ENUMS)
        LOGE("%s too many frame formats", __FUNCTION__);
    return NO_ERROR;
}

////////////////////////////////////////////////////////////////////
//                          PRIVATE METHODS
////////////////////////////////////////////////////////////////////

void V4L2VideoNode::destroyBufferPool()
{

    LOGI("%s: @%s", mName.c_str(), __FUNCTION__);

    mBufferPool.clear();
    arc::CameraBufferManager* bufManager = arc::CameraBufferManager::GetInstance();
    for (int i=0;i<mDmaBufferPool.size();i++) {
        bufManager->Free(mDmaBufferPool[i]);
    }
    mDmaBufferPool.clear();

    requestBuffers(0, mMemoryType);
}

int V4L2VideoNode::requestBuffers(size_t num_buffers, int memType)
{
    struct v4l2_requestbuffers req_buf;
    int ret;
    CLEAR(req_buf);

    if (mState == DEVICE_CLOSED)
        return 0;

    req_buf.memory = memType;
    req_buf.count = num_buffers;
    req_buf.type = mBufType;

    PERFORMANCE_ATRACE_NAME_SNPRINTF("VIDIOC_REQBUFS - %zu", num_buffers);
    ret = pioctl(mFd, VIDIOC_REQBUFS, &req_buf, mName.c_str());

    if (ret < 0) {
        LOGE("VIDIOC_REQBUFS(%zu) returned: %d (%s)",
            num_buffers, ret, strerror(errno));
        return ret;
    }
    LOGI("%s: VIDIOC_REQBUFS, count=%u, memory:%s, type:%s", mName.c_str(), req_buf.count, ENUM2STR(v4l2_memory_enum,req_buf.memory), ENUM2STR(v4l2_buf_type_enum,req_buf.type));

    if (req_buf.count < num_buffers)
        LOGW("Got less buffers than requested! %u < %zu",req_buf.count, num_buffers);

    return req_buf.count;
}

void V4L2VideoNode::printBufferInfo(const char *func, V4L2Buffer &buf)
{
    switch (mMemoryType) {
    case V4L2_MEMORY_USERPTR:
        LOGI("%s: @%s, idx:%d addr:%p", mName.c_str(), func, buf.index(), (void *)buf.userptr());
        break;
    case V4L2_MEMORY_MMAP:
        LOGI("%s: @%s, idx:%d offset:0x%x", mName.c_str(), func, buf.index(), buf.offset());
        break;
    case V4L2_MEMORY_DMABUF:
        LOGI("%s: @%s, idx:%d fd:%d", mName.c_str(), func, buf.index(), buf.fd());
        break;
    default:
        LOGI("%s: @%s, unknown memory type %d", mName.c_str(), func, mMemoryType);
        break;
    }
}

int V4L2VideoNode::qbuf(V4L2BufferInfo *buf)
{
    int ret = 0;

    PERFORMANCE_ATRACE_NAME_SNPRINTF("VIDIOC_QBUF - %s", mName.c_str());

    buf->vbuffer.setFlags(buf->cache_flags);
    buf->vbuffer.setMemory(mMemoryType);
    buf->vbuffer.setType(mBufType);
    ret = pioctl(mFd, VIDIOC_QBUF, buf->vbuffer.get(), mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_QBUF on %s failed: %s", mName.c_str(), strerror(errno));
        return ret;
    }
    mBuffersInDevice++;
    LOGI("%s: VIDIOC_QBUF, Fd(%d), index=%u, mBuffersInDevice(%d)", mName.c_str(), mFd, buf->vbuffer.index(), mBuffersInDevice.load());
    return ret;
}

int V4L2VideoNode::dqbuf(V4L2BufferInfo *buf)
{
    V4L2Buffer &v4l2_buf = buf->vbuffer;
    int ret = 0;

    PERFORMANCE_ATRACE_NAME_SNPRINTF("VIDIOC_DQBUF - %s", mName.c_str());

    v4l2_buf.setMemory(mMemoryType);
    v4l2_buf.setType(mBufType);

    ret = pioctl(mFd, VIDIOC_DQBUF, v4l2_buf.get(), mName.c_str());

    if (ret < 0) {
        if (errno != EAGAIN)
            LOGE("VIDIOC_DQBUF failed: %s", strerror(errno));
        return ret;
    }
    mBuffersInDevice--;
    LOGI("%s: VIDIOC_DQBUF, Fd(%d), index=%u, mBuffersInDevice(%d)", mName.c_str(),
            mFd, v4l2_buf.index(), mBuffersInDevice.load());
    return ret;
}

/**
 * creates an active buffer pool from the set-buffer-pool that is provided
 * to the device by the call setBufferPool.
 *
 * We request to the V4L2 driver certain amount of buffer slots with the
 * buffer configuration.
 *
 * Then copy the required number from the set-buffer-pool to the active-buffer-pool
 *
 * \param buffer_count: number of buffers that the active pool contains
 * This number is at maximum the number of buffers in the set-buffer-pool
 * \return 0  success
 *        -1  failure
 */
int V4L2VideoNode::createBufferPool(unsigned int buffer_count)
{
    LOGI("%s: @%s: buf count %d", mName.c_str(), __FUNCTION__, buffer_count);
    int i(0);
    int ret(0);

    if (mState != DEVICE_PREPARED) {
        LOGE("%s: Incorrect device state  %d", __FUNCTION__, mState);
        return -1;
    }

    if (buffer_count > mSetBufferPool.size()) {
        LOGE("%s: Incorrect parameter requested %u, but only %zu provided",
                __FUNCTION__, buffer_count, mSetBufferPool.size());
        return -1;
    }

    int num_buffers = requestBuffers(buffer_count);
    if (num_buffers <= 0) {
        LOGE("%s: Could not complete buffer request",__FUNCTION__);
        return -1;
    }

    mBufferPool.clear();
    mDmaBufferPool.clear();

    for (i = 0; i < num_buffers; i++) {
        ret = newBuffer(i, mSetBufferPool.at(i));
        if (ret < 0)
            goto error;
        mBufferPool.push_back(mSetBufferPool[i]);
    }

    return 0;

error:
    LOGE("Failed to VIDIOC_QUERYBUF some of the buffers, clearing the active buffer pool");
    mBufferPool.clear();
    mDmaBufferPool.clear();
    return ret;
}


int V4L2VideoNode::newBuffer(int index, V4L2BufferInfo &buf, int memType)
{
    int ret;
    V4L2Buffer &vbuf = buf.vbuffer;
    if (memType == V4L2_MEMORY_DMABUF) {
        PERFORMANCE_ATRACE_NAME("VIDIOC_ALLOC_DMABUF");
        arc::CameraBufferManager* bufManager = arc::CameraBufferManager::GetInstance();
        buffer_handle_t handle;
        uint32_t stride = 0, size = 0;
        uint32_t usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_HW_CAMERA_WRITE|
                         RK_GRALLOC_USAGE_SPECIFY_STRIDE;

        int stride_h = (buf.height + 0xf) & ~0xf;
        LOGE("%s, [wxh] = [%dx%d], format 0x%x, usage 0x%x",
          __FUNCTION__, buf.width, stride_h, buf.format, usage);
        int ret = bufManager->Allocate(buf.width, stride_h, HAL_PIXEL_FORMAT_YCrCb_NV12, usage, arc::GRALLOC, &handle, &stride);
        vbuf.setFlags(0x0);
        vbuf.setMemory(memType);
        vbuf.setType(mBufType);
        vbuf.setIndex(index);
        vbuf.setFd(bufManager->GetHandleFd(handle));
        size = buf.width*buf.height*3/2;
        vbuf.setLength(size);
        LOGD("rk-debug: fd = %d", bufManager->GetHandleFd(handle));
        mDmaBufferPool.push_back(handle);
    } else {
        PERFORMANCE_ATRACE_NAME("VIDIOC_QUERYBUF");
        vbuf.setFlags(0x0);
        vbuf.setMemory(memType);
        vbuf.setType(mBufType);
        vbuf.setIndex(index);
        ret = pioctl(mFd , VIDIOC_QUERYBUF, vbuf.get(), mName.c_str());

        if (ret < 0) {
            LOGE("VIDIOC_QUERYBUF failed: %s", strerror(errno));
            return ret;
        }

        if (memType == V4L2_MEMORY_USERPTR) {
            vbuf.userptr((unsigned long)(buf.data));
        }

        buf.length = vbuf.length();
        LOGE("rk-debug: buf.length=%d", buf.length);
    }
    LOGI("%s: index: %u, type: %d, bytesused: %u, length: %u, flags %08x", mName.c_str(), vbuf.index(), vbuf.type(), vbuf.bytesused(), vbuf.length(), vbuf.flags());
    if (memType == V4L2_MEMORY_MMAP) {
        LOGI("memory MMAP: offset 0x%X", vbuf.offset());
    } else if (memType == V4L2_MEMORY_USERPTR) {
        LOGI("memory USRPTR:  %p", (void*)vbuf.userptr());
    } else if (memType == V4L2_MEMORY_DMABUF) {
        LOGI("memory DMABUF:  %d", vbuf.fd());
    } else {
        LOGE("not support memory type %d", memType);
    }
    return ret;
}

int V4L2VideoNode::freeBuffer(V4L2BufferInfo *buf_info)
{
    /**
     * For devices using usr ptr as data this method is a no-op
     */
    UNUSED(buf_info);
    return 0;
}

status_t V4L2VideoNode::getFormat(V4L2Format &aFormat)
{
    int ret = 0;

    if ((mState != DEVICE_OPEN) &&
        (mState != DEVICE_CONFIGURED)){
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    PERFORMANCE_ATRACE_NAME("VIDIOC_G_FMT");
    aFormat.setType(mBufType);
    ret = pioctl(mFd, VIDIOC_G_FMT, aFormat.get(), mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_G_FMT failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    if (V4L2_TYPE_IS_META(mBufType)) {
        LOGI("%s: VIDIOC_G_FMT: %s format: %d, size: %d", mName.c_str(),
                mName.c_str(),
                aFormat.pixelformat(),
                aFormat.sizeimage());
    } else {
        LOGI("%s: VIDIOC_G_FMT: width: %d, height: %d, bpl: %d, fourcc: %s, field: %d",
                mName.c_str(),
                aFormat.width(),
                aFormat.height(),
                aFormat.bytesperline(),
                v4l2Fmt2Str(aFormat.pixelformat()),
                aFormat.field());
    }

    return NO_ERROR;
}

} NAMESPACE_DECLARATION_END
