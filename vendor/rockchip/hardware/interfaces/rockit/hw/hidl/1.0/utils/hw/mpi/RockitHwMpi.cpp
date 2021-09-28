/*
 * Copyright (C) 2019 Rockchip Electronics Co. LTD
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

 //#define LOG_NDEBUG 0
#define LOG_TAG "RockitHwMpi"

#include <dlfcn.h>
#include "RTUtils.h"
#include <utils/Log.h>
#include "RockitHwMpi.h"
#include "../include/mpp/rk_mpi.h"
#include "rt_drm.h"
#include "drm.h"
#include "drm_mode.h"
#include <sys/mman.h>
#include <utils/Vector.h>

#include <rockchip/hardware/rockit/hw/1.0/types.h>

using namespace ::android::hardware;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWCtrCmd;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWParamPair;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWParamPairs;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWDataType;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWParamKey;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWBufferFlags;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWQueryCmd;


namespace rockchip {
namespace hardware {
namespace rockit {
namespace hw {
namespace V1_0 {
namespace utils {

enum _MppBufferSite {
    BUFFER_SITE_BY_MPI = 0,
    BUFFER_SITE_BY_ROCKIT = 1,
} MppBufferSite;

#define DATABUFFERMAX 5
#define COMMITBUFFERMAX 50

typedef struct _MppBufferCtx {
    /* this fd is mpp can use, and only using in this process*/
    int       mFd;
    /* this fd is can use all process */
    int       mUniqueID;
    /* mpp buffer, the fd belong to which mpp buffer */
    MppBuffer mMppBuffer;
    /* who own this buffer */
    int       mSite;
} MppBufferCtx;

class DataBufferCtx {
public:
    /* this fd is mpp can use, and only using in this process*/
    int       mFd;
    /* this fd is can use all process */
    int       mUniqueID;
    void     *mData;
    int       mSize;
    /* who own this buffer */
    int       mSite;

public:
    DataBufferCtx();
    ~DataBufferCtx();
};

typedef struct _MpiCodecContext {
    MppCtx              mpp_ctx;
    MppApi             *mpp_mpi;
    MppBufferGroup      frm_grp;
    /*
     * commit buffer list.
     * This buffers are alloced by rockits,
     * commit to decoder(encoder) for keep frames(stream).
     */
    Vector<MppBufferCtx*>* mCommitList;

    /*
     * commit buffer list.
     * This buffers are alloced by rockits,
     * commit to decoder(encoder) for keep frames(stream).
     */
    Vector<DataBufferCtx*>* mDataList;
} MpiCodecContext;

DataBufferCtx::DataBufferCtx() {
    mFd = -1;
    mUniqueID = -1;
    mData = NULL;
    mSize = 0;
    mSite = BUFFER_SITE_BY_MPI;
}

DataBufferCtx::~DataBufferCtx() {
    if (mData != NULL) {
        drm_munmap(mData, mSize);
        mData = NULL;
    }
    mSize = 0;

    if (mFd >= 0) {
        close(mFd);
        mFd = -1;
    }
}


/*
 * Des: using mUniqueID to find the mpp buffer
 *       this function is used to give back buffer to mpi,
 * Param: fd : the unique ID.
 * Return: the index of mpp buffer in MppBufferList
 */
void* RockitHwMpi::findMppBuffer(int fd) {
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    MppBufferCtx* buffer = NULL;
    if (ctx != NULL && ctx->mCommitList != NULL) {
        for (int i = 0; i < ctx->mCommitList->size(); i++) {
            buffer = ctx->mCommitList->editItemAt(i);
            if (buffer && buffer->mUniqueID == fd) {
                return (void*)buffer;
            }
        }
    }

    return NULL;
}

/*
 * only clean buffer which owner by mpi/mpp
 */
void RockitHwMpi::cleanMppBuffer() {
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    if (ctx != NULL) {
        while (ctx->mCommitList != NULL && !ctx->mCommitList->isEmpty()) {
            MppBufferCtx* buffer = ctx->mCommitList->editItemAt(0);
            if (buffer != NULL)
                delete buffer;

            ctx->mCommitList->removeAt(0);
        }
    }
}

