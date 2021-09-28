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

#ifndef CAMERA3_HAL_RKISP2ControlUnit_H_
#define CAMERA3_HAL_RKISP2ControlUnit_H_
#include <memory>
#include <vector>
//#include <linux/rkisp1-config_v12.h>
#include "MessageQueue.h"
#include "MessageThread.h"
#include "RKISP2ImguUnit.h"
#include "SharedItemPool.h"
#include "RKISP2CaptureUnitSettings.h"
#include "RKISP2RequestCtrlState.h"
#include "RKISP2CtrlLoop.h"

extern "C" {
    typedef void (metadata_result_callback)(
        const struct cl_result_callback_ops *ops,
        struct rkisp_cl_frame_metadata_s *result);
}

namespace android {
namespace camera2 {



namespace rkisp2 {
class RKISP2Metadata;
struct RKISP2ProcUnitSettings;

class RKISP2SettingsProcessor;


// Forward definitions to avoid extra includes
class RKISP2IStreamConfigProvider;


class SocCamFlashCtrUnit {
public:
    explicit SocCamFlashCtrUnit(const char* name, int CameraId);
    virtual ~SocCamFlashCtrUnit();
    int setFlashSettings(const CameraMetadata *settings);
    int updateFlashResult(CameraMetadata *result);
    void setMeanLuma(float luma);
private:  /* Methods */
    // prevent copy constructor and assignment operator
    SocCamFlashCtrUnit(const SocCamFlashCtrUnit& other);
    SocCamFlashCtrUnit& operator=(const SocCamFlashCtrUnit& other);
    int setV4lFlashMode(int mode, int power, int timeout, int strobe);
    std::shared_ptr<V4L2Subdevice> mFlSubdev;
    int mV4lFlashMode;
    int mAePreTrigger;
    int mAeTrigFrms;
    uint8_t mAeFlashMode;
    uint8_t mAeMode;
    uint8_t mAeState;
    float mMeanLuma;
};

/**
 * \class RKISP2ControlUnit
 *
 * RKISP2ControlUnit class control the request flow between Capture Unit and
 * Processing Unit. It uses the Rockchip3Aplus to process 3A settings for
 * each request and to run the 3A algorithms.
 *
 */
class RKISP2ControlUnit : public IMessageHandler, public ICaptureEventListener,
                    private cl_result_callback_ops
{
public:
    explicit RKISP2ControlUnit(RKISP2ImguUnit *thePU,
                         int CameraId,
                         RKISP2IStreamConfigProvider &aStreamCfgProv,
                         std::shared_ptr<MediaController> mc);
    virtual ~RKISP2ControlUnit();

    status_t init();
    status_t configStreams(std::vector<camera3_stream_t*> &activeStreams, bool configChanged);

    status_t processRequest(Camera3Request* request,
                            std::shared_ptr<RKISP2GraphConfig> graphConfig);

    /* ICaptureEventListener interface*/
    bool notifyCaptureEvent(CaptureMessage *captureMsg);

    enum {
        FLUSH_FOR_NOCHANGE,
        FLUSH_FOR_STILLCAP,
        FLUSH_FOR_PREVIEW,
    };
    status_t flush(int configChanged = FLUSH_FOR_PREVIEW);
public:  /* private types */
    // thread message id's
    enum MessageId {
        MESSAGE_ID_EXIT = 0,
        MESSAGE_ID_NEW_REQUEST,
        MESSAGE_ID_NEW_SHUTTER,
        MESSAGE_ID_NEW_REQUEST_DONE,
        MESSAGE_ID_METADATA_RECEIVED,
        MESSAGE_ID_STILL_CAP_DONE,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_MAX
    };

    struct MessageGeneric {
        bool enable;
    };

    struct MessageRequest {
        unsigned int frame_number;
    };

    struct MessageShutter {
        int requestId;
        int64_t tv_sec;
        int64_t tv_usec;
    };

    // union of all message data
    union MessageData {
        MessageGeneric generic;
        MessageRequest request;
        MessageShutter shutter;
    };

    // message id and message data
    struct Message {
        MessageId id;
        int configChanged;
        unsigned int requestId; /**< For raw buffers from CaptureUnit as
                                     they don't have request */
        MessageData data;
        Camera3Request* request;
        std::shared_ptr<RKISP2RequestCtrlState> state;
        CameraMetadata metas;
        CaptureEventType type;
        Message(): id(MESSAGE_ID_EXIT),
            requestId(0),
            request(nullptr),
            state(nullptr),
            type(CAPTURE_EVENT_MAX)
        { CLEAR(data); }
    };

private:
    typedef struct {
        int reqId;
        RKISP2CaptureUnitSettings *captureSettings;
    } RequestSettings;

private:  /* Methods */
    // prevent copy constructor and assignment operator
    RKISP2ControlUnit(const RKISP2ControlUnit& other);
    RKISP2ControlUnit& operator=(const RKISP2ControlUnit& other);

    status_t initTonemaps();
    status_t requestExitAndWait();

    /* IMessageHandler overloads */
    virtual void messageThreadLoop();

    status_t handleMessageExit();
    status_t handleNewRequest(Message &msg);
    status_t handleNewRequestDone(Message &msg);
    status_t handleMetadataReceived(Message &msg);
    status_t handleNewShutter(Message &msg);
    status_t handleMessageFlush(Message &msg);

    status_t processRequestForCapture(std::shared_ptr<RKISP2RequestCtrlState> &reqState);

    status_t completeProcessing(std::shared_ptr<RKISP2RequestCtrlState> &reqState);
    status_t acquireRequestStateStruct(std::shared_ptr<RKISP2RequestCtrlState>& state);
    status_t initStaticMetadata();
    status_t metadataReceived(int id, const camera_metadata_t *metas);
    status_t fillMetadata(std::shared_ptr<RKISP2RequestCtrlState> &reqState);
    status_t getDevicesPath();
    status_t processSoCSettings(const CameraMetadata *settings);

private:  /* Members */
    SharedItemPool<RKISP2RequestCtrlState> mRequestStatePool;
    SharedItemPool<RKISP2CaptureUnitSettings> mCaptureUnitSettingsPool;
    SharedItemPool<RKISP2ProcUnitSettings> mProcUnitSettingsPool;

    std::map<int, std::shared_ptr<RKISP2RequestCtrlState>> mWaitingForCapture;
    CameraMetadata mLatestAiqMetadata;
    int64_t mLatestRequestId;

    RKISP2ImguUnit       *mImguUnit; /* RKISP2ControlUnit doesn't own ImguUnit */
    RKISP2CtrlLoop     *mCtrlLoop;
    bool            mEnable3A;
    int             mCameraId;

    std::shared_ptr<MediaController>             mMediaCtl;

    /**
     * Thread control members
     */
    bool mThreadRunning;
    MessageQueue<Message, MessageId> mMessageQueue;
    std::unique_ptr<MessageThread> mMessageThread;

    /**
     * Settings history
     */
    static const int16_t MAX_SETTINGS_HISTORY_SIZE = 10;
    std::vector<std::shared_ptr<RKISP2CaptureUnitSettings>>    mSettingsHistory;

    /*
     * Provider of details of the stream configuration
     */
    RKISP2IStreamConfigProvider &mStreamCfgProv;
    RKISP2SettingsProcessor *mRKISP2SettingsProcessor;
    RKISP2Metadata *mMetadata;

    int mSensorSettingsDelay;
    int mGainDelay;
    bool mLensSupported;
    bool mFlashSupported;

    uint32_t mSofSequence;
    int64_t mShutterDoneReqId;
    static const int16_t AWB_CONVERGENCE_WAIT_COUNT = 2;
    enum DevPathType {
        KDevPathTypeIspDevNode,
        KDevPathTypeIspStatsNode,
        KDevPathTypeIspInputParamsNode,
        KDevPathTypeSensorNode,
        KDevPathTypeLensNode,
        // deprecated, one sensor may have more than one
        // flash
        KDevPathTypeFlNode
    };
    std::map<enum DevPathType, std::string> mDevPathsMap;
    std::shared_ptr<V4L2Subdevice> mSensorSubdev;
    std::unique_ptr<SocCamFlashCtrUnit> mSocCamFlashCtrUnit;
    /**
     * Static callback forwarding methods from CL to instance
     */
    static ::metadata_result_callback sMetadatCb;
    bool mStillCapSyncNeeded;
    typedef enum StillCapSyncState_e {
        STILL_CAP_SYNC_STATE_TO_ENGINE_IDLE,
        STILL_CAP_SYNC_STATE_TO_ENGINE_PRECAP,
        STILL_CAP_SYNC_STATE_FORCE_TO_ENGINE_PRECAP,
        STILL_CAP_SYNC_STATE_FORCE_PRECAP_DONE,
        STILL_CAP_SYNC_STATE_TO_ENGINE_START,
        STILL_CAP_SYNC_STATE_WAITING_ENGINE_DONE,
        STILL_CAP_SYNC_STATE_FROM_ENGINE_DONE,
        STILL_CAP_SYNC_STATE_WATING_JPEG_FRAME,
        STILL_CAP_SYNC_STATE_JPEG_FRAME_DONE,
    } StillCapSyncState_e ;
    StillCapSyncState_e mStillCapSyncState;
    int mFlushForUseCase;
    CameraMetadata mLatestCamMeta;
};  // class RKISP2ControlUnit

const element_value_t CtlUMsg_stringEnum[] = {
    {"MESSAGE_ID_EXIT", RKISP2ControlUnit::MESSAGE_ID_EXIT },
    {"MESSAGE_ID_NEW_REQUEST", RKISP2ControlUnit::MESSAGE_ID_NEW_REQUEST },
    {"MESSAGE_ID_NEW_SHUTTER", RKISP2ControlUnit::MESSAGE_ID_NEW_SHUTTER },
    {"MESSAGE_ID_NEW_REQUEST_DONE", RKISP2ControlUnit::MESSAGE_ID_NEW_REQUEST_DONE },
    {"MESSAGE_ID_METADATA_RECEIVED", RKISP2ControlUnit::MESSAGE_ID_METADATA_RECEIVED },
    {"MESSAGE_ID_STILL_CAP_DONE", RKISP2ControlUnit::MESSAGE_ID_STILL_CAP_DONE},
    {"MESSAGE_ID_FLUSH", RKISP2ControlUnit::MESSAGE_ID_FLUSH },
    {"MESSAGE_ID_MAX", RKISP2ControlUnit::MESSAGE_ID_MAX },
};

}  // namespace rkisp2
}  // namespace camera2
}  // namespace android

#endif  // CAMERA3_HAL_RKISP2ControlUnit_H_
