/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 * author: kevin.chen@rock-chips.com
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "MpiJpegEncoder"
#include <utils/Log.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mpp_err.h"
#include "Utils.h"
#include "MpiDebug.h"
#include "version.h"
#include "RKExifWrapper.h"
#include "QList.h"
#include "MpiJpegEncoder.h"

uint32_t enc_debug = 0;

typedef struct {
    struct timeval start;
    struct timeval end;
} DebugTimeInfo;

static DebugTimeInfo time_info;

static void time_start_record()
{
    if (enc_debug & DEBUG_TIMING) {
        gettimeofday(&time_info.start, NULL);
    }
}

static void time_end_record(const char *task)
{
    if (enc_debug & DEBUG_TIMING) {
        gettimeofday(&time_info.end, NULL);
        ALOGD("%s consumes %ld ms", task,
              (time_info.end.tv_sec  - time_info.start.tv_sec)  * 1000 +
              (time_info.end.tv_usec - time_info.start.tv_usec) / 1000);
    }
}

MpiJpegEncoder::MpiJpegEncoder() :
    mMppCtx(NULL),
    mMpi(NULL),
    mInitOK(0),
    mFrameCount(0),
    mWidth(0),
    mHeight(0),
    mHorStride(0),
    mVerStride(0),
    mEncodeQuality(-1),
    mMemGroup(NULL),
    mPackets(NULL),
    mInputFile(NULL),
    mOutputFile(NULL)
{
    ALOGI("version: %s", HWJPEG_VERSION_INFO);

    /* input format set to YUV420SP default */
    mFmt = INPUT_FMT_YUV420SP;

    get_env_u32("hwjpeg_enc_debug", &enc_debug, 0);
}

MpiJpegEncoder::~MpiJpegEncoder()
{
    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }
    if (mMemGroup) {
        mpp_buffer_group_put(mMemGroup);
        mMemGroup = NULL;
    }

    if (mPackets) {
        delete mPackets;
        mPackets = NULL;
    }

    if (mInputFile != NULL) {
        fclose(mInputFile);
    }
    if (mOutputFile != NULL) {
        fclose(mOutputFile);
    }
}

bool MpiJpegEncoder::prepareEncoder()
{
    MPP_RET ret = MPP_OK;
    MppParam param = NULL;
    MppPollType timeout = MPP_POLL_BLOCK;

    if (mInitOK)
        return true;

    ret = mpp_create(&mMppCtx, &mMpi);
    if (MPP_OK != ret) {
        ALOGE("failed to create mpp context");
        goto FAIL;
    }

    ret = mpp_init(mMppCtx, MPP_CTX_ENC, MPP_VIDEO_CodingMJPEG);
    if (MPP_OK != ret) {
        ALOGE("failed to init mpp");
        goto FAIL;
    }

    // NOTE: timeout value please refer to MppPollType definition
    // 0   - non-block call (default)
    // -1   - block call
    // +val - timeout value in ms
    {
        param = &timeout;
        ret = mMpi->control(mMppCtx, MPP_SET_OUTPUT_TIMEOUT, param);
        if (MPP_OK != ret) {
            ALOGE("failed to set output timeout %d ret %d", timeout, ret);
            goto FAIL;
        }

        ret = mMpi->control(mMppCtx, MPP_SET_INPUT_TIMEOUT, param);
        if (MPP_OK != ret) {
            ALOGE("failed to set input timeout %d ret %d", timeout, ret);
            goto FAIL;
        }
    }

    mPackets = new QList((node_destructor)mpp_packet_deinit);

    /* mpp memery buffer group */
    mpp_buffer_group_get_internal(&mMemGroup, MPP_BUFFER_TYPE_ION);

    mInitOK = 1;

    return true;

FAIL:
    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }
    return false;
}

void MpiJpegEncoder::flushBuffer()
{
    if (mInitOK) {
        mPackets->lock();
        mPackets->flush();
        mPackets->unlock();

        mMpi->reset(mMppCtx);
    }
}

