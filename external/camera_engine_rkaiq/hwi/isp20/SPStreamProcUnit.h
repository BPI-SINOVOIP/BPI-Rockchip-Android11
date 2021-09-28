/*
 *  Copyright (c) 2021 Rockchip Corporation
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
 *
 */
#ifndef _SP_STREAM_PROC_UNIT_H_
#define _SP_STREAM_PROC_UNIT_H_
#include <v4l2_device.h>
#include "poll_thread.h"
#include "xcam_mutex.h"
#include "TnrStatsStream.h"

using namespace XCam;

namespace RkCam {

class SPImagBufferProxy : public SubV4l2BufferProxy
{
    public:
    explicit SPImagBufferProxy(SmartPtr<V4l2Buffer> &buf, SmartPtr<V4l2Device> &device)
             :SubV4l2BufferProxy(buf, device)
    {
    }
    virtual ~SPImagBufferProxy() {}
    virtual uint8_t *map ()
    {
        return (uint8_t *)get_v4l2_planar_userptr(0);
    }
    virtual bool unmap ()
    {
        return true;
    }
    int set_buff_fd(int fd)
    {
        _buff_fd = fd;
        return 0;
    }
protected:
    XCAM_DEAD_COPY (SPImagBufferProxy);
};

class CamHwIsp20;
class SPStreamProcUnit       : public RKStream, public PollCallback
{
public:
    explicit SPStreamProcUnit (SmartPtr<V4l2Device> isp_sp_dev, int type);
    virtual ~SPStreamProcUnit ();
    virtual void start ();
    virtual void stop ();
    virtual SmartPtr<VideoBuffer> new_video_buffer(SmartPtr<V4l2Buffer> buf, SmartPtr<V4l2Device> dev);
    XCamReturn prepare (int width = 0, int height = 0, int stride = 0);
    void set_devices (CamHwIsp20* camHw, SmartPtr<V4l2SubDevice> isp_core_dev, SmartPtr<V4l2SubDevice> ispp_dev);
    XCamReturn get_sp_resolution(int &width, int &height, int &aligned_w, int &aligned_h);
    // from PollCallback
    virtual XCamReturn poll_buffer_ready (SmartPtr<VideoBuffer> &buf, int type) { return XCAM_RETURN_ERROR_FAILED; }
    virtual XCamReturn poll_buffer_failed (int64_t timestamp, const char *msg) { return XCAM_RETURN_ERROR_FAILED; }
    virtual XCamReturn poll_buffer_ready (SmartPtr<VideoBuffer> &buf);
    virtual XCamReturn poll_buffer_ready (SmartPtr<V4l2BufferProxy> &buf, int dev_index) { return XCAM_RETURN_ERROR_FAILED; }
    virtual XCamReturn poll_event_ready (uint32_t sequence, int type) { return XCAM_RETURN_ERROR_FAILED; }
    virtual XCamReturn poll_event_failed (int64_t timestamp, const char *msg) { return XCAM_RETURN_ERROR_FAILED; }
protected:
    XCAM_DEAD_COPY (SPStreamProcUnit);
    bool init_fbcbuf_fd();
    bool deinit_fbcbuf_fd();
    int get_fd_by_index(int index);
protected:
    CamHwIsp20* _camHw;
    SmartPtr<V4l2SubDevice>  _isp_core_dev;
    SmartPtr<V4l2SubDevice> _ispp_dev;

    int _ds_width;
    int _ds_height;
    int _ds_width_align;
    int _ds_height_align;
    int _src_width;
    int _src_height;

    std::map<int, int> _buf_fd_map;
    bool _first;
    //SmartPtr<TnrStatsStream> _tnr_stream_unit;
    SmartPtr<SubVideoBuffer> _ispgain;
};

}
#endif

