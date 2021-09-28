/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#define LOG_TAG "RKISP2GraphConfigManager"

#include "RKISP2GraphConfigManager.h"
#include "RKISP2GraphConfig.h"
#include "PlatformData.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "Camera3Request.h"

using std::vector;
using std::map;

namespace android {
namespace camera2 {
namespace rkisp2 {

/* should support at least 4 streams comparetd to HAL1 */
#define MAX_NUM_STREAMS    4

GraphConfigNodes::GraphConfigNodes()
{
}

GraphConfigNodes::~GraphConfigNodes()
{
}

RKISP2GraphConfigManager::RKISP2GraphConfigManager(int32_t camId,
                                       GraphConfigNodes *testNodes) :
    mCameraId(camId),
    mIsOnlyEnableMp(false),
    mGraphConfigPool("RKISP2GraphConfig")
{

    status_t status = mGraphConfigPool.init(MAX_REQ_IN_FLIGHT * 2,
                                            RKISP2GraphConfig::reset);
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to initialize the pool of GraphConfigs");
    }
}

RKISP2GraphConfigManager::~RKISP2GraphConfigManager()
{
    // Check that all graph config objects are returned to the pool
    if(!mGraphConfigPool.isFull()) {
        LOGE("RKISP2GraphConfig pool is missing objects at destruction!");
    }
}

#define streamSizeGT(s1, s2) (((s1)->width * (s1)->height) > ((s2)->width * (s2)->height))
#define streamSizeEQ(s1, s2) (((s1)->width * (s1)->height) == ((s2)->width * (s2)->height))
#define streamSizeGE(s1, s2) (((s1)->width * (s1)->height) >= ((s2)->width * (s2)->height))

status_t RKISP2GraphConfigManager::mapStreamToKey(const std::vector<camera3_stream_t*> &streams,
                                                    int& videoStreamCnt, int& stillStreamCnt)
{
    // deal with CAMERA3_STREAM_OUTPUT streams and
    // CAMERA3_STREAM_BIDIRECTIONAL streams, input streams
    // are not processed here
    // Don't support RAW currently
    // Keep streams in order: BLOB, IMPL, YUV...
    std::vector<camera3_stream_t *> availableStreams;
    camera3_stream_t * blobStream = nullptr;
    camera3_stream_t * rawStream = nullptr;
    int yuvNum = 0;
    int blobNum = 0;
    for (int i = 0; i < streams.size(); i++) {
        if ((streams[i]->stream_type != CAMERA3_STREAM_OUTPUT) &&
            (streams[i]->stream_type != CAMERA3_STREAM_BIDIRECTIONAL))
            continue;
        switch (streams[i]->format) {
            case HAL_PIXEL_FORMAT_BLOB:
                blobNum++;
                blobStream = streams[i];
                break;
            case HAL_PIXEL_FORMAT_YCbCr_420_888:
                yuvNum++;
                availableStreams.push_back(streams[i]);
                break;
            case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
                yuvNum++;
                availableStreams.insert(availableStreams.begin(), streams[i]);
                break;
            case HAL_PIXEL_FORMAT_RAW_OPAQUE:
                rawStream = streams[i];
                break;
            default:
                LOGE("Unsupported stream format %d", streams.at(i)->format);
                return BAD_VALUE;
        }
    }

    // Current only one BLOB stream supported and always insert as fisrt stream
    if (blobStream) {
        availableStreams.insert(availableStreams.begin(), blobStream);
    }
    LOGI("@%s, blobNum:%d, yuvNum:%d", __FUNCTION__, blobNum, yuvNum);

    // Main output produces full size frames
    // Secondary output produces small size frames
    int mainOutputIndex = -1;
    int secondaryOutputIndex = -1;

    if (availableStreams.size() == 1) {
        mainOutputIndex = 0;
    } else if (availableStreams.size() == 2) {
        mainOutputIndex = (streamSizeGE(availableStreams[0], availableStreams[1])) ? 0 : 1;
        secondaryOutputIndex = mainOutputIndex ? 0 : 1;
        if(availableStreams[secondaryOutputIndex]->width > SP_MAX_WIDTH ||
           availableStreams[secondaryOutputIndex]->height > SP_MAX_HEIGHT) {
            secondaryOutputIndex = -1;
        }
    } else {
        mainOutputIndex = 0;
        // find the maxium size stream
        for (int i = 0; i < availableStreams.size(); i++) {
            if (streamSizeGT(availableStreams[i], availableStreams[mainOutputIndex]))
                mainOutputIndex = i;
        }

        for (int i = 0; i < availableStreams.size(); i++) {
            if (i == mainOutputIndex ||
                availableStreams[i]->width > SP_MAX_WIDTH ||
                availableStreams[i]->height > SP_MAX_HEIGHT) {
                continue ;
            } else {
                /* because ISP can output two different size streams
                 * concurrently, so we prefer to use this feature.
                 */
                if(secondaryOutputIndex == -1)
                    secondaryOutputIndex = i;
                if (streamSizeGT(availableStreams[i], availableStreams[secondaryOutputIndex]))
                    secondaryOutputIndex = i;
            }
        }
    }

    //in stillcapture case, only enable one path can optimize performance
    if(mIsOnlyEnableMp) {
        secondaryOutputIndex = -1;
        LOGI("@%s : only enable mainpath for some special cases", __FUNCTION__);
    }

    LOGD("@%s, mainOutputIndex %d, secondaryOutputIndex %d ", __FUNCTION__, mainOutputIndex, secondaryOutputIndex);
    mStreamToSinkIdMap[availableStreams[mainOutputIndex]] = GCSS_KEY_IMGU_VIDEO;
    if (secondaryOutputIndex >= 0)
        mStreamToSinkIdMap[availableStreams[secondaryOutputIndex]] = GCSS_KEY_IMGU_PREVIEW;

    if(rawStream)
        mStreamToSinkIdMap[rawStream] = GCSS_KEY_IMGU_RAW;

    return OK;
}

/**
 * Initialize the state of the RKISP2GraphConfigManager after parsing the stream
 * configuration.
 * Perform the first level query to find a subset of settings that fulfill the
 * constrains from the stream configuration.
 *
 * TODO: Pass the new stream config modifier
 *
 * \param[in] streams List of streams required by the client.
 */
status_t RKISP2GraphConfigManager::configStreams(const vector<camera3_stream_t*> &streams,
                                           uint32_t operationMode,
                                           int32_t testPatternMode)
{
    PERFORMANCE_ATRACE_NAME("RKISP2GraphConfigManager::configStreams");
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    UNUSED(operationMode);

    status_t ret = OK;

    //thera may be 4 output streams and 1 input stream in
    //CTS:testMandatoryReprocessConfigurations
    //we only support 4 streams max to config
    std::vector<camera3_stream_t*> outputStream;
    for(auto stream : streams) {
        if(stream->stream_type != CAMERA3_STREAM_INPUT)
            outputStream.push_back(stream);
    }

    CheckError(outputStream.size() > MAX_NUM_STREAMS, BAD_VALUE, "@%s,  Maximum number of outputStream %u exceeded: %zu",
                   __FUNCTION__, MAX_NUM_STREAMS, outputStream.size());

    /*
     * regenerate the stream resolutions vector if needed
     * We do this because we consume this vector for each stream configuration.
     * This allows us to have sequential stream numbers even when an input
     * stream is present.
     */
    mStreamToSinkIdMap.clear();

    int videoStreamCount = 0, stillStreamCount = 0;
    ret = mapStreamToKey(outputStream, videoStreamCount, stillStreamCount);
    CheckError(ret != OK, ret, "@%s, call mapStreamToKey fail, ret:%d",
                   __FUNCTION__, ret);

    /*
     * Currently it is enough to refresh information in graph config objects
     * per stream config. Here we populate all gc objects in the pool.
     */
    std::shared_ptr<RKISP2GraphConfig> gc = nullptr;
    int poolSize = mGraphConfigPool.availableItems();
    LOGD("@%s : poolSize:%d", __FUNCTION__, poolSize);
    for (int i = 0; i < poolSize; i++) {
        mGraphConfigPool.acquireItem(gc);
        ret = prepareGraphConfig(gc);
        CheckError(ret != OK, UNKNOWN_ERROR, "@%s, Failed to prepare graph config",
                       __FUNCTION__);
    }

    CheckError(gc.get() == nullptr, UNKNOWN_ERROR, "@%s, Graph config is NULL, BUG!",
                   __FUNCTION__);

    /**
     * since we map the max res stream to video, and the little one to preview, so
     * swapVideoPreview here is always false,  by the way, please make sure
     * the video or still stream size >= preview stream size in graph_settings_<sensor name>.xml, zyc.
     */
    gc->setMediaCtlConfig(mMediaCtl, mImgMediaCtl, false, false);

    // Get media control config
    for (size_t i = 0; i < MEDIA_TYPE_MAX_COUNT; i++) {
        mMediaCtlConfigsPrev[i] = mMediaCtlConfigs[i];

        // Reset old values
        mMediaCtlConfigs[i].mLinkParams.clear();
        mMediaCtlConfigs[i].mFormatParams.clear();
        mMediaCtlConfigs[i].mSelectionParams.clear();
        mMediaCtlConfigs[i].mSelectionVideoParams.clear();
        mMediaCtlConfigs[i].mControlParams.clear();
        mMediaCtlConfigs[i].mVideoNodes.clear();
        mMediaCtlConfigs[i].mParamsOrder.clear();
    }

    ret = gc->getSensorMediaCtlConfig(mCameraId, testPatternMode,
                                  &mMediaCtlConfigs[CIO2]);
    gc->dumpMediaCtlConfig(mMediaCtlConfigs[CIO2]);
    if (ret != OK)
        LOGE("Couldn't get mediaCtl config");

    ret |= gc->getImguMediaCtlConfig(mCameraId, testPatternMode,
                                     &mMediaCtlConfigs[IMGU_COMMON],
                                     outputStream);

    gc->dumpMediaCtlConfig(mMediaCtlConfigs[IMGU_COMMON]);
    if (ret != OK)
        LOGE("Couldn't get Imgu mediaCtl config");
    
    return OK;
}

/**
 * Prepare graph config object
 *
 * Use graph query results as a parameter to getGraph. The result will be given
 * to graph config object.
 *
 * \param[in/out] gc     Graph Config object.
 */
status_t RKISP2GraphConfigManager::prepareGraphConfig(std::shared_ptr<RKISP2GraphConfig> gc)
{
    status_t status = OK;

    status = gc->prepare(this, mStreamToSinkIdMap);

    return status;
}

/**
 * Retrieve the current active media controller configuration for Sensor + ISA
 *
 * This method will be removed as we clean up the CaptureUnit
 *
 */
const MediaCtlConfig* RKISP2GraphConfigManager::getMediaCtlConfig(RKISP2IStreamConfigProvider::MediaType type) const
{

    if (type >= MEDIA_TYPE_MAX_COUNT) {
        return nullptr;
    }

    /*
     * The size of this vector should be ideally 1, but in the future we will
     * have different sensor modes for the high-speed video. We should
     * know this at stream config time to filter them.
     * If there is more than one it means that there could be a potential change
     * of sensor mode for a new request.
     */

    return &mMediaCtlConfigs[type];
}

/**
 * Retrieve the previous media control configuration.
 */
const MediaCtlConfig* RKISP2GraphConfigManager::getMediaCtlConfigPrev(RKISP2IStreamConfigProvider::MediaType type) const
{
    if (type >= MEDIA_TYPE_MAX_COUNT) {
        return nullptr;
    }
    if (type == CIO2) {
        if (mMediaCtlConfigsPrev[type].mControlParams.size() < 1) {
            return nullptr;
        }
    } else if (mMediaCtlConfigsPrev[type].mLinkParams.size() < 1) {
        return nullptr;
    }
    return &mMediaCtlConfigsPrev[type];
}

std::shared_ptr<RKISP2GraphConfig>
RKISP2GraphConfigManager::getGraphConfig(Camera3Request &request)
{
    std::shared_ptr<RKISP2GraphConfig> gc;
    status_t status = OK;

    status = mGraphConfigPool.acquireItem(gc);
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to acquire RKISP2GraphConfig from pool!!- BUG");
        return gc;
    }
    // Init graph config with the current request id
    gc->init(request.getId());
    return gc;
}

/**
 * Used at stream configuration time to get the base graph that covers all
 * the possible request outputs that we have. This is used for pipeline
 * initialization.
 */
std::shared_ptr<RKISP2GraphConfig> RKISP2GraphConfigManager::getBaseGraphConfig()
{
    std::shared_ptr<RKISP2GraphConfig> gc;
    status_t status = OK;

    status = mGraphConfigPool.acquireItem(gc);
    if (CC_UNLIKELY(status != OK || gc.get() == nullptr)) {
        LOGE("Failed to acquire RKISP2GraphConfig from pool!!- BUG");
        return gc;
    }
    gc->init(0);
    return gc;
}

void RKISP2GraphConfigManager::getHwPathSize(const char* pathName, uint32_t &size)
{
    std::vector<MediaCtlFormatParams> &params = mMediaCtlConfigs[IMGU_COMMON].mFormatParams;

    for (size_t i = 0; i < params.size(); i++) {
        if(strcmp(pathName, params[i].entityName.data()) == 0) {
            size = params[i].width * params[i].height;
            ALOGD("@%s Curconfig : pathName:%s, size:%dx%d", __FUNCTION__, pathName, params[i].width, params[i].height);
        }
    }
}

void RKISP2GraphConfigManager::getSensorOutputSize(uint32_t &size) {
    std::vector<MediaCtlFormatParams> &params = mMediaCtlConfigs[CIO2].mFormatParams;
    if(params.size() == 1)
        size = params[0].width * params[0].height;
    else
        size = 0;
    LOGI("@%s Curconfig: senor output size:%dx%d", __FUNCTION__,
         size == 0 ? 0 : params[0].width,
         size == 0 ? 0 : params[0].height);
}

/******************************************************************************
 *  HELPER METHODS
 ******************************************************************************/
/**
 * Check the gralloc hint flags and decide whether this stream should be served
 * by Video Pipe or Still Pipe
 */
bool RKISP2GraphConfigManager::isVideoStream(camera3_stream_t *stream)
{
    bool display = false;
    bool videoEnc = false;
    display = CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_COMPOSER);
    display |= CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_TEXTURE);
    display |= CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_RENDER);

    videoEnc = CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_VIDEO_ENCODER);

    return (display || videoEnc);
}

