/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
 * Copyright (C) 2010-2011 LunarG Inc.
 *
 * Based on platform_x11, which has
 *
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <cutils/properties.h>
#include <errno.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <stdbool.h>
#include <stdio.h>
#include <sync/sync.h>
#include <sys/types.h>
#include <drm-uapi/drm_fourcc.h>

#include "util/os_file.h"

#include "loader.h"
#include "egl_dri2.h"

#ifdef HAVE_DRM_GRALLOC
#include <gralloc_drm_handle.h>
#include "gralloc_drm.h"
#endif /* HAVE_DRM_GRALLOC */

#define ALIGN(val, align)	(((val) + (align) - 1) & ~((align) - 1))

enum chroma_order {
   YCbCr,
   YCrCb,
};

struct droid_yuv_format {
   /* Lookup keys */
   int native; /* HAL_PIXEL_FORMAT_ */
   enum chroma_order chroma_order; /* chroma order is {Cb, Cr} or {Cr, Cb} */
   int chroma_step; /* Distance in bytes between subsequent chroma pixels. */

   /* Result */
   int fourcc; /* DRM_FORMAT_ */
};

/* The following table is used to look up a DRI image FourCC based
 * on native format and information contained in android_ycbcr struct. */
static const struct droid_yuv_format droid_yuv_formats[] = {
   /* Native format, YCrCb, Chroma step, DRI image FourCC */
   { HAL_PIXEL_FORMAT_YCbCr_420_888, YCbCr, 2, DRM_FORMAT_NV12 },
   { HAL_PIXEL_FORMAT_YCbCr_420_888, YCbCr, 1, DRM_FORMAT_YUV420 },
   { HAL_PIXEL_FORMAT_YCbCr_420_888, YCrCb, 1, DRM_FORMAT_YVU420 },
   { HAL_PIXEL_FORMAT_YV12,          YCrCb, 1, DRM_FORMAT_YVU420 },
   /* HACK: See droid_create_image_from_prime_fds() and
    * https://issuetracker.google.com/32077885. */
   { HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, YCbCr, 2, DRM_FORMAT_NV12 },
   { HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, YCbCr, 1, DRM_FORMAT_YUV420 },
   { HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, YCrCb, 1, DRM_FORMAT_YVU420 },
   { HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, YCrCb, 1, DRM_FORMAT_AYUV },
   { HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, YCrCb, 1, DRM_FORMAT_XYUV8888 },
};

static int
get_fourcc_yuv(int native, enum chroma_order chroma_order, int chroma_step)
{
   for (int i = 0; i < ARRAY_SIZE(droid_yuv_formats); ++i)
      if (droid_yuv_formats[i].native == native &&
          droid_yuv_formats[i].chroma_order == chroma_order &&
          droid_yuv_formats[i].chroma_step == chroma_step)
         return droid_yuv_formats[i].fourcc;

   return -1;
}

static bool
is_yuv(int native)
{
   for (int i = 0; i < ARRAY_SIZE(droid_yuv_formats); ++i)
      if (droid_yuv_formats[i].native == native)
         return true;

   return false;
}

static int
get_format_bpp(int native)
{
   int bpp;

   switch (native) {
   case HAL_PIXEL_FORMAT_RGBA_FP16:
      bpp = 8;
      break;
   case HAL_PIXEL_FORMAT_RGBA_8888:
   case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      /*
       * HACK: Hardcode this to RGBX_8888 as per cros_gralloc hack.
       * TODO: Remove this once https://issuetracker.google.com/32077885 is fixed.
       */
   case HAL_PIXEL_FORMAT_RGBX_8888:
   case HAL_PIXEL_FORMAT_BGRA_8888:
   case HAL_PIXEL_FORMAT_RGBA_1010102:
      bpp = 4;
      break;
   case HAL_PIXEL_FORMAT_RGB_565:
      bpp = 2;
      break;
   default:
      bpp = 0;
      break;
   }

   return bpp;
}

/* createImageFromFds requires fourcc format */
static int get_fourcc(int native)
{
   switch (native) {
   case HAL_PIXEL_FORMAT_RGB_565:   return DRM_FORMAT_RGB565;
   case HAL_PIXEL_FORMAT_BGRA_8888: return DRM_FORMAT_ARGB8888;
   case HAL_PIXEL_FORMAT_RGBA_8888: return DRM_FORMAT_ABGR8888;
   case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      /*
       * HACK: Hardcode this to RGBX_8888 as per cros_gralloc hack.
       * TODO: Remove this once https://issuetracker.google.com/32077885 is fixed.
       */
   case HAL_PIXEL_FORMAT_RGBX_8888: return DRM_FORMAT_XBGR8888;
   case HAL_PIXEL_FORMAT_RGBA_FP16: return DRM_FORMAT_ABGR16161616F;
   case HAL_PIXEL_FORMAT_RGBA_1010102: return DRM_FORMAT_ABGR2101010;
   default:
      _eglLog(_EGL_WARNING, "unsupported native buffer format 0x%x", native);
   }
   return -1;
}

/* returns # of fds, and by reference the actual fds */
static unsigned
get_native_buffer_fds(struct ANativeWindowBuffer *buf, int fds[3])
{
   native_handle_t *handle = (native_handle_t *)buf->handle;

   if (!handle)
      return 0;

   /*
    * Various gralloc implementations exist, but the dma-buf fd tends
    * to be first. Access it directly to avoid a dependency on specific
    * gralloc versions.
    */
   for (int i = 0; i < handle->numFds; i++)
      fds[i] = handle->data[i];

   return handle->numFds;
}

#ifdef HAVE_DRM_GRALLOC
static int
get_native_buffer_name(struct ANativeWindowBuffer *buf)
{
   return gralloc_drm_get_gem_handle(buf->handle);
}
#endif /* HAVE_DRM_GRALLOC */

