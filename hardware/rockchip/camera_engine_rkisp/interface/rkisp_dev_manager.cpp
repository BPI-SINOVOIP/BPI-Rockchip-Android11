/*
 * main_dev_manager.cpp - main device manager
 *
 *  Copyright (c) 2015 Intel Corporation
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
 * Author: John Ye <john.ye@intel.com>
 * Author: Wind Yuan <feng.yuan@intel.com>
 */

#include "rkisp_dev_manager.h"
#include "settings_processor.h"
#include "x3a_analyzer_rkiq.h"
#include "isp_poll_thread.h"
#include "rkcamera_vendor_tags.h"
#include <base/xcam_log.h>
#ifdef ANDROID_VERSION_ABOVE_8_X
#include <cutils/properties.h>
#define PROPERTY_VALUE_MAX 32
#define CAM_RKISP_PROPERTY_KEY  "vendor.cam.librkisp.ver"
#define CAM_AF_PROPERTY_KEY  "vendor.cam.librkisp.af.ver"
#define CAM_AEC_PROPERTY_KEY  "vendor.cam.librkisp.aec.ver"
#define CAM_AWB_PROPERTY_KEY  "vendor.cam.librkisp.awb.ver"

static char rkIspVersion[PROPERTY_VALUE_MAX] = CONFIG_CAM_ENGINE_LIB_VERSION;
static char rkIspAfVersion[PROPERTY_VALUE_MAX] = CONFIG_AF_LIB_VERSION;
static char rkIspAwbVersion[PROPERTY_VALUE_MAX] = CONFIG_AWB_LIB_VERSION;
static char rkIspAecVersion[PROPERTY_VALUE_MAX] = CONFIG_AE_LIB_VERSION;
#endif

using namespace XCam;

RkispDeviceManager::RkispDeviceManager(const cl_result_callback_ops_t *cb)
    : mCallbackOps (cb)
    , _isp_controller(nullptr)
    , _cur_settings(nullptr)
{
    _settingsProcessor = new SettingsProcessor();
    _settings.clear();
    _cl_state = -1;
}

RkispDeviceManager::~RkispDeviceManager()
{
    if(_settingsProcessor)
        delete _settingsProcessor;
    _settings.clear();
    _fly_settings.clear();
}

void
RkispDeviceManager::handle_message (const SmartPtr<XCamMessage> &msg)
{
    XCAM_UNUSED (msg);
}

void
RkispDeviceManager::handle_buffer (const SmartPtr<VideoBuffer> &buf)
{
    XCAM_ASSERT (buf.ptr ());
    _ready_buffers.push (buf);
}

SmartPtr<VideoBuffer>
RkispDeviceManager::dequeue_buffer ()
{
    SmartPtr<VideoBuffer> ret;
    ret = _ready_buffers.pop (-1);
    return ret;
}

void
RkispDeviceManager::x3a_calculation_done (XAnalyzer *analyzer, X3aResultList &results)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    CameraMetadata* metadata;
    camera_metadata_entry entry;
    int id = -1;

    SmartPtr<XmetaResult> meta_result;
    X3aResultList::iterator iter;
    for (iter = results.begin ();
            iter != results.end (); ++iter)
    {
        if ((*iter)->get_type() == XCAM_3A_METADATA_RESULT_TYPE) {
            meta_result = (*iter).dynamic_cast_ptr<XmetaResult> ();
            break ;
        }
    }
    if (iter == results.end()) {
        goto done;
    }

    metadata = meta_result->get_metadata_result();
    entry = metadata->find(ANDROID_REQUEST_ID);
    if (entry.count == 1)
        id = entry.data.i32[0];

    /* meta_result->dump(); */
    {
        SmartLock lock(_settingsMutex);
        if (!_fly_settings.empty())
            LOGI("@%s %d: flying id %d", __FUNCTION__, __LINE__, (*_fly_settings.begin())->reqId);
        if (!_fly_settings.empty() && (id == (*_fly_settings.begin())->reqId)) {
            _fly_settings.erase(_fly_settings.begin());
        } else {
            // return every meta result, we use meta to do extra work, eg.
            // flash stillcap synchronization
            id = -1;
        }
    }
    LOGI("@%s %d: result %d has %d metadata entries", __FUNCTION__, __LINE__,
         id, meta_result->get_metadata_result()->entryCount());

    rkisp_cl_frame_metadata_s cb_result;
    cb_result.id = id;
    cb_result.metas = metadata->getAndLock();
    if (mCallbackOps)
        mCallbackOps->metadata_result_callback(mCallbackOps, &cb_result);
    metadata->unlock(cb_result.metas);

