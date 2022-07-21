/*
 * Copyright (C) 2017 Intel Corporation.
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

#define LOG_TAG "RKISP2ControlUnit"

#include <sys/stat.h>
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "RKISP2ControlUnit.h"
#include "RKISP1CameraHw.h"
#include "RKISP2CameraHw.h"
#include "RKISP2ImguUnit.h"
#include "CameraStream.h"
#include "RKISP2CameraCapInfo.h"
#include "CameraMetadataHelper.h"
#include "PlatformData.h"
#include "RKISP2ProcUnitSettings.h"
#include "RKISP2SettingsProcessor.h"
#include "RKISP2Metadata.h"
#include "MediaEntity.h"
#include "rkcamera_vendor_tags.h"
//#include "TuningServer.h"

USING_METADATA_NAMESPACE;
static const int SETTINGS_POOL_SIZE = MAX_REQUEST_IN_PROCESS_NUM * 2;

namespace android {
namespace camera2 {
namespace rkisp2 {

SocCamFlashCtrUnit::SocCamFlashCtrUnit(const char* name,
                                       int CameraId) :
        mFlSubdev(new V4L2Subdevice(name)),
        mV4lFlashMode(V4L2_FLASH_LED_MODE_NONE),
        mAePreTrigger(0),
        mAeTrigFrms(0),
        mMeanLuma(1.0f),
        mAeFlashMode(ANDROID_FLASH_MODE_OFF),
        mAeMode(ANDROID_CONTROL_AE_MODE_ON),
        mAeState(ANDROID_CONTROL_AE_STATE_INACTIVE)
{
    LOGD("%s:%d", __FUNCTION__, __LINE__);
    if (mFlSubdev.get())
        mFlSubdev->open();
}

SocCamFlashCtrUnit::~SocCamFlashCtrUnit()
{
    LOGD("%s:%d", __FUNCTION__, __LINE__);
    if (mFlSubdev.get()) {
      setV4lFlashMode(CAM_AE_FLASH_MODE_OFF, 100, 0, 0);
      mFlSubdev->close();
    }
}

void SocCamFlashCtrUnit::setMeanLuma(float luma)
{
    if (mAeTrigFrms == 0)
        mMeanLuma = luma;
}

int SocCamFlashCtrUnit::setFlashSettings(const CameraMetadata *settings)
{
    int ret = 0;
    // parse flash mode, ae mode, ae precap trigger
    uint8_t aeMode = ANDROID_CONTROL_AE_MODE_ON;
    camera_metadata_ro_entry entry = settings->find(ANDROID_CONTROL_AE_MODE);
    if (entry.count == 1)
        aeMode = entry.data.u8[0];

    uint8_t flash_mode = ANDROID_FLASH_MODE_OFF;
    entry = settings->find(ANDROID_FLASH_MODE);
    if (entry.count == 1) {
        flash_mode = entry.data.u8[0];
    }

    mAeFlashMode = flash_mode;
    mAeMode = aeMode;

    // if aemode is *_flash, overide the flash mode of ANDROID_FLASH_MODE
    int flashMode;
    if (aeMode == ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH)
        flashMode = CAM_AE_FLASH_MODE_AUTO; // TODO: set always on for soc now
    else if (aeMode == ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH)
        flashMode = CAM_AE_FLASH_MODE_ON;
    else if (flash_mode  == ANDROID_FLASH_MODE_TORCH)
        flashMode = CAM_AE_FLASH_MODE_TORCH;
    else if (flash_mode  == ANDROID_FLASH_MODE_SINGLE)
        flashMode = CAM_AE_FLASH_MODE_SINGLE;
    else
        flashMode = CAM_AE_FLASH_MODE_OFF;

    // TODO: set always on for soc now
    /*if (flashMode == CAM_AE_FLASH_MODE_AUTO ||
        flashMode == CAM_AE_FLASH_MODE_SINGLE)
        flashMode = CAM_AE_FLASH_MODE_ON;*/

    if (flashMode == CAM_AE_FLASH_MODE_ON || flashMode == CAM_AE_FLASH_MODE_AUTO) {
        entry = settings->find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
        if (entry.count == 1) {
            if (!(entry.data.u8[0] == ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE
                && mAePreTrigger == ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START))
                mAePreTrigger = entry.data.u8[0];
        }
    }

    mAeState = ANDROID_CONTROL_AE_STATE_CONVERGED;
    int setToDrvFlMode = (flashMode == CAM_AE_FLASH_MODE_AUTO ||
        flashMode == CAM_AE_FLASH_MODE_SINGLE) ? CAM_AE_FLASH_MODE_ON : flashMode;
    if (flashMode == CAM_AE_FLASH_MODE_TORCH)
        setToDrvFlMode = CAM_AE_FLASH_MODE_TORCH;
    else if (flashMode == CAM_AE_FLASH_MODE_ON
        || (flashMode == CAM_AE_FLASH_MODE_AUTO)) {
        // assume SoC sensor has only torch flash mode, and for
        // ANDROID_CONTROL_CAPTURE_INTENT usecase, flash should keep
        // on until the jpeg image captured.
        if (mAePreTrigger == ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START) {
            if (flashMode == CAM_AE_FLASH_MODE_AUTO && mMeanLuma < 18.0f)
                setToDrvFlMode = CAM_AE_FLASH_MODE_TORCH;
            else if (flashMode == CAM_AE_FLASH_MODE_ON)
                setToDrvFlMode = CAM_AE_FLASH_MODE_TORCH;
            else
                setToDrvFlMode = CAM_AE_FLASH_MODE_OFF;
            mAeState = ANDROID_CONTROL_AE_STATE_PRECAPTURE;

            mAeTrigFrms++;
            // keep on precap for 10 frames to wait flash ae stable
            if (mAeTrigFrms > 10)
                mAeState = ANDROID_CONTROL_AE_STATE_CONVERGED;
            // keep flash on another 10 frames to make sure capture the
            // flashed frame
            if (mAeTrigFrms > 20) {
                setToDrvFlMode = CAM_AE_FLASH_MODE_OFF;
                mAeTrigFrms = 0;
                mAePreTrigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL;
                mAeState = ANDROID_CONTROL_AE_STATE_CONVERGED;
            }
        } else if (mAePreTrigger == ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL) {
            setToDrvFlMode = CAM_AE_FLASH_MODE_OFF;
            mAeTrigFrms = 0;
        }
        LOGD("aePreTrigger %d, mAeTrigFrms %d", mAePreTrigger, mAeTrigFrms);
    } else
        setToDrvFlMode = CAM_AE_FLASH_MODE_OFF;

    LOGD("%s:%d aePreTrigger %d, mAeTrigFrms %d, setToDrvFlMode %d",
         __FUNCTION__, __LINE__, mAePreTrigger, mAeTrigFrms, setToDrvFlMode);

    return setV4lFlashMode(setToDrvFlMode, 100, 500, 0);
}

int SocCamFlashCtrUnit::updateFlashResult(CameraMetadata *result)
{
    result->update(ANDROID_CONTROL_AE_MODE, &mAeMode, 1);
    result->update(ANDROID_CONTROL_AE_STATE, &mAeState, 1);
    result->update(ANDROID_FLASH_MODE, &mAeFlashMode, 1);

    uint8_t flashState = ANDROID_FLASH_STATE_READY;
    if (mV4lFlashMode == V4L2_FLASH_LED_MODE_FLASH ||
        mV4lFlashMode == V4L2_FLASH_LED_MODE_TORCH) {
        flashState = ANDROID_FLASH_STATE_FIRED;

        if (mAeMode >= ANDROID_CONTROL_AE_MODE_ON
            && mAeFlashMode == ANDROID_FLASH_MODE_OFF) {
           flashState = ANDROID_FLASH_STATE_PARTIAL;
        }
    }

    /* Using android.flash.mode == TORCH or SINGLE will always return FIRED.*/
    if (mAeFlashMode == ANDROID_FLASH_MODE_TORCH ||
        mAeFlashMode == ANDROID_FLASH_MODE_SINGLE) {
        ALOGD("%s:%d mAeFlashMode: %d, set flashState FIRED!", __FUNCTION__, __LINE__, mAeFlashMode);
        flashState = ANDROID_FLASH_STATE_FIRED;
    }
    //# ANDROID_METADATA_Dynamic android.flash.state done
    result->update(ANDROID_FLASH_STATE, &flashState, 1);

    return 0;
}

