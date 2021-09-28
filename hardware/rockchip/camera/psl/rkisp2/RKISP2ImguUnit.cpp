/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#define LOG_TAG "RKISP2ImguUnit"

#include <vector>
#include <algorithm>
#include "RKISP2ImguUnit.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "workers/RKISP2OutputFrameWorker.h"
#include "workers/RKISP2InputFrameWorker.h"
#include "CameraMetadataHelper.h"


namespace android {
namespace camera2 {
namespace rkisp2 {

RKISP2ImguUnit::RKISP2ImguUnit(int cameraId,
                   RKISP2GraphConfigManager &gcm,
                   std::shared_ptr<MediaController> sensorMediaCtl,std::shared_ptr<MediaController> imgMediaCtl) :
        mState(IMGU_IDLE),
        mConfigChanged(true),
        mCameraId(cameraId),
        mGCM(gcm),
        mThreadRunning(false),
        mMessageQueue("RKISP2ImguUnitThread", static_cast<int>(MESSAGE_ID_MAX)),
        mCurPipeConfig(nullptr),
        mRKISP2MediaCtlHelper(sensorMediaCtl,imgMediaCtl, nullptr, true),
        mPollerThread(new PollerThread("ImguPollerThread")),
        mFlushing(false),
        mFirstRequest(true),
        mNeedRestartPoll(true),
        mErrCb(nullptr),
        mTakingPicture(false)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    mActiveStreams.inputStream = nullptr;

    mMessageThread = std::unique_ptr<MessageThread>(new MessageThread(this, "ImguThread"));
    if (mMessageThread == nullptr) {
        LOGE("Error creating poller thread");
        return;
    }
    mMessageThread->run();

    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    camera_metadata_ro_entry entry;
    CLEAR(entry);
    if (meta)
        entry = MetadataHelper::getMetadataEntry(
            meta, ANDROID_REQUEST_PIPELINE_MAX_DEPTH);
    size_t pipelineDepth = entry.count == 1 ? entry.data.u8[0] : 1;

    mMainOutWorker =
        std::make_shared<RKISP2OutputFrameWorker>(mCameraId, "MainWork",
                                            IMGU_NODE_VIDEO, pipelineDepth);
    mSelfOutWorker =
        std::make_shared<RKISP2OutputFrameWorker>(mCameraId, "SelfWork",
                                            IMGU_NODE_VF_PREVIEW, pipelineDepth);
    mRawOutWorker =
        std::make_shared<RKISP2OutputFrameWorker>(mCameraId, "RawWork",
                                            IMGU_NODE_RAW, pipelineDepth);
    // Pre allocate hal internal buffer in order to speed up some case need
    // allocate buffer temporary.
    // process unit in postpipeline which is not last unit will allocate
    // internal buffer, now just acquire from the PreAllocateBuffer pool.
    SensorFormat availableSensorFormat;
    int ret = PlatformData::getCameraHWInfo()->getAvailableSensorOutputFormats(cameraId, availableSensorFormat);
    if (ret != NO_ERROR)
        return ;

    struct SensorFrameSize &frameSize = availableSensorFormat.begin()->second.back();
    int w, h, fmt, usage, num;
    w = frameSize.max_width;
    h = frameSize.max_height;
    fmt = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
    usage = GRALLOC_USAGE_SW_READ_OFTEN |
            GRALLOC_USAGE_HW_CAMERA_WRITE|
            RK_GRALLOC_USAGE_SPECIFY_STRIDE|
            /* TODO: same as the temp solution in RKISP1CameraHw.cpp configStreams func
             * add GRALLOC_USAGE_HW_VIDEO_ENCODER is a temp patch for gpu bug:
             * gpu cant alloc a nv12 buffer when format is
             * HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED. Need gpu provide a patch */
            GRALLOC_USAGE_HW_VIDEO_ENCODER;

    /* TODO buffer num should consider postpipeline depth
     * */
    num = pipelineDepth;

    if (MemoryUtils::creatHandlerBufferPool(cameraId, w, h, fmt, usage, num) != OK) {
        LOGE("@%s : Pre allocate buffers failed, wxh(%d,%d), num:%d", __FUNCTION__, w, h, num);
    }
}

RKISP2ImguUnit::~RKISP2ImguUnit()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;

    if (mPollerThread) {
        status |= mPollerThread->requestExitAndWait();
        mPollerThread.reset();
    }

    requestExitAndWait();
    if (mMessageThread != nullptr) {
        mMessageThread.reset();
        mMessageThread = nullptr;
    }

    if (mMessagesUnderwork.size())
        LOGW("There are messages that are not processed %zu:", mMessagesUnderwork.size());
    if (mMessagesPending.size())
        LOGW("There are pending messages %zu:", mMessagesPending.size());

    mActiveStreams.blobStreams.clear();
    mActiveStreams.rawStreams.clear();
    mActiveStreams.yuvStreams.clear();

