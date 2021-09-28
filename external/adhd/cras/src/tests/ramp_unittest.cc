// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "cras_ramp.c"
}

static int callback_called;
static void* callback_arg;

void ResetStubData() {
  callback_called = 0;
  callback_arg = NULL;
}

namespace {

TEST(RampTestSuite, Init) {
  struct cras_ramp* ramp;
  struct cras_ramp_action action;

  ResetStubData();

  ramp = cras_ramp_create();
  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(action.type, CRAS_RAMP_ACTION_NONE);
  EXPECT_FLOAT_EQ(1.0, action.scaler);
  EXPECT_FLOAT_EQ(0.0, action.increment);
  EXPECT_FLOAT_EQ(1.0, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampUpInitialIncrement) {
  float from = 0.0;
  float to = 1.0;
  int duration_frames = 48000;
  float increment = 1.0 / 48000;
  cras_ramp* ramp;
  cras_ramp_action action;

  ResetStubData();

  ramp = cras_ramp_create();
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(0.0, action.scaler);
  EXPECT_FLOAT_EQ(increment, action.increment);
  EXPECT_FLOAT_EQ(to, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampUpUpdateRampedFrames) {
  float from = 0.0;
  float to = 1.0;
  int duration_frames = 48000;
  float increment = 1.0 / 48000;
  int rc;
  int ramped_frames = 512;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;
  float scaler = increment * ramped_frames;

  ResetStubData();

  ramp = cras_ramp_create();
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(rc, 0);
  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(scaler, action.scaler);
  EXPECT_FLOAT_EQ(increment, action.increment);
  EXPECT_FLOAT_EQ(to, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampUpPassedRamp) {
  float from = 0.0;
  float to = 1.0;
  int duration_frames = 48000;
  int rc;
  int ramped_frames = 48000;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;

  ResetStubData();

  ramp = cras_ramp_create();
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_NONE, action.type);
  EXPECT_FLOAT_EQ(1.0, action.scaler);
  EXPECT_FLOAT_EQ(0.0, action.increment);
  EXPECT_FLOAT_EQ(1.0, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampUpWhileHalfWayRampDown) {
  float from;
  float to;
  int duration_frames = 48000;
  int rc;
  int ramped_frames = 24000;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;
  float down_increment = -1.0 / 48000;
  float up_increment;
  float scaler;

  ResetStubData();

  ramp = cras_ramp_create();
  from = 1.0;
  to = 0.0;
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  // Get expected current scaler.
  scaler = 1 + down_increment * ramped_frames;
  // The increment will be calculated by ramping to 1 starting from scaler.
  up_increment = (1 - scaler) / 48000;

  from = 0.0;
  to = 1.0;
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(scaler, action.scaler);
  EXPECT_FLOAT_EQ(up_increment, action.increment);
  EXPECT_FLOAT_EQ(to, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampUpWhileHalfWayRampUp) {
  float from = 0.0;
  float to = 1.0;
  int duration_frames = 48000;
  int rc;
  int ramped_frames = 24000;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;
  float first_increment = 1.0 / 48000;
  float second_increment;
  float scaler;

  ResetStubData();

  ramp = cras_ramp_create();
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  // Get expected current scaler.
  scaler = first_increment * ramped_frames;
  // The increment will be calculated by ramping to 1 starting from scaler.
  second_increment = (1 - scaler) / 48000;

  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(scaler, action.scaler);
  EXPECT_FLOAT_EQ(second_increment, action.increment);
  EXPECT_FLOAT_EQ(to, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampDownInitialIncrement) {
  float from = 1.0;
  float to = 0.0;
  int duration_frames = 48000;
  float increment = -1.0 / 48000;
  cras_ramp* ramp;
  cras_ramp_action action;

  ResetStubData();

  ramp = cras_ramp_create();
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(1.0, action.scaler);
  EXPECT_FLOAT_EQ(increment, action.increment);
  EXPECT_FLOAT_EQ(to, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampDownUpdateRampedFrames) {
  float from = 1.0;
  float to = 0.0;
  int duration_frames = 48000;
  float increment = -1.0 / 48000;
  int rc;
  int ramped_frames = 512;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;
  float scaler = 1 + increment * ramped_frames;

  ResetStubData();

  ramp = cras_ramp_create();
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(rc, 0);
  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(scaler, action.scaler);
  EXPECT_FLOAT_EQ(increment, action.increment);
  EXPECT_FLOAT_EQ(to, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampDownPassedRamp) {
  float from = 1.0;
  float to = 0.0;
  int duration_frames = 48000;
  int rc;
  int ramped_frames = 48000;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;

  ResetStubData();

  ramp = cras_ramp_create();
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_NONE, action.type);
  EXPECT_FLOAT_EQ(1.0, action.scaler);
  EXPECT_FLOAT_EQ(0.0, action.increment);
  EXPECT_FLOAT_EQ(1.0, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampDownWhileHalfWayRampUp) {
  float from;
  float to;
  int duration_frames = 48000;
  int rc;
  int ramped_frames = 24000;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;
  float up_increment = 1.0 / 48000;
  float down_increment;
  float scaler;

  ResetStubData();

  ramp = cras_ramp_create();
  // Ramp up first.
  from = 0.0;
  to = 1.0;
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  // Get expected current scaler.
  scaler = up_increment * ramped_frames;
  // The increment will be calculated by ramping to 0 starting from scaler.
  down_increment = -scaler / duration_frames;

  // Ramp down will start from current scaler.
  from = 1.0;
  to = 0.0;
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(scaler, action.scaler);
  EXPECT_FLOAT_EQ(down_increment, action.increment);
  EXPECT_FLOAT_EQ(to, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampDownWhileHalfWayRampDown) {
  float from = 1.0;
  float to = 0.0;
  int duration_frames = 48000;
  int rc;
  int ramped_frames = 24000;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;
  float down_increment = -1.0 / 48000;
  float second_down_increment;
  float scaler;

  ResetStubData();

  ramp = cras_ramp_create();
  // Ramp down.
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  // Get expected current scaler.
  scaler = 1 + down_increment * ramped_frames;
  // The increment will be calculated by ramping to 0 starting from scaler.
  second_down_increment = -scaler / duration_frames;

  // Ramp down starting from current scaler.
  cras_mute_ramp_start(ramp, from, to, duration_frames, NULL, NULL);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(scaler, action.scaler);
  EXPECT_FLOAT_EQ(second_down_increment, action.increment);
  EXPECT_FLOAT_EQ(to, action.target);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, PartialRamp) {
  float from_one = 0.75;
  float to_one = 0.4;
  float from_two = 0.6;
  float to_two = 0.9;
  int duration_frames = 1200;
  int rc;
  int ramped_frames = 600;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;
  float down_increment = (to_one - from_one) / duration_frames;
  float up_increment;
  float scaler;

  ResetStubData();

  ramp = cras_ramp_create();
  // Ramp down.
  cras_volume_ramp_start(ramp, from_one, to_one, duration_frames, NULL, NULL);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  scaler = from_one + ramped_frames * down_increment;
  action = cras_ramp_get_current_action(ramp);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(scaler, action.scaler);
  EXPECT_FLOAT_EQ(down_increment, action.increment);
  EXPECT_FLOAT_EQ(to_one, action.target);

  // Ramp up starting from current scaler.
  cras_volume_ramp_start(ramp, from_two, to_two, duration_frames, NULL, NULL);

  // we start by multiplying by previous scaler
  scaler = scaler * from_two;
  action = cras_ramp_get_current_action(ramp);
  up_increment = (to_two - scaler) / duration_frames;
  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_PARTIAL, action.type);
  EXPECT_FLOAT_EQ(scaler, action.scaler);
  EXPECT_FLOAT_EQ(up_increment, action.increment);
  EXPECT_FLOAT_EQ(to_two, action.target);

  cras_ramp_destroy(ramp);
}

void ramp_callback(void* arg) {
  callback_called++;
  callback_arg = arg;
}

TEST(RampTestSuite, RampUpPassedRampCallback) {
  float from = 0.0;
  float to = 1.0;
  int duration_frames = 48000;
  int rc;
  int ramped_frames = 48000;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;
  void* cb_data = reinterpret_cast<void*>(0x123);

  ResetStubData();

  ramp = cras_ramp_create();
  cras_mute_ramp_start(ramp, from, to, duration_frames, ramp_callback, cb_data);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_NONE, action.type);
  EXPECT_FLOAT_EQ(1.0, action.scaler);
  EXPECT_FLOAT_EQ(0.0, action.increment);
  EXPECT_FLOAT_EQ(1.0, action.target);
  EXPECT_EQ(1, callback_called);
  EXPECT_EQ(cb_data, callback_arg);

  cras_ramp_destroy(ramp);
}

TEST(RampTestSuite, RampDownPassedRampCallback) {
  float from = 1.0;
  float to = 0.0;
  int duration_frames = 48000;
  int rc;
  int ramped_frames = 48000;
  struct cras_ramp* ramp;
  struct cras_ramp_action action;
  void* cb_data = reinterpret_cast<void*>(0x123);

  ResetStubData();

  ramp = cras_ramp_create();
  cras_mute_ramp_start(ramp, from, to, duration_frames, ramp_callback, cb_data);

  rc = cras_ramp_update_ramped_frames(ramp, ramped_frames);

  action = cras_ramp_get_current_action(ramp);

  EXPECT_EQ(0, rc);
  EXPECT_EQ(CRAS_RAMP_ACTION_NONE, action.type);
  EXPECT_FLOAT_EQ(1.0, action.scaler);
  EXPECT_FLOAT_EQ(0.0, action.increment);
  EXPECT_FLOAT_EQ(1.0, action.target);
  EXPECT_EQ(1, callback_called);
  EXPECT_EQ(cb_data, callback_arg);

  cras_ramp_destroy(ramp);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int rc = RUN_ALL_TESTS();

  return rc;
}
