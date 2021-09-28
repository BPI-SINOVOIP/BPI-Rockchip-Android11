
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

#ifndef HAL_ROCKCHIP_PSL_RKISP1_WORKERS_POSTPROCESSPIPELINE_H_
#define HAL_ROCKCHIP_PSL_RKISP1_WORKERS_POSTPROCESSPIPELINE_H_

#include <memory>
#include <Utils.h>
#include <vector>
#include <mutex>
#include <array>
#include <dlfcn.h>
#include <condition_variable>
#include "common/MessageThread.h"
#include "common/SharedItemPool.h"
#include "CameraBuffer.h"
#include "ProcUnitSettings.h"
#include "tasks/JpegEncodeTask.h"
#include "LogHelper.h"
#include "uvc_hal_types.h"

namespace android {
namespace camera2 {

class PostProcessPipeLine;
/* priority form high to low, and from common process to
 * stream only
 */
#define MAX_COMMON_PROC_UNIT_SHIFT 16
#define MAX_STREAM_PROC_UNIT_SHIFT 32
enum PostProcessType {
    /* common */
    kPostProcessTypeComposingFileds   = 1 << 0,
    kPostProcessTypeFaceDetection     = 1 << 1,
    kPostProcessTypeSwLsc             = 1 << 2,
    kPostProcessTypeCropRotationScale = 1 << 3,
    kPostprocessTypeUvnr              = 1 << 4,
    kPostProcessTypeDigitalZoom       = 1 << 5,
    kPostProcessTypeCommonMax         = 1 << MAX_COMMON_PROC_UNIT_SHIFT,
    /* stream only */
    kPostProcessTypeScaleAndRotation  = 1 << 17,
    kPostProcessTypeJpegEncoder       = 1 << 18,
    kPostProcessTypeCopy              = 1 << 19,
    kPostProcessTypeUVC               = 1 << 20,
    kPostProcessTypeRaw               = 1 << 21,
    kPostProcessTypeDummy             = 1 << 22,
    kPostProcessTypeStreamMax         = 1 << MAX_STREAM_PROC_UNIT_SHIFT,
};

#define NO_NEED_INTERNAL_BUFFER_PROCESS_TYPES \
    (kPostProcessTypeFaceDetection | kPostProcessTypeCopy)
/*
 * encapsulate the CameraBuffer so we can use SharedItemPool
 * to manage the CameraBuffer
 */
struct PostProcBuffer {
 public:
    PostProcBuffer() : index(-1), cambuf(nullptr) {}
    ~PostProcBuffer() {}
    static void reset(PostProcBuffer *me) {
        me->cambuf = nullptr;
        me->request = nullptr;
    }
    int index;
    FrameInfo fmt;
    std::shared_ptr<CameraBuffer> cambuf;
    Camera3Request* request;
};

class PostProcBufferPools {
 public:
    PostProcBufferPools() : mPostProcItemsPool("PostProcBufPool"),
                            mBufferPoolSize(0) {}
    virtual ~PostProcBufferPools() {}
    status_t createBufferPools(PostProcessPipeLine* pl, const FrameInfo& outfmt, int numBufs);
    status_t acquireItem(std::shared_ptr<PostProcBuffer> &procbuf);
    std::shared_ptr<PostProcBuffer> acquireItem();
 private:
    PostProcessPipeLine* mPipeline;
    SharedItemPool<PostProcBuffer> mPostProcItemsPool;
    unsigned int mBufferPoolSize;
};

/*
 * notify next post procss unit that the new filled
 * buffer is ready. This acts as a buffer consumer.
 */
class IPostProcessListener {
 public:
    IPostProcessListener() {}
    virtual ~IPostProcessListener() {}
    virtual status_t notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
                                    const std::shared_ptr<ProcUnitSettings>& settings,
                                    int err) = 0;
};

/*
 * This is frame provider, and it will notify all listeners
 * that new frame is ready.
 */
class IPostProcessSource {
 public:
    IPostProcessSource() {}
    virtual ~IPostProcessSource() {}
    virtual status_t attachListener(IPostProcessListener* listener);
    virtual status_t notifyListeners(const std::shared_ptr<PostProcBuffer>& buf,
                                     const std::shared_ptr<ProcUnitSettings>& settings, int err);
 protected:
    std::mutex mListenersLock;
    std::vector<IPostProcessListener*> mListeners;
};

/*
 * some process units such as field composing need one
 * more than input frames.
 */
#define STATUS_NEED_NEXT_INPUT_FRAME (-EAGAIN)
#define STATUS_FORWRAD_TO_NEXT_UNIT (1)
#define kDefaultAllocBufferNums     (4)
/*
 * Post process unit is a base class that is used to extend the
 * frame propcess pipeline. A single process unit would do tasks
 * such as digital zoom, jpeg encorder, GPU uvnr, GPU face detection,
 * etc.
 */
class PostProcessUnit : public IPostProcessListener,
                        public IPostProcessSource,
                        public IMessageHandler {
 public:
    friend class PostProcessPipeLine;
    /* describe where the processed frame data will be stored */
    enum PostProcBufType {
        kPostProcBufTypeInt,  // stored to unit internal allocated buffer
        kPostProcBufTypePre,  // stored to previous unit provided buffer
        kPostProcBufTypeExt  // stored to external set buffer
    };

    explicit PostProcessUnit(const char* name, int type, uint32_t buftype = kPostProcBufTypeInt,
                             PostProcessPipeLine* pl = nullptr);
    virtual ~PostProcessUnit();
    virtual status_t prepare(const FrameInfo& outfmt, int bufNum = kDefaultAllocBufferNums);
    virtual status_t start();
    virtual status_t stop();
    virtual status_t flush();
    virtual status_t drain();
    /*
     * The processed frame result should be filled in output buffer
     * instead of the internal allocated buffer in this process unit.
     */
    status_t addOutputBuffer(std::shared_ptr<PostProcBuffer> buf);
    /* bypass this process unit if disabled */
    status_t setEnable(bool enable);
    /* process frame in |notifyListener| instead of threadloop if sync is true */
    status_t setProcessSync(bool sync);
 protected:
    /* overload IMessageHandler */
    void messageThreadLoop(void);
    /* overload IpostProcessListener */
    virtual status_t notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
                            const std::shared_ptr<ProcUnitSettings>& settings, int err);