void RockitHwMpi::cleanMppBuffer(int site) {
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    dumpMppBufferList();
    if (ctx != NULL && ctx->mCommitList != NULL) {
        for (int i = ctx->mCommitList->size()-1; i >= 0; i--) {
            MppBufferCtx* buffer = ctx->mCommitList->editItemAt(i);
            if (buffer != NULL && buffer->mSite == site) {
                delete buffer;
                ctx->mCommitList->removeAt(i);
            }
        }

        if (mDebug) {
            for (int i = 0; i < ctx->mCommitList->size(); i++) {
                MppBufferCtx* buffer = ctx->mCommitList->editItemAt(i);
                if (buffer != NULL ) {
                     ALOGD("%s: left i = %d, mUniqueID = %d, fd = %d",
                        __FUNCTION__, i, buffer->mUniqueID, buffer->mFd);
                }
            }
        }
    }
}


void* RockitHwMpi::findDataBuffer(int fd) {
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    if (ctx != NULL) {
        DataBufferCtx* buffer = NULL;
        for (int i = 0; i < ctx->mDataList->size(); i++) {
            buffer = ctx->mDataList->editItemAt(0);
            if (buffer && buffer->mUniqueID == fd) {
                return (void*)buffer;
            }
        }
    }

    return NULL;
}

int RockitHwMpi::addDataBufferList(int uniquefd, int mapfd,void* data, int size) {
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    if (ctx != NULL && ctx->mDataList != NULL) {
        if (ctx->mDataList->size() >= DATABUFFERMAX) {
            ALOGD("%s: mDataList is full, size = %d", __FUNCTION__, (int)ctx->mDataList->size());
        }

        ALOGD("%s: mUniqueID = %d, mapfd = %d, data = %p", __FUNCTION__, uniquefd, mapfd, data);
        DataBufferCtx* buffer = new DataBufferCtx();
        assert(buffer != NULL);
        buffer->mFd = mapfd;
        buffer->mUniqueID = uniquefd;
        buffer->mData = data;
        buffer->mSize = size;
        buffer->mSite = BUFFER_SITE_BY_MPI;
        ctx->mDataList->push(buffer);
        return 0;
    }

    return -1;
}

void RockitHwMpi::freeDataBuffer(int bufferId) {
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    ALOGD("%s: size = %d, bufferId = %d", __FUNCTION__, (int)ctx->mDataList->size(), bufferId);
    if (ctx != NULL && ctx->mDataList != NULL) {
        for (int i = 0; i < ctx->mDataList->size(); i++){
            DataBufferCtx* buffer = ctx->mDataList->editItemAt(i);
            if (buffer && buffer->mUniqueID == bufferId) {
                ctx->mDataList->removeAt(i);
                delete buffer;
                return;
            }
        }
    }
}

void RockitHwMpi::freeDataBufferList() {
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    if (ctx != NULL) {
        while (ctx->mDataList != NULL && !ctx->mDataList->isEmpty()) {
            DataBufferCtx* buffer = ctx->mDataList->editItemAt(0);
            if (buffer) {
                delete buffer;
            }
            ctx->mDataList->removeAt(0);
        }
    }
}

void RockitHwMpi::dumpMppBufferList() {
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    if (ctx != NULL && ctx->mCommitList != NULL) {
        for (int i = 0; i < ctx->mCommitList->size(); i++) {
            MppBufferCtx* buffer = ctx->mCommitList->editItemAt(i);
            if (buffer != NULL)
                ALOGD("%s this = %p, i = %d, map fd = %d, mUniqueID = %d, mMppBuffer = %p, mSite = %d",
                    __FUNCTION__, this, i, buffer->mFd, buffer->mUniqueID,
                    buffer->mMppBuffer, buffer->mSite);
        }
    }
}


RockitHwMpi::RockitHwMpi() {
    ALOGD("%s %p", __FUNCTION__, this);
    mCtx = NULL;
    mInput = NULL;
    mOutput = NULL;
    mDebug = false;
    mWStride = 0;
    mHStride = 0;
    mDrmFd = drm_open();
    if (mDrmFd < 0) {
        ALOGD("%s %p drm_open fail", __FUNCTION__, this);
    }
}

