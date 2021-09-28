/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <gtest/gtest.h>
#include <stdint.h>
#include <time.h>

extern "C" {
#include "cras_hfp_info.c"
#include "sbc_codec_stub.h"
}
static struct hfp_info* info;
static struct cras_iodev dev;
static cras_audio_format format;

static int cras_msbc_plc_create_called;
static int cras_msbc_plc_handle_good_frames_called;
static int cras_msbc_plc_handle_bad_frames_called;

static thread_callback thread_cb;
static void* cb_data;
static timespec ts;

void ResetStubData() {
  sbc_codec_stub_reset();
  cras_msbc_plc_create_called = 0;

  format.format = SND_PCM_FORMAT_S16_LE;
  format.num_channels = 1;
  format.frame_rate = 8000;
  dev.format = &format;
}

namespace {

TEST(HfpInfo, AddRmDev) {
  ResetStubData();

  info = hfp_info_create(HFP_CODEC_ID_CVSD);
  ASSERT_NE(info, (void*)NULL);
  dev.direction = CRAS_STREAM_OUTPUT;

  /* Test add dev */
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));
  ASSERT_TRUE(hfp_info_has_iodev(info));

  /* Test remove dev */
  ASSERT_EQ(0, hfp_info_rm_iodev(info, dev.direction));
  ASSERT_FALSE(hfp_info_has_iodev(info));

  hfp_info_destroy(info);
}

TEST(HfpInfo, AddRmDevInvalid) {
  ResetStubData();

  info = hfp_info_create(HFP_CODEC_ID_CVSD);
  ASSERT_NE(info, (void*)NULL);

  dev.direction = CRAS_STREAM_OUTPUT;

  /* Remove an iodev which doesn't exist */
  ASSERT_NE(0, hfp_info_rm_iodev(info, dev.direction));

  /* Adding an iodev twice returns error code */
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));
  ASSERT_NE(0, hfp_info_add_iodev(info, dev.direction, dev.format));

  hfp_info_destroy(info);
}

TEST(HfpInfo, AcquirePlaybackBuffer) {
  unsigned buffer_frames, buffer_frames2, queued;
  uint8_t* samples;

  ResetStubData();

  info = hfp_info_create(HFP_CODEC_ID_CVSD);
  ASSERT_NE(info, (void*)NULL);

  hfp_info_start(1, 48, info);
  dev.direction = CRAS_STREAM_OUTPUT;
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));

  buffer_frames = 500;
  hfp_buf_acquire(info, dev.direction, &samples, &buffer_frames);
  ASSERT_EQ(500, buffer_frames);

  hfp_buf_release(info, dev.direction, 500);
  ASSERT_EQ(500, hfp_buf_queued(info, dev.direction));

  /* Assert the amount of frames of available buffer + queued buf is
   * greater than or equal to the buffer size, 2 bytes per frame
   */
  queued = hfp_buf_queued(info, dev.direction);
  buffer_frames = 500;
  hfp_buf_acquire(info, dev.direction, &samples, &buffer_frames);
  ASSERT_GE(info->playback_buf->used_size / 2, buffer_frames + queued);

  /* Consume all queued data from read buffer */
  buf_increment_read(info->playback_buf, queued * 2);

  queued = hfp_buf_queued(info, dev.direction);
  ASSERT_EQ(0, queued);

  /* Assert consecutive acquire buffer will acquire full used size of buffer */
  buffer_frames = 500;
  hfp_buf_acquire(info, dev.direction, &samples, &buffer_frames);
  hfp_buf_release(info, dev.direction, buffer_frames);

  buffer_frames2 = 500;
  hfp_buf_acquire(info, dev.direction, &samples, &buffer_frames2);
  hfp_buf_release(info, dev.direction, buffer_frames2);

  ASSERT_GE(info->playback_buf->used_size / 2, buffer_frames + buffer_frames2);

  hfp_info_destroy(info);
}

