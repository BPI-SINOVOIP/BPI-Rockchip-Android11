/*
 * Copyright (C) 2015 Rockchip Electronics Co., Ltd.
 */
#ifndef HW_JPEGDECAPI_H
#define HW_JPEGDECAPI_H

#include "../../src_dec/inc/jpegdecapi.h"

#ifdef __cplusplus
extern "C"
{
#endif

//#define DEBUG_HW_JPEG
//#define WHALLLOG LOGE
#define WHALLLOG(...)

#ifdef DEBUG_HW_JPEG
#define WHREDLOG LOGE
#include <utils/Log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "HW_JPEG"
#define WHLOG LOGD
#else
#define WHLOG(...)
#define WHREDLOG(...)
#endif

typedef int HW_BOOL;

#ifndef CODEBEGIN
#define CODEBEGIN do{
#endif

#ifndef CODEEND
#define CODEEND }while(0);
#endif

#define PP_IN_FORMAT_YUV422INTERLAVE            0
#define PP_IN_FORMAT_YUV420SEMI                     1
#define PP_IN_FORMAT_YUV420PLANAR                  2
#define PP_IN_FORMAT_YUV400                             3
#define PP_IN_FORMAT_YUV422SEMI                     4
#define PP_IN_FORMAT_YUV420SEMITIELED           5
#define PP_IN_FORMAT_YUV440SEMI                     6
#define PP_IN_FORMAT_YUV444_SEMI                                 7
#define PP_IN_FORMAT_YUV411_SEMI                                 8

#define PP_OUT_FORMAT_RGB565                    0
#define PP_OUT_FORMAT_ARGB                       1
#define PP_OUT_FORMAT_YUV422INTERLAVE    3   
#define PP_OUT_FORMAT_YUV420INTERLAVE    5

#ifdef JPEG_INPUT_BUFFER
#undef JPEG_INPUT_BUFFER
#endif
#define JPEG_INPUT_BUFFER 5120

//modify following values in special product
#define BRIGHTNESS 4	// -128 ~ 127
#define CONTRAST 0		// -64 ~ 64
#define SATURATION 0	// -64 ~ 128

typedef struct
{
  struct hw_jpeg_source_mgr * inStream;
  int wholeStreamLength;
  int thumbOffset;
  int thumbLength;
  HW_BOOL useThumb;
  //int (*FillInputBuf) (void *, char *, size_t );
} SourceStreamCtl;

typedef struct
{
  int outFomart; /* =0,RGB565;=1,ARGB 8888 */
  //int destWidth;
  //int destHeight;
  int scale_denom;
  HW_BOOL shouldDither;
  int cropX;
  int cropY;
  int cropW;
  int cropH;
} PostProcessInfo;

typedef struct{
	VPUMemLinear_t thumbpmem;
	HW_BOOL reuse;
}ReusePmem;

typedef struct
{
  void * decoderHandle;
  char *outAddr;
  int ppscalew;
  int ppscaleh;
  int outWidth;
  int outHeight;
  HW_BOOL shouldScale;
  ReusePmem* thumbPmem;
} HwJpegOutputInfo;

typedef struct Hw_Jpeg_InputInfo
{
  SourceStreamCtl  streamCtl;
  PostProcessInfo  ppInfo;
  HW_BOOL justcaloutwh;
  //HW_BOOL (*get_ppInfo_msg)(struct Hw_Jpeg_InputInfo* hwInfo);
} HwJpegInputInfo;

struct hw_jpeg_source_mgr{
	HW_BOOL	isVpuMem;
	const unsigned char * next_input_byte;
	//const char * start_input_byte;
	long bytes_in_buffer;
	//size_t cur_pos_inbuffer;
	long cur_offset_instream;
	HwJpegInputInfo* info;
	void (*init_source)(HwJpegInputInfo* hwInfo);
	HW_BOOL (*fill_input_buffer)(HwJpegInputInfo* hwInfo);
	HW_BOOL (*skip_input_data)(HwJpegInputInfo* hwInfo, long num_bytes);
	HW_BOOL (*resync_to_restart)(HwJpegInputInfo* hwInfo);
	//void (*term_source)(HwJpegInputInfo* hwInfo);
	HW_BOOL (*seek_input_data)(HwJpegInputInfo* hwInfo, long byte_offset);
	int (*fill_buffer)(HwJpegInputInfo* hwInfo, void * destination, VPUMemLinear_t *newmem, int w, int h);//fill destination with stream
	int (*fill_thumb)(HwJpegInputInfo* hwInfo, void * thumbBuf);//fill thumbBuf with thumbmsg if has thumb
	HW_BOOL (*read_1_byte)(HwJpegInputInfo* hwInfo, unsigned char *ch);//read one byte from stream
	void (*get_vpumemInst)(HwJpegInputInfo* hwInfo, VPUMemLinear_t* vpumem);
};

extern int hw_jpeg_decode(HwJpegInputInfo *inInfo, HwJpegOutputInfo *outInfo, char *reuseBitmap, int bm_w, int bm_h);

extern int hw_jpeg_release(void *decInst);

extern int hw_jpeg_VPUMallocLinear(VPUMemLinear_t *p, int size);

extern int hw_jpeg_VPUFreeLinear(VPUMemLinear_t *p);

//extern void hw_jpeg_VPUMemFlush(VPUMemLinear_t *p);

extern int SetPostProcessor(unsigned int * reg,VPUMemLinear_t *dst,int inWidth,int inHeigth,
								int outWidth,int outHeight,int inColor, PostProcessInfo *ppInfo);

extern void resetImageInfo(JpegDecImageInfo * imageInfo);

#ifdef __cplusplus
}
#endif

#endif /* HW_JPEGDECAPI_H */

