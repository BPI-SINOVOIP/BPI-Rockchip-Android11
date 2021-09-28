// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

extern "C" {
#include "cras_bt_constants.h"
#include "cras_bt_device.h"
#include "cras_bt_io.h"
#include "cras_bt_log.h"
#include "cras_iodev.h"
#include "cras_main_message.h"

#define FAKE_OBJ_PATH "/obj/path"
}

static struct cras_iodev* cras_bt_io_create_profile_ret;
static struct cras_iodev* cras_bt_io_append_btio_val;
static struct cras_ionode* cras_bt_io_get_profile_ret;
static unsigned int cras_bt_io_create_called;
static unsigned int cras_bt_io_append_called;
static unsigned int cras_bt_io_remove_called;
static unsigned int cras_bt_io_destroy_called;
static enum cras_bt_device_profile cras_bt_io_create_profile_val;
static enum cras_bt_device_profile cras_bt_io_append_profile_val;
static unsigned int cras_bt_io_try_remove_ret;

static cras_main_message* cras_main_message_send_msg;
static cras_message_callback cras_main_message_add_handler_callback;
static void* cras_main_message_add_handler_callback_data;
static int cras_tm_create_timer_called;
static int cras_a2dp_start_called;
static int cras_a2dp_suspend_connected_device_called;
static int cras_hfp_ag_remove_conflict_called;
static int cras_hfp_ag_start_called;
static int cras_hfp_ag_suspend_connected_device_called;
static void (*cras_tm_create_timer_cb)(struct cras_timer* t, void* data);
static void* cras_tm_create_timer_cb_data;
static int dbus_message_new_method_call_called;
static const char* dbus_message_new_method_call_method;
static struct cras_bt_device* cras_a2dp_connected_device_ret;
static struct cras_bt_device* cras_a2dp_suspend_connected_device_dev;

struct MockDBusMessage {
  int type;
  void* value;
  MockDBusMessage* next;
  MockDBusMessage* recurse;
};

void ResetStubData() {
  cras_bt_io_get_profile_ret = NULL;
  cras_bt_io_create_called = 0;
  cras_bt_io_append_called = 0;
  cras_bt_io_remove_called = 0;
  cras_bt_io_destroy_called = 0;
  cras_bt_io_try_remove_ret = 0;
  cras_main_message_send_msg = NULL;
  cras_tm_create_timer_called = 0;
  cras_a2dp_start_called = 0;
  cras_a2dp_suspend_connected_device_called = 0;
  cras_hfp_ag_remove_conflict_called = 0;
  cras_hfp_ag_start_called = 0;
  cras_hfp_ag_suspend_connected_device_called = 0;
  dbus_message_new_method_call_method = NULL;
  dbus_message_new_method_call_called = 0;
  cras_a2dp_connected_device_ret = NULL;
}

static void FreeMockDBusMessage(MockDBusMessage* head) {
  if (head->next != NULL)
    FreeMockDBusMessage(head->next);
  if (head->recurse != NULL)
    FreeMockDBusMessage(head->recurse);
  if (head->type == DBUS_TYPE_STRING)
    free((char*)head->value);
  delete head;
}

static struct MockDBusMessage* NewMockDBusConnectedMessage() {
  MockDBusMessage* msg = new MockDBusMessage{DBUS_TYPE_ARRAY, NULL};
  MockDBusMessage* dict =
      new MockDBusMessage{DBUS_TYPE_STRING, (void*)strdup("Connected")};
  MockDBusMessage* variant = new MockDBusMessage{DBUS_TYPE_BOOLEAN, (void*)1};

  msg->recurse = dict;
  dict->next = new MockDBusMessage{DBUS_TYPE_INVALID, NULL};
  dict->next->recurse = variant;
  return msg;
}

namespace {

class BtDeviceTestSuite : public testing::Test {
 protected:
  virtual void SetUp() {
    ResetStubData();
    bt_iodev1.direction = CRAS_STREAM_OUTPUT;
    bt_iodev1.update_active_node = update_active_node;
    bt_iodev2.direction = CRAS_STREAM_INPUT;
    bt_iodev2.update_active_node = update_active_node;
    d1_.direction = CRAS_STREAM_OUTPUT;
    d1_.update_active_node = update_active_node;
    d2_.direction = CRAS_STREAM_OUTPUT;
    d2_.update_active_node = update_active_node;
    d3_.direction = CRAS_STREAM_INPUT;
    d3_.update_active_node = update_active_node;
    btlog = cras_bt_event_log_init();
  }