static __DRIimage *
droid_create_image_from_prime_fds_yuv(_EGLDisplay *disp,
                                     struct ANativeWindowBuffer *buf,
                                     int num_fds, int fds[3])
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct android_ycbcr ycbcr;
   int offsets[3];
   int pitches[3];
   enum chroma_order chroma_order;
   int fourcc;
   int ret;
   unsigned error;

   if (!dri2_dpy->gralloc->lock_ycbcr) {
      _eglLog(_EGL_WARNING, "Gralloc does not support lock_ycbcr");
      return NULL;
   }

   memset(&ycbcr, 0, sizeof(ycbcr));
   ret = dri2_dpy->gralloc->lock_ycbcr(dri2_dpy->gralloc, buf->handle,
                                       0, 0, 0, 0, 0, &ycbcr);
   if (ret) {
      /* HACK: See droid_create_image_from_prime_fds() and
       * https://issuetracker.google.com/32077885.*/
      if (buf->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
         return NULL;

      _eglLog(_EGL_WARNING, "gralloc->lock_ycbcr failed: %d", ret);
      return NULL;
   }
   dri2_dpy->gralloc->unlock(dri2_dpy->gralloc, buf->handle);

   /* When lock_ycbcr's usage argument contains no SW_READ/WRITE flags
    * it will return the .y/.cb/.cr pointers based on a NULL pointer,
    * so they can be interpreted as offsets. */
   offsets[0] = (size_t)ycbcr.y;
   /* We assume here that all the planes are located in one DMA-buf. */
   if ((size_t)ycbcr.cr < (size_t)ycbcr.cb) {
      chroma_order = YCrCb;
      offsets[1] = (size_t)ycbcr.cr;
      offsets[2] = (size_t)ycbcr.cb;
   } else {
      chroma_order = YCbCr;
      offsets[1] = (size_t)ycbcr.cb;
      offsets[2] = (size_t)ycbcr.cr;
   }

   /* .ystride is the line length (in bytes) of the Y plane,
    * .cstride is the line length (in bytes) of any of the remaining
    * Cb/Cr/CbCr planes, assumed to be the same for Cb and Cr for fully
    * planar formats. */
   pitches[0] = ycbcr.ystride;
   pitches[1] = pitches[2] = ycbcr.cstride;

   /* .chroma_step is the byte distance between the same chroma channel
    * values of subsequent pixels, assumed to be the same for Cb and Cr. */
   fourcc = get_fourcc_yuv(buf->format, chroma_order, ycbcr.chroma_step);
   if (fourcc == -1) {
      _eglLog(_EGL_WARNING, "unsupported YUV format, native = %x, chroma_order = %s, chroma_step = %d",
              buf->format, chroma_order == YCbCr ? "YCbCr" : "YCrCb", ycbcr.chroma_step);
      return NULL;
   }

   /*
    * Since this is EGL_NATIVE_BUFFER_ANDROID don't assume that
    * the single-fd case cannot happen.  So handle eithe single
    * fd or fd-per-plane case:
    */
   int num_planes = (ycbcr.chroma_step == 2) ? 2 : 3;
   if (num_fds == 1) {
      fds[2] = fds[1] = fds[0];
   } else {
      assert(num_fds == num_planes);
   }

   return dri2_dpy->image->createImageFromDmaBufs(dri2_dpy->dri_screen,
      buf->width, buf->height, fourcc,
      fds, num_planes, pitches, offsets,
      EGL_ITU_REC601_EXT,
      EGL_YUV_NARROW_RANGE_EXT,
      EGL_YUV_CHROMA_SITING_0_EXT,
      EGL_YUV_CHROMA_SITING_0_EXT,
      &error,
      NULL);
}