    /*
     * If the process need more than one input frame, the overload
     * should cache the PostProcBuffer shared_ptr, and release
     * after use.
     */
    virtual status_t processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                  const std::shared_ptr<PostProcBuffer>& out,
                                  const std::shared_ptr<ProcUnitSettings>& settings);

    virtual bool checkFmt(CameraBuffer* in, CameraBuffer* out);
    typedef std::pair<std::shared_ptr<PostProcBuffer>, \
                      std::shared_ptr<ProcUnitSettings>> ProcInfo;

    std::vector<ProcInfo> mInBufferPool;
    std::vector<std::shared_ptr<PostProcBuffer>> mOutBufferPool;
    /*
     * buffer pool owened by this process unit, and the buffers
     * in this pool can be sent to next process unit to do the
     * further process, and could be recycled automatically.
     */
    std::unique_ptr<PostProcBufferPools> mInternalBufPool;
    status_t allocCameraBuffer(const FrameInfo& outfmt, int bufNum);
    void prepareProcess();
    virtual status_t doProcess();
    status_t relayToNextProcUnit(int err);
    const char* mName;
    uint32_t mBufType;
    bool mEnable;
    bool mSyncProcess;
    bool mThreadRunning;
    std::unique_ptr<MessageThread> mProcThread;
    /* synchronize between api caller and work thread */
    std::mutex mApiLock;
    std::condition_variable mCondition;
    /* enum PostProcessType */
    int mProcessUnitType;
    PostProcessPipeLine* mPipeline;
    /*
     * below members are only used in threadloop context when
     * mSyncProcess is fales, and contrarily only in caller thread.
     */
    std::shared_ptr<PostProcBuffer> mCurPostProcBufIn;
    std::shared_ptr<ProcUnitSettings> mCurProcSettings;
    std::shared_ptr<PostProcBuffer> mCurPostProcBufOut;
 private:
    /*disable copy constructor and assignment*/
    PostProcessUnit(const PostProcessUnit&);
    PostProcessUnit& operator=(const PostProcessUnit&);
};

