/*
 * Copyright(C) 2010 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved
 * BY DOWNLOADING, INSTALLING, COPYING, SAVING OR OTHERWISE USING THIS
 * SOFTWARE, YOU ACKNOWLEDGE THAT YOU AGREE THE SOFTWARE RECEIVED FORM ROCKCHIP
 * IS PROVIDED TO YOU ON AN "AS IS" BASIS and ROCKCHIP DISCLAIMS ANY AND ALL
 * WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH FILE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED
 * WARRANTIES OF TITLE, NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY,
 * ACCURACY OR FITNESS FOR A PARTICULAR PURPOSE
 * Rockchip hereby grants to you a limited, non-exclusive, non-sublicensable and
 * non-transferable license
 *   (a) to install, save and use the Software;
 *   (b) to * copy and distribute the Software in binary code format only
 * Except as expressively authorized by Rockchip in writing, you may NOT:
 *   (a) distribute the Software in source code;
 *   (b) distribute on a standalone basis but you may distribute the Software in
 *   conjunction with platforms incorporating Rockchip integrated circuits;
 *   (c) modify the Software in whole or part;
 *   (d) decompile, reverse-engineer, dissemble, or attempt to derive any source
 *   code from the Software;
 *   (e) remove or obscure any copyright, patent, or trademark statement or
 *   notices contained in the Software
 */

/***************************************************************************************************
          	 File:
		frame.cpp
	Desription:
		frame management
	Author:
		Luo Ning
		Jian Huan
	Date:
		2010-12-8 21:30:24
 **************************************************************************************************/
#define LOG_TAG "vpu_framemanager"
#include <stdlib.h>
#include <string.h>
#include "framemanager.h"
#include <utils/Log.h>
framemanager::framemanager()
{
	memset(this,0,sizeof(framemanager));
}

framemanager::~framemanager()
{
	VPU_FRAME	*frame;
	RK_U32		i;

	if (FrmBufBase)
	{
		for (i=0; i<frameNum; i++)
		{
			if (FrmBufBase[i].vpumem.vir_addr)
				VPUFreeLinear(&FrmBufBase[i].vpumem);
		}
		free(FrmBufBase);
	}
	memset(this,0,sizeof(framemanager));
}

/***************************************************************************************************
	Func:
		init
	Description:
		init frame
	Notice:
		分配frame manage结构体的空间，并初始化。传递进来的framenum表示最大开辟的个数
	Author:
		Jian Huan
	Date:
		2010-12-6 15:19:53
 **************************************************************************************************/
RK_S32 framemanager::init(RK_U32 framenum)
{
	RK_U32	i;

	FrmBufBase = (VPU_FRAME *)malloc(framenum*sizeof(VPU_FRAME));

	if (FrmBufBase == NULL)
	{
		return VPU_ERR;
	}

	memset(FrmBufBase, 0, framenum*sizeof(VPU_FRAME));

	for (i=0; i<framenum; i++)
	{
		push_empty(&FrmBufBase[i]);
	}

	frameNum = framenum;

	return VPU_OK;
}

/***************************************************************************************************
	Func:
		push_empty
	Description:
		push empty frame struct to manager
	Notice:
		将空帧压入资源队列管理
	Author:
		Jesan
	Date:
		2010-12-13 15:10:00
 **************************************************************************************************/
void framemanager::push_empty(VPU_FRAME * frame)
{
	if (empty_cnt)
	{
		empty_end->next_frame = frame;
		empty_end = frame;
	}
	else
	{
		empty_head = frame;
		empty_end = frame;
	}
	frame->employ_cnt = 0;
	frame->next_frame = NULL;
	empty_cnt++;
}
/***************************************************************************************************
	Func:
		deinit
	Description:
		deinit frame manage structer
	Notice:
		释放帧管理结构体
	Author:
		Jian Huan
	Date:
		2010-12-6 15:19:53
 **************************************************************************************************/
RK_S32 framemanager::deinit()
{
	free(FrmBufBase);

	FrmBufBase = NULL;

	frameNum = 0;

	return VPU_OK;
}

/***************************************************************************************************
	Func:
		get_frame
	Description:
		malloc linear frame buffer and linked to related VPU_FRAME
	Author:
		Jian Huan
		Jesan
	Date:
		2010-12-8  21:46:30
		2010-12-13 15:16:30
 **************************************************************************************************/
VPU_FRAME* framemanager::get_frame(RK_U32 size,void *ctx)
{
	VPU_FRAME 	*frame = NULL;
	RK_S32		status;
    RK_S32     timeout = 0x10;
	while (empty_cnt == 0)
	{
        usleep(5000);
        if(timeout-- < 0)
        return NULL;
	}
	frame = empty_head;
	status = malloc_frame(frame, size,ctx);

	if (status != VPU_OK)
	{
		return NULL;
	}
	if (frame->next_frame)
	{
		empty_head = frame->next_frame;
		empty_cnt--;
	}
	else
	{
		empty_head = NULL;
		empty_end = NULL;
		empty_cnt = 0;
	}


	frame->employ_cnt = 1;
	return frame;
}

void framemanager::push_display(VPU_FRAME * frame)
{
	if (display_cnt)
	{
		display_end->next_frame = frame;
		display_end = frame;
	}
	else
	{
		display_head = frame;
		display_end = frame;
	}
	frame->employ_cnt++;
	frame->next_frame = NULL;
	display_cnt++;
}

void framemanager::employ_frame(VPU_FRAME *frame)
{
	if (frame)
		frame->employ_cnt++;
}

VPU_FRAME* framemanager::get_display(void)
{
	VPU_FRAME	*frame = NULL;

	if (display_cnt)
	{
		frame = display_head;
		if (frame->next_frame)
			display_head = frame->next_frame;
		else
		{
			display_head = NULL;
			display_end = NULL;
		}
		display_cnt--;
	}
	return frame;
}
/***************************************************************************************************
	Func:
		malloc_frame
	Description:
		malloc a new empty frame
	Return:
		VPU_OK:		malloc sucessfully
		VPU_ERR:	malloc failed
	Author:
		Jian Huan
	Date:
		2010-12-7 17:29:53
 **************************************************************************************************/
RK_S32 framemanager::malloc_frame(VPU_FRAME *frame, RK_U32 size,void *ctx)
{
	RK_S32		status;
    RK_S32      timeout = 0xFF;
	do{
		/*@jh:这里缺少一个延时判断，一旦malloc总是不成功，则需要有个时间判断，防止死循环*/
        if(ctx != NULL){
		status = VPUMallocLinearFromRender(&frame->vpumem, size,ctx);
            if(status != VPU_OK){
                return status;
            }
        }else{
		    status = VPUMallocLinear(&frame->vpumem, size);
        }
        if(status != VPU_OK)
        {
            usleep(5000);
            if(timeout-- < 0)
            return status;
        }
	}while(status != VPU_OK);

	return status;
}

/***************************************************************************************************
	Func:
		free_frame
	Description:
		malloc a new empty frame
	Return:
		VPU_OK:		malloc sucessfully
		VPU_ERR:	malloc failed
	Author:
		Jian Huan
		Jesan
	Date:
		2010-12-7 17:29:53
		2010-12-13 15:30:30
 **************************************************************************************************/
RK_S32 framemanager::free_frame(VPU_FRAME *frame)
{
	if (!frame)
		return 0;
	if (frame->employ_cnt <= 1)
	{
		//if (frame->vpumem.offset)
			VPUFreeLinear(&frame->vpumem);
		memset(frame,0,sizeof(VPU_FRAME));
		push_empty(frame);
	}
	else
		frame->employ_cnt--;

	return VPU_OK;
}