TEST(HfpInfo, AcquireCaptureBuffer) {
  unsigned buffer_frames, buffer_frames2;
  uint8_t* samples;

  ResetStubData();

  info = hfp_info_create(HFP_CODEC_ID_CVSD);
  ASSERT_NE(info, (void*)NULL);

  hfp_info_start(1, 48, info);
  dev.direction = CRAS_STREAM_INPUT;
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));

  /* Put fake data 100 bytes(50 frames) in capture buf for test */
  buf_increment_write(info->capture_buf, 100);

  /* Assert successfully acquire and release 100 bytes of data */
  buffer_frames = 50;
  hfp_buf_acquire(info, dev.direction, &samples, &buffer_frames);
  ASSERT_EQ(50, buffer_frames);

  hfp_buf_release(info, dev.direction, buffer_frames);
  ASSERT_EQ(0, hfp_buf_queued(info, dev.direction));

  /* Push fake data to capture buffer */
  buf_increment_write(info->capture_buf, info->capture_buf->used_size - 100);
  buf_increment_write(info->capture_buf, 100);

  /* Assert consecutive acquire call will consume the whole buffer */
  buffer_frames = 1000;
  hfp_buf_acquire(info, dev.direction, &samples, &buffer_frames);
  hfp_buf_release(info, dev.direction, buffer_frames);
  ASSERT_GE(1000, buffer_frames);

  buffer_frames2 = 1000;
  hfp_buf_acquire(info, dev.direction, &samples, &buffer_frames2);
  hfp_buf_release(info, dev.direction, buffer_frames2);

  ASSERT_GE(info->capture_buf->used_size / 2, buffer_frames + buffer_frames2);

  hfp_info_destroy(info);
}

TEST(HfpInfo, HfpReadWriteFD) {
  int rc;
  int sock[2];
  uint8_t sample[480];
  uint8_t* buf;
  unsigned buffer_count;

  ResetStubData();

  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));

  info = hfp_info_create(HFP_CODEC_ID_CVSD);
  ASSERT_NE(info, (void*)NULL);

  dev.direction = CRAS_STREAM_INPUT;
  hfp_info_start(sock[1], 48, info);
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));

  /* Mock the sco fd and send some fake data */
  send(sock[0], sample, 48, 0);

  rc = hfp_read(info);
  ASSERT_EQ(48, rc);

  rc = hfp_buf_queued(info, dev.direction);
  ASSERT_EQ(48 / 2, rc);

  /* Fill the write buffer*/
  buffer_count = info->capture_buf->used_size;
  buf = buf_write_pointer_size(info->capture_buf, &buffer_count);
  buf_increment_write(info->capture_buf, buffer_count);
  ASSERT_NE((void*)NULL, buf);

  rc = hfp_read(info);
  ASSERT_EQ(0, rc);

  ASSERT_EQ(0, hfp_info_rm_iodev(info, dev.direction));
  dev.direction = CRAS_STREAM_OUTPUT;
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));

  /* Initial buffer is empty */
  rc = hfp_write(info);
  ASSERT_EQ(0, rc);

  buffer_count = 1024;
  buf = buf_write_pointer_size(info->playback_buf, &buffer_count);
  buf_increment_write(info->playback_buf, buffer_count);

  rc = hfp_write(info);
  ASSERT_EQ(48, rc);

  rc = recv(sock[0], sample, 48, 0);
  ASSERT_EQ(48, rc);

  hfp_info_destroy(info);
}

TEST(HfpInfo, StartHfpInfo) {
  int sock[2];

  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));

  info = hfp_info_create(HFP_CODEC_ID_CVSD);
  ASSERT_NE(info, (void*)NULL);

  hfp_info_start(sock[0], 48, info);
  ASSERT_EQ(1, hfp_info_running(info));
  ASSERT_EQ(cb_data, (void*)info);

  hfp_info_stop(info);
  ASSERT_EQ(0, hfp_info_running(info));
  ASSERT_EQ(NULL, cb_data);

  hfp_info_destroy(info);
}

TEST(HfpInfo, StartHfpInfoAndRead) {
  int rc;
  int sock[2];
  uint8_t sample[480];

  ResetStubData();

  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));

  info = hfp_info_create(HFP_CODEC_ID_CVSD);
  ASSERT_NE(info, (void*)NULL);

  /* Start and send two chunk of fake data */
  hfp_info_start(sock[1], 48, info);
  send(sock[0], sample, 48, 0);
  send(sock[0], sample, 48, 0);

  /* Trigger thread callback */
  thread_cb((struct hfp_info*)cb_data);

  dev.direction = CRAS_STREAM_INPUT;
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));

  /* Expect no data read, since no idev present at previous thread callback */
  rc = hfp_buf_queued(info, dev.direction);
  ASSERT_EQ(0, rc);

  /* Trigger thread callback after idev added. */
  ts.tv_sec = 0;
  ts.tv_nsec = 5000000;
  thread_cb((struct hfp_info*)cb_data);

  rc = hfp_buf_queued(info, dev.direction);
  ASSERT_EQ(48 / 2, rc);

  /* Assert wait time is unchanged. */
  ASSERT_EQ(0, ts.tv_sec);
  ASSERT_EQ(5000000, ts.tv_nsec);

  hfp_info_stop(info);
  ASSERT_EQ(0, hfp_info_running(info));

  hfp_info_destroy(info);
}