    cleanListener();
    clearWorkers();
    MemoryUtils::destroyHandleBufferPool(mCameraId);
}

status_t RKISP2ImguUnit::stopAllWorkers()
{
    status_t status= NO_ERROR;
    // Stop all video nodes
    status = mMainOutWorker->stopWorker();
    if (status != OK) {
        LOGE("Fail to stop main woker");
        return status;
    }
    status = mSelfOutWorker->stopWorker();
    if (status != OK) {
        LOGE("Fail to stop self woker");
        return status;
    }
    status = mRawOutWorker->stopWorker();
    if (status != OK) {
        LOGE("Fail to stop raw woker");
        return status;
    }
    return status;
}

void RKISP2ImguUnit::clearWorkers()
{
    for (size_t i = 0; i < PIPE_NUM; i++) {
        PipeConfiguration* config = &(mPipeConfigs[i]);
        config->deviceWorkers.clear();
        config->pollableWorkers.clear();
        config->nodes.clear();
    }

    mListenerDeviceWorkers.clear();
}

status_t
RKISP2ImguUnit::configStreams(std::vector<camera3_stream_t*> &activeStreams, bool configChanged)
{
    PERFORMANCE_ATRACE_NAME("RKISP2ImguUnit::configStreams");
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    LOGI("@%s %d: configChanged :%d", __FUNCTION__, __LINE__, configChanged);

    std::shared_ptr<RKISP2GraphConfig> graphConfig = mGCM.getBaseGraphConfig();

    mConfigChanged = configChanged;
    mActiveStreams.blobStreams.clear();
    mActiveStreams.rawStreams.clear();
    mActiveStreams.yuvStreams.clear();
    mActiveStreams.inputStream = nullptr;
    mFirstRequest = true;
    mNeedRestartPoll = true;
    mCurPipeConfig = nullptr;
    mTakingPicture = false;
    mFlushing = false;

    for (unsigned int i = 0; i < activeStreams.size(); ++i) {
        // treat CAMERA3_STREAM_BIDIRECTIONAL as combination with an input
        // stream and an output stream
        if (activeStreams.at(i)->stream_type == CAMERA3_STREAM_INPUT ||
            activeStreams.at(i)->stream_type == CAMERA3_STREAM_BIDIRECTIONAL) {
            mActiveStreams.inputStream = activeStreams.at(i);
            if (activeStreams.at(i)->stream_type == CAMERA3_STREAM_INPUT)
                continue;
        }

        switch (activeStreams.at(i)->format) {
        case HAL_PIXEL_FORMAT_BLOB:
             mActiveStreams.blobStreams.push_back(activeStreams.at(i));
             graphConfig->setPipeType(RKISP2GraphConfig::PIPE_STILL);
             break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
             mActiveStreams.yuvStreams.push_back(activeStreams.at(i));
             break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
             // Always put IMPL stream on the begin for mapping, in the
             // 3 stream case, IMPL is prefered to use for preview
             mActiveStreams.yuvStreams.insert(mActiveStreams.yuvStreams.begin(), activeStreams.at(i));
             break;
        case HAL_PIXEL_FORMAT_RAW_OPAQUE:
             mActiveStreams.rawStreams.push_back(activeStreams.at(i));
             break;
        default:
            LOGW("Unsupported stream format %d",
                 activeStreams.at(i)->format);
            break;
        }
    }
    status_t status = createProcessingTasks(graphConfig);
    if (status != NO_ERROR) {
       LOGE("Processing tasks creation failed (ret = %d)", status);
       return UNKNOWN_ERROR;
    }

    status = mPollerThread->init(mCurPipeConfig->nodes,
                                 this, POLLPRI | POLLIN | POLLOUT | POLLERR, false);

    if (status != NO_ERROR) {
       LOGE("PollerThread init failed (ret = %d)", status);
       return UNKNOWN_ERROR;
    }

    return OK;
}

status_t
RKISP2ImguUnit::configStreamsDone()
{
    PERFORMANCE_ATRACE_NAME("RKISP2ImguUnit::configStreamsDone");
    if(!mConfigChanged)
        return OK;
    /*
     * Moved from processNextRequest because this call will cost more than 300ms,
     * and cause CTS android.hardware.camera2.cts.RecordingTest#testBasicRecording
     * failed, which compares the frames numbers started to calculated from the
     * first request in 3 seconds to the recording file's.
     */
    status_t status = kickstart();
    if (status != OK) {
       return status;
    }

    int32_t duration = 30;    //default duration 30ms
    status = PlatformData::getCameraHWInfo()->getSensorFrameDuration(mCameraId, duration);
    if (status != NO_ERROR)
        LOGW("@%s : Can't get sensor frame duration", __FUNCTION__);

    // Notice: frame.initialSkip configured in camera3_profiles.xml should be
    // the(actual skipFrams - 2) for the driver will alway drop 2 frames.
    // the first frame can't be captured by ISP, the second frame will be
    // captured to dummy buffer.
    const RKISP2CameraCapInfo *cap = getRKISP2CameraCapInfo(mCameraId);
    int skipFrames = cap->frameInitialSkip();
    LOGD("@%s : skipFrames: %d, sensorFrameDuration: %d", __FUNCTION__, skipFrames, duration);
    usleep(skipFrames * duration * 1000);

    return status;
}

#define streamSizeGT(s1, s2) (((s1)->width * (s1)->height) > ((s2)->width * (s2)->height))
#define streamSizeEQ(s1, s2) (((s1)->width * (s1)->height) == ((s2)->width * (s2)->height))
#define streamSizeGE(s1, s2) (((s1)->width * (s1)->height) >= ((s2)->width * (s2)->height))
#define streamSizeRatio(s1) ((float)((s1)->width) / (s1)->height)

status_t RKISP2ImguUnit::mapStreamWithDeviceNode(int phyStreamsNum)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int blobNum = mActiveStreams.blobStreams.size();
    int yuvNum = mActiveStreams.yuvStreams.size();
    int streamNum = blobNum + yuvNum;

    if (blobNum > 1 || phyStreamsNum <= 0) {
        LOGE("Don't support blobNum %d, phyStreamsNum %d", blobNum, phyStreamsNum);
        return BAD_VALUE;
    }

    mStreamNodeMapping.clear();
    mStreamListenerMapping.clear();

