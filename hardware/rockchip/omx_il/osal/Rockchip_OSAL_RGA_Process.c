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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "vpu_type.h"
#include "Rockchip_OMX_Def.h"
#include "vpu_global.h"
#include "Rockchip_OSAL_Log.h"
#include "Rockchip_OSAL_Memory.h"
#include "Rockchip_OSAL_RGA_Process.h"
#include "hardware/rga.h"

#ifdef USE_DRM
#include "drmrga.h"
#include "RgaApi.h"
#include <drm_fourcc.h>
#else
typedef struct _rga_ctx {
    int32_t rga_fd;
} rga_ctx_t;
typedef struct _rga_info {
    int xoffset;
    int yoffset;
    int width;
    int height;
    int vir_w;
    int vir_h;
    int format;
    int fd;
    void *vir_addr;
    int type;
} rga_info_t;
#endif

OMX_S32 rga_dev_open(void **rga_ctx)
{
#ifndef USE_DRM
    rga_ctx_t *ctx = NULL;
    ctx = Rockchip_OSAL_Malloc(sizeof(rga_ctx_t));
    Rockchip_OSAL_Memset(ctx, 0, sizeof(rga_ctx_t));
    ctx->rga_fd = -1;
    ctx->rga_fd = open("/dev/rga", O_RDWR, 0);
    if (ctx->rga_fd < 0) {
        omx_err("rga open fail");
        return -1;
    }
    *rga_ctx = ctx;
    return 0;
#else
    RgaInit(rga_ctx);
    if (*rga_ctx == NULL) {
        return -1;
    }
    return 0;
#endif
}

OMX_S32 rga_dev_close(void *rga_ctx)
{
#ifndef USE_DRM
    rga_ctx_t *ctx = (rga_ctx_t *)rga_ctx;
    if (ctx == NULL) {
        return 0;
    }
    if ( ctx->rga_fd >= 0) {
        close(ctx->rga_fd);
        ctx->rga_fd = -1;
    }
    Rockchip_OSAL_Free(rga_ctx);
    rga_ctx = NULL;
    return 0;
#else
    RgaDeInit(rga_ctx);
    return 0;
#endif
}
#ifndef USE_DRM
#define RGA_BUF_GEM_TYPE_DMA 0x80
OMX_S32 rga_copy(RockchipVideoPlane *plane, VPUMemLinear_t *vpumem, uint32_t Width, uint32_t Height, int format, int rga_fd)
{
    struct rga_req  Rga_Request;
    Rockchip_OSAL_Memset(&Rga_Request, 0x0, sizeof(Rga_Request));
#ifdef SOFIA_3GR
    Rga_Request.line_draw_info.color = plane->fd & 0xffff;
#else
    Rga_Request.src.yrgb_addr = plane->fd;
#endif
    Rga_Request.src.uv_addr  = 0;
    Rga_Request.src.v_addr   =  0;
    Rga_Request.src.vir_w = plane->stride;
    Rga_Request.src.vir_h = Height;

    Rga_Request.src.format = format;

    Rga_Request.src.act_w = Width;
    Rga_Request.src.act_h = Height;
    Rga_Request.src.x_offset = 0;
    Rga_Request.src.y_offset = 0;
    Rga_Request.dst.yrgb_addr = 0;
#ifdef SOFIA_3GR
    if (!VPUMemJudgeIommu()) {
        Rga_Request.dst.yrgb_addr  = vpumem->phy_addr;
    } else {
        Rga_Request.line_draw_info.color |= (vpumem->phy_addr & 0xffff) << 16;
        Rga_Request.dst.uv_addr  = vpumem->vir_addr;
    }

#else
    if (!VPUMemJudgeIommu()) {
        Rga_Request.dst.uv_addr  = vpumem->phy_addr;
    } else {
        Rga_Request.dst.yrgb_addr = vpumem->phy_addr;
        Rga_Request.dst.uv_addr  = (OMX_U32)vpumem->vir_addr;
    }
#endif
    Rga_Request.dst.v_addr   = 0;
    Rga_Request.dst.vir_w = Width;
    Rga_Request.dst.vir_h = Height;
    Rga_Request.dst.format = Rga_Request.src.format;

    Rga_Request.clip.xmin = 0;
    Rga_Request.clip.xmax = Width - 1;
    Rga_Request.clip.ymin = 0;
    Rga_Request.clip.ymax = Height - 1;

    Rga_Request.dst.act_w = Width;
    Rga_Request.dst.act_h = Height;
    Rga_Request.dst.x_offset = 0;
    Rga_Request.dst.y_offset = 0;
    Rga_Request.rotate_mode = 0;
    Rga_Request.render_mode = 5;
    Rga_Request.render_mode |= RGA_BUF_GEM_TYPE_DMA;
    if (plane->type == ANB_PRIVATE_BUF_VIRTUAL) {
        Rga_Request.src.uv_addr = (OMX_U32)plane->addr;
        Rga_Request.mmu_info.mmu_en = 1;
        Rga_Request.mmu_info.mmu_flag = ((2 & 0x3) << 4) | 1;
        if (VPUMemJudgeIommu()) {
            Rga_Request.mmu_info.mmu_flag |= ((1 << 31) | (1 << 10) | (1 << 8));
        } else {
            Rga_Request.mmu_info.mmu_flag |= ((1 << 31) | (1 << 8));
        }
    } else {
        if (VPUMemJudgeIommu()) {
            Rga_Request.mmu_info.mmu_en = 1;
            Rga_Request.mmu_info.mmu_flag = ((2 & 0x3) << 4) | 1;
            Rga_Request.mmu_info.mmu_flag |= ((1 << 31) | (1 << 10) | (1 << 8));
        }
    }
    omx_trace("rga start in");
    if (ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0) {
        omx_err("rga rga_copy fail");
        return -1;
    }
    omx_trace("rga start out");
    return 0;
}

