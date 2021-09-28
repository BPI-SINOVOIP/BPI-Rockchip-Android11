// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "buffer_share.c"
#include "cras_audio_area.h"
#include "cras_rstream.h"
#include "input_data.h"
}

namespace {

#ifdef HAVE_WEBRTC_APM
static struct cras_audio_area apm_area;
static unsigned int cras_apm_list_process_offset_val;
static unsigned int cras_apm_list_process_called;
static struct cras_apm* cras_apm_list_get_ret = NULL;
#endif  // HAVE_WEBRTC_APM

TEST(InputData, GetForInputStream) {
  void* dev_ptr = reinterpret_cast<void*>(0x123);
  struct input_data* data;
  struct cras_rstream stream;
  struct buffer_share* offsets;
  struct cras_audio_area* area;
  struct cras_audio_area dev_area;
  unsigned int offset;

#ifdef HAVE_WEBRTC_APM
  cras_apm_list_process_called = 0;
#endif  // HAVE_WEBRTC_APM
  stream.stream_id = 111;

  data = input_data_create(dev_ptr);
  data->ext.configure(&data->ext, 8192, 2, 48000);

  // Prepare offsets data for 2 streams.
  offsets = buffer_share_create(8192);
  buffer_share_add_id(offsets, 111, NULL);
  buffer_share_add_id(offsets, 222, NULL);
  buffer_share_offset_update(offsets, 111, 2048);

  dev_area.frames = 600;
  data->area = &dev_area;

  stream.apm_list = NULL;
  input_data_get_for_stream(data, &stream, offsets, &area, &offset);

  // Assert offset is clipped by area->frames
  EXPECT_EQ(600, area->frames);
  EXPECT_EQ(600, offset);
#ifdef HAVE_WEBRTC_APM
  EXPECT_EQ(0, cras_apm_list_process_called);
  cras_apm_list_get_ret = reinterpret_cast<struct cras_apm*>(0x99);
#endif  // HAVE_WEBRTC_APM

  input_data_get_for_stream(data, &stream, offsets, &area, &offset);

#ifdef HAVE_WEBRTC_APM
  // Assert APM process uses correct stream offset not the clipped one
  // used for audio area.
  EXPECT_EQ(1, cras_apm_list_process_called);
  EXPECT_EQ(2048, cras_apm_list_process_offset_val);
  EXPECT_EQ(0, offset);
#else
  // Without the APM, the offset shouldn't be changed.
  EXPECT_EQ(600, offset);
#endif  // HAVE_WEBRTC_APM

  input_data_destroy(&data);
  buffer_share_destroy(offsets);
}

extern "C" {
#ifdef HAVE_WEBRTC_APM
struct cras_apm* cras_apm_list_get(struct cras_apm_list* list, void* dev_ptr) {
  return cras_apm_list_get_ret;
}
int cras_apm_list_process(struct cras_apm* apm,
                          struct float_buffer* input,
                          unsigned int offset) {
  cras_apm_list_process_called++;
  cras_apm_list_process_offset_val = offset;
  return 0;
}

struct cras_audio_area* cras_apm_list_get_processed(struct cras_apm* apm) {
  return &apm_area;
}
void cras_apm_list_remove(struct cras_apm_list* list, void* dev_ptr) {}
void cras_apm_list_put_processed(struct cras_apm* apm, unsigned int frames) {}
#endif  // HAVE_WEBRTC_APM

}  // extern "C"
}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
