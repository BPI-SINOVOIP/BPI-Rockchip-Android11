// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "audio_thread_log.h"
#include "cras_audio_area.h"
#include "cras_iodev.h"
#include "cras_ramp.h"
#include "cras_rstream.h"
#include "dev_stream.h"
#include "input_data.h"
#include "utlist.h"

// Mock software volume scalers.
float softvol_scalers[101];

// For audio_thread_log.h use.
int atlog_rw_shm_fd;
int atlog_ro_shm_fd;
}

#define BUFFER_SIZE 8192

static const float RAMP_UNMUTE_DURATION_SECS = 0.5;
static const float RAMP_NEW_STREAM_DURATION_SECS = 0.01;
static const float RAMP_MUTE_DURATION_SECS = 0.1;
static const float RAMP_VOLUME_CHANGE_DURATION_SECS = 0.1;

static int cras_iodev_list_disable_dev_called;
static int select_node_called;
static enum CRAS_STREAM_DIRECTION select_node_direction;
static cras_node_id_t select_node_id;
static struct cras_ionode* node_selected;
static size_t notify_nodes_changed_called;
static size_t notify_active_node_changed_called;
static int dsp_context_new_sample_rate;
static const char* dsp_context_new_purpose;
static int dsp_context_free_called;
static int update_channel_layout_called;
static int update_channel_layout_return_val;
static int cras_audio_format_set_channel_layout_called;
static unsigned int cras_system_get_volume_return;
static int cras_dsp_get_pipeline_called;
static int cras_dsp_get_pipeline_ret;
static int cras_dsp_put_pipeline_called;
static int cras_dsp_pipeline_get_source_buffer_called;
static int cras_dsp_pipeline_get_sink_buffer_called;
static float cras_dsp_pipeline_source_buffer[2][DSP_BUFFER_SIZE];
static float cras_dsp_pipeline_sink_buffer[2][DSP_BUFFER_SIZE];
static int cras_dsp_pipeline_get_delay_called;
static int cras_dsp_pipeline_apply_called;
static int cras_dsp_pipeline_set_sink_ext_module_called;
static int cras_dsp_pipeline_apply_sample_count;
static unsigned int cras_mix_mute_count;
static unsigned int cras_dsp_num_input_channels_return;
static unsigned int cras_dsp_num_output_channels_return;
struct cras_dsp_context* cras_dsp_context_new_return;
static unsigned int cras_dsp_load_dummy_pipeline_called;
static unsigned int rate_estimator_add_frames_num_frames;
static unsigned int rate_estimator_add_frames_called;
static int cras_system_get_mute_return;
static snd_pcm_format_t cras_scale_buffer_fmt;
static float cras_scale_buffer_scaler;
static int cras_scale_buffer_called;
static unsigned int pre_dsp_hook_called;
static const uint8_t* pre_dsp_hook_frames;
static void* pre_dsp_hook_cb_data;
static unsigned int post_dsp_hook_called;
static const uint8_t* post_dsp_hook_frames;
static void* post_dsp_hook_cb_data;
static int iodev_buffer_size;
static long cras_system_get_capture_gain_ret_value;
static uint8_t audio_buffer[BUFFER_SIZE];
static struct cras_audio_area* audio_area;
static unsigned int put_buffer_nframes;
static int is_free_running_ret;
static int no_stream_called;
static int no_stream_enable;
// This will be used extensively in cras_iodev.
struct audio_thread_event_log* atlog;
static unsigned int simple_no_stream_called;
static int simple_no_stream_enable;
static int dev_stream_playback_frames_ret;
static int get_num_underruns_ret;
static int device_monitor_reset_device_called;
static int output_underrun_called;
static int set_mute_called;
static int cras_ramp_start_mute_ramp;
static float cras_ramp_start_from;
static float cras_ramp_start_to;
static int cras_ramp_start_duration_frames;
static int cras_ramp_start_is_called;
static int cras_ramp_reset_is_called;
static struct cras_ramp_action cras_ramp_get_current_action_ret;
static int cras_ramp_update_ramped_frames_num_frames;
static cras_ramp_cb cras_ramp_start_cb;
static void* cras_ramp_start_cb_data;
static int cras_device_monitor_set_device_mute_state_called;
unsigned int cras_device_monitor_set_device_mute_state_dev_idx;
static snd_pcm_format_t cras_scale_buffer_increment_fmt;
static uint8_t* cras_scale_buffer_increment_buff;
static unsigned int cras_scale_buffer_increment_frame;
static float cras_scale_buffer_increment_scaler;
static float cras_scale_buffer_increment_increment;
static float cras_scale_buffer_increment_target;
static int cras_scale_buffer_increment_channel;
static struct cras_audio_format audio_fmt;
static int buffer_share_add_id_called;
static int buffer_share_get_new_write_point_ret;
static int ext_mod_configure_called;
static struct input_data* input_data_create_ret;
static double rate_estimator_get_rate_ret;

static char* atlog_name;

// Iodev callback
int update_channel_layout(struct cras_iodev* iodev) {
  update_channel_layout_called = 1;
  return update_channel_layout_return_val;
}

void ResetStubData() {
  cras_iodev_list_disable_dev_called = 0;
  select_node_called = 0;
  notify_nodes_changed_called = 0;
  notify_active_node_changed_called = 0;
  dsp_context_new_sample_rate = 0;
  dsp_context_new_purpose = NULL;
  dsp_context_free_called = 0;
  cras_audio_format_set_channel_layout_called = 0;
  cras_dsp_get_pipeline_called = 0;
  cras_dsp_get_pipeline_ret = 0;
  cras_dsp_put_pipeline_called = 0;
  cras_dsp_pipeline_get_source_buffer_called = 0;
  cras_dsp_pipeline_get_sink_buffer_called = 0;
  memset(&cras_dsp_pipeline_source_buffer, 0,
         sizeof(cras_dsp_pipeline_source_buffer));
  memset(&cras_dsp_pipeline_sink_buffer, 0,
         sizeof(cras_dsp_pipeline_sink_buffer));
  cras_dsp_pipeline_get_delay_called = 0;
  cras_dsp_pipeline_apply_called = 0;
  cras_dsp_pipeline_set_sink_ext_module_called = 0;
  cras_dsp_pipeline_apply_sample_count = 0;
  cras_dsp_num_input_channels_return = 2;
  cras_dsp_num_output_channels_return = 2;
  cras_dsp_context_new_return = NULL;
  cras_dsp_load_dummy_pipeline_called = 0;
  rate_estimator_add_frames_num_frames = 0;
  rate_estimator_add_frames_called = 0;
  cras_system_get_mute_return = 0;
  cras_system_get_volume_return = 100;
  cras_mix_mute_count = 0;
  pre_dsp_hook_called = 0;
  pre_dsp_hook_frames = NULL;
  post_dsp_hook_called = 0;
  post_dsp_hook_frames = NULL;
  iodev_buffer_size = 0;
  cras_system_get_capture_gain_ret_value = 0;
  // Assume there is some data in audio buffer.
  memset(audio_buffer, 0xff, sizeof(audio_buffer));
  if (audio_area) {
    free(audio_area);
    audio_area = NULL;
  }
  put_buffer_nframes = 0;
  is_free_running_ret = 0;
  no_stream_called = 0;
  no_stream_enable = 0;
  simple_no_stream_called = 0;
  simple_no_stream_enable = 0;
  dev_stream_playback_frames_ret = 0;
  if (!atlog) {
    if (asprintf(&atlog_name, "/ATlog-%d", getpid()) < 0) {
      exit(-1);
    }
    /* To avoid un-used variable warning. */
    atlog_rw_shm_fd = atlog_ro_shm_fd = -1;
    atlog = audio_thread_event_log_init(atlog_name);
  }
  get_num_underruns_ret = 0;
  device_monitor_reset_device_called = 0;
  output_underrun_called = 0;
  set_mute_called = 0;
  cras_ramp_start_mute_ramp = 0;
  cras_ramp_start_from = 0.0;
  cras_ramp_start_to = 0.0;
  cras_ramp_start_duration_frames = 0;
  cras_ramp_start_cb = NULL;
  cras_ramp_start_cb_data = NULL;
  cras_ramp_start_is_called = 0;
  cras_ramp_reset_is_called = 0;
  cras_ramp_get_current_action_ret.type = CRAS_RAMP_ACTION_NONE;
  cras_ramp_update_ramped_frames_num_frames = 0;
  cras_device_monitor_set_device_mute_state_called = 0;
  cras_device_monitor_set_device_mute_state_dev_idx = 0;
  cras_scale_buffer_called = 0;
  cras_scale_buffer_increment_fmt = SND_PCM_FORMAT_UNKNOWN;
  cras_scale_buffer_increment_buff = NULL;
  cras_scale_buffer_increment_frame = 0;
  cras_scale_buffer_increment_scaler = 0;
  cras_scale_buffer_increment_increment = 0;
  cras_scale_buffer_increment_target = 0.0;
  cras_scale_buffer_increment_channel = 0;
  audio_fmt.format = SND_PCM_FORMAT_S16_LE;
  audio_fmt.frame_rate = 48000;
  audio_fmt.num_channels = 2;
  buffer_share_add_id_called = 0;
  ext_mod_configure_called = 0;
  rate_estimator_get_rate_ret = 0;
}

namespace {

//  Test fill_time_from_frames
TEST(IoDevTestSuite, FillTimeFromFramesNormal) {
  struct timespec ts;

  cras_iodev_fill_time_from_frames(12000, 48000, &ts);
  EXPECT_EQ(0, ts.tv_sec);
  EXPECT_GE(ts.tv_nsec, 249900000);
  EXPECT_LE(ts.tv_nsec, 250100000);
}

TEST(IoDevTestSuite, FillTimeFromFramesLong) {
  struct timespec ts;

  cras_iodev_fill_time_from_frames(120000 - 12000, 48000, &ts);
  EXPECT_EQ(2, ts.tv_sec);
  EXPECT_GE(ts.tv_nsec, 249900000);
  EXPECT_LE(ts.tv_nsec, 250100000);
}

TEST(IoDevTestSuite, FillTimeFromFramesShort) {
  struct timespec ts;

  cras_iodev_fill_time_from_frames(12000 - 12000, 48000, &ts);
  EXPECT_EQ(0, ts.tv_sec);
  EXPECT_EQ(0, ts.tv_nsec);
}

class IoDevSetFormatTestSuite : public testing::Test {
 protected:
  virtual void SetUp() {
    ResetStubData();
    sample_rates_[0] = 44100;
    sample_rates_[1] = 48000;
    sample_rates_[2] = 0;

    channel_counts_[0] = 2;
    channel_counts_[1] = 0;
    channel_counts_[2] = 0;

    pcm_formats_[0] = SND_PCM_FORMAT_S16_LE;
    pcm_formats_[1] = SND_PCM_FORMAT_S32_LE;
    pcm_formats_[2] = static_cast<snd_pcm_format_t>(0);

    update_channel_layout_called = 0;
    update_channel_layout_return_val = 0;

    memset(&iodev_, 0, sizeof(iodev_));
    iodev_.update_channel_layout = update_channel_layout;
    iodev_.supported_rates = sample_rates_;
    iodev_.supported_channel_counts = channel_counts_;
    iodev_.supported_formats = pcm_formats_;
    iodev_.dsp_context = NULL;

    cras_audio_format_set_channel_layout_called = 0;
  }

  virtual void TearDown() { cras_iodev_free_format(&iodev_); }

