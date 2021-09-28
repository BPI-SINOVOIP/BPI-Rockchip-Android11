/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#define LOG_TAG "RKISP2CameraHw"

#include "RKISP2CameraHw.h"
#include "LogHelper.h"
#include "CameraMetadataHelper.h"
#include "RequestThread.h"
#include "HwStreamBase.h"
#include "PlatformData.h"
#include "RKISP2ImguUnit.h"
#include "RKISP2ControlUnit.h"
#include "RKISP2PSLConfParser.h"
// #include "TuningServer.h"

namespace android {
namespace camera2 {

// Camera factory
ICameraHw *CreatePSLCamera(int cameraId) {
    return new rkisp2::RKISP2CameraHw(cameraId);
}

namespace rkisp2 {

static const uint8_t DEFAULT_PIPELINE_DEPTH = 4;



RKISP2CameraHw::RKISP2CameraHw(int cameraId):
        mCameraId(cameraId),
        mConfigChanged(true),
        mStaticMeta(nullptr),
        mPipelineDepth(-1),
        mImguUnit(nullptr),
        mControlUnit(nullptr),
        mGCM(cameraId),
        mUseCase(USECASE_VIDEO),
        mOperationMode(0),
        mTestPatternMode(ANDROID_SENSOR_TEST_PATTERN_MODE_OFF)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
}

RKISP2CameraHw::~RKISP2CameraHw()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    deInit();
}

status_t
RKISP2CameraHw::init()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;

    std::string sensorMediaDevice = RKISP2PSLConfParser::getSensorMediaDevice(mCameraId);

    mMediaCtl = std::make_shared<MediaController>(sensorMediaDevice.c_str());

    status = mMediaCtl->init();
    if (status != NO_ERROR) {
        LOGE("Error initializing Media Controller");
        return status;
    }

    std::string imguMediaDevice = RKISP2PSLConfParser::getImguMediaDevice(mCameraId);
    //imguMediaDevice = sensorMediaDevice;
    if (sensorMediaDevice == imguMediaDevice) {
        LOGI("Using sensor media device as imgu media device");
        mImguMediaCtl = mMediaCtl;
    } else {
        mImguMediaCtl = std::make_shared<MediaController>(imguMediaDevice.c_str());

        status = mImguMediaCtl->init();
        if (status != NO_ERROR) {
            LOGE("Error initializing Media Controller");
            return status;
        }
    }

    mGCM.setMediaCtl(mMediaCtl,mImguMediaCtl);

    mImguUnit = new RKISP2ImguUnit(mCameraId, mGCM, mMediaCtl, mImguMediaCtl);

    mControlUnit = new RKISP2ControlUnit(mImguUnit,
                                   mCameraId,
                                   mGCM,
                                   mMediaCtl);
    status = mControlUnit->init();
    if (status != NO_ERROR) {
        LOGE("Error initializing ControlUnit. ret code:%x", status);
        return status;
    }

    // mTuningServer = TuningServer::GetInstance();
    // if(mTuningServer) {
    //     mTuningServer->init(mControlUnit, this, mCameraId);
    // }

    // Register ControlUnit as a listener to capture events
    status = mImguUnit->attachListener(mControlUnit);
    status = initStaticMetadata();
    if (status != NO_ERROR) {
        LOGE("Error call initStaticMetadata, status:%d", status);
        return status;
    }

    mFakeRawStream.width = 0;
    mFakeRawStream.height = 0;
    mFakeRawStream.stream_type = CAMERA3_STREAM_OUTPUT;
    mFakeRawStream.format = HAL_PIXEL_FORMAT_RAW_OPAQUE;

    return status;
}

void
RKISP2CameraHw::deInit()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    if (mImguUnit) {
       mImguUnit->cleanListener();
       /* mImguUnit->flush(); */
    }

    if (mControlUnit) {
        mControlUnit->flush();
    }

    if (mImguUnit) {
        delete mImguUnit;
        mImguUnit = nullptr;
    }

    //ControlUnit destruct function will release RKISP2ProcUnitSettingsPool
    //and there are still ~shared_ptr<RKISP2ProcUnitSettings> in ImguUnit destruct
    //function and it will cause crash, so delete ControlUnit after ImguUnit
    if (mControlUnit) {
        delete mControlUnit;
        mControlUnit = nullptr;
    }

    if(mStaticMeta) {
        /**
         * The metadata buffer this object uses belongs to PlatformData
         * we release it before we destroy the object.
         */
        mStaticMeta->release();
        delete mStaticMeta;
        mStaticMeta = nullptr;
    }

    // mTuningServer = TuningServer::GetInstance();
    // if(mTuningServer) {
    //     mTuningServer->deinit();
    // }
}