RockitHwMpi::~RockitHwMpi() {
    ALOGD("%s %p", __FUNCTION__, this);
    if (mCtx != NULL) {
        reset();
        freeDataBufferList();
        cleanMppBuffer();
        MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
        if (ctx->frm_grp != NULL) {
            mpp_buffer_group_put(ctx->frm_grp);
            ctx->frm_grp = NULL;
        }

        if (ctx->mpp_ctx != NULL) {
            mpp_destroy(ctx->mpp_ctx);
            ctx->mpp_ctx = NULL;
        }

        if (ctx->mDataList != NULL) {
            delete ctx->mDataList;
            ctx->mDataList = NULL;
        }
        if (ctx->mCommitList != NULL) {
            delete ctx->mCommitList;
            ctx->mCommitList = NULL;
        }

        free(ctx);
        mCtx = NULL;
    }

    if (mDrmFd >= 0) {
        drm_close(mDrmFd);
        mDrmFd = -1;
    }
}

int RockitHwMpi::init(const RockitHWParamPairs& pairs) {
    MPP_RET             err     = MPP_OK;
    MppCtx              mpp_ctx = NULL;
    MppApi             *mpp_mpi = NULL;
    MppBufferGroup      frm_grp = NULL;
    MppCodingType       mpp_coding_type;
    uint32_t            param   = 1;

    uint32_t            type = 0;
    uint32_t            width = 0;
    uint32_t            height = 0;
    uint32_t            format = 0;
    uint32_t            fastMode = 0;
    uint32_t            fbcOutput = 0;
    uint32_t            timeMode = 0;
    uint32_t            debug = 0;

    Mutex::Autolock autoLock(mLock);
    MpiCodecContext* ctx = (MpiCodecContext*)malloc(sizeof(MpiCodecContext));
    if (ctx == NULL) {
        ALOGD("%s:%d malloc fail", __FUNCTION__, __LINE__);
        goto FAIL;
    }

    memset(ctx, 0, sizeof(*ctx));

    err = mpp_create(&mpp_ctx, &mpp_mpi);
    if (err != MPP_OK) {
        ALOGD("%s:%d create fail", __FUNCTION__, __LINE__);
        goto FAIL;
    }

    // get parameters from metadata
    type   = (uint32_t)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_CODECID);
    width  = (uint32_t)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_WIDTH);
    height = (uint32_t)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_HEIGHT);
    format = (uint32_t)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_FORMAT);
    fastMode = (uint32_t)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_FASTMODE);
    fbcOutput = (uint32_t)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_FBC_OUTPUT);
    timeMode = (uint32_t)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_PRESENT_TIME_ORDER);
    debug  = (uint32_t)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_DEBUG);

    if (debug > 0) {
        mDebug = true;
        ALOGD("%s: type = %d, width = %d, height = %d, format = %d,fastMode = %d, timeMode = %d",
            __FUNCTION__, type, width, height, format, fastMode, timeMode);
    }

    if ((type <= 0) || (width <= 0) || (height <= 0)) {
        ALOGE("%s: type = 0x%x, width = %d, height = %d, format = 0x%x not support",
            __FUNCTION__, type, width, height, format);
        goto FAIL;
    }

    mpp_coding_type = (MppCodingType)type;
    if (mpp_coding_type == MPP_VIDEO_CodingUnused) {
        ALOGE("%s unsupport rockit codec id: 0x%x", __FUNCTION__, type);
        goto FAIL;
    }

    if (width <= 0 || height <= 0) {
        goto FAIL;
    }

    if (fastMode > 0) {
        mpp_mpi->control(mpp_ctx, MPP_DEC_SET_PARSER_FAST_MODE, &param);
    }

    err = mpp_init(mpp_ctx, MPP_CTX_DEC, mpp_coding_type);
    if (err != MPP_OK) {
        ALOGE("%s unsupport rockit codec id: 0x%x", __FUNCTION__, type);
        goto FAIL;
    }

    {
        MppFrame frame = NULL;

        uint32_t mppFormat = format;

        if (fbcOutput) {
            ALOGD("use mpp fbc output mode");
            uint32_t MPP_FRAME_FBC_AFBC_V2 = 0x00200000;
            mppFormat |= MPP_FRAME_FBC_AFBC_V2;
            mpp_mpi->control(mpp_ctx, MPP_DEC_SET_OUTPUT_FORMAT, (MppParam)&mppFormat);
        }

        mpp_frame_init(&frame);
        mpp_frame_set_width(frame, width);
        mpp_frame_set_height(frame, height);
        mpp_frame_set_fmt(frame, (MppFrameFormat)mppFormat);
        mpp_mpi->control(mpp_ctx, MPP_DEC_SET_FRAME_INFO, (MppParam)frame);

        /*
         * in old mpp version, MPP_DEC_SET_FRAME_INFO can't provide
         * stride information, so it can only use unaligned width
         * and height. Infochange is used to solve this problem.
         */
        if (mpp_frame_get_hor_stride(frame) <= 0
                || mpp_frame_get_ver_stride(frame) <= 0) {
            mpp_frame_set_hor_stride(frame, width);
            mpp_frame_set_ver_stride(frame, height);
            mpp_mpi->control(mpp_ctx, MPP_DEC_SET_FRAME_INFO, (MppParam)frame);
        }

        mWStride = mpp_frame_get_hor_stride(frame);
        mHStride = mpp_frame_get_ver_stride(frame);

        mpp_frame_deinit(&frame);
    }

    if (timeMode > 0) {
        mpp_mpi->control(mpp_ctx, MPP_DEC_SET_PRESENT_TIME_ORDER, &param);
    }

    // init frame grp
    err = mpp_buffer_group_get_external(&frm_grp, MPP_BUFFER_TYPE_ION);
    if (err != MPP_OK) {
        ALOGE("%s unsupport rockit codec id: 0x%x", __FUNCTION__, type);
        goto FAIL;
    }

    // TODO(control cmds)
    mpp_mpi->control(mpp_ctx, MPP_DEC_SET_EXT_BUF_GROUP, frm_grp);

    ctx->mpp_ctx = mpp_ctx;
    ctx->mpp_mpi = mpp_mpi;
    ctx->frm_grp = frm_grp;

    mpp_buffer_group_clear(ctx->frm_grp);

    ctx->mCommitList = new Vector<MppBufferCtx*>;
    ctx->mDataList = new Vector<DataBufferCtx*>;
    assert(ctx->mCommitList != NULL);
    assert(ctx->mDataList != NULL);

    mCtx = (void*)ctx;
    return 0;