int SocCamFlashCtrUnit::setV4lFlashMode(int mode, int power, int timeout, int strobe)
{
    struct v4l2_control control;
    int fl_v4l_mode;

#define set_fl_contol_to_dev(control_id,val) \
        memset(&control, 0, sizeof(control)); \
        control.id = control_id; \
        control.value = val; \
        if (mFlSubdev.get()) { \
            if (pioctl(mFlSubdev->getFd(), VIDIOC_S_CTRL, &control, 0) < 0) { \
                LOGE(" set fl %s to %d failed", #control_id, val); \
                return -1; \
            } \
            LOGD("set fl %s to %d, success", #control_id, val); \
        } \

    if (mode == CAM_AE_FLASH_MODE_OFF)
        fl_v4l_mode = V4L2_FLASH_LED_MODE_NONE;
    else if (mode == CAM_AE_FLASH_MODE_ON)
        fl_v4l_mode = V4L2_FLASH_LED_MODE_FLASH;
    else if (mode == CAM_AE_FLASH_MODE_TORCH)
        fl_v4l_mode = V4L2_FLASH_LED_MODE_TORCH;
    else {
        LOGE(" set fl to mode  %d failed", mode);
        return -1;
    }

    if (mV4lFlashMode == fl_v4l_mode)
        return 0;

    if (fl_v4l_mode == V4L2_FLASH_LED_MODE_NONE) {
        set_fl_contol_to_dev(V4L2_CID_FLASH_LED_MODE, V4L2_FLASH_LED_MODE_NONE);
    } else if (fl_v4l_mode == V4L2_FLASH_LED_MODE_FLASH) {
        set_fl_contol_to_dev(V4L2_CID_FLASH_LED_MODE, V4L2_FLASH_LED_MODE_FLASH);
        set_fl_contol_to_dev(V4L2_CID_FLASH_TIMEOUT, timeout * 1000);
        // TODO: should query intensity range before setting
        /* set_fl_contol_to_dev(V4L2_CID_FLASH_INTENSITY, fl_intensity); */
        set_fl_contol_to_dev(strobe ? V4L2_CID_FLASH_STROBE : V4L2_CID_FLASH_STROBE_STOP, 0);
    } else if (fl_v4l_mode == V4L2_FLASH_LED_MODE_TORCH) {
        // TODO: should query intensity range before setting
        /* set_fl_contol_to_dev(V4L2_CID_FLASH_TORCH_INTENSITY, fl_intensity); */
        set_fl_contol_to_dev(V4L2_CID_FLASH_LED_MODE, V4L2_FLASH_LED_MODE_TORCH);
    } else {
        LOGE("setV4lFlashMode error fl mode %d", mode);
        return -1;
    }

    mV4lFlashMode = fl_v4l_mode;

    return 0;
}

RKISP2ControlUnit::RKISP2ControlUnit(RKISP2ImguUnit *thePU,
                         int cameraId,
                         RKISP2IStreamConfigProvider &aStreamCfgProv,
                         std::shared_ptr<MediaController> mc) :
        mRequestStatePool("CtrlReqState"),
        mCaptureUnitSettingsPool("CapUSettings"),
        mProcUnitSettingsPool("ProcUSettings"),
        mLatestRequestId(-1),
        mImguUnit(thePU),
        mCtrlLoop(nullptr),
        mEnable3A(true),
        mCameraId(cameraId),
        mMediaCtl(mc),
        mThreadRunning(false),
        mMessageQueue("CtrlUnitThread", static_cast<int>(MESSAGE_ID_MAX)),
        mMessageThread(nullptr),
        mStreamCfgProv(aStreamCfgProv),
        mRKISP2SettingsProcessor(nullptr),
        mMetadata(nullptr),
        mSensorSettingsDelay(0),
        mGainDelay(0),
        mLensSupported(false),
        mFlashSupported(false),
        mSofSequence(0),
        mShutterDoneReqId(-1),
        mSensorSubdev(nullptr),
        mSocCamFlashCtrUnit(nullptr),
        mStillCapSyncNeeded(0),
        mStillCapSyncState(STILL_CAP_SYNC_STATE_TO_ENGINE_IDLE),
        mFlushForUseCase(FLUSH_FOR_NOCHANGE)
{
    cl_result_callback_ops::metadata_result_callback = &sMetadatCb;
}

status_t
RKISP2ControlUnit::getDevicesPath()
{
    std::shared_ptr<MediaEntity> mediaEntity = nullptr;
    std::string entityName;
    const CameraHWInfo* camHwInfo = PlatformData::getCameraHWInfo();
    std::shared_ptr<V4L2Subdevice> subdev = nullptr;
    status_t status = OK;

    const struct SensorDriverDescriptor* sensorInfo = camHwInfo->getSensorDrvDes(mCameraId);
    // get lens device path
    if (!sensorInfo || sensorInfo->mModuleLensDevName == "") {
        LOGW("%s: No lens found", __FUNCTION__);
    } else {
        struct stat sb;
        int PathExists = stat(sensorInfo->mModuleLensDevName.c_str(), &sb);
        if (PathExists != 0) {
            LOGE("Error, could not find lens subdev %s !", entityName.c_str());
        } else {
            mDevPathsMap[KDevPathTypeLensNode] = sensorInfo->mModuleLensDevName;
        }
    }

    // get sensor device path
    camHwInfo->getSensorEntityName(mCameraId, entityName);
    if (entityName == "none") {
        LOGE("%s: No pixel_array found", __FUNCTION__);
        return UNKNOWN_ERROR;
    } else {
        status = mMediaCtl->getMediaEntity(mediaEntity, entityName.c_str());
        if (mediaEntity == nullptr || status != NO_ERROR) {
            LOGE("Could not retrieve media entity %s", entityName.c_str());
            return UNKNOWN_ERROR;
        }

        mediaEntity->getDevice((std::shared_ptr<V4L2DeviceBase>&) subdev);
        if (subdev.get()) {
            mDevPathsMap[KDevPathTypeSensorNode] = subdev->name();
            mSensorSubdev = subdev;
        }
    }

#if 0
    // get flashlight device path
    if (!sensorInfo || sensorInfo->mModuleFlashDevName == "") {
        LOGW("%s: No flashlight found", __FUNCTION__);
    } else {
        struct stat sb;
        int PathExists = stat(sensorInfo->mModuleFlashDevName.c_str(), &sb);
        if (PathExists != 0) {
            LOGE("Error, could not find flashlight subdev %s !", entityName.c_str());
        } else {
            mDevPathsMap[KDevPathTypeFlNode] = sensorInfo->mModuleFlashDevName;
        }
    }
#endif
    std::vector<media_link_desc> links;
    mediaEntity->getLinkDesc(links);
    if (links.size()) {
        struct media_pad_desc* pad = &links[0].sink;
        struct media_entity_desc entityDesc;
        mMediaCtl->findMediaEntityById(pad->entity, entityDesc);
        string name = entityDesc.name;
        // check linked to cif or isp
        if (name.find("cif") != std::string::npos)
            return OK;
    }

    // get isp subdevice path
    entityName = "rkisp-isp-subdev";
    status = mMediaCtl->getMediaEntity(mediaEntity, entityName.c_str());
    if (mediaEntity == nullptr || status != NO_ERROR) {
        LOGE("Could not retrieve media entity %s", entityName.c_str());
        return UNKNOWN_ERROR;
    }

    mediaEntity->getDevice((std::shared_ptr<V4L2DeviceBase>&) subdev);
    if (subdev.get())
        mDevPathsMap[KDevPathTypeIspDevNode] = subdev->name();

    // get isp input params device path
    entityName = "rkisp-input-params";
    status = mMediaCtl->getMediaEntity(mediaEntity, entityName.c_str());
    if (mediaEntity == nullptr || status != NO_ERROR) {
        LOGE("%s, Could not retrieve Media Entity %s", __FUNCTION__, entityName.c_str());
        return UNKNOWN_ERROR;
    }

    mediaEntity->getDevice((std::shared_ptr<V4L2DeviceBase>&) subdev);
    if (subdev.get())
        mDevPathsMap[KDevPathTypeIspInputParamsNode] = subdev->name();

    // get isp stats device path
    entityName = "rkisp-statistics";
    status = mMediaCtl->getMediaEntity(mediaEntity, entityName.c_str());
    if (mediaEntity == nullptr || status != NO_ERROR) {
        LOGE("%s, Could not retrieve Media Entity %s", __FUNCTION__, entityName.c_str());
        return UNKNOWN_ERROR;
    }

    mediaEntity->getDevice((std::shared_ptr<V4L2DeviceBase>&) subdev);
    if (subdev.get())
        mDevPathsMap[KDevPathTypeIspStatsNode] = subdev->name();

    return OK;
}

/**
 * initStaticMetadata
 *
 * Create CameraMetadata object to retrieve the static tags used in this class
 * we cache them as members so that we do not need to query CameraMetadata class
 * everytime we need them. This is more efficient since find() is not cheap
 */
status_t RKISP2ControlUnit::initStaticMetadata()
{
    //Initialize the CameraMetadata object with the static metadata tags
    camera_metadata_t* plainStaticMeta;
    plainStaticMeta = (camera_metadata_t*)PlatformData::getStaticMetadata(mCameraId);
    if (plainStaticMeta == nullptr) {
        LOGE("Failed to get camera %d StaticMetadata", mCameraId);
        return UNKNOWN_ERROR;
    }

    CameraMetadata staticMeta(plainStaticMeta);
    camera_metadata_entry entry;
    entry = staticMeta.find(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE);
    if (entry.count == 1) {
        LOGI("camera %d minimum focus distance:%f", mCameraId, entry.data.f[0]);
        mLensSupported = (entry.data.f[0] > 0) ? true : false;
        LOGI("Lens movement %s for camera id %d",
             mLensSupported ? "supported" : "NOT supported", mCameraId);
    }

    entry = staticMeta.find(ANDROID_FLASH_INFO_AVAILABLE);
    if (entry.count == 1) {
        mFlashSupported = (entry.data.u8[0] > 0) ? true : false;
        LOGI("Flash %s for camera id %d",
             mFlashSupported ? "supported" : "NOT supported", mCameraId);
    }
    staticMeta.release();

    const RKISP2CameraCapInfo *cap = getRKISP2CameraCapInfo(mCameraId);
    if (cap == nullptr) {
        LOGE("Failed to get cameraCapInfo");
        return UNKNOWN_ERROR;
    }
    mSensorSettingsDelay = MAX(cap->mExposureLag, cap->mGainLag);
    mGainDelay = cap->mGainLag;

    return NO_ERROR;
}

status_t
RKISP2ControlUnit::init()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = OK;
    const char *sensorName = nullptr;

    //Cache the static metadata values we are going to need in the capture unit
    if (initStaticMetadata() != NO_ERROR) {
        LOGE("Cannot initialize static metadata");
        return NO_INIT;
    }

    mMessageThread = std::unique_ptr<MessageThread>(new MessageThread(this, "CtrlUThread"));
    mMessageThread->run();

    const RKISP2CameraCapInfo *cap = getRKISP2CameraCapInfo(mCameraId);
    if (!cap) {
        LOGE("Not enough information for getting NVM data");
        return UNKNOWN_ERROR;
    } else {
        sensorName = cap->getSensorName();
    }

    // In the case: CAMERA_DUMP_RAW + no rawPath, disable 3a.
    // because isp is bypassed in this case
    // Note: only isp support rawPath, hal report the raw capability,
    // so the case "raw stream + no rawPath" shouln't exists
    if (cap->sensorType() == SENSOR_TYPE_RAW &&
        !(LogHelper::isDumpTypeEnable(CAMERA_DUMP_RAW) &&
          !PlatformData::getCameraHWInfo()->isIspSupportRawPath())) {
        mCtrlLoop = new RKISP2CtrlLoop(mCameraId);
        if (mCtrlLoop->init(sensorName, this) != NO_ERROR) {
            LOGE("Error initializing 3A control");
            return UNKNOWN_ERROR;
        }
        mImguUnit->setCtrlLoop(mCtrlLoop);
    } else {
        LOGW("No need 3A control, isSocSensor: %s, rawDump:%d",
             cap->sensorType() == SENSOR_TYPE_SOC ? "Yes" : "No",
             LogHelper::isDumpTypeEnable(CAMERA_DUMP_RAW));
        //return UNKNOWN_ERROR;
    }
    mRKISP2SettingsProcessor = new RKISP2SettingsProcessor(mCameraId);
    mRKISP2SettingsProcessor->init();

    mMetadata = new RKISP2Metadata(mCameraId);
    status = mMetadata->init();
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Error Initializing metadata");
        return UNKNOWN_ERROR;
    }

    /*
     * init the pools of Request State structs and CaptureUnit settings
     * and Processing Unit Settings
     */
    mRequestStatePool.init(MAX_REQUEST_IN_PROCESS_NUM, RKISP2RequestCtrlState::reset);
    mCaptureUnitSettingsPool.init(SETTINGS_POOL_SIZE + 2);
    mProcUnitSettingsPool.init(SETTINGS_POOL_SIZE, RKISP2ProcUnitSettings::reset);

    mSettingsHistory.clear();

    /* Set digi gain support */
    bool supportDigiGain = false;
    if (cap)
        supportDigiGain = cap->digiGainOnSensor();

    getDevicesPath();

    const CameraHWInfo* camHwInfo = PlatformData::getCameraHWInfo();
    const struct SensorDriverDescriptor* sensorInfo = camHwInfo->getSensorDrvDes(mCameraId);
    if (/*cap->sensorType() == SENSOR_TYPE_SOC &&*/
        sensorInfo->mFlashNum > 0 &&
        mFlashSupported) {
        mSocCamFlashCtrUnit = std::unique_ptr<SocCamFlashCtrUnit>(
                              new SocCamFlashCtrUnit(
                              // TODO: support only one flash for SoC now
                              sensorInfo->mModuleFlashDevName[0].c_str(),
                              mCameraId));
    }

    return status;
}