const camera_metadata_t *
RKISP2CameraHw::getDefaultRequestSettings(int type)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    return PlatformData::getDefaultMetadata(mCameraId, type);
}

status_t RKISP2CameraHw::checkStreamSizes(std::vector<camera3_stream_t*> &activeStreams)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    int32_t count;

    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    if (!meta) {
        LOGE("Cannot get static metadata.");
        return BAD_VALUE;
    }

    int32_t *availStreamConfig = (int32_t*)MetadataHelper::getMetadataValues(meta,
                                 ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                                 TYPE_INT32,
                                 &count);

    if (availStreamConfig == nullptr) {
        LOGE("Cannot get stream configuration from static metadata.");
        return BAD_VALUE;
    }

    // check the stream sizes are reasonable. Note the array of integers in
    // availStreamConfig needs to be jumped by four, W & H are at +1 & +2
    for (uint32_t i = 0; i < activeStreams.size(); i++) {
        bool matchFound = false;
        for (uint32_t j = 0; j < (uint32_t)count; j += 4) {
            if ((activeStreams[i]->width  == (uint32_t)availStreamConfig[j + 1]) &&
                (activeStreams[i]->height == (uint32_t)availStreamConfig[j + 2])) {
                matchFound = true;
                break;
            }
        }
        if (!matchFound) {
            LOGE("Camera stream config had unsupported dimension %dx%d.",
                 activeStreams[i]->width,
                 activeStreams[i]->height);
            return BAD_VALUE;
        }
    }

    return OK;
}

status_t RKISP2CameraHw::checkStreamRotation(const std::vector<camera3_stream_t*> activeStreams)
{
#ifdef CHROME_BOARD
    int stream_crop_rotate_scale_degrees = -1;

    for (const auto* stream : activeStreams) {
        if (stream->stream_type != CAMERA3_STREAM_OUTPUT)
            continue;

        if (stream->crop_rotate_scale_degrees != CAMERA3_STREAM_ROTATION_0
            && stream->crop_rotate_scale_degrees != CAMERA3_STREAM_ROTATION_90
            && stream->crop_rotate_scale_degrees != CAMERA3_STREAM_ROTATION_270) {
            LOGE("Invalid rotation value %d", stream->crop_rotate_scale_degrees);
            return BAD_VALUE;
        }

        if ((stream_crop_rotate_scale_degrees != -1) &&
           (stream->crop_rotate_scale_degrees != stream_crop_rotate_scale_degrees))
            return BAD_VALUE;

        stream_crop_rotate_scale_degrees = stream->crop_rotate_scale_degrees;
    }
    return OK;
#endif
    int rotation = -1;

    for (const auto* stream : activeStreams) {
        if (stream->stream_type != CAMERA3_STREAM_OUTPUT)
            continue;

        if (stream->rotation != CAMERA3_STREAM_ROTATION_0
            && stream->rotation != CAMERA3_STREAM_ROTATION_90
            && stream->rotation != CAMERA3_STREAM_ROTATION_270) {
            LOGE("Invalid rotation value %d", stream->rotation);
            return BAD_VALUE;
        }

        if ((rotation != -1) &&
           (stream->rotation != rotation))
            return BAD_VALUE;

        rotation = stream->rotation;
    }
    return OK;
}