FAIL:
    if (mpp_ctx) {
        mpp_destroy(mpp_ctx);
        mpp_ctx =NULL;
    }
    if (ctx) {
        free(ctx);
        ctx = NULL;
    }

    mCtx = NULL;
    return -1;
}

int RockitHwMpi::enqueue(const RockitHWBuffer& buffer) {
    Mutex::Autolock autoLock(mLock);
    if (mCtx == NULL || mDrmFd < 0) {
        return -1;
    }

    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    int      ret         = 0;

    // mpp data structures
    MPP_RET     err         = MPP_OK;
    MppPacket   pkt         = NULL;
    MppCtx      mpp_ctx     = ctx->mpp_ctx;
    MppApi     *mpp_mpi     = ctx->mpp_mpi;
    uint32_t    eos         = 0;
    uint32_t    extradata   = 0;
    uint32_t    flags       = 0;
    uint64_t    pts         = 0ll;
    uint64_t    dts         = 0ll;
    uint32_t    length      = buffer.length;
    int         fd          = -1;
    void*       data        = NULL;
    const RockitHWParamPairs& pairs = buffer.pair;

    uint32_t handle = 0;
    int size = 0;

    flags  = (uint32_t)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_FLAGS);
    pts = getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_PTS);
    dts = getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_DTS);

    eos = (flags & (uint32_t)RockitHWBufferFlags::HW_FLAGS_EOS);
    extradata = (flags & (uint32_t)RockitHWBufferFlags::HW_FLAGS_EXTRA_DATAS);
  //  newbuffer = (flags & (uint32_t)RockitHWBufferFlags::HW_FLAGS_NEW_HW_BUFFER);

    if (!eos) {
        DataBufferCtx* bufferCtx = (DataBufferCtx*)findDataBuffer((int)buffer.bufferId);
        if (bufferCtx == NULL) {
            ret = drm_get_info_from_name(mDrmFd, (int)buffer.bufferId, &handle, &size);
            if (ret < 0) {
                ALOGE("%s: drm_get_info_from_name fail", __FUNCTION__);
                ret = -1;
                goto EXIT;
            }
            ret = drm_map(mDrmFd, handle, (uint32_t)size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, 0, &data, &fd, 0);
            if (ret < 0) {
                ALOGE("%s: drm_map fail", __FUNCTION__);
                ret = -1;
                goto EXIT;
            }
            ret = addDataBufferList(buffer.bufferId, fd, data, size);
            if (ret < 0) {
                ret = -1;
                goto EXIT;
            }
        } else {
            data = bufferCtx->mData;
        }
    } else {
        data = NULL;
        length = 0;
    }

    mpp_packet_init(&pkt, data, length);
    if (mDebug) {
        ALOGD("%s: fd = %d, map fd = %d, length = %d,data = %p, pts = %lld, dts = %lld, eos = %d, extradata = %d",
            __FUNCTION__, (int)buffer.bufferId, fd, length, data, (long long)pts, (long long)dts, eos, extradata);
    }

    if (eos > 0) {
        mpp_packet_set_eos(pkt);
    }

    mpp_packet_set_pts(pkt, pts);
    mpp_packet_set_dts(pkt, dts);

    if (extradata > 0) {
        mpp_packet_set_extra_data(pkt);
    }

    mpp_packet_set_pos(pkt, data);
    mpp_packet_set_length(pkt, length);

    err = mpp_mpi->decode_put_packet(mpp_ctx, pkt);
    if (MPP_OK != err) {
        ret = -1;
    }
    // fd and data alread record in mDataList;
    data = NULL;
    fd = -1;