/**
 * reset
 *
 * This method is called by the SharedPoolItem when the item is recycled
 * At this stage we can cleanup before recycling the struct.
 * In this case we reset the TracingSP of the capture unit settings and buffers
 * to remove this reference. Other references may be still alive.
 */
void RKISP2RequestCtrlState::reset(RKISP2RequestCtrlState* me)
{
    if (CC_LIKELY(me != nullptr)) {
        me->captureSettings.reset();
        me->processingSettings.reset();
        me->graphConfig.reset();
    } else {
        LOGE("Trying to reset a null CtrlState structure !! - BUG ");
    }
}

void RKISP2RequestCtrlState::init(Camera3Request *req,
                                         std::shared_ptr<RKISP2GraphConfig> aGraphConfig)
{
    request = req;
    graphConfig = aGraphConfig;
    if (CC_LIKELY(captureSettings)) {
        captureSettings->aeRegion.init(0);
        captureSettings->makernote.data = nullptr;
        captureSettings->makernote.size = 0;
    } else {
        LOGE(" Failed to init Ctrl State struct: no capture settings!! - BUG");
        return;
    }
    if (CC_LIKELY(processingSettings.get() != nullptr)) {
        processingSettings->captureSettings = captureSettings;
        processingSettings->graphConfig = aGraphConfig;
        processingSettings->request = req;
    } else {
        LOGE(" Failed to init Ctrl State: no processing settings!! - BUG");
        return;
    }
    ctrlUnitResult = request->getPartialResultBuffer(CONTROL_UNIT_PARTIAL_RESULT);
    shutterDone = false;
    intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
    if (CC_UNLIKELY(ctrlUnitResult == nullptr)) {
        LOGE("no partial result buffer - BUG");
        return;
    }

    mClMetaReceived = false;
    mShutterMetaReceived = false;
    mImgProcessDone = false;

    /**
     * Apparently we need to have this tags in the results
     */
    const CameraMetadata* settings = request->getSettings();

    if (CC_UNLIKELY(settings == nullptr)) {
        LOGE("no settings in request - BUG");
        return;
    }

    int64_t id = request->getId();
    camera_metadata_ro_entry entry;
    entry = settings->find(ANDROID_REQUEST_ID);
    if (entry.count == 1) {
        ctrlUnitResult->update(ANDROID_REQUEST_ID, (int *)&id,
                entry.count);
    }
    ctrlUnitResult->update(ANDROID_SYNC_FRAME_NUMBER,
                          &id, 1);

    entry = settings->find(ANDROID_CONTROL_CAPTURE_INTENT);
    if (entry.count == 1) {
        intent = entry.data.u8[0];
    }
    LOGI("%s:%d: request id(%lld), capture_intent(%d)", __FUNCTION__, __LINE__, id, intent);
    ctrlUnitResult->update(ANDROID_CONTROL_CAPTURE_INTENT, entry.data.u8,
                                                           entry.count);
}

