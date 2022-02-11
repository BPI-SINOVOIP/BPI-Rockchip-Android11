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
#define LOG_TAG "MpiJpegDecoder"
#include <utils/Log.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <mpp_err.h>
#include <rk_type.h>
#include <sys/time.h>

#include "MpiDebug.h"
#include "QList.h"
#include "JpegParser.h"
#include "Utils.h"
#include "version.h"
#include "MpiJpegDecoder.h"

uint32_t dec_debug = 0;

typedef struct {
    struct timeval start;
    struct timeval end;
} DebugTimeInfo;

static DebugTimeInfo time_info;

static void time_start_record()
{
    if (dec_debug & DEBUG_TIMING) {
        gettimeofday(&time_info.start, NULL);
    }
}

static void time_end_record(const char *task)
{
    if (dec_debug & DEBUG_TIMING) {
        gettimeofday(&time_info.end, NULL);
        ALOGD("%s consumes %ld ms", task,
              (time_info.end.tv_sec  - time_info.start.tv_sec)  * 1000 +
              (time_info.end.tv_usec - time_info.start.tv_usec) / 1000);
    }
}

MpiJpegDecoder::MpiJpegDecoder() :
    mMppCtx(NULL),
    mMpi(NULL),
    mInitOK(0),
    mFdOutput(false),
    mOutputCrop(false),
    mDecWidth(0),
    mDecHeight(0),
    mPacketCount(0),
    mPackets(NULL),
    mFrames(NULL),
    mPacketGroup(NULL),
    mFrameGroup(NULL),
    mInputFile(NULL),
    mOutputFile(NULL)
{
    ALOGI("version: %s", HWJPEG_VERSION_INFO);

    // Output Format set to YUV420SP default
    mOutputFmt = OUT_FORMAT_YUV420SP;
    mBpp = 1.5;

    // keep DDR performance for usb camera preview mode
    CommonUtil::setPerformanceMode(1);

    get_env_u32("hwjpeg_dec_debug", &dec_debug, 0);
    if (dec_debug & DEBUG_OUTPUT_CROP) {
        ALOGD("decoder will crop output.");
        mOutputCrop = true;
    }
}

MpiJpegDecoder::~MpiJpegDecoder()
{
    CommonUtil::setPerformanceMode(0);

    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }

    if (mPackets) {
        delete mPackets;
        mPackets = NULL;
    }
    if (mFrames) {
        delete mFrames;
        mFrames = NULL;
    }
    if (mPacketGroup) {
        mpp_buffer_group_put(mPacketGroup);
        mPacketGroup = NULL;
    }
    if (mFrameGroup) {
        mpp_buffer_group_put(mFrameGroup);
        mFrameGroup = NULL;
    }

    if (mInputFile != NULL) {
        fclose(mInputFile);
    }
    if (mOutputFile != NULL) {
        fclose(mOutputFile);
    }
}

bool MpiJpegDecoder::reInitDecoder()
{
    MPP_RET ret = MPP_OK;
    MppParam param = NULL;
    // non-block call
    MppPollType timeout = MPP_POLL_NON_BLOCK;

    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }

    ret = mpp_create(&mMppCtx, &mMpi);
    if (MPP_OK != ret) {
        ALOGE("failed to create mpp context");
        goto FAIL;
    }

    // NOTE: timeout value please refer to MppPollType definition
    //  0   - non-block call (default)
    // -1   - block call
    // +val - timeout value in ms
    if (timeout) {
        param = &timeout;
        ret = mMpi->control(mMppCtx, MPP_SET_OUTPUT_TIMEOUT, param);
        if (MPP_OK != ret) {
            ALOGE("failed to set output timeout %d ret %d", timeout, ret);
            goto FAIL;
        }
    }

    ret = mpp_init(mMppCtx, MPP_CTX_DEC, MPP_VIDEO_CodingMJPEG);
    if (MPP_OK != ret) {
        ALOGE("failed to init mpp");
        goto FAIL;
    }

    /* NOTE: change output format before jpeg decoding */
    if (mOutputFmt < MPP_FMT_BUTT) {
        ret = mMpi->control(mMppCtx, MPP_DEC_SET_OUTPUT_FORMAT, &mOutputFmt);
        if (MPP_OK != ret)
            ALOGE("failed to set output format %d ret %d", mOutputFmt, ret);
    }

    return true;

FAIL:
    if (mMppCtx) {
        mpp_destroy(mMppCtx);
        mMppCtx = NULL;
    }
    return false;
}

