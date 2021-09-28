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
   * ppOp.h
   * Description:
   * Global struct definition in VPU module
   * Author:
   *     Wu JunMin
   * Date:
   *    2015-03-31 14:43:40
 */
#define LOG_TAG "PpOp"
#define ATRACE_TAG ATRACE_TAG_VIDEO
#include <utils/Log.h>
#ifndef AVS40
    #include <utils/Trace.h>
#endif
#include "vpu.h"
#include "vpu_mem.h"
#include "ppOp.h"
#include "reg.h"
#include <stdlib.h>
#ifdef AVS40
#undef ALOGV
#define ALOGV LOGV
#undef ALOGD
#define ALOGD LOGD
#undef ALOGI
#define ALOGI LOGI
#undef ALOGW
#define ALOGW LOGW
#undef ALOGE
#define ALOGE LOGE
#endif

namespace android {

#define PPOP_DEBUG      0

class VPUReg {
public:
    VPUReg(VPU_CLIENT_TYPE type);
    ~VPUReg();
    void    SetRegisterFile(RK_U32 id, RK_U32 value);
    RK_U32  GetRegisterFile(RK_U32 id);
    RK_U32 *addr() {return ptr;};
private:
    VPU_CLIENT_TYPE vpuType;
    RK_U32  size;
    RK_U32 *ptr;
    RK_U32 *start;
};

VPUReg::VPUReg(VPU_CLIENT_TYPE type)
    : vpuType(VPU_TYPE_BUTT), ptr(NULL), start(NULL)
{
    switch (type) {
    case VPU_ENC : {
        size = sizeof(RK_U32) * VPU_REG_NUM_ENC;
        ptr  = (RK_U32 *)malloc(size);
    } break;
    case VPU_DEC : {
        size = sizeof(RK_U32) * VPU_REG_NUM_DEC;
        ptr = (RK_U32 *)malloc(size);
    } break;
    case VPU_PP : {
        size = sizeof(RK_U32) * VPU_REG_NUM_PP;
        ptr = (RK_U32 *)malloc(size);
    } break;
    case VPU_DEC_PP : {
        size = sizeof(RK_U32) * VPU_REG_NUM_DEC_PP;
        ptr = (RK_U32 *)malloc(size);
    } break;
    default : {
        ALOGE("invalid vpu client type: %d", type);
    } break;
    }
    if (NULL != ptr) {
        vpuType = type;
        if (VPU_PP == vpuType) {
            start = ptr;
        } else {
            start = ptr + VPU_REG_NUM_PP;
        }
        memset(ptr, 0, size);
    } else {
        ALOGE("failed to malloc resource for VPUReg");
        size = 0;
    }
}

typedef struct {
    uint32_t srcAddr;           // 16 align
    uint32_t srcFormat;
    uint32_t srcWidth;          // 16 align max 2048
    uint32_t srcHeight;         // 16 align max 2048
    uint32_t srcHStride;        // 16 align max 2048
    uint32_t srcVStride;        // 16 align max 2048
    uint32_t srcCrop8R;         // crop rigth
    uint32_t srcCrop8D;         // crop down      
    uint32_t srcX;                  // src x
    uint32_t srcY;                  // src y        
    uint32_t srcReserv[2];

    uint32_t dstAddr;           // 16 align
    uint32_t dstFormat;
    uint32_t dstWidth;          // 16 align max 2048
    uint32_t dstHeight;         // 16 align max 2048
    uint32_t dstHStride;        // 16 align max 2048
    uint32_t dstVStride;        // 16 align max 2048
    uint32_t dstReserv[2];
    uint32_t dstX;                  // dst x
    uint32_t dstY;                  // dst y
    
    uint32_t vpuFd;
    uint32_t rotation;          // rotation angel
    uint32_t yuvFullRange;      // yuv is full range or not, set yuv trans table
    uint32_t deinterlace;       // do deinterlace or not
    uint32_t optReserv[9];
    uint32_t updated;           // update flag
    VPUReg  *reg;               // register set
    uint32_t *checkSum;
} PP_INTERNAL;

VPUReg::~VPUReg()
{
    vpuType = VPU_TYPE_BUTT;
    if (NULL != ptr) {
        free(ptr);
    }
    ptr   = NULL;
    start = NULL;
}

static const RK_U32 regMask[33] = {		0x00000000,
    0x00000001, 0x00000003, 0x00000007, 0x0000000F,
    0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
    0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
    0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
    0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
    0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
    0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
    0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
};

/* { SWREG, BITS, POSITION } */
static RK_U32 hwDecRegSpec[HWIF_LAST_REG + 1][3] = {
	/* include script-generated part */
	#include "vpuhwtable.h"
	/* HWIF_DEC_IRQ_STAT */ {1,  7, 12},
	/* HWIF_PP_IRQ_STAT */ 	{60, 2, 12},
	/* dummy entry */ 		{ 0, 0,  0}
};

void VPUReg::SetRegisterFile(RK_U32 id, RK_U32 value)
{
    if (vpuType >= VPU_TYPE_BUTT) {
        ALOGE("failed to SetRegisterFile to invalid VPUReg");
        return ;
    }
    RK_U32 tmp = start[hwDecRegSpec[id][0]];
    tmp &= ~(regMask[hwDecRegSpec[id][1]] << hwDecRegSpec[id][2]);
    tmp |= (value & regMask[hwDecRegSpec[id][1]]) << hwDecRegSpec[id][2];
    start[hwDecRegSpec[id][0]] = tmp;
}

RK_U32 VPUReg::GetRegisterFile(RK_U32 id)
{
    if (vpuType >= VPU_TYPE_BUTT) {
        ALOGE("failed to GetRegisterFile from invalid VPUReg");
        return 0;
    }
    RK_U32 tmp = start[hwDecRegSpec[id][0]];
    tmp = tmp >> hwDecRegSpec[id][2];
    tmp &= regMask[hwDecRegSpec[id][1]];
    return (tmp);
}

#define SetPpRegister(reg, id, value) do {reg->SetRegisterFile(id, value);} while (0)

static status_t ppSetSrcFormat(VPUReg *reg, uint32_t srcFormat)
{
    switch (srcFormat) {
        case PP_IN_FORMAT_YUV422INTERLAVE:
            SetPpRegister(reg, HWIF_PP_IN_FORMAT, 0);
            break;
        case PP_IN_FORMAT_YUV420SEMI:
            SetPpRegister(reg, HWIF_PP_IN_FORMAT, 1);
            break;
        case PP_IN_FORMAT_YUV420PLANAR:
            SetPpRegister(reg, HWIF_PP_IN_FORMAT, 2);
            break;
        case PP_IN_FORMAT_YUV400:
            SetPpRegister(reg, HWIF_PP_IN_FORMAT, 3);
            break;
        case PP_IN_FORMAT_YUV422SEMI:
            SetPpRegister(reg, HWIF_PP_IN_FORMAT, 4);
            break;
        case PP_IN_FORMAT_YUV420SEMITIELED:
            SetPpRegister(reg, HWIF_PP_IN_FORMAT, 5);
            break;
        case PP_IN_FORMAT_YUV440SEMI:
            SetPpRegister(reg, HWIF_PP_IN_FORMAT, 6);
            break;
        case PP_IN_FORMAT_YUV444_SEMI:
            SetPpRegister(reg, HWIF_PP_IN_FORMAT, 7);
            SetPpRegister(reg, HWIF_PP_IN_FORMAT_ES, 0);
            break;
        case PP_IN_FORMAT_YUV411_SEMI:
            SetPpRegister(reg, HWIF_PP_IN_FORMAT, 7);
            SetPpRegister(reg, HWIF_PP_IN_FORMAT_ES, 1);
            break;
        default:
            return BAD_VALUE;
    }
    return OK;
}

status_t ppSetDstFormat(VPUReg *reg, uint32_t dstFormat)
{
    switch (dstFormat) {
        case PP_OUT_FORMAT_RGB565:
            SetPpRegister(reg, HWIF_R_MASK, 0xF800F800);
            SetPpRegister(reg, HWIF_G_MASK, 0x07E007E0);
            SetPpRegister(reg, HWIF_B_MASK, 0x001F001F);

            SetPpRegister(reg, HWIF_RGB_R_PADD, 0);
            SetPpRegister(reg, HWIF_RGB_G_PADD, 5);
            SetPpRegister(reg, HWIF_RGB_B_PADD, 11);
            SetPpRegister(reg, HWIF_DITHER_SELECT_R, 2);
            SetPpRegister(reg, HWIF_DITHER_SELECT_G, 3);
            SetPpRegister(reg, HWIF_DITHER_SELECT_B, 2);
            SetPpRegister(reg, HWIF_RGB_PIX_IN32, 1);
            SetPpRegister(reg, HWIF_PP_OUT_SWAP16_E,1);
            SetPpRegister(reg, HWIF_PP_OUT_FORMAT, 0);
            break;
        case PP_OUT_FORMAT_ARGB:
            SetPpRegister(reg, HWIF_R_MASK, 0x000000FF | (0xff << 24));
            SetPpRegister(reg, HWIF_G_MASK, 0x0000FF00 | (0xff << 24));
            SetPpRegister(reg, HWIF_B_MASK, 0x00FF0000 | (0xff << 24));
            SetPpRegister(reg, HWIF_RGB_R_PADD, 24);
            SetPpRegister(reg, HWIF_RGB_G_PADD, 16);
            SetPpRegister(reg, HWIF_RGB_B_PADD, 8);

            SetPpRegister(reg, HWIF_RGB_PIX_IN32, 0);
            SetPpRegister(reg, HWIF_PP_OUT_FORMAT, 0);
            break;
        case PP_OUT_FORMAT_ABGR:
            SetPpRegister(reg, HWIF_B_MASK, 0x000000FF | (0xff << 24));
            SetPpRegister(reg, HWIF_G_MASK, 0x0000FF00 | (0xff << 24));
            SetPpRegister(reg, HWIF_R_MASK, 0x00FF0000 | (0xff << 24));
            SetPpRegister(reg, HWIF_RGB_B_PADD, 24);
            SetPpRegister(reg, HWIF_RGB_G_PADD, 16);
            SetPpRegister(reg, HWIF_RGB_R_PADD, 8);

            SetPpRegister(reg, HWIF_RGB_PIX_IN32, 0);
            SetPpRegister(reg, HWIF_PP_OUT_FORMAT, 0);
            break;
        case PP_OUT_FORMAT_YUV422INTERLAVE:
            SetPpRegister(reg, HWIF_PP_OUT_FORMAT, 3);
            break;
        case PP_OUT_FORMAT_YUV420INTERLAVE:
            SetPpRegister(reg, HWIF_PP_OUT_CH_BASE, 0);
            SetPpRegister(reg, HWIF_PP_OUT_FORMAT, 5);
            break;
        default:
            return BAD_VALUE;
    }
    return OK;
}

status_t ppOpUpdate(PP_INTERNAL *hnd)
{
    PP_INTERNAL *p = hnd;
    VPUReg *reg = p->reg;
    int     srcwid_alig16;
    int     srchei_alig16;   
    if ( p->optReserv[0] )
    {
        p->srcWidth = p->srcWidth&(~15);
        p->srcHeight = p->srcHeight&(~15);
    }
    srcwid_alig16 = (p->srcWidth+15)&(~15);
    srchei_alig16 = (p->srcHeight+15)&(~15);        
    if (p->updated) {
        //get srcCrop para
        if(srcwid_alig16 != p->srcWidth)
            p->srcCrop8R = 1;
        else
            p->srcCrop8R = 0;
        if(srchei_alig16 != p->srcHeight)
            p->srcCrop8D= 1;
        else
            p->srcCrop8D= 0;
        //update dstX and dstWid
        p->dstWidth = (p->dstWidth + (p->dstX&7) + 7)&(~7);
        p->dstX     = p->dstX&(~7);   
        
        SetPpRegister(reg, HWIF_PP_AXI_RD_ID, 0xFF);//0xFF);
    	SetPpRegister(reg, HWIF_PP_AXI_WR_ID, 0xFF);//0xFF);
    	SetPpRegister(reg, HWIF_PP_AHB_HLOCK_E, 1);
    	SetPpRegister(reg, HWIF_PP_SCMD_DIS, 1);
    	SetPpRegister(reg, HWIF_PP_IN_A2_ENDSEL, 1);
    	SetPpRegister(reg, HWIF_PP_IN_A1_SWAP32, 1);
    	SetPpRegister(reg, HWIF_PP_IN_A1_ENDIAN, 1);
    	SetPpRegister(reg, HWIF_PP_IN_SWAP32_E, 1);
    	SetPpRegister(reg, HWIF_PP_DATA_DISC_E, 1);
    	SetPpRegister(reg, HWIF_PP_CLK_GATE_E, 1);
    	SetPpRegister(reg, HWIF_PP_IN_ENDIAN, 1);
    	SetPpRegister(reg, HWIF_PP_OUT_ENDIAN, 1);
    	SetPpRegister(reg, HWIF_PP_OUT_SWAP32_E, 1);
    	SetPpRegister(reg, HWIF_PP_MAX_BURST, 16);

        SetPpRegister(reg, HWIF_EXT_ORIG_WIDTH, p->srcHStride>>4);

    	SetPpRegister(reg, HWIF_PP_IN_W_EXT, (((srcwid_alig16 / 16) & 0xE00) >> 9));
    	SetPpRegister(reg, HWIF_PP_IN_WIDTH,  ((srcwid_alig16 / 16) & 0x1FF));
    	SetPpRegister(reg, HWIF_PP_IN_H_EXT, (((srchei_alig16 / 16) & 0x700) >> 8));
    	SetPpRegister(reg, HWIF_PP_IN_HEIGHT, ((srchei_alig16 / 16) & 0x0FF));
    	SetPpRegister(reg, HWIF_DISPLAY_WIDTH, p->dstHStride);

    	SetPpRegister(reg, HWIF_PP_OUT_WIDTH,  p->dstWidth);
    	SetPpRegister(reg, HWIF_PP_OUT_HEIGHT, p->dstHeight);
        SetPpRegister(reg, HWIF_PP_IN_STRUCT, p->deinterlace?3:0);

        SetPpRegister(reg, HWIF_DEINT_E, p->deinterlace);

        SetPpRegister(reg, HWIF_ROTATION_MODE, p->rotation);
        
        SetPpRegister(reg, HWIF_DEINT_BLEND_E, 0);

        SetPpRegister(reg, HWIF_DEINT_THRESHOLD, 256);

        SetPpRegister(reg, HWIF_DEINT_EDGE_DET, 256);

        SetPpRegister(reg, HWIF_RANGEMAP_COEF_Y, 9);
    	SetPpRegister(reg, HWIF_RANGEMAP_COEF_C, 9);

        /*{
            uint32_t srcY  = p->srcAddr;
            uint32_t srcCB = srcY  + p->srcHStride * p->srcVStride;
            uint32_t srcCR = srcCB + p->srcWidth * p->srcHeight / 4;
            uint32_t bottomBusLuma = srcY + p->srcHStride;
    	    uint32_t bottomBusChroma = srcCB + p->srcHStride;

            SetPpRegister(reg, HWIF_PP_IN_LU_BASE, srcY);
            SetPpRegister(reg, HWIF_PP_IN_CB_BASE, srcCB);
            SetPpRegister(reg, HWIF_PP_IN_CR_BASE, srcCR);

            SetPpRegister(reg, HWIF_PP_BOT_YIN_BASE, bottomBusLuma);
            SetPpRegister(reg, HWIF_PP_BOT_CIN_BASE, bottomBusChroma);

            SetPpRegister(reg, HWIF_PP_OUT_LU_BASE, p->dstAddr);
            SetPpRegister(reg, HWIF_PP_OUT_CH_BASE, p->dstAddr + p->dstHStride * p->dstVStride);
        }*/
		{
            uint32_t srcY;
            uint32_t srcCB;
            uint32_t srcCR;
            uint32_t bottomBusLuma;
            uint32_t bottomBusChroma;
            uint32_t dstLum;
            uint32_t dstChr;

            if (!VPUMemJudgeIommu()) {
                srcY  = p->srcAddr + (p->srcX + p->srcY * p->srcHStride);
                srcCB = srcY  + p->srcHStride * p->srcVStride;
                srcCR = srcCB + p->srcHStride * p->srcVStride / 4;
                bottomBusLuma = srcY + p->srcHStride;
                bottomBusChroma = srcCB + p->srcHStride;
                
                dstLum = p->dstAddr + (p->dstX + p->dstY * p->dstHStride);
                dstChr = p->dstAddr + p->dstHStride * p->dstVStride + (p->dstX + p->dstY * p->dstHStride / 2);
            } else {
                srcY  = p->srcAddr | ((p->srcX + p->srcY * p->srcHStride)<<10);
                srcCB = srcY + ((p->srcHStride * p->srcVStride)<<10);
                srcCR = srcCB + ((p->srcHStride * p->srcVStride / 4)<<10);
                bottomBusLuma = srcY + (p->srcHStride << 10);
                bottomBusChroma = srcCB + (p->srcHStride << 10);
                
                dstLum = p->dstAddr | ((p->dstX + p->dstY * p->dstHStride)<<10);
                dstChr = p->dstAddr | ((p->dstHStride * p->dstVStride + (p->dstX + p->dstY * p->dstHStride / 2))<<10);
            }

            SetPpRegister(reg, HWIF_PP_IN_LU_BASE, srcY);
            SetPpRegister(reg, HWIF_PP_IN_CB_BASE, srcCB);
            SetPpRegister(reg, HWIF_PP_IN_CR_BASE, srcCR);

            SetPpRegister(reg, HWIF_PP_BOT_YIN_BASE, bottomBusLuma);
            SetPpRegister(reg, HWIF_PP_BOT_CIN_BASE, bottomBusChroma);

            SetPpRegister(reg, HWIF_PP_OUT_LU_BASE, dstLum);
            SetPpRegister(reg, HWIF_PP_OUT_CH_BASE, dstChr);
        }

        {
    		unsigned int		inw,inh;
    		unsigned int		outw,outh;

            if(p->rotation == PP_ROTATION_RIGHT_90 || p->rotation == PP_ROTATION_LEFT_90)
            {
        		outw = p->dstWidth - 1;//outWidth -1;
        		outh = p->dstHeight - 1;//outHeight -1;
        		inw = p->srcHeight - 1;//inWidth -1;
        		inh = p->srcWidth -1;//p->srcHeight - 1;//inHeigth -1;        		
            }else
            {
        		outw = p->dstWidth - 1;//outWidth -1;
        		outh = p->dstHeight - 1;//outHeight -1;        
        		inw = p->srcWidth - 1;//inWidth -1;
        		inh = p->srcHeight -1;//p->srcHeight - 1;//inHeigth -1;        		
            }
            if(outh > inh && p->srcCrop8D)
            {
                inh = inh&(~15);
            }
            if(outw > inw && p->srcCrop8R)
            {
                inw = inw&(~15);
            }
	    #if PPOP_DEBUG
            ALOGD("p->rotation=%d, outw=%d, outh=%d, p->srcCrop8D=%d, p->srcCrop8R=%d\n", p->rotation, outw, outh, p->srcCrop8D, p->srcCrop8R);
            #endif
            SetPpRegister(reg, HWIF_PP_CROP8_R_E, p->srcCrop8R);
            SetPpRegister(reg, HWIF_PP_CROP8_D_E, p->srcCrop8D);

    		if (inw < outw)
    		{
    			SetPpRegister(reg, HWIF_HOR_SCALE_MODE, 1);
    			SetPpRegister(reg, HWIF_SCALE_WRATIO, (outw<<16)/inw);
    			SetPpRegister(reg, HWIF_WSCALE_INVRA, (inw<<16)/outw);
    		}
    		else if (inw > outw)
    		{
    			SetPpRegister(reg, HWIF_HOR_SCALE_MODE, 2);
    			SetPpRegister(reg, HWIF_WSCALE_INVRA, ((outw+1)<<16)/(inw+1));
    		}
    		else
    			SetPpRegister(reg, HWIF_HOR_SCALE_MODE, 0);

    		if (inh < outh)
    		{
    			SetPpRegister(reg, HWIF_VER_SCALE_MODE, 1);
    			SetPpRegister(reg, HWIF_SCALE_HRATIO, (outh<<16)/inh);
    			SetPpRegister(reg, HWIF_HSCALE_INVRA, (inh<<16)/outh);
    		}
    		else if (inh > outh)
    		{
    			SetPpRegister(reg, HWIF_VER_SCALE_MODE, 2);
    			SetPpRegister(reg, HWIF_HSCALE_INVRA, ((outh+1)<<16)/(inh+1)+1);
    		}
    		else
    			SetPpRegister(reg, HWIF_VER_SCALE_MODE, 0);
    	}        
    }
	SetPpRegister(reg, HWIF_PP_E, 1);
    return OK;
}

status_t ppOpInit(PP_OP_HANDLE *hnd, PP_OPERATION *init)
{
    if (NULL == hnd || NULL == init) {
        ALOGE("invalid arg reg: %p init: %p", hnd, init);
        return BAD_VALUE;
    }
    VPUReg *reg = new VPUReg(VPU_PP);
    PP_INTERNAL *p = (PP_INTERNAL *)malloc(sizeof(PP_INTERNAL));
    if (NULL == p || NULL == reg) {
        ALOGE("can not malloc resource for pp operation");
        if (p) free(p);
        if (reg) delete(reg);
        return NO_MEMORY;
    }
    memcpy(p, init, sizeof(PP_INTERNAL));
#if PPOP_DEBUG    
    ALOGI("src: vw %d, vh %d, w: %d h %d x %d, y %d, format %d, addr 0x%x", init->srcHStride, init->srcVStride, init->srcWidth, init->srcHeight, init->srcX, init->srcY, init->srcFormat, init->srcAddr);
    ALOGI("dst: vw %d, vh %d, w: %d h %d x %d, y %d, format %d, addr 0x%x", init->dstHStride, init->dstVStride, init->dstWidth, init->dstHeight, init->dstX, init->dstY, init->dstFormat, init->dstAddr);
#endif    
    if (ppSetSrcFormat(reg, init->srcFormat) || ppSetDstFormat(reg, init->dstFormat)) {
        ALOGE("invalid format src: %d, dst %d", init->srcFormat, init->dstFormat);
        if (p) free(p);
        if (reg) free(reg);
        return BAD_VALUE;
    }

    {
		unsigned int		inw,inh;
		unsigned int		outw,outh;

        if(init->rotation == PP_ROTATION_RIGHT_90 || init->rotation == PP_ROTATION_LEFT_90)
        {
    		outw = init->dstWidth - 1;//outWidth -1;
    		outh = init->dstHeight - 1;//outHeight -1;
    		inw = init->srcHeight - 1;//inWidth -1;
    		inh = init->srcWidth -1;//p->srcHeight - 1;//inHeigth -1;        		
        }else
        {
    		outw = init->dstWidth - 1;//outWidth -1;
    		outh = init->dstHeight - 1;//outHeight -1;        
    		inw = init->srcWidth - 1;//inWidth -1;
    		inh = init->srcHeight -1;//p->srcHeight - 1;//inHeigth -1;        		
        }
        if((outw > inw && outh < inh) || (outw < inw && outh > inh))
        {
            ALOGD("PP operat error width and height scale mode is different!");
            return BAD_VALUE;
        }
        if((outw > 3*inw) || (outh > 3*(inh-2)))
        {
            ALOGD("PP operat error because scale rate more than 3x!");
            return BAD_VALUE;
        }       
        if( outw > inw && outh > inh)
            p->optReserv[0] = 1;
        else
            p->optReserv[0] = 0;	
    }

    //ALOGI("ppSetSrcFormat");
    p->updated = 1;
    p->reg = reg;
    p->checkSum = (uint32_t*)p;
    *hnd = (PP_OP_HANDLE)p;
    ppOpUpdate(p);
    return OK;
}

static status_t ppOptCheck(PP_SET_OPT opt)
{
    status_t ret = BAD_VALUE;
    switch (opt) {
    case PP_SET_SRC_ADDR:
    case PP_SET_SRC_FORMAT:
    case PP_SET_SRC_WIDTH:
    case PP_SET_SRC_HEIGHT:
    case PP_SET_SRC_HSTRIDE:
    case PP_SET_SRC_VSTRIDE:
    case PP_SET_DST_ADDR:
    case PP_SET_DST_FORMAT:
    case PP_SET_DST_WIDTH:
    case PP_SET_DST_HEIGHT:
    case PP_SET_DST_HSTRIDE:
    case PP_SET_DST_VSTRIDE:
    case PP_SET_ROTATION:
    case PP_SET_YUV_RANGE:
    case PP_SET_DEINTERLACE:
    case PP_SET_VPU_FD: {
        ret = OK;
    } break;
    default : {
    } break;
    }
    return ret;
}

status_t ppOpSet(PP_OP_HANDLE hnd, PP_SET_OPT opt, uint32_t val)
{
    if (NULL == hnd || ppOptCheck(opt)) {
        ALOGE("invalid arg hnd: %p opt: %d", hnd, opt);
        return BAD_VALUE;
    }
    if (((uint32_t *)hnd)[opt] != val) {
        ((uint32_t *)hnd)[opt] = val;
        ((PP_INTERNAL *)hnd)->updated = 1;
    }
    return OK;
}

static status_t ppHnadleCheck(PP_INTERNAL *p)
{
    if (NULL == p) {
        ALOGE("invalid NULL hnd");
        return BAD_VALUE;
    }
    if (p->checkSum != (uint32_t*)p) {
        ALOGE("invalid hnd: %p checkSum 0x%.8x", p, p->checkSum);
        return BAD_VALUE;
    }
    if (p->vpuFd <= 0) {
        ALOGE("invalid vpu client handle: %d", p->vpuFd);
        return BAD_VALUE;
    }
    if (NULL == p->reg) {
        ALOGE("invalid register set: %p", p->reg);
        return BAD_VALUE;
    }
    return OK;
}

status_t ppOpPerform(PP_OP_HANDLE hnd)
{
    PP_INTERNAL *p = (PP_INTERNAL *)hnd;
    status_t ret = ppHnadleCheck(p);
    if (ret) return ret;
    ppOpUpdate(p);
#if PPOP_DEBUG
    RK_U32 *ptr = p->reg->addr();
    for (int i=0; i < VPU_REG_NUM_PP; i++) {
        ALOGE("send reg[%.2d] val: 0x%.8x", i, ptr[i]);
    }
#endif
    {
    RK_U32 *ptr = p->reg->addr();
    printf("send reg[41] val: 0x%.8x\n", ptr[41]);
    }
    return VPUClientSendReg(p->vpuFd, p->reg->addr(), VPU_REG_NUM_PP);//VPU_REG_NUM_PP);
}

status_t ppOpSync(PP_OP_HANDLE hnd)
{
    PP_INTERNAL *p = (PP_INTERNAL *)hnd;
    status_t ret = ppHnadleCheck(p);
    if (ret) return ret;
    VPU_CMD_TYPE cmd;
    RK_S32 len;
    ret = VPUClientWaitResult(p->vpuFd, p->reg->addr(), VPU_REG_NUM_PP, &cmd, &len);
#if PPOP_DEBUG
    RK_U32 *ptr = p->reg->addr();
    for (int i=0; i < VPU_REG_NUM_PP; i++) {
        ALOGE("recv reg[%.2d] val: 0x%.8x", i, ptr[i]);
    }
#endif
    if (ret) return ret;
    if (cmd != VPU_SEND_CONFIG_ACK_OK) return BAD_VALUE;
    return OK;
}

status_t ppOpRelease(PP_OP_HANDLE hnd)
{
    PP_INTERNAL *p = (PP_INTERNAL *)hnd;
    status_t ret = ppHnadleCheck(p);
    if (ret) return ret;
    if (p->reg) delete(p->reg);
    free(p);
    return OK;
}

}