    std::vector<camera3_stream_t *> availableStreams = mActiveStreams.yuvStreams;
    if (blobNum) {
        availableStreams.insert(availableStreams.begin(), mActiveStreams.blobStreams[0]);
    }

    LOGI("@%s, %d streams, blobNum:%d, yuvNum:%d", __FUNCTION__, streamNum, blobNum, yuvNum);

    // support up to 4 output streams, and because ISP hardware can only support
    // 2 output streams directly, so other two streams should be implemented as
    // listeners.
    int videoIdx = -1;
    int previewIdx = -1;
    std::vector<std::pair<int, NodeTypes>> listeners;

    if (streamNum == 1) {
        // Force use video, rk use the IMGU_NODE_VIDEO firstly.
        //if second stream is needed, then IMGU_NODE_VF_PREVIEW will be usde.
        //and rk has no IMGU_NODE_PV_PREVIEW.
        videoIdx = 0;
    } else if (streamNum == 2) {
        videoIdx = (streamSizeGE(availableStreams[0], availableStreams[1])) ? 0 : 1;
        if (phyStreamsNum > 1) {
            previewIdx = videoIdx ? 0 : 1;
        } else {
            std::pair<int, NodeTypes> listener;
            listener.first = videoIdx ? 0 : 1;
            listener.second = IMGU_NODE_VIDEO;
            listeners.push_back(listener);
        }
    } else if (streamNum == 3 || mActiveStreams.inputStream) {
        videoIdx = 0;
        // find the maxium size stream
        for (int i = 0; i < availableStreams.size(); i++) {
            if (streamSizeGT(availableStreams[i], availableStreams[videoIdx]))
                    videoIdx = i;
        }

        if (phyStreamsNum > 1) {
            for (int i = 0; i < availableStreams.size(); i++) {
                if (i == videoIdx ||
                    availableStreams[i]->width > SP_MAX_WIDTH ||
                    availableStreams[i]->height > SP_MAX_HEIGHT) {
                    continue ;
                } else {
                    if(previewIdx == -1)
                        previewIdx = i;
                    if (streamSizeGT(availableStreams[i], availableStreams[previewIdx]))
                        previewIdx = i;
                }
            }

            if (previewIdx == -1) {
                LOGE("@%s : No stream map to SP while phyStreams(%d) more than one", __FUNCTION__, phyStreamsNum);
                return UNKNOWN_ERROR;
            }

            // deal with listners
            float videoSizeRatio = streamSizeRatio(availableStreams[videoIdx]);
            float previewSizeRatio = streamSizeRatio(availableStreams[previewIdx]);
            float listenerSizeRatio = 0;
            for (int i = 0; i < availableStreams.size(); i++) {
                if (i != videoIdx && i != previewIdx) {
                    listenerSizeRatio = streamSizeRatio(availableStreams[i]);
                    std::pair<int, NodeTypes> listener;
                    listener.first = i;
                    float lpRatioDiff = fabs(listenerSizeRatio - previewSizeRatio);
                    float lvRatioDiff = fabs(listenerSizeRatio - videoSizeRatio);
                    if (fabs(lpRatioDiff - lvRatioDiff) <= 0.000001f) {
                        if (streamSizeEQ(availableStreams[i], availableStreams[videoIdx]))
                            listener.second = IMGU_NODE_VIDEO;
                        else if (streamSizeEQ(availableStreams[i], availableStreams[previewIdx]))
                            listener.second = IMGU_NODE_VF_PREVIEW;
                        else if (streamSizeGT(availableStreams[previewIdx], availableStreams[videoIdx]))
                            listener.second = IMGU_NODE_VF_PREVIEW;
                        else
                            listener.second = IMGU_NODE_VIDEO;
                    } else if (lpRatioDiff < lvRatioDiff) {
                        if (streamSizeGE(availableStreams[previewIdx], availableStreams[i]))
                            listener.second = IMGU_NODE_VF_PREVIEW;
                        else
                            listener.second = IMGU_NODE_VIDEO;
                    } else {
                        if (streamSizeGE(availableStreams[videoIdx], availableStreams[i]))
                            listener.second = IMGU_NODE_VIDEO;
                        else
                            listener.second = IMGU_NODE_VF_PREVIEW;
                    }
                    listeners.push_back(listener);
                }
            }
        } else {
            for (int i = 0; i < availableStreams.size(); i++) {
                if (i != videoIdx) {
                    std::pair<int, NodeTypes> listener;
                    listener.first = i;
                    listener.second = IMGU_NODE_VIDEO;
                    listeners.push_back(listener);
                }
           }
       }
    } else {
        LOGE("@%s, ERROR, blobNum:%d, yuvNum:%d", __FUNCTION__, blobNum, yuvNum);
        return UNKNOWN_ERROR;
    }

    if (previewIdx >= 0) {
        mStreamNodeMapping[IMGU_NODE_VF_PREVIEW] = availableStreams[previewIdx];
        mStreamNodeMapping[IMGU_NODE_PV_PREVIEW] = mStreamNodeMapping[IMGU_NODE_VF_PREVIEW];
        LOGD("@%s, %d stream %p size preview: %dx%d, format %s", __FUNCTION__,
             previewIdx, availableStreams[previewIdx],
             availableStreams[previewIdx]->width, availableStreams[previewIdx]->height,
             METAID2STR(android_scaler_availableFormats_values,
                        availableStreams[previewIdx]->format));
    }

