/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <gtest/gtest.h>

extern "C" {
#include "cras_audio_format.h"
#include "cras_hfp_alsa_iodev.h"
#include "cras_hfp_slc.h"
#include "cras_iodev.h"
}

struct hfp_alsa_io {
  struct cras_iodev base;
  struct cras_bt_device* device;
  struct hfp_slc_handle* slc;
  struct cras_iodev* aio;
};

static struct cras_iodev fake_sco_out, fake_sco_in;
static struct cras_bt_device* fake_device;
static struct hfp_slc_handle* fake_slc;

static size_t cras_bt_device_append_iodev_called;
static size_t cras_bt_device_rm_iodev_called;
static size_t cras_iodev_add_node_called;
static size_t cras_iodev_rm_node_called;
static size_t cras_iodev_set_active_node_called;
static size_t cras_iodev_free_format_called;
static size_t cras_iodev_free_resources_called;
static size_t cras_iodev_set_format_called;
static size_t hfp_set_call_status_called;
static size_t hfp_event_speaker_gain_called;

#define _FAKE_CALL1(name)             \
  static size_t fake_##name##_called; \
  static int fake_##name(void* a) {   \
    fake_##name##_called++;           \
    return 0;                         \
  }
#define _FAKE_CALL2(name)                    \
  static size_t fake_##name##_called;        \
  static int fake_##name(void* a, void* b) { \
    fake_##name##_called++;                  \
    return 0;                                \
  }
#define _FAKE_CALL3(name)                             \
  static size_t fake_##name##_called;                 \
  static int fake_##name(void* a, void* b, void* c) { \
    fake_##name##_called++;                           \
    return 0;                                         \
  }

_FAKE_CALL1(open_dev);
_FAKE_CALL1(update_supported_formats);
_FAKE_CALL1(configure_dev);
_FAKE_CALL1(close_dev);
_FAKE_CALL2(frames_queued);
_FAKE_CALL1(delay_frames);
_FAKE_CALL3(get_buffer);
_FAKE_CALL2(put_buffer);
_FAKE_CALL1(flush_buffer);
_FAKE_CALL3(update_active_node);
_FAKE_CALL1(start);
_FAKE_CALL2(no_stream);
_FAKE_CALL1(is_free_running);

static void ResetStubData() {
  cras_bt_device_append_iodev_called = 0;
  cras_bt_device_rm_iodev_called = 0;
  cras_iodev_add_node_called = 0;
  cras_iodev_rm_node_called = 0;
  cras_iodev_set_active_node_called = 0;
  cras_iodev_free_format_called = 0;
  cras_iodev_free_resources_called = 0;
  cras_iodev_set_format_called = 0;
  hfp_set_call_status_called = 0;
  hfp_event_speaker_gain_called = 0;

  fake_sco_out.open_dev = fake_sco_in.open_dev =
      (int (*)(struct cras_iodev*))fake_open_dev;
  fake_open_dev_called = 0;

  fake_sco_out.update_supported_formats = fake_sco_in.update_supported_formats =
      (int (*)(struct cras_iodev*))fake_update_supported_formats;
  fake_update_supported_formats_called = 0;

  fake_sco_out.configure_dev = fake_sco_in.configure_dev =
      (int (*)(struct cras_iodev*))fake_configure_dev;
  fake_configure_dev_called = 0;

  fake_sco_out.close_dev = fake_sco_in.close_dev =
      (int (*)(struct cras_iodev*))fake_close_dev;
  fake_close_dev_called = 0;

  fake_sco_out.frames_queued = fake_sco_in.frames_queued =
      (int (*)(const struct cras_iodev*, struct timespec*))fake_frames_queued;
  fake_frames_queued_called = 0;

  fake_sco_out.delay_frames = fake_sco_in.delay_frames =
      (int (*)(const struct cras_iodev*))fake_delay_frames;
  fake_delay_frames_called = 0;

  fake_sco_out.get_buffer = fake_sco_in.get_buffer = (int (*)(
      struct cras_iodev*, struct cras_audio_area**, unsigned*))fake_get_buffer;
  fake_get_buffer_called = 0;

  fake_sco_out.put_buffer = fake_sco_in.put_buffer =
      (int (*)(struct cras_iodev*, unsigned))fake_put_buffer;
  fake_put_buffer_called = 0;

  fake_sco_out.flush_buffer = fake_sco_in.flush_buffer =
      (int (*)(struct cras_iodev*))fake_flush_buffer;
  fake_flush_buffer_called = 0;

  fake_sco_out.update_active_node = fake_sco_in.update_active_node =
      (void (*)(struct cras_iodev*, unsigned, unsigned))fake_update_active_node;
  fake_update_active_node_called = 0;

  fake_sco_out.start = fake_sco_in.start =
      (int (*)(const struct cras_iodev*))fake_start;
  fake_start_called = 0;

  fake_sco_out.no_stream = fake_sco_in.no_stream =
      (int (*)(struct cras_iodev*, int))fake_no_stream;
  fake_no_stream_called = 0;

  fake_sco_out.is_free_running = fake_sco_in.is_free_running =
      (int (*)(const struct cras_iodev*))fake_is_free_running;
  fake_is_free_running_called = 0;
}

