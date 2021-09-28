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

#ifndef _FRAME_H_
#define _FRAME_H_

#include "vpu_global.h"
#include "vpu_mem.h"


class framemanager
{

public:
    framemanager();
    ~framemanager();

    RK_S32      init(RK_U32 framenum);
    RK_S32      deinit();
    VPU_FRAME*  get_frame(RK_U32 size,void *ctx);
    RK_S32      malloc_frame(VPU_FRAME *frame, RK_U32 size,void *ctx);
    RK_S32      free_frame(VPU_FRAME *frame);
	void		push_empty(VPU_FRAME *frame);
	void		push_display(VPU_FRAME *frame);
	VPU_FRAME* get_display(void);
	void		employ_frame(VPU_FRAME *frame);
private:
    VPU_FRAME   *FrmBufBase;
    RK_U32      frameNum;
	VPU_FRAME *empty_head;
	VPU_FRAME *empty_end;
	RK_U32		empty_cnt;
	VPU_FRAME *display_head;
	VPU_FRAME *display_end;
	RK_U32		display_cnt;
};

#endif