class PostProcessUnitUVC : public PostProcessUnit
{
 public:
    explicit PostProcessUnitUVC(const char* name, int type, uint32_t buftype = kPostProcBufTypeExt, PostProcessPipeLine* pl = nullptr);
    virtual ~PostProcessUnitUVC();
    virtual status_t processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                  const std::shared_ptr<PostProcBuffer>& out,
                                  const std::shared_ptr<ProcUnitSettings>& settings);
    virtual status_t prepare(const FrameInfo& outfmt, int bufNum = kDefaultAllocBufferNums);
 private:
    /*disable copy constructor and assignment*/
    PostProcessUnitUVC(const PostProcessUnitUVC&);
    PostProcessUnitUVC& operator=(const PostProcessUnitUVC&);
    int uvcFrameW;
    int uvcFrameH;
    FrameInfo mOutFmtInfo;
    int mBufNum;
    uvc_vpu_ops_t *pUvc_vpu_ops;
    uvc_proc_ops_t *pUvc_proc_ops;
};

class PostProcessUnitJpegEnc : public PostProcessUnit
{
 public:
    explicit PostProcessUnitJpegEnc(const char* name, int type, uint32_t buftype = kPostProcBufTypeExt,
                                    PostProcessPipeLine* pl = nullptr);
    virtual ~PostProcessUnitJpegEnc();
    status_t notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
                            const std::shared_ptr<ProcUnitSettings>& settings,
                            int err);
    virtual status_t processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                  const std::shared_ptr<PostProcBuffer>& out,
                                  const std::shared_ptr<ProcUnitSettings>& settings);
    virtual status_t prepare(const FrameInfo& outfmt, int bufNum = kDefaultAllocBufferNums);
 private:
    status_t convertJpeg(std::shared_ptr<CameraBuffer> buffer,
                         std::shared_ptr<CameraBuffer> jpegBuffer,
                         Camera3Request *request);
 private:
    std::unique_ptr<JpegEncodeTask> mJpegTask;
    /*disable copy constructor and assignment*/
    PostProcessUnitJpegEnc(const PostProcessUnitJpegEnc&);
    PostProcessUnitJpegEnc& operator=(const PostProcessUnitJpegEnc&);
};

class PostProcessUnitRaw : public PostProcessUnit
{
 public:
    explicit PostProcessUnitRaw(const char* name, int type, uint32_t buftype = kPostProcBufTypeExt);
    virtual ~PostProcessUnitRaw();
    virtual status_t processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                  const std::shared_ptr<PostProcBuffer>& out,
                                  const std::shared_ptr<ProcUnitSettings>& settings);
 private:
    /*disable copy constructor and assignment*/
    PostProcessUnitRaw(const PostProcessUnitRaw&);
    PostProcessUnitRaw& operator=(const PostProcessUnitRaw&);
};

class PostProcessUnitSwLsc : public PostProcessUnit
{
 public:
    explicit PostProcessUnitSwLsc(const char* name, int type, uint32_t buftype = kPostProcBufTypeExt,
                                PostProcessPipeLine* pl = nullptr);
    virtual ~PostProcessUnitSwLsc();
    virtual status_t processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                  const std::shared_ptr<PostProcBuffer>& out,
                                  const std::shared_ptr<ProcUnitSettings>& settings);
    virtual status_t prepare(const FrameInfo& outfmt, int bufNum = kDefaultAllocBufferNums);
 private:
    typedef struct lsc_para
    {
        uint16_t sizex[8]; //width of 8 blocks
        uint16_t sizey[8]; //heigth of 8 blocks
        uint16_t gradx[8]; //pre-calculated factors of 8 blocks in horizontal direction
        uint16_t grady[8]; ////pre-calculated factors of 8 blocks in vertical direction
        uint16_t u16_coef_r[2][17][18]; // 2 table for r channel lens shade correction
        uint16_t u16_coef_gr[2][17][18];// 2 table for gr channel lens shade correction
        uint16_t u16_coef_gb[2][17][18];// 2 table for gb channel lens shade correction
        uint16_t u16_coef_b[2][17][18];// 2 table for b channel lens shade correction
        uint8_t lsc_en;
        uint8_t table_sel; // lesns shade correction coef table set selection
        uint32_t width;
        uint32_t height;
        uint32_t *u32_coef_pic_gr;
    } lsc_para_t;

    static void calcu_coef(lsc_para_t const * plsc_a,
                           uint16_t u16_coef_blk[2][17][18],
                           uint32_t * pu32_coef_pic,
                           uint32_t u32_z_max,
                           uint32_t u32_y_max,
                           uint32_t u32_x_max);
    static int lsc_config(void *para_v);
    static int lsc(uint8_t *indata, uint16_t input_h_size,
                   uint16_t input_v_size,
                   uint8_t bayer_pat, lsc_para_t *lsc_para,
                   uint8_t *outdata, uint8_t  c_dw_si);
 private:
    lsc_para_t mLscPara;
    /*disable copy constructor and assignment*/
    PostProcessUnitSwLsc(const PostProcessUnitSwLsc&);
    PostProcessUnitSwLsc& operator=(const PostProcessUnitSwLsc&);
};