bool MpiJpegDecoder::prepareDecoder()
{
    if (mInitOK)
        return true;

    if (!reInitDecoder()) {
        ALOGE("failed to init mpp decoder");
        return false;
    }

    mPackets = new QList((node_destructor)mpp_packet_deinit);
    mFrames = new QList((node_destructor)mpp_frame_deinit);

    /* Input packet buffer group */
    mpp_buffer_group_get_internal(&mPacketGroup, MPP_BUFFER_TYPE_ION);
    mpp_buffer_group_limit_config(mPacketGroup, 0, 5);

    /* Output frame buffer group */
    mpp_buffer_group_get_internal(&mFrameGroup, MPP_BUFFER_TYPE_ION);
    mpp_buffer_group_limit_config(mFrameGroup, 0, 24);

    mInitOK = 1;

    return true;
}

void MpiJpegDecoder::flushBuffer()
{
    if (mInitOK) {
        mPackets->lock();
        mPackets->flush();
        mPackets->unlock();

        mFrames->lock();
        mFrames->flush();
        mFrames->unlock();

        mMpi->reset(mMppCtx);
    }
}

void MpiJpegDecoder::setupOutputFrameFromMppFrame(
        OutputFrame_t *frameOut, MppFrame frame)
{
    MppBuffer frmBuf = mpp_frame_get_buffer(frame);

    memset(frameOut, 0, sizeof(frameOut));

    frameOut->DisplayWidth  = mpp_frame_get_width(frame);
    frameOut->DisplayHeight = mpp_frame_get_height(frame);
    frameOut->FrameWidth    = mpp_frame_get_hor_stride(frame);
    frameOut->FrameHeight   = mpp_frame_get_ver_stride(frame);
    frameOut->ErrorInfo     = mpp_frame_get_errinfo(frame) |
                              mpp_frame_get_discard(frame);
    frameOut->FrameHandler  = frame;

    if (frmBuf) {
        int fd = mpp_buffer_get_fd(frmBuf);
        void *ptr = mpp_buffer_get_ptr(frmBuf);

        frameOut->MemVirAddr = (char*)ptr;
        frameOut->MemPhyAddr = fd;
        frameOut->OutputSize = frameOut->FrameWidth * frameOut->FrameHeight * mBpp;
    }
}

void MpiJpegDecoder::cropOutputFrameIfNeccessary(OutputFrame_t *frameOut)
{
    MPP_RET ret = MPP_OK;

    if (!mOutputCrop)
        return;

    char *src_addr    = frameOut->MemVirAddr;
    char *dst_addr    = frameOut->MemVirAddr;
    int   src_wstride = frameOut->FrameWidth;
    int   src_hstride = frameOut->FrameHeight;
    int   src_width   = ALIGN(frameOut->DisplayWidth, 2);
    int   src_height  = ALIGN(frameOut->DisplayHeight, 2);
    int   dst_width   = ALIGN(src_width, 8);
    int   dst_height  = ALIGN(src_height, 8);

    if (src_width == src_wstride && src_height == src_hstride) {
        // NO NEED
        return;
    }

    if (NULL == frameOut->FrameHandler)
        return;

    ALOGV("librga: try crop from [%d, %d] -> [%d %d]",
          src_wstride, src_hstride, dst_width, dst_height);

    ret = CommonUtil::cropImage(src_addr, dst_addr,
                                src_width, src_height,
                                src_wstride, src_hstride,
                                dst_width, dst_height);
    if (MPP_OK == ret) {
        frameOut->FrameWidth  = dst_width;
        frameOut->FrameHeight = dst_height;
        frameOut->DisplayWidth  = dst_width;
        frameOut->DisplayHeight = dst_height;

        frameOut->OutputSize = frameOut->DisplayWidth * frameOut->DisplayHeight * mBpp;
    } else {
        ALOGW("failed to crop OutputFrame");
    }
}