void MpiJpegEncoder::updateEncodeQuality(int quant)
{
    MPP_RET ret = MPP_OK;
    MppEncCfg cfg = NULL;

    if (mEncodeQuality == quant)
        return;

    if (quant < 0 || quant > 10) {
        ALOGW("invalid quality level %d and set to default 8 default", quant);
        quant = 8;
    }

    ALOGD("update encode quality - %d", quant);

    mpp_enc_cfg_init(&cfg);

    mpp_enc_cfg_set_s32(cfg, "codec:type", MPP_VIDEO_CodingMJPEG);
    mpp_enc_cfg_set_s32(cfg, "rc:mode", MPP_ENC_RC_MODE_FIXQP);

    /* range from 1~10 */
    mpp_enc_cfg_set_s32(cfg, "jpeg:change", MPP_ENC_JPEG_CFG_CHANGE_QP);
    mpp_enc_cfg_set_s32(cfg, "jpeg:quant", quant);

    ret = mMpi->control(mMppCtx, MPP_ENC_SET_CFG, cfg);
    if (MPP_OK != ret)
        ALOGE("failed to set encode quality - %d", quant);
    else
        mEncodeQuality = quant;
}

bool MpiJpegEncoder::updateEncodeCfg(int width, int height,
                                     InputFormat fmt, int qLvl,
                                     int wstride, int hstride)
{
    MPP_RET ret = MPP_OK;
    MppEncPrepCfg prep_cfg;

    if (!mInitOK) {
        ALOGW("please prepare encoder first before updateEncodeCfg");
        return false;
    }

    if (mWidth == width && mHeight == height && mFmt == fmt)
        return true;

    if (width < 16 || width > 8192) {
        ALOGE("invalid width %d is not in range [16..8192]", width);
        return false;
    }

    if (height < 16 || height > 8192) {
        ALOGE("invalid height %d is not in range [16..8192]", height);
        return false;
    }

    int hor_stride = (wstride > 0) ? wstride : width;
    int ver_stride = (hstride > 0) ? hstride : height;

    prep_cfg.change       = MPP_ENC_PREP_CFG_CHANGE_INPUT |
                            MPP_ENC_PREP_CFG_CHANGE_ROTATION |
                            MPP_ENC_PREP_CFG_CHANGE_FORMAT;
    prep_cfg.width        = width;
    prep_cfg.height       = height;
    prep_cfg.hor_stride   = hor_stride;
    prep_cfg.ver_stride   = ver_stride;
    prep_cfg.format       = (MppFrameFormat)fmt;
    prep_cfg.rotation     = MPP_ENC_ROT_0;
    ret = mMpi->control(mMppCtx, MPP_ENC_SET_PREP_CFG, &prep_cfg);
    if (MPP_OK != ret) {
        ALOGE("mpi control enc set prep cfg failed ret %d", ret);
        return false;
    }

    updateEncodeQuality(qLvl);

    mWidth = width;
    mHeight = height;
    mHorStride = hor_stride;
    mVerStride = ver_stride;
    mFmt = fmt;

    ALOGD("updateCfg: w %d h %d wstride %d hstride %d inputFmt %d",
          mWidth, mHeight, mHorStride, mVerStride, mFmt);

    return true;
}

void MpiJpegEncoder::deinitOutputPacket(OutputPacket_t *aPktOut)
{
    MppPacket packet = NULL;

    if (NULL == aPktOut || NULL == aPktOut->packetHandler)
        return;

    mPackets->lock();
    mPackets->del_at_tail(&packet, sizeof(packet));
    if (packet == aPktOut->packetHandler) {
        mpp_packet_deinit(&packet);
    } else {
        ALOGW("deinit found invaild output packet");
        mpp_packet_deinit(&aPktOut->packetHandler);
    }
    mPackets->unlock();

    memset(aPktOut, 0, sizeof(OutputPacket_t));
}

int MpiJpegEncoder::getFrameSize(InputFormat fmt, int width, int height)
{
    int size = 0;
    int wstride = ALIGN(width, 16);
    int hstride = ALIGN(height, 16);

    if ((MppFrameFormat)fmt <= MPP_FMT_YUV420SP_VU)
        size = wstride * hstride * 3 / 2;
    else if ((MppFrameFormat)fmt <= MPP_FMT_YUV422_UYVY) {
        // NOTE: yuyv and uyvy need to double stride
        hstride *= 2;
        size = hstride * wstride;
    } else {
        size = hstride * wstride * 4;
    }

    return size;
}