  struct cras_iodev iodev_;
  size_t sample_rates_[3];
  size_t channel_counts_[3];
  snd_pcm_format_t pcm_formats_[3];
};

TEST_F(IoDevSetFormatTestSuite, SupportedFormatSecondary) {
  struct cras_audio_format fmt;
  int rc;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev_.direction = CRAS_STREAM_OUTPUT;
  ResetStubData();
  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(48000, iodev_.format->frame_rate);
  EXPECT_EQ(2, iodev_.format->num_channels);
  EXPECT_EQ(dsp_context_new_sample_rate, 48000);
  EXPECT_STREQ(dsp_context_new_purpose, "playback");
}

TEST_F(IoDevSetFormatTestSuite, SupportedFormat32bit) {
  struct cras_audio_format fmt;
  int rc;

  fmt.format = SND_PCM_FORMAT_S32_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev_.direction = CRAS_STREAM_OUTPUT;
  ResetStubData();
  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S32_LE, iodev_.format->format);
  EXPECT_EQ(48000, iodev_.format->frame_rate);
  EXPECT_EQ(2, iodev_.format->num_channels);
  EXPECT_EQ(dsp_context_new_sample_rate, 48000);
  EXPECT_STREQ(dsp_context_new_purpose, "playback");
}

TEST_F(IoDevSetFormatTestSuite, SupportedFormatPrimary) {
  struct cras_audio_format fmt;
  int rc;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 44100;
  fmt.num_channels = 2;
  iodev_.direction = CRAS_STREAM_INPUT;
  ResetStubData();
  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(44100, iodev_.format->frame_rate);
  EXPECT_EQ(2, iodev_.format->num_channels);
  EXPECT_EQ(dsp_context_new_sample_rate, 44100);
  EXPECT_STREQ(dsp_context_new_purpose, "capture");
}

TEST_F(IoDevSetFormatTestSuite, SupportedFormatDivisor) {
  struct cras_audio_format fmt;
  int rc;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 96000;
  fmt.num_channels = 2;
  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(48000, iodev_.format->frame_rate);
  EXPECT_EQ(2, iodev_.format->num_channels);
}

TEST_F(IoDevSetFormatTestSuite, Supported96k) {
  struct cras_audio_format fmt;
  int rc;

  sample_rates_[0] = 48000;
  sample_rates_[1] = 96000;
  sample_rates_[2] = 0;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 96000;
  fmt.num_channels = 2;
  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(96000, iodev_.format->frame_rate);
  EXPECT_EQ(2, iodev_.format->num_channels);
}

TEST_F(IoDevSetFormatTestSuite, LimitLowRate) {
  struct cras_audio_format fmt;
  int rc;

  sample_rates_[0] = 48000;
  sample_rates_[1] = 8000;
  sample_rates_[2] = 0;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 8000;
  fmt.num_channels = 2;
  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(48000, iodev_.format->frame_rate);
  EXPECT_EQ(2, iodev_.format->num_channels);
}

TEST_F(IoDevSetFormatTestSuite, UnsupportedChannelCount) {
  struct cras_audio_format fmt;
  int rc;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 96000;
  fmt.num_channels = 1;
  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(48000, iodev_.format->frame_rate);
  EXPECT_EQ(2, iodev_.format->num_channels);
}

TEST_F(IoDevSetFormatTestSuite, SupportedFormatFallbackDefault) {
  struct cras_audio_format fmt;
  int rc;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 96008;
  fmt.num_channels = 2;
  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(44100, iodev_.format->frame_rate);
  EXPECT_EQ(2, iodev_.format->num_channels);
}

TEST_F(IoDevSetFormatTestSuite, UpdateChannelLayoutSuccess) {
  struct cras_audio_format fmt;
  int rc;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 6;

  iodev_.supported_channel_counts[0] = 6;
  iodev_.supported_channel_counts[1] = 2;

  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(48000, iodev_.format->frame_rate);
  EXPECT_EQ(6, iodev_.format->num_channels);
}

TEST_F(IoDevSetFormatTestSuite, UpdateChannelLayoutFail) {
  static const int8_t stereo_layout[] = {0,  1,  -1, -1, -1, -1,
                                         -1, -1, -1, -1, -1};
  struct cras_audio_format fmt;
  int rc, i;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;

  cras_dsp_context_new_return = reinterpret_cast<cras_dsp_context*>(0xf0f);

  update_channel_layout_return_val = -1;
  iodev_.supported_channel_counts[0] = 6;
  iodev_.supported_channel_counts[1] = 2;

  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(48000, iodev_.format->frame_rate);
  EXPECT_EQ(2, iodev_.format->num_channels);
  EXPECT_EQ(0, dsp_context_free_called);
  for (i = 0; i < CRAS_CH_MAX; i++)
    EXPECT_EQ(iodev_.format->channel_layout[i], stereo_layout[i]);
}

TEST_F(IoDevSetFormatTestSuite, UpdateChannelLayoutFail6ch) {
  static const int8_t default_6ch_layout[] = {0,  1,  2,  3,  4, 5,
                                              -1, -1, -1, -1, -1};
  struct cras_audio_format fmt;
  int rc, i;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 6;

  cras_dsp_context_new_return = reinterpret_cast<cras_dsp_context*>(0xf0f);

  update_channel_layout_return_val = -1;
  iodev_.supported_channel_counts[0] = 6;
  iodev_.supported_channel_counts[1] = 2;

  rc = cras_iodev_set_format(&iodev_, &fmt);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, iodev_.format->format);
  EXPECT_EQ(48000, iodev_.format->frame_rate);
  EXPECT_EQ(6, iodev_.format->num_channels);
  EXPECT_EQ(0, dsp_context_free_called);
  for (i = 0; i < CRAS_CH_MAX; i++)
    EXPECT_EQ(iodev_.format->channel_layout[i], default_6ch_layout[i]);
}

// Put buffer tests

static int get_buffer(cras_iodev* iodev,
                      struct cras_audio_area** area,
                      unsigned int* num) {
  size_t sz = sizeof(*audio_area) + sizeof(struct cras_channel_area) * 2;

  audio_area = (cras_audio_area*)calloc(1, sz);
  audio_area->frames = *num;
  audio_area->num_channels = 2;
  audio_area->channels[0].buf = audio_buffer;
  channel_area_set_channel(&audio_area->channels[0], CRAS_CH_FL);
  audio_area->channels[0].step_bytes = 4;
  audio_area->channels[1].buf = audio_buffer + 2;
  channel_area_set_channel(&audio_area->channels[1], CRAS_CH_FR);
  audio_area->channels[1].step_bytes = 4;

  *area = audio_area;
  return 0;
}

static int put_buffer(struct cras_iodev* iodev, unsigned int nframes) {
  put_buffer_nframes = nframes;
  if (audio_area) {
    free(audio_area);
    audio_area = NULL;
  }
  return 0;
}

static int no_stream(struct cras_iodev* odev, int enable) {
  no_stream_called++;
  no_stream_enable = enable;
  // Use default no stream playback to test default behavior.
  return cras_iodev_default_no_stream_playback(odev, enable);
}

static int is_free_running(const struct cras_iodev* odev) {
  return is_free_running_ret;
}

static int pre_dsp_hook(const uint8_t* frames,
                        unsigned int nframes,
                        const struct cras_audio_format* fmt,
                        void* cb_data) {
  pre_dsp_hook_called++;
  pre_dsp_hook_frames = frames;
  pre_dsp_hook_cb_data = cb_data;
  return 0;
}

static int post_dsp_hook(const uint8_t* frames,
                         unsigned int nframes,
                         const struct cras_audio_format* fmt,
                         void* cb_data) {
  post_dsp_hook_called++;
  post_dsp_hook_frames = frames;
  post_dsp_hook_cb_data = cb_data;
  return 0;
}

static int loopback_hook_control(bool start, void* cb_data) {
  return 0;
}

TEST(IoDevPutOutputBuffer, SystemMuted) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  cras_system_get_mute_return = 1;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);

  rc = cras_iodev_put_output_buffer(&iodev, frames, 20, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(20, cras_mix_mute_count);
  EXPECT_EQ(20, put_buffer_nframes);
  EXPECT_EQ(20, rate_estimator_add_frames_num_frames);
}

TEST(IoDevPutOutputBuffer, MuteForVolume) {
  struct cras_iodev iodev;
  struct cras_ionode ionode;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  memset(&ionode, 0, sizeof(ionode));

  iodev.nodes = &ionode;
  iodev.active_node = &ionode;
  iodev.active_node->dev = &iodev;

  // Case: System volume 100; Node volume 0. => Mute
  cras_system_get_volume_return = 100;
  iodev.active_node->volume = 0;
  EXPECT_EQ(1, cras_iodev_is_zero_volume(&iodev));

  // Case: System volume 100; Node volume 50. => Not mute
  cras_system_get_volume_return = 100;
  iodev.active_node->volume = 50;
  EXPECT_EQ(0, cras_iodev_is_zero_volume(&iodev));

  // Case: System volume 0; Node volume 50. => Mute
  cras_system_get_volume_return = 0;
  iodev.active_node->volume = 50;
  EXPECT_EQ(1, cras_iodev_is_zero_volume(&iodev));

  // Case: System volume 50; Node volume 50. => Mute
  cras_system_get_volume_return = 50;
  iodev.active_node->volume = 50;
  EXPECT_EQ(1, cras_iodev_is_zero_volume(&iodev));
}

TEST(IoDevPutOutputBuffer, NodeVolumeZeroShouldMute) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  struct cras_ionode ionode;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  memset(&ionode, 0, sizeof(ionode));

  iodev.nodes = &ionode;
  iodev.active_node = &ionode;
  iodev.active_node->dev = &iodev;
  iodev.active_node->volume = 0;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);

  rc = cras_iodev_put_output_buffer(&iodev, frames, 20, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(20, cras_mix_mute_count);
  EXPECT_EQ(20, put_buffer_nframes);
  EXPECT_EQ(20, rate_estimator_add_frames_num_frames);
}

TEST(IoDevPutOutputBuffer, SystemMutedWithRamp) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  cras_system_get_mute_return = 1;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;

  // Assume device has ramp member.
  iodev.ramp = reinterpret_cast<struct cras_ramp*>(0x1);
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);

  // Assume ramping is done.
  cras_ramp_get_current_action_ret.type = CRAS_RAMP_ACTION_NONE;

  rc = cras_iodev_put_output_buffer(&iodev, frames, 20, NULL, nullptr);
  // Output should be muted.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(20, cras_mix_mute_count);
  EXPECT_EQ(20, put_buffer_nframes);
  EXPECT_EQ(20, rate_estimator_add_frames_num_frames);

  // Test for the case where ramping is not done yet.
  ResetStubData();
  cras_ramp_get_current_action_ret.type = CRAS_RAMP_ACTION_PARTIAL;
  rc = cras_iodev_put_output_buffer(&iodev, frames, 20, NULL, nullptr);

  // Output should not be muted.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  // Ramped frames should be increased by 20.
  EXPECT_EQ(20, cras_ramp_update_ramped_frames_num_frames);
  EXPECT_EQ(20, put_buffer_nframes);
  EXPECT_EQ(20, rate_estimator_add_frames_num_frames);
}

TEST(IoDevPutOutputBuffer, NodeVolumeZeroShouldMuteWithRamp) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  struct cras_ionode ionode;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  memset(&ionode, 0, sizeof(ionode));

  iodev.nodes = &ionode;
  iodev.active_node = &ionode;
  iodev.active_node->dev = &iodev;
  iodev.active_node->volume = 0;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);

  // Assume device has ramp member.
  iodev.ramp = reinterpret_cast<struct cras_ramp*>(0x1);

  // Assume ramping is done.
  cras_ramp_get_current_action_ret.type = CRAS_RAMP_ACTION_NONE;

  rc = cras_iodev_put_output_buffer(&iodev, frames, 20, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(20, cras_mix_mute_count);
  EXPECT_EQ(20, put_buffer_nframes);
  EXPECT_EQ(20, rate_estimator_add_frames_num_frames);

  // Test for the case where ramping is not done yet.
  ResetStubData();
  cras_ramp_get_current_action_ret.type = CRAS_RAMP_ACTION_PARTIAL;
  rc = cras_iodev_put_output_buffer(&iodev, frames, 20, NULL, nullptr);

  // Output should not be muted.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  // Ramped frames should be increased by 20.
  EXPECT_EQ(20, cras_ramp_update_ramped_frames_num_frames);
  EXPECT_EQ(20, put_buffer_nframes);
  EXPECT_EQ(20, rate_estimator_add_frames_num_frames);
}
TEST(IoDevPutOutputBuffer, NoDSP) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  struct cras_ionode ionode;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  memset(&ionode, 0, sizeof(ionode));

  iodev.nodes = &ionode;
  iodev.active_node = &ionode;
  iodev.active_node->dev = &iodev;
  iodev.active_node->volume = 100;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);

  rc = cras_iodev_put_output_buffer(&iodev, frames, 22, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  EXPECT_EQ(22, put_buffer_nframes);
  EXPECT_EQ(22, rate_estimator_add_frames_num_frames);
}

