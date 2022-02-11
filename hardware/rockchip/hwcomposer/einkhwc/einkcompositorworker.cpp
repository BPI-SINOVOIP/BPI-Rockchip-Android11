/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "hwc-eink-compositor-worker"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include "einkcompositorworker.h"
#include "worker.h"
#include "hwc_util.h"

#include <errno.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <sched.h>
#include <libsync/sw_sync.h>
#include <android/sync.h>
#include "libcfa/libcfa.h"
#include "libregal/libeink.h"

namespace android {

static const int kMaxQueueDepth = 1;
static const int kAcquireWaitTimeoutMs = 3000;
static int last_regal = 0;
EinkCompositorWorker::EinkCompositorWorker()
    : Worker("Eink-compositor", HAL_PRIORITY_URGENT_DISPLAY),
      timeline_fd_(-1),
      timeline_(0),
      timeline_current_(0),
      hwc_context_(NULL){
}

EinkCompositorWorker::~EinkCompositorWorker() {
  if (timeline_fd_ >= 0) {
    FinishComposition(timeline_);
    close(timeline_fd_);
    timeline_fd_ = -1;
  }
  munmap(ebc_buffer_base, EINK_FB_SIZE * 4);
  if(ebc_fd > 0){
    close(ebc_fd);
    ebc_fd = -1;
  }
  munmap(waveform_base, 0x100000);
  if (waveform_fd > 0) {
    close(waveform_fd);
    waveform_fd = -1;
  }
  if(rga_output_addr != NULL){
    //Get virtual address
    const gralloc_module_t *gralloc;
    int ret = 0;
    ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                      (const hw_module_t **)&gralloc);
    if (ret) {
        ALOGE("Failed to open gralloc module");
        return;
    }
    DrmRgaBuffer &gra_buffer = rgaBuffers[0];
    buffer_handle_t src_hnd = gra_buffer.buffer()->handle;
    gralloc->unlock(gralloc, src_hnd);
  }
  if (gray256_new_buffer != NULL)
    free(gray256_new_buffer);

  if (gray256_old_buffer != NULL)
    free(gray256_old_buffer);
}

const char *pvi_wf_get_version(const char *waveform)
{
	static char spi_id_buffer[32];
	int i;

	for (i = 0; i < 31; i++)
		spi_id_buffer[i] = waveform[i + 0x41];

	spi_id_buffer[31] = '\0';

	return (const char *)spi_id_buffer;
}

int EinkCompositorWorker::Init(struct hwc_context_t *ctx) {
  hwc_context_ = ctx;
  int ret = sw_sync_timeline_create();
  if (ret < 0) {
    ALOGE("Failed to create sw sync timeline %d", ret);
    return ret;
  }
  timeline_fd_ = ret;

  pthread_cond_init(&eink_queue_cond_, NULL);


  ebc_fd = open("/dev/ebc", O_RDWR,0);
  if (ebc_fd < 0){
      ALOGE("open /dev/ebc failed\n");
      return -1;
  }
  memset(&ebc_buf_info,0x00,sizeof(struct ebc_buf_info_t));
  if(ioctl(ebc_fd, EBC_GET_BUFFER_INFO,&ebc_buf_info)!=0){
      ALOGE("EBC_GET_BUFFER_INFO failed\n");
      close(ebc_fd);
      return -1;
  }
  ebc_buffer_base = mmap(0, EINK_FB_SIZE*4, PROT_READ|PROT_WRITE, MAP_SHARED, ebc_fd, 0);
  if (ebc_buffer_base == MAP_FAILED) {
      ALOGE("Error mapping the ebc buffer (%s)\n", strerror(errno));
      close(ebc_fd);
      return -1;
  }

  snprintf(commit_buf_info.tid_name, 16, "hwc_compose");

  if(ioctl(ebc_fd, EBC_GET_BUFFER,&commit_buf_info)!=0)
  {
     ALOGE("EBC_GET_BUFFER failed\n");
     close(ebc_fd);
     return -1;
  }

  unsigned long vaddr_real = intptr_t(ebc_buffer_base);
  gray16_buffer = (int*)(vaddr_real + commit_buf_info.offset);

  gray256_new_buffer = (int *)malloc(ebc_buf_info.width * ebc_buf_info.height);
  gray256_old_buffer = (int *)malloc(ebc_buf_info.width * ebc_buf_info.height);
  memset(gray256_old_buffer, 0xff, ebc_buf_info.width * ebc_buf_info.height);
  memset(gray256_new_buffer, 0xff, ebc_buf_info.width * ebc_buf_info.height);

  //init waveform for eink regal mode
  waveform_fd = open("/dev/waveform", O_RDWR,0);
  if (waveform_fd < 0) {
      ALOGE("open /dev/waveform failed\n");
      goto OUT;
  }
  waveform_base = mmap(0, 0x100000, PROT_READ|PROT_WRITE, MAP_SHARED, waveform_fd, 0);
  if (waveform_base == MAP_FAILED) {
      ALOGE("Error mapping the waveform buffer (%s)\n", strerror(errno));
      close(waveform_fd);
      waveform_fd = -1;
      goto OUT;
  }

  ALOGD("waveform version: %s\n", pvi_wf_get_version((char *)waveform_base));
  ret = EInk_Init((char *)waveform_base);
  if (ret) {
      ALOGE("EInk_Init error, ret = %d\n", ret);
      close(waveform_fd);
      waveform_fd = -1;
      goto OUT;
  }
  ALOGD("eink regal lib init success\n");

OUT:
  return InitWorker();
}