    if (videoIdx >= 0) {
        mStreamNodeMapping[IMGU_NODE_VIDEO] = availableStreams[videoIdx];
        LOGI("@%s, %d stream %p size video: %dx%d, format %s", __FUNCTION__,
             videoIdx, availableStreams[videoIdx],
             availableStreams[videoIdx]->width, availableStreams[videoIdx]->height,
             METAID2STR(android_scaler_availableFormats_values,
                        availableStreams[videoIdx]->format));
    }

    for (auto iter : listeners) {
        mStreamListenerMapping[availableStreams[iter.first]] = iter.second;
        LOGI("@%s (%dx%d 0x%x), %p listen to 0x%x", __FUNCTION__,
             availableStreams[iter.first]->width, availableStreams[iter.first]->height,
             availableStreams[iter.first]->format, availableStreams[iter.first], iter.second);
    }

    if(mActiveStreams.rawStreams.size() != 0) {
        //raw stream listen to mp if mp output raw or mapping to rawWork
        if(!PlatformData::getCameraHWInfo()->isIspSupportRawPath())
            mStreamListenerMapping[mActiveStreams.rawStreams[0]] = IMGU_NODE_VIDEO;
        else
            mStreamNodeMapping[IMGU_NODE_RAW] = mActiveStreams.rawStreams[0];

    }

    return OK;
}

/**
 * Create the processing tasks and listening tasks.
 * Processing tasks are:
 *  - video task (wraps video pipeline)
 *  - capture task (wraps still capture)
 *  - raw bypass (not done yet)
 *
 * \param[in] activeStreams StreamConfig struct filled during configStreams
 * \param[in] graphConfig Configuration of the base graph
 */
