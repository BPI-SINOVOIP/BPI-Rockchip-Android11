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

#ifndef __MPI_JPEG_ENCODER_H__
#define __MPI_JPEG_ENCODER_H__

#include "rk_mpi.h"

class QList;

class MpiJpegEncoder {
public:
    MpiJpegEncoder();
    ~MpiJpegEncoder();

    // Supported lists for InputFormat
    typedef enum {
        INPUT_FMT_YUV420SP     = MPP_FMT_YUV420SP,
        INPUT_FMT_YUV420P      = MPP_FMT_YUV420P,
        INPUT_FMT_YUV422SP_VU  = MPP_FMT_YUV422SP_VU,
        INPUT_FMT_YUV422_YUYV  = MPP_FMT_YUV422_YUYV,
        INPUT_FMT_YUV422_UYVY  = MPP_FMT_YUV422_UYVY,
        INPUT_FMT_ARGB8888     = MPP_FMT_ARGB8888,
        INPUT_FMT_RGBA8888     = MPP_FMT_RGBA8888,
        INPUT_FMT_ABGR8888     = MPP_FMT_ABGR8888
    } InputFormat;

    typedef struct {
        char  *data;
        int    size;
        /* Output packet hander */
        void  *packetHandler;
    } OutputPacket_t;

    typedef struct {
        /* input buffer information */
        int inputPhyAddr;
        unsigned char* inputVirAddr;
        int width;
        int height;

        InputFormat format;

        /* coding quality - range from (1 - 10) */
        int qLvl;

        /* insert thumbnail or not */
        int doThumbNail;
        /* dimension parameter for thumbnail */
        int thumbWidth;
        int thumbHeight;
        /* the coding quality of thumbnail */
        int thumbQLvl;

        void *exifInfo;
        void *gpsInfo;
    } EncInInfo;

    typedef struct {
        /* output buffer information */
        int outputPhyAddr;
        unsigned char *outputVirAddr;
        int outBufLen;

        /* output packet hander */
        OutputPacket_t outPkt;
    } EncOutInfo;

    bool prepareEncoder();
    void flushBuffer();

    void updateEncodeQuality(int quant);
    bool updateEncodeCfg(int width, int height,
                         InputFormat fmt = INPUT_FMT_YUV420SP, int qLvl = 8,
                         int wstride = 0, int hstride = 0);

    /*
     * output packet buffers within limits, so release packet buffer if one
     * packet has been display successful.
     */
    void deinitOutputPacket(OutputPacket_t *aPktOut);

    bool encodeFrame(char *data, OutputPacket_t *aPktOut);
    bool encodeFile(const char *inputFile, const char *outputFile);

    /*
     * designed for Rockchip cameraHal, commit input\output fd for encoder
     *
     * param[in] aInfoIn - pointer to input buffer parameter for encoder
     * param[in/out] aInfoOut - pointer to output buffer parameter for encoder
     */
    bool encode(EncInInfo *aInInfo, EncOutInfo *aOutInfo);

private:
    MppCtx          mMppCtx;
    MppApi         *mMpi;

    int             mInitOK;
    uint32_t        mFrameCount;

    /*
     * Format of the raw input data for encode
     */
    int             mWidth;
    int             mHeight;
    int             mHorStride;
    int             mVerStride;
    InputFormat     mFmt;

     /* coding quality - range from (1 - 10) */
    int             mEncodeQuality;

    MppBufferGroup  mMemGroup;

    /*
     * output packet list
     *
     * Note: output packet buffers within limits, so wo need to release output
     * buffer as soon as we process everything
     */
    QList          *mPackets;

    /* dump input & output for debug */
    FILE           *mInputFile;
    FILE           *mOutputFile;

    int getFrameSize(InputFormat fmt, int width, int height);

    MPP_RET runFrameEnc(MppFrame inFrm, MppPacket outPkt);

    MPP_RET cropThumbImage(EncInInfo *aInfoIn, void* outAddr);

    /* encode raw image by commit input fd to the encoder */
    bool encodeImageFD(EncInInfo *aInfoIn, EncOutInfo *aInfoOut);

    /* thumbNail encode for a large resolution input image */
    bool encodeThumb(EncInInfo *aInfoIn, uint8_t **data, int *len);
};

#endif  // __MPI_JPEG_ENCODER_H__