void EinkCompositorWorker::QueueComposite(hwc_display_contents_1_t *dc,int CurrentEpdMode,int ResetEpdMode) {
  ATRACE_CALL();

  std::unique_ptr<EinkComposition> composition(new EinkComposition);
  composition->einkMode = -1;
  composition->fb_handle = NULL;

  composition->outbuf_acquire_fence.Set(dc->outbufAcquireFenceFd);
  dc->outbufAcquireFenceFd = -1;
  if (dc->retireFenceFd >= 0)
    close(dc->retireFenceFd);
  dc->retireFenceFd = CreateNextTimelineFence();

  for (size_t i = 0; i < dc->numHwLayers; ++i) {
    hwc_layer_1_t *layer = &dc->hwLayers[i];
  if (layer != NULL && layer->handle != NULL && layer->compositionType == HWC_FRAMEBUFFER_TARGET){
    composition->layer_acquire_fences.emplace_back(layer->acquireFenceFd);
    layer->acquireFenceFd = -1;
    if (layer->releaseFenceFd >= 0)
      close(layer->releaseFenceFd);
    layer->releaseFenceFd = CreateNextTimelineFence();
    composition->fb_handle = layer->handle;
    composition->einkMode = CurrentEpdMode;

    composition->release_timeline = timeline_;

    Lock();
    int ret = pthread_mutex_lock(&eink_lock_);
    if (ret) {
      ALOGE("Failed to acquire compositor lock %d", ret);
    }

    while (composite_queue_.size() >= kMaxQueueDepth) {
      Unlock();
      pthread_cond_wait(&eink_queue_cond_,&eink_lock_);
      Lock();
    }

    composite_queue_.push(std::move(composition));

    ret = pthread_mutex_unlock(&eink_lock_);
    if (ret)
      ALOGE("Failed to release compositor lock %d", ret);

    SignalLocked();
    Unlock();
    }
  }

}

void EinkCompositorWorker::Routine() {
  ATRACE_CALL();

  ALOGD_IF(log_level(DBG_INFO),"----------------------------EinkCompositorWorker Routine start----------------------------");

  int ret = Lock();
  if (ret) {
    ALOGE("Failed to lock worker, %d", ret);
    return;
  }

  int wait_ret = 0;
  if (composite_queue_.empty()) {
    wait_ret = WaitForSignalOrExitLocked();
  }

  ret = pthread_mutex_lock(&eink_lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
  }

  std::unique_ptr<EinkComposition> composition;
  if (!composite_queue_.empty()) {
    composition = std::move(composite_queue_.front());
    composite_queue_.pop();
    pthread_cond_signal(&eink_queue_cond_);
  }

  ret = pthread_mutex_unlock(&eink_lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
  }


  ret = Unlock();
  if (ret) {
    ALOGE("Failed to unlock worker, %d", ret);
    return;
  }

  if (wait_ret == -EINTR) {
    return;
  } else if (wait_ret) {
    ALOGE("Failed to wait for signal, %d", wait_ret);
    return;
  }

  Compose(std::move(composition));

  ALOGD_IF(log_level(DBG_INFO),"----------------------------EinkCompositorWorker Routine end----------------------------");
}

int EinkCompositorWorker::CreateNextTimelineFence() {
  ++timeline_;
  char acBuf[50];
  sprintf(acBuf,"eink-frame-%d",get_frame());
  return sw_sync_fence_create(timeline_fd_, acBuf, timeline_);
}

int EinkCompositorWorker::FinishComposition(int point) {
  int timeline_increase = point - timeline_current_;
  if (timeline_increase <= 0)
    return 0;
  int ret = sw_sync_timeline_inc(timeline_fd_, timeline_increase);
  if (ret)
    ALOGE("Failed to increment sync timeline %d", ret);
  else
    timeline_current_ = point;
  return ret;
}

#define CLIP(x) (((x) > 255) ? 255 : (x))

extern void Luma8bit_to_4bit_row_16(int  *src,  int *dst, short int *res0,  short int*res1, int w);



extern int gray256_to_gray16_dither(char *gray256_addr,int *gray16_buffer,int  panel_h, int panel_w,int vir_width);

extern int gray256_to_gray16(char *gray256_addr,int *gray16_buffer,int h,int w,int vir_w);

extern int gray256_to_gray2(char *gray256_addr,int *gray16_buffer,int h,int w,int vir_w);

extern void Luma8bit_to_4bit_row_2(short int  *src,  char *dst, short int *res0,  short int*res1, int w,int threshold);


extern int gray256_to_gray2_dither(char *gray256_addr,char *gray2_buffer,int  panel_h, int panel_w,int vir_width,Region region);

extern void Rgb888_to_color_eink(char *dst,int *src,int  fb_height, int fb_width,int vir_width);
extern void Rgb888_to_color_eink2(char *dst,int *src,int  fb_height, int fb_width,int vir_width);

extern void Rgb565_to_color_eink2(char *dst,int16_t *src,int  fb_height, int fb_width,int vir_width);

extern void neon_rgb888_to_gray256ARM(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);

extern void rgb888_to_gray16_dither(int *dst,uint8_t *src,int  panel_h, int panel_w,int vir_width);

extern void neon_rgb888_to_gray16ARM(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);

extern void Luma8bit_to_4bit_dither(int *dst,int *src,int  vir_height, int vir_width,int panel_w);

extern void rgb888_to_gray2_dither(uint8_t *dst, uint8_t *src, int panel_h, int panel_w,
        int vir_width, Region region);