static __DRIimage *
droid_create_image_from_prime_fds(_EGLDisplay *disp,
                                  struct ANativeWindowBuffer *buf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   int pitches[4] = { 0 }, offsets[4] = { 0 };
   unsigned error;
   int num_fds;
   int fds[3];

   num_fds = get_native_buffer_fds(buf, fds);
   if (num_fds == 0)
      return NULL;

   if (is_yuv(buf->format)) {
      __DRIimage *image;

      image = droid_create_image_from_prime_fds_yuv(disp, buf, num_fds, fds);
      /*
       * HACK: https://issuetracker.google.com/32077885
       * There is no API available to properly query the IMPLEMENTATION_DEFINED
       * format. As a workaround we rely here on gralloc allocating either
       * an arbitrary YCbCr 4:2:0 or RGBX_8888, with the latter being recognized
       * by lock_ycbcr failing.
       */
      if (image || buf->format != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
         return image;
   }

   /*
    * Non-YUV formats could *also* have multiple planes, such as ancillary
    * color compression state buffer, but the rest of the code isn't ready
    * yet to deal with modifiers:
    */
   assert(num_fds == 1);

   const int fourcc = get_fourcc(buf->format);
   if (fourcc == -1) {
      _eglError(EGL_BAD_PARAMETER, "eglCreateEGLImageKHR");
      return NULL;
   }

   pitches[0] = buf->stride * get_format_bpp(buf->format);
   if (pitches[0] == 0) {
      _eglError(EGL_BAD_PARAMETER, "eglCreateEGLImageKHR");
      return NULL;
   }

   return dri2_dpy->image->createImageFromDmaBufs(dri2_dpy->dri_screen,
      buf->width, buf->height, fourcc,
      fds, num_fds, pitches, offsets,
      EGL_ITU_REC601_EXT,
      EGL_YUV_NARROW_RANGE_EXT,
      EGL_YUV_CHROMA_SITING_0_EXT,
      EGL_YUV_CHROMA_SITING_0_EXT,
      &error,
      NULL);
}

/* More recent CrOS gralloc has a perform op that fills out the struct below
 * with canonical information about the buffer and its modifier, planes,
 * offsets and strides.  If we have this, we can skip straight to
 * createImageFromDmaBufs2() and avoid all the guessing and recalculations.
 * This also gives us the modifier and plane offsets/strides for multiplanar
 * compressed buffers (eg Intel CCS buffers) in order to make that work in Android.
 */

static const char cros_gralloc_module_name[] = "CrOS Gralloc";

#define CROS_GRALLOC_DRM_GET_BUFFER_INFO 4

struct cros_gralloc0_buffer_info {
   uint32_t drm_fourcc;
   int num_fds;
   int fds[4];
   uint64_t modifier;
   int offset[4];
   int stride[4];
};

static __DRIimage *
droid_create_image_from_cros_info(_EGLDisplay *disp,
                                  struct ANativeWindowBuffer *buf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct cros_gralloc0_buffer_info info;
   unsigned error;

   if (strcmp(dri2_dpy->gralloc->common.name, cros_gralloc_module_name) == 0 &&
       dri2_dpy->gralloc->perform &&
       dri2_dpy->image->base.version >= 15 &&
       dri2_dpy->image->createImageFromDmaBufs2 != NULL &&
       dri2_dpy->gralloc->perform(dri2_dpy->gralloc,
                                  CROS_GRALLOC_DRM_GET_BUFFER_INFO,
                                  buf->handle, &info) == 0) {
      return dri2_dpy->image->createImageFromDmaBufs2(dri2_dpy->dri_screen,
                                                      buf->width, buf->height,
                                                      info.drm_fourcc, info.modifier,
                                                      info.fds, info.num_fds,
                                                      info.stride, info.offset,
                                                      EGL_ITU_REC601_EXT,
                                                      EGL_YUV_FULL_RANGE_EXT,
                                                      EGL_YUV_CHROMA_SITING_0_EXT,
                                                      EGL_YUV_CHROMA_SITING_0_EXT,
                                                      &error,
                                                      NULL);
   }

   return NULL;
}

static __DRIimage *
droid_create_image_from_native_buffer(_EGLDisplay *disp,
                                      struct ANativeWindowBuffer *buf)
{
   __DRIimage *dri_image;

   dri_image = droid_create_image_from_cros_info(disp, buf);
   if (dri_image)
      return dri_image;

   return droid_create_image_from_prime_fds(disp, buf);
}

static EGLBoolean
droid_window_dequeue_buffer(struct dri2_egl_surface *dri2_surf)
{
   int fence_fd;

   if (dri2_surf->window->dequeueBuffer(dri2_surf->window, &dri2_surf->buffer,
                                        &fence_fd))
      return EGL_FALSE;

   /* If access to the buffer is controlled by a sync fence, then block on the
    * fence.
    *
    * It may be more performant to postpone blocking until there is an
    * immediate need to write to the buffer. But doing so would require adding
    * hooks to the DRI2 loader.
    *
    * From the ANativeWindow::dequeueBuffer documentation:
    *
    *    The libsync fence file descriptor returned in the int pointed to by
    *    the fenceFd argument will refer to the fence that must signal
    *    before the dequeued buffer may be written to.  A value of -1
    *    indicates that the caller may access the buffer immediately without
    *    waiting on a fence.  If a valid file descriptor is returned (i.e.
    *    any value except -1) then the caller is responsible for closing the
    *    file descriptor.
    */
    if (fence_fd >= 0) {
       /* From the SYNC_IOC_WAIT documentation in <linux/sync.h>:
        *
        *    Waits indefinitely if timeout < 0.
        */
        int timeout = -1;
        sync_wait(fence_fd, timeout);
        close(fence_fd);
   }

   /* Record all the buffers created by ANativeWindow and update back buffer
    * for updating buffer's age in swap_buffers.
    */
   EGLBoolean updated = EGL_FALSE;
   for (int i = 0; i < dri2_surf->color_buffers_count; i++) {
      if (!dri2_surf->color_buffers[i].buffer) {
         dri2_surf->color_buffers[i].buffer = dri2_surf->buffer;
      }
      if (dri2_surf->color_buffers[i].buffer == dri2_surf->buffer) {
         dri2_surf->back = &dri2_surf->color_buffers[i];
         updated = EGL_TRUE;
         break;
      }
   }

   if (!updated) {
      /* In case of all the buffers were recreated by ANativeWindow, reset
       * the color_buffers
       */
      for (int i = 0; i < dri2_surf->color_buffers_count; i++) {
         dri2_surf->color_buffers[i].buffer = NULL;
         dri2_surf->color_buffers[i].age = 0;
      }
      dri2_surf->color_buffers[0].buffer = dri2_surf->buffer;
      dri2_surf->back = &dri2_surf->color_buffers[0];
   }

   return EGL_TRUE;
}

static EGLBoolean
droid_window_enqueue_buffer(_EGLDisplay *disp, struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   /* To avoid blocking other EGL calls, release the display mutex before
    * we enter droid_window_enqueue_buffer() and re-acquire the mutex upon
    * return.
    */
   mtx_unlock(&disp->Mutex);

   /* Queue the buffer with stored out fence fd. The ANativeWindow or buffer
    * consumer may choose to wait for the fence to signal before accessing
    * it. If fence fd value is -1, buffer can be accessed by consumer
    * immediately. Consumer or application shouldn't rely on timestamp
    * associated with fence if the fence fd is -1.
    *
    * Ownership of fd is transferred to consumer after queueBuffer and the
    * consumer is responsible for closing it. Caller must not use the fd
    * after passing it to queueBuffer.
    */
   int fence_fd = dri2_surf->out_fence_fd;
   dri2_surf->out_fence_fd = -1;
   dri2_surf->window->queueBuffer(dri2_surf->window, dri2_surf->buffer,
                                  fence_fd);

   dri2_surf->buffer = NULL;
   dri2_surf->back = NULL;

   mtx_lock(&disp->Mutex);

   if (dri2_surf->dri_image_back) {
      dri2_dpy->image->destroyImage(dri2_surf->dri_image_back);
      dri2_surf->dri_image_back = NULL;
   }

   return EGL_TRUE;
}

static void
droid_window_cancel_buffer(struct dri2_egl_surface *dri2_surf)
{
   int ret;
   int fence_fd = dri2_surf->out_fence_fd;

   dri2_surf->out_fence_fd = -1;
   ret = dri2_surf->window->cancelBuffer(dri2_surf->window,
                                         dri2_surf->buffer, fence_fd);
   dri2_surf->buffer = NULL;
   if (ret < 0) {
      _eglLog(_EGL_WARNING, "ANativeWindow::cancelBuffer failed");
      dri2_surf->base.Lost = EGL_TRUE;
   }
}

static bool
droid_set_shared_buffer_mode(_EGLDisplay *disp, _EGLSurface *surf, bool mode)
{
#if ANDROID_API_LEVEL >= 24
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);
   struct ANativeWindow *window = dri2_surf->window;

   assert(surf->Type == EGL_WINDOW_BIT);
   assert(_eglSurfaceHasMutableRenderBuffer(&dri2_surf->base));

   _eglLog(_EGL_DEBUG, "%s: mode=%d", __func__, mode);

   if (native_window_set_shared_buffer_mode(window, mode)) {
      _eglLog(_EGL_WARNING, "failed native_window_set_shared_buffer_mode"
              "(window=%p, mode=%d)", window, mode);
      return false;
   }

   return true;
#else
   _eglLog(_EGL_FATAL, "%s:%d: internal error: unreachable", __FILE__, __LINE__);
   return false;
#endif
}

static _EGLSurface *
droid_create_surface(_EGLDisplay *disp, EGLint type, _EGLConfig *conf,
                     void *native_window, const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   struct ANativeWindow *window = native_window;
   const __DRIconfig *config;

   dri2_surf = calloc(1, sizeof *dri2_surf);
   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "droid_create_surface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_surf->base, disp, type, conf, attrib_list,
                          true, native_window))
      goto cleanup_surface;

   if (type == EGL_WINDOW_BIT) {
      int format;
      int buffer_count;
      int min_buffer_count, max_buffer_count;

      /* Prefer triple buffering for performance reasons. */
      const int preferred_buffer_count = 3;

      if (window->common.magic != ANDROID_NATIVE_WINDOW_MAGIC) {
         _eglError(EGL_BAD_NATIVE_WINDOW, "droid_create_surface");
         goto cleanup_surface;
      }
      if (window->query(window, NATIVE_WINDOW_FORMAT, &format)) {
         _eglError(EGL_BAD_NATIVE_WINDOW, "droid_create_surface");
         goto cleanup_surface;
      }

      /* Query ANativeWindow for MIN_UNDEQUEUED_BUFFER, minimum amount
       * of undequeued buffers.
       */
      if (window->query(window, NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
                        &min_buffer_count)) {
         _eglError(EGL_BAD_NATIVE_WINDOW, "droid_create_surface");
         goto cleanup_surface;
      }

      /* Query for maximum buffer count, application can set this
       * to limit the total amount of buffers.
       */
      if (window->query(window, NATIVE_WINDOW_MAX_BUFFER_COUNT,
                        &max_buffer_count)) {
         _eglError(EGL_BAD_NATIVE_WINDOW, "droid_create_surface");
         goto cleanup_surface;
      }

      /* Clamp preferred between minimum (min undequeued + 1 dequeued)
       * and maximum.
       */
      buffer_count = CLAMP(preferred_buffer_count, min_buffer_count + 1,
                           max_buffer_count);

      if (native_window_set_buffer_count(window, buffer_count)) {
         _eglError(EGL_BAD_NATIVE_WINDOW, "droid_create_surface");
         goto cleanup_surface;
      }
      dri2_surf->color_buffers = calloc(buffer_count,
                                        sizeof(*dri2_surf->color_buffers));
      if (!dri2_surf->color_buffers) {
         _eglError(EGL_BAD_ALLOC, "droid_create_surface");
         goto cleanup_surface;
      }
      dri2_surf->color_buffers_count = buffer_count;

      if (format != dri2_conf->base.NativeVisualID) {
         _eglLog(_EGL_WARNING, "Native format mismatch: 0x%x != 0x%x",
               format, dri2_conf->base.NativeVisualID);
      }

      window->query(window, NATIVE_WINDOW_WIDTH, &dri2_surf->base.Width);
      window->query(window, NATIVE_WINDOW_HEIGHT, &dri2_surf->base.Height);

      uint32_t usage = strcmp(dri2_dpy->driver_name, "kms_swrast") == 0
            ? GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN
            : GRALLOC_USAGE_HW_RENDER;
      native_window_set_usage(window, usage);
   }

   config = dri2_get_dri_config(dri2_conf, type,
                                dri2_surf->base.GLColorspace);
   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_surface;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup_surface;

   if (window) {
      window->common.incRef(&window->common);
      dri2_surf->window = window;
   }

   return &dri2_surf->base;