int64_t RKISP2CameraHw::getMinFrameDurationNs(camera3_stream_t* stream) {
    CheckError(stream == NULL, -1, "@%s,  invalid stream", __FUNCTION__);

    const int STREAM_DURATION_SIZE = 4;
    const int STREAM_FORMAT_OFFSET = 0;
    const int STREAM_WIDTH_OFFSET = 1;
    const int STREAM_HEIGHT_OFFSET = 2;
    const int STREAM_DURATION_OFFSET = 3;
    camera_metadata_entry entry;
    entry = mStaticMeta->find(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS);
    for (size_t i = 0; i < entry.count; i+= STREAM_DURATION_SIZE) {
        int64_t format = entry.data.i64[i + STREAM_FORMAT_OFFSET];
        int64_t width = entry.data.i64[i + STREAM_WIDTH_OFFSET];
        int64_t height = entry.data.i64[i + STREAM_HEIGHT_OFFSET];
        int64_t duration = entry.data.i64[i + STREAM_DURATION_OFFSET];
        LOGD("@%s : idex:%d, format:%" PRId64 ", wxh: %" PRId64 "x%" PRId64 ", duration:%" PRId64 "",
             __FUNCTION__, i, format, width, height, duration);
        if (format == stream->format && width == stream->width && height == stream->height)
            return duration;
    }

    return -1;
}

camera3_stream_t*
RKISP2CameraHw::findStreamForStillCapture(const std::vector<camera3_stream_t*>& streams)
{
    static const int64_t stillCaptureCaseThreshold = 33400000LL; // 33.4 ms
    camera3_stream_t* jpegStream = nullptr;

    for (auto* s : streams) {
        /* TODO  reprocess case*/
        if (s->stream_type == CAMERA3_STREAM_INPUT ||
            s->stream_type == CAMERA3_STREAM_BIDIRECTIONAL) {
            LOGI("@%s : Reprocess case, not still capture case", __FUNCTION__);
            return nullptr;
        }

        if (s->format == HAL_PIXEL_FORMAT_BLOB)
            jpegStream = s;
    }

    // It means that sensor can output frames with different sizes and fps
    // if minFrameDuration for HAL_PIXEL_FORMAT_BLOB format is larger 33.4ms,
    // need do reconfig stream to reconfig media pipeline because preview always
    // can reach 30 fps.
    if(jpegStream) {
        int64_t minFrameDurationNs = getMinFrameDurationNs(jpegStream);
        if (minFrameDurationNs > stillCaptureCaseThreshold)
            return jpegStream;
        else
            return nullptr;
    }
    return nullptr;
}

void
RKISP2CameraHw::checkNeedReconfig(UseCase newUseCase, std::vector<camera3_stream_t*> &activeStreams)
{
    uint32_t lastPathSize = 0, pathSize = 0;
    uint32_t lastSensorSize = 0, sensorSize = 0;

    // must switch sensor output when switch between tunging case and others
    if(newUseCase == USECASE_TUNING || mUseCase != newUseCase) {
        mConfigChanged = true;
        return ;
    }

    //pipeline should reconfig when Sensor out put size changed
    mGCM.getSensorOutputSize(sensorSize);
    mImguUnit->getConfigedSensorOutputSize(lastSensorSize);
    mConfigChanged = (sensorSize != lastSensorSize);
    if (mConfigChanged)
        return ;

    const MediaCtlConfig* imgu_ctl = mGCM.getMediaCtlConfig(RKISP2IStreamConfigProvider::IMGU_COMMON);
    if (imgu_ctl->mVideoNodes[0].name.find("rkcif") != std::string::npos) {
        LOGI("@%s : rkcif device, no need reconifg when sensor output size does not change", __FUNCTION__);
        mConfigChanged = false;
        return ;
    }

    //pipeline should reconfig when mainpath size expand
    mGCM.getHwPathSize("rkisp1_mainpath", pathSize);
    mImguUnit->getConfigedHwPathSize("rkisp1_mainpath", lastPathSize);
    mConfigChanged = pathSize > lastPathSize ? true : false;
}

