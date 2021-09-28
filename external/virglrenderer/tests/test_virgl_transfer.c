/**************************************************************************
 *
 * Copyright (C) 2014 Red Hat Inc.
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

/* transfer and iov related tests */
#include <check.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <errno.h>
#include <virglrenderer.h>
#include "pipe/p_defines.h"
#include "virgl_hw.h"
#include "testvirgl_encode.h"

/* pass an illegal context to transfer fn */
START_TEST(virgl_test_transfer_read_illegal_ctx)
{
  int ret;
  struct virgl_box box;

  ret = virgl_renderer_transfer_read_iov(1, 2, 0, 1, 1, &box, 0, NULL, 0);
  ck_assert_int_eq(ret, EINVAL);
}
END_TEST

START_TEST(virgl_test_transfer_write_illegal_ctx)
{
  int ret;
  struct virgl_box box;

  ret = virgl_renderer_transfer_write_iov(1, 2, 0, 1, 1, &box, 0, NULL, 0);
  ck_assert_int_eq(ret, EINVAL);
}
END_TEST

/* pass a resource not bound to the context to transfers */
START_TEST(virgl_test_transfer_read_unbound_res)
{
  int ret;
  struct virgl_box box;

  ret = virgl_renderer_transfer_read_iov(1, 1, 0, 1, 1, &box, 0, NULL, 0);
  ck_assert_int_eq(ret, EINVAL);
}
END_TEST

START_TEST(virgl_test_transfer_write_unbound_res)
{
  int ret;
  struct virgl_box box;

  ret = virgl_renderer_transfer_write_iov(1, 1, 0, 1, 1, &box, 0, NULL, 0);
  ck_assert_int_eq(ret, EINVAL);
}
END_TEST

/* don't pass an IOV to read into */
START_TEST(virgl_test_transfer_read_no_iov)
{
  struct virgl_box box;
  struct virgl_renderer_resource_create_args res;
  int ret;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  ret = virgl_renderer_transfer_read_iov(1, 1, 0, 1, 1, &box, 0, NULL, 0);
  ck_assert_int_eq(ret, EINVAL);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_write_no_iov)
{
  struct virgl_box box;
  struct virgl_renderer_resource_create_args res;
  int ret;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  ret = virgl_renderer_transfer_write_iov(1, 1, 0, 1, 1, &box, 0, NULL, 0);
  ck_assert_int_eq(ret, EINVAL);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_read_no_box)
{
  struct virgl_renderer_resource_create_args res;
  struct iovec iovs[1];
  int niovs = 1;
  int ret;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  ret = virgl_renderer_transfer_read_iov(1, 1, 0, 1, 1, NULL, 0, iovs, niovs);
  ck_assert_int_eq(ret, EINVAL);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_write_no_box)
{
  struct virgl_renderer_resource_create_args res;
  struct iovec iovs[1];
  int niovs = 1;
  int ret;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  ret = virgl_renderer_transfer_write_iov(1, 1, 0, 1, 1, NULL, 0, iovs, niovs);
  ck_assert_int_eq(ret, EINVAL);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST


/* pass a bad box argument */
START_TEST(virgl_test_transfer_read_1d_bad_box)
{
  struct virgl_renderer_resource_create_args res;
  struct iovec iovs[1];
  int niovs = 1;
  int ret;
  struct virgl_box box;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  box.x = box.y = box.z = 0;
  box.w = 10;
  box.h = 2;
  box.d = 1;

  ret = virgl_renderer_transfer_read_iov(1, 1, 0, 1, 1, &box, 0, iovs, niovs);
  ck_assert_int_eq(ret, EINVAL);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_write_1d_bad_box)
{
  struct virgl_renderer_resource_create_args res;
  struct iovec iovs[1];
  int niovs = 1;
  int ret;
  struct virgl_box box;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  box.x = box.y = box.z = 0;
  box.w = 10;
  box.h = 2;
  box.d = 1;

  ret = virgl_renderer_transfer_write_iov(1, 1, 0, 1, 1, &box, 0, iovs, niovs);
  ck_assert_int_eq(ret, EINVAL);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_read_1d_array_bad_box)
{
  struct virgl_renderer_resource_create_args res;
  struct iovec iovs[1];
  int niovs = 1;
  int ret;
  struct virgl_box box;

  testvirgl_init_simple_1d_resource(&res, 1);
  res.target = PIPE_TEXTURE_1D_ARRAY;
  res.array_size = 5;

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  box.x = box.y = box.z = 0;
  box.w = 10;
  box.h = 2;
  box.d = 6;

  ret = virgl_renderer_transfer_read_iov(1, 1, 0, 1, 1, &box, 0, iovs, niovs);
  ck_assert_int_eq(ret, EINVAL);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_read_3d_bad_box)
{
  struct virgl_renderer_resource_create_args res;
  struct iovec iovs[1];
  int niovs = 1;
  int ret;
  struct virgl_box box;

  testvirgl_init_simple_1d_resource(&res, 1);
  res.target = PIPE_TEXTURE_3D;
  res.depth = 5;

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  box.x = box.y = box.z = 0;
  box.w = 10;
  box.h = 2;
  box.d = 6;

  ret = virgl_renderer_transfer_read_iov(1, 1, 0, 1, 1, &box, 0, iovs, niovs);
  ck_assert_int_eq(ret, EINVAL);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_1d)
{
    struct virgl_resource res;
    unsigned char data[50*4];
    struct iovec iov = { .iov_base = data, .iov_len = sizeof(data) };
    int niovs = 1;
    int ret;
    unsigned i;
    struct virgl_box box;

    /* init and create simple 2D resource */
    ret = testvirgl_create_backed_simple_1d_res(&res, 1);
    ck_assert_int_eq(ret, 0);

    /* attach resource to context */
    virgl_renderer_ctx_attach_resource(1, res.handle);

    box.x = box.y = box.z = 0;
    box.w = 50;
    box.h = 1;
    box.d = 1;
    for (i = 0; i < sizeof(data); i++)
        data[i] = i;

    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 0, 0, 0, &box, 0, &iov, niovs);
    ck_assert_int_eq(ret, 0);

    ret = virgl_renderer_transfer_read_iov(res.handle, 1, 0, 0, 0, &box, 0, NULL, 0);
    ck_assert_int_eq(ret, 0);

    /* check the returned values */
    unsigned char *ptr = res.iovs[0].iov_base;
    for (i = 0; i < sizeof(data); i++) {
        ck_assert_int_eq(ptr[i], i);
    }

    virgl_renderer_ctx_detach_resource(1, res.handle);
    testvirgl_destroy_backed_res(&res);
}
END_TEST