cleanup_surface:
   if (dri2_surf->color_buffers_count)
      free(dri2_surf->color_buffers);
   free(dri2_surf);

   return NULL;
}

static _EGLSurface *
droid_create_window_surface(_EGLDisplay *disp, _EGLConfig *conf,
                            void *native_window, const EGLint *attrib_list)
{
   return droid_create_surface(disp, EGL_WINDOW_BIT, conf,
                               native_window, attrib_list);
}

static _EGLSurface *
droid_create_pbuffer_surface(_EGLDisplay *disp, _EGLConfig *conf,
                             const EGLint *attrib_list)
{
   return droid_create_surface(disp, EGL_PBUFFER_BIT, conf,
                               NULL, attrib_list);
}

static EGLBoolean
droid_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   dri2_egl_surface_free_local_buffers(dri2_surf);

   if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
      if (dri2_surf->buffer)
         droid_window_cancel_buffer(dri2_surf);

      dri2_surf->window->common.decRef(&dri2_surf->window->common);
   }

   if (dri2_surf->dri_image_back) {
      _eglLog(_EGL_DEBUG, "%s : %d : destroy dri_image_back", __func__, __LINE__);
      dri2_dpy->image->destroyImage(dri2_surf->dri_image_back);
      dri2_surf->dri_image_back = NULL;
   }

   if (dri2_surf->dri_image_front) {
      _eglLog(_EGL_DEBUG, "%s : %d : destroy dri_image_front", __func__, __LINE__);
      dri2_dpy->image->destroyImage(dri2_surf->dri_image_front);
      dri2_surf->dri_image_front = NULL;
   }

   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);

   dri2_fini_surface(surf);
   free(dri2_surf->color_buffers);
   free(dri2_surf);

   return EGL_TRUE;
}

static EGLBoolean
droid_swap_interval(_EGLDisplay *disp, _EGLSurface *surf, EGLint interval)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);
   struct ANativeWindow *window = dri2_surf->window;

   if (window->setSwapInterval(window, interval))
      return EGL_FALSE;

   surf->SwapInterval = interval;
   return EGL_TRUE;
}

static int
update_buffers(struct dri2_egl_surface *dri2_surf)
{
   if (dri2_surf->base.Lost)
      return -1;

   if (dri2_surf->base.Type != EGL_WINDOW_BIT)
      return 0;

   /* try to dequeue the next back buffer */
   if (!dri2_surf->buffer && !droid_window_dequeue_buffer(dri2_surf)) {
      _eglLog(_EGL_WARNING, "Could not dequeue buffer from native window");
      dri2_surf->base.Lost = EGL_TRUE;
      return -1;
   }

   /* free outdated buffers and update the surface size */
   if (dri2_surf->base.Width != dri2_surf->buffer->width ||
       dri2_surf->base.Height != dri2_surf->buffer->height) {
      dri2_egl_surface_free_local_buffers(dri2_surf);
      dri2_surf->base.Width = dri2_surf->buffer->width;
      dri2_surf->base.Height = dri2_surf->buffer->height;
   }

   return 0;
}

static int
get_front_bo(struct dri2_egl_surface *dri2_surf, unsigned int format)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   if (dri2_surf->dri_image_front)
      return 0;

   if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
      /* According current EGL spec, front buffer rendering
       * for window surface is not supported now.
       * and mesa doesn't have the implementation of this case.
       * Add warning message, but not treat it as error.
       */
      _eglLog(_EGL_DEBUG, "DRI driver requested unsupported front buffer for window surface");
   } else if (dri2_surf->base.Type == EGL_PBUFFER_BIT) {
      dri2_surf->dri_image_front =
          dri2_dpy->image->createImage(dri2_dpy->dri_screen,
                                              dri2_surf->base.Width,
                                              dri2_surf->base.Height,
                                              format,
                                              0,
                                              dri2_surf);
      if (!dri2_surf->dri_image_front) {
         _eglLog(_EGL_WARNING, "dri2_image_front allocation failed");
         return -1;
      }
   }

   return 0;
}

static int
get_back_bo(struct dri2_egl_surface *dri2_surf)
{
   _EGLDisplay *disp = dri2_surf->base.Resource.Display;

   if (dri2_surf->dri_image_back)
      return 0;

   if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
      if (!dri2_surf->buffer) {
         _eglLog(_EGL_WARNING, "Could not get native buffer");
         return -1;
      }

      dri2_surf->dri_image_back =
         droid_create_image_from_native_buffer(disp, dri2_surf->buffer);
      if (!dri2_surf->dri_image_back) {
         _eglLog(_EGL_WARNING, "failed to create DRI image from FD");
         return -1;
      }
   } else if (dri2_surf->base.Type == EGL_PBUFFER_BIT) {
      /* The EGL 1.5 spec states that pbuffers are single-buffered. Specifically,
       * the spec states that they have a back buffer but no front buffer, in
       * contrast to pixmaps, which have a front buffer but no back buffer.
       *
       * Single-buffered surfaces with no front buffer confuse Mesa; so we deviate
       * from the spec, following the precedent of Mesa's EGL X11 platform. The
       * X11 platform correctly assigns pbuffers to single-buffered configs, but
       * assigns the pbuffer a front buffer instead of a back buffer.
       *
       * Pbuffers in the X11 platform mostly work today, so let's just copy its
       * behavior instead of trying to fix (and hence potentially breaking) the
       * world.
       */
      _eglLog(_EGL_DEBUG, "DRI driver requested unsupported back buffer for pbuffer surface");
   }

   return 0;
}

/* Some drivers will pass multiple bits in buffer_mask.
 * For such case, will go through all the bits, and
 * will not return error when unsupported buffer is requested, only
 * return error when the allocation for supported buffer failed.
 */