OMX_S32 rga_crop_scale(RockchipVideoPlane *plane,
                       VPUMemLinear_t *vpumem, OMX_VIDEO_PARAMS_EXTENDED *param_video,
                       RK_U32 orgin_w, RK_U32 orgin_h, int rga_fd)
{
    struct rga_req  Rga_Request;
    RK_U32 new_width = 0, new_height = 0;
    Rockchip_OSAL_Memset(&Rga_Request, 0x0, sizeof(Rga_Request));
    if (param_video->bEnableScaling || param_video->bEnableCropping) {
        if (param_video->bEnableScaling) {
            new_width = param_video->ui16ScaledWidth;
            new_height = param_video->ui16ScaledHeight;
        } else if (param_video->bEnableCropping) {
            new_width = orgin_w - param_video->ui16CropLeft - param_video->ui16CropRight;
            new_height = orgin_h - param_video->ui16CropTop - param_video->ui16CropBottom;
        }
    }
#ifdef SOFIA_3GR
    Rga_Request.line_draw_info.color = plane->fd & 0xffff;
#else
    Rga_Request.src.yrgb_addr = plane->fd;
#endif

    Rga_Request.src.uv_addr  = 0;
    Rga_Request.src.v_addr   =  0;
    Rga_Request.src.vir_w = plane->stride;
    Rga_Request.src.vir_h = orgin_h;

    Rga_Request.src.format = RK_FORMAT_YCbCr_420_SP;
    if (param_video->bEnableCropping) {
        Rga_Request.src.act_w = orgin_w - param_video->ui16CropLeft - param_video->ui16CropRight;
        Rga_Request.src.act_h = orgin_h - param_video->ui16CropTop - param_video->ui16CropBottom;
        Rga_Request.src.x_offset = param_video->ui16CropLeft;
        Rga_Request.src.y_offset = param_video->ui16CropTop;
    } else {
        Rga_Request.src.act_w = orgin_w;
        Rga_Request.src.act_h = orgin_h;
        Rga_Request.src.x_offset = 0;
        Rga_Request.src.y_offset = 0;
    }

    Rga_Request.dst.yrgb_addr = 0;

#ifdef SOFIA_3GR
    if (!VPUMemJudgeIommu()) {
        Rga_Request.dst.yrgb_addr = vpumem->phy_addr;
        Rga_Request.dst.uv_addr  = vpumem->phy_addr + plane->stride * orgin_h;
    } else {
        Rga_Request.line_draw_info.color |= (vpumem->phy_addr & 0xffff) << 16;
        Rga_Request.dst.uv_addr  = (OMX_U32)vpumem->vir_addr;
    }
#else
    if (!VPUMemJudgeIommu()) {
        Rga_Request.dst.uv_addr  = vpumem->phy_addr;
    } else {
        Rga_Request.dst.yrgb_addr = vpumem->phy_addr;
        Rga_Request.dst.uv_addr  = (OMX_U32)vpumem->vir_addr;
    }
#endif
    Rga_Request.dst.v_addr   = 0;
    Rga_Request.dst.vir_w = new_width;
    Rga_Request.dst.vir_h = new_height;
    Rga_Request.dst.format = RK_FORMAT_YCbCr_420_SP;

    Rga_Request.clip.xmin = 0;
    Rga_Request.clip.xmax = new_width - 1;
    Rga_Request.clip.ymin = 0;
    Rga_Request.clip.ymax = new_height - 1;

    Rga_Request.dst.act_w = new_width;
    Rga_Request.dst.act_h = new_height;
    Rga_Request.dst.x_offset = 0;
    Rga_Request.dst.y_offset = 0;
    Rga_Request.rotate_mode = 1;
    Rga_Request.sina = 0;
    Rga_Request.cosa = 65536;

    if (plane->type == ANB_PRIVATE_BUF_VIRTUAL) {
        Rga_Request.src.uv_addr = (OMX_U32)plane->addr;
        Rga_Request.mmu_info.mmu_en = 1;
        Rga_Request.mmu_info.mmu_flag = ((2 & 0x3) << 4) | 1;
        if (VPUMemJudgeIommu()) {
            Rga_Request.mmu_info.mmu_flag |= ((1 << 31) | (1 << 10) | (1 << 8));
        } else {
            Rga_Request.mmu_info.mmu_flag |= ((1 << 31) | (1 << 8));
        }
    } else {
        if (VPUMemJudgeIommu()) {
            Rga_Request.mmu_info.mmu_en = 1;
            Rga_Request.mmu_info.mmu_flag = ((2 & 0x3) << 4) | 1;
            Rga_Request.mmu_info.mmu_flag |= ((1 << 31) | (1 << 10) | (1 << 8));
        }
    }

    omx_trace("rga start in");

    if (ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0) {
        omx_err("rga_rgb2nv12 rga RGA_BLIT_SYNC fail");
        return -1;
    }
    omx_trace("rga start out");
    return 0;
}