START_TEST(virgl_test_transfer_1d_bad_iov)
{
    struct virgl_renderer_resource_create_args res;
    struct iovec iovs[1] = { { NULL, 23 } };
    int niovs = 1;
    int ret;
    struct virgl_box box = { .w = 50, .h = 1, .d = 1 };

    testvirgl_init_simple_1d_resource(&res, 1);
    res.target = PIPE_TEXTURE_1D;
    res.depth = 1;

    ret = virgl_renderer_resource_create(&res, NULL, 0);
    ck_assert_int_eq(ret, 0);

    virgl_renderer_ctx_attach_resource(1, res.handle);

    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 0, 0, 0, &box, 0, iovs, niovs);
    ck_assert_int_eq(ret, EINVAL);

    virgl_renderer_ctx_detach_resource(1, res.handle);

    virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_1d_bad_iov_offset)
{
    struct virgl_renderer_resource_create_args res;
    unsigned char data[50*4];
    struct iovec iov = { .iov_base = data, .iov_len = sizeof(data) };
    int niovs = 1;
    int ret;
    struct virgl_box box = { .w = 50, .h = 1, .d = 1 };

    testvirgl_init_simple_1d_resource(&res, 1);
    res.target = PIPE_TEXTURE_1D;
    res.depth = 1;

    ret = virgl_renderer_resource_create(&res, NULL, 0);
    ck_assert_int_eq(ret, 0);

    virgl_renderer_ctx_attach_resource(1, res.handle);

    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 0, 0, 0, &box, 20, &iov, niovs);
    ck_assert_int_eq(ret, EINVAL);

    virgl_renderer_ctx_detach_resource(1, res.handle);

    virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_1d_bad_layer_stride)
{
    struct virgl_renderer_resource_create_args res;
    unsigned char data[50*4];
    struct iovec iov = { .iov_base = data, .iov_len = sizeof(data) };
    int niovs = 1;
    int ret;
    struct virgl_box box = { .w = 50, .h = 1, .d = 1 };

    testvirgl_init_simple_1d_resource(&res, 1);
    res.target = PIPE_TEXTURE_1D;
    res.depth = 1;

    ret = virgl_renderer_resource_create(&res, NULL, 0);
    ck_assert_int_eq(ret, 0);

    virgl_renderer_ctx_attach_resource(1, res.handle);

    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 0, 0, 50, &box, 0, &iov, niovs);
    ck_assert_int_eq(ret, EINVAL);

    virgl_renderer_ctx_detach_resource(1, res.handle);

    virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_2d_bad_layer_stride)
{
    struct virgl_renderer_resource_create_args res;
    unsigned char data[50*4];
    struct iovec iov = { .iov_base = data, .iov_len = sizeof(data) };
    int niovs = 1;
    int ret;
    struct virgl_box box = { .w = 50, .h = 1, .d = 1 };

    testvirgl_init_simple_2d_resource(&res, 1);

    ret = virgl_renderer_resource_create(&res, NULL, 0);
    ck_assert_int_eq(ret, 0);

    virgl_renderer_ctx_attach_resource(1, res.handle);

    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 0, 0, 50, &box, 0, &iov, niovs);
    ck_assert_int_eq(ret, EINVAL);

    virgl_renderer_ctx_detach_resource(1, res.handle);

    virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_buffer_bad_layer_stride)
{
    struct virgl_renderer_resource_create_args res;
    unsigned char data[50*4];
    struct iovec iov = { .iov_base = data, .iov_len = sizeof(data) };
    int niovs = 1;
    int ret;
    struct virgl_box box = { .w = 50, .h = 1, .d = 1 };

    testvirgl_init_simple_buffer(&res, 1);

    ret = virgl_renderer_resource_create(&res, NULL, 0);
    ck_assert_int_eq(ret, 0);

    virgl_renderer_ctx_attach_resource(1, res.handle);

    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 0, 0, 50, &box, 0, &iov, niovs);
    ck_assert_int_eq(ret, EINVAL);

    virgl_renderer_ctx_detach_resource(1, res.handle);

    virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_transfer_2d_array_bad_layer_stride)
{
    struct virgl_renderer_resource_create_args res;
    unsigned char *data;
    struct iovec iov;
    int niovs = 1;
    int ret;
    struct virgl_box box = { .w = 50, .h = 5, .d = 2 };
    int size = 50*50*2*4;
    data = calloc(1, size);
    iov.iov_base = data;
    iov.iov_len = size;
    testvirgl_init_simple_2d_resource(&res, 1);
    res.target = PIPE_TEXTURE_2D_ARRAY;
    res.array_size = 5;

    ret = virgl_renderer_resource_create(&res, NULL, 0);
    ck_assert_int_eq(ret, 0);

    virgl_renderer_ctx_attach_resource(1, res.handle);

    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 0, 0, 100, &box, 0, &iov, niovs);
    ck_assert_int_eq(ret, EINVAL);

    virgl_renderer_ctx_detach_resource(1, res.handle);

    virgl_renderer_resource_unref(1);
    free(data);
}
END_TEST