MPP_RET MpiJpegDecoder::sendpacket(char *inputBuf, size_t length, int outPhyAddr)
{
    MPP_RET    ret       = MPP_OK;
    /* input packet and output frame */
    MppPacket  inPkt     = NULL;
    MppBuffer  inPktBuf  = NULL;
    MppFrame   outFrm    = NULL;
    MppBuffer  outFrmBuf = NULL;
    MppTask    task      = NULL;

    if (!mInitOK)
        return MPP_ERR_VPU_CODEC_INIT;

    uint32_t picWidth = 0, picHeight = 0;
    uint32_t wstride = 0, hstride = 0;

    // NOTE: the size of output frame and input packet depends on JPEG
    // dimens, so we get JPEG dimens from file header first.
    ret = jpeg_parser_get_dimens(inputBuf, length, &picWidth, &picHeight);
    if (MPP_OK != ret) {
        ALOGE("failed to get dimens from parser");
        return ret;
    }

    /* dump input data if neccessary */
    if ((dec_debug & DEBUG_RECORD_IN) && (mPacketCount % 10 == 0)) {
        char fileName[40];

        sprintf(fileName, "/data/video/dec_input_%d.jpg", mPacketCount);
        mInputFile = fopen(fileName, "wb");
        if (mInputFile) {
            CommonUtil::dumpDataToFile(inputBuf, length, mInputFile);
            ALOGD("dump input jpeg to %s", fileName);
        } else {
            ALOGD("failed to open input file, err %s", strerror(errno));
        }
    }

    ALOGV("get dimens w %d h %d", picWidth, picHeight);

    wstride = ALIGN(picWidth, 16);
    hstride = ALIGN(picHeight, 16);

    /* reinit mpp decoder when get resolution or format info-change */
    if (mDecWidth != 0 && mDecHeight != 0
        && (mDecWidth != picWidth || mDecHeight != picHeight)) {
        ALOGD("found info-change, reInit decoder.");
        reInitDecoder();
    }

    ret = mpp_buffer_get(mPacketGroup, &inPktBuf, length);
    if (MPP_OK != ret) {
        ALOGE("failed to get buffer for input packet ret %d", ret);
        goto SEND_OUT;
    }

    mpp_packet_init_with_buffer(&inPkt, inPktBuf);
    mpp_buffer_write(inPktBuf, 0, inputBuf, length);
    mpp_packet_set_length(inPkt, length);

    mPackets->lock();
    mPackets->add_at_tail(&inPkt, sizeof(inPkt));
    mPackets->unlock();

    if (outPhyAddr > 0) {
        mFdOutput = CommonUtil::isValidDmaFd(outPhyAddr);
        if (!mFdOutput)
            ALOGW("output dma buffer %d not a valid buffer,"
                  "change to use internal allocator", outPhyAddr);
    } else {
        mFdOutput = false;
    }

    ret = mpp_frame_init(&outFrm); /* output frame */
    if (MPP_OK != ret) {
        ALOGE("failed to init output frame");
        goto SEND_OUT;
    }

    if (mFdOutput) {
        /* try import output fd to vpu */
        MppBufferInfo outputCommit;

        memset(&outputCommit, 0, sizeof(outputCommit));
        outputCommit.type = MPP_BUFFER_TYPE_ION;
        outputCommit.fd = outPhyAddr;
        outputCommit.size = wstride * hstride * 2; // YUV420SP

        ret = mpp_buffer_import(&outFrmBuf, &outputCommit);
        if (MPP_OK != ret) {
            ALOGE("import output buffer failed");
            goto SEND_OUT;
        }
    } else {
        /*
         * NOTE: For jpeg could have YUV420 and ARGB the buffer should be
         * larger for output. And the buffer dimension should align to 16.
         * YUV420 buffer is 3/2 times of w*h.
         * YUV422 buffer is 2 times of w*h.
         * AGRB buffer is 4 times of w*h.
         */
        ret = mpp_buffer_get(mFrameGroup, &outFrmBuf,
                             wstride * hstride * (int)(mBpp + 0.5));
        if (MPP_OK != ret) {
            ALOGE("failed to get buffer for output frame ret %d", ret);
            goto SEND_OUT;
        }
    }

    mpp_frame_set_buffer(outFrm, outFrmBuf);

    /* start queue input task */
    ret = mMpi->poll(mMppCtx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (MPP_OK != ret) {
        ALOGE("failed to poll input_task");
        goto SEND_OUT;
    }

    /* input queue */
    ret = mMpi->dequeue(mMppCtx, MPP_PORT_INPUT, &task);
    if (MPP_OK != ret) {
        ALOGE("failed dequeue to input_task ");
        goto SEND_OUT;
    }

    mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, inPkt);
    mpp_task_meta_set_frame(task, KEY_OUTPUT_FRAME, outFrm);

    ret = mMpi->enqueue(mMppCtx, MPP_PORT_INPUT, task);
    if (MPP_OK != ret)
        ALOGE("failed to enqueue input_task");

    mDecWidth = picWidth;
    mDecHeight = picHeight;
    mPacketCount++;

SEND_OUT:
    if (inPktBuf) {
        mpp_buffer_put(inPktBuf);
        inPktBuf = NULL;
    }

    if (outFrmBuf) {
        mpp_buffer_put(outFrmBuf);
        outFrmBuf = NULL;
    }

    if (MPP_OK != ret) {
        mpp_frame_deinit(&outFrm);
        outFrm = NULL;
    }

    return ret;
}