status_t
RKISP2ImguUnit::createProcessingTasks(std::shared_ptr<RKISP2GraphConfig> graphConfig)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = OK;

    if (CC_UNLIKELY(graphConfig.get() == nullptr)) {
        LOGE("ERROR: Graph config is nullptr");
        return UNKNOWN_ERROR;
    }

    // rk only has video config, set it as default,zyc
    mCurPipeConfig = &mPipeConfigs[PIPE_VIDEO_INDEX];

    if(mConfigChanged) {
        //when need reconfig hw pipeline, all works should stop
        stopAllWorkers();
        clearWorkers();

        // Open and configure imgu video nodes
        status = mRKISP2MediaCtlHelper.configure(mGCM, RKISP2IStreamConfigProvider::CIO2);
        if (status != OK) {
            LOGE("Failed to configure input system.");
            return status;
        }

        status = mRKISP2MediaCtlHelper.configure(mGCM, RKISP2IStreamConfigProvider::IMGU_COMMON);
        if (status != OK)
            return UNKNOWN_ERROR;
        if (mGCM.getMediaCtlConfig(RKISP2IStreamConfigProvider::IMGU_STILL)) {
            status = mRKISP2MediaCtlHelper.configurePipe(mGCM, RKISP2IStreamConfigProvider::IMGU_STILL, true);
            if (status != OK)
                return UNKNOWN_ERROR;
            mCurPipeConfig = &mPipeConfigs[PIPE_STILL_INDEX];
        }
        // Set video pipe by default
        if (mGCM.getMediaCtlConfig(RKISP2IStreamConfigProvider::IMGU_VIDEO)) {
            status = mRKISP2MediaCtlHelper.configurePipe(mGCM, RKISP2IStreamConfigProvider::IMGU_VIDEO, true);
            if (status != OK)
                return UNKNOWN_ERROR;
            mCurPipeConfig = &mPipeConfigs[PIPE_VIDEO_INDEX];
        }
    } else {
        clearWorkers();
    }


    mConfiguredNodesPerName = mRKISP2MediaCtlHelper.getConfiguredNodesPerName();
    if (mConfiguredNodesPerName.size() == 0) {
        LOGD("No nodes present");
        return UNKNOWN_ERROR;
    }

    /* TODO */
    // Raw Path can not be considered as a normal phyStream now
    int phyStreamsNum = mConfiguredNodesPerName.size();

    LOGD("phyStreamsNum:%d",phyStreamsNum);
    for (auto it = mConfiguredNodesPerName.begin(); it != mConfiguredNodesPerName.end(); ++it) {
        if((*it).first == IMGU_NODE_RAW) {
            phyStreamsNum--;
            break;
        }
    }
    if (mapStreamWithDeviceNode(phyStreamsNum) != OK)
        return UNKNOWN_ERROR;

    PipeConfiguration* videoConfig = &(mPipeConfigs[PIPE_VIDEO_INDEX]);
    PipeConfiguration* stillConfig = &(mPipeConfigs[PIPE_STILL_INDEX]);

    std::shared_ptr<RKISP2OutputFrameWorker> vfWorker = nullptr;
    std::shared_ptr<RKISP2OutputFrameWorker> pvWorker = nullptr;
    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    camera_metadata_ro_entry entry;
    CLEAR(entry);
    if (meta)
        entry = MetadataHelper::getMetadataEntry(
            meta, ANDROID_REQUEST_PIPELINE_MAX_DEPTH);
    size_t pipelineDepth = entry.count == 1 ? entry.data.u8[0] : 1;
    LOGD("pipelineDepth:%d",pipelineDepth);
    for (const auto &it : mConfiguredNodesPerName) {
        if (it.first == IMGU_NODE_STILL || it.first == IMGU_NODE_VIDEO) {
            if(mStreamNodeMapping[it.first] == NULL)
                continue;
            
            LOGD("mMainOutWorker attach node:name :%s",it.second->name());
            mMainOutWorker->attachNode(it.second);
            mMainOutWorker->attachStream(mStreamNodeMapping[it.first]);

            videoConfig->deviceWorkers.push_back(mMainOutWorker);
            videoConfig->pollableWorkers.push_back(mMainOutWorker);
            videoConfig->nodes.push_back(mMainOutWorker->getNode());
            setStreamListeners(it.first, mMainOutWorker);
            //shutter event for non isys, zyc
            mListenerDeviceWorkers.push_back(mMainOutWorker.get());
        } else if (it.first == IMGU_NODE_VF_PREVIEW) {
            if(mStreamNodeMapping[it.first] == NULL)
                continue;
            LOGD("mSelfOutWorker IMGU_NODE_VF_PREVIEW  attach node:name :%s",it.second->name());
            mSelfOutWorker->attachNode(it.second);
            mSelfOutWorker->attachStream(mStreamNodeMapping[it.first]);

            videoConfig->deviceWorkers.push_back(mSelfOutWorker);
            videoConfig->pollableWorkers.push_back(mSelfOutWorker);
            videoConfig->nodes.push_back(mSelfOutWorker->getNode());
            setStreamListeners(it.first, mSelfOutWorker);
            //shutter event for non isys, zyc
            mListenerDeviceWorkers.push_back(mSelfOutWorker.get());
        } else if (it.first == IMGU_NODE_PV_PREVIEW) {
            if(mStreamNodeMapping[it.first] == NULL)
                continue;
            pvWorker = std::make_shared<RKISP2OutputFrameWorker>(mCameraId, "PVWork",
                it.first, pipelineDepth);

            pvWorker->attachNode(it.second);
            pvWorker->attachStream(mStreamNodeMapping[it.first]);

            setStreamListeners(it.first, pvWorker);
            //shutter event for non isys, zyc
            mListenerDeviceWorkers.push_back(pvWorker.get());
        } else if (it.first == IMGU_NODE_RAW) {
            if(mStreamNodeMapping[it.first] == NULL)
                continue;

            mRawOutWorker->attachNode(it.second);
            mRawOutWorker->attachStream(mStreamNodeMapping[it.first]);

            videoConfig->deviceWorkers.push_back(mRawOutWorker);
            videoConfig->pollableWorkers.push_back(mRawOutWorker);
            videoConfig->nodes.push_back(mRawOutWorker->getNode());
            setStreamListeners(it.first, mRawOutWorker);
            //shutter event for non isys, zyc
            mListenerDeviceWorkers.push_back(mRawOutWorker.get());
        } else {
            LOGE("Unknown NodeName: %d", it.first);
            return UNKNOWN_ERROR;
        }
    }

    if (pvWorker.get()) {
        // Copy common part for still pipe, then add pv
        *stillConfig = *videoConfig;
        stillConfig->deviceWorkers.insert(stillConfig->deviceWorkers.begin(), pvWorker);
        stillConfig->pollableWorkers.insert(stillConfig->pollableWorkers.begin(), pvWorker);
        stillConfig->nodes.insert(stillConfig->nodes.begin(), pvWorker->getNode());

        if (mCurPipeConfig == videoConfig) {
            LOGI("%s: configure postview in advance", __FUNCTION__);
            pvWorker->configure(mConfigChanged);
        }
    }

    // Prepare for video pipe
    if (vfWorker.get()) {
        videoConfig->deviceWorkers.insert(videoConfig->deviceWorkers.begin(), vfWorker);
        videoConfig->pollableWorkers.insert(videoConfig->pollableWorkers.begin(), vfWorker);
        videoConfig->nodes.insert(videoConfig->nodes.begin(), vfWorker->getNode());

        // vf node provides source frame during still preview instead of pv node.
        if (pvWorker.get()) {
            setStreamListeners(IMGU_NODE_PV_PREVIEW, vfWorker);
        }

        if (mCurPipeConfig == stillConfig) {
            LOGI("%s: configure preview in advance", __FUNCTION__);
            vfWorker->configure(mConfigChanged);
        }
    }

    if (mActiveStreams.inputStream) {
        std::vector<camera3_stream_t*> outStreams;

        outStreams.insert(outStreams.begin(), mActiveStreams.blobStreams.begin(),
                         mActiveStreams.blobStreams.end());
        outStreams.insert(outStreams.begin(), mActiveStreams.yuvStreams.begin(),
                         mActiveStreams.yuvStreams.end());

        std::shared_ptr<RKISP2InputFrameWorker> inWorker =
            std::make_shared<RKISP2InputFrameWorker>(mCameraId,
                mActiveStreams.inputStream, outStreams, pipelineDepth);

        videoConfig->deviceWorkers.insert(videoConfig->deviceWorkers.begin(), inWorker);
        mListenerDeviceWorkers.push_back(inWorker.get());
    }

    status_t ret = OK;
    for (const auto &it : mCurPipeConfig->deviceWorkers) {
        ret = (*it).configure(mConfigChanged);
        if (ret != OK) {
            LOGE("Failed to configure workers.");
            return ret;
        }
    }
    std::vector<RKISP2ICaptureEventSource*>::iterator it = mListenerDeviceWorkers.begin();
    for (;it != mListenerDeviceWorkers.end(); ++it) {
        for (const auto &listener : mListeners) {
            (*it)->attachListener(listener);
        }
    }

    return OK;
}

void RKISP2ImguUnit::setStreamListeners(NodeTypes nodeName,
                                  std::shared_ptr<RKISP2OutputFrameWorker>& source)
{
    for (const auto &it : mStreamListenerMapping) {
        if (it.second == nodeName) {
            LOGI("@%s stream %p listen to nodeName 0x%x",
                 __FUNCTION__, it.first, nodeName);
            source->addListener(it.first);
        }
    }
}