START_TEST(virgl_test_transfer_2d_bad_level)
{
    struct virgl_renderer_resource_create_args res;
    unsigned char data[50*4];
    struct iovec iov = { .iov_base = data, .iov_len = sizeof(data) };
    int niovs = 1;
    int ret;
    struct virgl_box box = { .w = 50, .h = 1, .d = 1 };

    testvirgl_init_simple_2d_resource(&res, 1);
    res.last_level = 1;
    ret = virgl_renderer_resource_create(&res, NULL, 0);
    ck_assert_int_eq(ret, 0);

    virgl_renderer_ctx_attach_resource(1, res.handle);

    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 2, 0, 0, &box, 0, &iov, niovs);
    ck_assert_int_eq(ret, EINVAL);

    virgl_renderer_ctx_detach_resource(1, res.handle);

    virgl_renderer_resource_unref(1);
}
END_TEST

/* test stride less than box size */
START_TEST(virgl_test_transfer_2d_bad_stride)
{
    struct virgl_renderer_resource_create_args res;
    unsigned char data[50*4*2];
    struct iovec iov = { .iov_base = data, .iov_len = sizeof(data) };
    int niovs = 1;
    int ret;
    struct virgl_box box = { .w = 50, .h = 2, .d = 1 };

    testvirgl_init_simple_2d_resource(&res, 1);

    ret = virgl_renderer_resource_create(&res, NULL, 0);
    ck_assert_int_eq(ret, 0);

    virgl_renderer_ctx_attach_resource(1, res.handle);

    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 0, 10, 0, &box, 0, &iov, niovs);
    ck_assert_int_eq(ret, EINVAL);

    virgl_renderer_ctx_detach_resource(1, res.handle);

    virgl_renderer_resource_unref(1);
}
END_TEST

/* for each texture type construct a valid and invalid transfer,
   invalid using a box outside the bounds of the transfer */