namespace {

class HfpAlsaIodev : public testing::Test {
 protected:
  virtual void SetUp() { ResetStubData(); }

  virtual void TearDown() {}
};

TEST_F(HfpAlsaIodev, CreateHfpAlsaOutputIodev) {
  struct cras_iodev* iodev;
  struct hfp_alsa_io* hfp_alsa_io;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  hfp_alsa_io = (struct hfp_alsa_io*)iodev;

  EXPECT_EQ(CRAS_STREAM_OUTPUT, iodev->direction);
  EXPECT_EQ(1, cras_bt_device_append_iodev_called);
  EXPECT_EQ(1, cras_iodev_add_node_called);
  EXPECT_EQ(1, cras_iodev_set_active_node_called);
  EXPECT_EQ(&fake_sco_out, hfp_alsa_io->aio);

  hfp_alsa_iodev_destroy(iodev);

  EXPECT_EQ(1, cras_bt_device_rm_iodev_called);
  EXPECT_EQ(1, cras_iodev_rm_node_called);
  EXPECT_EQ(1, cras_iodev_free_resources_called);
}

TEST_F(HfpAlsaIodev, CreateHfpAlsaInputIodev) {
  struct cras_iodev* iodev;
  struct hfp_alsa_io* hfp_alsa_io;

  fake_sco_in.direction = CRAS_STREAM_INPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_in, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  hfp_alsa_io = (struct hfp_alsa_io*)iodev;

  EXPECT_EQ(CRAS_STREAM_INPUT, iodev->direction);
  EXPECT_EQ(1, cras_bt_device_append_iodev_called);
  EXPECT_EQ(1, cras_iodev_add_node_called);
  EXPECT_EQ(1, cras_iodev_set_active_node_called);
  EXPECT_EQ(&fake_sco_in, hfp_alsa_io->aio);
  /* Input device does not use software gain. */
  EXPECT_EQ(0, iodev->software_volume_needed);

  hfp_alsa_iodev_destroy(iodev);

  EXPECT_EQ(1, cras_bt_device_rm_iodev_called);
  EXPECT_EQ(1, cras_iodev_rm_node_called);
  EXPECT_EQ(1, cras_iodev_free_resources_called);
}

TEST_F(HfpAlsaIodev, OpenDev) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->open_dev(iodev);