RKISP2ControlUnit::~RKISP2ControlUnit()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    mSettingsHistory.clear();

    requestExitAndWait();

    if (mMessageThread != nullptr) {
        mMessageThread.reset();
        mMessageThread = nullptr;
    }

    if (mRKISP2SettingsProcessor) {
        delete mRKISP2SettingsProcessor;
        mRKISP2SettingsProcessor = nullptr;
    }

    if (mCtrlLoop) {
        mCtrlLoop->deinit();
        delete mCtrlLoop;
        mCtrlLoop = nullptr;
    }

    delete mMetadata;
    mMetadata = nullptr;

}

status_t
RKISP2ControlUnit::configStreams(std::vector<camera3_stream_t*> &activeStreams, bool configChanged)
{
    PERFORMANCE_ATRACE_NAME("RKISP2ControlUnit::configStreams");
    LOGI("@%s %d: configChanged :%d", __FUNCTION__, __LINE__, configChanged);
    status_t status = OK;
    if(configChanged) {
        // this will be necessary when configStream called twice without calling
        // destruct function which called in the close camera stack
        mLatestRequestId = -1;
        mWaitingForCapture.clear();
        mSettingsHistory.clear();

        struct rkisp_cl_prepare_params_s prepareParams;

        memset(&prepareParams, 0, sizeof(struct rkisp_cl_prepare_params_s));
        prepareParams.staticMeta = PlatformData::getStaticMetadata(mCameraId);
        if (prepareParams.staticMeta == nullptr) {
            LOGE("Failed to get camera %d StaticMetadata for CL", mCameraId);
            return UNKNOWN_ERROR;
        }

        //start 3a when config video stream done
        for (auto &it : mDevPathsMap) {
            switch (it.first) {
            case KDevPathTypeIspDevNode:
                prepareParams.isp_sd_node_path = it.second.c_str();
                break;
            case KDevPathTypeIspStatsNode:
                prepareParams.isp_vd_stats_path = it.second.c_str();
                break;
            case KDevPathTypeIspInputParamsNode:
                prepareParams.isp_vd_params_path = it.second.c_str();
                break;
            case KDevPathTypeSensorNode:
                prepareParams.sensor_sd_node_path = it.second.c_str();
                break;
            case KDevPathTypeLensNode:
                if (mLensSupported)
                    prepareParams.lens_sd_node_path = it.second.c_str();
                break;
            // deprecated
            case KDevPathTypeFlNode:
                if (mFlashSupported)
                    prepareParams.flashlight_sd_node_path[0] = it.second.c_str();
                break;
            default:
                continue;
            }
        }

        const CameraHWInfo* camHwInfo = PlatformData::getCameraHWInfo();
        const struct SensorDriverDescriptor* sensorInfo = camHwInfo->getSensorDrvDes(mCameraId);

        if (mFlashSupported) {
            for (int i = 0; i < sensorInfo->mFlashNum; i++) {
                prepareParams.flashlight_sd_node_path[i] =
                    sensorInfo->mModuleFlashDevName[i].c_str();
            }
        }

        mEnable3A = true;
        for (auto it = activeStreams.begin(); it != activeStreams.end(); ++it) {
            prepareParams.width = (*it)->width;
            prepareParams.height = (*it)->height;
            LOGD("@%s : mEnable3A :%d,  prepareParams.width*height(%dx%d).", __FUNCTION__,
                  mEnable3A, prepareParams.width, prepareParams.height);

            if((*it)->format == HAL_PIXEL_FORMAT_RAW_OPAQUE &&
               !PlatformData::getCameraHWInfo()->isIspSupportRawPath()) {
                mEnable3A = false;
                break;
            }
        }
        LOGD("@%s : mEnable3A :%d", __FUNCTION__, mEnable3A);

        const RKISP2CameraCapInfo *cap = getRKISP2CameraCapInfo(mCameraId);
        prepareParams.work_mode = cap->getAiqWorkingMode();

        if (mCtrlLoop && mEnable3A ) {
            status = mCtrlLoop->start(prepareParams);
            if (CC_UNLIKELY(status != OK)) {
                LOGE("Failed to start 3a control loop!");
                return status;
            }
        }
    }

    return NO_ERROR;
}

status_t
RKISP2ControlUnit::requestExitAndWait()
{
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    status_t status = mMessageQueue.send(&msg, MESSAGE_ID_EXIT);
    status |= mMessageThread->requestExitAndWait();
    return status;
}

status_t
RKISP2ControlUnit::handleMessageExit()
{
    mThreadRunning = false;
    return NO_ERROR;
}

/**
 * acquireRequestStateStruct
 *
 * acquire a free request control state structure.
 * Since this structure contains also a capture settings item that are also
 * stored in a pool we need to acquire one of those as well.
 *
 */
status_t
RKISP2ControlUnit::acquireRequestStateStruct(std::shared_ptr<RKISP2RequestCtrlState>& state)
{
    status_t status = NO_ERROR;
    status = mRequestStatePool.acquireItem(state);
    if (status != NO_ERROR) {
        LOGE("Failed to acquire free request state struct - BUG");
        /*
         * This should not happen since AAL is holding clients to send more
         * requests than we can take
         */
        return UNKNOWN_ERROR;
    }

    status = mCaptureUnitSettingsPool.acquireItem(state->captureSettings);
    if (status != NO_ERROR) {
        LOGE("Failed to acquire free CapU settings  struct - BUG");
        /*
         * This should not happen since AAL is holding clients to send more
         * requests than we can take
         */
        return UNKNOWN_ERROR;
    }

    // set a unique ID for the settings
    state->captureSettings->settingsIdentifier = systemTime();

    status = mProcUnitSettingsPool.acquireItem(state->processingSettings);
    if (status != NO_ERROR) {
        LOGE("Failed to acquire free ProcU settings  struct - BUG");
        /*
         * This should not happen since AAL is holding clients to send more
         * requests than we can take
         */
        return UNKNOWN_ERROR;
    }
    return OK;
}

/**
 * processRequest
 *
 * Acquire the control structure to keep the state of the request in the control
 * unit and send the message to be handled in the internal message thread.
 */