void
RKISP2ImguUnit::cleanListener()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    // clean all the listening tasks
    std::shared_ptr<RKISP2ITaskEventListener> lTask = nullptr;
    for (unsigned int i = 0; i < mListeningTasks.size(); i++) {
        lTask = mListeningTasks.at(i);
        if (lTask.get() == nullptr) {
            LOGE("Listening task null - BUG.");
        }
        else
            lTask->cleanListeners();
    }

    mListeningTasks.clear();
}

status_t RKISP2ImguUnit::attachListener(ICaptureEventListener *aListener)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    mListeners.push_back(aListener);

    return OK;
}

status_t
RKISP2ImguUnit::completeRequest(std::shared_ptr<RKISP2ProcUnitSettings> &processingSettings,
                          bool updateMeta)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Camera3Request *request = processingSettings->request;
    if (CC_UNLIKELY(request == nullptr)) {
        LOGE("ProcUnit: nullptr request - BUG");
        return UNKNOWN_ERROR;
    }
    const std::vector<camera3_stream_buffer> *outBufs = request->getOutputBuffers();
    const std::vector<camera3_stream_buffer> *inBufs = request->getInputBuffers();
    int reqId = request->getId();

    LOGD("@%s: Req id %d,  Num outbufs %zu Num inbufs %zu",
         __FUNCTION__, reqId, outBufs ? outBufs->size() : 0, inBufs ? inBufs->size() : 0);

    RKISP2ProcTaskMsg procMsg;
    procMsg.reqId = reqId;
    procMsg.processingSettings = processingSettings;

    MessageCallbackMetadata cbMetadataMsg;
    cbMetadataMsg.updateMeta = updateMeta;
    cbMetadataMsg.request = request;

    DeviceMessage msg;
    msg.id = MESSAGE_COMPLETE_REQ;
    msg.pMsg = procMsg;
    msg.cbMetadataMsg = cbMetadataMsg;
    mMessageQueue.send(&msg);

    return NO_ERROR;
}

status_t
RKISP2ImguUnit::handleMessageCompleteReq(DeviceMessage &msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = OK;

    Camera3Request *request = msg.cbMetadataMsg.request;
    if (request == nullptr) {
        LOGE("Request is nullptr");
        return BAD_VALUE;
    }
    std::shared_ptr<DeviceMessage> tmp = std::make_shared<DeviceMessage>(msg);
    mMessagesPending.push_back(tmp);

    mCurPipeConfig->nodes.clear();
    status = processNextRequest();
    if (status != OK) {
        LOGE("Process request %d failed", request->getId());
        request->setError();
    }

    /**
     * Send poll request for every requests(even when error), so that we can
     * handle them in the right order.
     */
    if (mCurPipeConfig->nodes.size() > 0)
        status |= mPollerThread->pollRequest(request->getId(), 3000,
                                             &(mCurPipeConfig->nodes));
    return status;
}

status_t RKISP2ImguUnit::processNextRequest()
{
    status_t status = NO_ERROR;
    std::shared_ptr<DeviceMessage> msg = nullptr;
    Camera3Request *request = nullptr;

    LOGD("%s: pending size %zu,underwork.size(%d), state %d", __FUNCTION__, mMessagesPending.size(), mMessagesUnderwork.size(), mState);
    if (mMessagesPending.empty())
        return NO_ERROR;

    msg = mMessagesPending[0];
    mMessagesPending.erase(mMessagesPending.begin());

    // update and return metadata firstly
    request = msg->cbMetadataMsg.request;
    if (request == nullptr) {
        LOGE("Request is nullptr");
        // Ignore this request
        return NO_ERROR;
    }
    LOGD("@%s:handleExecuteReq for Req id %d, ", __FUNCTION__, request->getId());

    mMessagesUnderwork.push_back(msg);

    // Pass settings to the listening tasks *before* sending metadata
    // up to framework. Some tasks might need e.g. the result data.
    std::shared_ptr<RKISP2ITaskEventListener> lTask = nullptr;
    for (unsigned int i = 0; i < mListeningTasks.size(); i++) {
        lTask = mListeningTasks.at(i);
        if (lTask.get() == nullptr) {
            LOGE("Listening task null - BUG.");
            return UNKNOWN_ERROR;
        }
        status |= lTask->settings(msg->pMsg);
    }

    mCurPipeConfig->nodes.clear();
    mRequestToWorkMap[request->getId()].clear();

    std::vector<std::shared_ptr<RKISP2IDeviceWorker>>::iterator it = mCurPipeConfig->deviceWorkers.begin();
    for (;it != mCurPipeConfig->deviceWorkers.end(); ++it) {
        // construct an dummy poll event for RKISP2InputFrameWorker
        // notice that this would cause poll event disorder,
        // so we should do some workaround in startProcessing.
        if ((*it)->getNode().get() == nullptr && request->getInputBuffers()->size() > 0) {
            mRequestToWorkMap[request->getId()].push_back(*it);
            MessageCallbackMetadata cbMetadataMsg;
            cbMetadataMsg.request = request;

            DeviceMessage dummyMsg;
            dummyMsg.pollEvent.requestId = request->getId();
            dummyMsg.pollEvent.numDevices = 0;
            dummyMsg.pollEvent.polledDevices = 0;
            dummyMsg.pollEvent.activeDevices = nullptr;
            dummyMsg.id = MESSAGE_ID_POLL;
            dummyMsg.cbMetadataMsg = cbMetadataMsg;
            status |= (*it)->prepareRun(msg);
            mMessageQueue.send(&dummyMsg);
            return status;
        } else
            status |= (*it)->prepareRun(msg);
    }

    std::vector<std::shared_ptr<RKISP2FrameWorker>>::iterator pollDevice = mCurPipeConfig->pollableWorkers.begin();
    for (;pollDevice != mCurPipeConfig->pollableWorkers.end(); ++pollDevice) {
        bool needsPolling = (*pollDevice)->needPolling();
        if (needsPolling) {
            if (request->getInputBuffers()->size() == 0)
                mCurPipeConfig->nodes.push_back((*pollDevice)->getNode());
            mRequestToWorkMap[request->getId()].push_back((std::shared_ptr<RKISP2IDeviceWorker>&)(*pollDevice));
        }
    }

    return status;
}

