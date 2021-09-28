/*
 *  Copyright (c) 2019 Rockchip Corporation
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

#include <sys/select.h>
#include <time.h>
#include "FakeSensorHw.h"
#include "fake_v4l2_device.h"
namespace RkCam {
using namespace std;

static uint32_t rk_format_to_media_format(rk_aiq_format_t format)
{
    uint32_t pixelformat = -1;
    switch (format) {
    case RK_PIX_FMT_SBGGR10:
        pixelformat = MEDIA_BUS_FMT_SBGGR10_1X10;
        break;
    case RK_PIX_FMT_SRGGB10:
        pixelformat = MEDIA_BUS_FMT_SRGGB10_1X10;
        break;
    case RK_PIX_FMT_SGBRG10:
        pixelformat = MEDIA_BUS_FMT_SGBRG10_1X10;
        break;
    case RK_PIX_FMT_SGRBG10:
        pixelformat = MEDIA_BUS_FMT_SGRBG10_1X10;
        break;
    default:
        LOGE_CAMHW_SUBM(FAKECAM_SUBM, "%s no support format: %d\n",
                        __func__, format);
    }
    return pixelformat;
}

FakeSensorHw::FakeSensorHw()
    : BaseSensorHw ("/dev/null")
    , _first(true)
    , _working_mode(RK_AIQ_WORKING_MODE_NORMAL)
    ,_sync_cond(false)
    ,_need_sync(false)
{
    ENTER_CAMHW_FUNCTION();
    _timer = new CTimer(this);
    pFunc = NULL;
    mAiqExpParamsPool = new RkAiqExpParamsPool("FakeSensorExpParams", 8);
    EXIT_CAMHW_FUNCTION();
}

FakeSensorHw::~FakeSensorHw()
{
    ENTER_CAMHW_FUNCTION();
    delete _timer;
    EXIT_CAMHW_FUNCTION();
}

int
FakeSensorHw::get_blank(rk_aiq_exposure_sensor_descriptor* sns_des)
{
    sns_des->pixel_periods_per_line = sns_des->sensor_output_width;
    sns_des->line_periods_per_field = sns_des->sensor_output_height;
    return 0;
}

int
FakeSensorHw::get_pixel(rk_aiq_exposure_sensor_descriptor* sns_des)
{
    sns_des->pixel_clock_freq_mhz = 600.0f;
    return 0;
}

int
FakeSensorHw::get_sensor_fps(float& fps)
{
    fps = 25.0f;

    return 0;
}

int
FakeSensorHw::get_format(rk_aiq_exposure_sensor_descriptor* sns_des)
{
    sns_des->sensor_output_width = _width;
    sns_des->sensor_output_height = _height;
    sns_des->sensor_pixelformat = get_v4l2_pixelformat(_fmt_code);
    return 0;
}

int
FakeSensorHw::get_exposure_range(rk_aiq_exposure_sensor_descriptor* sns_des)
{
    sns_des->coarse_integration_time_min = 1;
    sns_des->coarse_integration_time_max_margin = 10;
    return 0;
}

int
FakeSensorHw::get_nr_switch(rk_aiq_sensor_nr_switch_t* nr_switch)
{

    nr_switch->valid = false;
    nr_switch->direct = 0;
    nr_switch->up_thres = 0;
    nr_switch->down_thres = 0;
    nr_switch->div_coeff = 0;

    return 0;
}

XCamReturn
FakeSensorHw::get_sensor_descriptor(rk_aiq_exposure_sensor_descriptor *sns_des)
{
    memset(sns_des, 0, sizeof(rk_aiq_exposure_sensor_descriptor));

    if (get_format(sns_des))
        return XCAM_RETURN_ERROR_IOCTL;

    if (get_blank(sns_des))
        return XCAM_RETURN_ERROR_IOCTL;

    /*
     * pixel rate is not equal to pclk sometimes
     * prefer to use pclk = ppl * lpp * fps
     */
    float fps = 0;
    if (get_sensor_fps(fps) == 0)
        sns_des->pixel_clock_freq_mhz =
            (float)(sns_des->pixel_periods_per_line) *
            sns_des->line_periods_per_field * fps / 1000000.0;
    else if (get_pixel(sns_des))
        return XCAM_RETURN_ERROR_IOCTL;

    if (get_exposure_range(sns_des))
        return XCAM_RETURN_ERROR_IOCTL;

    if (get_nr_switch(&sns_des->nr_switch)) {
        // do nothing;
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::setExposureParams(SmartPtr<RkAiqExpParamsProxy>& expPar)
{
    ENTER_CAMHW_FUNCTION();

    if (_first) {
        if (expPar->data()->exp_tbl_size > 0) {
            int lastIdx = expPar->data()->exp_tbl_size - 1;
            expPar->data()->aecExpInfo.LinearExp = expPar->data()->exp_tbl[lastIdx].LinearExp;
            expPar->data()->aecExpInfo.HdrExp[0] = expPar->data()->exp_tbl[lastIdx].HdrExp[0];
            expPar->data()->aecExpInfo.HdrExp[1] = expPar->data()->exp_tbl[lastIdx].HdrExp[1];
            expPar->data()->aecExpInfo.HdrExp[2] = expPar->data()->exp_tbl[lastIdx].HdrExp[2];
            expPar->data()->aecExpInfo.frame_length_lines = expPar->data()->exp_tbl[lastIdx].frame_length_lines;
            expPar->data()->aecExpInfo.line_length_pixels = expPar->data()->exp_tbl[lastIdx].line_length_pixels;
            expPar->data()->aecExpInfo.pixel_clock_freq_mhz = expPar->data()->exp_tbl[lastIdx].pixel_clock_freq_mhz;
        }
        SmartLock locker (_map_mutex);
        _effecting_exp_map[0] = expPar;
        _first = false;

        LOGD_CAMHW_SUBM(FAKECAM_SUBM, "exp-sync: first set exp, add id[0] to the effected exp map");
    } 

    EXIT_CAMHW_FUNCTION();
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::getEffectiveExpParams(SmartPtr<RkAiqExpParamsProxy>& expParams, int frame_id)
{
    ENTER_CAMHW_FUNCTION();

    std::map<int, SmartPtr<RkAiqExpParamsProxy>>::iterator it;
    int search_id = frame_id < 0 ? 0 : frame_id;
    SmartLock locker (_map_mutex);
    it = _effecting_exp_map.find(search_id);
    if (it == _effecting_exp_map.end()) {
        LOGW_CAMHW_SUBM(FAKECAM_SUBM, "Can't find exp params of frameid(%d), use the last one\n",search_id);
        expParams = _effecting_exp_map.rbegin()->second;
        search_id = _effecting_exp_map.rbegin()->first;
    } else {
        expParams = _effecting_exp_map[search_id];
    }
    LOGD_CAMHW_SUBM(FAKECAM_SUBM, "get id:%d, reg_gain:0x%x,reg_time:0x%x", search_id,
            expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.analog_gain_code_global,
            expParams->data()->aecExpInfo.LinearExp.exp_sensor_params.coarse_integration_time);
    EXIT_CAMHW_FUNCTION();

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::getSensorModeData(const char* sns_ent_name,
                            rk_aiq_exposure_sensor_descriptor& sns_des)
{
    rk_aiq_exposure_sensor_descriptor sensor_desc;
    get_sensor_descriptor (&sensor_desc);

    _sns_entity_name = sns_ent_name;
    sns_des.coarse_integration_time_min =
        sensor_desc.coarse_integration_time_min;
    sns_des.coarse_integration_time_max_margin =
        sensor_desc.coarse_integration_time_max_margin;
    sns_des.fine_integration_time_min =
        sensor_desc.fine_integration_time_min;
    sns_des.fine_integration_time_max_margin =
        sensor_desc.fine_integration_time_max_margin;

    sns_des.frame_length_lines = sensor_desc.line_periods_per_field;
    sns_des.line_length_pck = sensor_desc.pixel_periods_per_line;
    sns_des.vt_pix_clk_freq_hz = sensor_desc.pixel_clock_freq_mhz/*  * 1000000 */;
    sns_des.pixel_clock_freq_mhz = sensor_desc.pixel_clock_freq_mhz/* * 1000000 */;

    //add nr_switch
    sns_des.nr_switch = sensor_desc.nr_switch;

    sns_des.sensor_output_width = sensor_desc.sensor_output_width;
    sns_des.sensor_output_height = sensor_desc.sensor_output_height;
    sns_des.sensor_pixelformat = sensor_desc.sensor_pixelformat;

    LOGD_CAMHW_SUBM(FAKECAM_SUBM, "vts-hts-pclk: %d-%d-%d-%f, rect: [%dx%d]\n",
                    sns_des.frame_length_lines,
                    sns_des.line_length_pck,
                    sns_des.vt_pix_clk_freq_hz,
                    sns_des.pixel_clock_freq_mhz,
                    sns_des.sensor_output_width,
                    sns_des.sensor_output_height);

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::handle_sof(int64_t time, int frameid)
{
    ENTER_CAMHW_FUNCTION();
    EXIT_CAMHW_FUNCTION();
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::set_working_mode(int mode)
{
    __u32 hdr_mode = NO_HDR;

    if (mode == RK_AIQ_WORKING_MODE_NORMAL) {
        hdr_mode = NO_HDR;
    } else if (mode == RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR ||
               mode == RK_AIQ_ISP_HDR_MODE_2_LINE_HDR) {
        hdr_mode = HDR_X2;
    } else if (mode == RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR ||
               mode == RK_AIQ_ISP_HDR_MODE_3_LINE_HDR) {
        hdr_mode = HDR_X3;
    } else {
        LOGE_CAMHW_SUBM(FAKECAM_SUBM, "failed to set hdr mode to %d", mode);
        return XCAM_RETURN_ERROR_FAILED;
    }

    _working_mode = mode;

    LOGD_CAMHW_SUBM(FAKECAM_SUBM, "%s _working_mode: %d\n",
                    __func__, _working_mode);

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::set_mirror_flip(bool mirror, bool flip, int32_t& skip_frame_sequence)
{
    return XCAM_RETURN_NO_ERROR;
}


XCamReturn
FakeSensorHw::get_mirror_flip(bool& mirror, bool& flip)
{
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::set_exp_delay_info(int time_delay, int gain_delay, int hcg_lcg_mode_delay)
{

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::start()
{
    ENTER_CAMHW_FUNCTION();

    V4l2SubDevice::start();

    _timer->SetTimer(0, 10000);
    _timer->StartTimer();

    EXIT_CAMHW_FUNCTION();
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::stop()
{
    ENTER_CAMHW_FUNCTION();
    _timer->StopTimer();
    _effecting_exp_map.clear();
    _vbuf_list.clear();
    V4l2SubDevice::stop();
    EXIT_CAMHW_FUNCTION();
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::get_selection (int pad, uint32_t target, struct v4l2_subdev_selection &select)
{
    XCAM_ASSERT (is_opened());

    return XCAM_RETURN_ERROR_IOCTL;
}

XCamReturn
FakeSensorHw::getFormat(struct v4l2_subdev_format &aFormat)
{
    int ret = 0;
    ENTER_CAMHW_FUNCTION();
    aFormat.format.width = _width;
    aFormat.format.height = _height;
    aFormat.format.code = _fmt_code;
    aFormat.format.field = V4L2_FIELD_NONE;
    aFormat.format.colorspace = V4L2_COLORSPACE_470_SYSTEM_M;
    LOGD_CAMHW_SUBM(FAKECAM_SUBM, "pad: %d, which: %d, width: %d, "
         "height: %d, format: 0x%x, field: %d, color space: %d",
         aFormat.pad,
         aFormat.which,
         aFormat.format.width,
         aFormat.format.height,
         aFormat.format.code,
         aFormat.format.field,
         aFormat.format.colorspace);
    EXIT_CAMHW_FUNCTION();
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::prepare(rk_aiq_raw_prop_t prop)
{
    ENTER_CAMHW_FUNCTION();
    _width = prop.frame_width;
    _height = prop.frame_height;
    _fmt_code = rk_format_to_media_format(prop.format);
    _rawbuf_type = prop.rawbuf_type;
    EXIT_CAMHW_FUNCTION();
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::enqueue_rawbuffer(struct rk_aiq_vbuf *vbuf, bool sync)
{
    int max_count = 0;
    SmartPtr<FakeV4l2Device> fake_v4l2_dev;
    ENTER_CAMHW_FUNCTION();
    if (_working_mode == RK_AIQ_WORKING_MODE_NORMAL) {
        max_count = 1;
    } else if (_working_mode == RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR ||
               _working_mode == RK_AIQ_ISP_HDR_MODE_2_LINE_HDR) {
        max_count = 2;
    } else if (_working_mode == RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR ||
               _working_mode == RK_AIQ_ISP_HDR_MODE_3_LINE_HDR) {
        max_count = 3;
    }
    for (int i=0; i<max_count; i++) {
        fake_v4l2_dev = _mipi_tx_dev[i].dynamic_cast_ptr<FakeV4l2Device>();
        fake_v4l2_dev->enqueue_rawbuffer(&vbuf->buf_info[i]);
    }
    _mutex.lock();
    _vbuf_list.push_back(*vbuf);
    _mutex.unlock();

    _map_mutex.lock();
    while (_effecting_exp_map.size() > 4)
        _effecting_exp_map.erase(_effecting_exp_map.begin());

    int fid = vbuf->buf_info[0].frame_id;
    SmartPtr<RkAiqExpParamsProxy> exp_param_prx = mAiqExpParamsPool->get_item();

    exp_param_prx->data()->aecExpInfo.LinearExp.exp_sensor_params.analog_gain_code_global = vbuf->buf_info[0].exp_gain;
    exp_param_prx->data()->aecExpInfo.LinearExp.exp_sensor_params.coarse_integration_time = vbuf->buf_info[0].exp_time;
    exp_param_prx->data()->aecExpInfo.HdrExp[2].exp_sensor_params.analog_gain_code_global = vbuf->buf_info[2].exp_gain;
    exp_param_prx->data()->aecExpInfo.HdrExp[2].exp_sensor_params.coarse_integration_time = vbuf->buf_info[2].exp_time;
    exp_param_prx->data()->aecExpInfo.HdrExp[1].exp_sensor_params.analog_gain_code_global = vbuf->buf_info[1].exp_gain;
    exp_param_prx->data()->aecExpInfo.HdrExp[1].exp_sensor_params.coarse_integration_time = vbuf->buf_info[1].exp_time;
    exp_param_prx->data()->aecExpInfo.HdrExp[0].exp_sensor_params.analog_gain_code_global = vbuf->buf_info[0].exp_gain;
    exp_param_prx->data()->aecExpInfo.HdrExp[0].exp_sensor_params.coarse_integration_time = vbuf->buf_info[0].exp_time;
    _effecting_exp_map[fid] = exp_param_prx;
    LOGD_CAMHW_SUBM(FAKECAM_SUBM, "add id[%d] to the effected exp map", fid);
    _map_mutex.unlock();
    if (sync) {
        _need_sync = sync;
        if (_sync_cond.timedwait(_sync_mutex, 5000000) != 0) {
           LOGE_CAMHW_SUBM(FAKECAM_SUBM, "wait raw buffer process done timeout");
           return XCAM_RETURN_ERROR_TIMEOUT;
        }
    }
    EXIT_CAMHW_FUNCTION();
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
FakeSensorHw::register_rawdata_callback(void (*callback)(void *))
{
    ENTER_XCORE_FUNCTION();
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    pFunc = callback;
    EXIT_XCORE_FUNCTION();
    return ret;
}

XCamReturn
FakeSensorHw::on_dqueue(int dev_idx, SmartPtr<V4l2BufferProxy> buf_proxy)
{
    ENTER_XCORE_FUNCTION();
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    std::list<struct rk_aiq_vbuf>::iterator it;
    SmartLock locker (_mutex);

    if (!_vbuf_list.empty()) {
        for(it = _vbuf_list.begin(); it != _vbuf_list.end(); it++) {
            if (_rawbuf_type == RK_AIQ_RAW_DATA) {
                uintptr_t ptr = buf_proxy->get_reserved();
                LOGD_CAMHW_SUBM(FAKECAM_SUBM, "rawbuf_type(data): %p vs 0x%x",it->buf_info[dev_idx].data_addr,ptr);
                if (it->buf_info[dev_idx].data_addr == (uint8_t*)ptr) {
                    it->buf_info[dev_idx].valid = false;
                    break;
                }
            } else if (_rawbuf_type == RK_AIQ_RAW_ADDR) {
                uintptr_t ptr = buf_proxy->get_v4l2_userptr();
                LOGD_CAMHW_SUBM(FAKECAM_SUBM, "rawbuf_type(addr): %p vs 0x%x",it->buf_info[dev_idx].data_addr,ptr);
                if (it->buf_info[dev_idx].data_addr == (uint8_t*)ptr) {
                    it->buf_info[dev_idx].valid = false;
                    break;
                }
            } else if (_rawbuf_type == RK_AIQ_RAW_FD) {
                uint32_t buf_fd = buf_proxy->get_expbuf_fd();
                LOGD_CAMHW_SUBM(FAKECAM_SUBM, "rawbuf_type(fd): %d vs %d",it->buf_info[dev_idx].data_fd,buf_fd);
                if (it->buf_info[dev_idx].data_fd == buf_fd) {
                    it->buf_info[dev_idx].valid = false;
                    break;
                }
            } else if (_rawbuf_type == RK_AIQ_RAW_FILE) {
                uintptr_t ptr = buf_proxy->get_v4l2_userptr();
                LOGD_CAMHW_SUBM(FAKECAM_SUBM, "rawbuf_type(file): %p vs 0x%x",it->buf_info[dev_idx].data_addr,ptr);
                if (it->buf_info[dev_idx].data_addr == (uint8_t*)ptr) {
                    it->buf_info[dev_idx].valid = false;
                    break;
                }
            } else {
                LOGE_CAMHW_SUBM(FAKECAM_SUBM, "raw buf type is wrong:0x%x",_rawbuf_type);
                return XCAM_RETURN_ERROR_FAILED;
            }

        }
        if (it != _vbuf_list.end()) {
            switch (_working_mode)
            {
                case RK_AIQ_WORKING_MODE_NORMAL:
                if (!it->buf_info[0].valid) {
                    goto out;
                }
                break;
                case RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR:
                case RK_AIQ_ISP_HDR_MODE_2_LINE_HDR:
                if (!it->buf_info[0].valid && !it->buf_info[1].valid) {
                    goto out;
                }
                break;
                case RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR:
                case RK_AIQ_ISP_HDR_MODE_3_LINE_HDR:
                if (!it->buf_info[0].valid && !it->buf_info[1].valid && !it->buf_info[2].valid) {
                    goto out;
                }
                break;
            }
        }
    }
    EXIT_XCORE_FUNCTION();
    return ret;
 out:
    _vbuf_list.erase(it);

    if (_need_sync) {
        LOGD_CAMHW_SUBM(FAKECAM_SUBM, "give off signal");
        _sync_cond.signal();
    }else {
        if (pFunc)
            pFunc(it->base_addr);
    }
    EXIT_XCORE_FUNCTION();
    return ret;
}

XCamReturn
FakeSensorHw::set_mipi_tx_devs(SmartPtr<V4l2Device> mipi_tx_devs[3])
{
    _mipi_tx_dev[0] = mipi_tx_devs[0];
    _mipi_tx_dev[1] = mipi_tx_devs[1];
    _mipi_tx_dev[2] = mipi_tx_devs[2];
    return XCAM_RETURN_NO_ERROR;
}

CTimer::CTimer(FakeSensorHw *dev):
    m_second(0), m_microsecond(0)
{
    _dev = dev;
}

CTimer::~CTimer()
{
}

void CTimer::SetTimer(long second, long microsecond)
{
    m_second = second;
    m_microsecond = microsecond;
}

void CTimer::StartTimer()
{
    pthread_create(&thread_timer, NULL, OnTimer_stub, this);
}

void CTimer::StopTimer()
{
    pthread_cancel(thread_timer);
    pthread_join(thread_timer, NULL);
}

void CTimer::thread_proc()
{
    while (true)
    {
        OnTimer();
        pthread_testcancel();
        struct timeval tempval;
        tempval.tv_sec = m_second;
        tempval.tv_usec = m_microsecond;
        select(0, NULL, NULL, NULL, &tempval);
    }
}

void CTimer::OnTimer()
{
    SmartPtr<FakeV4l2Device> fake_v4l2_dev;
    ENTER_CAMHW_FUNCTION();
    if (_dev->_working_mode == RK_AIQ_WORKING_MODE_NORMAL) {
        fake_v4l2_dev = _dev->_mipi_tx_dev[0].dynamic_cast_ptr<FakeV4l2Device>();
        fake_v4l2_dev->on_timer_proc();
    } else if (_dev->_working_mode == RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR ||
               _dev->_working_mode == RK_AIQ_ISP_HDR_MODE_2_LINE_HDR) {
        fake_v4l2_dev = _dev->_mipi_tx_dev[0].dynamic_cast_ptr<FakeV4l2Device>();
        fake_v4l2_dev->on_timer_proc();
        fake_v4l2_dev = _dev->_mipi_tx_dev[1].dynamic_cast_ptr<FakeV4l2Device>();
        fake_v4l2_dev->on_timer_proc();
    } else if (_dev->_working_mode == RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR ||
               _dev->_working_mode == RK_AIQ_ISP_HDR_MODE_3_LINE_HDR) {
        fake_v4l2_dev = _dev->_mipi_tx_dev[0].dynamic_cast_ptr<FakeV4l2Device>();
        fake_v4l2_dev->on_timer_proc();
        fake_v4l2_dev = _dev->_mipi_tx_dev[1].dynamic_cast_ptr<FakeV4l2Device>();
        fake_v4l2_dev->on_timer_proc();
        fake_v4l2_dev = _dev->_mipi_tx_dev[2].dynamic_cast_ptr<FakeV4l2Device>();
        fake_v4l2_dev->on_timer_proc();
        
    }
    EXIT_XCORE_FUNCTION();
}

}; //namespace RkCam