  EXPECT_EQ(1, fake_open_dev_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, UpdateSupportedFormat) {
  struct cras_iodev* iodev;
  size_t supported_rates[] = {8000, 0};
  size_t supported_channel_counts[] = {1, 0};
  snd_pcm_format_t supported_formats[] = {SND_PCM_FORMAT_S16_LE,
                                          (snd_pcm_format_t)0};

  fake_sco_out.supported_rates = supported_rates;
  fake_sco_out.supported_channel_counts = supported_channel_counts;
  fake_sco_out.supported_formats = supported_formats;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->update_supported_formats(iodev);

  EXPECT_EQ(1, fake_update_supported_formats_called);
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_EQ(supported_rates[i], iodev->supported_rates[i]);
    EXPECT_EQ(supported_channel_counts[i], iodev->supported_channel_counts[i]);
    EXPECT_EQ(supported_formats[i], iodev->supported_formats[i]);
  }

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, ConfigureDev) {
  struct cras_iodev* iodev;
  size_t buf_size = 8192;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  fake_sco_out.buffer_size = buf_size;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->configure_dev(iodev);

  EXPECT_EQ(1, fake_configure_dev_called);
  EXPECT_EQ(1, hfp_set_call_status_called);
  EXPECT_EQ(buf_size, iodev->buffer_size);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, CloseDev) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->close_dev(iodev);

  EXPECT_EQ(1, cras_iodev_free_format_called);
  EXPECT_EQ(1, fake_close_dev_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, FramesQueued) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->frames_queued(iodev, (struct timespec*)NULL);

  EXPECT_EQ(1, fake_frames_queued_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, DelayFrames) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->delay_frames(iodev);

  EXPECT_EQ(1, fake_delay_frames_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, GetBuffer) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->get_buffer(iodev, (struct cras_audio_area**)NULL, (unsigned*)NULL);

  EXPECT_EQ(1, fake_get_buffer_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, PutBuffer) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->put_buffer(iodev, 0xdeadbeef);

  EXPECT_EQ(1, fake_put_buffer_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, FlushBuffer) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->flush_buffer(iodev);

  EXPECT_EQ(1, fake_flush_buffer_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, UpdateActiveNode) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->update_active_node(iodev, 0xdeadbeef, 0xdeadbeef);

  EXPECT_EQ(1, fake_update_active_node_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, Start) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->start(iodev);

  EXPECT_EQ(1, fake_start_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, SetVolume) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->set_volume(iodev);

  EXPECT_EQ(1, hfp_event_speaker_gain_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, NoStream) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->min_cb_level = 0xab;
  iodev->max_cb_level = 0xcd;

  iodev->no_stream(iodev, 1);

  EXPECT_EQ(0xab, fake_sco_out.min_cb_level);
  EXPECT_EQ(0xcd, fake_sco_out.max_cb_level);
  EXPECT_EQ(1, fake_no_stream_called);

  hfp_alsa_iodev_destroy(iodev);
}

TEST_F(HfpAlsaIodev, IsFreeRunning) {
  struct cras_iodev* iodev;

  fake_sco_out.direction = CRAS_STREAM_OUTPUT;
  iodev = hfp_alsa_iodev_create(&fake_sco_out, fake_device, fake_slc,
                                CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  iodev->is_free_running(iodev);

  EXPECT_EQ(1, fake_is_free_running_called);

  hfp_alsa_iodev_destroy(iodev);
}

}  // namespace

extern "C" {

int cras_iodev_set_format(struct cras_iodev* iodev,
                          const struct cras_audio_format* fmt) {
  cras_iodev_set_format_called++;
  return 0;
}

void cras_iodev_free_format(struct cras_iodev* iodev) {
  cras_iodev_free_format_called++;
}

void cras_iodev_add_node(struct cras_iodev* iodev, struct cras_ionode* node) {
  cras_iodev_add_node_called++;
  iodev->nodes = node;
}

void cras_iodev_rm_node(struct cras_iodev* iodev, struct cras_ionode* node) {
  cras_iodev_rm_node_called++;
  iodev->nodes = NULL;
}

void cras_iodev_set_active_node(struct cras_iodev* iodev,
                                struct cras_ionode* node) {
  cras_iodev_set_active_node_called++;
  iodev->active_node = node;
}

size_t cras_system_get_volume() {
  return 0;
}

const char* cras_bt_device_name(const struct cras_bt_device* device) {
  return "fake-device-name";
}

const char* cras_bt_device_address(const struct cras_bt_device* device) {
  return "1A:2B:3C:4D:5E:6F";
}

void cras_bt_device_append_iodev(struct cras_bt_device* device,
                                 struct cras_iodev* iodev,
                                 enum cras_bt_device_profile profile) {
  cras_bt_device_append_iodev_called++;
}

void cras_bt_device_rm_iodev(struct cras_bt_device* device,
                             struct cras_iodev* iodev) {
  cras_bt_device_rm_iodev_called++;
}

const char* cras_bt_device_object_path(const struct cras_bt_device* device) {
  return "/fake/object/path";
}

void cras_iodev_free_resources(struct cras_iodev* iodev) {
  cras_iodev_free_resources_called++;
}

int hfp_set_call_status(struct hfp_slc_handle* handle, int call) {
  hfp_set_call_status_called++;
  return 0;
}

int hfp_event_speaker_gain(struct hfp_slc_handle* handle, int gain) {
  hfp_event_speaker_gain_called++;
  return 0;
}

int cras_bt_device_get_sco(struct cras_bt_device* device, int codec) {
  return 0;
}

void cras_bt_device_put_sco(struct cras_bt_device* device) {}

int hfp_slc_get_selected_codec(struct hfp_slc_handle* handle) {
  return HFP_CODEC_ID_CVSD;
}

}  // extern "C"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