TEST(IoDevPutOutputBuffer, DSP) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;
  struct cras_loopback pre_dsp;
  struct cras_loopback post_dsp;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  iodev.dsp_context = reinterpret_cast<cras_dsp_context*>(0x15);
  cras_dsp_get_pipeline_ret = 0x25;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);
  pre_dsp.type = LOOPBACK_POST_MIX_PRE_DSP;
  pre_dsp.hook_data = pre_dsp_hook;
  pre_dsp.hook_control = loopback_hook_control;
  pre_dsp.cb_data = (void*)0x1234;
  DL_APPEND(iodev.loopbacks, &pre_dsp);
  post_dsp.type = LOOPBACK_POST_DSP;
  post_dsp.hook_data = post_dsp_hook;
  post_dsp.hook_control = loopback_hook_control;
  post_dsp.cb_data = (void*)0x5678;
  DL_APPEND(iodev.loopbacks, &post_dsp);

  rc = cras_iodev_put_output_buffer(&iodev, frames, 32, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  EXPECT_EQ(1, pre_dsp_hook_called);
  EXPECT_EQ(frames, pre_dsp_hook_frames);
  EXPECT_EQ((void*)0x1234, pre_dsp_hook_cb_data);
  EXPECT_EQ(1, post_dsp_hook_called);
  EXPECT_EQ((void*)0x5678, post_dsp_hook_cb_data);
  EXPECT_EQ(32, put_buffer_nframes);
  EXPECT_EQ(32, rate_estimator_add_frames_num_frames);
  EXPECT_EQ(32, cras_dsp_pipeline_apply_sample_count);
  EXPECT_EQ(cras_dsp_get_pipeline_called, cras_dsp_put_pipeline_called);
}

TEST(IoDevPutOutputBuffer, SoftVol) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  iodev.software_volume_needed = 1;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);

  cras_system_get_volume_return = 13;
  softvol_scalers[13] = 0.435;

  rc = cras_iodev_put_output_buffer(&iodev, frames, 53, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  EXPECT_EQ(53, put_buffer_nframes);
  EXPECT_EQ(53, rate_estimator_add_frames_num_frames);
  EXPECT_EQ(softvol_scalers[13], cras_scale_buffer_scaler);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, cras_scale_buffer_fmt);
}

TEST(IoDevPutOutputBuffer, SoftVolWithRamp) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;
  int n_frames = 53;
  float ramp_scaler = 0.2;
  float increment = 0.001;
  float target = 1.0;
  int volume = 13;
  float volume_scaler = 0.435;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  iodev.software_volume_needed = 1;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;
  // Assume device has ramp member.
  iodev.ramp = reinterpret_cast<struct cras_ramp*>(0x1);
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);

  // Assume ramping is done.
  cras_ramp_get_current_action_ret.type = CRAS_RAMP_ACTION_NONE;

  cras_system_get_volume_return = volume;
  softvol_scalers[volume] = volume_scaler;

  rc = cras_iodev_put_output_buffer(&iodev, frames, n_frames, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  EXPECT_EQ(n_frames, put_buffer_nframes);
  EXPECT_EQ(n_frames, rate_estimator_add_frames_num_frames);
  EXPECT_EQ(softvol_scalers[volume], cras_scale_buffer_scaler);
  EXPECT_EQ(SND_PCM_FORMAT_S16_LE, cras_scale_buffer_fmt);

  ResetStubData();
  // Assume ramping is not done.
  cras_ramp_get_current_action_ret.type = CRAS_RAMP_ACTION_PARTIAL;
  cras_ramp_get_current_action_ret.scaler = ramp_scaler;
  cras_ramp_get_current_action_ret.increment = increment;
  cras_ramp_get_current_action_ret.target = target;

  cras_system_get_volume_return = volume;
  softvol_scalers[volume] = volume_scaler;

  rc = cras_iodev_put_output_buffer(&iodev, frames, n_frames, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  // cras_scale_buffer is not called.
  EXPECT_EQ(0, cras_scale_buffer_called);

  // Verify the arguments passed to cras_scale_buffer_increment.
  EXPECT_EQ(fmt.format, cras_scale_buffer_increment_fmt);
  EXPECT_EQ(frames, cras_scale_buffer_increment_buff);
  EXPECT_EQ(n_frames, cras_scale_buffer_increment_frame);
  // Initial scaler will be product of software volume scaler and
  // ramp scaler.
  EXPECT_FLOAT_EQ(softvol_scalers[volume] * ramp_scaler,
                  cras_scale_buffer_increment_scaler);
  // Increment scaler will be product of software volume scaler and
  // ramp increment.
  EXPECT_FLOAT_EQ(softvol_scalers[volume] * increment,
                  cras_scale_buffer_increment_increment);
  EXPECT_FLOAT_EQ(softvol_scalers[volume] * target,
                  cras_scale_buffer_increment_target);
  EXPECT_EQ(fmt.num_channels, cras_scale_buffer_increment_channel);

  EXPECT_EQ(n_frames, put_buffer_nframes);
  EXPECT_EQ(n_frames, rate_estimator_add_frames_num_frames);
}

TEST(IoDevPutOutputBuffer, NoSoftVolWithRamp) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;
  int n_frames = 53;
  float ramp_scaler = 0.2;
  float increment = 0.001;
  float target = 1.0;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  iodev.software_volume_needed = 0;

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;
  // Assume device has ramp member.
  iodev.ramp = reinterpret_cast<struct cras_ramp*>(0x1);
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);

  // Assume ramping is done.
  cras_ramp_get_current_action_ret.type = CRAS_RAMP_ACTION_NONE;

  rc = cras_iodev_put_output_buffer(&iodev, frames, n_frames, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  // cras_scale_buffer is not called.
  EXPECT_EQ(0, cras_scale_buffer_called);
  EXPECT_EQ(n_frames, put_buffer_nframes);
  EXPECT_EQ(n_frames, rate_estimator_add_frames_num_frames);

  ResetStubData();
  // Assume ramping is not done.
  cras_ramp_get_current_action_ret.type = CRAS_RAMP_ACTION_PARTIAL;
  cras_ramp_get_current_action_ret.scaler = ramp_scaler;
  cras_ramp_get_current_action_ret.increment = increment;
  cras_ramp_get_current_action_ret.target = target;

  rc = cras_iodev_put_output_buffer(&iodev, frames, n_frames, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  // cras_scale_buffer is not called.
  EXPECT_EQ(0, cras_scale_buffer_called);

  // Verify the arguments passed to cras_scale_buffer_increment.
  EXPECT_EQ(fmt.format, cras_scale_buffer_increment_fmt);
  EXPECT_EQ(frames, cras_scale_buffer_increment_buff);
  EXPECT_EQ(n_frames, cras_scale_buffer_increment_frame);
  EXPECT_FLOAT_EQ(ramp_scaler, cras_scale_buffer_increment_scaler);
  EXPECT_FLOAT_EQ(increment, cras_scale_buffer_increment_increment);
  EXPECT_FLOAT_EQ(1.0, cras_scale_buffer_increment_target);
  EXPECT_EQ(fmt.num_channels, cras_scale_buffer_increment_channel);

  EXPECT_EQ(n_frames, put_buffer_nframes);
  EXPECT_EQ(n_frames, rate_estimator_add_frames_num_frames);
}

TEST(IoDevPutOutputBuffer, Scale32Bit) {
  struct cras_audio_format fmt;
  struct cras_iodev iodev;
  uint8_t* frames = reinterpret_cast<uint8_t*>(0x44);
  int rc;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  iodev.software_volume_needed = 1;

  cras_system_get_volume_return = 13;
  softvol_scalers[13] = 0.435;

  fmt.format = SND_PCM_FORMAT_S32_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.put_buffer = put_buffer;
  iodev.rate_est = reinterpret_cast<struct rate_estimator*>(0xdeadbeef);

  rc = cras_iodev_put_output_buffer(&iodev, frames, 53, NULL, nullptr);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_mix_mute_count);
  EXPECT_EQ(53, put_buffer_nframes);
  EXPECT_EQ(53, rate_estimator_add_frames_num_frames);
  EXPECT_EQ(SND_PCM_FORMAT_S32_LE, cras_scale_buffer_fmt);
}

// frames queued/avail tests

static unsigned fr_queued = 0;

static int frames_queued(const struct cras_iodev* iodev,
                         struct timespec* tstamp) {
  clock_gettime(CLOCK_MONOTONIC_RAW, tstamp);
  return fr_queued;
}

TEST(IoDevQueuedBuffer, ZeroMinBufferLevel) {
  struct cras_iodev iodev;
  struct timespec tstamp;
  int rc;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.frames_queued = frames_queued;
  iodev.min_buffer_level = 0;
  iodev.buffer_size = 200;
  fr_queued = 50;

  rc = cras_iodev_frames_queued(&iodev, &tstamp);
  EXPECT_EQ(50, rc);
  rc = cras_iodev_buffer_avail(&iodev, rc);
  EXPECT_EQ(150, rc);
}

TEST(IoDevQueuedBuffer, NonZeroMinBufferLevel) {
  struct cras_iodev iodev;
  struct timespec hw_tstamp;
  int rc;

  ResetStubData();
  memset(&iodev, 0, sizeof(iodev));
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.frames_queued = frames_queued;
  iodev.min_buffer_level = 100;
  iodev.buffer_size = 200;
  fr_queued = 180;

  rc = cras_iodev_frames_queued(&iodev, &hw_tstamp);
  EXPECT_EQ(80, rc);
  rc = cras_iodev_buffer_avail(&iodev, rc);
  EXPECT_EQ(20, rc);

  /* When fr_queued < min_buffer_level*/
  fr_queued = 80;
  rc = cras_iodev_frames_queued(&iodev, &hw_tstamp);
  EXPECT_EQ(0, rc);
  rc = cras_iodev_buffer_avail(&iodev, rc);
  EXPECT_EQ(100, rc);
}

static void update_active_node(struct cras_iodev* iodev,
                               unsigned node_idx,
                               unsigned dev_enabled) {}

static void dev_set_mute(struct cras_iodev* iodev) {
  set_mute_called++;
}

TEST(IoNodePlug, PlugUnplugNode) {
  struct cras_iodev iodev;
  struct cras_ionode ionode, ionode2;

  memset(&iodev, 0, sizeof(iodev));
  memset(&ionode, 0, sizeof(ionode));
  memset(&ionode2, 0, sizeof(ionode2));
  iodev.direction = CRAS_STREAM_INPUT;
  iodev.update_active_node = update_active_node;
  ionode.dev = &iodev;
  cras_iodev_add_node(&iodev, &ionode);
  ionode2.dev = &iodev;
  cras_iodev_add_node(&iodev, &ionode2);
  cras_iodev_set_active_node(&iodev, &ionode);
  ResetStubData();
  cras_iodev_set_node_plugged(&ionode, 1);
  EXPECT_EQ(0, cras_iodev_list_disable_dev_called);
  cras_iodev_set_node_plugged(&ionode, 0);
  EXPECT_EQ(1, cras_iodev_list_disable_dev_called);

  /* Unplug non-active node shouldn't disable iodev. */
  cras_iodev_set_node_plugged(&ionode2, 1);
  EXPECT_EQ(1, cras_iodev_list_disable_dev_called);
  cras_iodev_set_node_plugged(&ionode2, 0);
  EXPECT_EQ(1, cras_iodev_list_disable_dev_called);
}