TEST(HfpInfo, StartHfpInfoAndWrite) {
  int rc;
  int sock[2];
  uint8_t sample[480];

  ResetStubData();

  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));

  info = hfp_info_create(HFP_CODEC_ID_CVSD);
  ASSERT_NE(info, (void*)NULL);

  hfp_info_start(sock[1], 48, info);
  send(sock[0], sample, 48, 0);
  send(sock[0], sample, 48, 0);

  /* Trigger thread callback */
  thread_cb((struct hfp_info*)cb_data);

  /* Without odev in presence, zero packet should be sent. */
  rc = recv(sock[0], sample, 48, 0);
  ASSERT_EQ(48, rc);

  dev.direction = CRAS_STREAM_OUTPUT;
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));

  /* Assert queued samples unchanged before output device added */
  ASSERT_EQ(0, hfp_buf_queued(info, dev.direction));

  /* Put some fake data and trigger thread callback again */
  buf_increment_write(info->playback_buf, 1008);
  thread_cb((struct hfp_info*)cb_data);

  /* Assert some samples written */
  rc = recv(sock[0], sample, 48, 0);
  ASSERT_EQ(48, rc);
  ASSERT_EQ(480, hfp_buf_queued(info, dev.direction));

  hfp_info_stop(info);
  hfp_info_destroy(info);
}

void send_mSBC_packet(int fd, unsigned seq, int broken_pkt) {
  /* These three bytes are h2 header, frame count and mSBC sync word.
   * The second octet of H2 header is composed by 4 bits fixed 0x8 and 4 bits
   * sequence number 0000, 0011, 1100, 1111.
   */
  uint8_t headers[4][3] = {{0x01, 0x08, 0xAD},
                           {0x01, 0x38, 0xAD},
                           {0x01, 0xc8, 0xAD},
                           {0x01, 0xf8, 0xAD}};
  /* These three bytes are HCI SCO Data packet header, we only care the
   * Packet_Status_Flag bits, which are the bit 4 to 5 in the second octet.
   */
  uint8_t sco_header[] = {0x01, 0x01, 0x3c};
  uint8_t zero_frame[] = {
      0xad, 0x00, 0x00, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x77, 0x6d, 0xb6, 0xdd,
      0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d, 0xb6,
      0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d,
      0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77,
      0x6d, 0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6c};
  if (broken_pkt)
    sco_header[1] = 0x11;

  send(fd, sco_header, 3, 0);
  send(fd, headers[seq % 4], 3, 0);
  send(fd, zero_frame, 57, 0);
}

TEST(HfpInfo, StartHfpInfoAndReadMsbc) {
  int sock[2];
  int pkt_count = 0;
  int rc;
  uint8_t sample[480];
  ResetStubData();

  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));

  set_sbc_codec_decoded_out(MSBC_CODE_SIZE);

  info = hfp_info_create(HFP_CODEC_ID_MSBC);
  ASSERT_NE(info, (void*)NULL);
  ASSERT_EQ(2, get_msbc_codec_create_called());
  ASSERT_EQ(1, cras_msbc_plc_create_called);

  /* Start and send an mSBC packets with all zero samples */
  hfp_info_start(sock[1], 63, info);
  send_mSBC_packet(sock[0], pkt_count++, 0);

  /* Trigger thread callback */
  thread_cb((struct hfp_info*)cb_data);

  /* Expect one empty mSBC packet is send, because no odev in presence. */
  rc = recv(sock[0], sample, MSBC_PKT_SIZE, 0);
  ASSERT_EQ(MSBC_PKT_SIZE, rc);

  dev.direction = CRAS_STREAM_INPUT;
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));

  /* Expect no data read, since no idev present at previous thread callback */
  ASSERT_EQ(0, hfp_buf_queued(info, dev.direction));

  send_mSBC_packet(sock[0], pkt_count, 0);

  /* Trigger thread callback after idev added. */
  thread_cb((struct hfp_info*)cb_data);
  rc = recv(sock[0], sample, MSBC_PKT_SIZE, 0);
  ASSERT_EQ(MSBC_PKT_SIZE, rc);

  ASSERT_EQ(pkt_count * MSBC_CODE_SIZE / 2,
            hfp_buf_queued(info, dev.direction));
  ASSERT_EQ(2, cras_msbc_plc_handle_good_frames_called);
  pkt_count++;
  /* When the third packet is lost, we should call the handle_bad_packet and
   * still have right size of samples queued
   */
  pkt_count++;
  send_mSBC_packet(sock[0], pkt_count, 0);
  thread_cb((struct hfp_info*)cb_data);
  rc = recv(sock[0], sample, MSBC_PKT_SIZE, 0);
  ASSERT_EQ(MSBC_PKT_SIZE, rc);

  /* Packet 1, 2, 4 are all good frames */
  ASSERT_EQ(3, cras_msbc_plc_handle_good_frames_called);
  ASSERT_EQ(1, cras_msbc_plc_handle_bad_frames_called);
  ASSERT_EQ(pkt_count * MSBC_CODE_SIZE / 2,
            hfp_buf_queued(info, dev.direction));
  pkt_count++;
  /* If the erroneous data reporting marks the packet as broken, we should
   * also call the handle_bad_packet and have the right size of samples queued.
   */
  send_mSBC_packet(sock[0], pkt_count, 1);

  set_sbc_codec_decoded_fail(1);

  thread_cb((struct hfp_info*)cb_data);
  rc = recv(sock[0], sample, MSBC_PKT_SIZE, 0);
  ASSERT_EQ(MSBC_PKT_SIZE, rc);

  ASSERT_EQ(3, cras_msbc_plc_handle_good_frames_called);
  ASSERT_EQ(2, cras_msbc_plc_handle_bad_frames_called);
  ASSERT_EQ(pkt_count * MSBC_CODE_SIZE / 2,
            hfp_buf_queued(info, dev.direction));
  pkt_count++;
  /* If we can't decode the packet, we should also call the handle_bad_packet
   * and have the right size of samples queued
   */
  send_mSBC_packet(sock[0], pkt_count, 0);

  set_sbc_codec_decoded_fail(1);

  thread_cb((struct hfp_info*)cb_data);
  rc = recv(sock[0], sample, MSBC_PKT_SIZE, 0);
  ASSERT_EQ(MSBC_PKT_SIZE, rc);

  ASSERT_EQ(3, cras_msbc_plc_handle_good_frames_called);
  ASSERT_EQ(3, cras_msbc_plc_handle_bad_frames_called);
  ASSERT_EQ(pkt_count * MSBC_CODE_SIZE / 2,
            hfp_buf_queued(info, dev.direction));

  hfp_info_stop(info);
  ASSERT_EQ(0, hfp_info_running(info));

  hfp_info_destroy(info);
}

