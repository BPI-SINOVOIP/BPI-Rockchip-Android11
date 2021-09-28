/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <gtest/gtest.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>

extern "C" {
#include "cras_bt_log.h"
#include "cras_hfp_slc.h"
#include "cras_telephony.h"
}

static struct hfp_slc_handle* handle;
static struct cras_telephony_handle fake_telephony;
static int cras_bt_device_update_hardware_volume_called;
static int slc_initialized_cb_called;
static int slc_disconnected_cb_called;
static int cras_system_add_select_fd_called;
static void (*slc_cb)(void* data);
static void* slc_cb_data;
static int fake_errno;
static struct cras_bt_device* device =
    reinterpret_cast<struct cras_bt_device*>(2);
static void (*cras_tm_timer_cb)(struct cras_timer* t, void* data);
static void* cras_tm_timer_cb_data;

int slc_initialized_cb(struct hfp_slc_handle* handle);
int slc_disconnected_cb(struct hfp_slc_handle* handle);

void ResetStubData() {
  slc_initialized_cb_called = 0;
  cras_system_add_select_fd_called = 0;
  cras_bt_device_update_hardware_volume_called = 0;
  slc_cb = NULL;
  slc_cb_data = NULL;
}

namespace {

TEST(HfpSlc, CreateSlcHandle) {
  ResetStubData();

  handle = hfp_slc_create(0, 0, AG_ENHANCED_CALL_STATUS, device,
                          slc_initialized_cb, slc_disconnected_cb);
  ASSERT_EQ(1, cras_system_add_select_fd_called);
  ASSERT_EQ(handle, slc_cb_data);

  hfp_slc_destroy(handle);
}

TEST(HfpSlc, InitializeSlc) {
  int err;
  int sock[2];
  char buf[256];
  char* chp;
  ResetStubData();

  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));
  handle = hfp_slc_create(sock[0], 0, AG_ENHANCED_CALL_STATUS, device,
                          slc_initialized_cb, slc_disconnected_cb);

  err = write(sock[1], "AT+CIND=?\r", 10);
  ASSERT_EQ(10, err);
  slc_cb(slc_cb_data);
  err = read(sock[1], buf, 256);

  /* Assert "\r\n+CIND: ... \r\n" response is received */
  chp = strstr(buf, "\r\n");
  ASSERT_NE((void*)NULL, (void*)chp);
  ASSERT_EQ(0, strncmp("\r\n+CIND:", chp, 8));
  chp += 2;
  chp = strstr(chp, "\r\n");
  ASSERT_NE((void*)NULL, (void*)chp);

  /* Assert "\r\nOK\r\n" response is received */
  chp += 2;
  chp = strstr(chp, "\r\n");
  ASSERT_NE((void*)NULL, (void*)chp);
  ASSERT_EQ(0, strncmp("\r\nOK", chp, 4));

  err = write(sock[1], "AT+CMER=3,0,0,1\r", 16);
  ASSERT_EQ(16, err);
  slc_cb(slc_cb_data);

  ASSERT_EQ(1, slc_initialized_cb_called);

  /* Assert "\r\nOK\r\n" response is received */
  err = read(sock[1], buf, 256);

  chp = strstr(buf, "\r\n");
  ASSERT_NE((void*)NULL, (void*)chp);
  ASSERT_EQ(0, strncmp("\r\nOK", chp, 4));

  err = write(sock[1], "AT+VGS=13\r", 10);
  ASSERT_EQ(err, 10);
  slc_cb(slc_cb_data);

  err = read(sock[1], buf, 256);

  chp = strstr(buf, "\r\n");
  ASSERT_NE((void*)NULL, (void*)chp);
  ASSERT_EQ(0, strncmp("\r\nOK", chp, 4));

  ASSERT_EQ(1, cras_bt_device_update_hardware_volume_called);

  hfp_slc_destroy(handle);
}

TEST(HfpSlc, DisconnectSlc) {
  int sock[2];
  ResetStubData();

  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));
  handle = hfp_slc_create(sock[0], 0, AG_ENHANCED_CALL_STATUS, device,
                          slc_initialized_cb, slc_disconnected_cb);
  /* Close socket right away to make read() get negative err code, and
   * fake the errno to ECONNRESET. */
  close(sock[0]);
  close(sock[1]);
  fake_errno = 104;
  slc_cb(slc_cb_data);

  ASSERT_EQ(1, slc_disconnected_cb_called);

  hfp_slc_destroy(handle);
}