bool MpiJpegEncoder::encodeFrame(char *data, OutputPacket_t *aPktOut)
{
    MPP_RET    ret       = MPP_OK;
    /* input frame and output packet */
    MppFrame   inFrm     = NULL;
    MppBuffer  inFrmBuf  = NULL;
    void      *inFrmPtr  = NULL;
    MppPacket  outPkt    = NULL;

    if (!mInitOK) {
        ALOGW("please prepare encoder first before encodeFrame");
        return false;
    }

    time_start_record();

    int size = getFrameSize(mFmt, mWidth, mHeight);
    ret = mpp_buffer_get(mMemGroup, &inFrmBuf, size);
    if (MPP_OK != ret) {
        ALOGE("failed to get buffer for input frame ret %d", ret);
        goto ENCODE_OUT;
    }

    inFrmPtr = mpp_buffer_get_ptr(inFrmBuf);

    // NOTE: The hardware vpu only process buffer aligned, so we translate
    // input frame to aligned before encode
    ret = CommonUtil::readImage(data, (char*)inFrmPtr,
                                mWidth, mHeight,
                                mHorStride, mVerStride,
                                (MppFrameFormat)mFmt);
    if (MPP_OK != ret)
        goto ENCODE_OUT;

    ret = mpp_frame_init(&inFrm);
    if (MPP_OK != ret) {
        ALOGE("failed to init input frame");
        goto ENCODE_OUT;
    }

    mpp_frame_set_width(inFrm, mWidth);
    mpp_frame_set_height(inFrm, mHeight);
    mpp_frame_set_hor_stride(inFrm, mHorStride);
    mpp_frame_set_ver_stride(inFrm, mVerStride);
    mpp_frame_set_fmt(inFrm, (MppFrameFormat)mFmt);
    mpp_frame_set_buffer(inFrm, inFrmBuf);

    /* dump input frame at fp_input if neccessary */
    if (enc_debug & DEBUG_RECORD_IN) {
        char fileName[40];

        sprintf(fileName, "/data/video/enc_input_%d.yuv", mFrameCount);
        mInputFile = fopen(fileName, "wb");
        if (mInputFile) {
            CommonUtil::dumpMppFrameToFile(inFrm, mInputFile);
            ALOGD("dump input yuv[%d %d] to %s", mWidth, mHeight, fileName);
        } else {
            ALOGD("failed to open input file, err - %s", strerror(errno));
        }
    }

    ret = mMpi->encode_put_frame(mMppCtx, inFrm);
    if (MPP_OK != ret) {
        ALOGE("failed to put input frame");
        goto ENCODE_OUT;
    }

    ret = mMpi->encode_get_packet(mMppCtx, &outPkt);
    if (MPP_OK != ret) {
        ALOGE("failed to get output packet");
        goto ENCODE_OUT;
    }

    if (outPkt) {
        memset(aPktOut, 0, sizeof(OutputPacket_t));

        aPktOut->data = (char*)mpp_packet_get_pos(outPkt);
        aPktOut->size = mpp_packet_get_length(outPkt);
        aPktOut->packetHandler = outPkt;

        /* dump output packet at mOutputFile if neccessary */
        if (enc_debug & DEBUG_RECORD_OUT) {
            char fileName[40];

            sprintf(fileName, "/data/video/enc_output_%d.jpg", mFrameCount);
            mOutputFile = fopen(fileName, "wb");
            if (mOutputFile) {
                CommonUtil::dumpMppPacketToFile(outPkt, mOutputFile);
                ALOGD("dump output jpg to %s", fileName);
            } else {
                ALOGD("failed to open output file, err - %s", strerror(errno));
            }
        }

        mPackets->lock();
        mPackets->add_at_tail(&outPkt, sizeof(outPkt));
        mPackets->unlock();

        ALOGV("encoded one frame get output size %d", aPktOut->size);
    }

ENCODE_OUT:
    if (inFrm) {
        mpp_frame_deinit(&inFrm);
        inFrm = NULL;
    }
    if (inFrmBuf) {
        mpp_buffer_put(inFrmBuf);
        inFrmBuf = NULL;
    }

    mFrameCount++;
    time_end_record("encode frame");

    return ret == MPP_OK ? true : false;
}