void rga_set_info(rga_info_t *info, int w, int h, int v_w, int v_h, int fd, int format, void *vir_addr, int type)
{
    info->width = w;
    info->height = h;
    info->vir_w = v_w;
    info->vir_h = v_h;
    info->fd = fd;
    info->format = format;
    info->vir_addr = vir_addr;
    info->type = type;
}

OMX_S32 rga_convert(rga_info_t *src, rga_info_t *dst, int rga_fd)
{
    if (rga_fd < 0) {
        return -1;
    }
    struct rga_req  Rga_Request;
    Rockchip_OSAL_Memset(&Rga_Request, 0x0, sizeof(Rga_Request));
    Rga_Request.src.yrgb_addr =  0;
#ifdef SOFIA_3GR
    if (!VPUMemJudgeIommu()) {
        Rga_Request.src.yrgb_addr  = src->fd;
        Rga_Request.src.uv_addr  = src->fd + src->vir_w * src->vir_h;
    } else {
        Rga_Request.line_draw_info.color = src->fd & 0xffff;
        Rga_Request.src.yrgb_addr = src->fd;
        Rga_Request.src.uv_addr  = src->vir_addr;
    }
#else
    if (!VPUMemJudgeIommu()) {
        Rga_Request.src.uv_addr  = src->fd;
    } else {
        Rga_Request.src.yrgb_addr = src->fd;
        Rga_Request.src.uv_addr  = (OMX_U32)src->vir_addr;
    }
#endif
    Rga_Request.src.v_addr   = 0;
    Rga_Request.src.vir_w = src->vir_w;
    Rga_Request.src.vir_h = src->vir_h;
    Rga_Request.src.format = src->format;

    Rga_Request.src.act_w = src->width;
    Rga_Request.src.act_h = src->height;
    Rga_Request.src.x_offset = 0;
    Rga_Request.src.y_offset = 0;
#ifdef SOFIA_3GR
    Rga_Request.line_draw_info.color |= (dst->fd & 0xffff) << 16;
#else
    Rga_Request.dst.yrgb_addr = dst->fd ;
#endif
    Rga_Request.dst.uv_addr  = 0;
    Rga_Request.dst.v_addr   = 0;
    Rga_Request.dst.vir_w = dst->vir_w;
    Rga_Request.dst.vir_h = dst->vir_h;
    Rga_Request.dst.format = dst->format;
    Rga_Request.clip.xmin = 0;
    Rga_Request.clip.xmax = dst->vir_w - 1;
    Rga_Request.clip.ymin = 0;
    Rga_Request.clip.ymax =  dst->vir_h - 1;
    Rga_Request.dst.act_w = dst->width;
    Rga_Request.dst.act_h = dst->height;
    Rga_Request.dst.x_offset = 0;
    Rga_Request.dst.y_offset = 0;
    Rga_Request.rotate_mode = 0;
    Rga_Request.yuv2rgb_mode = (2 << 4);
    if (src->type == ANB_PRIVATE_BUF_VIRTUAL || dst->type == ANB_PRIVATE_BUF_VIRTUAL) {
        Rga_Request.mmu_info.mmu_flag  = ((2 & 0x3) << 4) | 1;
        Rga_Request.mmu_info.mmu_en = 1;
        if (src->type == ANB_PRIVATE_BUF_VIRTUAL) {
            Rga_Request.src.uv_addr  =  (OMX_U32)(src->vir_addr);
            Rga_Request.mmu_info.mmu_flag |= ((1 << 31) | (1 << 8));
        } else {
            Rga_Request.dst.uv_addr  =  (OMX_U32)(dst->vir_addr);
            Rga_Request.mmu_info.mmu_flag |= ((1 << 31) | (1 << 10));
        }
        if (VPUMemJudgeIommu()) {
            Rga_Request.mmu_info.mmu_flag |= ((1 << 31) | (1 << 10) | (1 << 8));
        }
    } else {
        if (VPUMemJudgeIommu()) {
            Rga_Request.mmu_info.mmu_en    = 1;
            Rga_Request.mmu_info.mmu_flag  = ((2 & 0x3) << 4) | 1;
            Rga_Request.mmu_info.mmu_flag |= (1 << 31) | (1 << 10) | (1 << 8);
        }
    }
    if (ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0) {
        omx_err("rga_convert fail");
        return -1;
    }
    return 0;
}
#endif