#if BUILD_PPOP_TEST
#define SRC_WIDTH           (1920)//(1920)
#define SRC_HEIGHT          (1080)//(1080)
#define SRC_SIZE            (SRC_WIDTH*SRC_HEIGHT*2)

#define DST_WIDTH           (720)//(720)
#define DST_HEIGHT          (1280)
#define DST_SIZE            (DST_WIDTH*DST_HEIGHT*2)

#include "vpu_mem.h"
#include "vpu.h"
#include "ppOp.h"

int main()
{
    FILE *fpr, *fpw;
    int   wid_alig16 = ((SRC_WIDTH+15)&(~0xf));
    int   hei_alig16 = ((SRC_HEIGHT+15)&(~0xf));
    int   src_vir_width = 1920;//2048;    
    int   src_vir_height = 1088;//1088;    
    int   dst_vir_width = 800;//800;    
    int   dst_vir_height = 1280;//1280;       
    int   framecnt = 0;
    char *tmpbuf = (char *)malloc(src_vir_width*src_vir_height/2);
    RK_S32 ret = 0, i,  j;
    
    ALOGI("ppOp test start\n");
    VPUMemLinear_t src, dst;
    android::PP_OP_HANDLE hnd;
    int vpuFd = VPUClientInit(VPU_PP);
    ret |= VPUMallocLinear(&src, src_vir_width*src_vir_height*2);//SRC_SIZE);
    ret |= VPUMallocLinear(&dst, dst_vir_width*dst_vir_height*2);//DST_SIZE);
    if (ret) {
        ALOGE("failed to malloc vpu_mem");
        goto end;
    }
    if (vpuFd < 0) {
        ALOGE("failed to open vpu client");
        goto end;
    }

    {
        #if 0
        int i,  j;
        char *tmp = (char *)src.vir_addr;
        for(i=0; i<SRC_HEIGHT; i++)
        {
            memset(tmp, (i&0xff), SRC_WIDTH);
            tmp += SRC_WIDTH;
        }
        #endif
        
        #if 1
        char *tmp = (char *)src.vir_addr;
        fpr = fopen("/sdcard/testin.yuv", "rb");
        for(i=0; i<SRC_HEIGHT; i++)
        {
            if(fpr)fread(tmp, 1, SRC_WIDTH, fpr);
            tmp += src_vir_width;
        }
        tmp = (char *)src.vir_addr;
        if(fpr)fread(&tmp[src_vir_width*src_vir_height], 1, SRC_WIDTH*SRC_HEIGHT/2, fpr);
        if(fpr)fclose(fpr);
        
        for(i=0; i<SRC_HEIGHT/2; i++)     //planar to semi planar
        {
            for(j=0; j<SRC_WIDTH/2; j++)     //planar to semi planar
            {
                tmpbuf[i*src_vir_width+j*2+0] = tmp[src_vir_width*src_vir_height + i*SRC_WIDTH/2 + j];
                tmpbuf[i*src_vir_width+j*2+1] = tmp[src_vir_width*src_vir_height + SRC_WIDTH*SRC_HEIGHT/4 + i*SRC_WIDTH/2 + j];
            }
        }
        memcpy(&tmp[src_vir_width*src_vir_height], tmpbuf, src_vir_width*src_vir_height/2);        
        //memset(&tmp[SRC_WIDTH*SRC_HEIGHT], 0x80, SRC_WIDTH*SRC_HEIGHT/2);        
        #endif
        VPUMemClean(&src);
    }

    while(1)
    {
        printf("framecnt=%d\n", framecnt);
        
        if(framecnt++ >= 1)
            break;
        
        wid_alig16 = ((SRC_WIDTH+15)&(~0xf));
        hei_alig16 = ((SRC_HEIGHT+15)&(~0xf));

        android::PP_OPERATION opt;
        memset(&opt, 0, sizeof(opt));
        opt.srcAddr     = src.phy_addr;
        opt.srcFormat   = PP_IN_FORMAT_YUV420SEMI;
        opt.srcWidth    = SRC_WIDTH;
        opt.srcHeight   = SRC_HEIGHT;
        opt.srcHStride  = src_vir_width;
        opt.srcVStride  = src_vir_height;
        opt.srcX   = 0;
        opt.srcY   = 0;        
        if(wid_alig16 != SRC_WIDTH)
            opt.srcCrop8R = 1;
        if(hei_alig16 != SRC_HEIGHT)
            opt.srcCrop8D= 1;

        wid_alig16 = ((DST_WIDTH+15)&(~0xf));
        hei_alig16 = ((DST_HEIGHT+15)&(~0xf));
        
        opt.dstAddr     = dst.phy_addr;
        opt.dstFormat   = PP_OUT_FORMAT_YUV420INTERLAVE;
        opt.dstWidth    = DST_WIDTH;
        opt.dstHeight   = DST_HEIGHT;
        opt.dstHStride  = dst_vir_width;
        opt.dstVStride  = dst_vir_height;
        opt.dstX   = 0;
        opt.dstY   = 0;
        
        opt.deinterlace = 0;
        
        opt.rotation    = PP_ROTATION_RIGHT_90;//PP_ROTATION_RIGHT_90;//PP_ROTATION_RIGHT_90;//PP_ROTATION_RIGHT_90;
        
        opt.vpuFd       = vpuFd;
        ret |= android::ppOpInit(&hnd, &opt);
        if (ret) {
            ALOGE("ppOpInit failed");
            hnd = NULL;
            goto end;
        }
        ret = android::ppOpPerform(hnd);
        if (ret) {
            ALOGE("ppOpPerform failed");
        }
        ret = android::ppOpSync(hnd);
        if (ret) {
            ALOGE("ppOpSync failed");
        }
        ret = android::ppOpRelease(hnd);
        if (ret) {
            ALOGE("ppOpPerform failed");
        }
    
        VPUMemInvalidate(&dst);
        {
            #if 0
            int i,  j, ret = 0;
            char *tmp = (char *)dst.vir_addr;
            /*for(i=0; i<3; i++)
            {
                printf("col out_pos=%d, 0x%x\n", i*DST_WIDTH, tmp[i*DST_WIDTH]);
            }
            for(i=0; i<3; i++)
            {
                printf("las out_pos=%d, 0x%x\n", (DST_HEIGHT-1-i)*DST_WIDTH, tmp[(DST_HEIGHT-1-i)*DST_WIDTH]);
            }*/
            for(i=0; i<DST_HEIGHT; i++)
            {
                for(j=0; j<DST_WIDTH; j++)
                {
                    if( tmp[i*DST_WIDTH + j] != (i&0xff))
                        ret = 1;
                }
            }
            if( ret)
                printf("framecount=%d pp operation is failed!\n", (framecnt-1));
            else
                printf("framecount=%d pp operation is suceess!\n", (framecnt-1));

            memset(dst.vir_addr, 0xff, DST_SIZE);  
            VPUMemClean(&dst);
            #endif
            
            #if 1
            char *tmp = (char *)dst.vir_addr;
            //memset(&tmp[DST_WIDTH*DST_HEIGHT], 0x80, DST_WIDTH*DST_HEIGHT/2);    
            //VPUMemClean(&dst);
            fpw = fopen("/data/testout.yuv", "wb+");
            
            if(fpw)fwrite((char *)(dst.vir_addr), 1, dst_vir_width*dst_vir_height*3/2, fpw);
            if(fpw)fclose(fpw);
            #endif
        }
    }
    
end:
    if(tmpbuf)free(tmpbuf);
    if (src.phy_addr) VPUFreeLinear(&src);
    if (dst.phy_addr) VPUFreeLinear(&dst);
    if (vpuFd > 0) VPUClientRelease(vpuFd);
    ALOGI("ppOp test end\n");
    return 0;
}

#endif