TEST(HfpInfo, StartHfpInfoAndWriteMsbc) {
  int rc;
  int sock[2];
  uint8_t sample[480];

  ResetStubData();

  set_sbc_codec_encoded_out(57);
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sock));

  info = hfp_info_create(HFP_CODEC_ID_MSBC);
  ASSERT_NE(info, (void*)NULL);

  hfp_info_start(sock[1], 63, info);
  send(sock[0], sample, 63, 0);

  /* Trigger thread callback */
  thread_cb((struct hfp_info*)cb_data);

  dev.direction = CRAS_STREAM_OUTPUT;
  ASSERT_EQ(0, hfp_info_add_iodev(info, dev.direction, dev.format));

  /* Assert queued samples unchanged before output device added */
  ASSERT_EQ(0, hfp_buf_queued(info, dev.direction));

  /* Put some fake data and trigger thread callback again */
  send(sock[0], sample, 63, 0);
  buf_increment_write(info->playback_buf, 240);
  thread_cb((struct hfp_info*)cb_data);

  /* Assert some samples written */
  rc = recv(sock[0], sample, 60, 0);
  ASSERT_EQ(60, rc);
  ASSERT_EQ(0, hfp_buf_queued(info, dev.direction));

  hfp_info_stop(info);
  hfp_info_destroy(info);
}

}  // namespace

extern "C" {

struct audio_thread* cras_iodev_list_get_audio_thread() {
  return NULL;
}

void audio_thread_add_callback(int fd, thread_callback cb, void* data) {
  thread_cb = cb;
  cb_data = data;
  return;
}

int audio_thread_rm_callback_sync(struct audio_thread* thread, int fd) {
  thread_cb = NULL;
  cb_data = NULL;
  return 0;
}

void audio_thread_rm_callback(int fd) {}

struct cras_msbc_plc* cras_msbc_plc_create() {
  cras_msbc_plc_create_called++;
  return NULL;
}

void cras_msbc_plc_destroy(struct cras_msbc_plc* plc) {}

int cras_msbc_plc_handle_bad_frames(struct cras_msbc_plc* plc,
                                    struct cras_audio_codec* codec,
                                    uint8_t* output) {
  cras_msbc_plc_handle_bad_frames_called++;
  return MSBC_CODE_SIZE;
}

int cras_msbc_plc_handle_good_frames(struct cras_msbc_plc* plc,
                                     const uint8_t* input,
                                     uint8_t* output) {
  cras_msbc_plc_handle_good_frames_called++;
  return MSBC_CODE_SIZE;
}
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