TEST(IoDev, AddRemoveNode) {
  struct cras_iodev iodev;
  struct cras_ionode ionode;

  memset(&iodev, 0, sizeof(iodev));
  memset(&ionode, 0, sizeof(ionode));
  ResetStubData();
  EXPECT_EQ(0, notify_nodes_changed_called);
  cras_iodev_add_node(&iodev, &ionode);
  EXPECT_EQ(1, notify_nodes_changed_called);
  cras_iodev_rm_node(&iodev, &ionode);
  EXPECT_EQ(2, notify_nodes_changed_called);
}

TEST(IoDev, SetActiveNode) {
  struct cras_iodev iodev;
  struct cras_ionode ionode;

  memset(&iodev, 0, sizeof(iodev));
  memset(&ionode, 0, sizeof(ionode));
  ResetStubData();
  EXPECT_EQ(0, notify_active_node_changed_called);
  cras_iodev_set_active_node(&iodev, &ionode);
  EXPECT_EQ(1, notify_active_node_changed_called);
}

TEST(IoDev, SetMute) {
  struct cras_iodev iodev;
  int rc;

  memset(&iodev, 0, sizeof(iodev));
  iodev.set_mute = dev_set_mute;
  iodev.state = CRAS_IODEV_STATE_CLOSE;

  ResetStubData();
  rc = cras_iodev_set_mute(&iodev);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, set_mute_called);

  iodev.state = CRAS_IODEV_STATE_OPEN;
  rc = cras_iodev_set_mute(&iodev);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(1, set_mute_called);
}

// Test software volume changes for default output.
TEST(IoDev, SoftwareVolume) {
  struct cras_iodev iodev;
  struct cras_ionode ionode;

  memset(&iodev, 0, sizeof(iodev));
  memset(&ionode, 0, sizeof(ionode));
  ResetStubData();

  iodev.nodes = &ionode;
  iodev.active_node = &ionode;
  iodev.active_node->dev = &iodev;

  iodev.active_node->volume = 100;
  iodev.software_volume_needed = 0;

  softvol_scalers[80] = 0.5;
  softvol_scalers[70] = 0.3;

  // Check that system volume changes software volume if needed.
  cras_system_get_volume_return = 80;
  // system_volume - 100 + node_volume = 80 - 100 + 100 = 80
  EXPECT_FLOAT_EQ(0.5, cras_iodev_get_software_volume_scaler(&iodev));

  // Check that node volume changes software volume if needed.
  iodev.active_node->volume = 90;
  // system_volume - 100 + node_volume = 80 - 100 + 90 = 70
  EXPECT_FLOAT_EQ(0.3, cras_iodev_get_software_volume_scaler(&iodev));
}

// Test software gain scaler.
TEST(IoDev, SoftwareGain) {
  struct cras_iodev iodev;
  struct cras_ionode ionode;

  memset(&iodev, 0, sizeof(iodev));
  memset(&ionode, 0, sizeof(ionode));
  ResetStubData();

  iodev.nodes = &ionode;
  iodev.active_node = &ionode;
  iodev.active_node->dev = &iodev;

  ionode.capture_gain = 400;
  ionode.software_volume_needed = 1;
  ionode.max_software_gain = 3000;

  // Check that system volume changes software volume if needed.
  cras_system_get_capture_gain_ret_value = 2000;
  // system_gain + node_gain = 2000 + 400  = 2400
  // 2400 * 0.01 dB is 15.848931
  EXPECT_FLOAT_EQ(15.848931, cras_iodev_get_software_gain_scaler(&iodev));
  EXPECT_FLOAT_EQ(3000, cras_iodev_maximum_software_gain(&iodev));

  // Software gain scaler should be 1.0 if software gain is not needed.
  ionode.software_volume_needed = 0;
  EXPECT_FLOAT_EQ(1.0, cras_iodev_get_software_gain_scaler(&iodev));
  EXPECT_FLOAT_EQ(0, cras_iodev_maximum_software_gain(&iodev));
}

// This get_buffer implementation set returned frames larger than requested
// frames.
static int bad_get_buffer(struct cras_iodev* iodev,
                          struct cras_audio_area** area,
                          unsigned* frames) {
  *frames = *frames + 1;
  return 0;
}

// Check that if get_buffer implementation returns invalid frames,
// cras_iodev_get_output_buffer and cras_iodev_get_input_buffer can return
// error.
TEST(IoDev, GetBufferInvalidFrames) {
  struct cras_iodev iodev;
  struct cras_audio_area** area = NULL;
  unsigned int frames = 512;
  struct cras_audio_format fmt;

  // Format is used in cras_iodev_get_input_buffer;
  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;

  memset(&iodev, 0, sizeof(iodev));

  ResetStubData();

  iodev.format = &fmt;
  iodev.get_buffer = bad_get_buffer;

  EXPECT_EQ(-EINVAL, cras_iodev_get_output_buffer(&iodev, area, &frames));
  EXPECT_EQ(-EINVAL, cras_iodev_get_input_buffer(&iodev, &frames));
}

static int configure_dev(struct cras_iodev* iodev) {
  iodev->buffer_size = iodev_buffer_size;
  return 0;
}

TEST(IoDev, OpenOutputDeviceNoStart) {
  struct cras_iodev iodev;

  memset(&iodev, 0, sizeof(iodev));
  iodev.configure_dev = configure_dev;
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.format = &audio_fmt;
  ResetStubData();

  iodev.state = CRAS_IODEV_STATE_CLOSE;

  iodev_buffer_size = 1024;
  cras_iodev_open(&iodev, 240, &audio_fmt);
  EXPECT_EQ(0, iodev.max_cb_level);
  EXPECT_EQ(240, iodev.min_cb_level);

  // Test that state is no stream run when there is no start ops.
  EXPECT_EQ(CRAS_IODEV_STATE_NO_STREAM_RUN, iodev.state);
}

TEST(IoDev, OpenOutputDeviceWithLowRateFmt) {
  struct cras_iodev iodev;

  memset(&iodev, 0, sizeof(iodev));
  iodev.configure_dev = configure_dev;
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.format = &audio_fmt;
  ResetStubData();

  cras_audio_format low_rate_fmt = audio_fmt;
  low_rate_fmt.frame_rate = 8000;
  iodev.state = CRAS_IODEV_STATE_CLOSE;

  iodev_buffer_size = 1024;
  cras_iodev_open(&iodev, 40, &low_rate_fmt);
  EXPECT_EQ(0, iodev.max_cb_level);

  // Test that iodev min_cb_level should be set to
  // 40 * 48000 / 8000 = 240
  EXPECT_EQ(240, iodev.min_cb_level);
}

int fake_start(const struct cras_iodev* iodev) {
  return 0;
}

TEST(IoDev, OpenOutputDeviceWithStart) {
  struct cras_iodev iodev;

  memset(&iodev, 0, sizeof(iodev));
  iodev.configure_dev = configure_dev;
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.format = &audio_fmt;
  ResetStubData();

  iodev.state = CRAS_IODEV_STATE_CLOSE;
  iodev.start = fake_start;

  iodev_buffer_size = 1024;
  cras_iodev_open(&iodev, 240, &audio_fmt);
  EXPECT_EQ(0, iodev.max_cb_level);
  EXPECT_EQ(240, iodev.min_cb_level);

  // Test that state is no stream run when there is start ops.
  EXPECT_EQ(CRAS_IODEV_STATE_OPEN, iodev.state);
}

TEST(IoDev, OpenInputDeviceNoStart) {
  struct cras_iodev iodev;

  memset(&iodev, 0, sizeof(iodev));
  iodev.configure_dev = configure_dev;
  iodev.direction = CRAS_STREAM_INPUT;
  iodev.format = &audio_fmt;
  ResetStubData();

  iodev.state = CRAS_IODEV_STATE_CLOSE;

  iodev_buffer_size = 1024;
  cras_iodev_open(&iodev, 240, &audio_fmt);
  EXPECT_EQ(0, iodev.max_cb_level);
  EXPECT_EQ(240, iodev.min_cb_level);

  // Test that state is normal run when there is start ops.
  EXPECT_EQ(CRAS_IODEV_STATE_NORMAL_RUN, iodev.state);
}

TEST(IoDev, OpenInputDeviceWithStart) {
  struct cras_iodev iodev;

  memset(&iodev, 0, sizeof(iodev));
  iodev.configure_dev = configure_dev;
  iodev.direction = CRAS_STREAM_INPUT;
  iodev.format = &audio_fmt;
  ResetStubData();

  iodev.state = CRAS_IODEV_STATE_CLOSE;
  iodev.start = fake_start;

  iodev_buffer_size = 1024;
  cras_iodev_open(&iodev, 240, &audio_fmt);
  EXPECT_EQ(0, iodev.max_cb_level);
  EXPECT_EQ(240, iodev.min_cb_level);

  // Test that state is normal run even if there is start ops.
  EXPECT_EQ(CRAS_IODEV_STATE_NORMAL_RUN, iodev.state);
}

TEST(IoDev, OpenInputDeviceWithLowRateFmt) {
  struct cras_iodev iodev;

  memset(&iodev, 0, sizeof(iodev));
  iodev.configure_dev = configure_dev;
  iodev.direction = CRAS_STREAM_INPUT;
  iodev.format = &audio_fmt;
  ResetStubData();

  cras_audio_format low_rate_fmt = audio_fmt;
  low_rate_fmt.frame_rate = 8000;
  iodev.state = CRAS_IODEV_STATE_CLOSE;

  iodev_buffer_size = 1024;
  cras_iodev_open(&iodev, 40, &low_rate_fmt);
  EXPECT_EQ(0, iodev.max_cb_level);

  // Test that iodev min_cb_level should be set to
  // 40 * 48000 / 8000 = 240
  EXPECT_EQ(240, iodev.min_cb_level);
}

static int simple_no_stream(struct cras_iodev* dev, int enable) {
  simple_no_stream_enable = enable;
  simple_no_stream_called++;
  return 0;
}

TEST(IoDev, AddRmStream) {
  struct cras_iodev iodev;
  struct cras_rstream rstream1, rstream2;
  struct dev_stream stream1, stream2;

  memset(&iodev, 0, sizeof(iodev));
  memset(&rstream1, 0, sizeof(rstream1));
  memset(&rstream2, 0, sizeof(rstream2));
  iodev.configure_dev = configure_dev;
  iodev.no_stream = simple_no_stream;
  iodev.format = &audio_fmt;
  iodev.state = CRAS_IODEV_STATE_NORMAL_RUN;
  rstream1.cb_threshold = 800;
  stream1.stream = &rstream1;
  stream1.is_running = 0;
  rstream2.cb_threshold = 400;
  stream2.stream = &rstream2;
  stream2.is_running = 0;
  ResetStubData();

  iodev_buffer_size = 1024;
  cras_iodev_open(&iodev, rstream1.cb_threshold, &audio_fmt);
  EXPECT_EQ(0, iodev.max_cb_level);
  EXPECT_EQ(512, iodev.min_cb_level);

  /* min_cb_level should not exceed half the buffer size. */
  cras_iodev_add_stream(&iodev, &stream1);
  cras_iodev_start_stream(&iodev, &stream1);
  EXPECT_EQ(800, iodev.max_cb_level);
  EXPECT_EQ(512, iodev.min_cb_level);
  EXPECT_EQ(1, buffer_share_add_id_called);

  cras_iodev_add_stream(&iodev, &stream2);
  cras_iodev_start_stream(&iodev, &stream2);
  EXPECT_EQ(800, iodev.max_cb_level);
  EXPECT_EQ(400, iodev.min_cb_level);
  EXPECT_EQ(2, buffer_share_add_id_called);

  cras_iodev_rm_stream(&iodev, &rstream1);
  EXPECT_EQ(400, iodev.max_cb_level);
  EXPECT_EQ(400, iodev.min_cb_level);
  EXPECT_EQ(0, simple_no_stream_called);

  /* When all streams are removed, keep the last min_cb_level for draining. */
  cras_iodev_rm_stream(&iodev, &rstream2);
  EXPECT_EQ(0, iodev.max_cb_level);
  EXPECT_EQ(512, iodev.min_cb_level);
}