bool MpiJpegEncoder::encodeFile(const char *inputFile, const char *outputFile)
{
    MPP_RET  ret   = MPP_OK;
    /* input data and length */
    char    *buf   = NULL;
    size_t   lenth = 0;

    /* output packet handler */
    OutputPacket_t pktOut;

    ALOGD("mpi_jpeg_enc encodeFile start with cfg %dx%d inputFmt %d",
          mWidth, mHeight, mFmt);

    ret = CommonUtil::storeFileData(inputFile, &buf, &lenth);
    if (MPP_OK != ret)
        goto ENCODE_OUT;

    if (!encodeFrame(buf, &pktOut)) {
        ALOGE("failed to encode input frame");
        goto ENCODE_OUT;
    }

    // write output packet to destination.
    CommonUtil::dumpDataToFile(pktOut.data, pktOut.size, outputFile);

    ALOGD("get output file %s - size %d", outputFile, pktOut.size);

    deinitOutputPacket(&pktOut);
    flushBuffer();

ENCODE_OUT:
    if (buf)
        free(buf);

    return ret == MPP_OK ? true : false;
}

MPP_RET MpiJpegEncoder::runFrameEnc(MppFrame inFrm, MppPacket outPkt)
{
    MPP_RET ret         = MPP_OK;
    MppTask task        = NULL;

    if (!inFrm || !outPkt)
        return MPP_NOK;

    /* start queue input task */
    ret = mMpi->poll(mMppCtx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (MPP_OK != ret) {
        ALOGE("failed to poll input_task");
        return ret;
    }

    /* dequeue input port */
    ret = mMpi->dequeue(mMppCtx, MPP_PORT_INPUT, &task);
    if (MPP_OK != ret) {
        ALOGE("failed dequeue to input_task ");
        return ret;
    }

    mpp_task_meta_set_frame(task, KEY_INPUT_FRAME, inFrm);
    mpp_task_meta_set_packet(task, KEY_OUTPUT_PACKET, outPkt);

    /* enqueue input port */
    ret = mMpi->enqueue(mMppCtx, MPP_PORT_INPUT, task);
    if (MPP_OK != ret) {
        ALOGE("failed to enqueue input_task");
        return ret;
    }

    task = NULL;

    /* poll and wait here */
    ret = mMpi->poll(mMppCtx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (MPP_OK != ret) {
        ALOGE("failed to poll output_task");
        return ret;
    }

    /* dequeue output port */
    ret = mMpi->dequeue(mMppCtx, MPP_PORT_OUTPUT, &task);
    if (MPP_OK != ret) {
        ALOGE("failed to dequeue output_task");
        return ret;
    }

    if (task) {
        MppPacket packet = NULL;
        mpp_task_meta_get_packet(task, KEY_OUTPUT_PACKET, &packet);

        /* enqueue output port */
        ret = mMpi->enqueue(mMppCtx, MPP_PORT_OUTPUT, task);
        if (MPP_OK != ret) {
            ALOGE("failed to enqueue output_task");
            return ret;
        }

        return (packet == outPkt) ? MPP_OK : MPP_NOK;
    }

    return MPP_NOK;
}

MPP_RET MpiJpegEncoder::cropThumbImage(EncInInfo *aInfoIn, void* outAddr)
{
    MPP_RET ret = MPP_OK;

    char *src_addr   = (char*)aInfoIn->inputVirAddr;
    char *dst_addr   = (char*)outAddr;
    int   src_width  = ALIGN(aInfoIn->width, 2);
    int   src_height = ALIGN(aInfoIn->height, 2);
    int   dst_width  = ALIGN(aInfoIn->thumbWidth, 2);
    int   dst_height = ALIGN(aInfoIn->thumbHeight, 2);

    float hScale = (float)src_width / dst_width;
    float vScale = (float)src_height / dst_height;

    // librga can't support scale largger than 16
    if (hScale > 16 || vScale > 16) {
        int scale_width, scale_height;

        ALOGV("Big YUV scale[%f,%f], will crop twice instead.", hScale, vScale);

        scale_width = ALIGN(dst_width + (src_width - dst_width) / 2, 2);
        scale_height = ALIGN(dst_height + (src_height - dst_height) / 2, 2);

        ret = CommonUtil::cropImage(src_addr, dst_addr,
                                    src_width, src_height,
                                    src_width, src_height,
                                    scale_width, scale_height);
        if (MPP_OK != ret) {
            ALOGE("failed to crop scale ret %d", ret);
            return ret;
        }

        src_addr = dst_addr = (char*)outAddr;
        src_width = scale_width;
        src_height = scale_height;
    }

    ret = CommonUtil::cropImage(src_addr, dst_addr,
                                src_width, src_height,
                                src_width, src_height,
                                dst_width, dst_height);

    return ret;
}


bool MpiJpegEncoder::encodeImageFD(EncInInfo *aInfoIn, EncOutInfo *aOutInfo)
{
    MPP_RET   ret       = MPP_OK;
    /* input frame and output packet */
    MppFrame  inFrm     = NULL;
    MppBuffer inFrmBuf  = NULL;
    MppPacket outPkt    = NULL;
    MppBuffer outPktBuf = NULL;

    int width  = aInfoIn->width;
    int height = aInfoIn->height;

    ALOGV("start encode frame w %d h %d", width, height);

    if (!CommonUtil::isValidDmaFd(aInfoIn->inputPhyAddr)) {
        ALOGE("invalid dma fd %d", aInfoIn->inputPhyAddr);
        return false;
    }

    /* update encode quality and config before encode */
    updateEncodeCfg(width, height, aInfoIn->format, aInfoIn->qLvl);

    mpp_frame_init(&inFrm);
    mpp_frame_set_width(inFrm, width);
    mpp_frame_set_height(inFrm, height);
    // yuv buffer from cameraHal didn't has stride
    mpp_frame_set_hor_stride(inFrm, width);
    mpp_frame_set_ver_stride(inFrm, height);
    mpp_frame_set_fmt(inFrm, (MppFrameFormat)aInfoIn->format);

    {
        /* try import input fd to vpu */
        MppBufferInfo inputCommit;
        memset(&inputCommit, 0, sizeof(MppBufferInfo));
        inputCommit.type = MPP_BUFFER_TYPE_ION;
        inputCommit.size = getFrameSize(aInfoIn->format, width, height);
        inputCommit.fd = aInfoIn->inputPhyAddr;

        ret = mpp_buffer_import(&inFrmBuf, &inputCommit);
        if (MPP_OK != ret) {
            ALOGE("failed to import input buffer");
            goto ENCODE_OUT;
        }
        mpp_frame_set_buffer(inFrm, inFrmBuf);
    }

    {
        /* try import output fd to vpu */
        MppBufferInfo outputCommit;
        memset(&outputCommit, 0, sizeof(MppBufferInfo));
        outputCommit.type = MPP_BUFFER_TYPE_ION;
        outputCommit.size = aOutInfo->outBufLen;
        outputCommit.fd = aOutInfo->outputPhyAddr;

        ret = mpp_buffer_import(&outPktBuf, &outputCommit);
        if (MPP_OK != ret) {
            ALOGE("failed to import output buffer");
            goto ENCODE_OUT;
        }

        mpp_packet_init_with_buffer(&outPkt, outPktBuf);
        /* NOTE: It is important to clear output packet length */
        mpp_packet_set_length(outPkt, 0);
    }

    ret = runFrameEnc(inFrm, outPkt);
    if (MPP_OK == ret) {
        OutputPacket_t *pOutPkt = &aOutInfo->outPkt;
        memset(pOutPkt, 0, sizeof(OutputPacket_t));

        pOutPkt->data = (char*)mpp_packet_get_pos(outPkt);
        pOutPkt->size = mpp_packet_get_length(outPkt);
        pOutPkt->packetHandler = outPkt;

        mPackets->lock();
        mPackets->add_at_tail(&outPkt, sizeof(outPkt));
        mPackets->unlock();

        ALOGV("encod frame get output size %d", pOutPkt->size);
    }

ENCODE_OUT:
    if (inFrmBuf)
        mpp_buffer_put(inFrmBuf);
    if (outPktBuf)
        mpp_buffer_put(outPktBuf);

    if (inFrm)
        mpp_frame_deinit(&inFrm);

    return ret == MPP_OK ? true : false;;
}

bool MpiJpegEncoder::encodeThumb(EncInInfo *aInfoIn, uint8_t **data, int *len)
{
    MPP_RET    ret       = MPP_OK;
    /* input frame and output packet */
    MppFrame   inFrm     = NULL;
    MppBuffer  inFrmBuf  = NULL;
    void      *inFrmPtr  = NULL;
    MppPacket  outPkt    = NULL;
    MppBuffer  outPktBuf = NULL;

    int width  = aInfoIn->thumbWidth;
    int height = aInfoIn->thumbHeight;
    int allocWidth  = width;
    int allocHeight = height;

    ALOGV("start encode thumb size w %d h %d", width, height);

    /* update encode quality and config before encode */
    updateEncodeCfg(width, height, aInfoIn->format, aInfoIn->thumbQLvl);

    mpp_frame_init(&inFrm);
    mpp_frame_set_width(inFrm, width);
    mpp_frame_set_height(inFrm, height);
    mpp_frame_set_hor_stride(inFrm, width);
    mpp_frame_set_ver_stride(inFrm, height);
    mpp_frame_set_fmt(inFrm, (MppFrameFormat)aInfoIn->format);

    {
        /* we need to cut raw yuv image into small size for thumbnail
           first. since librga can't support scale larger than 16, we
           need crop twice sometimes.
           in this case, we need allocate larger buffer. */
        float hScale = (float)aInfoIn->width / aInfoIn->thumbWidth;
        float vScale = (float)aInfoIn->height / aInfoIn->thumbHeight;

        if (hScale > 16 || vScale > 16) {
            allocWidth = width + (aInfoIn->width - width) / 2;
            allocHeight = height + (aInfoIn->height - height) / 2;
        }
    }

    int size = getFrameSize(aInfoIn->format, allocWidth, allocHeight);
    ret = mpp_buffer_get(mMemGroup, &inFrmBuf, size);
    if (MPP_OK != ret) {
        ALOGE("failed to get buffer for input frame ret %d", ret);
        goto ENCODE_OUT;
    }

    inFrmPtr = mpp_buffer_get_ptr(inFrmBuf);

    /* crop raw buffer to the size of thumbnail */
    ret = cropThumbImage(aInfoIn, inFrmPtr);
    if (MPP_OK != ret) {
        ALOGE("failed to crop yuv image before encode thumb.");
        goto ENCODE_OUT;
    }

    mpp_frame_set_buffer(inFrm, inFrmBuf);

    /* allocate output packet buffer */
    ret = mpp_buffer_get(mMemGroup, &outPktBuf, width * height);
    if (MPP_OK != ret) {
        ALOGE("failed to get buffer for output packet ret %d", ret);
        goto ENCODE_OUT;
    }
    mpp_packet_init_with_buffer(&outPkt, outPktBuf);
    /* NOTE: It is important to clear output packet length */
    mpp_packet_set_length(outPkt, 0);

    ret = runFrameEnc(inFrm, outPkt);
    if (MPP_OK == ret) {
        uint8_t *ptr = (uint8_t*)mpp_packet_get_data(outPkt);
        int length = mpp_packet_get_length(outPkt);

        if (length > 0) {
            *data = (uint8_t*)malloc(length);
            if (*data) {
                *len = length;
                memcpy(*data, ptr, length);
            }
        }

        mpp_packet_deinit(&outPkt);

        ALOGV("get thumb jpg output size %d", length);
    }

ENCODE_OUT:
    if (inFrmBuf)
        mpp_buffer_put(inFrmBuf);
    if (outPktBuf)
        mpp_buffer_put(outPktBuf);

    if (inFrm)
        mpp_frame_deinit(&inFrm);

    return ret == MPP_OK ? true : false;
}

bool MpiJpegEncoder::encode(EncInInfo *inInfo, EncOutInfo *outInfo)
{
    bool ret = false;
    uint8_t *tmpBuf = NULL;
    OutputPacket_t pktOut;

    /* exif header releated */
    ExifParam eParam;
    uint8_t *header_buf = NULL;
    int header_size = 0;

    /* APP0 header length of encoded picture default */
    static const int g_app0_header_length = 20;

    if (!mInitOK) {
        ALOGW("please prepare encoder first before encode");
        return false;
    }

    ALOGD("start task: width %d height %d thumbWidth %d thumbHeight %d",
          inInfo->width, inInfo->height, inInfo->thumbWidth, inInfo->thumbHeight);

    time_start_record();

    /* dump input data if neccessary */
    if (enc_debug & DEBUG_RECORD_IN) {
        char fileName[40];

        sprintf(fileName, "/data/video/enc_input_%d.yuv", mFrameCount);
        mInputFile = fopen(fileName, "wb");
        if (mInputFile) {
            int size = getFrameSize(inInfo->format, inInfo->width, inInfo->height);
            CommonUtil::dumpDataToFile((char*)inInfo->inputVirAddr, size, mInputFile);
            ALOGD("dump input yuv[%d %d] to %s", inInfo->width, inInfo->height, fileName);
        } else {
            ALOGD("failed to open input file, err - %s", strerror(errno));
        }
    }

    memset(&eParam, 0, sizeof(ExifParam));
    eParam.exif_info = (RkExifInfo*)inInfo->exifInfo;

    if (inInfo->doThumbNail) {
        ret = encodeThumb(inInfo, &eParam.thumb_data, &eParam.thumb_size);
        if (!ret || eParam.thumb_size <= 0) {
            inInfo->doThumbNail = 0;
            ALOGW("failed to get thumbNail, will remove it.");
        }
    }

    /* produce exif header, insert thumbnail if nessessary */
    ret = RKExifWrapper::getExifHeader(&eParam, &header_buf, &header_size);
    if (!ret || header_size <= 0) {
        ALOGE("failed to get exif header.");
        goto TASK_OUT;
    }

    /* encode raw image by commit input\output fd */
    ret = encodeImageFD(inInfo, outInfo);
    if (!ret) {
        ALOGE("failed to encode task.");
        goto TASK_OUT;
    }

    /*
     * output frame carried APP0 header information deault, so remove
     * APP0 header first if wants to carry APP1 header.
     */
    pktOut = outInfo->outPkt;
    tmpBuf = (uint8_t*)malloc(pktOut.size + header_size - g_app0_header_length);
    memcpy(tmpBuf, header_buf, header_size);
    memcpy(tmpBuf + header_size,
           pktOut.data + g_app0_header_length,
           pktOut.size - g_app0_header_length);

    outInfo->outBufLen = pktOut.size + header_size - g_app0_header_length;

    memset(outInfo->outputVirAddr, 0, outInfo->outBufLen);
    memcpy(outInfo->outputVirAddr, tmpBuf, outInfo->outBufLen);

    /* dump output buffer if neccessary */
    if (enc_debug & DEBUG_RECORD_OUT) {
        char fileName[40];

        sprintf(fileName, "/data/video/enc_output_%d.jpg", mFrameCount);
        mOutputFile = fopen(fileName, "wb");
        if (mOutputFile) {
            CommonUtil::dumpDataToFile((char*)outInfo->outputVirAddr,
                                       outInfo->outBufLen, mOutputFile);
            ALOGD("dump output jpg to %s", fileName);
        } else {
            ALOGD("failed to open output file, err - %s", strerror(errno));
        }
    }

    deinitOutputPacket(&pktOut);

    ALOGD("get output w %d h %d len %d", mWidth, mHeight, outInfo->outBufLen);

TASK_OUT:
    if (eParam.thumb_data) {
        free(eParam.thumb_data);
        eParam.thumb_data = NULL;
    }
    if (header_buf) {
        free(header_buf);
        header_buf = NULL;
    }
    if (tmpBuf) {
        free(tmpBuf);
        tmpBuf = NULL;
    }

    if (!ret) {
        // indicate that the execution failed
        outInfo->outBufLen = 0;
    }

    time_end_record("encodeImage");

    return ret;
}