void rga_nv12_crop_scale(RockchipVideoPlane *plane,
                         VPUMemLinear_t *vpumem, OMX_VIDEO_PARAMS_EXTENDED *param_video,
                         RK_U32 orgin_w, RK_U32 orgin_h, void* rga_ctx)
{
#ifndef USE_DRM
    rga_ctx_t *ctx = (rga_ctx_t *)rga_ctx;
    if (ctx == NULL) {
        return;
    }
    if (rga_crop_scale(plane, vpumem, param_video, orgin_w, orgin_h, ctx->rga_fd) < 0) {
        omx_err("rga_crop_scale fail");
    }
#else
    RK_U32 new_width = 0, new_height = 0;
    rga_info_t src;
    rga_info_t dst;
    (void) rga_ctx;
    memset((void*)&src, 0, sizeof(rga_info_t));
    memset((void*)&dst, 0, sizeof(rga_info_t));
    if (param_video->bEnableScaling || param_video->bEnableCropping) {
        if (param_video->bEnableScaling) {
            new_width = param_video->ui16ScaledWidth;
            new_height = param_video->ui16ScaledHeight;
        } else if (param_video->bEnableCropping) {
            new_width = orgin_w - param_video->ui16CropLeft - param_video->ui16CropRight;
            new_height = orgin_h - param_video->ui16CropTop - param_video->ui16CropBottom;
        }
    }

    if (param_video->bEnableCropping) {
        RK_U32 x, y, w, h;
        w = orgin_w - param_video->ui16CropLeft - param_video->ui16CropRight;
        h = orgin_h - param_video->ui16CropTop - param_video->ui16CropBottom;
        x = param_video->ui16CropLeft;
        y = param_video->ui16CropTop;
        rga_set_rect(&src.rect, x, y, w, h, plane->stride, h, HAL_PIXEL_FORMAT_YCrCb_NV12);
    } else {
        rga_set_rect(&src.rect, 0, 0, orgin_w, orgin_h, plane->stride, orgin_h, HAL_PIXEL_FORMAT_YCrCb_NV12);

    }
    rga_set_rect(&dst.rect, 0, 0, orgin_w, orgin_h, orgin_w, orgin_h, HAL_PIXEL_FORMAT_YCrCb_NV12);
    src.fd = plane->fd;
    dst.fd = vpumem->phy_addr;
    if (RgaBlit(&src, &dst, NULL)) {
        omx_err("RgaBlit fail");
    }
#endif
}