extern void Luma8bit_to_4bit(unsigned int *graynew,unsigned int *gray8bit,int  vir_height, int vir_width,int panel_w);

static inline void apply_white_region(char *buffer, int height, int width, Region region,struct ebc_buf_info *ebc_buf_info_t )
{
	int left,right;
    if (region.isEmpty()) return;
    size_t count = 0;
    const Rect* rects = region.getArray(&count);
    for (int i = 0;i < (int)count;i++) {
      left = rects[i].left;
      right = rects[i].right;
      int w = right - left;
      int offset = rects[i].top * width + left;
      for (int h = rects[i].top;h <= rects[i].bottom && h < height;h++) {
          memset(buffer + (offset >> 1), 0xFF, w >> 1);
          offset += width;
      }
    }
}


int EinkCompositorWorker::Rgba8888ClipRgba(DrmRgaBuffer &rgaBuffer,const buffer_handle_t          &fb_handle) {
    ATRACE_CALL();
    int ret = 0;
    int rga_transform = 0;
    int src_l,src_t,src_w,src_h;
    int dst_l,dst_t,dst_r,dst_b;

    int dst_w,dst_h,dst_stride;
    int src_buf_w,src_buf_h,src_buf_stride,src_buf_format,dst_format;
    rga_info_t src, dst;
    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));
    src.fd = -1;
    dst.fd = -1;

#if (!RK_PER_MODE && RK_DRM_GRALLOC)
    src_buf_w = hwc_get_handle_attibute(fb_handle,ATT_WIDTH);
    src_buf_h = hwc_get_handle_attibute(fb_handle,ATT_HEIGHT);
    src_buf_stride = hwc_get_handle_attibute(fb_handle,ATT_STRIDE);
    src_buf_format = hwc_get_handle_attibute(fb_handle,ATT_FORMAT);
#else
    src_buf_w = hwc_get_handle_width(fb_handle);
    src_buf_h = hwc_get_handle_height(fb_handle);
    src_buf_stride = hwc_get_handle_stride(fb_handle);
    src_buf_format = hwc_get_handle_format(fb_handle);
#endif

    dst_format = hwc_get_handle_attibute(rgaBuffer.buffer()->handle,ATT_FORMAT);

    src_l = 0;
    src_t = 0;
    dst_l = 0;
    dst_t = 0;
    src_w = ebc_buf_info.width - (ebc_buf_info.width % 8);
    src_h = ebc_buf_info.height - (ebc_buf_info.height % 2);
    dst_w = ebc_buf_info.width - (ebc_buf_info.width % 8);
    dst_h = ebc_buf_info.height - (ebc_buf_info.height % 2);

    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    dst_stride = rgaBuffer.buffer()->getStride();

    src.sync_mode = RGA_BLIT_SYNC;
    rga_set_rect(&src.rect,
                src_l, src_t, src_w, src_h,
                src_buf_stride, src_buf_h, src_buf_format);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w, dst_h, dst_w, dst_h, dst_format);

    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale : src hnd=%p,dst hnd=%p, format=0x%x, transform=0x%x\n",
        (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle), dst_format, rga_transform);

    src.hnd = fb_handle;
    dst.hnd = rgaBuffer.buffer()->handle;
    src.rotation = rga_transform;

    RockchipRga& rkRga(RockchipRga::get());
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst hnd=%p",
            strerror(errno), (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle));
    }

    DumpLayer("rga", dst.hnd);

    return ret;
}

int EinkCompositorWorker::Rgba888ToGray16ByRga(int *output_buffer, const buffer_handle_t &fb_handle, int epd_mode) {
    ATRACE_CALL();
    int ret = 0;
    int rga_transform = 0;
    int src_l,src_t,src_w,src_h;
    int dst_l,dst_t,dst_r,dst_b;

    int dst_w,dst_h,dst_stride;
    int src_buf_w,src_buf_h,src_buf_stride,src_buf_format;
    rga_info_t src, dst;
    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));
    src.fd = -1;
    dst.fd = -1;

    int *src_vir = NULL;
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
    src_buf_w = hwc_get_handle_attibute(fb_handle,ATT_WIDTH);
    src_buf_h = hwc_get_handle_attibute(fb_handle,ATT_HEIGHT);
    src_buf_stride = hwc_get_handle_attibute(fb_handle,ATT_STRIDE);
    src_buf_format = hwc_get_handle_attibute(fb_handle,ATT_FORMAT);
#else
    src_buf_w = hwc_get_handle_width(fb_handle);
    src_buf_h = hwc_get_handle_height(fb_handle);
    src_buf_stride = hwc_get_handle_stride(fb_handle);
    src_buf_format = hwc_get_handle_format(fb_handle);