#define LARGE_FLAG_WIDTH (1 << 0)
#define LARGE_FLAG_HEIGHT (1 << 1)
#define LARGE_FLAG_DEPTH (1 << 2)
static void get_resource_args(enum pipe_texture_target target, bool invalid,
                              struct virgl_renderer_resource_create_args *args,
                              struct pipe_box *box, int nsamples, int large_flags)
{
  memset(args, 0, sizeof(*args));
  memset(box, 0, sizeof(*box));

  args->handle = 1;
  args->target = target;
  if (args->target == PIPE_BUFFER) {
    args->format = PIPE_FORMAT_R8_UNORM;
    args->bind = PIPE_BIND_VERTEX_BUFFER;
  } else {
    args->bind = PIPE_BIND_SAMPLER_VIEW;
    args->format = PIPE_FORMAT_B8G8R8X8_UNORM;
  }
  args->nr_samples = nsamples;
  args->flags = 0;

  if (large_flags & LARGE_FLAG_WIDTH)
    args->width = 65536*2;
  else
    args->width = 50;
  args->height = args->depth = args->array_size = 1;

  switch (target) {
  case PIPE_TEXTURE_CUBE_ARRAY:
    args->array_size = 12;
    break;
  case PIPE_TEXTURE_1D_ARRAY:
  case PIPE_TEXTURE_2D_ARRAY:
    args->array_size = 10;
    break;
  case PIPE_TEXTURE_3D:
    args->depth = 8;
    break;
  case PIPE_TEXTURE_CUBE:
    args->array_size = 6;
    break;
  default:
    break;
  }

  switch (target) {
  case PIPE_BUFFER:
  case PIPE_TEXTURE_1D:
  case PIPE_TEXTURE_1D_ARRAY:
    break;
  default:
    if (large_flags & LARGE_FLAG_HEIGHT)
      args->height = 64000;
    else
      args->height = 50;
    break;
  }

  if (invalid) {
    box->width = args->width + 10;
    box->height = args->height;
    box->depth = 1;
  } else {
    box->width = args->width;
    box->height = args->height;
    box->depth = 1;
    if (args->depth > 1)
      box->depth = 6;
    if (args->array_size > 1)
      box->depth = 4;
  }
}

static unsigned get_box_size(struct pipe_box *box, int elsize)
{
  return elsize * box->width * box->height * box->depth;
}

static void virgl_test_transfer_res(enum pipe_texture_target target,
				    bool write, bool invalid)
{
  struct virgl_renderer_resource_create_args res;
  struct pipe_box box;
  void *data;
  struct iovec iovs[1];
  int niovs = 1;
  int ret;
  int size;

  get_resource_args(target, invalid, &res, &box, 1, 0);

  size = get_box_size(&box, target == PIPE_BUFFER ? 1 : 4);
  data = calloc(1, size);
  iovs[0].iov_base = data;
  iovs[0].iov_len = size;

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  if (write)
    ret = virgl_renderer_transfer_write_iov(res.handle, 1, 0, 0, 0,
					 (struct virgl_box *)&box, 0, iovs, niovs);
  else
    ret = virgl_renderer_transfer_read_iov(res.handle, 1, 0, 0, 0,
					   (struct virgl_box *)&box, 0, iovs, niovs);
  ck_assert_int_eq(ret, invalid ? EINVAL : 0);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(res.handle);
  free(data);
}

START_TEST(virgl_test_transfer_res_read_valid)
{
  virgl_test_transfer_res(_i, false, false);
}
END_TEST

START_TEST(virgl_test_transfer_res_write_valid)
{
  virgl_test_transfer_res(_i, true, false);
}
END_TEST

START_TEST(virgl_test_transfer_res_read_invalid)
{
  virgl_test_transfer_res(_i, false, true);
}
END_TEST

START_TEST(virgl_test_transfer_res_write_invalid)
{
  virgl_test_transfer_res(_i, true, true);
}
END_TEST