EXIT:
    if (pkt != NULL) {
        mpp_packet_deinit(&pkt);
    }

    if (data != NULL) {
        drm_munmap(data, size);
        data = NULL;
    }

    if(fd >= 0) {
        close(fd);
        fd = -1;
    }

    return ret;
}

int RockitHwMpi::dequeue(RockitHWBuffer& hwBuffer) {
    Mutex::Autolock autoLock(mLock);
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    if ((ctx == NULL) || (mDrmFd < 0)) {
        ALOGE("%s ctx = NULL", __FUNCTION__);
        return -1;
    }

    MppFrame                mpp_frame = NULL;
    MPP_RET                 err = MPP_OK;
    MppCtx                  mpp_ctx = NULL;
    MppApi                 *mpp_mpi = NULL;
    MppBufferGroup          frm_grp;
    MppBuffer               buffer = NULL;
    RockitHWParamPairs&     param = hwBuffer.pair;
    bool isI4O2 = false;
    int  ret = 0;
    int  eos = 0;
    int  infochange = 0;
    uint64_t flags = 0;
    int  fd = -1;
    hwBuffer.bufferId = -1;
    param.pairs.resize(10);
    param.counter = 0;

    mpp_ctx = ctx->mpp_ctx;
    mpp_mpi = ctx->mpp_mpi;
    frm_grp = ctx->frm_grp;

    err = mpp_mpi->decode_get_frame(mpp_ctx, &mpp_frame);
    if (MPP_OK != err) {
        ALOGD("mpp decode get frame failed! ret: %d", err);
        ret = -1;
        goto __FAILED;
    }

    if (mpp_frame) {
        MppBufferInfo info;
        if (mpp_frame_get_info_change(mpp_frame)) {
            RK_U32 width = mpp_frame_get_width(mpp_frame);
            RK_U32 height = mpp_frame_get_height(mpp_frame);
            RK_U32 hor_stride = mpp_frame_get_hor_stride(mpp_frame);
            RK_U32 ver_stride = mpp_frame_get_ver_stride(mpp_frame);

            ALOGD("decode_get_frame get info changed found\n");
            ALOGD("decoder require buffer w:h [%d:%d] stride [%d:%d]",
                        width, height, hor_stride, ver_stride);
        } else {
            buffer = mpp_frame_get_buffer(mpp_frame);
        }

        // get media buffer from mpp buffer
        infochange = mpp_frame_get_info_change(mpp_frame);
        eos = mpp_frame_get_eos(mpp_frame);
        if (buffer) {
            mpp_buffer_info_get(buffer, &info);
            hwBuffer.bufferId = info.index;
            {
                MppBufferCtx* bufferCtx = (MppBufferCtx*)findMppBuffer(hwBuffer.bufferId);
                if (bufferCtx != NULL) {
                    bufferCtx->mSite = BUFFER_SITE_BY_ROCKIT;
                    fd = bufferCtx->mFd;
                }
            }
        } else if (infochange || eos) {
            // info change or eos will reciver a empty buffer
            hwBuffer.length = 0;
            hwBuffer.bufferId = -1;
        } else {
            ALOGE("%s null mpp buffer...", __FUNCTION__);
            ret = -1;
            goto __FAILED;
        }

        isI4O2 = (mpp_frame_get_mode(mpp_frame) & MPP_FRAME_FLAG_IEP_DEI_MASK) == MPP_FRAME_FLAG_IEP_DEI_I4O2;
        setValue(param, (uint32_t)RockitHWParamKey::HW_KEY_WIDTH_STRIDE,
            (uint64_t)mpp_frame_get_hor_stride(mpp_frame));
        setValue(param, (uint32_t)RockitHWParamKey::HW_KEY_HEIGHT_STRIDE,
            (uint64_t)mpp_frame_get_ver_stride(mpp_frame));
        setValue(param, (uint32_t)RockitHWParamKey::HW_KEY_WIDTH,
            (uint64_t)mpp_frame_get_width(mpp_frame));
        setValue(param, (uint32_t)RockitHWParamKey::HW_KEY_HEIGHT,
            (uint64_t)mpp_frame_get_height(mpp_frame));
        setValue(param, (uint32_t)RockitHWParamKey::HW_KEY_PTS,
            (uint64_t)mpp_frame_get_pts(mpp_frame));
        setValue(param, (uint32_t)RockitHWParamKey::HW_KEY_DTS,
            (uint64_t)mpp_frame_get_dts(mpp_frame));

        if (mpp_frame_get_errinfo(mpp_frame) || mpp_frame_get_discard(mpp_frame)) {
            flags |= (uint32_t)RockitHWBufferFlags::HW_FLAGS_ERROR_INFOR;
        }
        if (isI4O2) {
            flags |= (uint32_t)RockitHWBufferFlags::HW_FLAGS_I4O2;
        }

        if (infochange) {
            flags |= (uint32_t)RockitHWBufferFlags::HW_FLAGS_INFOR_CHANGE;
        } else if (eos) {
            flags |= (uint32_t)RockitHWBufferFlags::HW_FLAGS_EOS;
        }

        setValue(param, (uint32_t)RockitHWParamKey::HW_KEY_FLAGS, flags);
        if (infochange || eos) {
        //    hwBuffer.length = 0;
        //    hwBuffer.bufferId = -1;
        } else {
            hwBuffer.length = mpp_frame_get_hor_stride(mpp_frame) * mpp_frame_get_ver_stride(mpp_frame) * 3 / 2;
        }

        if (mDebug) {
           ALOGD("%s: this = %p, mUniqueID = %d, fd = %d, mppBuffer = %p, mpp_frame = %p,frame width: %d frame height: %d width: %d height %d "
                                  "pts %lld dts %lld Err %d EOS %d Infochange %d isI4O2: %d flags: %lld",
                                  __FUNCTION__, this, hwBuffer.bufferId, 
                                  fd, buffer, mpp_frame,
                                  mpp_frame_get_hor_stride(mpp_frame),
                                  mpp_frame_get_ver_stride(mpp_frame),
                                  mpp_frame_get_width(mpp_frame),
                                  mpp_frame_get_height(mpp_frame),
                                  mpp_frame_get_pts(mpp_frame),
                                  mpp_frame_get_dts(mpp_frame),
                                  mpp_frame_get_errinfo(mpp_frame),
                                  eos, infochange, isI4O2,
                                  (long long)flags);
        }
    } else {
        ret = -1;
    }

    if (buffer)
        mpp_buffer_inc_ref(buffer);

__FAILED:
    if (mpp_frame)
        mpp_frame_deinit(&mpp_frame);

    return ret;
}

