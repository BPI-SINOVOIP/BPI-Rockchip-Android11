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

/*
 * resource tests
 * test illegal resource combinations
 - 1D resources with height or depth
 - 2D with depth
*/
#include <check.h>
#include <stdlib.h>
#include <errno.h>
#include <virglrenderer.h>
#include "virgl_hw.h"
#include "testvirgl.h"

#include "pipe/p_defines.h"
#include "pipe/p_format.h"

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

struct res_test {
    struct virgl_renderer_resource_create_args args;
    int retval;
};

#define TEST(thandle, ttarget, tformat, tbind, twidth, theight, tdepth, tarray_size, tnr_samples, tretval) \
  { .args = { .handle = (thandle),					\
	      .target = (ttarget),					\
	      .format = (tformat),					\
	      .bind = (tbind),						\
	      .width = (twidth),					\
	      .height = (theight),					\
	      .depth = (tdepth),					\
	      .array_size = (tarray_size),				\
	      .last_level = 0,						\
	      .nr_samples = (tnr_samples),				\
	      .flags = 0 },					\
      .retval = (tretval)}

#define TEST_MIP(thandle, ttarget, tformat, tbind, twidth, theight, tdepth, tarray_size, tnr_samples, tlast_level, tretval) \
  { .args = { .handle = (thandle),					\
	      .target = (ttarget),					\
	      .format = (tformat),					\
	      .bind = (tbind),						\
	      .width = (twidth),					\
	      .height = (theight),					\
	      .depth = (tdepth),					\
	      .array_size = (tarray_size),				\
	      .last_level = (tlast_level),				\
	      .nr_samples = (tnr_samples),				\
	      .flags = 0 },					\
      .retval = (tretval)}

#define TEST_F(thandle, ttarget, tformat, tbind, twidth, theight, tdepth, tarray_size, tnr_samples, tflags, tretval) \
  { .args = { .handle = (thandle),					\
	      .target = (ttarget),					\
	      .format = (tformat),					\
	      .bind = (tbind),						\
	      .width = (twidth),					\
	      .height = (theight),					\
	      .depth = (tdepth),					\
	      .array_size = (tarray_size),				\
	      .nr_samples = (tnr_samples),				\
	      .flags = (tflags) },					\
      .retval = (tretval)}


static struct res_test testlist[] = {

  /* illegal target - FAIL */
  TEST(1, PIPE_MAX_TEXTURE_TYPES + 1, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 0, EINVAL),

  /* illegal format - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_COUNT + 1, 0, 50, 1, 1, 1, 0, EINVAL),

  /* legal flags - PASS */
  TEST_F(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, VIRGL_RESOURCE_Y_0_TOP, 0),
  /* legal flags - PASS */
  TEST_F(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, VIRGL_RESOURCE_Y_0_TOP, 0),
  /* illegal flags - FAIL */
  TEST_F(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0xF, EINVAL),
  /* illegal flags - FAIL */
  TEST_F(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, VIRGL_RESOURCE_Y_0_TOP, EINVAL),
  /* illegal flags - FAIL */
  TEST_F(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, VIRGL_RESOURCE_Y_0_TOP, EINVAL),

