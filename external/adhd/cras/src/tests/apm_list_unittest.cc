// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "cras_apm_list.h"
#include "cras_audio_area.h"
#include "cras_dsp_pipeline.h"
#include "cras_iodev.h"
#include "cras_iodev_list.h"
#include "cras_types.h"
#include "float_buffer.h"
#include "webrtc_apm.h"
}

namespace {

static void* stream_ptr = reinterpret_cast<void*>(0x123);
static void* dev_ptr = reinterpret_cast<void*>(0x345);
static void* dev_ptr2 = reinterpret_cast<void*>(0x678);
static struct cras_apm_list* list;
static struct cras_audio_area fake_audio_area;
static unsigned int dsp_util_interleave_frames;
static unsigned int webrtc_apm_process_stream_f_called;
static unsigned int webrtc_apm_process_reverse_stream_f_called;
static device_enabled_callback_t device_enabled_callback_val;
static struct ext_dsp_module* ext_dsp_module_value;
static struct cras_iodev fake_iodev;
static int webrtc_apm_create_called;

TEST(ApmList, ApmListCreate) {
  list = cras_apm_list_create(stream_ptr, 0);
  EXPECT_EQ((void*)NULL, list);

  list = cras_apm_list_create(stream_ptr, APM_ECHO_CANCELLATION);
  EXPECT_NE((void*)NULL, list);
  EXPECT_EQ(APM_ECHO_CANCELLATION, cras_apm_list_get_effects(list));

  cras_apm_list_destroy(list);
}

TEST(ApmList, AddRemoveApm) {
  struct cras_audio_format fmt;

  fmt.num_channels = 2;
  fmt.frame_rate = 48000;
  fmt.format = SND_PCM_FORMAT_S16_LE;

  list = cras_apm_list_create(stream_ptr, APM_ECHO_CANCELLATION);
  EXPECT_NE((void*)NULL, list);

  EXPECT_NE((void*)NULL, cras_apm_list_add(list, dev_ptr, &fmt));
  EXPECT_EQ((void*)NULL, cras_apm_list_get(list, dev_ptr2));

  EXPECT_NE((void*)NULL, cras_apm_list_add(list, dev_ptr2, &fmt));
  EXPECT_NE((void*)NULL, cras_apm_list_get(list, dev_ptr));

  cras_apm_list_remove(list, dev_ptr);
  EXPECT_EQ((void*)NULL, cras_apm_list_get(list, dev_ptr));
  EXPECT_NE((void*)NULL, cras_apm_list_get(list, dev_ptr2));

  cras_apm_list_remove(list, dev_ptr2);
  EXPECT_EQ((void*)NULL, cras_apm_list_get(list, dev_ptr2));

  cras_apm_list_destroy(list);
}

TEST(ApmList, ApmProcessForwardBuffer) {
  struct cras_apm* apm;
  struct cras_audio_format fmt;
  struct cras_audio_area* area;
  struct float_buffer* buf;

  fmt.num_channels = 2;
  fmt.frame_rate = 48000;
  fmt.format = SND_PCM_FORMAT_S16_LE;

  list = cras_apm_list_create(stream_ptr, APM_ECHO_CANCELLATION);
  EXPECT_NE((void*)NULL, list);

  apm = cras_apm_list_add(list, dev_ptr, &fmt);

  buf = float_buffer_create(500, 2);
  float_buffer_written(buf, 300);
  webrtc_apm_process_stream_f_called = 0;
  cras_apm_list_process(apm, buf, 0);
  EXPECT_EQ(0, webrtc_apm_process_stream_f_called);

  area = cras_apm_list_get_processed(apm);
  EXPECT_EQ(0, area->frames);

  float_buffer_reset(buf);
  float_buffer_written(buf, 200);
  cras_apm_list_process(apm, buf, 0);
  area = cras_apm_list_get_processed(apm);
  EXPECT_EQ(1, webrtc_apm_process_stream_f_called);
  EXPECT_EQ(480, dsp_util_interleave_frames);
  EXPECT_EQ(480, area->frames);

  /* Put some processed frames. Another apm_list process will not call
   * into webrtc_apm because the processed buffer is not yet empty.
   */
  cras_apm_list_put_processed(apm, 200);
  float_buffer_reset(buf);
  float_buffer_written(buf, 500);
  cras_apm_list_process(apm, buf, 0);
  EXPECT_EQ(1, webrtc_apm_process_stream_f_called);

  /* Put another 280 processed frames, so it's now ready for webrtc_apm
   * to process another chunk of 480 frames (10ms) data.
   */
  cras_apm_list_put_processed(apm, 280);
  cras_apm_list_process(apm, buf, 0);
  EXPECT_EQ(2, webrtc_apm_process_stream_f_called);

  float_buffer_destroy(&buf);
  cras_apm_list_destroy(list);
}

TEST(ApmList, ApmProcessReverseData) {
  struct cras_apm* apm;
  struct cras_audio_format fmt;
  struct float_buffer* buf;
  float* const* rp;
  unsigned int nread;
  struct cras_iodev fake_iodev;

  fmt.num_channels = 2;
  fmt.frame_rate = 48000;
  fmt.format = SND_PCM_FORMAT_S16_LE;

  fake_iodev.direction = CRAS_STREAM_OUTPUT;
  device_enabled_callback_val = NULL;
  ext_dsp_module_value = NULL;
  webrtc_apm_process_reverse_stream_f_called = 0;

  cras_apm_list_init("");
  EXPECT_NE((void*)NULL, device_enabled_callback_val);

  device_enabled_callback_val(&fake_iodev, NULL);
  EXPECT_NE((void*)NULL, ext_dsp_module_value);
  EXPECT_NE((void*)NULL, ext_dsp_module_value->ports);

  buf = float_buffer_create(500, 2);
  float_buffer_written(buf, 500);
  nread = 500;
  rp = float_buffer_read_pointer(buf, 0, &nread);

  for (int i = 0; i < buf->num_channels; i++)
    ext_dsp_module_value->ports[i] = rp[i];

  ext_dsp_module_value->configure(ext_dsp_module_value, 800, 2, 48000);
  ext_dsp_module_value->run(ext_dsp_module_value, 500);
  EXPECT_EQ(0, webrtc_apm_process_reverse_stream_f_called);

  list = cras_apm_list_create(stream_ptr, APM_ECHO_CANCELLATION);
  EXPECT_NE((void*)NULL, list);

  apm = cras_apm_list_add(list, dev_ptr, &fmt);

  ext_dsp_module_value->run(ext_dsp_module_value, 250);
  EXPECT_EQ(0, webrtc_apm_process_reverse_stream_f_called);

  ext_dsp_module_value->run(ext_dsp_module_value, 250);
  EXPECT_EQ(1, webrtc_apm_process_reverse_stream_f_called);

  float_buffer_destroy(&buf);
  cras_apm_list_destroy(list);
  cras_apm_list_deinit();
}

TEST(ApmList, StreamAddToAlreadyOpenedDev) {
  struct cras_audio_format fmt;
  struct cras_apm *apm1, *apm2;

  fmt.num_channels = 2;
  fmt.frame_rate = 48000;
  fmt.format = SND_PCM_FORMAT_S16_LE;

  webrtc_apm_create_called = 0;
  list = cras_apm_list_create(stream_ptr, APM_ECHO_CANCELLATION);
  EXPECT_NE((void*)NULL, list);

  apm1 = cras_apm_list_add(list, dev_ptr, &fmt);
  EXPECT_EQ(1, webrtc_apm_create_called);
  EXPECT_NE((void*)NULL, apm1);

  apm2 = cras_apm_list_add(list, dev_ptr, &fmt);
  EXPECT_EQ(1, webrtc_apm_create_called);
  EXPECT_EQ(apm1, apm2);

  cras_apm_list_destroy(list);
}

extern "C" {
int cras_iodev_list_set_device_enabled_callback(
    device_enabled_callback_t enabled_cb,
    device_disabled_callback_t disabled_cb,
    void* cb_data) {
  device_enabled_callback_val = enabled_cb;
  return 0;
}
struct cras_iodev* cras_iodev_list_get_first_enabled_iodev(
    enum CRAS_STREAM_DIRECTION direction) {
  return &fake_iodev;
}
void cras_iodev_set_ext_dsp_module(struct cras_iodev* iodev,
                                   struct ext_dsp_module* ext) {
  ext_dsp_module_value = ext;
}
struct cras_audio_area* cras_audio_area_create(int num_channels) {
  return &fake_audio_area;
}

void cras_audio_area_destroy(struct cras_audio_area* area) {}
void cras_audio_area_config_channels(struct cras_audio_area* area,
                                     const struct cras_audio_format* fmt) {}
void cras_audio_area_config_buf_pointers(struct cras_audio_area* area,
                                         const struct cras_audio_format* fmt,
                                         uint8_t* base_buffer) {}
void dsp_util_interleave(float* const* input,
                         int16_t* output,
                         int channels,
                         snd_pcm_format_t format,
                         int frames) {
  dsp_util_interleave_frames = frames;
}
struct aec_config* aec_config_get(const char* device_config_dir) {
  return NULL;
}
void aec_config_dump(struct aec_config* config) {}
struct apm_config* apm_config_get(const char* device_config_dir) {
  return NULL;
}
void apm_config_dump(struct apm_config* config) {}
webrtc_apm webrtc_apm_create(unsigned int num_channels,
                             unsigned int frame_rate,
                             dictionary* aec_ini,
                             dictionary* apm_ini) {
  webrtc_apm_create_called++;
  return reinterpret_cast<webrtc_apm>(0x11);
}
void webrtc_apm_dump_configs(dictionary* aec_ini, dictionary* apm_ini) {}
void webrtc_apm_destroy(webrtc_apm apm) {
  return;
}
int webrtc_apm_process_stream_f(webrtc_apm ptr,
                                int num_channels,
                                int rate,
                                float* const* data) {
  webrtc_apm_process_stream_f_called++;
  return 0;
}

int webrtc_apm_process_reverse_stream_f(webrtc_apm ptr,
                                        int num_channels,
                                        int rate,
                                        float* const* data) {
  webrtc_apm_process_reverse_stream_f_called++;
  return 0;
}
int webrtc_apm_aec_dump(webrtc_apm ptr,
                        void** work_queue,
                        int start,
                        FILE* handle) {
  return 0;
}

}  // extern "C"
}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