MPP_RET MpiJpegDecoder::getoutframe(OutputFrame_t *aframeOut)
{
    MPP_RET ret   = MPP_OK;
    MppTask task  = NULL;

    if (!mInitOK)
        return MPP_ERR_VPU_CODEC_INIT;

    /* poll and wait here */
    ret = mMpi->poll(mMppCtx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (MPP_OK != ret) {
        ALOGE("failed to poll output_task");
        return ret;
    }

    /* output queue */
    ret = mMpi->dequeue(mMppCtx, MPP_PORT_OUTPUT, &task);
    if (MPP_OK != ret) {
        ALOGE("failed to dequeue output_task");
        return ret;
    }

    if (task) {
        MppFrame  outFrm = NULL;
        MppPacket inPkt  = NULL;

        mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &outFrm);

        /* setup output handler OutputFrame_t from MppFrame */
        setupOutputFrameFromMppFrame(aframeOut, outFrm);

        /* output from decoder may aligned by 16, so crop it before display */
        cropOutputFrameIfNeccessary(aframeOut);

        /* dump output buffer if neccessary */
        if ((dec_debug & DEBUG_RECORD_OUT) && mPacketCount % 10 == 0) {
            char fileName[40];

            sprintf(fileName, "/data/video/dec_output_%dx%d_%d.yuv",
                    aframeOut->FrameWidth, aframeOut->FrameHeight, mPacketCount);
            mOutputFile = fopen(fileName, "wb");
            if (mOutputFile) {
                if (mFdOutput) {
                    CommonUtil::dumpDmaFdToFile(aframeOut->MemPhyAddr,
                                                aframeOut->OutputSize, mOutputFile);
                } else {
                    CommonUtil::dumpDataToFile(aframeOut->MemVirAddr,
                                               aframeOut->OutputSize, mOutputFile);
                }
                ALOGD("dump output yuv [%d %d] to %s",
                      aframeOut->FrameWidth, aframeOut->FrameHeight, fileName);
            } else {
                ALOGD("failed to open output file, err - %s", strerror(errno));
            }
        }

        /* output queue */
        ret = mMpi->enqueue(mMppCtx, MPP_PORT_OUTPUT, task);
        if (MPP_OK != ret)
            ALOGE("failed to enqueue output_task");

        mFrames->lock();
        mFrames->add_at_tail(&outFrm, sizeof(outFrm));
        mFrames->unlock();

        mPackets->lock();
        mPackets->del_at_head(&inPkt, sizeof(inPkt));
        mpp_packet_deinit(&inPkt);
        mPackets->unlock();
    }

    return ret;
}

void MpiJpegDecoder::deinitOutputFrame(OutputFrame_t *aframeOut)
{
    MppFrame frame = NULL;

    if (NULL == aframeOut || NULL == aframeOut->FrameHandler) {
        ALOGW("deinitFrame found null input");
        return;
    }

    mFrames->lock();
    mFrames->del_at_tail(&frame, sizeof(frame));
    if (frame == aframeOut->FrameHandler) {
        mpp_frame_deinit(&frame);
    } else {
        ALOGW("deinit found negative output frame");
        mpp_frame_deinit(&aframeOut->FrameHandler);
    }
    mFrames->unlock();

    memset(aframeOut, 0, sizeof(OutputFrame_t));
}

bool MpiJpegDecoder::decodePacket(char* buf, size_t length, OutputFrame_t *frameOut)
{
    MPP_RET ret = MPP_OK;

    if (NULL == buf) {
        ALOGE("invalid null input");
        return false;
    }

    time_start_record();

    ret = sendpacket(buf, length, frameOut->outputPhyAddr);
    if (MPP_OK != ret) {
        ALOGE("failed to send input packet");
        goto DECODE_OUT;
    }

    ret = getoutframe(frameOut);
    if (MPP_OK != ret) {
        ALOGE("failed to get output frame");
        goto DECODE_OUT;
    }

    time_end_record("decode packet");

DECODE_OUT:
    return ret == MPP_OK ? true : false;
}

bool MpiJpegDecoder::decodeFile(const char *inputFile, const char *outputFile)
{
    MPP_RET  ret   = MPP_OK;
    /* input data and length */
    char    *buf   = NULL;
    size_t   lenth = 0;

    /* output frame handler */
    OutputFrame_t frameOut;

    ret = CommonUtil::storeFileData(inputFile, &buf, &lenth);
    if (MPP_OK != ret)
        goto DECODE_OUT;

    memset(&frameOut, 0, sizeof(OutputFrame_t));
    if (!decodePacket(buf, lenth, &frameOut)) {
        ALOGE("failed to decode input packet");
        goto DECODE_OUT;
    }

    ALOGD("get output file %s - dimens %dx%d",
          outputFile, frameOut.FrameWidth, frameOut.FrameHeight);

    // write output frame to destination.
    CommonUtil::dumpDataToFile(frameOut.MemVirAddr,
                               frameOut.OutputSize, outputFile);

    deinitOutputFrame(&frameOut);
    flushBuffer();

DECODE_OUT:
    if (buf)
        free(buf);

    return ret == MPP_OK ? true : false;
}
