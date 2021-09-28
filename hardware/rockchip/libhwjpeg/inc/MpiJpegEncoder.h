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
        INPUT_FMT_YUV420SP      = MPP_FMT_YUV420SP,
        INPUT_FMT_YUV420P       = MPP_FMT_YUV420P,
        INPUT_FMT_YUV422SP_VU   = MPP_FMT_YUV422SP_VU,
        INPUT_FMT_YUV422_YUYV   = MPP_FMT_YUV422_YUYV,
        INPUT_FMT_YUV422_UYVY   = MPP_FMT_YUV422_UYVY,
        INPUT_FMT_ARGB8888      = MPP_FMT_ARGB8888,
        INPUT_FMT_RGBA8888      = MPP_FMT_RGBA8888,
        INPUT_FMT_ABGR8888      = MPP_FMT_ABGR8888
    } InputFormat;

    typedef struct {
        uint8_t *data;
        int size;
        /* Output packet hander */
        void *packetHandler;
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
    } EncInInfo;

    bool prepareEncoder();
    void flushBuffer();
    bool updateEncodeCfg(int width, int height,
                         InputFormat fmt = INPUT_FMT_YUV420SP, int qLvl = 8);

    /*
     * Output packet buffers within limits, so release packet buffer if one
     * packet has been display successful.
     */
    void deinitOutputPacket(OutputPacket_t *aPktOut);

    bool encodeFrame(char *data, OutputPacket_t *aPktOut);
    bool encodeFile(const char *input_file, const char *output_file);


    /*
     * Encode raw image by commit input fd to the encoder.
     *
     * param[in] aInfoIn    - input parameter for picture encode
     * param[in] dst_offset - output buffer offset, equals to jpeg header_len
     * param[out] aPktOut   - pointer to buffer pointer containing output data.
     */
    bool encodeImageFD(EncInInfo *aInfoIn,
                       int dst_offset, OutputPacket_t *aPktOut);

    /*
     * ThumbNail encode for a large resolution input image.
     *
     * param[in] aInfoIn   - input parameter for thumnNail encode
     * param[out] outPkt   - pointer to buffer pointer containing output data.
     */
    bool encodeThumb(EncInInfo *aInfoIn, uint8_t **data, int *len);

    /* designed for Rockchip cameraHal, commit input fd for encode */
    bool encode(EncInInfo *inInfo, OutputPacket_t *outPkt);

private:
    MppCtx          mMppCtx;
    MppApi          *mMpi;

    int             mInitOK;
    uint32_t        mFrameCount;

    /*
     * Format of the raw input data for encode
     */
    int             mInputWidth;
    int             mInputHeight;
    InputFormat     mInputFmt;

     /* coding quality - range from (1 - 10) */
    int             mEncodeQuality;

    MppBufferGroup  mMemGroup;

    /*
     * output packet list
     *
     * Note: Output packet buffers within limits, so wo need to release output
     * buffer as soon as we process everything
     */
    QList           *mPackets;

    /* Dump input & output for debug */
    FILE            *mInputFile;
    FILE            *mOutputFile;

    void updateEncodeQuality(int quant);
    MPP_RET cropInputYUVImage(EncInInfo *aInfoIn, void* outAddr);

    MPP_RET runFrameEnc(MppFrame in_frame, MppPacket out_packet);
};

#endif  // __MPI_JPEG_ENCODER_H__
