/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <gtest/gtest.h>

extern "C" {
#include "cras_audio_area.h"
#include "cras_empty_iodev.h"
#include "cras_iodev.h"
}

static struct timespec clock_gettime_retspec;
static struct cras_audio_format fake_format;
static cras_audio_area dummy_audio_area;

namespace {

TEST(EmptyIodev, GetInputBuffer) {
  struct cras_iodev* iodev;
  struct timespec ts;
  cras_audio_area* area;
  unsigned nframes;

  iodev = empty_iodev_create(CRAS_STREAM_INPUT, CRAS_NODE_TYPE_FALLBACK_NORMAL);

  clock_gettime_retspec.tv_sec = 0;
  clock_gettime_retspec.tv_nsec = 10000000;
  fake_format.frame_rate = 48000;
  iodev->format = &fake_format;
  iodev->configure_dev(iodev);

  clock_gettime_retspec.tv_nsec = 20000000;
  nframes = iodev->frames_queued(iodev, &ts);
  ASSERT_EQ(480, nframes);

  /* If client takes too long to get input data, number of frames
   * returned shouldn't exceeds device's own buffer size. */
  nframes = 5000;
  clock_gettime_retspec.tv_sec = 1;
  iodev->get_buffer(iodev, &area, &nframes);
  ASSERT_EQ(4096, nframes);

  iodev->close_dev(iodev);
  empty_iodev_destroy(iodev);
}

}  // namespace

extern "C" {

void cras_iodev_free_format(struct cras_iodev* iodev) {}

int cras_iodev_default_no_stream_playback(struct cras_iodev* odev, int enable) {
  return 0;
}

void cras_iodev_init_audio_area(struct cras_iodev* iodev, int num_channels) {
  iodev->area = &dummy_audio_area;
}

void cras_iodev_free_audio_area(struct cras_iodev* iodev) {}

void cras_audio_area_config_buf_pointers(struct cras_audio_area* area,
                                         const struct cras_audio_format* fmt,
                                         uint8_t* base_buffer) {}

int cras_iodev_list_rm_input(struct cras_iodev* input) {
  return 0;
}

int cras_iodev_list_rm_output(struct cras_iodev* output) {
  return 0;
}

void cras_iodev_free_resources(struct cras_iodev* iodev) {}

void cras_iodev_add_node(struct cras_iodev* iodev, struct cras_ionode* node) {
  iodev->nodes = node;
}

void cras_iodev_set_active_node(struct cras_iodev* iodev,
                                struct cras_ionode* node) {
  iodev->active_node = node;
}

//  From librt.
int clock_gettime(clockid_t clk_id, struct timespec* tp) {
  tp->tv_sec = clock_gettime_retspec.tv_sec;
  tp->tv_nsec = clock_gettime_retspec.tv_nsec;
  return 0;
}

}  // extern "C"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