status_t
RKISP2ControlUnit::processRequest(Camera3Request* request,
                            std::shared_ptr<RKISP2GraphConfig> graphConfig)
{
    Message msg;
    status_t status = NO_ERROR;
    std::shared_ptr<RKISP2RequestCtrlState> state;

    status = acquireRequestStateStruct(state);
    if (CC_UNLIKELY(status != OK) || CC_UNLIKELY(state.get() == nullptr)) {
        return status;  // error log already done in the helper method
    }

    state->init(request, graphConfig);

    msg.id = MESSAGE_ID_NEW_REQUEST;
    msg.state = state;
    status = mMessageQueue.send(&msg);
    return status;
}

status_t
RKISP2ControlUnit::handleNewRequest(Message &msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    status_t status = NO_ERROR;
    std::shared_ptr<RKISP2RequestCtrlState> reqState = msg.state;

    /**
     * PHASE 1: Process the settings
     * In this phase we analyze the request's metadata settings and convert them
     * into either:
     *  - input parameters for 3A algorithms
     *  - parameters used for SoC sensors
     *  - Capture Unit settings
     *  - Processing Unit settings
     */
    const CameraMetadata *reqSettings = reqState->request->getSettings();

    if (reqSettings == nullptr) {
        LOGE("no settings in request - BUG");
        return UNKNOWN_ERROR;
    }

    status = mRKISP2SettingsProcessor->processRequestSettings(*reqSettings, *reqState);
    if (status != NO_ERROR) {
        LOGE("Could not process all settings, reporting request as invalid");
    }

    status = processRequestForCapture(reqState);
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to process req %d for capture [%d]",
        reqState->request->getId(), status);
        // TODO: handle error !
    }

    return status;
}

status_t
RKISP2ControlUnit::processSoCSettings(const CameraMetadata *settings)
{
	uint8_t reqTemplate;
	camera_metadata_ro_entry entry;

	//# ANDROID_METADATA_Dynamic android.control.captureIntent copied
	entry = settings->find(ANDROID_CONTROL_CAPTURE_INTENT);
	if (entry.count == 1) {
		reqTemplate = entry.data.u8[0];
		LOGD("%s:%d reqTemplate(%d)!\n ", __FUNCTION__, __LINE__, reqTemplate);
	}

	// fill target fps range, it needs to be proper in results anyway
	entry = settings->find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
	if (entry.count == 2) {
	   int32_t minFps = entry.data.i32[0];
       int32_t maxFps = entry.data.i32[1];

       // set to driver
       LOGD("%s:%d enter: minFps= %d maxFps = %d!\n ", __FUNCTION__, __LINE__, minFps, maxFps);
		if(reqTemplate != ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT) {
		    //case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
		    //case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
			if (mSensorSubdev.get())
				mSensorSubdev->setFramerate(0, maxFps);
       }
    }

    if (mSocCamFlashCtrUnit.get()) {
        int ret = mSocCamFlashCtrUnit->setFlashSettings(settings);
        if (ret < 0)
            LOGE("%s:%d set flash settings failed", __FUNCTION__, __LINE__);
    }

    return OK;
}

/**
 * processRequestForCapture
 *
 * Run 3A algorithms and send the results to capture unit for capture
 *
 * This is the second phase in the request processing flow.
 *
 * The request settings have been processed in the first phase
 *
 * If this step is successful the request will be moved to the
 * mWaitingForCapture waiting for the pixel buffers.
 */
status_t
RKISP2ControlUnit::processRequestForCapture(std::shared_ptr<RKISP2RequestCtrlState> &reqState)
{
    status_t status = NO_ERROR;
    if (CC_UNLIKELY(reqState.get() == nullptr)) {
        LOGE("Invalid parameters passed- request not captured - BUG");
        return BAD_VALUE;
    }
    reqState->request->dumpSetting();

    if (CC_UNLIKELY(reqState->captureSettings.get() == nullptr)) {
        LOGE("capture Settings not given - BUG");
        return BAD_VALUE;
    }

    /* Write the dump flag into capture settings, so that the PAL dump can be
     * done all the way down at PgParamAdaptor. For the time being, only dump
     * during jpeg captures.
     */
    reqState->processingSettings->dump =
            LogHelper::isDumpTypeEnable(CAMERA_DUMP_RAW) &&
            reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB) > 0;
    // dump the PAL run from ISA also
    reqState->captureSettings->dump = reqState->processingSettings->dump;

    int reqId = reqState->request->getId();

    /**
     * Move the request to the vector mWaitingForCapture
     */
    mWaitingForCapture.insert(std::make_pair(reqId, reqState));

    mLatestRequestId = reqId;

    int jpegBufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB);
    int implDefinedBufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
    int yuv888BufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_YCbCr_420_888);
    LOGD("@%s jpegs:%d impl defined:%d yuv888:%d inputbufs:%d req id %d",
         __FUNCTION__,
         jpegBufCount,
         implDefinedBufCount,
         yuv888BufCount,
         reqState->request->getNumberInputBufs(),
         reqState->request->getId());

    if (jpegBufCount > 0) {
        // NOTE: Makernote should be get after isp_bxt_run()
        // NOTE: makernote.data deleted in JpegEncodeTask::handleMakernote()
        /* TODO */
        /* const unsigned mknSize = MAKERNOTE_SECTION1_SIZE + MAKERNOTE_SECTION2_SIZE; */
        /* MakernoteData mkn = {nullptr, mknSize}; */
        /* mkn.data = new char[mknSize]; */
        /* m3aWrapper->getMakerNote(ia_mkn_trg_section_2, mkn); */

        /* reqState->captureSettings->makernote = mkn; */
    } else {
        // No JPEG buffers in request. Reset MKN info, just in case.
        reqState->captureSettings->makernote.data = nullptr;
        reqState->captureSettings->makernote.size = 0;
    }

    // if this request is a reprocess request, no need to setFrameParam to CL.
    if (reqState->request->getNumberInputBufs() == 0) {
        if (mCtrlLoop && mEnable3A) {
            const CameraMetadata *settings = reqState->request->getSettings();
            rkisp_cl_frame_metadata_s frame_metas;
            /* frame_metas.metas = settings->getAndLock(); */
            /* frame_metas.id = reqId; */

            CameraMetadata tempCamMeta = *settings;

            camera_metadata_ro_entry entry =
                settings->find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
            if (entry.count == 1) {
                if (entry.data.u8[0] == ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START) {
                    mStillCapSyncState = STILL_CAP_SYNC_STATE_TO_ENGINE_PRECAP;
                }
            }

            if(jpegBufCount == 0) {
                uint8_t intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
                tempCamMeta.update(ANDROID_CONTROL_CAPTURE_INTENT, &intent, 1);
            } else {
                if (mStillCapSyncNeeded) {
                    if (mStillCapSyncState == STILL_CAP_SYNC_STATE_TO_ENGINE_IDLE) {
                        LOGD("forcely trigger ae precapture");
                        uint8_t precap = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START;
                        tempCamMeta.update(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER, &precap, 1);
                        mStillCapSyncState = STILL_CAP_SYNC_STATE_TO_ENGINE_PRECAP;
                    }
                    if (mStillCapSyncState == STILL_CAP_SYNC_STATE_TO_ENGINE_PRECAP) {
                        uint8_t stillCapSync = RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCSTART;
                        tempCamMeta.update(RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD, &stillCapSync, 1);
                        mStillCapSyncState = STILL_CAP_SYNC_STATE_WAITING_ENGINE_DONE;
                    } else
                        LOGW("already in stillcap_sync state %d",
                             mStillCapSyncState);
                }
            }

            // TuningServer *pserver = TuningServer::GetInstance();
            // if (pserver && pserver->isTuningMode()) {
            //     pserver->set_tuning_params(tempCamMeta);
            // }
            frame_metas.metas = tempCamMeta.getAndLock();
            frame_metas.id = reqId;

            status = mCtrlLoop->setFrameParams(&frame_metas);
            if (status != OK)
                LOGE("CtrlLoop setFrameParams error");

            /* status = settings->unlock(frame_metas.metas); */
            status = tempCamMeta.unlock(frame_metas.metas);
            if (status != OK) {
                LOGE("unlock frame frame_metas failed");
                return UNKNOWN_ERROR;
            }

            int max_counts = 500;
            while (mStillCapSyncState == STILL_CAP_SYNC_STATE_WAITING_ENGINE_DONE &&
                   max_counts > 0) {
                LOGD("waiting for stillcap_sync_done");
                usleep(10*1000);
                max_counts--;
            }

            if (max_counts == 0) {
                mStillCapSyncState = STILL_CAP_SYNC_STATE_FROM_ENGINE_DONE;
                LOGW("waiting for stillcap_sync_done timeout!");
            }

            if (mStillCapSyncState == STILL_CAP_SYNC_STATE_FROM_ENGINE_DONE) {
                mStillCapSyncState = STILL_CAP_SYNC_STATE_WATING_JPEG_FRAME;
            }

            if (mSocCamFlashCtrUnit.get()) {
                int ret = mSocCamFlashCtrUnit->setFlashSettings(settings);
                if (ret < 0)
                    LOGE("%s:%d set flash settings failed", __FUNCTION__, __LINE__);
            }

            LOGD("%s:%d, stillcap_sync_state %d",
                 __FUNCTION__, __LINE__, mStillCapSyncState);
        } else {
            // set SoC sensor's params
            const CameraMetadata *settings = reqState->request->getSettings();
            processSoCSettings(settings);
        }
    } else {
        LOGD("@%s %d: reprocess request:%d, no need setFrameParam", __FUNCTION__, __LINE__, reqId);
        reqState->mClMetaReceived = true;
        /* Result as reprocessing request: The HAL can expect that a reprocessing request is a copy */
        /* of one of the output results with minor allowed setting changes. */
        reqState->ctrlUnitResult->append(*reqState->request->getSettings());
    }

    /*TODO, needn't this anymore ? zyc*/
    status = completeProcessing(reqState);
    if (status != OK)
        LOGE("Cannot complete the buffer processing - fix the bug!");

    return status;
}