TEST(HfpSlc, CodecNegotiation) {
  int codec;
  int err;
  int sock[2];
  char buf[256];
  char* pos;
  ResetStubData();

  btlog = cras_bt_event_log_init();

  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));
  handle = hfp_slc_create(sock[0], 0, AG_CODEC_NEGOTIATION, device,
                          slc_initialized_cb, slc_disconnected_cb);

  codec = hfp_slc_get_selected_codec(handle);
  EXPECT_EQ(HFP_CODEC_ID_CVSD, codec);

  /* Fake that HF supports codec negotiation. */
  err = write(sock[1], "AT+BRSF=128\r", 12);
  ASSERT_EQ(err, 12);
  slc_cb(slc_cb_data);
  err = read(sock[1], buf, 256);

  /* Fake that HF supports mSBC codec. */
  err = write(sock[1], "AT+BAC=1,2\r", 11);
  ASSERT_EQ(err, 11);
  slc_cb(slc_cb_data);
  err = read(sock[1], buf, 256);

  /* Fake event reporting command to indicate SLC established. */
  err = write(sock[1], "AT+CMER=3,0,0,1\r", 16);
  ASSERT_EQ(err, 16);
  slc_cb(slc_cb_data);

  /* Assert that AG side prefers mSBC codec. */
  codec = hfp_slc_get_selected_codec(handle);
  EXPECT_EQ(HFP_CODEC_ID_MSBC, codec);

  /* Assert CRAS initiates codec selection to mSBC. */
  memset(buf, 0, 256);
  err = read(sock[1], buf, 256);
  pos = strstr(buf, "\r\n+BCS:2\r\n");
  ASSERT_NE((void*)NULL, pos);

  err = write(sock[1], "AT+VGS=9\r", 9);
  ASSERT_EQ(err, 9);
  slc_cb(slc_cb_data);

  /* Assert CRAS initiates codec selection to mSBC. */
  memset(buf, 0, 256);
  err = read(sock[1], buf, 256);
  pos = strstr(buf, "\r\n+BCS:2\r\n");
  ASSERT_NE((void*)NULL, pos);

  /* Fake that receiving codec selection from HF. */
  err = write(sock[1], "AT+BCS=2\r", 9);
  ASSERT_EQ(err, 9);
  slc_cb(slc_cb_data);

  memset(buf, 0, 256);
  err = read(sock[1], buf, 256);
  pos = strstr(buf, "\r\n+BCS:2\r\n");
  ASSERT_EQ((void*)NULL, pos);

  hfp_slc_destroy(handle);
  cras_bt_event_log_deinit(btlog);
}

TEST(HfpSlc, CodecNegotiationTimeout) {
  int codec;
  int err;
  int sock[2];
  char buf[256];
  char* pos;
  ResetStubData();

  btlog = cras_bt_event_log_init();

  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));
  handle = hfp_slc_create(sock[0], 0, AG_CODEC_NEGOTIATION, device,
                          slc_initialized_cb, slc_disconnected_cb);

  codec = hfp_slc_get_selected_codec(handle);
  EXPECT_EQ(HFP_CODEC_ID_CVSD, codec);

  /* Fake that HF supports codec negotiation. */
  err = write(sock[1], "AT+BRSF=128\r", 12);
  ASSERT_EQ(err, 12);
  slc_cb(slc_cb_data);
  err = read(sock[1], buf, 256);

  /* Fake that HF supports mSBC codec. */
  err = write(sock[1], "AT+BAC=1,2\r", 11);
  ASSERT_EQ(err, 11);
  slc_cb(slc_cb_data);
  err = read(sock[1], buf, 256);

  /* Fake event reporting command to indicate SLC established. */
  err = write(sock[1], "AT+CMER=3,0,0,1\r", 16);
  ASSERT_EQ(err, 16);
  slc_cb(slc_cb_data);

  ASSERT_NE((void*)NULL, cras_tm_timer_cb);

  /* Assert that AG side prefers mSBC codec. */
  codec = hfp_slc_get_selected_codec(handle);
  EXPECT_EQ(HFP_CODEC_ID_MSBC, codec);

  /* Assert CRAS initiates codec selection to mSBC. */
  memset(buf, 0, 256);
  err = read(sock[1], buf, 256);
  pos = strstr(buf, "\r\n+BCS:2\r\n");
  ASSERT_NE((void*)NULL, pos);

  /* Assume codec negotiation failed. so timeout is reached. */
  cras_tm_timer_cb(NULL, cras_tm_timer_cb_data);

  codec = hfp_slc_get_selected_codec(handle);
  EXPECT_EQ(HFP_CODEC_ID_CVSD, codec);

  /* Expects CRAS fallback and selects to CVSD codec. */
  memset(buf, 0, 256);
  err = read(sock[1], buf, 256);
  pos = strstr(buf, "\r\n+BCS:1\r\n");
  ASSERT_NE((void*)NULL, pos);

  hfp_slc_destroy(handle);
  cras_bt_event_log_deinit(btlog);
}

}  // namespace

int slc_initialized_cb(struct hfp_slc_handle* handle) {
  slc_initialized_cb_called++;
  return 0;
}

int slc_disconnected_cb(struct hfp_slc_handle* handle) {
  slc_disconnected_cb_called++;
  return 0;
}

extern "C" {

struct cras_bt_event_log* btlog;

int cras_system_add_select_fd(int fd,
                              void (*callback)(void* data),
                              void* callback_data) {
  cras_system_add_select_fd_called++;
  slc_cb = callback;
  slc_cb_data = callback_data;
  return 0;
}

void cras_system_rm_select_fd(int fd) {}

void cras_bt_device_update_hardware_volume(struct cras_bt_device* device,
                                           int volume) {
  cras_bt_device_update_hardware_volume_called++;
}

/* To return fake errno */
int* __errno_location() {
  return &fake_errno;
}

struct cras_tm* cras_system_state_get_tm() {
  return NULL;
}

struct cras_timer* cras_tm_create_timer(struct cras_tm* tm,
                                        unsigned int ms,
                                        void (*cb)(struct cras_timer* t,
                                                   void* data),
                                        void* cb_data) {
  cras_tm_timer_cb = cb;
  cras_tm_timer_cb_data = cb_data;
  return reinterpret_cast<struct cras_timer*>(0x404);
}

void cras_tm_cancel_timer(struct cras_tm* tm, struct cras_timer* t) {}
}

// For telephony
struct cras_telephony_handle* cras_telephony_get() {
  return &fake_telephony;
}

void cras_telephony_store_dial_number(int len, const char* num) {}

int cras_telephony_event_answer_call() {
  return 0;
}

int cras_telephony_event_terminate_call() {
  return 0;
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