#endif

    src_vir = NULL;
    ret = hwc_lock(fb_handle, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                  0, 0, src_buf_w, src_buf_h, (void **)&src_vir);
    if(ret || src_vir == NULL){
      ALOGE("Failed to lock rga buffer, rga_output_addr =%p, ret=%d", src_vir, ret);
      return ret;
    }

    src_l = 0;
    src_t = 0;
    src_w = ebc_buf_info.width - (ebc_buf_info.width % 8);
    src_h = ebc_buf_info.height - (ebc_buf_info.height % 2);


    dst_l = 0;
    dst_t = 0;
    dst_w = ebc_buf_info.width - (ebc_buf_info.width % 8);
    dst_h = ebc_buf_info.height - (ebc_buf_info.height % 2);


    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    src.sync_mode = RGA_BLIT_SYNC;
    rga_set_rect(&src.rect,
                src_l, src_t, src_w, src_h,
                src_buf_stride, src_buf_h, src_buf_format);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w, dst_h, dst_w, dst_h, RK_FORMAT_Y4);

    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale : src hnd=%p,dst vir=%p, format=0x%x, transform=0x%x\n",
        (void*)fb_handle, (void*)(output_buffer), HAL_PIXEL_FORMAT_RGBA_8888, rga_transform);

    //src.hnd = fb_handle;
    src.virAddr = src_vir;
    dst.virAddr = output_buffer;
    dst.mmuFlag = 1;
    src.mmuFlag = 1;
    src.rotation = rga_transform;
    dst.color_space_mode = 0x1 << 2;
		dst.dither.enable = 0;
		dst.dither.mode = 0;

    //A2,DU only support two greys(f,0), DU4 support greys(f,a,5,0), others support 16 greys
    uint64_t contrast_key =0xfedcba9876543210;
    if ((epd_mode == EPD_A2) || (epd_mode == EPD_DU) || (epd_mode == EPD_AUTO_DU)) {
        contrast_key = 0xffffff0000000000;
    } else if ((epd_mode == EPD_DU4) || (epd_mode == EPD_AUTO_DU4)) {
        contrast_key = 0xfffffaaa55500000;
    }
//使用新的对比度调节方法debug.sf.gamma.gamma，不再使用persist.vendor.hwc.contrast_key属性
/*
    else {
        char value[PROPERTY_VALUE_MAX];
        property_get("persist.vendor.hwc.contrast_key",value,"0xffccba9876540000");
        sscanf(value,"%" PRIx64,&contrast_key);
    }
*/
    dst.dither.lut0_l = (contrast_key & 0xffff);
    dst.dither.lut0_h = (contrast_key & 0xffff0000) >> 16;
    dst.dither.lut1_l = (contrast_key & 0xffff00000000) >> 32;
    dst.dither.lut1_h = (contrast_key & 0xffff000000000000) >> 48;

    RockchipRga& rkRga(RockchipRga::get());
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst vir=%p",
            strerror(errno), (void*)fb_handle, (void*)(output_buffer));
    }

    DumpLayer("rga", dst.hnd);

    if(src_vir != NULL){
      hwc_unlock(fb_handle);
      src_vir = NULL;
    }


    return ret;
}

int EinkCompositorWorker::Rgba888ToGray256ByRga(DrmRgaBuffer &rgaBuffer,const buffer_handle_t          &fb_handle) {
    ATRACE_CALL();
    int ret = 0;
    int rga_transform = 0;
    int src_l,src_t,src_w,src_h;
    int dst_l,dst_t,dst_r,dst_b;

    int dst_w,dst_h,dst_stride;
    int src_buf_w,src_buf_h,src_buf_stride,src_buf_format;
    rga_info_t src, dst;
    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));
    src.fd = -1;
    dst.fd = -1;

#if (!RK_PER_MODE && RK_DRM_GRALLOC)
    src_buf_w = hwc_get_handle_attibute(fb_handle,ATT_WIDTH);
    src_buf_h = hwc_get_handle_attibute(fb_handle,ATT_HEIGHT);
    src_buf_stride = hwc_get_handle_attibute(fb_handle,ATT_STRIDE);
    src_buf_format = hwc_get_handle_attibute(fb_handle,ATT_FORMAT);
#else
    src_buf_w = hwc_get_handle_width(fb_handle);
    src_buf_h = hwc_get_handle_height(fb_handle);
    src_buf_stride = hwc_get_handle_stride(fb_handle);
    src_buf_format = hwc_get_handle_format(fb_handle);
#endif

    src_l = 0;
    src_t = 0;
    src_w = ebc_buf_info.width - (ebc_buf_info.width % 8);
    src_h = ebc_buf_info.height - (ebc_buf_info.height % 2);


    dst_l = 0;
    dst_t = 0;
    dst_w = ebc_buf_info.width - (ebc_buf_info.width % 8);
    dst_h = ebc_buf_info.height - (ebc_buf_info.height % 2);


    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    dst_stride = rgaBuffer.buffer()->getStride();

    src.sync_mode = RGA_BLIT_SYNC;
    rga_set_rect(&src.rect,
                src_l, src_t, src_w, src_h,
                src_buf_stride, src_buf_h, src_buf_format);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w, dst_h, dst_w, dst_h, HAL_PIXEL_FORMAT_YCrCb_NV12);

    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale : src hnd=%p,dst hnd=%p, format=0x%x, transform=0x%x\n",
        (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle), HAL_PIXEL_FORMAT_RGBA_8888, rga_transform);

    src.hnd = fb_handle;
    dst.hnd = rgaBuffer.buffer()->handle;
    dst.color_space_mode = 0x1 << 2;
    src.rotation = rga_transform;

    RockchipRga& rkRga(RockchipRga::get());
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst hnd=%p",
            strerror(errno), (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle));
    }
    DumpLayer("rga", dst.hnd);


    return ret;
}


int EinkCompositorWorker::RgaClipGrayRect(DrmRgaBuffer &rgaBuffer,const buffer_handle_t       &fb_handle) {
    ATRACE_CALL();

    int ret = 0;
    int rga_transform = 0;
    int src_l,src_t,src_w,src_h;
    int dst_l,dst_t,dst_r,dst_b;

    int dst_w,dst_h,dst_stride;
    int src_buf_w,src_buf_h,src_buf_stride,src_buf_format;
    rga_info_t src, dst;
    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));
    src.fd = -1;
    dst.fd = -1;