void RKISP2GraphConfigManager::dumpMediaCtlConfig(const MediaCtlConfig &config) {

    size_t i = 0;
    LOGD("MediaCtl config w=%d ,height=%d"
        ,config.mCameraProps.outputWidth,
        config.mCameraProps.outputHeight);
    for (i = 0; i < config.mLinkParams.size() ; i++) {
       LOGD("Link Params srcName=%s  srcPad=%d ,sinkName=%s, sinkPad=%d enable=%d"
            ,config.mLinkParams[i].srcName.c_str(),
            config.mLinkParams[i].srcPad,
            config.mLinkParams[i].sinkName.c_str(),
            config.mLinkParams[i].sinkPad,
            config.mLinkParams[i].enable);
    }
    for (i = 0; i < config.mFormatParams.size() ; i++) {
       LOGD("Format Params entityName=%s  pad=%d ,width=%d, height=%d formatCode=%x"
            ,config.mFormatParams[i].entityName.c_str(),
            config.mFormatParams[i].pad,
            config.mFormatParams[i].width,
            config.mFormatParams[i].height,
            config.mFormatParams[i].formatCode);
    }
    for (i = 0; i < config.mSelectionVideoParams.size() ; i++) {
       LOGD("Selection video Params entityName=%s  type=%d ,target=%d, flag=%d"
            ,config.mSelectionVideoParams[i].entityName.c_str(),
            config.mSelectionVideoParams[i].select.type,
            config.mSelectionVideoParams[i].select.target,
            config.mSelectionVideoParams[i].select.flags);
    }
    for (i = 0; i < config.mSelectionParams.size() ; i++) {
       LOGD("Selection Params entityName=%s  pad=%d ,target=%d, top=%d left=%d width=%d, height=%d"
            ,config.mSelectionParams[i].entityName.c_str(),
            config.mSelectionParams[i].pad,
            config.mSelectionParams[i].target,
            config.mSelectionParams[i].top,
            config.mSelectionParams[i].left,
            config.mSelectionParams[i].width,
            config.mSelectionParams[i].height);
    }
    for (i = 0; i < config.mControlParams.size() ; i++) {
       LOGD("Control Params entityName=%s  controlId=%x ,value=%d, controlName=%s"
            ,config.mControlParams[i].entityName.c_str(),
            config.mControlParams[i].controlId,
            config.mControlParams[i].value,
            config.mControlParams[i].controlName.c_str());
    }
}

void RKISP2GraphConfigManager::dumpStreamConfig(const vector<camera3_stream_t*> &streams)
{
    bool display = false;
    bool videoEnc = false;
    bool zsl = false;

    for (size_t i = 0; i < streams.size(); i++) {
        display = CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_COMPOSER);
        display |= CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_TEXTURE);
        display |= CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_RENDER);

        videoEnc = CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_VIDEO_ENCODER);
        zsl = CHECK_FLAG(streams[i]->usage, GRALLOC_USAGE_HW_CAMERA_ZSL);

        LOGI("stream[%zu] (%s): %dx%d, fmt %s, max buffers:%d, gralloc hints (0x%x) display:%s, video:%s, zsl:%s",
                i,
                METAID2STR(android_scaler_availableStreamConfigurations_values, streams[i]->stream_type),
                streams[i]->width, streams[i]->height,
                METAID2STR(android_scaler_availableFormats_values, streams[i]->format),
                streams[i]->max_buffers,
                streams[i]->usage,
                display? "YES":"NO",
                videoEnc? "YES":"NO",
                zsl? "YES":"NO");
    }
}

}  // namespace rkisp2
}  // namespace camera2
}  // namespace android
