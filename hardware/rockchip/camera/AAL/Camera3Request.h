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
#ifndef _CAMERA3_HAL_CAMERA3REQUEST_H_
#define _CAMERA3_HAL_CAMERA3REQUEST_H_

#include <CameraMetadata.h>
#include <hardware/camera3.h>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>             // std::chrono::second

#include "CameraStreamNode.h"
#include "CameraBuffer.h"
USING_METADATA_NAMESPACE;
NAMESPACE_DECLARATION {

/**
 * This define is only used for the purpose of the allocation of output buffer
 * pool
 * The exact value for this should be coming from the static metadata tag
 * maxNumOutputStreams. But at this stage we cannot query it because we do
 * not know the camera id. This value should always be bigger than the static
 * tag.
 */
#define MAX_NUMBER_OUTPUT_STREAMS 8


/**
 * \class IRequestCallback
 *
 * This interface is implemented by the ResultProcessor
 * It is used by CameraStreams to report that an output buffer that belongs to
 * a particular request is done
 * It is used by PSL entities to report that  part of the result information is
 * ready
 */
class IRequestCallback {
public:
    virtual ~IRequestCallback() {}

    virtual status_t shutterDone(Camera3Request* request,
                                 int64_t timestamp) = 0;

    virtual status_t metadataDone(Camera3Request* request,
                                  int resultIndex = -1) = 0;

    virtual status_t bufferDone(Camera3Request* request,
                                std::shared_ptr<CameraBuffer> buffer) = 0;
};


// generic template for objects that are shared among threads
// If you see deadlocks with SharedObject, you probably didn't let the previous
// incarnation around the same object to go out of scope (destructor releases).
template<typename TYPE, typename MEMBERTYPE>
class SharedObject {
public:
    explicit SharedObject(TYPE &p) :
        mMembers(p.mMembers), mSharedObject(nullptr), mRefSharedObject(p) {
        mRefSharedObject.mLock.lock();
    }
    explicit SharedObject(TYPE* p) :
        mMembers(p->mMembers), mSharedObject(p), mRefSharedObject(*p) {
        mSharedObject->mLock.lock();
    }
    ~SharedObject() {
        mRefSharedObject.mLock.unlock();
    }
    MEMBERTYPE &mMembers;
private:
    SharedObject();
    SharedObject(const SharedObject &);
    SharedObject &operator=(const SharedObject &);
    TYPE* mSharedObject;
    TYPE &mRefSharedObject;
};

/**
 * \class Camera3Request
 *
 * Internal representation of a user request (capture or re-process)
 * Objects of this class are initialized for each capture request received
 * by the camera device. Once those objects are initialized the request is safe
 * for processing by the Platform Specific Layer.
 *
 * Basic integrity checks are performed on initialization.
 * The class also has other utility methods to ease the PSL implementations
 *
 *
 */
class Camera3Request {
public:
    Camera3Request();
    virtual ~Camera3Request();
    status_t init(camera3_capture_request* req,
                  IRequestCallback* cb, CameraMetadata &settings, int cameraId);
    void deInit();

    /* access methods */
    unsigned int getNumberOutputBufs();
    unsigned int getNumberInputBufs();
             int getBufferCountOfFormat(int format) const;
             int getId();
             int getpartialResultCount() { return mPartialResultBuffers.size(); }
    int getCameraId() {return mCameraId;}
    CameraMetadata* getPartialResultBuffer(unsigned int index);
    void notifyFinalmetaFilled();
    CameraMetadata* getAndWaitforFilledResults(unsigned int index);
    const CameraMetadata* getSettings() const;
    bool isAnyBufActive();
    int waitAllBufsSignaled();

    const std::vector<camera3_stream_buffer>* getInputBuffers();
    const std::vector<camera3_stream_buffer>* getOutputBuffers();
    const std::vector<CameraStreamNode*>* getOutputStreams();
    const std::vector<CameraStreamNode*>* getInputStreams();
    std::shared_ptr<CameraBuffer> findBuffer(CameraStreamNode *stream, bool warn = true);
    bool  isInputBuffer(std::shared_ptr<CameraBuffer> buffer);

    void setSequenceId(int sequenceId) {mSequenceId = sequenceId; }
    int sequenceId() const {return mSequenceId; }
    void dumpSetting();
    void dumpResults();

    // Something wrong when process this request.
    void setError() {mError = true; }
    bool getError() const {return mError; }

    class Members {
    public:
        CameraMetadata mSettings;
    };

    IRequestCallback * mCallback;

private:  /* methods */
    bool isRequestValid(camera3_capture_request * request3);
    status_t checkInputStreams(camera3_capture_request * request3);
    status_t checkOutputStreams(camera3_capture_request * request3);
    status_t initPartialResultBuffers(int cameraId);
    status_t allocatePartialResultBuffers(int partialResultCount);
    void freePartialResultBuffers(void);
    void reAllocateResultBuffer(camera_metadata_t* m, int index);

private:  /* types and members */
    // following macro adds the parts that SharedObject template requires, in practice:
    // mMembers, mLock and friendship with the SharedObject template
    Members mMembers;
    mutable std::mutex mLock; /* protects mMembers and SharedObjects */
    bool mMetadtaFilled;
    std::mutex mResultLock; /* for PartialResultBuffers*/
    std::condition_variable mCondition;
    friend class SharedObject<Camera3Request, Camera3Request::Members>;
    friend class SharedObject<const Camera3Request, const Camera3Request::Members>;

    bool mError;

    bool  mInitialized;
    CameraMetadata mSettings; /* request settings metadata. Always contains a
                                    valid metadata buffer even if the request
                                    had nullptr */
     std::mutex mAccessLock;  /* protects mInBuffers, mOutBuffers and mRequestId,
                            to ensure thread safe access to private
                            camera3_capture_request and camera3_stream_buffer
                            members*/
    unsigned int mRequestId;    /* the frame_count from the original request struct */
    int mCameraId;
    int mSequenceId;
    camera3_capture_request mRequest3;
    std::vector<camera3_stream_buffer> mOutBuffers;
    std::vector<camera3_stream_buffer> mInBuffers;
    std::vector<CameraStreamNode*> mOutStreams;
    std::vector<CameraStreamNode*> mInStreams;
    std::shared_ptr<CameraBuffer>    mOutputBufferPool[MAX_NUMBER_OUTPUT_STREAMS];
    std::vector<std::shared_ptr<CameraBuffer> > mOutputBuffers;
    std::shared_ptr<CameraBuffer>        mInputBuffer;
    std::map<int, int>      mBuffersPerFormat;
    /* Partial result support */
    bool mResultBufferAllocated;
   /**
    * \struct MemoryManagedMetadata
    * This struct is used to store the metadata buffers that are created from
    * memory managed by the HAL.
    * This is needed to avoid continuous allocation/de-allocation of metadata
    * buffers. The underlying memory for this metadata buffes is allocated
    * once but the metadata object can be cleared many times.
    * The need for this struct comes from the fact that there is no API to
    * clear the contents of a metadata buffer completely.
    */
    typedef struct {
         CameraMetadata *metaBuf;
         void   *baseBuf; /* ownership mPartialResultBuffers */
         size_t size;
         int dataCap;
         int entryCap;
    } MemoryManagedMetadata;
    std::vector<MemoryManagedMetadata> mPartialResultBuffers;
};

} NAMESPACE_DECLARATION_END
#endif /* _CAMERA3_HAL_CAMERA3REQUEST_H_ */