#if (!RK_PER_MODE && RK_DRM_GRALLOC)
    src_buf_w = hwc_get_handle_attibute(fb_handle,ATT_WIDTH);
    src_buf_h = hwc_get_handle_attibute(fb_handle,ATT_HEIGHT);
    src_buf_stride = hwc_get_handle_attibute(fb_handle,ATT_STRIDE);
    src_buf_format = hwc_get_handle_attibute(fb_handle,ATT_FORMAT);
#else
    src_buf_w = hwc_get_handle_width(fb_handle);
    src_buf_h = hwc_get_handle_height(fb_handle);
    src_buf_stride = hwc_get_handle_stride(fb_handle);
    src_buf_format = hwc_get_handle_format(fb_handle);
#endif

    src_l = 0;
    src_t = 0;
    src_w = ebc_buf_info.width - (ebc_buf_info.width % 8);
    src_h = ebc_buf_info.height - (ebc_buf_info.height % 2);


    dst_l = 0;
    dst_t = 0;
    dst_w = ebc_buf_info.width - (ebc_buf_info.width % 8);
    dst_h = ebc_buf_info.height - (ebc_buf_info.height % 2);


    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    dst_stride = rgaBuffer.buffer()->getStride();

    src.sync_mode = RGA_BLIT_SYNC;
    rga_set_rect(&src.rect,
                src_l, src_t, src_w / 8, src_h,
                src_buf_stride, src_buf_h, src_buf_format);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w / 8, dst_h, dst_w / 8, dst_h, HAL_PIXEL_FORMAT_RGBA_8888);
    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale : src hnd=%p,dst hnd=%p, format=0x%x, transform=0x%x\n",
        (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle), HAL_PIXEL_FORMAT_RGBA_8888, rga_transform);

    src.hnd = fb_handle;
    dst.hnd = rgaBuffer.buffer()->handle;
    src.rotation = rga_transform;

    RockchipRga& rkRga(RockchipRga::get());
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst hnd=%p",
            strerror(errno), (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle));
    }
    DumpLayer("rga", dst.hnd);

    return ret;
}

int EinkCompositorWorker::DumpEinkSurface(int *buffer){

  char value[PROPERTY_VALUE_MAX];
  property_get("debug.dump", value, "0");
  int new_value = 0;
  new_value = atoi(value);
  if(new_value > 0){
      char data_name[100] ;
      static int DumpSurfaceCount = 0;

      sprintf(data_name,"/data/dump/dmlayer%d_%d_%d.bin", DumpSurfaceCount,
               ebc_buf_info.width, ebc_buf_info.height);
      DumpSurfaceCount++;
      FILE *file = fopen(data_name, "wb+");
      if (!file)
      {
          ALOGW("Could not open %s\n",data_name);
      } else{
          ALOGW("open %s and write ok\n",data_name);
          fwrite(buffer, ebc_buf_info.height * ebc_buf_info.width >> 1 , 1, file);
          fclose(file);

      }
      if(DumpSurfaceCount > 20){
          property_set("debug.dump","0");
          DumpSurfaceCount = 0;
      }
  }
  return 0;
}
int EinkCompositorWorker::PostEink(int *buffer, Rect rect, int mode){
  ATRACE_CALL();

  DumpEinkSurface(buffer);

  commit_buf_info.win_x1 = rect.left;
  commit_buf_info.win_x2 = rect.right;
  commit_buf_info.win_y1 = rect.top;
  commit_buf_info.win_y2 = rect.bottom;
  commit_buf_info.epd_mode = mode;
  commit_buf_info.needpic = 16;

  ALOGD_IF(log_level(DBG_DEBUG),"%s, line = %d ,mode = %d, (x1,x2,y1,y2) = (%d,%d,%d,%d) ",
            __FUNCTION__,__LINE__,mode,commit_buf_info.win_x1,commit_buf_info.win_x2,
            commit_buf_info.win_y1,commit_buf_info.win_y2);

  if(ioctl(ebc_fd, EBC_SEND_BUFFER,&commit_buf_info)!=0)
  {
     ALOGE("EBC_SEND_BUFFER failed\n");
     return -1;
  }

  if(ioctl(ebc_fd, EBC_GET_BUFFER,&commit_buf_info)!=0)
  {
     ALOGE("EBC_GET_BUFFER failed\n");
    return -1;
  }

  unsigned long vaddr_real = intptr_t(ebc_buffer_base);
  gray16_buffer = (int*)(vaddr_real + commit_buf_info.offset);

  return 0;
}

int EinkCompositorWorker::PostEinkY8(int *buffer, Rect rect, int mode){
  ATRACE_CALL();

  DumpEinkSurface(buffer);

  commit_buf_info.win_x1 = rect.left;
  commit_buf_info.win_x2 = rect.right;
  commit_buf_info.win_y1 = rect.top;
  commit_buf_info.win_y2 = rect.bottom;
  commit_buf_info.epd_mode = mode;
  commit_buf_info.needpic = 32;

  ALOGD_IF(log_level(DBG_DEBUG),"%s, line = %d ,mode = %d, (x1,x2,y1,y2) = (%d,%d,%d,%d) ",
            __FUNCTION__,__LINE__,mode,commit_buf_info.win_x1,commit_buf_info.win_x2,
            commit_buf_info.win_y1,commit_buf_info.win_y2);


  if(ioctl(ebc_fd, EBC_SEND_BUFFER,&commit_buf_info)!=0)
  {
     ALOGE("EBC_SEND_BUFFER failed\n");
     return -1;
  }

  if(ioctl(ebc_fd, EBC_GET_BUFFER,&commit_buf_info)!=0)
  {
     ALOGE("EBC_GET_BUFFER failed\n");
    return -1;
  }

  unsigned long vaddr_real = intptr_t(ebc_buffer_base);
  gray16_buffer = (int*)(vaddr_real + commit_buf_info.offset);

  return 0;
}


