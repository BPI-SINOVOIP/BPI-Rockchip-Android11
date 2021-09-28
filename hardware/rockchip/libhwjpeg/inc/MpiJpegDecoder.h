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

#ifndef __MPI_JPEG_DECODER_H__
#define __MPI_JPEG_DECODER_H__

#include "rk_mpi.h"

class QList;

class MpiJpegDecoder {
public:
    MpiJpegDecoder();
    ~MpiJpegDecoder();

    typedef struct {
        /**************** Decode-Input  *****************/
        /*
         * Output address memory allocated by mpp-decoder default. You can
         * designated the "outputPhyAddr" to specifies the decoded output
         * address.
         */
        uint32_t   outputPhyAddr;

        /**************** Decode-Ouput  *****************/
        /*
         * NOTE: Since the output frame buffer send to vpu is aligned, it is
         * neccessary to crop output with JPEG image dimens.
         *
         * vpu frame buffer width: FrameWidth
         * actual image buffer width: DisplayWidth
         */
        uint32_t   FrameWidth;         // buffer horizontal stride
        uint32_t   FrameHeight;        // buffer vertical stride
        uint32_t   DisplayWidth;       // valid width for display
        uint32_t   DisplayHeight;      // valid height for display

        uint32_t   ErrorInfo;          // error information
        uint32_t   OutputSize;

        uint8_t   *MemVirAddr;
        uint32_t   MemPhyAddr;

        void      *FrameHandler;       // MppFrame handler
    } OutputFrame_t;

    bool prepareDecoder();
    void flushBuffer();

    /**
     * Output frame buffers within limits, so release frame buffer if one
     * frame has been display successful.
     */
    void deinitOutputFrame(OutputFrame_t *aframeOut);

    MPP_RET decode_sendpacket(char* input_buf, size_t buf_len,
                              uint32_t outPhyAddr = 0);
    MPP_RET decode_getoutframe(OutputFrame_t *aframeOut);

    bool decodePacket(char* data, size_t size, OutputFrame_t *aframeOut);
    bool decodeFile(const char *input_file, const char *output_file);

private:
    typedef enum {
        OUT_FORMAT_YUV420SP =  MPP_FMT_YUV420SP,
        OUT_FORMAT_ARGB     =  MPP_FMT_ARGB8888,
    } OutputFormat;

    MppCtx          mMppCtx;
    MppApi          *mMpi;

    int             mInitOK;
    bool            mFdOutput;
    bool            mOutputCrop;
    uint32_t        mDecWidth;
    uint32_t        mDecHeight;
    // bit per pixel
    float           mBpp;
    int             mOutputFmt;
    uint32_t        mPacketCount;

    QList           *mPackets;
    QList           *mFrames;

    /*
     * packet buffer group
     *      - packets in I/O, can be ion buffer or normal buffer
     * frame buffer group
     *      - frames in I/O, normally should be a ion buffer group
     */
    MppBufferGroup  mPacketGroup;
    MppBufferGroup  mFrameGroup;

    /* Dump input & output for debug */
    FILE            *mInputFile;
    FILE            *mOutputFile;

    bool reinitMppDecoder();

    void setup_output_frame_from_mpp_frame(OutputFrame_t *oframe, MppFrame mframe);
    MPP_RET crop_output_frame_if_neccessary(OutputFrame_t *oframe);
};

#endif  // __MPI_JPEG_DECODER_H__