int RockitHwMpi::commitBuffer(const RockitHWBuffer& buffer) {
    Mutex::Autolock autoLock(mLock);
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    if ((ctx == NULL) || (mDrmFd < 0)) {
        ALOGE("%s ctx = NULL", __FUNCTION__);
        return -1;
    }

    MppBufferInfo info;
    MppBuffer mppBuffer = NULL;
    uint32_t handle = -1;
    int map_fd = -1;
    int size = 0;
    int ret = 0;
    MppBufferCtx* bufferCtx = NULL;

    memset(&info, 0, sizeof(MppBufferInfo));
    /* convert buffer's name to handle */
    int err = drm_get_info_from_name(mDrmFd, buffer.bufferId, &handle, &size);
    if (err) {
        ALOGE("%s: failed to drm_get_info_from_name(...)! err: 0x%x",
            __FUNCTION__, err);
        ret = -1;
        goto __FAILED;
    }
    /* convert buffer's handle to fd */
    err = drm_handle_to_fd(mDrmFd, handle, &map_fd, 0);
    if (err) {
        ALOGE("%s: failed to drm_handle_to_fd(...)! err: 0x%x",
            __FUNCTION__, err);
        ret = -1;
        goto __FAILED;
    }

    info.type = MPP_BUFFER_TYPE_ION;
    info.fd = map_fd;
    info.ptr = NULL;
    info.hnd = NULL;
    info.size = size;
    info.index = buffer.bufferId;

    mpp_buffer_import_with_tag(ctx->frm_grp, &info, &mppBuffer, "Rockit-Mpp-Group", __FUNCTION__);
    if (mDebug) {
        ALOGE("%s: this = %p, fd = %d, map_fd = %d, mUniqueID = %d, size = %d, mppBuffer = %p",
            __FUNCTION__, this, info.fd, map_fd, buffer.bufferId, (int)info.size, mppBuffer);
    }

    bufferCtx = (MppBufferCtx*)findMppBuffer(buffer.bufferId);
    if (bufferCtx == NULL) {
        bufferCtx = new MppBufferCtx();
        bufferCtx->mFd = info.fd;
        bufferCtx->mUniqueID = buffer.bufferId;
        bufferCtx->mMppBuffer = mppBuffer;
        bufferCtx->mSite = BUFFER_SITE_BY_MPI;
        ctx->mCommitList->push(bufferCtx);

        if (ctx->mCommitList->size() >= COMMITBUFFERMAX) {
            ALOGE("%s:%d too many buffer in list", __FUNCTION__, __LINE__);
            dumpMppBufferList();
        }
    } else {
        ALOGE("%s:%d add buffer fail, already in it", __FUNCTION__, __LINE__);
        if (mDebug) {
            dumpMppBufferList();
        }
    }

__FAILED:
    /*
     * Must close the fd and handle here.
     * mpp will dup info.fd itself(info.fd->info.hnd->info.fd),
     * so the info.fd which pass to it is not be used, it will
     * leak if not closed here.
     */
    if (map_fd >= 0) {
        close(map_fd);
    }

    if (handle >= 0) {
        int err = drm_free(mDrmFd, handle);
        if (err) {
            ALOGE("failed to drm_free(dev=%d, fd=%03d)", mDrmFd, handle);
        }
    }
    return ret;
}