status_t
RKISP2CameraHw::configStreams(std::vector<camera3_stream_t*> &activeStreams,
                            uint32_t operation_mode)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    uint32_t maxBufs, usage;
    status_t status = NO_ERROR;
    bool configChanged = true;

    mOperationMode = operation_mode;
    mStreamsStill.clear();
    mStreamsVideo.clear();

    if (checkStreamSizes(activeStreams) != OK)
        return BAD_VALUE;

    if (checkStreamRotation(activeStreams) != OK)
        return BAD_VALUE;

    maxBufs = mPipelineDepth;  /* value from XML static metadata */
    if (maxBufs > MAX_REQUEST_IN_PROCESS_NUM)
        maxBufs = MAX_REQUEST_IN_PROCESS_NUM;

    /**
     * Here we could give different gralloc flags depending on the stream
     * format, at the moment we give the same to all
     */
    /* TODO: usage may different between the streams,
     * add GRALLOC_USAGE_HW_VIDEO_ENCODER is a temp patch for gpu bug:
     * gpu cant alloc a nv12 buffer when format is
     * HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED. Need gpu provide a patch */
    usage = GRALLOC_USAGE_SW_READ_OFTEN |
            GRALLOC_USAGE_SW_WRITE_NEVER |
            GRALLOC_USAGE_HW_VIDEO_ENCODER |
            GRALLOC_USAGE_HW_CAMERA_WRITE |
            RK_GRALLOC_USAGE_SPECIFY_STRIDE|
            GRALLOC_USAGE_PRIVATE_1; // full range

    camera3_stream_t* stillStream = findStreamForStillCapture(activeStreams);

    for (unsigned int i = 0; i < activeStreams.size(); i++) {
        activeStreams[i]->max_buffers = maxBufs;
        activeStreams[i]->usage |= usage;

        if (activeStreams[i] != stillStream) {
            mStreamsVideo.push_back(activeStreams[i]);
            mStreamsStill.push_back(activeStreams[i]);
        } else if (stillStream) {
            // always insert BLOB as fisrt stream if exists
            ALOGD("%s: find still stream %dx%d, 0x%x", __FUNCTION__, stillStream->width,
                  stillStream->height, stillStream->format);
            mStreamsStill.insert(mStreamsStill.begin(), stillStream);
        }
    }

    mUseCase = USECASE_VIDEO; // Configure video pipe by default
    if (mStreamsVideo.empty()) {
        mUseCase = USECASE_STILL;
    }

    LOGI("%s: select usecase: %s, video/still stream num: %zu/%zu", __FUNCTION__,
            mUseCase ? "USECASE_VIDEO" : "USECASE_STILL", mStreamsVideo.size(), mStreamsStill.size());
    status = doConfigureStreams(mUseCase, operation_mode, ANDROID_SENSOR_TEST_PATTERN_MODE_OFF);

    return status;
}

status_t
RKISP2CameraHw::bindStreams(std::vector<CameraStreamNode *> activeStreams)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;

    mDummyHwStreamsVector.clear();
    // Need to have a producer stream as it is required by common code
    // bind all streams to dummy HW stream class.
    for (unsigned int i = 0; i < activeStreams.size(); i++) {
        CameraStreamNode *stream = activeStreams.at(i);
        std::shared_ptr<HwStreamBase> hwStream = std::make_shared<HwStreamBase>(*stream);
        CameraStream::bind(stream, hwStream.get());
        mDummyHwStreamsVector.push_back(hwStream);
    }

    return status;
}

status_t
RKISP2CameraHw::processRequest(Camera3Request* request, int inFlightCount)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    if (inFlightCount > mPipelineDepth) {
        LOGI("@%s:blocking request %d", __FUNCTION__, request->getId());
        return RequestThread::REQBLK_WAIT_ONE_REQUEST_COMPLETED;
    }

    status_t status = NO_ERROR;
    // Check reconfiguration
    UseCase newUseCase = checkUseCase(request);
    camera_metadata_ro_entry entry;
    int32_t testPatternMode = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
    entry = request->getSettings()->find(ANDROID_SENSOR_TEST_PATTERN_MODE);
    if(entry.count == 1) {
        status = getTestPatternMode(request, &testPatternMode);
        CheckError(status != NO_ERROR, status, "@%s: failed to get test pattern mode", __FUNCTION__);
    }

    //workround for CTS:ImageReaderTest#testRepeatingJpeg
    //this test will call mReader.acquireLatestImage, this function will
    //do get latest frame and acquire it's fence until there no new frame
    //queued. Now, we return the jpeg buffer back to framework in advance
    //and then signal the fence after some latency. this will result
    //acquireLatestImage can alway get the new frame and cause infinite loops
    //so wait previous request completed and fence signaled to avoid this.
    std::vector<camera3_stream_t*>& streams = (mUseCase == USECASE_STILL) ?
                                              mStreamsStill : mStreamsVideo;
    int jpegStreamCnt = 0;
    for (auto it : streams) {
        if (it->format == HAL_PIXEL_FORMAT_BLOB)
            jpegStreamCnt++;
    }
    if (jpegStreamCnt == streams.size()) {
        LOGI("There are only blob streams, it should be a cts case rather than a normal case");
        if (inFlightCount > 1) {
            return RequestThread::REQBLK_WAIT_ALL_PREVIOUS_COMPLETED_AND_FENCE_SIGNALED;
        }
    }

    if (newUseCase != mUseCase || testPatternMode != mTestPatternMode ||
        (newUseCase == USECASE_TUNING && mTuningSizeChanged)) {
        LOGI("%s: request %d need reconfigure, infilght %d, usecase %d -> %d", __FUNCTION__,
                request->getId(), inFlightCount, mUseCase, newUseCase);
        if (inFlightCount > 1) {
            return RequestThread::REQBLK_WAIT_ALL_PREVIOUS_COMPLETED;
        }

        status = doConfigureStreams(newUseCase, mOperationMode, testPatternMode);
    }

    if (status == NO_ERROR) {
        mControlUnit->processRequest(request,
                                     mGCM.getGraphConfig(*request));
    }
    return status;
}

