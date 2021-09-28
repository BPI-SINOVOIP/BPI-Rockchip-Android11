/**************************************************************************
 *
 * Copyright (C) 2015 Red Hat Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "virglrenderer.h"

#include <sys/uio.h>
#include "vtest.h"
#include "vtest_protocol.h"
#include "util.h"
#include "util/u_debug.h"

static int ctx_id = 1;
static int fence_id = 1;

static int last_fence;
static void vtest_write_fence(UNUSED void *cookie, uint32_t fence_id_in)
{
  last_fence = fence_id_in;
}

struct virgl_renderer_callbacks vtest_cbs = {
    .version = 1,
    .write_fence = vtest_write_fence,
};

struct vtest_renderer {
  int in_fd;
  int out_fd;
};

struct vtest_renderer renderer;

struct virgl_box {
	uint32_t x, y, z;
	uint32_t w, h, d;
};

static int vtest_block_write(int fd, void *buf, int size)
{
   void *ptr = buf;
   int left;
   int ret;
   left = size;
   do {
      ret = write(fd, ptr, left);
      if (ret < 0)
         return -errno;
      left -= ret;
      ptr += ret;
   } while (left);
   return size;
}

int vtest_block_read(int fd, void *buf, int size)
{
   void *ptr = buf;
   int left;
   int ret;
   static int savefd = -1;

   left = size;
   do {
      ret = read(fd, ptr, left);
      if (ret <= 0)
	return ret == -1 ? -errno : 0;
      left -= ret;
      ptr += ret;
   } while (left);
   if (getenv("VTEST_SAVE")) {
      if (savefd == -1) {
         savefd = open(getenv("VTEST_SAVE"),
                       O_CLOEXEC|O_CREAT|O_WRONLY|O_TRUNC|O_DSYNC, S_IRUSR|S_IWUSR);
         if (savefd == -1) {
            perror("error opening save file");
            exit(1);
         }
      }
      if (write(savefd, buf, size) != size) {
         perror("failed to save");
         exit(1);
      }
   }
   return size;
}

int vtest_create_renderer(int in_fd, int out_fd, uint32_t length)
{
    char *vtestname;
    int ret;
    int ctx = VIRGL_RENDERER_USE_EGL;

    renderer.in_fd = in_fd;
    renderer.out_fd = out_fd;

    if (getenv("VTEST_USE_GLX"))
       ctx = VIRGL_RENDERER_USE_GLX;

    if (getenv("VTEST_USE_EGL_SURFACELESS")) {
        if (ctx & VIRGL_RENDERER_USE_GLX) {
            fprintf(stderr, "Cannot use surfaceless with GLX.\n");
            return -1;
        }
        ctx |= VIRGL_RENDERER_USE_SURFACELESS;
    }

    if (getenv("VTEST_USE_GLES")) {
        if (ctx & VIRGL_RENDERER_USE_GLX) {
            fprintf(stderr, "Cannot use GLES with GLX.\n");
            return -1;
        }
        ctx |= VIRGL_RENDERER_USE_GLES;
    }

    ret = virgl_renderer_init(&renderer,
                              ctx | VIRGL_RENDERER_THREAD_SYNC, &vtest_cbs);
    if (ret) {
      fprintf(stderr, "failed to initialise renderer.\n");
      return -1;
    }

    vtestname = calloc(1, length + 1);
    if (!vtestname)
      return -1;

    ret = vtest_block_read(renderer.in_fd, vtestname, length);
    if (ret != (int)length) {
       ret = -1;
       goto end;
    }

    ret = virgl_renderer_context_create(ctx_id, strlen(vtestname), vtestname);

end:
    free(vtestname);
    return ret;
}

void vtest_destroy_renderer(void)
{
  virgl_renderer_context_destroy(ctx_id);
  virgl_renderer_cleanup(&renderer);
  renderer.in_fd = -1;
  renderer.out_fd = -1;
}

int vtest_send_caps2(void)
{
    uint32_t hdr_buf[2];
    void *caps_buf;
    int ret;
    uint32_t max_ver, max_size;

    virgl_renderer_get_cap_set(2, &max_ver, &max_size);

    if (max_size == 0)
	return -1;
    caps_buf = malloc(max_size);
    if (!caps_buf)
	return -1;

    virgl_renderer_fill_caps(2, 1, caps_buf);

    hdr_buf[0] = max_size + 1;
    hdr_buf[1] = 2;
    ret = vtest_block_write(renderer.out_fd, hdr_buf, 8);
    if (ret < 0)
	goto end;
    vtest_block_write(renderer.out_fd, caps_buf, max_size);
    if (ret < 0)
	goto end;

end:
    free(caps_buf);
    return 0;
}

int vtest_send_caps(void)
{
    uint32_t  max_ver, max_size;
    void *caps_buf;
    uint32_t hdr_buf[2];
    int ret;

    virgl_renderer_get_cap_set(1, &max_ver, &max_size);

    caps_buf = malloc(max_size);
    if (!caps_buf)
	return -1;
    
    virgl_renderer_fill_caps(1, 1, caps_buf);

    hdr_buf[0] = max_size + 1;
    hdr_buf[1] = 1;
    ret = vtest_block_write(renderer.out_fd, hdr_buf, 8);
    if (ret < 0)
       goto end;
    vtest_block_write(renderer.out_fd, caps_buf, max_size);
    if (ret < 0)
       goto end;

end:
    free(caps_buf);
    return 0;
}

int vtest_create_resource(void)
{
    uint32_t res_create_buf[VCMD_RES_CREATE_SIZE];
    struct virgl_renderer_resource_create_args args;
    int ret;

    ret = vtest_block_read(renderer.in_fd, &res_create_buf, sizeof(res_create_buf));
    if (ret != sizeof(res_create_buf))
	return -1;
	
    args.handle = res_create_buf[VCMD_RES_CREATE_RES_HANDLE];
    args.target = res_create_buf[VCMD_RES_CREATE_TARGET];
    args.format = res_create_buf[VCMD_RES_CREATE_FORMAT];
    args.bind = res_create_buf[VCMD_RES_CREATE_BIND];

    args.width = res_create_buf[VCMD_RES_CREATE_WIDTH];
    args.height = res_create_buf[VCMD_RES_CREATE_HEIGHT];
    args.depth = res_create_buf[VCMD_RES_CREATE_DEPTH];
    args.array_size = res_create_buf[VCMD_RES_CREATE_ARRAY_SIZE];
    args.last_level = res_create_buf[VCMD_RES_CREATE_LAST_LEVEL];
    args.nr_samples = res_create_buf[VCMD_RES_CREATE_NR_SAMPLES];
    args.flags = 0;

    ret = virgl_renderer_resource_create(&args, NULL, 0);

    virgl_renderer_ctx_attach_resource(ctx_id, args.handle);
    return ret;
}

int vtest_resource_unref(void)
{
    uint32_t res_unref_buf[VCMD_RES_UNREF_SIZE];
    int ret;
    uint32_t handle;

    ret = vtest_block_read(renderer.in_fd, &res_unref_buf, sizeof(res_unref_buf));
    if (ret != sizeof(res_unref_buf))
      return -1;

    handle = res_unref_buf[VCMD_RES_UNREF_RES_HANDLE];
    virgl_renderer_ctx_attach_resource(ctx_id, handle);
    virgl_renderer_resource_unref(handle);
    return 0;
}

int vtest_submit_cmd(uint32_t length_dw)
{
    uint32_t *cbuf;
    int ret;

    if (length_dw > UINT_MAX / 4)
       return -1;

    cbuf = malloc(length_dw * 4);
    if (!cbuf)
	return -1;

    ret = vtest_block_read(renderer.in_fd, cbuf, length_dw * 4);
    if (ret != (int)length_dw * 4) {
       free(cbuf);
       return -1;
    }

    virgl_renderer_submit_cmd(cbuf, ctx_id, length_dw);

    free(cbuf);
    return 0;
}

#define DECODE_TRANSFER \
  do {							\
  handle = thdr_buf[VCMD_TRANSFER_RES_HANDLE];		\
  level = thdr_buf[VCMD_TRANSFER_LEVEL];		\
  stride = thdr_buf[VCMD_TRANSFER_STRIDE];		\
  layer_stride = thdr_buf[VCMD_TRANSFER_LAYER_STRIDE];	\
  box.x = thdr_buf[VCMD_TRANSFER_X];			\
  box.y = thdr_buf[VCMD_TRANSFER_Y];			\
  box.z = thdr_buf[VCMD_TRANSFER_Z];			\
  box.w = thdr_buf[VCMD_TRANSFER_WIDTH];		\
  box.h = thdr_buf[VCMD_TRANSFER_HEIGHT];		\
  box.d = thdr_buf[VCMD_TRANSFER_DEPTH];		\
  data_size = thdr_buf[VCMD_TRANSFER_DATA_SIZE];		\
  } while(0)


int vtest_transfer_get(UNUSED uint32_t length_dw)
{
    uint32_t thdr_buf[VCMD_TRANSFER_HDR_SIZE];
    int ret;
    int level;
    uint32_t stride, layer_stride, handle;
    struct virgl_box box;
    uint32_t data_size;
    void *ptr;
    struct iovec iovec;

    ret = vtest_block_read(renderer.in_fd, thdr_buf, VCMD_TRANSFER_HDR_SIZE * 4);
    if (ret != VCMD_TRANSFER_HDR_SIZE * 4)
      return ret;

    DECODE_TRANSFER;

    ptr = malloc(data_size);
    if (!ptr)
      return -ENOMEM;

    iovec.iov_len = data_size;
    iovec.iov_base = ptr;
    ret = virgl_renderer_transfer_read_iov(handle,
				     ctx_id,
				     level,
				     stride,
				     layer_stride,
				     &box,
				     0,
				     &iovec, 1);
    if (ret)
      fprintf(stderr," transfer read failed %d\n", ret);
    ret = vtest_block_write(renderer.out_fd, ptr, data_size);

    free(ptr);
    return ret < 0 ? ret : 0;
}

int vtest_transfer_put(UNUSED uint32_t length_dw)
{
    uint32_t thdr_buf[VCMD_TRANSFER_HDR_SIZE];
    int ret;
    int level;
    uint32_t stride, layer_stride, handle;
    struct virgl_box box;
    uint32_t data_size;
    void *ptr;
    struct iovec iovec;

    ret = vtest_block_read(renderer.in_fd, thdr_buf, VCMD_TRANSFER_HDR_SIZE * 4);
    if (ret != VCMD_TRANSFER_HDR_SIZE * 4)
      return ret;

    DECODE_TRANSFER;

    ptr = malloc(data_size);
    if (!ptr)
      return -ENOMEM;

    ret = vtest_block_read(renderer.in_fd, ptr, data_size);
    if (ret < 0)
      return ret;

    iovec.iov_len = data_size;
    iovec.iov_base = ptr;
    ret = virgl_renderer_transfer_write_iov(handle,
					    ctx_id,
					    level,
					    stride,
					    layer_stride,
					    &box,
					    0,
					    &iovec, 1);
    if (ret)
      fprintf(stderr," transfer write failed %d\n", ret);
    free(ptr);
    return 0;
}

int vtest_resource_busy_wait(void)
{
  uint32_t bw_buf[VCMD_BUSY_WAIT_SIZE];
  int ret, fd;
  int flags;
  uint32_t hdr_buf[VTEST_HDR_SIZE];
  uint32_t reply_buf[1];
  bool busy = false;
  ret = vtest_block_read(renderer.in_fd, &bw_buf, sizeof(bw_buf));
  if (ret != sizeof(bw_buf))
    return -1;

  /*  handle = bw_buf[VCMD_BUSY_WAIT_HANDLE]; unused as of now */
  flags = bw_buf[VCMD_BUSY_WAIT_FLAGS];

  if (flags == VCMD_BUSY_WAIT_FLAG_WAIT) {
    do {
       if (last_fence == (fence_id - 1))
          break;

       fd = virgl_renderer_get_poll_fd();
       if (fd != -1)
          vtest_wait_for_fd_read(fd);
       virgl_renderer_poll();
    } while (1);
    busy = false;
  } else {
    busy = last_fence != (fence_id - 1);
  }

  hdr_buf[VTEST_CMD_LEN] = 1;
  hdr_buf[VTEST_CMD_ID] = VCMD_RESOURCE_BUSY_WAIT;
  reply_buf[0] = busy ? 1 : 0;

  ret = vtest_block_write(renderer.out_fd, hdr_buf, sizeof(hdr_buf));
  if (ret < 0)
    return ret;

  ret = vtest_block_write(renderer.out_fd, reply_buf, sizeof(reply_buf));
  if (ret < 0)
    return ret;

  return 0;
}

int vtest_renderer_create_fence(void)
{
  virgl_renderer_create_fence(fence_id++, ctx_id);
  return 0;
}

int vtest_poll(void)
{
  virgl_renderer_poll();
  return 0;
}