static int not_fullmode_num = 500;
static int curr_not_fullmode_num = -1;

int EinkCompositorWorker::ConvertToColorEink2(const buffer_handle_t &fb_handle){

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  char *gray256_addr = NULL;

  char* framebuffer_base = NULL;
  int framebuffer_wdith, framebuffer_height, output_format, ret;

  output_format = hwc_get_handle_attibute(fb_handle,ATT_FORMAT);

  if (ebc_buf_info.panel_color == 2) {
    framebuffer_wdith = ebc_buf_info.width;
    framebuffer_height = ebc_buf_info.height;
  } else{
    return -1;
  }

  DumpLayer("rgba", fb_handle);

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(src_hnd,ATT_SIZE);

  ret = Rgba8888ClipRgba(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  framebuffer_base = NULL;
  ret = hwc_lock(src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&framebuffer_base);
  if(ret || framebuffer_base == NULL){
    ALOGE("Failed to lock rga buffer, rga_output_addr =%p, ret=%d", framebuffer_base, ret);
    return ret;
  }

  if(output_format == HAL_PIXEL_FORMAT_RGBA_8888)
    Rgb888_to_color_eink2((char*)gray16_buffer,(int*)(framebuffer_base),height,width,ebc_buf_info.width);
  else if(output_format == HAL_PIXEL_FORMAT_RGB_565)
    Rgb565_to_color_eink2((char*)gray16_buffer,(int16_t*)(framebuffer_base),height,width,ebc_buf_info.width);

  if(framebuffer_base != NULL){
    hwc_unlock(src_hnd);
    framebuffer_base = NULL;
  }

  return 0;
}

int EinkCompositorWorker::ConvertToColorEink1(const buffer_handle_t &fb_handle){

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  char *gray256_addr = NULL;

  char* framebuffer_base = NULL;
  int framebuffer_wdith, framebuffer_height, output_format, ret;

  output_format = hwc_get_handle_attibute(fb_handle,ATT_FORMAT);

  if (ebc_buf_info.panel_color == 1) {
    framebuffer_wdith = ebc_buf_info.width - (ebc_buf_info.width % 8);
    framebuffer_height = ebc_buf_info.height - (ebc_buf_info.height % 2);
  } else{
    return -1;
  }

  DumpLayer("rgba", fb_handle);

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(src_hnd,ATT_SIZE);

  ret = Rgba8888ClipRgba(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  framebuffer_base = NULL;
  ret = hwc_lock(src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&framebuffer_base);
  if(ret || framebuffer_base == NULL){
    ALOGE("Failed to lock rga buffer, rga_output_addr =%p, ret=%d", framebuffer_base, ret);
    return ret;
  }

  if(output_format == HAL_PIXEL_FORMAT_RGBA_8888) {
    image_to_cfa_grayscale_gen2_ARGBB8888(width, height, (unsigned char *)framebuffer_base, (unsigned char *)gray256_new_buffer);
    gray256_to_gray16_dither((char *)gray256_new_buffer, gray16_buffer, height, width, ebc_buf_info.width);
  }
  if(framebuffer_base != NULL){
    hwc_unlock(src_hnd);
    framebuffer_base = NULL;
  }

  return 0;
}

static void do_gray256_buffer(uint32_t *buffer_in, uint32_t *buffer_out, int width, int height)
{
	uint32_t src_data;
	uint32_t *src = buffer_in;
	uint32_t *dst = buffer_out;

	for(int i = 0; i < height; i++) {
		for(int j = 0; j< width/4; j++) {
			src_data = *src++;
			src_data &= 0xf0f0f0f0;
			*dst++ = src_data;
		}
	}
}

int EinkCompositorWorker::InToOrOutY8Regal(const buffer_handle_t &fb_handle) {

  DumpLayer("rgba", fb_handle);

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  int framebuffer_wdith, framebuffer_height, output_format, ret;
  framebuffer_wdith = ebc_buf_info.width - (ebc_buf_info.width % 8);
  framebuffer_height = ebc_buf_info.height - (ebc_buf_info.height % 2);
  output_format = HAL_PIXEL_FORMAT_YCrCb_NV12;

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(src_hnd,ATT_SIZE);

  ret = Rgba888ToGray256ByRga(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  rga_output_addr = NULL;
  ret = hwc_lock(src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&rga_output_addr);
  if(ret || rga_output_addr == NULL){
    ALOGE("Failed to lock rga buffer, rga_output_addr =%p, ret=%d", rga_output_addr, ret);
    return ret;
  }
  do_gray256_buffer((uint32_t *)rga_output_addr, (uint32_t *)gray16_buffer, ebc_buf_info.width, ebc_buf_info.height);
  memcpy(gray256_old_buffer, gray16_buffer, ebc_buf_info.width * ebc_buf_info.height);

  if(rga_output_addr != NULL){
    hwc_unlock(src_hnd);
    rga_output_addr = NULL;
  }
  return 0;
}

int EinkCompositorWorker::ConvertToY8Regal(const buffer_handle_t &fb_handle) {

  DumpLayer("rgba", fb_handle);

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  int framebuffer_wdith, framebuffer_height, output_format, ret;
  framebuffer_wdith = ebc_buf_info.width - (ebc_buf_info.width % 8);
  framebuffer_height = ebc_buf_info.height - (ebc_buf_info.height % 2);
  output_format = HAL_PIXEL_FORMAT_YCrCb_NV12;

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(src_hnd,ATT_SIZE);

  ret = Rgba888ToGray256ByRga(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  rga_output_addr = NULL;
  ret = hwc_lock(src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&rga_output_addr);
  if(ret || rga_output_addr == NULL){
    ALOGE("Failed to lock rga buffer, rga_output_addr =%p, ret=%d", rga_output_addr, ret);
    return ret;
  }
  do_gray256_buffer((uint32_t *)rga_output_addr, (uint32_t *)gray16_buffer, ebc_buf_info.width, ebc_buf_info.height);
  eink_process((uint8_t *)gray16_buffer, (uint8_t *)gray256_old_buffer, ebc_buf_info.width, ebc_buf_info.height);
  memcpy(gray256_old_buffer, gray16_buffer, ebc_buf_info.width * ebc_buf_info.height);

  if(rga_output_addr != NULL){
    hwc_unlock(src_hnd);
    rga_output_addr = NULL;
  }
  return 0;
}

int EinkCompositorWorker::ConvertToY4Dither(const buffer_handle_t &fb_handle, int epd_mode) {

  DumpLayer("rgba", fb_handle);

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);


  rgba_to_y4_by_rga = hwc_get_int_property("sys.eink.rgba2y4_by_rga","0") > 0;

  if(rgba_to_y4_by_rga){
    int ret = Rgba888ToGray16ByRga(gray16_buffer, fb_handle, epd_mode);
    if (ret) {
      ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
      return ret;
    }
    return 0;
  }else{
    char *gray256_addr = NULL;
    int framebuffer_wdith, framebuffer_height, output_format, ret;
    framebuffer_wdith = ebc_buf_info.width - (ebc_buf_info.width % 8);
    framebuffer_height = ebc_buf_info.height - (ebc_buf_info.height % 2);
    output_format = HAL_PIXEL_FORMAT_YCrCb_NV12;

    DrmRgaBuffer &rga_buffer = rgaBuffers[0];
    if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
      ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
      return -ENOMEM;
    }

    int width,height,stride,byte_stride,format,size;
    buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

    width = hwc_get_handle_attibute(src_hnd,ATT_WIDTH);
    height = hwc_get_handle_attibute(src_hnd,ATT_HEIGHT);
    stride = hwc_get_handle_attibute(src_hnd,ATT_STRIDE);
    byte_stride = hwc_get_handle_attibute(src_hnd,ATT_BYTE_STRIDE);
    format = hwc_get_handle_attibute(src_hnd,ATT_FORMAT);
    size = hwc_get_handle_attibute(src_hnd,ATT_SIZE);

    ret = Rgba888ToGray256ByRga(rga_buffer, fb_handle);
    if (ret) {
      ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
      return ret;
    }
    rga_output_addr = NULL;
    ret = hwc_lock(src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                  0, 0, width, height, (void **)&rga_output_addr);
    if(ret || rga_output_addr == NULL){
      ALOGE("Failed to lock rga buffer, rga_output_addr =%p, ret=%d", rga_output_addr, ret);
      return ret;
    }

    gray256_addr = rga_output_addr;
    gray256_to_gray16_dither(gray256_addr,gray16_buffer,ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width);

    if(rga_output_addr != NULL){
      hwc_unlock(src_hnd);
      rga_output_addr = NULL;
    }

  }
  return 0;
}

int EinkCompositorWorker::ConvertToY1Dither(const buffer_handle_t &fb_handle) {

  DumpLayer("rgba", fb_handle);

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  char *gray256_addr = NULL;
  int framebuffer_wdith, framebuffer_height, output_format, ret;
  framebuffer_wdith = ebc_buf_info.width - (ebc_buf_info.width % 8);
  framebuffer_height = ebc_buf_info.height - (ebc_buf_info.height % 2);
  output_format = HAL_PIXEL_FORMAT_YCrCb_NV12;

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(src_hnd,ATT_SIZE);

  ret = Rgba888ToGray256ByRga(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  rga_output_addr=NULL;
  hwc_lock(src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&rga_output_addr);
  if(ret || rga_output_addr == NULL){
    ALOGE("Failed to lock rga buffer, rga_output_addr =%p, ret=%d", rga_output_addr, ret);
    return ret;
  }

  gray256_addr = rga_output_addr;
  if(rga_output_addr == NULL)
    ALOGE("rga_output_addr == NULL, hwc_lock maybe failed\n");

  Region screen_region(Rect(0, 0, ebc_buf_info.width - 1, ebc_buf_info.height -1));
  gray256_to_gray2_dither(gray256_addr,(char *)gray16_buffer,ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width,screen_region);


  if(rga_output_addr != NULL){
    hwc_unlock(src_hnd);
    rga_output_addr = NULL;
  }
  return 0;
}

int EinkCompositorWorker::ColorCommit(int epd_mode) {
  Rect screen_rect = Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);
  int *gray16_buffer_bak = gray16_buffer;
  PostEink(gray16_buffer_bak, screen_rect, epd_mode);
  gLastEpdMode = epd_mode;
  return 0;
}

int EinkCompositorWorker::EinkCommit(int epd_mode) {
  Rect screen_rect = Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);
  int *gray256_new_buffer_bak = gray16_buffer;
  PostEinkY8(gray256_new_buffer_bak, screen_rect, epd_mode);
  gLastEpdMode = epd_mode;
  return 0;
}
 
int EinkCompositorWorker::Y4Commit(int epd_mode) {
  Rect screen_rect = Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);
  int *gray16_buffer_bak = gray16_buffer;
  PostEink(gray16_buffer_bak, screen_rect, epd_mode);
  gLastEpdMode = epd_mode;
  return 0;
}