status_t
RKISP2ImguUnit::kickstart()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = OK;

    for (const auto &it : mCurPipeConfig->deviceWorkers) {
        status = (*it).startWorker();
        if (status != OK) {
            LOGE("Failed to start workers.");
            return status;
        }
    }

    mFirstRequest = false;
    return status;
}

/**
 * Start the processing task for each input buffer.
 * Each of the input buffers has an associated terminal id. This is the
 * destination terminal id. This terminal id is the input terminal for one
 * or the execute tasks we have.
 *
 * Check the map that links the input terminals of the pipelines to the
 * tasks that wrap them to decide which tasks need to be executed.
 */
status_t
RKISP2ImguUnit::startProcessing(DeviceMessage pollmsg)
{
    PERFORMANCE_ATRACE_CALL();

    status_t status = OK;
    std::shared_ptr<V4L2VideoNode> *activeNodes = pollmsg.pollEvent.activeDevices;
    int processReqNum = 1;
    bool deviceError = pollmsg.pollEvent.polledDevices && !activeNodes;

    std::shared_ptr<DeviceMessage> msg;
    Camera3Request *request;

    if (mMessagesUnderwork.empty())
        return status;

    msg = *(mMessagesUnderwork.begin());
    request = msg->cbMetadataMsg.request;
    unsigned int reqId = pollmsg.pollEvent.requestId;

    if (request->getId() < reqId) {
        // poll event may disorder, and we should process it in
        // order, so add it to the delay queue that will be processed
        // later.
        LOGD("%s: poll event disorder, exp %d, real %d", __FUNCTION__,
              request->getId(), reqId);
        mDelayProcessRequest.push_back(reqId);
        return status;
    } else if (request->getId() > reqId) {
        LOGE("%s: request id dont match: exp %d, real %d", __FUNCTION__,
              request->getId(), reqId);
        return UNKNOWN_ERROR;
    }

    if (mDelayProcessRequest.size() > 0) {
        unsigned int startId = reqId + 1;
        int i = 0;
        for (; i < mDelayProcessRequest.size(); i++) {
            if (mDelayProcessRequest[i] != startId)
                break;
            processReqNum++;
            startId++;
        }

        while (i > 0 && processReqNum > 1) {
            mDelayProcessRequest.erase(mDelayProcessRequest.begin());
            i--;
        }
    }

    /* tell workers and AAL that device error occured */
    if (deviceError && request->getInputBuffers()->size() == 0) {
        for (const auto &it : mCurPipeConfig->deviceWorkers)
             (*it).deviceError();

        if (mErrCb)
            mErrCb->deviceError();
        /* clear the polling msg*/
        mPollerThread->flush(false);
        processReqNum =  mMessagesUnderwork.size();
    }

    for ( int i = 0; i < processReqNum; i++) {
        msg = *(mMessagesUnderwork.begin());
        request = msg->cbMetadataMsg.request;
        reqId = request->getId();
        std::vector<std::shared_ptr<RKISP2IDeviceWorker>>::iterator it = mRequestToWorkMap[reqId].begin();
        for (;it != mRequestToWorkMap[reqId].end(); ++it) {
            std::shared_ptr<RKISP2FrameWorker> worker = (std::shared_ptr<RKISP2FrameWorker>&)(*it);
            status |= worker->asyncPollDone(*(mMessagesUnderwork.begin()), true);
        }

        it = mRequestToWorkMap[reqId].begin();
        for (;it != mRequestToWorkMap[reqId].end(); ++it) {
            status |= (*it)->run();
        }

        it = mRequestToWorkMap[reqId].begin();
        for (;it != mRequestToWorkMap[reqId].end(); ++it) {
            status |= (*it)->postRun();
        }
        mRequestToWorkMap.erase(reqId);

        // Report request error when anything wrong
        if (status != OK || deviceError)
            request->setError();

        //HACK: return metadata after updated it
        LOGI("%s: request %d done", __func__, request->getId());
        ICaptureEventListener::CaptureMessage outMsg;
        outMsg.data.event.reqId = request->getId();
        outMsg.data.event.type = ICaptureEventListener::CAPTURE_REQUEST_DONE;
        outMsg.id = ICaptureEventListener::CAPTURE_MESSAGE_ID_EVENT;
        for (const auto &listener : mListeners)
            listener->notifyCaptureEvent(&outMsg);

        mMessagesUnderwork.erase(mMessagesUnderwork.begin());
    }

    return status;
}