static void virgl_test_transfer_inline(enum pipe_texture_target target,
				       bool invalid, int large_flags)
{
  struct virgl_renderer_resource_create_args args;
  struct pipe_box box;
  struct virgl_context ctx;
  struct virgl_resource res;
  int ret;
  int elsize = target == 0 ? 1 : 4;
  void *data;
  unsigned size;
  ret = testvirgl_init_ctx_cmdbuf(&ctx);
  ck_assert_int_eq(ret, 0);

  get_resource_args(target, invalid, &args, &box, 1, large_flags);

  size = get_box_size(&box, elsize);
  data = calloc(1, size);
  ret = virgl_renderer_resource_create(&args, NULL, 0);
  ck_assert_int_eq(ret, 0);

  res.handle = args.handle;
  res.base.target = args.target;
  res.base.format = args.format;

  virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);
  virgl_encoder_inline_write(&ctx, &res, 0, 0, (struct pipe_box *)&box, data, box.width * elsize, 0);
  ret = virgl_renderer_submit_cmd(ctx.cbuf->buf, ctx.ctx_id, ctx.cbuf->cdw);
  ck_assert_int_eq(ret, invalid ? EINVAL : 0);
  virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

  virgl_renderer_resource_unref(res.handle);
  testvirgl_fini_ctx_cmdbuf(&ctx);
  free(data);
}

START_TEST(virgl_test_transfer_inline_valid)
{
  virgl_test_transfer_inline(_i, false, 0);
}
END_TEST

START_TEST(virgl_test_transfer_inline_invalid)
{
  virgl_test_transfer_inline(_i, true, 0);
}
END_TEST

START_TEST(virgl_test_transfer_inline_valid_large)
{
  virgl_test_transfer_inline(_i, false, LARGE_FLAG_WIDTH);
}
END_TEST

static Suite *virgl_init_suite(void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create("virgl_transfer");
  tc_core = tcase_create("transfer_direct");

  tcase_add_checked_fixture(tc_core, testvirgl_init_single_ctx_nr, testvirgl_fini_single_ctx);
  tcase_add_test(tc_core, virgl_test_transfer_read_illegal_ctx);
  tcase_add_test(tc_core, virgl_test_transfer_write_illegal_ctx);
  tcase_add_test(tc_core, virgl_test_transfer_read_unbound_res);
  tcase_add_test(tc_core, virgl_test_transfer_write_unbound_res);
  tcase_add_test(tc_core, virgl_test_transfer_read_no_iov);
  tcase_add_test(tc_core, virgl_test_transfer_write_no_iov);
  tcase_add_test(tc_core, virgl_test_transfer_read_no_box);
  tcase_add_test(tc_core, virgl_test_transfer_write_no_box);
  tcase_add_test(tc_core, virgl_test_transfer_read_1d_bad_box);
  tcase_add_test(tc_core, virgl_test_transfer_write_1d_bad_box);
  tcase_add_test(tc_core, virgl_test_transfer_read_1d_array_bad_box);
  tcase_add_test(tc_core, virgl_test_transfer_read_3d_bad_box);
  tcase_add_test(tc_core, virgl_test_transfer_1d);
  tcase_add_test(tc_core, virgl_test_transfer_1d_bad_iov);
  tcase_add_test(tc_core, virgl_test_transfer_1d_bad_iov_offset);
  tcase_add_test(tc_core, virgl_test_transfer_1d_bad_layer_stride);
  tcase_add_test(tc_core, virgl_test_transfer_2d_bad_layer_stride);
  tcase_add_test(tc_core, virgl_test_transfer_buffer_bad_layer_stride);
  tcase_add_test(tc_core, virgl_test_transfer_2d_array_bad_layer_stride);
  tcase_add_test(tc_core, virgl_test_transfer_2d_bad_level);
  tcase_add_test(tc_core, virgl_test_transfer_2d_bad_stride);

  tcase_add_loop_test(tc_core, virgl_test_transfer_res_read_valid, 0, PIPE_MAX_TEXTURE_TYPES);
  tcase_add_loop_test(tc_core, virgl_test_transfer_res_write_valid, 0, PIPE_MAX_TEXTURE_TYPES);
  tcase_add_loop_test(tc_core, virgl_test_transfer_res_read_invalid, 0, PIPE_MAX_TEXTURE_TYPES);
  tcase_add_loop_test(tc_core, virgl_test_transfer_res_write_invalid, 0, PIPE_MAX_TEXTURE_TYPES);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("transfer_inline_write");
  tcase_add_loop_test(tc_core, virgl_test_transfer_inline_valid, 0, PIPE_MAX_TEXTURE_TYPES);
  tcase_add_loop_test(tc_core, virgl_test_transfer_inline_invalid, 0, PIPE_MAX_TEXTURE_TYPES);
  tcase_add_loop_test(tc_core, virgl_test_transfer_inline_valid_large, 0, PIPE_MAX_TEXTURE_TYPES);

  suite_add_tcase(s, tc_core);
  return s;

}


int main(void)
{
  Suite *s;
  SRunner *sr;
  int number_failed;

  s = virgl_init_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
