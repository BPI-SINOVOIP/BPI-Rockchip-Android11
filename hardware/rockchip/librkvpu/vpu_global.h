/*
 *
 * Copyright 2010 Rockchip Electronics S.LSI Co. LTD
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
/*
   * File:
   * vpu_global.h
   * Description:
   * Global struct definition in VPU module
   * Author:
   *     Jian Huan
   * Date:
   *    2010-11-23 21:48:40
 */
#ifndef _VPU_GLOBAL_
#define _VPU_GLOBAL_

#include "vpu_macro.h"
#include "vpu_mem.h"

typedef struct
{
    RK_U32   TimeLow;
    RK_U32   TimeHigh;
}TIME_STAMP;

typedef struct
{
    RK_U32   CodecType;
    RK_U32   ImgWidth;      //非16X必须，从文件中解析
    RK_U32   ImgHeight;      //非16X必须，从文件中解析
}VPU_GENERIC;

typedef enum DEC_TYPE_T {
   HW,
   SW,
}DEC_TYPE;

typedef struct
{
    RK_U32      StartCode;
    RK_U32      SliceLength;
    TIME_STAMP  SliceTime;
    RK_U32      SliceType;
    RK_U32      SliceNum;
    RK_U32      Res[2];
}VPU_BITSTREAM;  /*completely same as RK28*/

typedef struct
{
    RK_U32      InputAddr[2];
    RK_U32      OutputAddr[2];
    RK_U32      InputWidth;
    RK_U32      InputHeight;
    RK_U32      OutputWidth;
    RK_U32      OutputHeight;
    RK_U32      ColorType;
    RK_U32      ScaleEn;
    RK_U32      RotateEn;
    RK_U32      DitherEn;
    RK_U32      DeblkEn;
    RK_U32      DeinterlaceEn;
    RK_U32      Res[5];
}VPU_POSTPROCESSING;

typedef struct tVPU_FRAME
{
    RK_U32          FrameBusAddr[2];       //0: Y address; 1: UV address;
    RK_U32          FrameWidth;         //16X对齐宽度
    RK_U32          FrameHeight;        //16X对齐高度
    RK_U32          OutputWidth;        //非16X必须
    RK_U32          OutputHeight;       //非16X必须
    RK_U32          DisplayWidth;       //显示宽度
    RK_U32          DisplayHeight;      //显示高度
    RK_U32          CodingType;
    RK_U32          FrameType;          //frame;top_field_first;bot_field_first
    RK_U32          ColorType;
    RK_U32          DecodeFrmNum;
    TIME_STAMP      ShowTime;
    RK_U32          ErrorInfo;          //该帧的错误信息，返回给系统方便调试
    RK_U32          employ_cnt;
    VPUMemLinear_t  vpumem;
    struct tVPU_FRAME *    next_frame;

    union {
        struct {
            RK_U32      Res0[2];
            struct {
                RK_U32      ColorPrimaries : 8;
                RK_U32      ColorTransfer  : 8;
                RK_U32      ColorCoeffs    : 8;
                RK_U32      ColorRange     : 1;
                RK_U32      Res1           : 7;
            };
            RK_U32      Res2;
        };

        RK_U32          Res[4];
    };
}VPU_FRAME;

typedef enum VPU_API_CMD
{
   VPU_API_ENC_SETCFG = 0,
   VPU_API_ENC_GETCFG = 1,
   VPU_API_ENC_SETFORMAT = 2,
   VPU_API_ENC_SETIDRFRAME = 3,
   VPU_API_ENABLE_DEINTERLACE = 4,
   VPU_API_SET_VPUMEM_CONTEXT = 5,
   VPU_API_USE_PRESENT_TIME_ORDER = 6,
   VPU_API_SET_DEFAULT_WIDTH_HEIGH = 7,
   VPU_API_SET_INFO_CHANGE = 8,
   VPU_API_USE_FAST_MODE = 9,
   VPU_API_DEC_GET_PACKETS_STORED = 10,
   VPU_API_DEC_GET_STREAM_COUNT = 11,
   VPU_API_SET_OUTPUT_MODE = 15,
   VPU_API_DEC_GET_DPB_SIZE = 0X100,
   VPU_API_SET_IMMEDIATE_OUT = 0x1000,
   VPU_API_SET_CODEC_TAG = 0x1001,
   VPU_API_DEC_GET_STREAM_TOTAL = 0x2000,
   VPU_API_SET_SECURE_CONTEXT,
}VPU_API_CMD;


#endif /*_VPU_GLOBAL_*/