done:
    DeviceManager::x3a_calculation_done (analyzer, results);
}

XCamReturn
RkispDeviceManager::set_control_params(const int request_frame_id,
                              const camera_metadata_t *metas)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    static bool stillcap_sync_cmd_end_delay = false;

    XCam::SmartPtr<AiqInputParams> inputParams = new AiqInputParams();
    inputParams->reqId = request_frame_id;
    inputParams->settings = metas;
    inputParams->staticMeta = &RkispDeviceManager::staticMeta;
    if(_settingsProcessor) {
        SmartPtr<X3aAnalyzerRKiq> RK3a_analyzer =  _3a_analyzer.dynamic_cast_ptr<X3aAnalyzerRKiq> ();
        struct isp_supplemental_sensor_mode_data* sensorModeData = RK3a_analyzer->getSensorModeData ();
        inputParams->sensorOutputWidth = sensorModeData->sensor_output_width;
        inputParams->sensorOutputHeight = sensorModeData->sensor_output_height;
        _settingsProcessor->processRequestSettings(inputParams->settings, *inputParams.ptr());
    } else {
        LOGE("@%s %d: _settingsProcessor is null , is a bug, fix me", __FUNCTION__, __LINE__);
        return XCAM_RETURN_ERROR_UNKNOWN;
    }
    XCamAeParam aeparams = inputParams->aeInputParams.aeParams;
    AeControls aectl = inputParams->aaaControls.ae;
    AfControls afctl = inputParams->aaaControls.af;
    LOGI("@%s: request %d: aeparms: mode-%d, metering_mode-%d, flicker_mode-%d,"
         "ex_min-%" PRId64 ",ex_max-%" PRId64 ", manual_exp-%" PRId64 ", manual_gain-%f,"
         "aeControls: mode-%d, lock-%d, preTrigger-%d, antibanding-%d,"
         "evCompensation-%d, fpsrange[%d, %d]", __FUNCTION__, request_frame_id,
         aeparams.mode, aeparams.metering_mode, aeparams.flicker_mode,
         aeparams.exposure_time_min, aeparams.exposure_time_max,
         aeparams.manual_exposure_time, aeparams.manual_analog_gain,
         aectl.aeMode, aectl.aeLock, aectl.aePreCaptureTrigger,
         aectl.aeAntibanding, aectl.evCompensation,
         aectl.aeTargetFpsRange[0], aectl.aeTargetFpsRange[1]);
    LOGI("@%s : reqId %d, afMode %d, afTrigger %d", __FUNCTION__, request_frame_id, afctl.afMode, afctl.afTrigger);
    LOGI("@%s : reqId %d, frame usecase %d, flash_mode %d, stillCapSyncCmd %d",
         __FUNCTION__, request_frame_id, inputParams->frameUseCase,
         aeparams.flash_mode, inputParams->stillCapSyncCmd);
    {
        SmartLock lock(_settingsMutex);
        // to speed up flash off routine
        if (inputParams->stillCapSyncCmd == RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCEND) {
            float power[2] = {0.0f, 0.0f};
            if (_isp_controller.ptr()) {
                _isp_controller->set_3a_fl (RKISP_FLASH_MODE_OFF, power, 0, 0);
                LOGD("reqId %d, stillCapSyncCmd %d, flash off",
                     request_frame_id, inputParams->stillCapSyncCmd);
            }
        }

        // we use id -1 request to do special work, eg. flash stillcap sync
        if (request_frame_id != -1) {
            if (stillcap_sync_cmd_end_delay) {
                stillcap_sync_cmd_end_delay = false;
                inputParams->stillCapSyncCmd = RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCEND;
            }
            _settings.push_back(inputParams);
        } else {
            // merged to next params
            if (inputParams->stillCapSyncCmd == RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCEND) {
                if (!_settings.empty()) {
                    XCam::SmartPtr<AiqInputParams> settings = *_settings.begin();
                    settings->stillCapSyncCmd = RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCEND;
                } else {
                    stillcap_sync_cmd_end_delay = true;
                }
            }
            if (inputParams->aaaControls.ae.aePreCaptureTrigger == ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START) {
                if (!_settings.empty()) {
                    XCam::SmartPtr<AiqInputParams> settings = *_settings.begin();
                    settings->aaaControls.ae.aePreCaptureTrigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START;
                    settings->reqId = -1;
                } else {
                    _cur_settings->aaaControls.ae.aePreCaptureTrigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START;
                    _cur_settings->reqId = -1;
                }
            }
        }
    }

    return ret;
}