  /* buffer test - PASS */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 0, 0),
  /* buffer test with height - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 50, 1, 1, 0, EINVAL),
  /* buffer test with depth - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 5, 1, 0, EINVAL),
  /* buffer test with array - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 4, 0, EINVAL),
  /* buffer test with samples - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 4, EINVAL),
  /* buffer test with miplevels - FAIL */
  TEST_MIP(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 1, 4, EINVAL),

  /* buffer test - sampler view */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* buffer test - custom binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_CUSTOM, 50, 1, 1, 1, 0, 0),
  /* buffer test - vertex binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_VERTEX_BUFFER, 50, 1, 1, 1, 0, 0),
  /* buffer test - index binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_INDEX_BUFFER, 50, 1, 1, 1, 0, 0),
  /* buffer test - constant binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_CONSTANT_BUFFER, 50, 1, 1, 1, 0, 0),
  /* buffer test - stream binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_STREAM_OUTPUT, 50, 1, 1, 1, 0, 0),

  /* 1D test - stream binding - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_R8_UNORM, PIPE_BIND_VERTEX_BUFFER, 50, 1, 1, 1, 0, EINVAL),
  /* 1D test - no binding - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 0, EINVAL),

  /* 1D texture - PASS */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 1D texture with height - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, EINVAL),
  /* 1D texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* 1D texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, EINVAL),
  /* 1D texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, EINVAL),
  /* 1D texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 1, 4, 0),

  /* 1D array texture - PASS */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 1D array texture with height - FAIL */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, EINVAL),
  /* 1D texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* 1D texture with array - PASS */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, 0),
  /* 1D texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, EINVAL),
  /* 1D array texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 1, 4, 0),

  /* 2D texture - PASS */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_CURSOR, 50, 50, 1, 1, 0, 0),
  /* 2D texture with height - PASS */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, 0),
  /* 2D texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* 2D texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, EINVAL),
  /* 2D texture with samples - PASS */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, 0),
  /* 2D texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 1, 4, 0),
  /* 2D texture with samples and miplevels - FAIL */
  TEST_MIP(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, 4, EINVAL),

  /* RECT texture - PASS */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* RECT texture with height - PASS */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, 0),
  /* RECT texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* RECT texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, EINVAL),
  /* RECT texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, EINVAL),
  /* RECT texture with miplevels - FAIL */
  TEST_MIP(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 1, 4, EINVAL),

  /* 2D texture array - PASS */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 2D texture with height - PASS */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, 0),
  /* 2D texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* 2D texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, 0),
  /* 2D texture with samples - PASS */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, 0),
  /* 2D texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 1, 4, 0),
  /* 2D texture with samplesmiplevels - FAIL */
  TEST_MIP(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, 4, EINVAL),

  /* 3D texture - PASS */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 3D texture with height - PASS */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, 0),
  /* 3D texture with depth - PASS */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, 0),
  /* 3D texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, EINVAL),
  /* 3D texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, EINVAL),
  /* 3D texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 1, 4, 0),

  /* CUBE texture with array size == 6 - PASS */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 6, 0, 0),
  /* CUBE texture with array size != 6 - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, EINVAL),
  /* CUBE texture with height - PASS */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 6, 0, 0),
  /* CUBE texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 6, 0, EINVAL),
  /* CUBE texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 6, 4, EINVAL),
  /* CUBE texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 6, 1, 4, 0),
};

/* separate since these may fail on a GL that doesn't support cube map arrays */
static struct res_test cubemaparray_testlist[] = {
  /* CUBE array with array size = 6 - PASS */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 6, 1, 0),
  /* CUBE array with array size = 12 - PASS */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 12, 1, 0),
  /* CUBE array with array size = 10 - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 10, 1, EINVAL),
  /* CUBE array with array size = 12 and height - PASS */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 12, 1, 0),
  /* CUBE array with array size = 12 and depth - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 5, 12, 1, EINVAL),
  /* CUBE array with array size = 12 and samples - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 12, 4, EINVAL),
  /* CUBE array with array size = 12 and miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 12, 1, 4, 0),
};

START_TEST(virgl_res_tests)
{
  int ret;
  ret = testvirgl_init_single_ctx();
  ck_assert_int_eq(ret, 0);

  /* Do not test if multisample is not available */
  unsigned multisample = testvirgl_get_multisample_from_caps();
  if (!multisample) {
    testvirgl_fini_single_ctx();
    return;
  }

  ret = virgl_renderer_resource_create(&testlist[_i].args, NULL, 0);
  ck_assert_int_eq(ret, testlist[_i].retval);

  testvirgl_fini_single_ctx();
}
END_TEST

START_TEST(cubemaparray_res_tests)
{
  int ret;
  ret = testvirgl_init_single_ctx();
  ck_assert_int_eq(ret, 0);

  ret = virgl_renderer_resource_create(&cubemaparray_testlist[_i].args, NULL, 0);
  ck_assert_int_eq(ret, cubemaparray_testlist[_i].retval);

  testvirgl_fini_single_ctx();
}
END_TEST

static Suite *virgl_init_suite(void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create("virgl_resource");
  tc_core = tcase_create("resource");

  tcase_add_loop_test(tc_core, virgl_res_tests, 0, ARRAY_SIZE(testlist));
  tcase_add_loop_test(tc_core, cubemaparray_res_tests, 0, ARRAY_SIZE(cubemaparray_testlist));
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