int RockitHwMpi::giveBackBuffer(const RockitHWBuffer& buffer) {
    Mutex::Autolock autoLock(mLock);
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    if (ctx == NULL) {
        return -1;
    }

    int fd = buffer.bufferId;
    MppBufferCtx* bufferCtx = (MppBufferCtx*)findMppBuffer(fd);
    if (bufferCtx != NULL) {
        MppBuffer mppBuffer = bufferCtx->mMppBuffer;
        bufferCtx->mSite = BUFFER_SITE_BY_MPI;
        if (mDebug) {
            ALOGE("%s this = %p, mUniqueID = %d, mFd = %d, mppBuffer = %p",
                __FUNCTION__, this, fd, bufferCtx->mFd, mppBuffer);
        }
        if (mppBuffer != NULL) {
            mpp_buffer_put(mppBuffer);
        } else {
             ALOGE("%s mppBuffer = NULL, fd = %d", __FUNCTION__, fd);
        }
    } else {
        ALOGE("%s: no find buffer in list, fd = %d", __FUNCTION__, fd);
        if (mDebug) {
            dumpMppBufferList();
        }
    }

    return 0;
}

int RockitHwMpi::process(const RockitHWBufferList& list) {
    (void)list;
    return 0;
}

int RockitHwMpi::reset() {
    Mutex::Autolock autoLock(mLock);
    if (mCtx == NULL) {
        return -1;
    }

    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    int ret = ctx->mpp_mpi->reset(ctx->mpp_ctx);
    if (MPP_OK != ret) {
        ALOGE("reset: mpi->reset failed\n");
        return -1;
    }
    return 0;
}