TEST(IoDev, RmStreamUpdateFetchTime) {
  struct cras_iodev iodev;
  struct cras_rstream rstream1, rstream2, rstream3;
  struct dev_stream stream1, stream2, stream3;

  memset(&iodev, 0, sizeof(iodev));
  memset(&rstream1, 0, sizeof(rstream1));
  memset(&rstream2, 0, sizeof(rstream2));
  memset(&rstream3, 0, sizeof(rstream2));
  memset(&stream1, 0, sizeof(stream2));
  memset(&stream2, 0, sizeof(stream2));
  memset(&stream3, 0, sizeof(stream2));
  iodev.configure_dev = configure_dev;
  iodev.no_stream = simple_no_stream;
  iodev.format = &audio_fmt;
  iodev.state = CRAS_IODEV_STATE_NORMAL_RUN;
  rstream1.direction = CRAS_STREAM_OUTPUT;
  rstream2.direction = CRAS_STREAM_OUTPUT;
  rstream3.direction = CRAS_STREAM_OUTPUT;
  stream1.stream = &rstream1;
  stream2.stream = &rstream2;
  stream3.stream = &rstream3;
  ResetStubData();

  cras_iodev_open(&iodev, 1024, &audio_fmt);

  cras_iodev_add_stream(&iodev, &stream1);
  cras_iodev_start_stream(&iodev, &stream1);
  cras_iodev_add_stream(&iodev, &stream2);
  cras_iodev_start_stream(&iodev, &stream2);
  cras_iodev_add_stream(&iodev, &stream3);

  rstream1.next_cb_ts.tv_sec = 2;
  rstream1.next_cb_ts.tv_nsec = 0;
  rstream2.next_cb_ts.tv_sec = 1;
  rstream2.next_cb_ts.tv_nsec = 0;
  rstream3.next_cb_ts.tv_sec = 1;
  rstream3.next_cb_ts.tv_nsec = 0;

  /*
   * Because rstream3 has not started yet, the next_cb_ts will be change to the
   * earliest fetch time of remaining streams, which is rstream1.
   */
  cras_iodev_rm_stream(&iodev, &rstream2);

  EXPECT_EQ(rstream3.next_cb_ts.tv_sec, rstream1.next_cb_ts.tv_sec);
  EXPECT_EQ(rstream3.next_cb_ts.tv_nsec, rstream1.next_cb_ts.tv_nsec);
}

TEST(IoDev, StartStreams) {
  struct cras_iodev iodev1, iodev2;
  struct cras_rstream rstream1, rstream2;
  struct dev_stream stream1, stream2;

  memset(&iodev1, 0, sizeof(iodev1));
  memset(&iodev2, 0, sizeof(iodev2));
  memset(&rstream1, 0, sizeof(rstream1));
  memset(&rstream2, 0, sizeof(rstream2));
  memset(&stream1, 0, sizeof(stream1));
  memset(&stream2, 0, sizeof(stream2));
  iodev1.configure_dev = configure_dev;
  iodev1.format = &audio_fmt;
  iodev1.state = CRAS_IODEV_STATE_NORMAL_RUN;
  iodev2.configure_dev = configure_dev;
  iodev2.format = &audio_fmt;
  iodev2.state = CRAS_IODEV_STATE_NORMAL_RUN;
  rstream1.direction = CRAS_STREAM_INPUT;
  rstream2.direction = CRAS_STREAM_OUTPUT;
  stream1.stream = &rstream1;
  stream2.stream = &rstream2;

  /* An input stream starts running immediately. */
  ResetStubData();
  iodev1.direction = CRAS_STREAM_INPUT;
  cras_iodev_open(&iodev1, 1024, &audio_fmt);
  cras_iodev_add_stream(&iodev1, &stream1);
  EXPECT_EQ(1, dev_stream_is_running(&stream1));
  EXPECT_EQ(1, buffer_share_add_id_called);

  /* An output stream starts running after its first fetch. */
  ResetStubData();
  iodev2.direction = CRAS_STREAM_OUTPUT;
  cras_iodev_open(&iodev2, 1024, &audio_fmt);
  cras_iodev_add_stream(&iodev2, &stream2);
  EXPECT_EQ(0, dev_stream_is_running(&stream2));
  EXPECT_EQ(0, buffer_share_add_id_called);
}

TEST(IoDev, TriggerOnlyStreamNoBufferShare) {
  struct cras_iodev iodev;
  struct cras_rstream rstream;
  struct dev_stream stream;

  memset(&iodev, 0, sizeof(iodev));
  memset(&rstream, 0, sizeof(rstream));
  iodev.configure_dev = configure_dev;
  iodev.format = &audio_fmt;
  iodev.state = CRAS_IODEV_STATE_NORMAL_RUN;
  rstream.cb_threshold = 800;
  rstream.flags = TRIGGER_ONLY;
  stream.stream = &rstream;
  ResetStubData();

  cras_iodev_open(&iodev, rstream.cb_threshold, &audio_fmt);
  /* TRIGGER_ONLY streams shall not be added to buffer_share. */
  cras_iodev_add_stream(&iodev, &stream);
  EXPECT_EQ(0, buffer_share_add_id_called);
}

TEST(IoDev, FillZeros) {
  struct cras_iodev iodev;
  struct cras_audio_format fmt;
  unsigned int frames = 50;
  int16_t* zeros;
  int rc;

  ResetStubData();

  memset(&iodev, 0, sizeof(iodev));
  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.get_buffer = get_buffer;
  iodev.put_buffer = put_buffer;

  iodev.direction = CRAS_STREAM_INPUT;
  rc = cras_iodev_fill_odev_zeros(&iodev, frames);
  EXPECT_EQ(-EINVAL, rc);

  iodev.direction = CRAS_STREAM_OUTPUT;
  rc = cras_iodev_fill_odev_zeros(&iodev, frames);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(frames, put_buffer_nframes);
  zeros = (int16_t*)calloc(frames * 2, sizeof(*zeros));
  rc = memcmp(audio_buffer, zeros, frames * 2 * 2);
  free(zeros);
  EXPECT_EQ(0, rc);
}

TEST(IoDev, DefaultNoStreamPlaybackRunning) {
  struct cras_iodev iodev;
  struct cras_audio_format fmt;
  unsigned int hw_level = 50;
  unsigned int min_cb_level = 240;
  unsigned int zeros_to_fill;
  int16_t* zeros;
  int rc;

  memset(&iodev, 0, sizeof(iodev));

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.min_cb_level = min_cb_level;
  iodev.get_buffer = get_buffer;
  iodev.put_buffer = put_buffer;
  iodev.frames_queued = frames_queued;
  iodev.min_buffer_level = 0;
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.buffer_size = BUFFER_SIZE;
  iodev.no_stream = no_stream;

  ResetStubData();

  // Device is running. hw_level is less than target.
  // Need to fill to callback level * 2;
  iodev.state = CRAS_IODEV_STATE_NO_STREAM_RUN;
  fr_queued = hw_level;
  zeros_to_fill = min_cb_level * 2 - hw_level;

  rc = cras_iodev_default_no_stream_playback(&iodev, 1);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_IODEV_STATE_NO_STREAM_RUN, iodev.state);
  EXPECT_EQ(zeros_to_fill, put_buffer_nframes);
  zeros = (int16_t*)calloc(zeros_to_fill * 2, sizeof(*zeros));
  EXPECT_EQ(0, memcmp(audio_buffer, zeros, zeros_to_fill * 2 * 2));
  free(zeros);

  ResetStubData();

  // Device is running. hw_level is not less than target.
  // No need to fill zeros.
  iodev.state = CRAS_IODEV_STATE_NO_STREAM_RUN;
  hw_level = min_cb_level * 2;
  fr_queued = hw_level;
  zeros_to_fill = 0;

  rc = cras_iodev_default_no_stream_playback(&iodev, 1);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_IODEV_STATE_NO_STREAM_RUN, iodev.state);
  EXPECT_EQ(zeros_to_fill, put_buffer_nframes);
}

TEST(IoDev, PrepareOutputBeforeWriteSamples) {
  struct cras_iodev iodev;
  struct cras_audio_format fmt;
  unsigned int min_cb_level = 240;
  int rc;
  struct cras_rstream rstream1;
  struct dev_stream stream1;
  struct cras_iodev_info info;

  memset(&info, 0, sizeof(info));

  ResetStubData();

  rstream1.cb_threshold = min_cb_level;
  stream1.stream = &rstream1;
  stream1.is_running = 1;

  memset(&iodev, 0, sizeof(iodev));

  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.format = &fmt;
  iodev.min_cb_level = min_cb_level;
  iodev.get_buffer = get_buffer;
  iodev.put_buffer = put_buffer;
  iodev.frames_queued = frames_queued;
  iodev.min_buffer_level = 0;
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.buffer_size = BUFFER_SIZE;
  iodev.no_stream = no_stream;
  iodev.configure_dev = configure_dev;
  iodev.start = fake_start;
  iodev.info = info;
  iodev_buffer_size = BUFFER_SIZE;

  // Open device.
  cras_iodev_open(&iodev, rstream1.cb_threshold, &fmt);

  // Add one stream to device.
  cras_iodev_add_stream(&iodev, &stream1);

  // Case 1: Assume device is not started yet.
  iodev.state = CRAS_IODEV_STATE_OPEN;
  // Assume sample is not ready yet.
  dev_stream_playback_frames_ret = 0;

  rc = cras_iodev_prepare_output_before_write_samples(&iodev);

  EXPECT_EQ(0, rc);
  // Device should remain in open state.
  EXPECT_EQ(CRAS_IODEV_STATE_OPEN, iodev.state);
  EXPECT_EQ(0, no_stream_called);

  // Assume now sample is ready.
  dev_stream_playback_frames_ret = 100;

  rc = cras_iodev_prepare_output_before_write_samples(&iodev);

  EXPECT_EQ(0, rc);
  // Device should enter normal run state.
  EXPECT_EQ(CRAS_IODEV_STATE_NORMAL_RUN, iodev.state);
  EXPECT_EQ(0, no_stream_called);
  // Need to fill 1 callback level of zeros;
  EXPECT_EQ(min_cb_level, put_buffer_nframes);

  ResetStubData();

  // Case 2: Assume device is started and is in no stream state.
  iodev.state = CRAS_IODEV_STATE_NO_STREAM_RUN;
  // Sample is not ready yet.
  dev_stream_playback_frames_ret = 0;

  rc = cras_iodev_prepare_output_before_write_samples(&iodev);

  EXPECT_EQ(0, rc);
  // Device should remain in no_stream state.
  EXPECT_EQ(CRAS_IODEV_STATE_NO_STREAM_RUN, iodev.state);
  // Device in no_stream state should call no_stream ops once.
  EXPECT_EQ(1, no_stream_called);
  EXPECT_EQ(1, no_stream_enable);

  // Assume now sample is ready.
  dev_stream_playback_frames_ret = 100;

  rc = cras_iodev_prepare_output_before_write_samples(&iodev);

  EXPECT_EQ(0, rc);
  // Device should enter normal run state.
  EXPECT_EQ(CRAS_IODEV_STATE_NORMAL_RUN, iodev.state);
  // Device should call no_stream ops with enable=0 to leave no stream state.
  EXPECT_EQ(2, no_stream_called);
  EXPECT_EQ(0, no_stream_enable);

  ResetStubData();

  // Case 3: Assume device is started and is in normal run state.
  iodev.state = CRAS_IODEV_STATE_NORMAL_RUN;

  rc = cras_iodev_prepare_output_before_write_samples(&iodev);

  EXPECT_EQ(0, rc);
  // Device should remain in normal run state.
  EXPECT_EQ(CRAS_IODEV_STATE_NORMAL_RUN, iodev.state);
  // Device in no_stream state should call no_stream ops once.
  EXPECT_EQ(0, no_stream_called);

  ResetStubData();

  // Test for device with ramp. Device should start ramping
  // when sample is ready.

  // Assume device has ramp member.
  iodev.ramp = reinterpret_cast<struct cras_ramp*>(0x1);

  // Case 4.1: Assume device with ramp is started and is in no stream state.
  iodev.state = CRAS_IODEV_STATE_NO_STREAM_RUN;
  // Assume sample is ready.
  dev_stream_playback_frames_ret = 100;

  rc = cras_iodev_prepare_output_before_write_samples(&iodev);

  // Device should start ramping up without setting mute callback.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(1, cras_ramp_start_is_called);
  EXPECT_EQ(1, cras_ramp_start_mute_ramp);
  EXPECT_FLOAT_EQ(0.0, cras_ramp_start_from);
  EXPECT_FLOAT_EQ(1.0, cras_ramp_start_to);
  EXPECT_EQ(fmt.frame_rate * RAMP_NEW_STREAM_DURATION_SECS,
            cras_ramp_start_duration_frames);
  EXPECT_EQ(NULL, cras_ramp_start_cb);
  EXPECT_EQ(NULL, cras_ramp_start_cb_data);

  ResetStubData();

  // Case 4.2: Assume device with ramp is started and is in no stream state.
  //           But system is muted.
  iodev.state = CRAS_IODEV_STATE_NO_STREAM_RUN;
  // Assume system is muted.
  cras_system_get_mute_return = 1;
  // Assume sample is ready.
  dev_stream_playback_frames_ret = 100;

  rc = cras_iodev_prepare_output_before_write_samples(&iodev);

  // Device should not start ramping up because system is muted.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_ramp_start_is_called);

  ResetStubData();

  // Case 5.1: Assume device with ramp is in open state.
  iodev.state = CRAS_IODEV_STATE_OPEN;
  // Assume sample is ready.
  dev_stream_playback_frames_ret = 100;

  rc = cras_iodev_prepare_output_before_write_samples(&iodev);

  // Device should start ramping up without setting mute callback.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(1, cras_ramp_start_is_called);
  EXPECT_EQ(1, cras_ramp_start_mute_ramp);
  EXPECT_FLOAT_EQ(0.0, cras_ramp_start_from);
  EXPECT_FLOAT_EQ(1.0, cras_ramp_start_to);
  EXPECT_EQ(fmt.frame_rate * RAMP_NEW_STREAM_DURATION_SECS,
            cras_ramp_start_duration_frames);
  EXPECT_EQ(NULL, cras_ramp_start_cb);
  EXPECT_EQ(NULL, cras_ramp_start_cb_data);

  ResetStubData();

  // Case 5.2: Assume device with ramp is in open state. But system is muted.
  iodev.state = CRAS_IODEV_STATE_OPEN;
  // Assume system is muted.
  cras_system_get_mute_return = 1;
  // Assume sample is ready.
  dev_stream_playback_frames_ret = 100;

  rc = cras_iodev_prepare_output_before_write_samples(&iodev);

  // Device should not start ramping up because system is muted.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_ramp_start_is_called);
}