int EinkCompositorWorker::A2Commit(int epd_mode) {
  int epd_tmp_mode = EPD_NULL;
  Rect screen_rect = Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);
  int *gray16_buffer_bak = gray16_buffer;
  if((gLastEpdMode != EPD_A2) && (gLastEpdMode != EPD_A2_DITHER))
      epd_tmp_mode = EPD_A2_ENTER;
  else
      epd_tmp_mode = epd_mode;
  PostEink(gray16_buffer_bak, screen_rect, epd_tmp_mode);
  gLastEpdMode = epd_mode;
  return 0;
}

int EinkCompositorWorker::update_fullmode_num(){
  char value[PROPERTY_VALUE_MAX];
  property_get("persist.vendor.fullmode_cnt",value,"500");

  not_fullmode_num = atoi(value);
  if (not_fullmode_num != curr_not_fullmode_num) {
    if(ioctl(ebc_fd, EBC_SET_FULL_MODE_NUM, &not_fullmode_num) != 0) {
        ALOGE("EBC_SET_FULL_MODE_NUM failed\n");
        return -1;
    }
    curr_not_fullmode_num = not_fullmode_num;
  }
  return 0;
}

int EinkCompositorWorker::SetColorEinkMode(EinkComposition *composition) {
  ATRACE_CALL();

  if(!composition){
    ALOGE("%s,line=%d composition is null",__FUNCTION__,__LINE__);
    return -1;
  }

  switch(composition->einkMode){
    case EPD_SUSPEND:
       // release_wake_lock("show_advt_lock");
      break;
    default:
	if (ebc_buf_info.panel_color == 1)
        ConvertToColorEink1(composition->fb_handle);
	else
        ConvertToColorEink2(composition->fb_handle);
      Y4Commit(composition->einkMode);
      break;
  }
  update_fullmode_num();

  return 0;
}