class PostProcessUnitDigitalZoom : public PostProcessUnit
{
 public:
    PostProcessUnitDigitalZoom(const char* name, int type, int camid, uint32_t buftype = kPostProcBufTypeInt,
                                PostProcessPipeLine* pl = nullptr);
    virtual ~PostProcessUnitDigitalZoom();
    virtual status_t processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                  const std::shared_ptr<PostProcBuffer>& out,
                                  const std::shared_ptr<ProcUnitSettings>& settings);
 private:
    virtual bool checkFmt(CameraBuffer* in, CameraBuffer* out);
    // cache active pixel array
    CameraWindow mApa;
    /*disable copy constructor and assignment*/
    PostProcessUnitDigitalZoom(const PostProcessUnitDigitalZoom&);
    PostProcessUnitDigitalZoom& operator=(const PostProcessUnitDigitalZoom&);
};

/*
 * used to do post processes for camera3 stream.
 *
 */
class PostProcessPipeLine: public IMessageHandler {
 public:
    // thread message id's
    enum MessageId {
        MESSAGE_ID_EXIT = 0,
        MESSAGE_ID_START,
        MESSAGE_ID_STOP,
        MESSAGE_ID_PREPARE,
        MESSAGE_ID_PROCESSFRAME,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_MAX
    };

    struct MessagePrepare {
        FrameInfo in;
        std::vector<camera3_stream_t*> streams;
        bool  needpostprocess;
        int   pipelineDepth;
    };

    struct MessageProcess {
        std::shared_ptr<PostProcBuffer> in;
        std::vector<std::shared_ptr<PostProcBuffer>> out;
        std::shared_ptr<ProcUnitSettings> settings;
    };

    // union of all message data
    union MessageData {
        MessagePrepare msgPrepare;
        MessageProcess msgProcess;
    };

    struct Message {
        MessageId id;
        MessagePrepare prepareMsg;
        MessageProcess processMsg;
        Message(): id(MESSAGE_ID_EXIT) {
            CLEAR(prepareMsg);
            CLEAR(processMsg);
        }
    };

    /* Describle the unit position in pipeline */
    enum ProcessUnitLevel {
        kFirstLevel,
        kMiddleLevel,
        kLastLevel,
        kMaxLevel
    };

    /*
     * |worker| defines which ISP phisical path to be extended.
     * |listener| specifies where the processed buffer will be output.
     */
    PostProcessPipeLine(IPostProcessListener* listener, int camid);
    ~PostProcessPipeLine();
    /* construt the pipeline*/
    status_t prepare(const FrameInfo& in,
                     const std::vector<camera3_stream_t*>& streams,
                     bool& needpostprocess,
                     int   pipelineDepth = kDefaultAllocBufferNums);
    status_t prepare_internal(const FrameInfo& in,
                     const std::vector<camera3_stream_t*>& streams,
                     bool& needpostprocess,
                     int   pipelineDepth = kDefaultAllocBufferNums);
    status_t start();
    status_t stop();
    status_t clear();
    void flush();
    /*
     * |in| buffer usually comes from driver,
     * |out| buffers usually comes from camera3 streams, can be empty
     */
    status_t processFrame(const std::shared_ptr<PostProcBuffer>& in,
                          const std::vector<std::shared_ptr<PostProcBuffer>>& out,
                          const std::shared_ptr<ProcUnitSettings>& settings);

    int getCameraId() { return mCameraId; };
    camera3_stream_t* getStreamByType(int stream_type);


