/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <gtest/gtest.h>

extern "C" {
#include "cras_bt_log.h"
#include "cras_bt_profile.h"
#include "cras_hfp_ag_profile.h"
#include "cras_iodev.h"
}

static int with_sco_pcm;
static struct cras_iodev fake_sco_out, fake_sco_in;
static struct cras_bt_device* fake_device;
static struct cras_bt_profile* internal_bt_profile;

static size_t hfp_alsa_iodev_create_called;
static size_t hfp_alsa_iodev_destroy_called;
static size_t hfp_iodev_create_called;
static size_t hfp_iodev_destroy_called;
static size_t cras_bt_device_notify_profile_dropped_called;
static struct cras_bt_device* cras_bt_device_notify_profile_dropped_dev;
static enum cras_bt_device_profile
    cras_bt_device_notify_profile_dropped_profile;

static void ResetStubData() {
  hfp_alsa_iodev_create_called = 0;
  hfp_alsa_iodev_destroy_called = 0;
  hfp_iodev_create_called = 0;
  hfp_iodev_destroy_called = 0;
  cras_bt_device_notify_profile_dropped_called = 0;
}

namespace {

class HfpAgProfile : public testing::Test {
 protected:
  virtual void SetUp() {
    btlog = cras_bt_event_log_init();
    ResetStubData();
  }

  virtual void TearDown() { cras_bt_event_log_deinit(btlog); }
};

TEST_F(HfpAgProfile, StartWithoutScoPCM) {
  int ret;
  struct cras_bt_profile* bt_profile;

  with_sco_pcm = 0;
  fake_device = (struct cras_bt_device*)0xdeadbeef;
  /* to get the cras_hfp_ag_profile */
  cras_hfp_ag_profile_create(NULL);
  bt_profile = internal_bt_profile;
  bt_profile->new_connection(NULL, bt_profile, fake_device, 0);

  ret = cras_hfp_ag_start(fake_device);

  EXPECT_EQ(0, ret);
  EXPECT_EQ(2, hfp_iodev_create_called);

  /* Start ag twice won't create more iodev. */
  ret = cras_hfp_ag_start(fake_device);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(2, hfp_iodev_create_called);

  bt_profile->release(bt_profile);

  EXPECT_EQ(2, hfp_iodev_destroy_called);
}

TEST_F(HfpAgProfile, StartWithScoPCM) {
  int ret;
  struct cras_bt_profile* bt_profile;

  with_sco_pcm = 1;
  fake_device = (struct cras_bt_device*)0xdeadbeef;
  /* to get the cras_hfp_ag_profile */
  cras_hfp_ag_profile_create(NULL);
  bt_profile = internal_bt_profile;
  bt_profile->new_connection(NULL, bt_profile, fake_device, 0);

  ret = cras_hfp_ag_start(fake_device);

  EXPECT_EQ(0, ret);
  EXPECT_EQ(2, hfp_alsa_iodev_create_called);

  bt_profile->release(bt_profile);

  EXPECT_EQ(2, hfp_alsa_iodev_destroy_called);
}

TEST_F(HfpAgProfile, RemoveConflictAG) {
  struct cras_bt_profile* bt_profile;
  struct cras_bt_device* new_dev =
      reinterpret_cast<struct cras_bt_device*>(0x123);
  ;

  with_sco_pcm = 0;
  fake_device = (struct cras_bt_device*)0xdeadbeef;
  /* to get the cras_hfp_ag_profile */
  cras_hfp_ag_profile_create(NULL);
  bt_profile = internal_bt_profile;
  bt_profile->new_connection(NULL, bt_profile, fake_device, 0);

  bt_profile->new_connection(NULL, bt_profile, new_dev, 0);

  /* Expect removing conflict of new_dev triggers profile drop on
   * fake_device. */
  cras_hfp_ag_remove_conflict(new_dev);
  EXPECT_EQ(1, cras_bt_device_notify_profile_dropped_called);
  EXPECT_EQ(fake_device, cras_bt_device_notify_profile_dropped_dev);
  EXPECT_EQ(CRAS_BT_DEVICE_PROFILE_HFP_HANDSFREE,
            cras_bt_device_notify_profile_dropped_profile);

  bt_profile->release(bt_profile);
}

}  // namespace

