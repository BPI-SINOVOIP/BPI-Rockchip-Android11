// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "cras_messages.h"

//  Include C file to test static functions.
#include "cras_client.c"
}

static const cras_stream_id_t FIRST_STREAM_ID = 1;

static int pthread_create_called;
static int pthread_join_called;
static int pthread_cond_timedwait_called;
static int pthread_cond_timedwait_retval;
static int close_called;
static int sendmsg_called;
static void* mmap_return_value;
static int samples_ready_called;
static int samples_ready_frames_value;
static uint8_t* samples_ready_samples_value;

static int pthread_create_returned_value;

namespace {

void InitStaticVariables() {
  pthread_create_called = 0;
  pthread_join_called = 0;
  pthread_cond_timedwait_called = 0;
  pthread_cond_timedwait_retval = 0;
  close_called = 0;
  sendmsg_called = 0;
  pthread_create_returned_value = 0;
  mmap_return_value = NULL;
  samples_ready_called = 0;
  samples_ready_frames_value = 0;
}

class CrasClientTestSuite : public testing::Test {
 protected:
  struct cras_audio_shm* InitShm() {
    struct cras_audio_shm* shm =
        static_cast<struct cras_audio_shm*>(calloc(1, sizeof(*shm)));
    shm->header =
        static_cast<cras_audio_shm_header*>(calloc(1, sizeof(*shm->header)));
    cras_shm_set_frame_bytes(shm, 4);
    uint32_t used_size = shm_writable_frames_ * 4;
    cras_shm_set_used_size(shm, used_size);
    shm->samples_info.length = used_size * 2;
    memcpy(&shm->header->config, &shm->config, sizeof(shm->config));
    return shm;
  }

  virtual void SetUp() {
    shm_writable_frames_ = 100;
    InitStaticVariables();

    memset(&client_, 0, sizeof(client_));
    client_.server_fd_state = CRAS_SOCKET_STATE_CONNECTED;
    memset(&stream_, 0, sizeof(stream_));
    stream_.id = FIRST_STREAM_ID;

    struct cras_stream_params* config =
        static_cast<cras_stream_params*>(calloc(1, sizeof(*config)));
    config->buffer_frames = 1024;
    config->cb_threshold = 512;
    stream_.config = config;
  }

  virtual void TearDown() {
    if (stream_.config) {
      free(stream_.config);
      stream_.config = NULL;
    }

    if (stream_.shm) {
      free(stream_.shm->header);
    }
    free(stream_.shm);
    stream_.shm = NULL;
  }

  void StreamConnected(CRAS_STREAM_DIRECTION direction);

  void StreamConnectedFail(CRAS_STREAM_DIRECTION direction);