 private:
    virtual void messageThreadLoop(void);
    status_t requestExitAndWait();
    status_t handleMessageExit();
    status_t handleStart(Message &msg);
    status_t handleStop(Message &msg);
    status_t handlePrepare(Message &msg);
    status_t handleProcessFrame(Message &msg);
    status_t handleFlush(Message &msg);

    int getRotationDegrees(camera3_stream_t* stream) const;
    /*
     * Links the unit together.
     * |from| is the unit that would be added as a consumer to |to|.
     * If |to| is null, then |from| represents the first level unit
     * in pipeline.
     */
    status_t linkPostProcUnit(const std::shared_ptr<PostProcessUnit>& from,
                              const std::shared_ptr<PostProcessUnit>& to,
                              enum ProcessUnitLevel level);
    status_t enablePostProcUnit(PostProcessUnit* procunit, bool enable);
    status_t setPostProcUnitAsync(PostProcessUnit* procunit, bool async);
    status_t addOutputBuffer(const std::vector<std::shared_ptr<PostProcBuffer>>& out);

    bool IsRawStream(camera3_stream_t* stream);

    std::vector<std::map<camera3_stream_t*, int>> mStreamToTypeMap;
    std::vector<std::shared_ptr<PostProcessUnit>> mPostProcUnits;
    std::map<camera3_stream_t*, PostProcessUnit*> mStreamToProcUnitMap;
    typedef std::vector<PostProcessUnit*> ProcUnitList;
    std::array<ProcUnitList, kMaxLevel> mPostProcUnitArray;
    IPostProcessListener* mPostProcFrameListener;
    int mCameraId;
    bool mThreadRunning;
    MessageQueue<Message, MessageId> mMessageQueue;
    std::unique_ptr<MessageThread> mMessageThread;

    std::condition_variable mCondition;
    /*
     * when more than one camera3_stream is linked to one pipeline, the
     * OutputBuffer of same request from different stream may need to be
     * returned concurrently. The size of |streams| of prepare call determins
     * this variable's value, and whether the sync is needed actually is
     * decided by |processFrame| call. If |out| size bigger than 1, and out
     * buffers come from different stream, and |in| is the same as one of |out|,
     * then the sync is really needed
     */
    bool mMayNeedSyncStreamsOutput;
    friend class OutputBuffersHandler;
    class OutputBuffersHandler : public IPostProcessListener {
     public:
        explicit OutputBuffersHandler(PostProcessPipeLine* pipeline);
        virtual ~OutputBuffersHandler() {}
        status_t notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
                                const std::shared_ptr<ProcUnitSettings>& settings,
                                int err);
        void addSyncBuffersIfNeed(const std::shared_ptr<PostProcBuffer>& in,
                                  const std::vector<std::shared_ptr<PostProcBuffer>>& out);
     private:
        PostProcessPipeLine* mPipeline;
        struct syncItem {
            std::vector<std::shared_ptr<PostProcBuffer>> sync_buffers;
            int sync_nums;
        };
        std::map<CameraBuffer*, std::shared_ptr<syncItem>> mCamBufToSyncItemMap;
        std::mutex mLock;
    };
    explicit PostProcessPipeLine(const PostProcessPipeLine&);
    PostProcessPipeLine& operator=(const PostProcessPipeLine&);
    std::unique_ptr<OutputBuffersHandler> mOutputBuffersHandler;
    camera3_stream_t mUvc;
};
const element_value_t PPMsg_stringEnum[] = {
    {"MESSAGE_ID_EXIT", PostProcessPipeLine::MESSAGE_ID_EXIT },
    {"MESSAGE_ID_START", PostProcessPipeLine::MESSAGE_ID_START },
    {"MESSAGE_ID_STOP", PostProcessPipeLine::MESSAGE_ID_STOP },
    {"MESSAGE_ID_PREPARE", PostProcessPipeLine::MESSAGE_ID_PREPARE },
    {"MESSAGE_ID_PROCESSFRAME", PostProcessPipeLine::MESSAGE_ID_PROCESSFRAME },
    {"MESSAGE_ID_FLUSH", PostProcessPipeLine::MESSAGE_ID_FLUSH },
    {"MESSAGE_ID_MAX", PostProcessPipeLine::MESSAGE_ID_MAX },
};

} /* namespace camera2 */
} /* namespace android */

#endif  // HAL_ROCKCHIP_PSL_RKISP1_WORKERS_POSTPROCESSPIPELINE_H_