  virtual void TearDown() {
    if (cras_main_message_send_msg)
      free(cras_main_message_send_msg);
    cras_bt_event_log_deinit(btlog);
  }

  static void update_active_node(struct cras_iodev* iodev,
                                 unsigned node_idx,
                                 unsigned dev_enabled) {}

  struct cras_iodev bt_iodev1;
  struct cras_iodev bt_iodev2;
  struct cras_iodev d3_;
  struct cras_iodev d2_;
  struct cras_iodev d1_;
};

TEST(BtDeviceSuite, CreateBtDevice) {
  struct cras_bt_device* device;

  device = cras_bt_device_create(NULL, FAKE_OBJ_PATH);
  EXPECT_NE((void*)NULL, device);

  device = cras_bt_device_get(FAKE_OBJ_PATH);
  EXPECT_NE((void*)NULL, device);

  cras_bt_device_remove(device);
  device = cras_bt_device_get(FAKE_OBJ_PATH);
  EXPECT_EQ((void*)NULL, device);
}

TEST_F(BtDeviceTestSuite, AppendRmIodev) {
  struct cras_bt_device* device;
  device = cras_bt_device_create(NULL, FAKE_OBJ_PATH);
  bt_iodev1.nodes = reinterpret_cast<struct cras_ionode*>(0x123);
  cras_bt_io_create_profile_ret = &bt_iodev1;
  cras_bt_device_append_iodev(device, &d1_, CRAS_BT_DEVICE_PROFILE_A2DP_SOURCE);
  EXPECT_EQ(1, cras_bt_io_create_called);
  EXPECT_EQ(0, cras_bt_io_append_called);
  EXPECT_EQ(CRAS_BT_DEVICE_PROFILE_A2DP_SOURCE, cras_bt_io_create_profile_val);
  cras_bt_device_set_active_profile(device, CRAS_BT_DEVICE_PROFILE_A2DP_SOURCE);

  cras_bt_device_append_iodev(device, &d2_,
                              CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  EXPECT_EQ(1, cras_bt_io_create_called);
  EXPECT_EQ(1, cras_bt_io_append_called);
  EXPECT_EQ(CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY,
            cras_bt_io_append_profile_val);
  EXPECT_EQ(&bt_iodev1, cras_bt_io_append_btio_val);

  /* Test HFP disconnected and switch to A2DP. */
  cras_bt_io_get_profile_ret = bt_iodev1.nodes;
  cras_bt_io_try_remove_ret = CRAS_BT_DEVICE_PROFILE_A2DP_SOURCE;
  cras_bt_device_set_active_profile(device,
                                    CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);
  cras_bt_device_rm_iodev(device, &d2_);
  EXPECT_EQ(1, cras_bt_io_remove_called);

  /* Test A2DP disconnection will cause bt_io destroy. */
  cras_bt_io_try_remove_ret = 0;
  cras_bt_device_rm_iodev(device, &d1_);
  EXPECT_EQ(1, cras_bt_io_remove_called);
  EXPECT_EQ(1, cras_bt_io_destroy_called);
  EXPECT_EQ(0, cras_bt_device_get_active_profile(device));
  cras_bt_device_remove(device);
}

TEST_F(BtDeviceTestSuite, SwitchProfile) {
  struct cras_bt_device* device;

  ResetStubData();
  device = cras_bt_device_create(NULL, FAKE_OBJ_PATH);
  cras_bt_io_create_profile_ret = &bt_iodev1;
  cras_bt_device_append_iodev(device, &d1_, CRAS_BT_DEVICE_PROFILE_A2DP_SOURCE);
  cras_bt_io_create_profile_ret = &bt_iodev2;
  cras_bt_device_append_iodev(device, &d3_,
                              CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY);

  cras_bt_device_start_monitor();
  cras_bt_device_switch_profile_enable_dev(device, &bt_iodev1);

  /* Two bt iodevs were all active. */
  cras_main_message_add_handler_callback(
      cras_main_message_send_msg, cras_main_message_add_handler_callback_data);

  /* One bt iodev was active, the other was not. */
  cras_bt_device_switch_profile_enable_dev(device, &bt_iodev2);
  cras_main_message_add_handler_callback(
      cras_main_message_send_msg, cras_main_message_add_handler_callback_data);

  /* Output bt iodev wasn't active, close the active input iodev. */
  cras_bt_device_switch_profile(device, &bt_iodev2);
  cras_main_message_add_handler_callback(
      cras_main_message_send_msg, cras_main_message_add_handler_callback_data);
  cras_bt_device_remove(device);
}

TEST_F(BtDeviceTestSuite, SetDeviceConnectedA2dpOnly) {
  struct cras_bt_device* device;
  struct MockDBusMessage *msg_root, *cur;
  ResetStubData();

  device = cras_bt_device_create(NULL, FAKE_OBJ_PATH);
  EXPECT_NE((void*)NULL, device);

  cras_bt_device_add_supported_profiles(device, A2DP_SINK_UUID);

  cur = msg_root = NewMockDBusConnectedMessage();
  cras_bt_device_update_properties(device, (DBusMessageIter*)&cur, NULL);
  EXPECT_EQ(1, cras_tm_create_timer_called);
  EXPECT_NE((void*)NULL, cras_tm_create_timer_cb);

  /* Schedule another timer, if A2DP not yet configured. */
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(2, cras_tm_create_timer_called);
  EXPECT_EQ(1, dbus_message_new_method_call_called);
  EXPECT_STREQ("ConnectProfile", dbus_message_new_method_call_method);

  cras_bt_device_a2dp_configured(device);
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(2, cras_tm_create_timer_called);
  EXPECT_EQ(1, cras_hfp_ag_remove_conflict_called);
  EXPECT_EQ(1, cras_a2dp_start_called);

  cras_bt_device_remove(device);
  FreeMockDBusMessage(msg_root);
}

TEST_F(BtDeviceTestSuite, SetDeviceConnectedHfpHspOnly) {
  struct cras_bt_device* device;
  struct MockDBusMessage *msg_root, *cur;

  ResetStubData();

  device = cras_bt_device_create(NULL, FAKE_OBJ_PATH);
  EXPECT_NE((void*)NULL, device);

  cras_bt_device_add_supported_profiles(device, HSP_HS_UUID);
  cras_bt_device_add_supported_profiles(device, HFP_HF_UUID);

  cur = msg_root = NewMockDBusConnectedMessage();
  cras_bt_device_update_properties(device, (DBusMessageIter*)&cur, NULL);
  EXPECT_EQ(1, cras_tm_create_timer_called);
  EXPECT_NE((void*)NULL, cras_tm_create_timer_cb);

  /* Schedule another timer, if HFP AG not yet intialized. */
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(2, cras_tm_create_timer_called);
  EXPECT_EQ(1, dbus_message_new_method_call_called);
  EXPECT_STREQ("ConnectProfile", dbus_message_new_method_call_method);

  cras_bt_device_audio_gateway_initialized(device);

  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(2, cras_tm_create_timer_called);
  EXPECT_EQ(1, cras_hfp_ag_remove_conflict_called);
  EXPECT_EQ(1, cras_hfp_ag_start_called);

  cras_bt_device_remove(device);
  FreeMockDBusMessage(msg_root);
}

TEST_F(BtDeviceTestSuite, SetDeviceConnectedA2dpHfpHsp) {
  struct cras_bt_device* device;
  struct MockDBusMessage *msg_root, *cur;

  ResetStubData();

  device = cras_bt_device_create(NULL, FAKE_OBJ_PATH);
  EXPECT_NE((void*)NULL, device);

  cras_bt_device_add_supported_profiles(device, A2DP_SINK_UUID);
  cras_bt_device_add_supported_profiles(device, HSP_HS_UUID);
  cras_bt_device_add_supported_profiles(device, HFP_HF_UUID);

  cur = msg_root = NewMockDBusConnectedMessage();
  cras_bt_device_update_properties(device, (DBusMessageIter*)&cur, NULL);
  EXPECT_EQ(1, cras_tm_create_timer_called);
  EXPECT_NE((void*)NULL, cras_tm_create_timer_cb);

  /* Schedule another timer, if not HFP nor A2DP is ready. */
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(2, cras_tm_create_timer_called);

  cras_bt_device_audio_gateway_initialized(device);

  /* Schedule another timer, because A2DP is not ready. */
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(3, cras_tm_create_timer_called);
  EXPECT_EQ(0, cras_hfp_ag_start_called);
  EXPECT_EQ(1, dbus_message_new_method_call_called);
  EXPECT_STREQ("ConnectProfile", dbus_message_new_method_call_method);

  cras_bt_device_a2dp_configured(device);

  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(3, cras_tm_create_timer_called);
  EXPECT_EQ(1, cras_hfp_ag_remove_conflict_called);
  EXPECT_EQ(1, cras_a2dp_start_called);
  EXPECT_EQ(1, cras_hfp_ag_start_called);

  cras_bt_device_remove(device);
  FreeMockDBusMessage(msg_root);
}

TEST_F(BtDeviceTestSuite, DevConnectedConflictCheck) {
  struct cras_bt_device* device;
  struct MockDBusMessage *msg_root, *cur;

  ResetStubData();

  device = cras_bt_device_create(NULL, FAKE_OBJ_PATH);
  EXPECT_NE((void*)NULL, device);

  cras_bt_device_add_supported_profiles(device, A2DP_SINK_UUID);
  cras_bt_device_add_supported_profiles(device, HSP_HS_UUID);
  cras_bt_device_add_supported_profiles(device, HFP_HF_UUID);

  cur = msg_root = NewMockDBusConnectedMessage();
  cras_bt_device_update_properties(device, (DBusMessageIter*)&cur, NULL);
  cras_bt_device_audio_gateway_initialized(device);
  cras_bt_device_a2dp_configured(device);
  EXPECT_EQ(1, cras_tm_create_timer_called);

  /* Fake that a different device already connected with A2DP */
  cras_a2dp_connected_device_ret =
      reinterpret_cast<struct cras_bt_device*>(0x99);
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(1, cras_tm_create_timer_called);

  /* Expect check conflict in HFP AG and A2DP. */
  EXPECT_EQ(1, cras_hfp_ag_remove_conflict_called);
  EXPECT_EQ(1, cras_a2dp_suspend_connected_device_called);
  EXPECT_EQ(cras_a2dp_suspend_connected_device_dev,
            cras_a2dp_connected_device_ret);

  EXPECT_EQ(1, cras_a2dp_start_called);
  EXPECT_EQ(1, cras_hfp_ag_start_called);

  cras_bt_device_remove(device);
  FreeMockDBusMessage(msg_root);
}

TEST_F(BtDeviceTestSuite, A2dpDropped) {
  struct cras_bt_device* device;
  struct MockDBusMessage *msg_root, *cur;

  ResetStubData();

  device = cras_bt_device_create(NULL, FAKE_OBJ_PATH);
  EXPECT_NE((void*)NULL, device);

  cras_bt_device_add_supported_profiles(device, A2DP_SINK_UUID);
  cras_bt_device_add_supported_profiles(device, HSP_HS_UUID);
  cras_bt_device_add_supported_profiles(device, HFP_HF_UUID);

  cur = msg_root = NewMockDBusConnectedMessage();
  cras_bt_device_update_properties(device, (DBusMessageIter*)&cur, NULL);
  EXPECT_EQ(1, cras_tm_create_timer_called);
  EXPECT_NE((void*)NULL, cras_tm_create_timer_cb);

  /* Schedule another timer, if HFP AG not yet intialized. */
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(2, cras_tm_create_timer_called);
  EXPECT_EQ(1, dbus_message_new_method_call_called);
  EXPECT_STREQ("ConnectProfile", dbus_message_new_method_call_method);

  cras_bt_device_a2dp_configured(device);

  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(3, cras_tm_create_timer_called);

  cras_bt_device_notify_profile_dropped(device,
                                        CRAS_BT_DEVICE_PROFILE_A2DP_SINK);
  EXPECT_EQ(4, cras_tm_create_timer_called);

  /* Expect suspend timer is scheduled. */
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(1, cras_a2dp_suspend_connected_device_called);
  EXPECT_EQ(1, cras_hfp_ag_suspend_connected_device_called);
  EXPECT_EQ(2, dbus_message_new_method_call_called);
  EXPECT_STREQ("Disconnect", dbus_message_new_method_call_method);

  cras_bt_device_remove(device);
  FreeMockDBusMessage(msg_root);
}

TEST_F(BtDeviceTestSuite, ConnectionWatchTimeout) {
  struct cras_bt_device* device;
  struct MockDBusMessage *msg_root, *cur;

  ResetStubData();

  device = cras_bt_device_create(NULL, FAKE_OBJ_PATH);
  EXPECT_NE((void*)NULL, device);

  cras_bt_device_add_supported_profiles(device, A2DP_SINK_UUID);
  cras_bt_device_add_supported_profiles(device, HSP_HS_UUID);
  cras_bt_device_add_supported_profiles(device, HFP_HF_UUID);

  cur = msg_root = NewMockDBusConnectedMessage();
  cras_bt_device_update_properties(device, (DBusMessageIter*)&cur, NULL);
  EXPECT_EQ(1, cras_tm_create_timer_called);
  EXPECT_NE((void*)NULL, cras_tm_create_timer_cb);

  /* Schedule another timer, if HFP AG not yet intialized. */
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(2, cras_tm_create_timer_called);
  EXPECT_EQ(1, dbus_message_new_method_call_called);
  EXPECT_STREQ("ConnectProfile", dbus_message_new_method_call_method);

  cras_bt_device_a2dp_configured(device);

  for (int i = 0; i < 29; i++) {
    cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
    EXPECT_EQ(i + 3, cras_tm_create_timer_called);
    EXPECT_EQ(0, cras_a2dp_start_called);
    EXPECT_EQ(0, cras_hfp_ag_start_called);
    EXPECT_EQ(0, cras_hfp_ag_remove_conflict_called);
  }

  dbus_message_new_method_call_called = 0;

  /* Expect suspend timer is scheduled. */
  cras_tm_create_timer_cb(NULL, cras_tm_create_timer_cb_data);
  EXPECT_EQ(1, cras_a2dp_suspend_connected_device_called);
  EXPECT_EQ(1, cras_hfp_ag_suspend_connected_device_called);
  EXPECT_EQ(1, dbus_message_new_method_call_called);
  EXPECT_STREQ("Disconnect", dbus_message_new_method_call_method);

  cras_bt_device_remove(device);
  FreeMockDBusMessage(msg_root);
}

/* Stubs */
extern "C" {

struct cras_bt_event_log* btlog;

/* From bt_io */
struct cras_iodev* cras_bt_io_create(struct cras_bt_device* device,
                                     struct cras_iodev* dev,
                                     enum cras_bt_device_profile profile) {
  cras_bt_io_create_called++;
  cras_bt_io_create_profile_val = profile;
  return cras_bt_io_create_profile_ret;
}
void cras_bt_io_destroy(struct cras_iodev* bt_iodev) {
  cras_bt_io_destroy_called++;
}
struct cras_ionode* cras_bt_io_get_profile(
    struct cras_iodev* bt_iodev,
    enum cras_bt_device_profile profile) {
  return cras_bt_io_get_profile_ret;
}
int cras_bt_io_append(struct cras_iodev* bt_iodev,
                      struct cras_iodev* dev,
                      enum cras_bt_device_profile profile) {
  cras_bt_io_append_called++;
  cras_bt_io_append_profile_val = profile;
  cras_bt_io_append_btio_val = bt_iodev;
  return 0;
}
int cras_bt_io_on_profile(struct cras_iodev* bt_iodev,
                          enum cras_bt_device_profile profile) {
  return 0;
}
unsigned int cras_bt_io_try_remove(struct cras_iodev* bt_iodev,
                                   struct cras_iodev* dev) {
  return cras_bt_io_try_remove_ret;
}
int cras_bt_io_remove(struct cras_iodev* bt_iodev, struct cras_iodev* dev) {
  cras_bt_io_remove_called++;
  return 0;
}

/* From bt_adapter */
struct cras_bt_adapter* cras_bt_adapter_get(const char* object_path) {
  return NULL;
}
const char* cras_bt_adapter_address(const struct cras_bt_adapter* adapter) {
  return NULL;
}

int cras_bt_adapter_on_usb(struct cras_bt_adapter* adapter) {
  return 1;
}

/* From bt_profile */
void cras_bt_profile_on_device_disconnected(struct cras_bt_device* device) {}

/* From hfp_ag_profile */
struct hfp_slc_handle* cras_hfp_ag_get_slc(struct cras_bt_device* device) {
  return NULL;
}

void cras_hfp_ag_suspend_connected_device(struct cras_bt_device* device) {
  cras_hfp_ag_suspend_connected_device_called++;
}

void cras_a2dp_suspend_connected_device(struct cras_bt_device* device) {
  cras_a2dp_suspend_connected_device_called++;
  cras_a2dp_suspend_connected_device_dev = device;
}

void cras_a2dp_start(struct cras_bt_device* device) {
  cras_a2dp_start_called++;
}

struct cras_bt_device* cras_a2dp_connected_device() {
  return cras_a2dp_connected_device_ret;
}

int cras_hfp_ag_remove_conflict(struct cras_bt_device* device) {
  cras_hfp_ag_remove_conflict_called++;
  return 0;
}

int cras_hfp_ag_start(struct cras_bt_device* device) {
  cras_hfp_ag_start_called++;
  return 0;
}

void cras_hfp_ag_suspend() {}

/* From hfp_slc */
int hfp_event_speaker_gain(struct hfp_slc_handle* handle, int gain) {
  return 0;
}

/* From iodev_list */

int cras_iodev_open(struct cras_iodev* dev,
                    unsigned int cb_level,
                    const struct cras_audio_format* fmt) {
  return 0;
}

int cras_iodev_close(struct cras_iodev* dev) {
  return 0;
}

int cras_iodev_list_dev_is_enabled(const struct cras_iodev* dev) {
  return 0;
}

void cras_iodev_list_suspend_dev(struct cras_iodev* dev) {}

void cras_iodev_list_resume_dev(struct cras_iodev* dev) {}

void cras_iodev_list_notify_node_volume(struct cras_ionode* node) {}

int cras_main_message_send(struct cras_main_message* msg) {
  // cras_main_message is a local variable from caller, we should allocate
  // memory from heap and copy its data
  if (cras_main_message_send_msg)
    free(cras_main_message_send_msg);
  cras_main_message_send_msg =
      (struct cras_main_message*)calloc(1, msg->length);
  memcpy((void*)cras_main_message_send_msg, (void*)msg, msg->length);
  return 0;
}

int cras_main_message_add_handler(enum CRAS_MAIN_MESSAGE_TYPE type,
                                  cras_message_callback callback,
                                  void* callback_data) {
  cras_main_message_add_handler_callback = callback;
  cras_main_message_add_handler_callback_data = callback_data;
  return 0;
}

/* From cras_system_state */
struct cras_tm* cras_system_state_get_tm() {
  return NULL;
}

/* From cras_tm */
struct cras_timer* cras_tm_create_timer(struct cras_tm* tm,
                                        unsigned int ms,
                                        void (*cb)(struct cras_timer* t,
                                                   void* data),
                                        void* cb_data) {
  cras_tm_create_timer_called++;
  cras_tm_create_timer_cb = cb;
  cras_tm_create_timer_cb_data = cb_data;
  return NULL;
}

void cras_tm_cancel_timer(struct cras_tm* tm, struct cras_timer* t) {}

DBusMessage* dbus_message_new_method_call(const char* destination,
                                          const char* path,
                                          const char* iface,
                                          const char* method) {
  dbus_message_new_method_call_called++;
  dbus_message_new_method_call_method = method;
  return reinterpret_cast<DBusMessage*>(0x456);
}

void dbus_message_unref(DBusMessage* message) {}

dbus_bool_t dbus_message_append_args(DBusMessage* message,
                                     int first_arg_type,
                                     ...) {
  return true;
}

dbus_bool_t dbus_connection_send_with_reply(DBusConnection* connection,
                                            DBusMessage* message,
                                            DBusPendingCall** pending_return,
                                            int timeout_milliseconds) {
  return true;
}

dbus_bool_t dbus_pending_call_set_notify(DBusPendingCall* pending,
                                         DBusPendingCallNotifyFunction function,
                                         void* user_data,
                                         DBusFreeFunction free_user_data) {
  return true;
}

void dbus_message_iter_recurse(DBusMessageIter* iter, DBusMessageIter* sub) {
  MockDBusMessage* msg = *(MockDBusMessage**)iter;
  MockDBusMessage** cur = (MockDBusMessage**)sub;
  *cur = msg->recurse;
}

dbus_bool_t dbus_message_iter_next(DBusMessageIter* iter) {
  MockDBusMessage** cur = (MockDBusMessage**)iter;
  MockDBusMessage* msg = *cur;
  *cur = msg->next;
  return true;
}

int dbus_message_iter_get_arg_type(DBusMessageIter* iter) {
  MockDBusMessage* msg;

  if (iter == NULL)
    return DBUS_TYPE_INVALID;

  msg = *(MockDBusMessage**)iter;
  if (msg == NULL)
    return DBUS_TYPE_INVALID;

  return msg->type;
}

void dbus_message_iter_get_basic(DBusMessageIter* iter, void* value) {
  MockDBusMessage* msg = *(MockDBusMessage**)iter;
  switch (msg->type) {
    case DBUS_TYPE_BOOLEAN:
      memcpy(value, &msg->value, sizeof(int));
      break;
    case DBUS_TYPE_STRING:
      memcpy(value, &msg->value, sizeof(char*));
      break;
  }
}

}  // extern "C"
}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