static int
droid_image_get_buffers(__DRIdrawable *driDrawable,
                  unsigned int format,
                  uint32_t *stamp,
                  void *loaderPrivate,
                  uint32_t buffer_mask,
                  struct __DRIimageList *images)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;

   images->image_mask = 0;
   images->front = NULL;
   images->back = NULL;

   if (update_buffers(dri2_surf) < 0)
      return 0;

   if (_eglSurfaceInSharedBufferMode(&dri2_surf->base)) {
      if (get_back_bo(dri2_surf) < 0)
         return 0;

      /* We have dri_image_back because this is a window surface and
       * get_back_bo() succeeded.
       */
      assert(dri2_surf->dri_image_back);
      images->back = dri2_surf->dri_image_back;
      images->image_mask |= __DRI_IMAGE_BUFFER_SHARED;

      /* There exists no accompanying back nor front buffer. */
      return 1;
   }

   if (buffer_mask & __DRI_IMAGE_BUFFER_FRONT) {
      if (get_front_bo(dri2_surf, format) < 0)
         return 0;

      if (dri2_surf->dri_image_front) {
         images->front = dri2_surf->dri_image_front;
         images->image_mask |= __DRI_IMAGE_BUFFER_FRONT;
      }
   }

   if (buffer_mask & __DRI_IMAGE_BUFFER_BACK) {
      if (get_back_bo(dri2_surf) < 0)
         return 0;

      if (dri2_surf->dri_image_back) {
         images->back = dri2_surf->dri_image_back;
         images->image_mask |= __DRI_IMAGE_BUFFER_BACK;
      }
   }

   return 1;
}

static EGLint
droid_query_buffer_age(_EGLDisplay *disp, _EGLSurface *surface)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surface);

   if (update_buffers(dri2_surf) < 0) {
      _eglError(EGL_BAD_ALLOC, "droid_query_buffer_age");
      return -1;
   }

   return dri2_surf->back ? dri2_surf->back->age : 0;
}

static EGLBoolean
droid_swap_buffers(_EGLDisplay *disp, _EGLSurface *draw)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);
   const bool has_mutable_rb = _eglSurfaceHasMutableRenderBuffer(draw);

   /* From the EGL_KHR_mutable_render_buffer spec (v12):
    *
    *    If surface is a single-buffered window, pixmap, or pbuffer surface
    *    for which there is no pending change to the EGL_RENDER_BUFFER
    *    attribute, eglSwapBuffers has no effect.
    */
   if (has_mutable_rb &&
       draw->RequestedRenderBuffer == EGL_SINGLE_BUFFER &&
       draw->ActiveRenderBuffer == EGL_SINGLE_BUFFER) {
      _eglLog(_EGL_DEBUG, "%s: remain in shared buffer mode", __func__);
      return EGL_TRUE;
   }

   for (int i = 0; i < dri2_surf->color_buffers_count; i++) {
      if (dri2_surf->color_buffers[i].age > 0)
         dri2_surf->color_buffers[i].age++;
   }

   /* "XXX: we don't use get_back_bo() since it causes regressions in
    * several dEQP tests.
    */
   if (dri2_surf->back)
      dri2_surf->back->age = 1;

   dri2_flush_drawable_for_swapbuffers(disp, draw);

   /* dri2_surf->buffer can be null even when no error has occured. For
    * example, if the user has called no GL rendering commands since the
    * previous eglSwapBuffers, then the driver may have not triggered
    * a callback to ANativeWindow::dequeueBuffer, in which case
    * dri2_surf->buffer remains null.
    */
   if (dri2_surf->buffer)
      droid_window_enqueue_buffer(disp, dri2_surf);

   dri2_dpy->flush->invalidate(dri2_surf->dri_drawable);

   /* Update the shared buffer mode */
   if (has_mutable_rb &&
       draw->ActiveRenderBuffer != draw->RequestedRenderBuffer) {
       bool mode = (draw->RequestedRenderBuffer == EGL_SINGLE_BUFFER);
      _eglLog(_EGL_DEBUG, "%s: change to shared buffer mode %d",
              __func__, mode);

      if (!droid_set_shared_buffer_mode(disp, draw, mode))
         return EGL_FALSE;
      draw->ActiveRenderBuffer = draw->RequestedRenderBuffer;
   }

   return EGL_TRUE;
}

#ifdef HAVE_DRM_GRALLOC
static int get_format(int format)
{
   switch (format) {
   case HAL_PIXEL_FORMAT_BGRA_8888: return __DRI_IMAGE_FORMAT_ARGB8888;
   case HAL_PIXEL_FORMAT_RGB_565:   return __DRI_IMAGE_FORMAT_RGB565;
   case HAL_PIXEL_FORMAT_RGBA_8888: return __DRI_IMAGE_FORMAT_ABGR8888;
   case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      /*
       * HACK: Hardcode this to RGBX_8888 as per cros_gralloc hack.
       * TODO: Revert this once https://issuetracker.google.com/32077885 is fixed.
       */
   case HAL_PIXEL_FORMAT_RGBX_8888: return __DRI_IMAGE_FORMAT_XBGR8888;
   case HAL_PIXEL_FORMAT_RGBA_FP16: return __DRI_IMAGE_FORMAT_ABGR16161616F;
   case HAL_PIXEL_FORMAT_RGBA_1010102: return __DRI_IMAGE_FORMAT_ABGR2101010;
   default:
      _eglLog(_EGL_WARNING, "unsupported native buffer format 0x%x", format);
   }
   return -1;
}

static __DRIimage *
droid_create_image_from_name(_EGLDisplay *disp,
                             struct ANativeWindowBuffer *buf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   int name;
   int format;

   name = get_native_buffer_name(buf);
   if (!name) {
      _eglError(EGL_BAD_PARAMETER, "eglCreateEGLImageKHR");
      return NULL;
   }

   format = get_format(buf->format);
   if (format == -1)
       return NULL;

   return
      dri2_dpy->image->createImageFromName(dri2_dpy->dri_screen,
					   buf->width,
					   buf->height,
					   format,
					   name,
					   buf->stride,
					   NULL);
}
#endif /* HAVE_DRM_GRALLOC */

static EGLBoolean
droid_query_surface(_EGLDisplay *disp, _EGLSurface *surf,
                    EGLint attribute, EGLint *value)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);
   switch (attribute) {
      case EGL_WIDTH:
         if (dri2_surf->base.Type == EGL_WINDOW_BIT && dri2_surf->window) {
            dri2_surf->window->query(dri2_surf->window,
                                     NATIVE_WINDOW_DEFAULT_WIDTH, value);
            return EGL_TRUE;
         }
         break;
      case EGL_HEIGHT:
         if (dri2_surf->base.Type == EGL_WINDOW_BIT && dri2_surf->window) {
            dri2_surf->window->query(dri2_surf->window,
                                     NATIVE_WINDOW_DEFAULT_HEIGHT, value);
            return EGL_TRUE;
         }
         break;
      default:
         break;
   }
   return _eglQuerySurface(disp, surf, attribute, value);
}