status_t RKISP2ControlUnit::fillMetadata(std::shared_ptr<RKISP2RequestCtrlState> &reqState)
{
    /**
     * Apparently we need to have this tags in the results
     */
    const CameraMetadata* settings = reqState->request->getSettings();
    CameraMetadata* ctrlUnitResult = reqState->ctrlUnitResult;

    if (CC_UNLIKELY(settings == nullptr)) {
        LOGE("no settings in request - BUG");
        return UNKNOWN_ERROR;
    }

    camera_metadata_ro_entry entry;
    entry = settings->find(ANDROID_CONTROL_MODE);
    if (entry.count == 1) {
        ctrlUnitResult->update(ANDROID_CONTROL_MODE, entry.data.u8, entry.count);
    }
    //# ANDROID_METADATA_Dynamic android.control.videoStabilizationMode copied
    entry = settings->find(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE);
    if (entry.count == 1) {
        ctrlUnitResult->update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, entry.data.u8, entry.count);
    }
    //# ANDROID_METADATA_Dynamic android.lens.opticalStabilizationMode copied
    entry = settings->find(ANDROID_LENS_OPTICAL_STABILIZATION_MODE);
    if (entry.count == 1) {
        ctrlUnitResult->update(ANDROID_LENS_OPTICAL_STABILIZATION_MODE, entry.data.u8, entry.count);
    }
    //# ANDROID_METADATA_Dynamic android.control.effectMode done
    entry = settings->find(ANDROID_CONTROL_EFFECT_MODE);
    if (entry.count == 1) {
        ctrlUnitResult->update(ANDROID_CONTROL_EFFECT_MODE, entry.data.u8, entry.count);
    }
    //# ANDROID_METADATA_Dynamic android.noiseReduction.mode done
    entry = settings->find(ANDROID_NOISE_REDUCTION_MODE);
    if (entry.count == 1) {
        ctrlUnitResult->update(ANDROID_NOISE_REDUCTION_MODE, entry.data.u8, entry.count);
    }
    //# ANDROID_METADATA_Dynamic android.edge.mode done
    entry = settings->find(ANDROID_EDGE_MODE);
    if (entry.count == 1) {
        ctrlUnitResult->update(ANDROID_EDGE_MODE, entry.data.u8, entry.count);
    }

    /**
     * We don't have AF, so just update metadata now
     */
    // return 0.0f for the fixed-focus
    if (!mLensSupported) {
        float focusDistance = 0.0f;
        reqState->ctrlUnitResult->update(ANDROID_LENS_FOCUS_DISTANCE,
                                         &focusDistance, 1);
        // framework says it can't be off mode for zsl,
        // so we report EDOF for fixed focus
        // TODO: need to judge if the request is ZSL ?
        /* uint8_t afMode = ANDROID_CONTROL_AF_MODE_EDOF; */
        uint8_t afMode = ANDROID_CONTROL_AF_MODE_OFF;
        ctrlUnitResult->update(ANDROID_CONTROL_AF_MODE, &afMode, 1);
        uint8_t afTrigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
        ctrlUnitResult->update(ANDROID_CONTROL_AF_TRIGGER, &afTrigger, 1);

        uint8_t afState = ANDROID_CONTROL_AF_STATE_INACTIVE;
        ctrlUnitResult->update(ANDROID_CONTROL_AF_STATE, &afState, 1);
    }

    bool flash_available = false;
    uint8_t flash_mode = ANDROID_FLASH_MODE_OFF;
    mRKISP2SettingsProcessor->getStaticMetadataCache().getFlashInfoAvailable(flash_available);
    if (!flash_available) {
        reqState->ctrlUnitResult->update(ANDROID_FLASH_MODE,
                                         &flash_mode, 1);
        uint8_t flashState = ANDROID_FLASH_STATE_UNAVAILABLE;
        //# ANDROID_METADATA_Dynamic android.flash.state done
        reqState->ctrlUnitResult->update(ANDROID_FLASH_STATE, &flashState, 1);
    }

    mMetadata->writeJpegMetadata(*reqState);
    uint8_t pipelineDepth;
    mRKISP2SettingsProcessor->getStaticMetadataCache().getPipelineDepth(pipelineDepth);
    //# ANDROID_METADATA_Dynamic android.request.pipelineDepth done
    reqState->ctrlUnitResult->update(ANDROID_REQUEST_PIPELINE_DEPTH,
                                     &pipelineDepth, 1);
    // for soc camera
    if (!mCtrlLoop || !mEnable3A) {
        uint8_t awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
        ctrlUnitResult->update(ANDROID_CONTROL_AWB_MODE, &awbMode, 1);
        uint8_t awbState = ANDROID_CONTROL_AWB_STATE_CONVERGED;
        ctrlUnitResult->update(ANDROID_CONTROL_AWB_STATE, &awbState, 1);
        if (mSocCamFlashCtrUnit.get()) {
            mSocCamFlashCtrUnit->updateFlashResult(ctrlUnitResult);
        } else {
            uint8_t aeMode = ANDROID_CONTROL_AE_MODE_ON;
            ctrlUnitResult->update(ANDROID_CONTROL_AE_MODE, &aeMode, 1);
            uint8_t aeState = ANDROID_CONTROL_AE_STATE_CONVERGED;
            ctrlUnitResult->update(ANDROID_CONTROL_AE_STATE, &aeState, 1);
        }
        reqState->mClMetaReceived = true;
    } else {
        if (mSocCamFlashCtrUnit.get()) {
            mSocCamFlashCtrUnit->updateFlashResult(ctrlUnitResult);
        }
    }
    return OK;
}