TEST(IoDev, StartRampUp) {
  struct cras_iodev iodev;
  int rc;
  struct cras_audio_format fmt;
  enum CRAS_IODEV_RAMP_REQUEST req;
  memset(&iodev, 0, sizeof(iodev));

  // Format will be used in cras_iodev_start_ramp to determine ramp duration.
  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;

  // Assume device has ramp member.
  iodev.ramp = reinterpret_cast<struct cras_ramp*>(0x1);

  // Case 1: Device is not opened yet.
  ResetStubData();
  iodev.state = CRAS_IODEV_STATE_CLOSE;
  req = CRAS_IODEV_RAMP_REQUEST_UP_UNMUTE;

  rc = cras_iodev_start_ramp(&iodev, req);

  // Ramp request is ignored.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_ramp_start_is_called);

  // Case 2: Ramp up without mute.
  ResetStubData();
  iodev.state = CRAS_IODEV_STATE_OPEN;
  req = CRAS_IODEV_RAMP_REQUEST_UP_START_PLAYBACK;

  rc = cras_iodev_start_ramp(&iodev, req);

  // Device should start ramping up without setting mute callback.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(1, cras_ramp_start_is_called);
  EXPECT_EQ(1, cras_ramp_start_mute_ramp);
  EXPECT_FLOAT_EQ(0.0, cras_ramp_start_from);
  EXPECT_FLOAT_EQ(1.0, cras_ramp_start_to);
  EXPECT_EQ(fmt.frame_rate * RAMP_NEW_STREAM_DURATION_SECS,
            cras_ramp_start_duration_frames);
  EXPECT_EQ(NULL, cras_ramp_start_cb);
  EXPECT_EQ(NULL, cras_ramp_start_cb_data);

  // Case 3: Ramp up for unmute.
  ResetStubData();
  iodev.state = CRAS_IODEV_STATE_OPEN;
  req = CRAS_IODEV_RAMP_REQUEST_UP_UNMUTE;

  rc = cras_iodev_start_ramp(&iodev, req);

  // Device should start ramping up.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(1, cras_ramp_start_is_called);
  EXPECT_EQ(1, cras_ramp_start_mute_ramp);
  EXPECT_FLOAT_EQ(0.0, cras_ramp_start_from);
  EXPECT_FLOAT_EQ(1.0, cras_ramp_start_to);
  EXPECT_EQ(fmt.frame_rate * RAMP_UNMUTE_DURATION_SECS,
            cras_ramp_start_duration_frames);
  // Callback for unmute is not used.
  EXPECT_EQ(NULL, cras_ramp_start_cb);
  // Device mute state is set after ramping starts.
  EXPECT_EQ(1, cras_device_monitor_set_device_mute_state_called);
  EXPECT_EQ(iodev.info.idx, cras_device_monitor_set_device_mute_state_dev_idx);
}

TEST(IoDev, StartRampDown) {
  struct cras_iodev iodev;
  int rc;
  struct cras_audio_format fmt;
  enum CRAS_IODEV_RAMP_REQUEST req;
  memset(&iodev, 0, sizeof(iodev));

  // Format will be used in cras_iodev_start_ramp to determine ramp duration.
  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;

  // Assume device has ramp member.
  iodev.ramp = reinterpret_cast<struct cras_ramp*>(0x1);

  // Case 1: Device is not opened yet.
  ResetStubData();
  iodev.state = CRAS_IODEV_STATE_CLOSE;
  req = CRAS_IODEV_RAMP_REQUEST_DOWN_MUTE;

  rc = cras_iodev_start_ramp(&iodev, req);

  // Ramp request is ignored.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_ramp_start_is_called);

  // Case 2: Ramp down for mute.
  ResetStubData();
  iodev.state = CRAS_IODEV_STATE_OPEN;
  req = CRAS_IODEV_RAMP_REQUEST_DOWN_MUTE;

  rc = cras_iodev_start_ramp(&iodev, req);

  // Device should start ramping down with mute callback.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(1, cras_ramp_start_is_called);
  EXPECT_EQ(1, cras_ramp_start_mute_ramp);
  EXPECT_FLOAT_EQ(1.0, cras_ramp_start_from);
  EXPECT_FLOAT_EQ(0.0, cras_ramp_start_to);
  EXPECT_EQ(fmt.frame_rate * RAMP_MUTE_DURATION_SECS,
            cras_ramp_start_duration_frames);

  // Device mute state is not set yet. It should wait for ramp to finish.
  EXPECT_EQ(0, cras_device_monitor_set_device_mute_state_called);

  // Assume the callback is set, and it is later called after ramp is done.
  // It should trigger cras_device_monitor_set_device_mute_state.
  cras_ramp_start_cb(cras_ramp_start_cb_data);
  EXPECT_EQ(1, cras_device_monitor_set_device_mute_state_called);
  EXPECT_EQ(iodev.info.idx, cras_device_monitor_set_device_mute_state_dev_idx);
}

TEST(IoDev, StartVolumeRamp) {
  struct cras_ionode ionode;
  struct cras_iodev iodev;
  int rc;
  struct cras_audio_format fmt;
  int expected_frames;
  float ionode_softvol_scalers[101];
  memset(&iodev, 0, sizeof(iodev));

  // Format will be used in cras_iodev_start_ramp to determine ramp duration.
  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  expected_frames = fmt.frame_rate * RAMP_VOLUME_CHANGE_DURATION_SECS;

  // Assume device has ramp member.
  iodev.ramp = reinterpret_cast<struct cras_ramp*>(0x1);

  // Case 1: Device is not opened yet.
  ResetStubData();
  iodev.state = CRAS_IODEV_STATE_CLOSE;
  rc = cras_iodev_start_volume_ramp(&iodev, 30, 94);

  // Ramp request is ignored.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_ramp_start_is_called);

  // Case 2: Volumes are equal.
  ResetStubData();
  iodev.state = CRAS_IODEV_STATE_OPEN;
  rc = cras_iodev_start_volume_ramp(&iodev, 70, 70);

  // Ramp request is ignored.
  EXPECT_EQ(0, rc);
  EXPECT_EQ(0, cras_ramp_start_is_called);

  // Case 3: Ramp up, global scalers
  ResetStubData();
  iodev.state = CRAS_IODEV_STATE_OPEN;
  softvol_scalers[40] = 0.2;
  softvol_scalers[60] = 0.8;

  rc = cras_iodev_start_volume_ramp(&iodev, 40, 60);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(1, cras_ramp_start_is_called);
  EXPECT_EQ(0, cras_ramp_start_mute_ramp);
  EXPECT_FLOAT_EQ(0.25, cras_ramp_start_from);
  EXPECT_FLOAT_EQ(1.0, cras_ramp_start_to);
  EXPECT_EQ(expected_frames, cras_ramp_start_duration_frames);
  EXPECT_EQ(NULL, cras_ramp_start_cb);
  EXPECT_EQ(NULL, cras_ramp_start_cb_data);

  // Case 4: Ramp down, device saclers
  ResetStubData();
  iodev.state = CRAS_IODEV_STATE_OPEN;

  ionode_softvol_scalers[40] = 0.4;
  ionode_softvol_scalers[60] = 0.5;
  ionode.softvol_scalers = ionode_softvol_scalers;
  iodev.active_node = &ionode;

  rc = cras_iodev_start_volume_ramp(&iodev, 60, 40);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(1, cras_ramp_start_is_called);
  EXPECT_EQ(0, cras_ramp_start_mute_ramp);
  EXPECT_FLOAT_EQ(1.25, cras_ramp_start_from);
  EXPECT_FLOAT_EQ(1.0, cras_ramp_start_to);
  EXPECT_EQ(expected_frames, cras_ramp_start_duration_frames);
  EXPECT_EQ(NULL, cras_ramp_start_cb);
  EXPECT_EQ(NULL, cras_ramp_start_cb_data);
}

TEST(IoDev, OutputDeviceShouldWake) {
  struct cras_iodev iodev;
  int rc;

  memset(&iodev, 0, sizeof(iodev));

  ResetStubData();

  // Device is not running. No need to wake for this device.
  iodev.state = CRAS_IODEV_STATE_OPEN;
  rc = cras_iodev_odev_should_wake(&iodev);
  EXPECT_EQ(0, rc);

  // Device is running. Need to wake for this device.
  iodev.state = CRAS_IODEV_STATE_NORMAL_RUN;
  rc = cras_iodev_odev_should_wake(&iodev);
  EXPECT_EQ(1, rc);

  // Device is running. Device has is_free_running ops.
  iodev.is_free_running = is_free_running;
  is_free_running_ret = 1;
  rc = cras_iodev_odev_should_wake(&iodev);
  EXPECT_EQ(0, rc);

  // Device is running. Device has is_free_running ops.
  is_free_running_ret = 0;
  rc = cras_iodev_odev_should_wake(&iodev);
  EXPECT_EQ(1, rc);

  // Ignore input device.
  iodev.direction = CRAS_STREAM_INPUT;
  rc = cras_iodev_odev_should_wake(&iodev);
  EXPECT_EQ(0, rc);
}