static _EGLImage *
dri2_create_image_android_native_buffer(_EGLDisplay *disp,
                                        _EGLContext *ctx,
                                        struct ANativeWindowBuffer *buf)
{
   if (ctx != NULL) {
      /* From the EGL_ANDROID_image_native_buffer spec:
       *
       *     * If <target> is EGL_NATIVE_BUFFER_ANDROID and <ctx> is not
       *       EGL_NO_CONTEXT, the error EGL_BAD_CONTEXT is generated.
       */
      _eglError(EGL_BAD_CONTEXT, "eglCreateEGLImageKHR: for "
                "EGL_NATIVE_BUFFER_ANDROID, the context must be "
                "EGL_NO_CONTEXT");
      return NULL;
   }

   if (!buf || buf->common.magic != ANDROID_NATIVE_BUFFER_MAGIC ||
       buf->common.version != sizeof(*buf)) {
      _eglError(EGL_BAD_PARAMETER, "eglCreateEGLImageKHR");
      return NULL;
   }

   __DRIimage *dri_image =
      droid_create_image_from_native_buffer(disp, buf);

#ifdef HAVE_DRM_GRALLOC
   if (dri_image == NULL)
      dri_image = droid_create_image_from_name(disp, buf);
#endif

   if (dri_image)
      return dri2_create_image_from_dri(disp, dri_image);

   return NULL;
}

static _EGLImage *
droid_create_image_khr(_EGLDisplay *disp, _EGLContext *ctx, EGLenum target,
                       EGLClientBuffer buffer, const EGLint *attr_list)
{
   switch (target) {
   case EGL_NATIVE_BUFFER_ANDROID:
      return dri2_create_image_android_native_buffer(disp, ctx,
            (struct ANativeWindowBuffer *) buffer);
   default:
      return dri2_create_image_khr(disp, ctx, target, buffer, attr_list);
   }
}

static void
droid_flush_front_buffer(__DRIdrawable * driDrawable, void *loaderPrivate)
{
}

#ifdef HAVE_DRM_GRALLOC
static int
droid_get_buffers_parse_attachments(struct dri2_egl_surface *dri2_surf,
                                    unsigned int *attachments, int count)
{
   int num_buffers = 0;

   /* fill dri2_surf->buffers */
   for (int i = 0; i < count * 2; i += 2) {
      __DRIbuffer *buf, *local;

      assert(num_buffers < ARRAY_SIZE(dri2_surf->buffers));
      buf = &dri2_surf->buffers[num_buffers];

      switch (attachments[i]) {
      case __DRI_BUFFER_BACK_LEFT:
         if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
            buf->attachment = attachments[i];
            buf->name = get_native_buffer_name(dri2_surf->buffer);
            buf->cpp = get_format_bpp(dri2_surf->buffer->format);
            buf->pitch = dri2_surf->buffer->stride * buf->cpp;
            buf->flags = 0;

            if (buf->name)
               num_buffers++;

            break;
         }
         /* fall through for pbuffers */
      case __DRI_BUFFER_DEPTH:
      case __DRI_BUFFER_STENCIL:
      case __DRI_BUFFER_ACCUM:
      case __DRI_BUFFER_DEPTH_STENCIL:
      case __DRI_BUFFER_HIZ:
         local = dri2_egl_surface_alloc_local_buffer(dri2_surf,
               attachments[i], attachments[i + 1]);

         if (local) {
            *buf = *local;
            num_buffers++;
         }
         break;
      case __DRI_BUFFER_FRONT_LEFT:
      case __DRI_BUFFER_FRONT_RIGHT:
      case __DRI_BUFFER_FAKE_FRONT_LEFT:
      case __DRI_BUFFER_FAKE_FRONT_RIGHT:
      case __DRI_BUFFER_BACK_RIGHT:
      default:
         /* no front or right buffers */
         break;
      }
   }

   return num_buffers;
}

static __DRIbuffer *
droid_get_buffers_with_format(__DRIdrawable * driDrawable,
			     int *width, int *height,
			     unsigned int *attachments, int count,
			     int *out_count, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;

   if (update_buffers(dri2_surf) < 0)
      return NULL;

   *out_count = droid_get_buffers_parse_attachments(dri2_surf, attachments, count);

   if (width)
      *width = dri2_surf->base.Width;
   if (height)
      *height = dri2_surf->base.Height;

   return dri2_surf->buffers;
}
#endif /* HAVE_DRM_GRALLOC */

static unsigned
droid_get_capability(void *loaderPrivate, enum dri_loader_cap cap)
{
   /* Note: loaderPrivate is _EGLDisplay* */
   switch (cap) {
   case DRI_LOADER_CAP_RGBA_ORDERING:
      return 1;
   default:
      return 0;
   }
}

static EGLBoolean
droid_add_configs_for_visuals(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   static const struct {
      int format;
      int rgba_shifts[4];
      unsigned int rgba_sizes[4];
   } visuals[] = {
      { HAL_PIXEL_FORMAT_RGBA_8888, { 0, 8, 16, 24 }, { 8, 8, 8, 8 } },
      { HAL_PIXEL_FORMAT_RGBX_8888, { 0, 8, 16, -1 }, { 8, 8, 8, 0 } },
      { HAL_PIXEL_FORMAT_RGB_565,   { 11, 5, 0, -1 }, { 5, 6, 5, 0 } },
      /* This must be after HAL_PIXEL_FORMAT_RGBA_8888, we only keep BGRA
       * visual if it turns out RGBA visual is not available.
       */
      { HAL_PIXEL_FORMAT_BGRA_8888, { 16, 8, 0, 24 }, { 8, 8, 8, 8 } },
   };

   unsigned int format_count[ARRAY_SIZE(visuals)] = { 0 };
   int config_count = 0;

   /* The nesting of loops is significant here. Also significant is the order
    * of the HAL pixel formats. Many Android apps (such as Google's official
    * NDK GLES2 example app), and even portions the core framework code (such
    * as SystemServiceManager in Nougat), incorrectly choose their EGLConfig.
    * They neglect to match the EGLConfig's EGL_NATIVE_VISUAL_ID against the
    * window's native format, and instead choose the first EGLConfig whose
    * channel sizes match those of the native window format while ignoring the
    * channel *ordering*.
    *
    * We can detect such buggy clients in logcat when they call
    * eglCreateSurface, by detecting the mismatch between the EGLConfig's
    * format and the window's format.
    *
    * As a workaround, we generate EGLConfigs such that all EGLConfigs for HAL
    * pixel format i precede those for HAL pixel format i+1. In my
    * (chadversary) testing on Android Nougat, this was good enough to pacify
    * the buggy clients.
    */
   bool has_rgba = false;
   for (int i = 0; i < ARRAY_SIZE(visuals); i++) {
      /* Only enable BGRA configs when RGBA is not available. BGRA configs are
       * buggy on stock Android.
       */
      if (visuals[i].format == HAL_PIXEL_FORMAT_BGRA_8888 && has_rgba)
         continue;
      for (int j = 0; dri2_dpy->driver_configs[j]; j++) {
         const EGLint surface_type = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;

         const EGLint config_attrs[] = {
           EGL_NATIVE_VISUAL_ID,   visuals[i].format,
           EGL_NATIVE_VISUAL_TYPE, visuals[i].format,
           EGL_FRAMEBUFFER_TARGET_ANDROID, EGL_TRUE,
           EGL_RECORDABLE_ANDROID, EGL_TRUE,
           EGL_NONE
         };

         struct dri2_egl_config *dri2_conf =
            dri2_add_config(disp, dri2_dpy->driver_configs[j],
                            config_count + 1, surface_type, config_attrs,
                            visuals[i].rgba_shifts, visuals[i].rgba_sizes);
         if (dri2_conf) {
            if (dri2_conf->base.ConfigID == config_count + 1)
               config_count++;
            format_count[i]++;
         }
      }
      if (visuals[i].format == HAL_PIXEL_FORMAT_RGBA_8888 && format_count[i])
         has_rgba = true;
   }

   for (int i = 0; i < ARRAY_SIZE(format_count); i++) {
      if (!format_count[i]) {
         _eglLog(_EGL_DEBUG, "No DRI config supports native format 0x%x",
                 visuals[i].format);
      }
   }

   return (config_count != 0);
}