status_t RKISP2ImguUnit::notifyPollEvent(PollEventMessage *pollMsg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (pollMsg == nullptr || pollMsg->data.activeDevices == nullptr)
        return BAD_VALUE;

    // Common thread message fields for any case
    DeviceMessage msg;
    msg.pollEvent.pollMsgId = pollMsg->id;
    msg.pollEvent.requestId = pollMsg->data.reqId;

    if (pollMsg->id == POLL_EVENT_ID_EVENT) {
        int numDevices = pollMsg->data.activeDevices->size();
        if (numDevices == 0) {
            LOGI("@%s: devices flushed", __FUNCTION__);
            return OK;
        }

        int numPolledDevices = pollMsg->data.polledDevices->size();
        if (CC_UNLIKELY(numPolledDevices == 0)) {
            LOGW("No devices Polled?");
            return OK;
        }

        msg.pollEvent.activeDevices = new std::shared_ptr<V4L2VideoNode>[numDevices];
        for (int i = 0; i < numDevices; i++) {
            msg.pollEvent.activeDevices[i] = (std::shared_ptr<V4L2VideoNode>&) pollMsg->data.activeDevices->at(i);
        }
        msg.pollEvent.numDevices = numDevices;
        msg.pollEvent.polledDevices = numPolledDevices;

        if (pollMsg->data.activeDevices->size() != pollMsg->data.polledDevices->size()) {
            LOGD("@%s: %zu inactive nodes for request %u, retry poll", __FUNCTION__,
                                                                      pollMsg->data.inactiveDevices->size(),
                                                                      pollMsg->data.reqId);
            pollMsg->data.polledDevices->clear();
            *pollMsg->data.polledDevices = *pollMsg->data.inactiveDevices; // retry with inactive devices

            delete [] msg.pollEvent.activeDevices;

            return -EAGAIN;
        }

        {
            std::lock_guard<std::mutex> l(mFlushMutex);
            if (mFlushing)
                return OK;
            msg.id = MESSAGE_ID_POLL;
            mMessageQueue.send(&msg, MESSAGE_ID_POLL);
        }

    } else if (pollMsg->id == POLL_EVENT_ID_ERROR) {
        LOGE("Device poll failed");
        // For now, set number of device to zero in error case
        msg.pollEvent.numDevices = 0;
        msg.pollEvent.polledDevices = pollMsg->data.polledDevices->size();
        msg.id = MESSAGE_ID_POLL;

        mMessageQueue.send(&msg);
    } else {
        LOGW("unknown poll event id (%d)", pollMsg->id);
    }

    return OK;
}

status_t RKISP2ImguUnit::handleMessagePoll(DeviceMessage msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    status_t status = startProcessing(msg);

    if (msg.pollEvent.activeDevices)
        delete [] msg.pollEvent.activeDevices;
    msg.pollEvent.activeDevices = nullptr;

    return status;
}

void RKISP2ImguUnit::getConfigedHwPathSize(const char* pathName, uint32_t &size)
{
    mRKISP2MediaCtlHelper.getConfigedHwPathSize(pathName, size);
}

void RKISP2ImguUnit::getConfigedSensorOutputSize(uint32_t &size)
{
    mRKISP2MediaCtlHelper.getConfigedSensorOutputSize(size);
}

void
RKISP2ImguUnit::messageThreadLoop(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    mThreadRunning = true;
    while (mThreadRunning) {
        status_t status = NO_ERROR;

        PERFORMANCE_ATRACE_BEGIN("Imgu-PollMsg");
        DeviceMessage msg;
        mMessageQueue.receive(&msg);
        PERFORMANCE_ATRACE_END();

        PERFORMANCE_ATRACE_NAME_SNPRINTF("Imgu-%s", ENUM2STR(ImguMsg_stringEnum, msg.id));
        PERFORMANCE_HAL_ATRACE_PARAM1("msg", msg.id);
        LOGD("@%s, receive message id:%d", __FUNCTION__, msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;
        case MESSAGE_COMPLETE_REQ:
            status = handleMessageCompleteReq(msg);
            break;
        case MESSAGE_ID_POLL:
        case MESSAGE_ID_POLL_META:
            status = handleMessagePoll(msg);
            break;
        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;
        default:
            LOGE("ERROR Unknown message %d in thread loop", msg.id);
            status = BAD_VALUE;
            break;
        }
        if (status != NO_ERROR)
            LOGE("error %d in handling message: %d",
                 status, static_cast<int>(msg.id));
        LOGD("@%s, finish message id:%d", __FUNCTION__, msg.id);
        mMessageQueue.reply(msg.id, status);
        PERFORMANCE_ATRACE_END();
    }
    LOGD("%s: Exit", __FUNCTION__);
}

status_t
RKISP2ImguUnit::handleMessageExit(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    mThreadRunning = false;
    return NO_ERROR;
}

status_t
RKISP2ImguUnit::requestExitAndWait(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    DeviceMessage msg;
    msg.id = MESSAGE_ID_EXIT;
    status_t status = mMessageQueue.send(&msg, MESSAGE_ID_EXIT);
    status |= mMessageThread->requestExitAndWait();

    status |= stopAllWorkers();
    clearWorkers();
    return status;
}

status_t
RKISP2ImguUnit::flush(void)
{
    PERFORMANCE_ATRACE_NAME("RKISP2ImguUnit::flush");
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    DeviceMessage msg;
    msg.id = MESSAGE_ID_FLUSH;

    {
        std::lock_guard<std::mutex> l(mFlushMutex);
        mFlushing = true;
    }

    mMessageQueue.remove(MESSAGE_ID_POLL);

    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t
RKISP2ImguUnit::handleMessageFlush(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    mPollerThread->flush(true);
    // flush all video nodes
    if (mCurPipeConfig) {
        status_t status;
        for (const auto &it : mCurPipeConfig->deviceWorkers) {
            status = (*it).flushWorker();
            if (status != OK) {
                LOGE("Fail to flush wokers");
                return status;
            }
        }
    }

    return NO_ERROR;
}

} /* namespace rksip2 */
} /* namespace camera2 */
} /* namespace android */
