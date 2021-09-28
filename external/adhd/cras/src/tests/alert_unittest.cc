// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "cras_alert.h"

namespace {

void callback1(void* arg, void* data);
void callback2(void* arg, void* data);
void prepare(struct cras_alert* alert);

struct cb_data_struct {
  int data;
};

static int cb1_called = 0;
static cb_data_struct cb1_data;
static int cb2_called = 0;
static int cb2_set_pending = 0;
static int prepare_called = 0;

void ResetStub() {
  cb1_called = 0;
  cb2_called = 0;
  cb2_set_pending = 0;
  prepare_called = 0;
  cb1_data.data = 0;
}

class Alert : public testing::Test {
 protected:
  virtual void SetUp() { cb1_data.data = 0; }

  virtual void TearDown() {}
};

TEST_F(Alert, OneCallback) {
  struct cras_alert* alert = cras_alert_create(NULL, 0);
  cras_alert_add_callback(alert, &callback1, NULL);
  ResetStub();
  cras_alert_pending(alert);
  EXPECT_EQ(0, cb1_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(1, cb1_called);
  cras_alert_destroy(alert);
}

TEST_F(Alert, OneCallbackPost2Call1) {
  struct cras_alert* alert = cras_alert_create(NULL, 0);
  cras_alert_add_callback(alert, &callback1, NULL);
  ResetStub();
  // Alert twice, callback should only be called once.
  cras_alert_pending(alert);
  cras_alert_pending(alert);
  EXPECT_EQ(0, cb1_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(1, cb1_called);
  cras_alert_destroy(alert);
}

TEST_F(Alert, OneCallbackWithData) {
  struct cras_alert* alert = cras_alert_create(NULL, 0);
  struct cb_data_struct data = {1};
  cras_alert_add_callback(alert, &callback1, NULL);
  ResetStub();
  cras_alert_pending_data(alert, (void*)&data, sizeof(struct cb_data_struct));
  EXPECT_EQ(0, cb1_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(1, cb1_called);
  EXPECT_EQ(1, cb1_data.data);
  cras_alert_destroy(alert);
}

TEST_F(Alert, OneCallbackTwoDataCalledOnce) {
  struct cras_alert* alert = cras_alert_create(NULL, 0);
  struct cb_data_struct data = {1};
  struct cb_data_struct data2 = {2};
  cras_alert_add_callback(alert, &callback1, NULL);
  ResetStub();
  // Callback called with last data only.
  cras_alert_pending_data(alert, (void*)&data, sizeof(struct cb_data_struct));
  cras_alert_pending_data(alert, (void*)&data2, sizeof(struct cb_data_struct));
  EXPECT_EQ(0, cb1_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(1, cb1_called);
  EXPECT_EQ(2, cb1_data.data);
  cras_alert_destroy(alert);
}

TEST_F(Alert, OneCallbackTwoDataKeepAll) {
  struct cras_alert* alert =
      cras_alert_create(NULL, CRAS_ALERT_FLAG_KEEP_ALL_DATA);
  struct cb_data_struct data = {1};
  struct cb_data_struct data2 = {2};
  cras_alert_add_callback(alert, &callback1, NULL);
  ResetStub();
  // Callbacks with data should each be called.
  cras_alert_pending_data(alert, (void*)&data, sizeof(cb_data_struct));
  cras_alert_pending_data(alert, (void*)&data2, sizeof(cb_data_struct));
  EXPECT_EQ(0, cb1_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(2, cb1_called);
  EXPECT_EQ(2, cb1_data.data);
  cras_alert_destroy(alert);
}

TEST_F(Alert, TwoCallbacks) {
  struct cras_alert* alert = cras_alert_create(NULL, 0);
  cras_alert_add_callback(alert, &callback1, NULL);
  cras_alert_add_callback(alert, &callback2, NULL);
  ResetStub();
  cras_alert_pending(alert);
  EXPECT_EQ(0, cb1_called);
  EXPECT_EQ(0, cb2_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(1, cb1_called);
  EXPECT_EQ(1, cb2_called);
  cras_alert_destroy(alert);
}

TEST_F(Alert, NoPending) {
  struct cras_alert* alert = cras_alert_create(NULL, 0);
  cras_alert_add_callback(alert, &callback1, NULL);
  ResetStub();
  EXPECT_EQ(0, cb1_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(0, cb1_called);
  cras_alert_destroy(alert);
}

TEST_F(Alert, PendingInCallback) {
  struct cras_alert* alert1 = cras_alert_create(NULL, 0);
  struct cras_alert* alert2 = cras_alert_create(NULL, 0);
  cras_alert_add_callback(alert1, &callback1, NULL);
  cras_alert_add_callback(alert2, &callback2, alert1);
  ResetStub();
  cras_alert_pending(alert2);
  EXPECT_EQ(0, cb1_called);
  EXPECT_EQ(0, cb2_called);
  cb2_set_pending = 1;
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(1, cb1_called);
  EXPECT_EQ(1, cb2_called);
  cras_alert_destroy(alert1);
  cras_alert_destroy(alert2);
}

TEST_F(Alert, Prepare) {
  struct cras_alert* alert = cras_alert_create(prepare, 0);
  cras_alert_add_callback(alert, &callback1, NULL);
  ResetStub();
  cras_alert_pending(alert);
  EXPECT_EQ(0, cb1_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(1, prepare_called);
  EXPECT_EQ(1, cb1_called);
  cras_alert_destroy(alert);
}

TEST_F(Alert, TwoAlerts) {
  struct cras_alert* alert1 = cras_alert_create(prepare, 0);
  struct cras_alert* alert2 = cras_alert_create(prepare, 0);
  cras_alert_add_callback(alert1, &callback1, NULL);
  cras_alert_add_callback(alert2, &callback2, NULL);

  ResetStub();
  cras_alert_pending(alert1);
  EXPECT_EQ(0, cb1_called);
  EXPECT_EQ(0, cb2_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(1, prepare_called);
  EXPECT_EQ(1, cb1_called);
  EXPECT_EQ(0, cb2_called);

  ResetStub();
  cras_alert_pending(alert2);
  EXPECT_EQ(0, cb1_called);
  EXPECT_EQ(0, cb2_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(1, prepare_called);
  EXPECT_EQ(0, cb1_called);
  EXPECT_EQ(1, cb2_called);

  ResetStub();
  cras_alert_pending(alert1);
  cras_alert_pending(alert2);
  EXPECT_EQ(0, cb1_called);
  EXPECT_EQ(0, cb2_called);
  cras_alert_process_all_pending_alerts();
  EXPECT_EQ(2, prepare_called);
  EXPECT_EQ(1, cb1_called);
  EXPECT_EQ(1, cb2_called);

  cras_alert_destroy_all();
}

void callback1(void* arg, void* data) {
  cb1_called++;
  if (data)
    cb1_data.data = ((struct cb_data_struct*)data)->data;
}

void callback2(void* arg, void* data) {
  cb2_called++;
  if (cb2_set_pending) {
    cb2_set_pending = 0;
    cras_alert_pending((struct cras_alert*)arg);
  }
}

void prepare(struct cras_alert* alert) {
  prepare_called++;
  return;
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
