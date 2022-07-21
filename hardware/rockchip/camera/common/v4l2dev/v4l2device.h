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

#ifndef _CAMERA3_HAL_V4L2DEVICE_H_
#define _CAMERA3_HAL_V4L2DEVICE_H_

#include <memory>
#include <atomic>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <poll.h>
#include <string>
#include <sstream>
#include <vector>
#include "PerformanceTraces.h"
#include "FrameInfo.h"
#include "SysCall.h"
#include <map>

#include "arc/camera_buffer_manager.h"

NAMESPACE_DECLARATION {
#define pioctl(fd, ctrlId, attr, name) \
    SysCall::ioctl(fd, ctrlId, attr)

#define perfopen(name, attr, fd) \
    fd = SysCall::open(name, attr)

#define perfclose(name, fd) \
    SysCall::close(fd)

#define pbxioctl(ctrlId, attr) \
    V4L2DeviceBase::xioctl(ctrlId, attr)

#define perfpoll(fd, value, timeout) \
    SysCall::poll(fd, value, timeout)

/*
 * Wrapper for v4l2_buffer to provide compatible
 * interfaces for multi-plane buffers.
 */
class V4L2Buffer
{
public:
    V4L2Buffer();
    V4L2Buffer(const struct v4l2_buffer &buf);
    uint32_t index() const {return vbuf.index;}
    void setIndex(uint32_t index) {vbuf.index = index;}
    uint32_t type() {return vbuf.type;}
    void setType(uint32_t type);
    uint32_t flags() {return vbuf.flags;}
    void setFlags(uint32_t flags) {vbuf.flags = flags;}
    uint32_t field() {return vbuf.field;}
    void setField(uint32_t field) {vbuf.field = field;}
    struct timeval timestamp() {return vbuf.timestamp;}
    void setTimestamp(struct timeval timestamp) {vbuf.timestamp = timestamp;}
    struct v4l2_timecode timecode() {return vbuf.timecode;}
    void setTimecode(struct v4l2_timecode timecode) {vbuf.timecode = timecode;}
    uint32_t sequence() {return vbuf.sequence;}
    void setSequence(uint32_t sequence) {vbuf.sequence = sequence;}
    uint32_t memory() {return vbuf.memory;}
    void setMemory(uint32_t memory) {vbuf.memory = memory;}
    uint32_t offset(int plane = 0);
    void setOffset(uint32_t offset, int plane = 0);
    unsigned long userptr(int plane = 0);
    void setUserptr(unsigned long userptr, int plane = 0);
    int fd(int plane = 0);
    void setFd(int fd, int plane = 0);
    uint32_t bytesused(int plane = 0);
    void setBytesused(uint32_t bytesused, int plane = 0);
    uint32_t length(int plane = 0);
    void setLength(uint32_t length, int plane = 0);
    uint32_t getNumPlanes();
    void setNumPlanes(int numPlanes);
    struct v4l2_buffer *get() {return &vbuf;}
    const V4L2Buffer &operator=(const V4L2Buffer &buf);

private:
    struct v4l2_buffer vbuf;
    std::vector<struct v4l2_plane> planes; // For multi-planar buffers.
};

/**
 * v4l2 buffer descriptor.
 *
 * This information is stored in the pool
 */
class V4L2BufferInfo {
public:
    V4L2BufferInfo();
    void *data;
    size_t length;
    int width;
    int height;
    int format;
    int cache_flags;        /*!< initial flags used when creating buffers */
    V4L2Buffer vbuffer;
};

/*
 * Wrapper for v4l2_format to provide compatible
 * interfaces for multi-plane buffers.
 */
class V4L2Format
{
public:
    V4L2Format() {CLEAR(vfmt);}
    V4L2Format(const struct v4l2_format &fmt) {vfmt = fmt;}
    uint32_t type() {return vfmt.type;}
    void setType(uint32_t type);
    uint32_t width();
    void setWidth(uint32_t width);
    uint32_t height();
    void setHeight(uint32_t height);
    uint32_t pixelformat();
    void setPixelformat(uint32_t format);
    uint32_t field();
    void setField(uint32_t field);
    uint32_t bytesperline(int plane = 0);
    void setBytesperline(uint32_t bytesperline, int plane = 0);
    uint32_t sizeimage(int plane = 0);
    void setSizeimage(uint32_t size, int plane = 0);
    struct v4l2_format *get() {return &vfmt;}
    const V4L2Format &operator=(const V4L2Format &fmt);

private:
    struct v4l2_format vfmt;
};

struct v4l2_sensor_mode {
    struct v4l2_fmtdesc fmt;
    struct v4l2_frmsizeenum size;
    struct v4l2_frmivalenum ival;
};

/**
 * A class encapsulating simple V4L2 device operations
 *
 * Base class that contains common V4L2 operations used by video nodes and
 * subdevices. It provides a slightly higher interface than IOCTLS to access
 * the devices. It also stores:
 * - State of the device to protect from invalid usage sequence
 * - Name
 * - File descriptor
 */
class V4L2DeviceBase {
public:
    explicit V4L2DeviceBase(const char *name);
    virtual ~V4L2DeviceBase();

    virtual status_t open();
    virtual status_t close();

    virtual int xioctl(int request, void *arg, int *errnoCopy = nullptr) const;
    virtual int poll(int timeout);

    virtual status_t setControl(int aControlNum, const int value, const char *name);
    virtual status_t getControl(int aControlNum, int *value);
    virtual status_t queryMenu(v4l2_querymenu &menu);
    virtual status_t queryControl(v4l2_queryctrl &control);

    virtual int subscribeEvent(int event);
    virtual int unsubscribeEvent(int event);
    virtual int dequeueEvent(struct v4l2_event *event);

    bool isOpen() { return mFd != -1; }
    int getFd() { return mFd; }

    const char* name() { return mName.c_str(); }

    static int pollDevices(const std::vector<std::shared_ptr<V4L2DeviceBase>> &devices,
                               std::vector<std::shared_ptr<V4L2DeviceBase>> &activeDevices,
                               std::vector<std::shared_ptr<V4L2DeviceBase>> &inactiveDevices,
                               int timeOut, int flushFd = -1,
                               int events = POLLPRI | POLLIN | POLLERR);

    static int frmsizeWidth(struct v4l2_frmsizeenum &size);
    static int frmsizeHeight(struct v4l2_frmsizeenum &size);
    static void frmivalIval(struct v4l2_frmivalenum &frmival, struct v4l2_fract &ival);
    static int cmpFract(struct v4l2_fract &f1, struct v4l2_fract &f2);
    static int cmpIval(struct v4l2_frmivalenum &i1, struct v4l2_frmivalenum &i2);

protected:
    std::string  mName;     /*!< path to device in file system, ex: /dev/video0 */
    int          mFd;       /*!< file descriptor obtained when device is open */

};


/**
 * A class encapsulating simple V4L2 video device node operations
 *
 * This class extends V4L2DeviceBase and adds extra internal state
 * and more convenience methods to  manage an associated buffer pool
 * with the device.
 * This class introduces new methods specifics to control video device nodes
 *
 */
class V4L2VideoNode: public V4L2DeviceBase {
public:
    explicit V4L2VideoNode(const char *name);
    virtual ~V4L2VideoNode();

    virtual status_t open();
    virtual status_t close();

    virtual status_t setBlock(bool block);

    virtual status_t setInput(int index);
    virtual status_t queryCap(struct v4l2_capability *cap);
    virtual status_t enumerateInputs(struct v4l2_input *anInput);

    // Video node configuration
    virtual int getFramerate(float * framerate, int width, int height, int pix_fmt);
    virtual status_t setParameter (struct v4l2_streamparm *aParam);
    virtual status_t getMaxCropRectangle(struct v4l2_rect *crop);
    virtual status_t setCropRectangle (struct v4l2_rect *crop);
    virtual status_t getCropRectangle (struct v4l2_rect *crop);
    virtual status_t setFormat(FrameInfo &aConfig);
    virtual status_t getFormat(V4L2Format &aFormat);
    virtual status_t setSelection(const struct v4l2_selection &aSelection);
    virtual status_t queryCapturePixelFormats(std::vector<v4l2_fmtdesc> &formats);
    virtual int getMemoryType();

    // Buffer pool management -- DEPRECATED!
    virtual status_t setBufferPool(void **pool, unsigned int poolSize,
                                   FrameInfo *aFrameInfo, bool cached);
    virtual void destroyBufferPool();
    virtual int createBufferPool(unsigned int buffer_count);

     // New Buffer pool management
    virtual status_t setBufferPool(std::vector<V4L2Buffer> &pool,
                                   bool cached,
                                   int memType = V4L2_MEMORY_USERPTR);

    // Buffer flow control
    virtual int stop(bool keepBuffers = false);
    virtual int start(int initialSkips);

    virtual int grabFrame(V4L2BufferInfo *buf);
    virtual int putFrame(const V4L2Buffer &buf);
    virtual int putFrame(unsigned int index);
    virtual int exportFrame(unsigned int index);

    // Convenience accessors
    virtual bool isStarted() const { return mState == DEVICE_STARTED; }
    virtual unsigned int getFrameCount() const { return mFrameCounter; }
    virtual unsigned int getBufsInDeviceCount() const { return mBuffersInDevice; }
    virtual unsigned int getInitialFrameSkips() const { return mInitialSkips; }
    virtual void getConfig(FrameInfo &config) const { config = mConfig; }

    virtual status_t enumModes(std::vector<struct v4l2_sensor_mode> &modes);

private:
    virtual status_t setPixFormat(V4L2Format &aFormat);
    virtual status_t setMetaFormat(V4L2Format &aFormat);

protected:
    virtual int qbuf(V4L2BufferInfo *buf);
    virtual int dqbuf(V4L2BufferInfo *buf);
    virtual int newBuffer(int index, V4L2BufferInfo &buf,
                          int memType = V4L2_MEMORY_USERPTR);
    virtual int freeBuffer(V4L2BufferInfo *buf_info);
    virtual int requestBuffers(size_t num_buffers,
                               int memType = V4L2_MEMORY_USERPTR);
    virtual void printBufferInfo(const char *func, V4L2Buffer &buf);

protected:

    enum VideoNodeState  {
            DEVICE_CLOSED = 0,  /*!< kernel device closed */
            DEVICE_OPEN,        /*!< device node opened */
            DEVICE_CONFIGURED,  /*!< device format set, IOC_S_FMT */
            DEVICE_PREPARED,    /*!< device has requested buffers (set_buffer_pool)*/
            DEVICE_STARTED,     /*!< stream started, IOC_STREAMON */
            DEVICE_ERROR        /*!< undefined state */
        };

    VideoNodeState  mState;
    // Device capture configuration
    FrameInfo mConfig;

    std::atomic<int32_t> mBuffersInDevice;      /*!< Tracks the amount of buffers inside the driver. Goes from 0 to the size of the pool*/
    unsigned int mFrameCounter;             /*!< Tracks the number of output buffers produced by the device. Running counter. It is reset when we start the device*/
    unsigned int mInitialSkips;

    std::vector<V4L2BufferInfo> mSetBufferPool; /*!< DEPRECATED:This is the buffer pool set before the device is prepared*/
    std::vector<V4L2BufferInfo> mBufferPool;    /*!< This is the active buffer pool */
    std::vector<buffer_handle_t> mDmaBufferPool;

    enum v4l2_buf_type mBufType;
    int                mMemoryType;
};

/**
 * A class encapsulating simple V4L2 sub device node operations
 *
 * Sub-devices are control points to the new media controller architecture
 * in V4L2
 *
 */
class V4L2Subdevice: public V4L2DeviceBase {
public:
    explicit V4L2Subdevice(const char *name);
    virtual ~V4L2Subdevice();

    virtual status_t open();
    virtual status_t close();

    status_t queryDvTimings(struct v4l2_dv_timings &timings);
    status_t queryFormats(int pad, std::vector<uint32_t> &formats);
    status_t getFormat(struct v4l2_subdev_format &aFormat);
    status_t setFormat(int pad, int width, int height, int formatCode, int field, int quantization);
    status_t getSelection(struct v4l2_subdev_selection &aSelection);
    status_t setSelection(int pad, int target, int top, int left, int width, int height);
    status_t getPadFormat(int padIndex, int &width, int &height, int &code);
    status_t setFramerate(int pad, int fps);
    status_t getSensorFrameDuration(int32_t &duration);
    status_t getSensorFormats(int pad, uint32_t code, std::vector<struct v4l2_subdev_frame_size_enum> &fse);
private:
    status_t setFormat(struct v4l2_subdev_format &aFormat);
    status_t setSelection(struct v4l2_subdev_selection &aSelection);
    status_t setFrameInterval(struct v4l2_subdev_frame_interval &finterval);
private:
    enum SubdevState  {
            DEVICE_CLOSED = 0,  /*!< kernel device closed */
            DEVICE_OPEN,        /*!< device node opened */
            DEVICE_CONFIGURED,  /*!< device format set, IOC_S_FMT */
            DEVICE_ERROR        /*!< undefined state */
        };

    SubdevState     mState;
    FrameInfo       mConfig;
    std::map<uint32_t, char> codeIndex;
};

} NAMESPACE_DECLARATION_END
#endif // _CAMERA3_HAL_V4L2DEVICE_H_