TEST(IoDev, FramesToPlayInSleep) {
  struct cras_iodev iodev;
  struct cras_audio_format fmt;
  unsigned int min_cb_level = 512, hw_level;
  unsigned int got_hw_level, got_frames;
  struct timespec hw_tstamp;
  struct cras_rstream rstream;
  struct dev_stream stream;

  memset(&iodev, 0, sizeof(iodev));
  memset(&fmt, 0, sizeof(fmt));
  iodev.frames_queued = frames_queued;
  iodev.min_buffer_level = 0;
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.buffer_size = BUFFER_SIZE;
  iodev.min_cb_level = min_cb_level;
  iodev.state = CRAS_IODEV_STATE_NORMAL_RUN;
  iodev.format = &fmt;
  fmt.frame_rate = 48000;
  rstream.cb_threshold = min_cb_level;
  stream.stream = &rstream;

  ResetStubData();

  cras_iodev_add_stream(&iodev, &stream);
  cras_iodev_start_stream(&iodev, &stream);

  // Device is running. There is at least one stream for this device
  // and there are frames waiting to be played. hw_level is greater
  // than min_cb_level.
  dev_stream_playback_frames_ret = 100;
  hw_level = min_cb_level + 50;
  fr_queued = hw_level;
  got_frames =
      cras_iodev_frames_to_play_in_sleep(&iodev, &got_hw_level, &hw_tstamp);
  EXPECT_EQ(got_hw_level, hw_level);
  EXPECT_EQ(got_frames, 50);
  dev_stream_playback_frames_ret = 0;

  // Device is running. There is at least one stream for this device.
  // hw_level is greater than min_cb_level.
  hw_level = min_cb_level + 50;
  fr_queued = hw_level;
  got_frames =
      cras_iodev_frames_to_play_in_sleep(&iodev, &got_hw_level, &hw_tstamp);
  EXPECT_EQ(got_hw_level, hw_level);
  EXPECT_EQ(got_frames, 514);

  // Device is running. There is at least one stream for this device.
  // hw_level is 2x greater than min_cb_level.
  hw_level = 2 * min_cb_level + 50;
  fr_queued = hw_level;
  got_frames =
      cras_iodev_frames_to_play_in_sleep(&iodev, &got_hw_level, &hw_tstamp);
  EXPECT_EQ(got_hw_level, hw_level);
  EXPECT_EQ(got_frames, 1026);

  // Device is running. There is at least one stream for this device.
  // hw_level is less than min_cb_level.
  hw_level = min_cb_level / 2;
  fr_queued = hw_level;
  got_frames =
      cras_iodev_frames_to_play_in_sleep(&iodev, &got_hw_level, &hw_tstamp);
  EXPECT_EQ(got_hw_level, hw_level);
  EXPECT_EQ(got_frames, 208);

  // Device is running. There is no stream for this device. The audio thread
  // will wake up until hw_level drops to DEV_NO_STREAM_WAKE_UP_LATEST_TIME,
  // which is defined as 5 milliseconds in cras_iodev.c.
  iodev.streams = NULL;
  hw_level = min_cb_level;
  fr_queued = hw_level;
  got_frames =
      cras_iodev_frames_to_play_in_sleep(&iodev, &got_hw_level, &hw_tstamp);
  EXPECT_EQ(got_hw_level, hw_level);
  EXPECT_EQ(got_frames, hw_level - fmt.frame_rate / 1000 * 5);
}

static unsigned int get_num_underruns(const struct cras_iodev* iodev) {
  return get_num_underruns_ret;
}

TEST(IoDev, GetNumUnderruns) {
  struct cras_iodev iodev;
  memset(&iodev, 0, sizeof(iodev));

  EXPECT_EQ(0, cras_iodev_get_num_underruns(&iodev));

  iodev.get_num_underruns = get_num_underruns;
  get_num_underruns_ret = 10;
  EXPECT_EQ(10, cras_iodev_get_num_underruns(&iodev));
}

TEST(IoDev, RequestReset) {
  struct cras_iodev iodev;
  memset(&iodev, 0, sizeof(iodev));

  ResetStubData();

  iodev.configure_dev = configure_dev;
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.format = &audio_fmt;

  iodev.state = CRAS_IODEV_STATE_CLOSE;
  iodev_buffer_size = 1024;

  // Open device.
  cras_iodev_open(&iodev, 240, &audio_fmt);

  // The first reset request works.
  EXPECT_EQ(0, cras_iodev_reset_request(&iodev));
  EXPECT_EQ(1, device_monitor_reset_device_called);

  // The second reset request will do nothing.
  EXPECT_EQ(0, cras_iodev_reset_request(&iodev));
  EXPECT_EQ(1, device_monitor_reset_device_called);

  // Assume device is opened again.
  cras_iodev_open(&iodev, 240, &audio_fmt);

  // The reset request works.
  EXPECT_EQ(0, cras_iodev_reset_request(&iodev));
  EXPECT_EQ(2, device_monitor_reset_device_called);
}

static int output_underrun(struct cras_iodev* iodev) {
  output_underrun_called++;
  return 0;
}

TEST(IoDev, HandleOutputUnderrun) {
  struct cras_iodev iodev;
  struct cras_audio_format fmt;
  unsigned int frames = 240;
  int16_t* zeros;
  int rc;

  ResetStubData();

  memset(&iodev, 0, sizeof(iodev));
  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.format = &fmt;
  iodev.get_buffer = get_buffer;
  iodev.put_buffer = put_buffer;
  iodev.direction = CRAS_STREAM_OUTPUT;
  iodev.min_cb_level = frames;

  // Default case, fill one block of zeros.
  EXPECT_EQ(0, cras_iodev_output_underrun(&iodev));

  EXPECT_EQ(frames, put_buffer_nframes);
  zeros = (int16_t*)calloc(frames * 2, sizeof(*zeros));
  rc = memcmp(audio_buffer, zeros, frames * 2 * 2);
  free(zeros);
  EXPECT_EQ(0, rc);

  // Test iodev has output_underrun ops.
  iodev.output_underrun = output_underrun;
  EXPECT_EQ(0, cras_iodev_output_underrun(&iodev));
  EXPECT_EQ(1, output_underrun_called);
}

static void ext_mod_configure(struct ext_dsp_module* ext,
                              unsigned int buffer_size,
                              unsigned int num_channels,
                              unsigned int rate) {
  ext_mod_configure_called++;
}

TEST(IoDev, SetExtDspMod) {
  struct cras_iodev iodev;
  struct cras_audio_format fmt;
  struct ext_dsp_module ext;

  ResetStubData();

  memset(&iodev, 0, sizeof(iodev));
  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.configure_dev = configure_dev;
  iodev.format = &fmt;
  iodev.format = &fmt;
  iodev.state = CRAS_IODEV_STATE_CLOSE;
  ext.configure = ext_mod_configure;

  iodev.dsp_context = reinterpret_cast<cras_dsp_context*>(0xf0f);
  cras_dsp_get_pipeline_ret = 0x25;

  cras_iodev_set_ext_dsp_module(&iodev, &ext);
  EXPECT_EQ(0, ext_mod_configure_called);

  cras_iodev_open(&iodev, 240, &fmt);
  EXPECT_EQ(1, ext_mod_configure_called);
  EXPECT_EQ(1, cras_dsp_get_pipeline_called);
  EXPECT_EQ(1, cras_dsp_pipeline_set_sink_ext_module_called);

  cras_iodev_set_ext_dsp_module(&iodev, NULL);
  EXPECT_EQ(1, ext_mod_configure_called);
  EXPECT_EQ(2, cras_dsp_get_pipeline_called);
  EXPECT_EQ(2, cras_dsp_pipeline_set_sink_ext_module_called);

  cras_iodev_set_ext_dsp_module(&iodev, &ext);
  EXPECT_EQ(2, ext_mod_configure_called);
  EXPECT_EQ(3, cras_dsp_get_pipeline_called);
  EXPECT_EQ(3, cras_dsp_pipeline_set_sink_ext_module_called);

  /* If pipeline doesn't exist, dummy pipeline should be loaded. */
  cras_dsp_get_pipeline_ret = 0x0;
  cras_iodev_set_ext_dsp_module(&iodev, &ext);
  EXPECT_EQ(3, ext_mod_configure_called);
  EXPECT_EQ(5, cras_dsp_get_pipeline_called);
  EXPECT_EQ(1, cras_dsp_load_dummy_pipeline_called);
  EXPECT_EQ(4, cras_dsp_pipeline_set_sink_ext_module_called);
}

TEST(IoDev, InputDspOffset) {
  struct cras_iodev iodev;
  struct cras_audio_format fmt;
  struct cras_rstream rstream1;
  struct dev_stream stream1;
  struct input_data data;
  unsigned int frames = 240;
  int rc;

  ResetStubData();

  rstream1.cb_threshold = 240;
  rstream1.stream_id = 123;
  stream1.stream = &rstream1;

  memset(&iodev, 0, sizeof(iodev));
  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.configure_dev = configure_dev;
  iodev.format = &fmt;
  iodev.format = &fmt;
  iodev.state = CRAS_IODEV_STATE_CLOSE;
  iodev.get_buffer = get_buffer;
  iodev.put_buffer = put_buffer;
  iodev.direction = CRAS_STREAM_INPUT;
  iodev.buffer_size = 480;

  iodev.dsp_context = reinterpret_cast<cras_dsp_context*>(0xf0f);
  cras_dsp_get_pipeline_ret = 0x25;
  input_data_create_ret = &data;

  cras_iodev_open(&iodev, 240, &fmt);

  cras_iodev_add_stream(&iodev, &stream1);
  cras_iodev_get_input_buffer(&iodev, &frames);

  buffer_share_get_new_write_point_ret = 100;
  rc = cras_iodev_put_input_buffer(&iodev);
  EXPECT_EQ(140, iodev.input_dsp_offset);
  EXPECT_EQ(100, rc);

  frames = 130;
  cras_iodev_get_input_buffer(&iodev, &frames);
  EXPECT_EQ(130, iodev.input_frames_read);

  buffer_share_get_new_write_point_ret = 80;
  rc = cras_iodev_put_input_buffer(&iodev);
  EXPECT_EQ(60, iodev.input_dsp_offset);
  EXPECT_EQ(80, rc);
}

TEST(IoDev, DropDeviceFramesByTime) {
  struct cras_iodev iodev;
  struct cras_audio_format fmt;
  struct input_data data;
  struct timespec ts;
  int rc;

  ResetStubData();

  memset(&iodev, 0, sizeof(iodev));
  fmt.format = SND_PCM_FORMAT_S16_LE;
  fmt.frame_rate = 48000;
  fmt.num_channels = 2;
  iodev.configure_dev = configure_dev;
  iodev.format = &fmt;
  iodev.state = CRAS_IODEV_STATE_CLOSE;
  iodev.get_buffer = get_buffer;
  iodev.put_buffer = put_buffer;
  iodev.frames_queued = frames_queued;
  iodev.direction = CRAS_STREAM_INPUT;
  iodev.buffer_size = 480;
  input_data_create_ret = &data;
  cras_iodev_open(&iodev, 240, &fmt);
  rate_estimator_get_rate_ret = 48000.0;

  /* hw_level: 240, drop: 48(1ms). */
  fr_queued = 240;
  ts.tv_sec = 0;
  ts.tv_nsec = 1000000;
  rc = cras_iodev_drop_frames_by_time(&iodev, ts);
  EXPECT_EQ(48, rc);
  EXPECT_EQ(48, put_buffer_nframes);
  EXPECT_EQ(1, rate_estimator_add_frames_called);
  EXPECT_EQ(-48, rate_estimator_add_frames_num_frames);

  /* hw_level: 360, drop: 240(5ms). */
  fr_queued = 360;
  ts.tv_sec = 0;
  ts.tv_nsec = 5000000;
  rc = cras_iodev_drop_frames_by_time(&iodev, ts);
  EXPECT_EQ(240, rc);
  EXPECT_EQ(240, put_buffer_nframes);
  EXPECT_EQ(2, rate_estimator_add_frames_called);
  EXPECT_EQ(-240, rate_estimator_add_frames_num_frames);

  /* hw_level: 360, drop: 480(10ms). Only drop 360 because of lower hw_level. */
  fr_queued = 360;
  ts.tv_sec = 0;
  ts.tv_nsec = 10000000;
  rc = cras_iodev_drop_frames_by_time(&iodev, ts);
  EXPECT_EQ(360, rc);
  EXPECT_EQ(360, put_buffer_nframes);
  EXPECT_EQ(3, rate_estimator_add_frames_called);
  EXPECT_EQ(-360, rate_estimator_add_frames_num_frames);
}