  struct client_stream stream_;
  struct cras_client client_;
  int shm_writable_frames_;
};

void set_audio_format(struct cras_audio_format* format,
                      snd_pcm_format_t pcm_format,
                      size_t frame_rate,
                      size_t num_channels) {
  format->format = pcm_format;
  format->frame_rate = frame_rate;
  format->num_channels = num_channels;
  for (size_t i = 0; i < CRAS_CH_MAX; ++i)
    format->channel_layout[i] = i < num_channels ? i : -1;
}

int capture_samples_ready(cras_client* client,
                          cras_stream_id_t stream_id,
                          uint8_t* samples,
                          size_t frames,
                          const timespec* sample_ts,
                          void* arg) {
  samples_ready_called++;
  samples_ready_samples_value = samples;
  samples_ready_frames_value = frames;
  return frames;
}

TEST_F(CrasClientTestSuite, HandleCaptureDataReady) {
  struct cras_audio_shm* shm;

  stream_.direction = CRAS_STREAM_INPUT;

  shm_writable_frames_ = 480;
  shm = InitShm();
  stream_.shm = shm;
  stream_.config->buffer_frames = 480;
  stream_.config->cb_threshold = 480;
  stream_.config->aud_cb = capture_samples_ready;
  stream_.config->unified_cb = 0;

  shm->header->write_buf_idx = 0;
  shm->header->read_buf_idx = 0;
  shm->header->write_offset[0] = 480 * 4;
  shm->header->read_offset[0] = 0;

  /* Normal scenario: read buffer has full of data written,
   * handle_capture_data_ready() should consume all 480 frames and move
   * read_buf_idx to the next buffer. */
  handle_capture_data_ready(&stream_, 480);
  EXPECT_EQ(1, samples_ready_called);
  EXPECT_EQ(480, samples_ready_frames_value);
  EXPECT_EQ(cras_shm_buff_for_idx(shm, 0), samples_ready_samples_value);
  EXPECT_EQ(1, shm->header->read_buf_idx);
  EXPECT_EQ(0, shm->header->write_offset[0]);
  EXPECT_EQ(0, shm->header->read_offset[0]);

  /* At the beginning of overrun: handle_capture_data_ready() should not
   * proceed to call audio_cb because there's no data captured. */
  shm->header->read_buf_idx = 0;
  shm->header->write_offset[0] = 0;
  shm->header->read_offset[0] = 0;
  handle_capture_data_ready(&stream_, 480);
  EXPECT_EQ(1, samples_ready_called);
  EXPECT_EQ(0, shm->header->read_buf_idx);

  /* In the middle of overrun: partially written buffer should trigger
   * audio_cb, feed the full-sized read buffer to client. */
  shm->header->read_buf_idx = 0;
  shm->header->write_offset[0] = 123;
  shm->header->read_offset[0] = 0;
  handle_capture_data_ready(&stream_, 480);
  EXPECT_EQ(1, samples_ready_called);
  EXPECT_EQ(0, shm->header->read_buf_idx);
}

void CrasClientTestSuite::StreamConnected(CRAS_STREAM_DIRECTION direction) {
  struct cras_client_stream_connected msg;
  int shm_fds[2] = {0, 1};
  int shm_max_size = 600;
  size_t format_bytes;
  size_t effects = 123;
  struct cras_audio_shm_header* header;

  stream_.direction = direction;
  set_audio_format(&stream_.config->format, SND_PCM_FORMAT_S16_LE, 48000, 4);

  struct cras_audio_format server_format;
  set_audio_format(&server_format, SND_PCM_FORMAT_S16_LE, 44100, 2);

  // Initialize shm area
  format_bytes = cras_get_format_bytes(&server_format);
  header = (struct cras_audio_shm_header*)calloc(1, sizeof(*header));
  header->config.frame_bytes = format_bytes;
  header->config.used_size = shm_writable_frames_ * format_bytes;

  mmap_return_value = header;

  cras_fill_client_stream_connected(&msg, 0, stream_.id, &server_format,
                                    shm_max_size, effects);

  stream_connected(&stream_, &msg, shm_fds, 2);

  EXPECT_EQ(CRAS_THREAD_RUNNING, stream_.thread.state);
  EXPECT_EQ(header, stream_.shm->header);
}

TEST_F(CrasClientTestSuite, InputStreamConnected) {
  StreamConnected(CRAS_STREAM_INPUT);
}

TEST_F(CrasClientTestSuite, OutputStreamConnected) {
  StreamConnected(CRAS_STREAM_OUTPUT);
}

void CrasClientTestSuite::StreamConnectedFail(CRAS_STREAM_DIRECTION direction) {
  struct cras_client_stream_connected msg;
  int shm_fds[2] = {0, 1};
  int shm_max_size = 600;
  size_t format_bytes;
  size_t effects = 123;
  struct cras_audio_shm_header header;
  int rc;

  stream_.direction = direction;
  set_audio_format(&stream_.config->format, SND_PCM_FORMAT_S16_LE, 48000, 4);

  struct cras_audio_format server_format;
  set_audio_format(&server_format, SND_PCM_FORMAT_S16_LE, 44100, 2);

  // Thread setup
  rc = pipe(stream_.wake_fds);
  ASSERT_EQ(0, rc);
  stream_.thread.state = CRAS_THREAD_WARMUP;

  // Initialize shm area
  format_bytes = cras_get_format_bytes(&server_format);
  memset(&header, 0, sizeof(header));
  header.config.frame_bytes = format_bytes;
  header.config.used_size = shm_writable_frames_ * format_bytes;

  mmap_return_value = &header;

  // Put an error in the message.
  cras_fill_client_stream_connected(&msg, 1, stream_.id, &server_format,
                                    shm_max_size, effects);

  stream_connected(&stream_, &msg, shm_fds, 2);

  EXPECT_EQ(CRAS_THREAD_STOP, stream_.thread.state);
  EXPECT_EQ(4, close_called);  // close the pipefds and shm_fds
}

TEST_F(CrasClientTestSuite, InputStreamConnectedFail) {
  StreamConnectedFail(CRAS_STREAM_INPUT);
}

TEST_F(CrasClientTestSuite, OutputStreamConnectedFail) {
  StreamConnectedFail(CRAS_STREAM_OUTPUT);
}

TEST_F(CrasClientTestSuite, AddAndRemoveStream) {
  cras_stream_id_t stream_id;
  struct cras_disconnect_stream_message msg;
  int serv_fds[2];
  int rc;

  // Dynamically allocate the stream so that it can be freed later.
  struct client_stream* stream_ptr =
      (struct client_stream*)malloc(sizeof(*stream_ptr));
  memcpy(stream_ptr, &stream_, sizeof(client_stream));

  stream_ptr->config =
      (struct cras_stream_params*)malloc(sizeof(*(stream_ptr->config)));
  memcpy(stream_ptr->config, stream_.config, sizeof(*(stream_.config)));

  stream_ptr->wake_fds[0] = -1;
  stream_ptr->wake_fds[1] = -1;

  pthread_cond_timedwait_retval = ETIMEDOUT;
  EXPECT_EQ(-ETIMEDOUT, client_thread_add_stream(&client_, stream_ptr,
                                                 &stream_id, NO_DEVICE));
  EXPECT_EQ(pthread_cond_timedwait_called, 1);
  EXPECT_EQ(pthread_join_called, 0);

  InitStaticVariables();
  EXPECT_EQ(
      0, client_thread_add_stream(&client_, stream_ptr, &stream_id, NO_DEVICE));
  EXPECT_EQ(&client_, stream_ptr->client);
  EXPECT_EQ(stream_id, stream_ptr->id);
  EXPECT_EQ(pthread_create_called, 1);
  EXPECT_NE(-1, stream_ptr->wake_fds[0]);
  EXPECT_NE(-1, stream_ptr->wake_fds[1]);
  EXPECT_EQ(1, sendmsg_called);  // send connect message to server
  EXPECT_EQ(stream_ptr, stream_from_id(&client_, stream_id));

  stream_ptr->thread.state = CRAS_THREAD_RUNNING;

  rc = pipe(serv_fds);
  EXPECT_EQ(0, rc);
  client_.server_fd = serv_fds[1];
  client_.server_fd_state = CRAS_SOCKET_STATE_CONNECTED;
  EXPECT_EQ(0, client_thread_rm_stream(&client_, stream_id));

  rc = read(serv_fds[0], &msg, sizeof(msg));
  EXPECT_EQ(sizeof(msg), rc);
  EXPECT_EQ(stream_id, msg.stream_id);
  EXPECT_EQ(1, pthread_join_called);

  EXPECT_EQ(NULL, stream_from_id(&client_, stream_id));
}

TEST_F(CrasClientTestSuite, SetOutputStreamVolume) {
  cras_stream_id_t stream_id;

  client_thread_add_stream(&client_, &stream_, &stream_id, NO_DEVICE);
  EXPECT_EQ(&stream_, stream_from_id(&client_, stream_id));

  /* Set volume before stream connected. */
  client_thread_set_stream_volume(&client_, stream_id, 0.3f);
  StreamConnected(CRAS_STREAM_OUTPUT);
  EXPECT_EQ(0.3f, cras_shm_get_volume_scaler(stream_.shm));

  /* Set volume after stream connected. */
  client_thread_set_stream_volume(&client_, stream_id, 0.6f);
  EXPECT_EQ(0.6f, cras_shm_get_volume_scaler(stream_.shm));
}

TEST_F(CrasClientTestSuite, SetInputStreamVolume) {
  cras_stream_id_t stream_id;

  client_thread_add_stream(&client_, &stream_, &stream_id, NO_DEVICE);
  EXPECT_EQ(&stream_, stream_from_id(&client_, stream_id));

  /* Set volume before stream connected. */
  client_thread_set_stream_volume(&client_, stream_id, 0.3f);
  StreamConnected(CRAS_STREAM_INPUT);
  EXPECT_EQ(0.3f, cras_shm_get_volume_scaler(stream_.shm));

  /* Set volume after stream connected. */
  client_thread_set_stream_volume(&client_, stream_id, 0.6f);
  EXPECT_EQ(0.6f, cras_shm_get_volume_scaler(stream_.shm));
}

TEST(CrasClientTest, InitStreamVolume) {
  cras_stream_id_t stream_id;
  struct cras_stream_params config;
  struct add_stream_command_message cmd_msg;
  int rc;
  struct cras_client client;

  memset(&client, 0, sizeof(client));
  memset(&config, 0, sizeof(config));
  client.server_fd_state = CRAS_SOCKET_STATE_CONNECTED;

  config.aud_cb = reinterpret_cast<cras_playback_cb_t>(0x123);
  config.err_cb = reinterpret_cast<cras_error_cb_t>(0x456);
  client.thread.state = CRAS_THREAD_RUNNING;
  rc = pipe(client.command_reply_fds);
  EXPECT_EQ(0, rc);
  rc = pipe(client.command_fds);
  EXPECT_EQ(0, rc);

  rc = write(client.command_reply_fds[1], &rc, sizeof(rc));
  cras_client_add_stream(&client, &stream_id, &config);

  rc = read(client.command_fds[0], &cmd_msg, sizeof(cmd_msg));
  EXPECT_EQ(sizeof(cmd_msg), rc);
  EXPECT_NE((void*)NULL, cmd_msg.stream);

  EXPECT_EQ(1.0f, cmd_msg.stream->volume_scaler);

  if (cmd_msg.stream->config)
    free(cmd_msg.stream->config);
  free(cmd_msg.stream);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

/* stubs */
extern "C" {

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
  ++sendmsg_called;
  return msg->msg_iov->iov_len;
}

int close(int fd) {
  ++close_called;
  return 0;
}

int pthread_create(pthread_t* thread,
                   const pthread_attr_t* attr,
                   void* (*start_routine)(void*),
                   void* arg) {
  ++pthread_create_called;
  return pthread_create_returned_value;
}

int pthread_join(pthread_t thread, void** retval) {
  ++pthread_join_called;
  return 0;
}

int pthread_cond_timedwait(pthread_cond_t* __restrict cond,
                           pthread_mutex_t* __restrict mutex,
                           const struct timespec* __restrict timeout) {
  ++pthread_cond_timedwait_called;
  return pthread_cond_timedwait_retval;
}

int clock_gettime(clockid_t clk_id, struct timespec* tp) {
  tp->tv_sec = 0;
  tp->tv_nsec = 0;
  return 0;
}

void* mmap(void* addr,
           size_t length,
           int prot,
           int flags,
           int fd,
           off_t offset) {
  return mmap_return_value;
}

struct cras_audio_format* cras_audio_format_create(snd_pcm_format_t format,
                                                   size_t frame_rate,
                                                   size_t num_channels) {
  return reinterpret_cast<struct cras_audio_format*>(0x123);
}

void cras_audio_format_destroy(struct cras_audio_format* fmt) {}
}