static const struct dri2_egl_display_vtbl droid_display_vtbl = {
   .authenticate = NULL,
   .create_window_surface = droid_create_window_surface,
   .create_pbuffer_surface = droid_create_pbuffer_surface,
   .destroy_surface = droid_destroy_surface,
   .create_image = droid_create_image_khr,
   .swap_buffers = droid_swap_buffers,
   .swap_interval = droid_swap_interval,
   .query_buffer_age = droid_query_buffer_age,
   .query_surface = droid_query_surface,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
   .set_shared_buffer_mode = droid_set_shared_buffer_mode,
};

#ifdef HAVE_DRM_GRALLOC
static const __DRIdri2LoaderExtension droid_dri2_loader_extension = {
   .base = { __DRI_DRI2_LOADER, 4 },

   .getBuffers           = NULL,
   .flushFrontBuffer     = droid_flush_front_buffer,
   .getBuffersWithFormat = droid_get_buffers_with_format,
   .getCapability        = droid_get_capability,
};

static const __DRIextension *droid_dri2_loader_extensions[] = {
   &droid_dri2_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   /* No __DRI_MUTABLE_RENDER_BUFFER_LOADER because it requires
    * __DRI_IMAGE_LOADER.
    */
   NULL,
};
#endif /* HAVE_DRM_GRALLOC */

static const __DRIimageLoaderExtension droid_image_loader_extension = {
   .base = { __DRI_IMAGE_LOADER, 2 },

   .getBuffers          = droid_image_get_buffers,
   .flushFrontBuffer    = droid_flush_front_buffer,
   .getCapability       = droid_get_capability,
};

static void
droid_display_shared_buffer(__DRIdrawable *driDrawable, int fence_fd,
                            void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   struct ANativeWindowBuffer *old_buffer UNUSED = dri2_surf->buffer;

   if (!_eglSurfaceInSharedBufferMode(&dri2_surf->base)) {
      _eglLog(_EGL_WARNING, "%s: internal error: buffer is not shared",
              __func__);
      return;
   }

   if (fence_fd >= 0) {
      /* The driver's fence is more recent than the surface's out fence, if it
       * exists at all. So use the driver's fence.
       */
      if (dri2_surf->out_fence_fd >= 0) {
         close(dri2_surf->out_fence_fd);
         dri2_surf->out_fence_fd = -1;
      }
   } else if (dri2_surf->out_fence_fd >= 0) {
      fence_fd = dri2_surf->out_fence_fd;
      dri2_surf->out_fence_fd = -1;
   }

   if (dri2_surf->window->queueBuffer(dri2_surf->window, dri2_surf->buffer,
                                      fence_fd)) {
      _eglLog(_EGL_WARNING, "%s: ANativeWindow::queueBuffer failed", __func__);
      close(fence_fd);
      return;
   }

   fence_fd = -1;

   if (dri2_surf->window->dequeueBuffer(dri2_surf->window, &dri2_surf->buffer,
                                        &fence_fd)) {
      /* Tear down the surface because it no longer has a back buffer. */
      struct dri2_egl_display *dri2_dpy =
         dri2_egl_display(dri2_surf->base.Resource.Display);

      _eglLog(_EGL_WARNING, "%s: ANativeWindow::dequeueBuffer failed", __func__);

      dri2_surf->base.Lost = true;
      dri2_surf->buffer = NULL;
      dri2_surf->back = NULL;

      if (dri2_surf->dri_image_back) {
         dri2_dpy->image->destroyImage(dri2_surf->dri_image_back);
         dri2_surf->dri_image_back = NULL;
      }

      dri2_dpy->flush->invalidate(dri2_surf->dri_drawable);
      return;
   }

   if (fence_fd < 0)
      return;

   /* Access to the buffer is controlled by a sync fence. Block on it.
    *
    * Ideally, we would submit the fence to the driver, and the driver would
    * postpone command execution until it signalled. But DRI lacks API for
    * that (as of 2018-04-11).
    *
    *  SYNC_IOC_WAIT waits forever if timeout < 0
    */
   sync_wait(fence_fd, -1);
   close(fence_fd);
}

static const __DRImutableRenderBufferLoaderExtension droid_mutable_render_buffer_extension = {
   .base = { __DRI_MUTABLE_RENDER_BUFFER_LOADER, 1 },
   .displaySharedBuffer = droid_display_shared_buffer,
};

static const __DRIextension *droid_image_loader_extensions[] = {
   &droid_image_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   &droid_mutable_render_buffer_extension.base,
   NULL,
};

static EGLBoolean
droid_load_driver(_EGLDisplay *disp, bool swrast)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   dri2_dpy->driver_name = loader_get_driver_for_fd(dri2_dpy->fd);
   if (dri2_dpy->driver_name == NULL)
      return false;

#ifdef HAVE_DRM_GRALLOC
   /* Handle control nodes using __DRI_DRI2_LOADER extension and GEM names
    * for backwards compatibility with drm_gralloc. (Do not use on new
    * systems.) */
   dri2_dpy->loader_extensions = droid_dri2_loader_extensions;
   if (!dri2_load_driver(disp)) {
      goto error;
   }
#else
   if (swrast) {
      /* Use kms swrast only with vgem / virtio_gpu.
       * virtio-gpu fallbacks to software rendering when 3D features
       * are unavailable since 6c5ab.
       */
      if (strcmp(dri2_dpy->driver_name, "vgem") == 0 ||
          strcmp(dri2_dpy->driver_name, "virtio_gpu") == 0) {
         free(dri2_dpy->driver_name);
         dri2_dpy->driver_name = strdup("kms_swrast");
      } else {
         goto error;
      }
   }

   dri2_dpy->loader_extensions = droid_image_loader_extensions;
   if (!dri2_load_driver_dri3(disp)) {
      goto error;
   }
#endif

   return true;

error:
   free(dri2_dpy->driver_name);
   dri2_dpy->driver_name = NULL;
   return false;
}

static void
droid_unload_driver(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   dlclose(dri2_dpy->driver);
   dri2_dpy->driver = NULL;
   free(dri2_dpy->driver_name);
   dri2_dpy->driver_name = NULL;
}

