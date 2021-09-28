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

#ifndef _CAMERA3_REQUESTTHREAD_H_
#define _CAMERA3_REQUESTTHREAD_H_

#include "MessageThread.h"
#include "MessageQueue.h"
#include "ResultProcessor.h"
#include "ItemPool.h"
#include <hardware/camera3.h>

NAMESPACE_DECLARATION {

/**
 * \class RequestThread
 * Active object in charge of request management
 *
 * The RequestThread  is the in charge of controlling the flow of request from
 * the client to the HW class.
 */
class RequestThread: public IMessageHandler,
                     public MessageThread {
public:
    RequestThread(int cameraId, ICameraHw *aCameraHW);
    virtual ~RequestThread();

    status_t init(const camera3_callback_ops_t *callback_ops);
    status_t deinit(void);

    status_t configureStreams(camera3_stream_configuration_t *stream_list);
    status_t constructDefaultRequest(int type, camera_metadata_t** meta);
    status_t processCaptureRequest(camera3_capture_request_t *request);
    status_t flush();

    /* IMessageHandler override */
    void messageThreadLoop(void);

    int returnRequest(Camera3Request* req);

    void dump(int fd);

    enum RequestBlockAction {
        REQBLK_NONBLOCKING = NO_ERROR,      /* request is non blocking */
        REQBLK_WAIT_ALL_PREVIOUS_COMPLETED, /* wait all previous requests completed */
        REQBLK_WAIT_ONE_REQUEST_COMPLETED,  /* the count of request in process reached the max,
                                              wait at least one request is completed */
        REQBLK_WAIT_ALL_PREVIOUS_COMPLETED_AND_FENCE_SIGNALED, /* wait all previous requests completed
                                                                  and all buffers fence signaled*/
        REQBLK_UNKOWN_ERROR,                /* unknow issue */
    };

private:  /* types */

    enum MessageId {
        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait

        MESSAGE_ID_REQUEST_DONE,
        MESSAGE_ID_FLUSH,

        // For HAL API
        MESSAGE_ID_CONFIGURE_STREAMS,
        MESSAGE_ID_CONSTRUCT_DEFAULT_REQUEST,
        MESSAGE_ID_PROCESS_CAPTURE_REQUEST,

        MESSAGE_ID_MAX
    };

    struct MessageConfigureStreams {
        camera3_stream_configuration_t * list;
    };

    struct MessageRegisterStreamBuffers {
        const camera3_stream_buffer_set_t * set;
    };

    struct MessageConstructDefaultRequest {
        int type;
        camera_metadata_t ** request;
    };

    struct MessageProcessCaptureRequest {
        camera3_capture_request * request3;
    };

    struct MessageShutter {
        int reqId;
        int64_t time;
    };

    struct MessageCaptureDone {
        int reqId;
        int64_t time;
        struct timeval timestamp;
    };

    struct MessageStreamOutDone {
        int reqId;
        int finished;
        status_t status;
    };

    // union of all message data
    union MessageData {
        MessageConfigureStreams streams;
        MessageRegisterStreamBuffers buffers;
        MessageConstructDefaultRequest defaultRequest;
        MessageProcessCaptureRequest request3;
        MessageShutter shutter;
        MessageCaptureDone capture;
        MessageStreamOutDone streamOut;
    };

    // message id and message data
    struct Message {
        Message() : id(MESSAGE_ID_MAX), request(nullptr) { CLEAR(data); }
        MessageId id;
        MessageData data;
        Camera3Request *request;
    };

private:  /* methods */

    status_t handleConfigureStreams(Message & msg);
    status_t handleConstructDefaultRequest(Message & msg);
    status_t handleProcessCaptureRequest(Message & msg);
    int handleReturnRequest(Message & msg);
    void recycleRequest(Camera3Request* request);
    void waitRequestsDrain();

    status_t captureRequest(Camera3Request* request);

    bool areAllStreamsUnderMaxBuffers() const;
    void deleteStreams(bool inactiveOnly);

private:  /* members */
    int mCameraId;
    ICameraHw   *mCameraHw; /* allocate from outside and should not delete in here */
    MessageQueue<Message, MessageId> mMessageQueue;
    ItemPool<Camera3Request> mRequestsPool;
    bool mThreadRunning;

    int mRequestsInHAL;
    bool mFlushing;
    Camera3Request* mWaitingRequest;  /*!< storage during need to wait for
                                           captures to be finished.
                                           It is one item from mRequestsPool */
    int mBlockAction;   /*!< the action if request is blocked */
    CameraMetadata mLastSettings;

    bool mInitialized;  /*!< tracking the status of the RequestThread */
    /* *********************************************************************
     * Stream info
     */
    ResultProcessor* mResultProcessor;
    std::vector<camera3_stream_t *> mStreams; /* Map to camera3_stream_t from framework
                                              which are not allocated here */
    std::vector<CameraStream*> mLocalStreams; /* Local storage of streaming informations */
    /* the request has been done to framework, but the buffers are still been
     * processing in hal holding the release fence */
    std::vector<Camera3Request*> mActiveRequest;
    uint8_t mPipelineDepth;
    unsigned int mStreamSeqNo;

};

} NAMESPACE_DECLARATION_END
#endif /* _CAMERA3_REQUESTTHREAD_H_ */