void RKISP2CameraHw::sendTuningDumpCmd(int w, int h)
{
    if(w != mFakeRawStream.width || h != mFakeRawStream.height)
        mTuningSizeChanged = true;
    mFakeRawStream.width = w;
    mFakeRawStream.height = h;
}

RKISP2CameraHw::UseCase RKISP2CameraHw::checkUseCase(Camera3Request* request)
{
#if 0
    ////////////////////////////////////////////////////////
    // tuning tools demo
    // setprop persist.vendor.camera.tuning 1  to dump full raw
    // setprop persist.vendor.camera.tuning 2  to dump bining raw
    int pixel_width = 0;
    int pixel_height = 0;

    CameraMetadata staticMeta;
    staticMeta = PlatformData::getStaticMetadata(mCameraId);
    camera_metadata_entry entry = staticMeta.find(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE);
    if(entry.count == 2) {
        pixel_width = entry.data.i32[0];
        pixel_height = entry.data.i32[1];
    }

    char property_value[PROPERTY_VALUE_MAX] = {0};
    property_get("persist.vendor.camera.tuning", property_value, "0");
    if(strcmp(property_value, "1") == 0) {
        LOGD("@%s : dump full raw, return USECASE_TUNING", __FUNCTION__);
        sendTuningDumpCmd(pixel_width, pixel_height);

    } else if(strcmp(property_value, "2") == 0) {
        LOGD("@%s : dump binning raw, return USECASE_TUNING", __FUNCTION__);
            sendTuningDumpCmd(pixel_width / 2, pixel_height / 2);
    } else {
        sendTuningDumpCmd(0, 0);
    }
    // tuning demo end
    ////////////////////////////////////////////////////////
#endif

    if(mFakeRawStream.width && mFakeRawStream.height) {
        return USECASE_TUNING;
    }

    if (mStreamsStill.size() == mStreamsVideo.size()) {
        return USECASE_VIDEO;
    }
    const std::vector<camera3_stream_buffer>* buffers = request->getOutputBuffers();
    for (const auto & buf : *buffers) {
        if (buf.stream == mStreamsStill[0]) {
            return USECASE_STILL;
        }
    }

    return USECASE_VIDEO;
}

status_t RKISP2CameraHw::getTestPatternMode(Camera3Request* request, int32_t* testPatternMode)
{
    const CameraMetadata *reqSetting = request->getSettings();
    CheckError(reqSetting == nullptr, UNKNOWN_ERROR, "no settings in request - BUG");

    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    camera_metadata_ro_entry entry, availableTestPatternModes;

    availableTestPatternModes = MetadataHelper::getMetadataEntry(meta,
        ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES);

    entry = reqSetting->find(ANDROID_SENSOR_TEST_PATTERN_MODE);
    MetadataHelper::getSetting(availableTestPatternModes, entry, testPatternMode);
    CheckError(*testPatternMode < 0, BAD_VALUE, "@%s: invalid test pattern mode: %d",
        __FUNCTION__, *testPatternMode);

    LOGI("@%s: current test pattern mode: %d", __FUNCTION__, *testPatternMode);
    return NO_ERROR;
}