static int
droid_filter_device(_EGLDisplay *disp, int fd, const char *vendor)
{
   drmVersionPtr ver = drmGetVersion(fd);
   if (!ver)
      return -1;

   if (strcmp(vendor, ver->name) != 0) {
      drmFreeVersion(ver);
      return -1;
   }

   drmFreeVersion(ver);
   return 0;
}

static EGLBoolean
droid_probe_device(_EGLDisplay *disp, bool swrast)
{
  /* Check that the device is supported, by attempting to:
   * - load the dri module
   * - and, create a screen
   */
   if (!droid_load_driver(disp, swrast))
      return EGL_FALSE;

   if (!dri2_create_screen(disp)) {
      _eglLog(_EGL_WARNING, "DRI2: failed to create screen");
      droid_unload_driver(disp);
      return EGL_FALSE;
   }
   return EGL_TRUE;
}

#ifdef HAVE_DRM_GRALLOC
static EGLBoolean
droid_open_device(_EGLDisplay *disp, bool swrast)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   int fd = -1, err = -EINVAL;

   if (swrast)
      return EGL_FALSE;

   if (dri2_dpy->gralloc->perform)
      err = dri2_dpy->gralloc->perform(dri2_dpy->gralloc,
                                       GRALLOC_MODULE_PERFORM_GET_DRM_FD,
                                       &fd);
   if (err || fd < 0) {
      _eglLog(_EGL_WARNING, "fail to get drm fd");
      return EGL_FALSE;
   }

   dri2_dpy->fd = os_dupfd_cloexec(fd);
   if (dri2_dpy->fd < 0)
      return EGL_FALSE;

   if (drmGetNodeTypeFromFd(dri2_dpy->fd) == DRM_NODE_RENDER)
      return EGL_FALSE;

   return droid_probe_device(disp, swrast);
}
#else
static EGLBoolean
droid_open_device(_EGLDisplay *disp, bool swrast)
{
#define MAX_DRM_DEVICES 64
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   drmDevicePtr device, devices[MAX_DRM_DEVICES] = { NULL };
   int num_devices;

   char *vendor_name = NULL;
   char vendor_buf[PROPERTY_VALUE_MAX];

#ifdef EGL_FORCE_RENDERNODE
   const unsigned node_type = DRM_NODE_RENDER;
#else
   const unsigned node_type = swrast ? DRM_NODE_PRIMARY : DRM_NODE_RENDER;
#endif

   if (property_get("drm.gpu.vendor_name", vendor_buf, NULL) > 0)
      vendor_name = vendor_buf;

   num_devices = drmGetDevices2(0, devices, ARRAY_SIZE(devices));
   if (num_devices < 0)
      return EGL_FALSE;

   for (int i = 0; i < num_devices; i++) {
      device = devices[i];

      if (!(device->available_nodes & (1 << node_type)))
         continue;

      dri2_dpy->fd = loader_open_device(device->nodes[node_type]);
      if (dri2_dpy->fd < 0) {
         _eglLog(_EGL_WARNING, "%s() Failed to open DRM device %s",
                 __func__, device->nodes[node_type]);
         continue;
      }

      /* If a vendor is explicitly provided, we use only that.
       * Otherwise we fall-back the first device that is supported.
       */
      if (vendor_name) {
         if (droid_filter_device(disp, dri2_dpy->fd, vendor_name)) {
            /* Device does not match - try next device */
            close(dri2_dpy->fd);
            dri2_dpy->fd = -1;
            continue;
         }
         /* If the requested device matches - use it. Regardless if
          * init fails, do not fall-back to any other device.
          */
         if (!droid_probe_device(disp, false)) {
            close(dri2_dpy->fd);
            dri2_dpy->fd = -1;
         }

         break;
      }
      if (droid_probe_device(disp, swrast))
         break;

      /* No explicit request - attempt the next device */
      close(dri2_dpy->fd);
      dri2_dpy->fd = -1;
   }
   drmFreeDevices(devices, num_devices);

   if (dri2_dpy->fd < 0) {
      _eglLog(_EGL_WARNING, "Failed to open %s DRM device",
            vendor_name ? "desired": "any");
      return EGL_FALSE;
   }

   return EGL_TRUE;
#undef MAX_DRM_DEVICES
}

#endif

EGLBoolean
dri2_initialize_android(_EGLDisplay *disp)
{
   _EGLDevice *dev;
   bool device_opened = false;
   struct dri2_egl_display *dri2_dpy;
   const char *err;
   int ret;

   dri2_dpy = calloc(1, sizeof(*dri2_dpy));
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_dpy->fd = -1;
   ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                       (const hw_module_t **)&dri2_dpy->gralloc);
   if (ret) {
      err = "DRI2: failed to get gralloc module";
      goto cleanup;
   }

   disp->DriverData = (void *) dri2_dpy;
   device_opened = droid_open_device(disp, disp->Options.ForceSoftware);

   if (!device_opened) {
      err = "DRI2: failed to open device";
      goto cleanup;
   }

   dev = _eglAddDevice(dri2_dpy->fd, false);
   if (!dev) {
      err = "DRI2: failed to find EGLDevice";
      goto cleanup;
   }

   disp->Device = dev;

   if (!dri2_setup_extensions(disp)) {
      err = "DRI2: failed to setup extensions";
      goto cleanup;
   }

   dri2_setup_screen(disp);

   /* We set the maximum swap interval as 1 for Android platform, since it is
    * the maximum value supported by Android according to the value of
    * ANativeWindow::maxSwapInterval.
    */
   dri2_setup_swap_interval(disp, 1);

   disp->Extensions.ANDROID_framebuffer_target = EGL_TRUE;
   disp->Extensions.ANDROID_image_native_buffer = EGL_TRUE;
   disp->Extensions.ANDROID_recordable = EGL_TRUE;

   /* Querying buffer age requires a buffer to be dequeued.  Without
    * EGL_ANDROID_native_fence_sync, dequeue might call eglClientWaitSync and
    * result in a deadlock (the lock is already held by eglQuerySurface).
    */
   if (disp->Extensions.ANDROID_native_fence_sync) {
      disp->Extensions.EXT_buffer_age = EGL_TRUE;
   } else {
      /* disable KHR_partial_update that might have been enabled in
       * dri2_setup_screen
       */
      disp->Extensions.KHR_partial_update = EGL_FALSE;
   }

   disp->Extensions.KHR_image = EGL_TRUE;
#if ANDROID_API_LEVEL >= 24
   if (dri2_dpy->mutable_render_buffer &&
       dri2_dpy->loader_extensions == droid_image_loader_extensions) {
      disp->Extensions.KHR_mutable_render_buffer = EGL_TRUE;
   }
#endif

   /* Create configs *after* enabling extensions because presence of DRI
    * driver extensions can affect the capabilities of EGLConfigs.
    */
   if (!droid_add_configs_for_visuals(disp)) {
      err = "DRI2: failed to add configs";
      goto cleanup;
   }

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &droid_display_vtbl;

   return EGL_TRUE;

cleanup:
   dri2_display_destroy(disp);
   return _eglError(EGL_NOT_INITIALIZED, err);
}