int RockitHwMpi::flush() {
    if (mCtx == NULL) {
        return -1;
    }

    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    int ret = ctx->mpp_mpi->reset(ctx->mpp_ctx);
    if (MPP_OK != ret) {
        ALOGE("flush: mpi->reset failed\n");
        return -1;
    }
    return 0;
}

int RockitHwMpi::control(int cmd, const RockitHWParamPairs& param) {
    Mutex::Autolock autoLock(mLock);
    int ret = 0;
    if (mCtx == NULL) {
        return -1;
    }

    RockitHWParamPairs pairs = (RockitHWParamPairs)param;
    RockitHWCtrCmd event = (RockitHWCtrCmd)cmd;
    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    switch (event) {
        case RockitHWCtrCmd::HW_CMD_BUFFER_GROUP_CLEAR:
            if (mDebug) {
                ALOGD("%s: HW_CMD_BUFFER_GROUP_CLEAR", __FUNCTION__);
                dumpMppBufferList();
            }
            freeDataBufferList();
            cleanMppBuffer(BUFFER_SITE_BY_MPI);
            mpp_buffer_group_clear(ctx->frm_grp);
            break;
        case RockitHWCtrCmd::HW_CMD_BUFFER_READY:
            if (mDebug) {
                ALOGD("%s: HW_CMD_BUFFER_READY", __FUNCTION__);
            }
            ret = bufferReady();
            break;
        case RockitHWCtrCmd::HW_CMD_BUFFER_DATA_CLEAR: {
                if (mDebug) {
                    ALOGD("%s: HW_CMD_BUFFER_DATA_CLEAR", __FUNCTION__);
                }

                int buffer_id = (int)getValue(pairs, (uint32_t)RockitHWParamKey::HW_KEY_DATA_BUFFER);
                freeDataBuffer(buffer_id);
            } break;
        default:
            ALOGE("%s: cmd = %d not support", __FUNCTION__, cmd);
            ret = -1;
            break;
    }

    return ret;
}

int RockitHwMpi::query(int cmd, RockitHWParamPairs& out) {
    Mutex::Autolock autoLock(mLock);
    int result = 0;
    if (mCtx == NULL) {
        return -1;
    }

    RockitHWQueryCmd event = (RockitHWQueryCmd)cmd;
    switch (event) {
        case RockitHWQueryCmd::KEY_HW_QUERY_WIDTH_STRIDE: {
                out.pairs.resize(1);
                out.counter = 0;
                setValue(out, (uint32_t)RockitHWParamKey::HW_KEY_WIDTH_STRIDE, (uint64_t)mWStride);
            } break;
        case RockitHWQueryCmd::KEY_HW_QUERY_HEIGHT_STRIDE: {
                out.pairs.resize(1);
                out.counter = 0;
                setValue(out, (uint32_t)RockitHWParamKey::HW_KEY_HEIGHT_STRIDE, (uint64_t)mHStride);
            } break;
        case RockitHWQueryCmd::KEY_HW_QUERY_UNUSED_BUFFER_COUNTER: {
                out.pairs.resize(1);
                out.counter = 0;
                int number = 0;
                MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
                if (ctx) {
                    number = mpp_buffer_group_unused(ctx->frm_grp);
                }
                setValue(out, (uint32_t)RockitHWParamKey::HW_KEY_TOKEN, (uint64_t)number);
            } break;
        default:
            ALOGD("%s: cmd = %d not support, add codes here", __FUNCTION__, event);
            result = -1;
            break;
    }

    return result;
}

int RockitHwMpi::bufferReady() {
    int ret = 0;
    if (mCtx == NULL) {
        return -1;
    }

    MpiCodecContext* ctx = (MpiCodecContext*)mCtx;
    MppCtx mpp_ctx         = ctx->mpp_ctx;
    MppApi *mpp_mpi        = ctx->mpp_mpi;
    MppBufferGroup frm_grp = ctx->frm_grp;

    if ((mpp_ctx != NULL) && (mpp_ctx != NULL) && (frm_grp != NULL)) {
        mpp_mpi->control(mpp_ctx, MPP_DEC_SET_EXT_BUF_GROUP, frm_grp);
        mpp_mpi->control(mpp_ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
    } else {
        ret = -1;
    }

    return ret;
}

}  // namespace utils
}  // namespace V1_0
}  // namespace hw
}  // namespace rockit
}  // namespace hardware
}  // namespace rockchip