extern "C" {

struct cras_bt_event_log* btlog;

struct cras_iodev* cras_iodev_list_get_sco_pcm_iodev(
    enum CRAS_STREAM_DIRECTION direction) {
  if (with_sco_pcm) {
    if (direction == CRAS_STREAM_OUTPUT)
      return &fake_sco_out;
    else
      return &fake_sco_in;
  }

  return NULL;
}

struct cras_iodev* hfp_alsa_iodev_create(enum CRAS_STREAM_DIRECTION dir,
                                         struct cras_bt_device* device,
                                         struct hfp_slc_handle* slc,
                                         enum cras_bt_device_profile profile) {
  hfp_alsa_iodev_create_called++;
  return (struct cras_iodev*)0xdeadbeef;
}

void hfp_alsa_iodev_destroy(struct cras_iodev* iodev) {
  hfp_alsa_iodev_destroy_called++;
}

struct cras_iodev* hfp_iodev_create(enum CRAS_STREAM_DIRECTION dir,
                                    struct cras_bt_device* device,
                                    struct hfp_slc_handle* slc,
                                    enum cras_bt_device_profile profile,
                                    struct hfp_info* info) {
  hfp_iodev_create_called++;
  return (struct cras_iodev*)0xdeadbeef;
}

void hfp_iodev_destroy(struct cras_iodev* iodev) {
  hfp_iodev_destroy_called++;
}

int cras_bt_add_profile(DBusConnection* conn, struct cras_bt_profile* profile) {
  internal_bt_profile = profile;
  return 0;
}

struct hfp_info* hfp_info_create() {
  return NULL;
}

int hfp_info_running(struct hfp_info* info) {
  return 0;
}

int hfp_info_stop(struct hfp_info* info) {
  return 0;
}

void hfp_info_destroy(struct hfp_info* info) {}

void hfp_slc_destroy(struct hfp_slc_handle* slc_handle) {}

int cras_bt_device_has_a2dp(struct cras_bt_device* device) {
  return 0;
}

int cras_bt_device_disconnect(DBusConnection* conn,
                              struct cras_bt_device* device) {
  return 0;
}

const char* cras_bt_device_name(const struct cras_bt_device* device) {
  return NULL;
}

void cras_bt_device_set_append_iodev_cb(struct cras_bt_device* device,
                                        void (*cb)(void* data)) {}

enum cras_bt_device_profile cras_bt_device_profile_from_uuid(const char* uuid) {
  return CRAS_BT_DEVICE_PROFILE_HSP_AUDIOGATEWAY;
}

struct cras_bt_adapter* cras_bt_device_adapter(
    const struct cras_bt_device* device) {
  return reinterpret_cast<struct cras_bt_adapter*>(0x123);
}

int cras_bt_adapter_wbs_supported(struct cras_bt_adapter* adapter) {
  return 1;
}

struct hfp_slc_handle* hfp_slc_create(int fd,
                                      int is_hsp,
                                      int ag_supported_features,
                                      struct cras_bt_device* device,
                                      hfp_slc_init_cb init_cb,
                                      hfp_slc_disconnect_cb disconnect_cb) {
  return NULL;
}

int hfp_slc_get_selected_codec(struct hfp_slc_handle* handle) {
  return HFP_CODEC_ID_CVSD;
}

int hfp_slc_get_hf_codec_negotiation_supported(struct hfp_slc_handle* handle) {
  return 1;
}

struct cras_bt_device* cras_a2dp_connected_device() {
  return NULL;
}

int cras_bt_device_supports_profile(const struct cras_bt_device* device,
                                    enum cras_bt_device_profile profile) {
  return 0;
}

void cras_a2dp_suspend_connected_device(struct cras_bt_device* device) {}

int cras_bt_device_audio_gateway_initialized(struct cras_bt_device* device) {
  return 0;
}

void cras_bt_device_notify_profile_dropped(
    struct cras_bt_device* device,
    enum cras_bt_device_profile profile) {
  cras_bt_device_notify_profile_dropped_called++;
  cras_bt_device_notify_profile_dropped_dev = device;
  cras_bt_device_notify_profile_dropped_profile = profile;
}

bool cras_system_get_bt_wbs_enabled() {
  return true;
}

}  // extern "C"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