void
RkispDeviceManager::pause_dequeue ()
{
    // should stop 3a because isp video stream may have been stopped
     _3a_analyzer->pause (true);
    if (_poll_thread.ptr())
        _poll_thread->stop();
    if (_event_subdevice.ptr())
        _event_subdevice->unsubscribe_event (V4L2_EVENT_FRAME_SYNC);
    if (_isp_params_device.ptr())
        _isp_params_device->stop();
    if (_isp_stats_device.ptr())
        _isp_stats_device->stop();

    return _ready_buffers.pause_pop ();
}

void
RkispDeviceManager::resume_dequeue ()
{
    SmartPtr<IspPollThread> ispPollThread =  _poll_thread.dynamic_cast_ptr<IspPollThread> ();

    if (_event_subdevice.ptr())
        _event_subdevice->subscribe_event (V4L2_EVENT_FRAME_SYNC);

    if (_isp_params_device.ptr() && !_isp_params_device->is_activated())
        _isp_params_device->start(false);
    if (_isp_stats_device.ptr() && !_isp_stats_device->is_activated())
        _isp_stats_device->start();

    _3a_analyzer->pause (false);
    // for IspController
    ispPollThread->resume();
    // sensor mode may be changed, so we should re-generate the first isp
    // configs
    _3a_analyzer->configure();
    ispPollThread->start();

    return _ready_buffers.resume_pop ();
}

static void xcam_init_cam_engine_lib(void) __attribute__((constructor));
static void xcam_init_cam_engine_lib(void)
{
    xcam_get_log_level();
    LOGI("\n*******************************************\n"
         "        CAM ENGINE LIB VERSION IS  %s\n"
         "        CAM ENGINE AF VERSION IS   %s\n"
         "        CAM ENGINE AWB VERSION IS  %s\n"
         "        CAM ENGINE AEC VERSION IS  %s\n"
         "\n*******************************************\n"
         , CONFIG_CAM_ENGINE_LIB_VERSION
		 , CONFIG_AF_LIB_VERSION
		 , CONFIG_AWB_LIB_VERSION
		 , CONFIG_AE_LIB_VERSION);
#ifdef ANDROID_VERSION_ABOVE_8_X
	property_set(CAM_RKISP_PROPERTY_KEY,rkIspVersion);
	property_set(CAM_AF_PROPERTY_KEY,rkIspAfVersion);
	property_set(CAM_AWB_PROPERTY_KEY,rkIspAwbVersion);
	property_set(CAM_AEC_PROPERTY_KEY,rkIspAecVersion);
#endif
}