status_t
RKISP2ControlUnit::handleNewRequestDone(Message &msg)
{
    status_t status = OK;
    std::shared_ptr<RKISP2RequestCtrlState> reqState = nullptr;
    int reqId = msg.requestId;

    std::map<int, std::shared_ptr<RKISP2RequestCtrlState>>::iterator it =
                                    mWaitingForCapture.find(reqId);
    if (it == mWaitingForCapture.end()) {
        LOGE("Unexpected request done event received for request %d - Fix the bug", reqId);
        return UNKNOWN_ERROR;
    }

    reqState = it->second;
    if (CC_UNLIKELY(reqState.get() == nullptr || reqState->request == nullptr)) {
        LOGE("No valid state or settings for request Id = %d"
             "- Fix the bug!", reqId);
        return UNKNOWN_ERROR;
    }

    reqState->mImgProcessDone = true;
    Camera3Request* request = reqState->request;
    // when deviceError, should not wait for meta and metadataDone an error index
    if ((!reqState->mClMetaReceived) && !request->getError())
        return OK;

    request->mCallback->metadataDone(request, request->getError() ? -1 : CONTROL_UNIT_PARTIAL_RESULT);
    /*
     * Remove the request from Q once we have received all pixel data buffers
     * we expect from ISA. Query the graph config for that.
     */
    mWaitingForCapture.erase(reqId);
    return status;
}

/**
 * completeProcessing
 *
 * Forward the pixel buffer to the Processing Unit to complete the processing.
 * If all the buffers from Capture Unit have arrived then:
 * - it updates the metadata
 * - it removes the request from the vector mWaitingForCapture.
 *
 * The metadata update is now transferred to the ProcessingUnit.
 * This is done only on arrival of the last pixel data buffer. RKISP2ControlUnit still
 * keeps the state, so it is responsible for triggering the update.
 */
status_t
RKISP2ControlUnit::completeProcessing(std::shared_ptr<RKISP2RequestCtrlState> &reqState)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int reqId = reqState->request->getId();

    if (CC_LIKELY((reqState->request != nullptr) &&
                  (reqState->captureSettings.get() != nullptr))) {

        /* TODO: cleanup
         * This struct copy from state is only needed for JPEG creation.
         * Ideally we should directly write inside members of processingSettings
         * whatever settings are needed for Processing Unit.
         * This should be moved to any of the processXXXSettings.
         */

        fillMetadata(reqState);
        mImguUnit->completeRequest(reqState->processingSettings, true);
    } else {
        LOGE("request or captureSetting is nullptr - Fix the bug!");
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t
RKISP2ControlUnit::handleNewShutter(Message &msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    std::shared_ptr<RKISP2RequestCtrlState> reqState = nullptr;
    int reqId = msg.data.shutter.requestId;

    //check whether this reqId has been shutter done
    if (reqId <= mShutterDoneReqId)
        return OK;

    std::map<int, std::shared_ptr<RKISP2RequestCtrlState>>::iterator it =
                                    mWaitingForCapture.find(reqId);
    if (it == mWaitingForCapture.end()) {
        LOGE("Unexpected shutter event received for request %d - Fix the bug", reqId);
        return UNKNOWN_ERROR;
    }

    reqState = it->second;
    if (CC_UNLIKELY(reqState.get() == nullptr || reqState->captureSettings.get() == nullptr)) {
        LOGE("No valid state or settings for request Id = %d"
             "- Fix the bug!", reqId);
        return UNKNOWN_ERROR;
    }

    const CameraMetadata* metaData = reqState->request->getSettings();
    if (metaData == nullptr) {
        LOGE("Metadata should not be nullptr. Fix the bug!");
        return UNKNOWN_ERROR;
    }

    int jpegBufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB);
    if (jpegBufCount &&
        (mStillCapSyncState == STILL_CAP_SYNC_STATE_WATING_JPEG_FRAME)) {
        mStillCapSyncState = STILL_CAP_SYNC_STATE_JPEG_FRAME_DONE;

        status_t status = OK;
        const CameraMetadata *settings = reqState->request->getSettings();
        rkisp_cl_frame_metadata_s frame_metas;
        CameraMetadata tempCamMeta = *settings;
        uint8_t stillCapSyncEnd = RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCEND;
        tempCamMeta.update(RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD, &stillCapSyncEnd, 1);
        mStillCapSyncState = STILL_CAP_SYNC_STATE_TO_ENGINE_IDLE;

        frame_metas.metas = tempCamMeta.getAndLock();
        frame_metas.id = -1;
        status = mCtrlLoop->setFrameParams(&frame_metas);
        if (status != OK)
            LOGE("CtrlLoop setFrameParams error");

        /* status = settings->unlock(frame_metas.metas); */
        status = tempCamMeta.unlock(frame_metas.metas);
        if (status != OK) {
            LOGE("unlock frame frame_metas failed");
            return UNKNOWN_ERROR;
        }
        LOGD("%s:%d, stillcap_sync_state %d",
             __FUNCTION__, __LINE__, mStillCapSyncState);
    }

    int64_t ts = msg.data.shutter.tv_sec * 1000000000; // seconds to nanoseconds
    ts += msg.data.shutter.tv_usec * 1000; // microseconds to nanoseconds

    //# ANDROID_METADATA_Dynamic android.sensor.timestamp done
    reqState->ctrlUnitResult->update(ANDROID_SENSOR_TIMESTAMP, &ts, 1);
    reqState->mShutterMetaReceived = true;
    if(reqState->mClMetaReceived) {
        mMetadata->writeRestMetadata(*reqState);
        reqState->request->notifyFinalmetaFilled();
    }
    reqState->request->mCallback->shutterDone(reqState->request, ts);
    reqState->shutterDone = true;
    reqState->captureSettings->timestamp = ts;
    mShutterDoneReqId = reqId;

    return NO_ERROR;
}

status_t
RKISP2ControlUnit::flush(int configChanged)
{
    PERFORMANCE_ATRACE_NAME("RKISP2ControlUnit::flush");
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    msg.configChanged = configChanged;
    mMessageQueue.remove(MESSAGE_ID_NEW_REQUEST);
    mMessageQueue.remove(MESSAGE_ID_NEW_SHUTTER);
    mMessageQueue.remove(MESSAGE_ID_NEW_REQUEST_DONE);
    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t
RKISP2ControlUnit::handleMessageFlush(Message &msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to stop 3a control loop!");
    }
    mFlushForUseCase = msg.configChanged;
    if(msg.configChanged && mCtrlLoop && mEnable3A) {
        if (mStillCapSyncNeeded &&
            mStillCapSyncState != STILL_CAP_SYNC_STATE_TO_ENGINE_PRECAP &&
            mFlushForUseCase == FLUSH_FOR_STILLCAP) {
            rkisp_cl_frame_metadata_s frame_metas;
            // force precap
            uint8_t precap = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START;
            mLatestCamMeta.update(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER, &precap, 1);
            frame_metas.metas = mLatestCamMeta.getAndLock();
            frame_metas.id = -1;
            status = mCtrlLoop->setFrameParams(&frame_metas);
            if (status != OK)
                LOGE("CtrlLoop setFrameParams error");

            status = mLatestCamMeta.unlock(frame_metas.metas);
            if (status != OK) {
                LOGE("unlock frame frame_metas failed");
                return UNKNOWN_ERROR;
            }
            mStillCapSyncState = STILL_CAP_SYNC_STATE_FORCE_TO_ENGINE_PRECAP;
            // wait precap 3A done
            while (mStillCapSyncState != STILL_CAP_SYNC_STATE_FORCE_PRECAP_DONE) {
                LOGD("%d:wait forceprecap done...", __LINE__);
                usleep(10*1000);
            }
            mStillCapSyncState = STILL_CAP_SYNC_STATE_TO_ENGINE_PRECAP;
        }
    }

    mImguUnit->flush();

    mWaitingForCapture.clear();
    mSettingsHistory.clear();

    return NO_ERROR;
}

