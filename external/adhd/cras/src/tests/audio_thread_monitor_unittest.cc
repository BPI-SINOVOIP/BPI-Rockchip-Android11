// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "cras_audio_thread_monitor.c"
#include "cras_main_message.h"
}

// Function call counters
static int cras_system_state_add_snapshot_called;
static int audio_thread_dump_thread_info_called;

// Stub data
static enum CRAS_MAIN_MESSAGE_TYPE type_set;
struct cras_audio_thread_event_message message;

void ResetStubData() {
  cras_system_state_add_snapshot_called = 0;
  audio_thread_dump_thread_info_called = 0;
  type_set = (enum CRAS_MAIN_MESSAGE_TYPE)999;
  message.event_type = (enum CRAS_AUDIO_THREAD_EVENT_TYPE)999;
}

namespace {

class AudioThreadMonitorTestSuite : public testing::Test {
 protected:
  virtual void SetUp() { ResetStubData(); }

  virtual void TearDown() {}
};

TEST_F(AudioThreadMonitorTestSuite, Init) {
  cras_audio_thread_monitor_init();
  EXPECT_EQ(type_set, CRAS_MAIN_AUDIO_THREAD_EVENT);
}

TEST_F(AudioThreadMonitorTestSuite, Busyloop) {
  cras_audio_thread_event_busyloop();
  EXPECT_EQ(message.event_type, AUDIO_THREAD_EVENT_BUSYLOOP);
}

TEST_F(AudioThreadMonitorTestSuite, Debug) {
  cras_audio_thread_event_debug();
  EXPECT_EQ(message.event_type, AUDIO_THREAD_EVENT_DEBUG);
}

TEST_F(AudioThreadMonitorTestSuite, Underrun) {
  cras_audio_thread_event_underrun();
  EXPECT_EQ(message.event_type, AUDIO_THREAD_EVENT_UNDERRUN);
}

TEST_F(AudioThreadMonitorTestSuite, SevereUnderrun) {
  cras_audio_thread_event_severe_underrun();
  EXPECT_EQ(message.event_type, AUDIO_THREAD_EVENT_SEVERE_UNDERRUN);
}

TEST_F(AudioThreadMonitorTestSuite, DropSamples) {
  cras_audio_thread_event_drop_samples();
  EXPECT_EQ(message.event_type, AUDIO_THREAD_EVENT_DROP_SAMPLES);
}

TEST_F(AudioThreadMonitorTestSuite, TakeSnapshot) {
  take_snapshot(AUDIO_THREAD_EVENT_DEBUG);
  EXPECT_EQ(cras_system_state_add_snapshot_called, 1);
  EXPECT_EQ(audio_thread_dump_thread_info_called, 1);
}

TEST_F(AudioThreadMonitorTestSuite, EventHandlerDoubleCall) {
  struct cras_audio_thread_event_message msg;
  msg.event_type = AUDIO_THREAD_EVENT_DEBUG;
  handle_audio_thread_event_message((struct cras_main_message*)&msg, NULL);
  EXPECT_EQ(cras_system_state_add_snapshot_called, 1);
  EXPECT_EQ(audio_thread_dump_thread_info_called, 1);

  // take_snapshot shouldn't be called since the time interval is short
  handle_audio_thread_event_message((struct cras_main_message*)&msg, NULL);
  EXPECT_EQ(cras_system_state_add_snapshot_called, 1);
  EXPECT_EQ(audio_thread_dump_thread_info_called, 1);
}

TEST_F(AudioThreadMonitorTestSuite, EventHandlerIgnoreInvalidEvent) {
  struct cras_audio_thread_event_message msg;
  msg.event_type = (enum CRAS_AUDIO_THREAD_EVENT_TYPE)999;
  handle_audio_thread_event_message((struct cras_main_message*)&msg, NULL);
  EXPECT_EQ(cras_system_state_add_snapshot_called, 0);
  EXPECT_EQ(audio_thread_dump_thread_info_called, 0);
}

extern "C" {

void cras_system_state_add_snapshot(
    struct cras_audio_thread_snapshot* snapshot) {
  cras_system_state_add_snapshot_called++;
}

struct audio_thread* cras_iodev_list_get_audio_thread() {
  return reinterpret_cast<struct audio_thread*>(0xff);
}

int audio_thread_dump_thread_info(struct audio_thread* thread,
                                  struct audio_debug_info* info) {
  audio_thread_dump_thread_info_called++;
  return 0;
}

int cras_main_message_add_handler(enum CRAS_MAIN_MESSAGE_TYPE type,
                                  cras_message_callback callback,
                                  void* callback_data) {
  type_set = type;
  return 0;
}

int cras_main_message_send(struct cras_main_message* msg) {
  message = *(struct cras_audio_thread_event_message*)msg;
  return 0;
}

}  // extern "C"
}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int rc = RUN_ALL_TESTS();

  return rc;
}