extern "C" {

//  From libpthread.
int pthread_create(pthread_t* thread,
                   const pthread_attr_t* attr,
                   void* (*start_routine)(void*),
                   void* arg) {
  return 0;
}

int pthread_join(pthread_t thread, void** value_ptr) {
  return 0;
}

// Fromt fmt_conv
void cras_channel_remix_convert(struct cras_fmt_conv* conv,
                                uint8_t* in_buf,
                                size_t frames) {}

size_t cras_fmt_conv_in_frames_to_out(struct cras_fmt_conv* conv,
                                      size_t in_frames) {
  return in_frames;
}

// From buffer_share
struct buffer_share* buffer_share_create(unsigned int buf_sz) {
  return NULL;
}

void buffer_share_destroy(struct buffer_share* mix) {}

int buffer_share_offset_update(struct buffer_share* mix,
                               unsigned int id,
                               unsigned int frames) {
  return 0;
}

unsigned int buffer_share_get_new_write_point(struct buffer_share* mix) {
  return buffer_share_get_new_write_point_ret;
}

int buffer_share_add_id(struct buffer_share* mix, unsigned int id) {
  buffer_share_add_id_called++;
  return 0;
}

int buffer_share_rm_id(struct buffer_share* mix, unsigned int id) {
  return 0;
}

unsigned int buffer_share_id_offset(const struct buffer_share* mix,
                                    unsigned int id) {
  return 0;
}

// From cras_system_state.
void cras_system_state_stream_added(enum CRAS_STREAM_DIRECTION direction) {}

void cras_system_state_stream_removed(enum CRAS_STREAM_DIRECTION direction) {}

// From cras_dsp
struct cras_dsp_context* cras_dsp_context_new(int sample_rate,
                                              const char* purpose) {
  dsp_context_new_sample_rate = sample_rate;
  dsp_context_new_purpose = purpose;
  return cras_dsp_context_new_return;
}

void cras_dsp_context_free(struct cras_dsp_context* ctx) {
  dsp_context_free_called++;
}

void cras_dsp_load_pipeline(struct cras_dsp_context* ctx) {}
void cras_dsp_load_dummy_pipeline(struct cras_dsp_context* ctx,
                                  unsigned int num_channels) {
  cras_dsp_load_dummy_pipeline_called++;
}

void cras_dsp_set_variable_string(struct cras_dsp_context* ctx,
                                  const char* key,
                                  const char* value) {}

void cras_dsp_set_variable_boolean(struct cras_dsp_context* ctx,
                                   const char* key,
                                   char value) {}

struct pipeline* cras_dsp_get_pipeline(struct cras_dsp_context* ctx) {
  cras_dsp_get_pipeline_called++;
  return reinterpret_cast<struct pipeline*>(cras_dsp_get_pipeline_ret);
}

void cras_dsp_put_pipeline(struct cras_dsp_context* ctx) {
  cras_dsp_put_pipeline_called++;
}

float* cras_dsp_pipeline_get_source_buffer(struct pipeline* pipeline,
                                           int index) {
  cras_dsp_pipeline_get_source_buffer_called++;
  return cras_dsp_pipeline_source_buffer[index];
}

float* cras_dsp_pipeline_get_sink_buffer(struct pipeline* pipeline, int index) {
  cras_dsp_pipeline_get_sink_buffer_called++;
  return cras_dsp_pipeline_sink_buffer[index];
}

int cras_dsp_pipeline_get_delay(struct pipeline* pipeline) {
  cras_dsp_pipeline_get_delay_called++;
  return 0;
}

int cras_dsp_pipeline_apply(struct pipeline* pipeline,
                            uint8_t* buf,
                            snd_pcm_format_t format,
                            unsigned int frames) {
  cras_dsp_pipeline_apply_called++;
  cras_dsp_pipeline_apply_sample_count = frames;
  return 0;
}

void cras_dsp_pipeline_add_statistic(struct pipeline* pipeline,
                                     const struct timespec* time_delta,
                                     int samples) {}
void cras_dsp_pipeline_set_sink_ext_module(struct pipeline* pipeline,
                                           struct ext_dsp_module* ext_module) {
  cras_dsp_pipeline_set_sink_ext_module_called++;
}

unsigned int cras_dsp_num_output_channels(const struct cras_dsp_context* ctx) {
  return cras_dsp_num_output_channels_return;
}

unsigned int cras_dsp_num_input_channels(const struct cras_dsp_context* ctx) {
  return cras_dsp_num_input_channels_return;
}

// From audio thread
int audio_thread_post_message(struct audio_thread* thread,
                              struct audio_thread_msg* msg) {
  return 0;
}

void cras_iodev_list_select_node(enum CRAS_STREAM_DIRECTION direction,
                                 cras_node_id_t node_id) {
  select_node_called++;
  select_node_direction = direction;
  select_node_id = node_id;
}

int cras_iodev_list_node_selected(struct cras_ionode* node) {
  return node == node_selected;
}

void cras_iodev_list_disable_dev(struct cras_iodev* dev) {
  cras_iodev_list_disable_dev_called++;
}

void cras_iodev_list_notify_nodes_changed() {
  notify_nodes_changed_called++;
}

void cras_iodev_list_notify_active_node_changed(
    enum CRAS_STREAM_DIRECTION direction) {
  notify_active_node_changed_called++;
}

struct cras_audio_area* cras_audio_area_create(int num_channels) {
  return NULL;
}

void cras_audio_area_destroy(struct cras_audio_area* area) {}

void cras_audio_area_config_channels(struct cras_audio_area* area,
                                     const struct cras_audio_format* fmt) {}

int cras_audio_format_set_channel_layout(struct cras_audio_format* format,
                                         const int8_t layout[CRAS_CH_MAX]) {
  int i;
  cras_audio_format_set_channel_layout_called++;
  for (i = 0; i < CRAS_CH_MAX; i++)
    format->channel_layout[i] = layout[i];
  return 0;
}

float softvol_get_scaler(unsigned int volume_index) {
  return softvol_scalers[volume_index];
}

size_t cras_system_get_volume() {
  return cras_system_get_volume_return;
}

long cras_system_get_capture_gain() {
  return cras_system_get_capture_gain_ret_value;
}

int cras_system_get_mute() {
  return cras_system_get_mute_return;
}

int cras_system_get_capture_mute() {
  return 0;
}

void cras_scale_buffer(snd_pcm_format_t fmt,
                       uint8_t* buffer,
                       unsigned int count,
                       float scaler) {
  cras_scale_buffer_called++;
  cras_scale_buffer_fmt = fmt;
  cras_scale_buffer_scaler = scaler;
}

void cras_scale_buffer_increment(snd_pcm_format_t fmt,
                                 uint8_t* buff,
                                 unsigned int frame,
                                 float scaler,
                                 float increment,
                                 float target,
                                 int channel) {
  cras_scale_buffer_increment_fmt = fmt;
  cras_scale_buffer_increment_buff = buff;
  cras_scale_buffer_increment_frame = frame;
  cras_scale_buffer_increment_scaler = scaler;
  cras_scale_buffer_increment_increment = increment;
  cras_scale_buffer_increment_target = target;
  cras_scale_buffer_increment_channel = channel;
}

size_t cras_mix_mute_buffer(uint8_t* dst, size_t frame_bytes, size_t count) {
  cras_mix_mute_count = count;
  return count;
}

struct rate_estimator* rate_estimator_create(unsigned int rate,
                                             const struct timespec* window_size,
                                             double smooth_factor) {
  return NULL;
}

void rate_estimator_destroy(struct rate_estimator* re) {}

void rate_estimator_add_frames(struct rate_estimator* re, int fr) {
  rate_estimator_add_frames_called++;
  rate_estimator_add_frames_num_frames = fr;
}

int rate_estimator_check(struct rate_estimator* re,
                         int level,
                         struct timespec* now) {
  return 0;
}

void rate_estimator_reset_rate(struct rate_estimator* re, unsigned int rate) {}

double rate_estimator_get_rate(struct rate_estimator* re) {
  return rate_estimator_get_rate_ret;
}

unsigned int dev_stream_cb_threshold(const struct dev_stream* dev_stream) {
  if (dev_stream->stream)
    return dev_stream->stream->cb_threshold;
  return 0;
}

int dev_stream_attached_devs(const struct dev_stream* dev_stream) {
  return 1;
}

void dev_stream_update_frames(const struct dev_stream* dev_stream) {}

int dev_stream_playback_frames(const struct dev_stream* dev_stream) {
  return dev_stream_playback_frames_ret;
}

int cras_device_monitor_reset_device(struct cras_iodev* iodev) {
  device_monitor_reset_device_called++;
  return 0;
}

void cras_ramp_destroy(struct cras_ramp* ramp) {
  return;
}

int cras_ramp_start(struct cras_ramp* ramp,
                    int mute_ramp,
                    float from,
                    float to,
                    int duration_frames,
                    cras_ramp_cb cb,
                    void* cb_data) {
  cras_ramp_start_is_called++;
  cras_ramp_start_mute_ramp = mute_ramp;
  cras_ramp_start_from = from;
  cras_ramp_start_to = to;
  cras_ramp_start_duration_frames = duration_frames;
  cras_ramp_start_cb = cb;
  cras_ramp_start_cb_data = cb_data;
  return 0;
}

int cras_ramp_reset(struct cras_ramp* ramp) {
  cras_ramp_reset_is_called++;
  return 0;
}

struct cras_ramp_action cras_ramp_get_current_action(
    const struct cras_ramp* ramp) {
  return cras_ramp_get_current_action_ret;
}

int cras_ramp_update_ramped_frames(struct cras_ramp* ramp, int num_frames) {
  cras_ramp_update_ramped_frames_num_frames = num_frames;
  return 0;
}

int cras_device_monitor_set_device_mute_state(unsigned int dev_idx) {
  cras_device_monitor_set_device_mute_state_called++;
  cras_device_monitor_set_device_mute_state_dev_idx = dev_idx;
  return 0;
}

static void mod_run(struct ext_dsp_module* ext, unsigned int nframes) {}

static void mod_configure(struct ext_dsp_module* ext,
                          unsigned int buffer_size,
                          unsigned int num_channels,
                          unsigned int rate) {}

struct input_data* input_data_create(void* dev_ptr) {
  if (input_data_create_ret) {
    input_data_create_ret->ext.run = mod_run;
    input_data_create_ret->ext.configure = mod_configure;
  }
  return input_data_create_ret;
}

void input_data_destroy(struct input_data** data) {}
void input_data_set_all_streams_read(struct input_data* data,
                                     unsigned int nframes) {}

int cras_audio_thread_event_severe_underrun() {
  return 0;
}

int cras_audio_thread_event_underrun() {
  return 0;
}

int cras_server_metrics_device_runtime(struct cras_iodev* iodev) {
  return 0;
}

}  // extern "C"
}  //  namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int rc = RUN_ALL_TESTS();

  audio_thread_event_log_deinit(atlog, atlog_name);
  free(atlog_name);
  return rc;
}