void
RKISP2ControlUnit::messageThreadLoop()
{
    LOGD("@%s - Start", __FUNCTION__);

    mThreadRunning = true;
    while (mThreadRunning) {
        status_t status = NO_ERROR;

        PERFORMANCE_ATRACE_BEGIN("CtlU-PollMsg");
        Message msg;
        mMessageQueue.receive(&msg);
        PERFORMANCE_ATRACE_END();

        PERFORMANCE_ATRACE_NAME_SNPRINTF("CtlU-%s", ENUM2STR(CtlUMsg_stringEnum, msg.id));
        PERFORMANCE_HAL_ATRACE_PARAM1("msg", msg.id);
        LOGD("@%s, receive message id:%d", __FUNCTION__, msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;
        case MESSAGE_ID_NEW_REQUEST:
            status = handleNewRequest(msg);
            break;
        case MESSAGE_ID_NEW_SHUTTER:
            status = handleNewShutter(msg);
            break;
        case MESSAGE_ID_NEW_REQUEST_DONE:
            status = handleNewRequestDone(msg);
            break;
        case MESSAGE_ID_METADATA_RECEIVED:
            status = handleMetadataReceived(msg);
            break;
        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush(msg);
            break;
        default:
            LOGE("ERROR Unknown message %d", msg.id);
            status = BAD_VALUE;
            break;
        }
        if (status != NO_ERROR)
            LOGE("error %d in handling message: %d", status,
                    static_cast<int>(msg.id));
        LOGD("@%s, finish message id:%d", __FUNCTION__, msg.id);
        mMessageQueue.reply(msg.id, status);
        PERFORMANCE_ATRACE_END();
    }

    LOGD("%s: Exit", __FUNCTION__);
}

bool
RKISP2ControlUnit::notifyCaptureEvent(CaptureMessage *captureMsg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (captureMsg == nullptr) {
        return false;
    }

    if (captureMsg->id == CAPTURE_MESSAGE_ID_ERROR) {
        // handle capture error
        return true;
    }

    Message msg;
    switch (captureMsg->data.event.type) {
        case CAPTURE_EVENT_SHUTTER:
            msg.id = MESSAGE_ID_NEW_SHUTTER;
            msg.data.shutter.requestId = captureMsg->data.event.reqId;
            msg.data.shutter.tv_sec = captureMsg->data.event.timestamp.tv_sec;
            msg.data.shutter.tv_usec = captureMsg->data.event.timestamp.tv_usec;
            mMessageQueue.send(&msg, MESSAGE_ID_NEW_SHUTTER);
            break;
        case CAPTURE_EVENT_NEW_SOF:
            mSofSequence = captureMsg->data.event.sequence;
            LOGD("sof event sequence = %u", mSofSequence);
            break;
        case CAPTURE_REQUEST_DONE:
            msg.id = MESSAGE_ID_NEW_REQUEST_DONE;
            msg.requestId = captureMsg->data.event.reqId;
            mMessageQueue.send(&msg, MESSAGE_ID_NEW_REQUEST_DONE);
            break;
        default:
            LOGW("Unsupported Capture event ");
            break;
    }

    return true;
}

status_t
RKISP2ControlUnit::metadataReceived(int id, const camera_metadata_t *metas) {
    Message msg;
    status_t status = NO_ERROR;
    camera_metadata_entry entry;
    static std::map<int,uint8_t> sLastAeStateMap;

    CameraMetadata result(const_cast<camera_metadata_t*>(metas));
    entry = result.find(RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_NEEDED);
    if (entry.count == 1) {
        mStillCapSyncNeeded = !!entry.data.u8[0];
    }

    entry = result.find(RK_MEANLUMA_VALUE);
    if (entry.count == 1) {
        LOGD("metadataReceived meanluma:%f", entry.data.f[0]);
    if (mSocCamFlashCtrUnit.get())
        mSocCamFlashCtrUnit->setMeanLuma(entry.data.f[0]);
    }

    entry = result.find(RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD);
    if (entry.count == 1) {
        if (entry.data.u8[0] == RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCDONE &&
            (mStillCapSyncState == STILL_CAP_SYNC_STATE_WAITING_ENGINE_DONE ||
             mFlushForUseCase == FLUSH_FOR_STILLCAP))
            mStillCapSyncState = STILL_CAP_SYNC_STATE_FROM_ENGINE_DONE;
            LOGD("%s:%d, stillcap_sync_state %d",
                 __FUNCTION__, __LINE__, mStillCapSyncState);
    }

    entry = result.find(ANDROID_CONTROL_AE_STATE);
    if (entry.count == 1) {
        if (id == -1 && entry.data.u8[0] == ANDROID_CONTROL_AE_STATE_CONVERGED &&
            mStillCapSyncState == STILL_CAP_SYNC_STATE_FORCE_TO_ENGINE_PRECAP &&
            sLastAeStateMap[mCameraId] == ANDROID_CONTROL_AE_STATE_PRECAPTURE) {
            mStillCapSyncState = STILL_CAP_SYNC_STATE_FORCE_PRECAP_DONE;
            sLastAeStateMap[mCameraId] = 0;
            LOGD("%s:%d, stillcap_sync_state %d",
                 __FUNCTION__, __LINE__, mStillCapSyncState);
        }
        sLastAeStateMap[mCameraId] = entry.data.u8[0];
    }

    result.release();

    if (id != -1) {
        msg.id = MESSAGE_ID_METADATA_RECEIVED;
        msg.requestId = id;
        msg.metas = metas;
        status = mMessageQueue.send(&msg);
    }

    return status;
}

status_t
RKISP2ControlUnit::handleMetadataReceived(Message &msg) {
    status_t status = OK;
    std::shared_ptr<RKISP2RequestCtrlState> reqState = nullptr;
    int reqId = msg.requestId;

    std::map<int, std::shared_ptr<RKISP2RequestCtrlState>>::iterator it =
                                    mWaitingForCapture.find(reqId);
    if(reqId == -1)
        return OK;
    if (it == mWaitingForCapture.end()) {
        LOGE("Unexpected request done event received for request %d - Fix the bug", reqId);
        return UNKNOWN_ERROR;
    }

    reqState = it->second;
    if (CC_UNLIKELY(reqState.get() == nullptr || reqState->request == nullptr)) {
        LOGE("No valid state or request for request Id = %d"
             "- Fix the bug!", reqId);
        return UNKNOWN_ERROR;
    }

    mLatestCamMeta = msg.metas;
    //Metadata reuslt are mainly divided into three parts
    //1. some settings from app
    //2. 3A metas from Control loop
    //3. some items like sensor timestamp from shutter
    reqState->ctrlUnitResult->append(msg.metas);
    reqState->mClMetaReceived = true;
    if(reqState->mShutterMetaReceived) {
        mMetadata->writeRestMetadata(*reqState);
        reqState->request->notifyFinalmetaFilled();
    }

    if (!reqState->mImgProcessDone)
        return OK;

    Camera3Request* request = reqState->request;
    request->mCallback->metadataDone(request, request->getError() ? -1 : CONTROL_UNIT_PARTIAL_RESULT);
    mWaitingForCapture.erase(reqId);

    return status;
}

/**
 * Static callback forwarding methods from CL to instance
 */
void RKISP2ControlUnit::sMetadatCb(const struct cl_result_callback_ops* ops,
                             struct rkisp_cl_frame_metadata_s *result) {
    LOGI("@%s %d: frame %d result meta received", __FUNCTION__, __LINE__, result->id);
    // TuningServer *pserver = TuningServer::GetInstance();

    RKISP2ControlUnit *ctl = const_cast<RKISP2ControlUnit*>(static_cast<const RKISP2ControlUnit*>(ops));

    ctl->metadataReceived(result->id, result->metas);
    // if(pserver && pserver->isTuningMode()){
    //     CameraMetadata uvcCamMeta(const_cast<camera_metadata_t*>(result->metas));
    //     pserver->get_tuning_params(uvcCamMeta);
    //     uvcCamMeta.release();
    // }
}

} // namespace rkisp2
} // namespace camera2
} // namespace android