void rga_rgb2nv12(RockchipVideoPlane *plane, VPUMemLinear_t *vpumem,
                  uint32_t Width, uint32_t Height, uint32_t dstWidth, uint32_t dstHeight,  void* rga_ctx)
{

    (void)dstWidth;
    (void)dstHeight;
#ifndef USE_DRM
    rga_info_t src;
    rga_info_t dst;

    rga_ctx_t *ctx = (rga_ctx_t *)rga_ctx;
    memset((void*)&src, 0, sizeof(rga_info_t));
    memset((void*)&dst, 0, sizeof(rga_info_t));
    if (ctx == NULL) {
        return;
    }
    rga_set_info(&src, Width, Height, plane->stride, Height, plane->fd, RK_FORMAT_RGBA_8888, (void *)plane->addr, plane->type);
    rga_set_info(&dst, dstWidth, dstHeight, dstWidth, dstHeight, vpumem->phy_addr, RK_FORMAT_YCbCr_420_SP, (void *)vpumem->vir_addr, 0);
    if (rga_convert(&src, &dst, ctx->rga_fd) < 0) {
        omx_err("rga_rgb2nv12 fail");
    }
#else
    rga_info_t src;
    rga_info_t dst;
    (void) rga_ctx;
    memset((void*)&src, 0, sizeof(rga_info_t));
    memset((void*)&dst, 0, sizeof(rga_info_t));
    omx_trace(" plane->stride %d", plane->stride);
    rga_set_rect(&src.rect, 0, 0, Width, Height, plane->stride, Height, HAL_PIXEL_FORMAT_RGBA_8888);
    rga_set_rect(&dst.rect, 0, 0, Width, Height, dstWidth, dstHeight, HAL_PIXEL_FORMAT_YCrCb_NV12);
    src.fd = plane->fd;
    dst.fd = vpumem->phy_addr;
    omx_trace("RgaBlit in src.fd = 0x%x, dst.fd = 0x%x", src.fd, dst.fd);
    if (RgaBlit(&src, &dst, NULL)) {
        omx_err("RgaBlit fail");
    }
    omx_trace("RgaBlit out");
#endif
}