int EinkCompositorWorker::SetEinkMode(EinkComposition *composition) {
  ATRACE_CALL();

  if(!composition){
    ALOGE("%s,line=%d composition is null",__FUNCTION__,__LINE__);
    return -1;
  }

  if (last_regal) {
  	if (composition->einkMode != EPD_FULL_GLD16
		&& composition->einkMode != EPD_FULL_GLR16
		&& composition->einkMode != EPD_PART_GLD16
		&& composition->einkMode != EPD_PART_GLR16) {
  		last_regal = !last_regal;
  	}
  }

  switch(composition->einkMode){
    case EPD_A2_DITHER:
      ConvertToY1Dither(composition->fb_handle);
      A2Commit(EPD_A2_DITHER);
      break;
    case EPD_A2:
      ConvertToY4Dither(composition->fb_handle, composition->einkMode);
      A2Commit(EPD_A2);
      break;
    case EPD_SUSPEND:
       // release_wake_lock("show_advt_lock");
      break;
    case EPD_FULL_GLD16:
    case EPD_FULL_GLR16:
    case EPD_PART_GLD16:
    case EPD_PART_GLR16:
      if (waveform_fd > 0) {
	   if (last_regal) {
          	ConvertToY8Regal(composition->fb_handle);
          	EinkCommit(composition->einkMode);
	   } else {
	       last_regal = !last_regal;
		InToOrOutY8Regal(composition->fb_handle);
          	EinkCommit(EPD_FORCE_FULL);
	   }
          break;
      }
      FALLTHROUGH_INTENDED;
    default:
      ConvertToY4Dither(composition->fb_handle, composition->einkMode);
      Y4Commit(composition->einkMode);
      break;
  }
  update_fullmode_num();

  return 0;
}

void EinkCompositorWorker::Compose(
    std::unique_ptr<EinkComposition> composition) {
    ATRACE_CALL();
  if (!composition.get())
    return;

  int ret;
  int outbuf_acquire_fence = composition->outbuf_acquire_fence.get();
  if (outbuf_acquire_fence >= 0) {
    ret = sync_wait(outbuf_acquire_fence, kAcquireWaitTimeoutMs);
    if (ret) {
      ALOGE("Failed to wait for outbuf acquire %d/%d", outbuf_acquire_fence,
            ret);
      return;
    }
    composition->outbuf_acquire_fence.Close();
  }
  for (size_t i = 0; i < composition->layer_acquire_fences.size(); ++i) {
    int layer_acquire_fence = composition->layer_acquire_fences[i].get();
    if (layer_acquire_fence >= 0) {
      ret = sync_wait(layer_acquire_fence, kAcquireWaitTimeoutMs);
      if (ret) {
        ALOGE("Failed to wait for layer acquire %d/%d", layer_acquire_fence,
              ret);
        return;
      }
      composition->layer_acquire_fences[i].Close();
    }
  }
  if(isSupportRkRga()){
    if(ebc_buf_info.panel_color)
      ret = SetColorEinkMode(composition.get());
    else
      ret = SetEinkMode(composition.get());
    if (ret){
      for(int i = 0; i < MaxRgaBuffers; i++) {
        rgaBuffers[i].Clear();
      }
      return;
    }
  }
  FinishComposition(composition->release_timeline);
}
}