status_t RKISP2CameraHw::doConfigureStreams(UseCase newUseCase,
                                  uint32_t operation_mode, int32_t testPatternMode)
{
    PERFORMANCE_ATRACE_CALL();
    mTestPatternMode = testPatternMode;
    std::vector<camera3_stream_t*> streams = (newUseCase == USECASE_STILL) ?
                                              mStreamsStill : mStreamsVideo;
    mTuningSizeChanged = false;

    // consider USECASE_TUNING first
    if(newUseCase == USECASE_TUNING) {
        streams.push_back(&mFakeRawStream);
    } else if(LogHelper::isDumpTypeEnable(CAMERA_DUMP_RAW)){
        // fake raw stream for dump raw
        mFakeRawStream.width = 0;
        mFakeRawStream.height = 0;
        streams.push_back(&mFakeRawStream);
    }

    LOGI("%s: select usecase: %s, stream nums: %zu", __FUNCTION__,
            newUseCase == USECASE_VIDEO ? "USECASE_VIDEO" :
            newUseCase == USECASE_STILL ? "USECASE_STILL" : "USECASE_TUNING",
            streams.size());

    mGCM.enableMainPathOnly(newUseCase == USECASE_STILL ? true : false);

    status_t status = mGCM.configStreams(streams, operation_mode, testPatternMode);
    if (status != NO_ERROR) {
        LOGE("Unable to configure stream: No matching graph config found! BUG");
        return status;
    }
    checkNeedReconfig(newUseCase, streams);
    mUseCase = newUseCase;

    /* Flush to make sure we return all graph config objects to the pool before
       next stream config. */
    // mImguUnit->flush() moves to the controlunit for sync
    /* mImguUnit->flush(); */
    mControlUnit->flush(!mConfigChanged ? RKISP2ControlUnit::FLUSH_FOR_NOCHANGE :
                        newUseCase == USECASE_STILL ? RKISP2ControlUnit::FLUSH_FOR_STILLCAP :
                        RKISP2ControlUnit::FLUSH_FOR_PREVIEW);

    status = mImguUnit->configStreams(streams, mConfigChanged);
    if (status != NO_ERROR) {
        LOGE("Unable to configure stream for imgunit");
        return status;
    }

    status = mControlUnit->configStreams(streams, mConfigChanged);
    if (status != NO_ERROR) {
        LOGE("Unable to configure stream for controlunit");
        return status;
    }
    mGCM.dumpStreamConfig(streams);
    return mImguUnit->configStreamsDone();
}

status_t
RKISP2CameraHw::flush()
{
    return NO_ERROR;
}

void
RKISP2CameraHw::registerErrorCallback(IErrorCallback* errCb)
{
    if (mImguUnit)
        mImguUnit->registerErrorCallback(errCb);
}

void
RKISP2CameraHw::dump(int fd)
{
    UNUSED(fd);
}

/**
 * initStaticMetadata
 *
 * Create CameraMetadata object to retrieve the static tags used in this class
 * we cache them as members so that we do not need to query CameraMetadata class
 * every time we need them. This is more efficient since find() is not cheap
 */
status_t
RKISP2CameraHw::initStaticMetadata(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    status_t status = NO_ERROR;
    /**
    * Initialize the CameraMetadata object with the static metadata tags
    */
    camera_metadata_t* staticMeta;
    staticMeta = (camera_metadata_t*)PlatformData::getStaticMetadata(mCameraId);
    mStaticMeta = new CameraMetadata(staticMeta);

    camera_metadata_entry entry;
    entry = mStaticMeta->find(ANDROID_REQUEST_PIPELINE_MAX_DEPTH);
    if(entry.count == 1) {
        mPipelineDepth = entry.data.u8[0];
    } else {
        mPipelineDepth = DEFAULT_PIPELINE_DEPTH;
    }

    /**
     * Check the consistency of the information we had in XML file.
     * partial result count is very tightly linked to the PSL
     * implementation
     */
    int32_t xmlPartialCount = PlatformData::getPartialMetadataCount(mCameraId);
    if (xmlPartialCount != PARTIAL_RESULT_COUNT) {
            LOGW("Partial result count does not match current implementation "
                 " got %d should be %d, fix the XML!", xmlPartialCount,
                 PARTIAL_RESULT_COUNT);
            status = NO_INIT;
    }
    return status;
}

}  // namespace rkisp2
}  // namespace camera2
}  // namespace android