void rga_nv122rgb( RockchipVideoPlane *planes, VPUMemLinear_t *vpumem, uint32_t Width, uint32_t Height, int dst_format, void* rga_ctx)
{
#ifndef USE_DRM
    rga_info_t src;
    rga_info_t dst;
    rga_ctx_t *ctx = (rga_ctx_t *)rga_ctx;
    memset((void*)&src, 0, sizeof(rga_info_t));
    memset((void*)&dst, 0, sizeof(rga_info_t));
    Width  = (Width + 15) & (~15);
    Height = (Height + 15) & (~15);
    rga_set_info(&src, Width, Height, Width, Height, vpumem->phy_addr, RK_FORMAT_YCbCr_420_SP, (void *)vpumem->vir_addr, 0);
    rga_set_info(&dst, Width, Height, planes->stride, Height, planes->fd, dst_format, (void *)planes->addr, planes->type);
    if (ctx == NULL) {
        return;
    }
    if (rga_convert(&src, &dst, ctx->rga_fd) < 0) {
        omx_err("rga_nv122rgb fail");
    }
#else
    rga_info_t src;
    rga_info_t dst;
    (void) rga_ctx;
    memset((void*)&src, 0, sizeof(rga_info_t));
    memset((void*)&dst, 0, sizeof(rga_info_t));
    rga_set_rect(&src.rect, 0, 0, Width, Height, (Width + 15) & (~15), Height, HAL_PIXEL_FORMAT_YCrCb_NV12);
    if (dst_format == RK_FORMAT_BGRA_8888) {
        dst_format = HAL_PIXEL_FORMAT_BGRA_8888;
    } else if (dst_format == RK_FORMAT_RGBA_8888) {
        dst_format = HAL_PIXEL_FORMAT_RGBA_8888;
    }
    rga_set_rect(&dst.rect, 0, 0, Width, Height, planes->stride, Height, dst_format);
    src.fd = vpumem->phy_addr;
    dst.fd = planes->fd;
    if (RgaBlit(&src, &dst, NULL)) {
        omx_err("RgaBlit fail");
    }
#endif
}

void rga_nv12_copy(RockchipVideoPlane *plane, VPUMemLinear_t *vpumem, uint32_t Width, uint32_t Height, void* rga_ctx)
{
#ifndef USE_DRM
    int format =  RK_FORMAT_YCbCr_420_SP;
    rga_ctx_t *ctx = (rga_ctx_t *)rga_ctx;
    if (ctx == NULL) {
        return;
    }

    if (rga_copy(plane, vpumem, Width, Height, format, ctx->rga_fd) < 0) {
        omx_err("rga_nv12_copy fail");
    }
#else
    rga_info_t src;
    rga_info_t dst;
    (void) rga_ctx;
    memset((void*)&src, 0, sizeof(rga_info_t));
    memset((void*)&dst, 0, sizeof(rga_info_t));
    rga_set_rect(&src.rect, 0, 0, Width, Height, plane->stride, Height, HAL_PIXEL_FORMAT_YCrCb_NV12);
    rga_set_rect(&dst.rect, 0, 0, Width, Height, Width, Height, HAL_PIXEL_FORMAT_YCrCb_NV12);
    src.fd = plane->fd;
    dst.fd = vpumem->phy_addr;
    if (RgaBlit(&src, &dst, NULL)) {
        omx_err("RgaBlit fail");
    }

#endif
}

void rga_rgb_copy(RockchipVideoPlane *plane, VPUMemLinear_t *vpumem, uint32_t Width, uint32_t Height, void* rga_ctx)
{
#ifndef USE_DRM
    int format =  RK_FORMAT_RGBA_8888;
    rga_ctx_t *ctx = (rga_ctx_t *)rga_ctx;
    if (ctx == NULL) {
        return;
    }
    if (rga_copy(plane, vpumem, Width, Height, format, ctx->rga_fd) < 0) {
        omx_err("rga_nv12_copy fail");
    }
#else
    rga_info_t src;
    rga_info_t dst;
    (void) rga_ctx;
    memset((void*)&src, 0, sizeof(rga_info_t));
    memset((void*)&dst, 0, sizeof(rga_info_t));
    rga_set_rect(&src.rect, 0, 0, Width, Height, plane->stride, Height, HAL_PIXEL_FORMAT_RGBA_8888);
    rga_set_rect(&dst.rect, 0, 0, Width, Height, Width, Height, HAL_PIXEL_FORMAT_RGBA_8888);
    src.fd = plane->fd;
    dst.fd = vpumem->phy_addr;
    if (RgaBlit(&src, &dst, NULL)) {
        omx_err("RgaBlit fail");
    }
#endif
}


