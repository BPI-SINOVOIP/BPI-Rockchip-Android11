// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdint.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>

#include <memory>

extern "C" {
#include "cras_iodev.h"    // stubbed
#include "cras_rstream.h"  // stubbed
#include "cras_shm.h"
#include "cras_types.h"
#include "dev_io.h"      // tested
#include "dev_stream.h"  // tested
#include "utlist.h"

struct audio_thread_event_log* atlog;
}

#include "dev_io_stubs.h"
#include "iodev_stub.h"
#include "metrics_stub.h"
#include "rstream_stub.h"

#define FAKE_POLL_FD 33

namespace {

class TimingSuite : public testing::Test {
 protected:
  virtual void SetUp() {
    atlog = static_cast<audio_thread_event_log*>(calloc(1, sizeof(*atlog)));
    iodev_stub_reset();
    rstream_stub_reset();
  }

  virtual void TearDown() { free(atlog); }

  timespec SingleInputDevNextWake(
      size_t dev_cb_threshold,
      size_t dev_level,
      const timespec* level_timestamp,
      cras_audio_format* dev_format,
      const std::vector<StreamPtr>& streams,
      CRAS_NODE_TYPE active_node_type = CRAS_NODE_TYPE_MIC) {
    struct open_dev* dev_list_ = NULL;

    DevicePtr dev = create_device(CRAS_STREAM_INPUT, dev_cb_threshold,
                                  dev_format, active_node_type);
    dev->dev->input_streaming = true;
    DL_APPEND(dev_list_, dev->odev.get());

    for (auto const& stream : streams) {
      add_stream_to_dev(dev->dev, stream);
    }

    // Set response for frames_queued.
    iodev_stub_frames_queued(dev->dev.get(), dev_level, *level_timestamp);

    dev_io_send_captured_samples(dev_list_);

    struct timespec dev_time;
    dev_time.tv_sec = level_timestamp->tv_sec + 500;  // Far in the future.
    dev_io_next_input_wake(&dev_list_, &dev_time);
    return dev_time;
  }

  timespec SingleOutputDevNextWake(
      size_t dev_cb_threshold,
      size_t dev_level,
      const timespec* level_timestamp,
      cras_audio_format* dev_format,
      const std::vector<StreamPtr>& streams,
      const timespec* dev_wake_ts,
      CRAS_NODE_TYPE active_node_type = CRAS_NODE_TYPE_HEADPHONE) {
    struct open_dev* dev_list_ = NULL;

    DevicePtr dev = create_device(CRAS_STREAM_OUTPUT, dev_cb_threshold,
                                  dev_format, active_node_type);
    DL_APPEND(dev_list_, dev->odev.get());

    for (auto const& stream : streams) {
      add_stream_to_dev(dev->dev, stream);
    }

    dev->odev->wake_ts = *dev_wake_ts;

    // Set response for frames_queued.
    iodev_stub_frames_queued(dev->dev.get(), dev_level, *level_timestamp);

    struct timespec dev_time, now;
    dev_time.tv_sec = level_timestamp->tv_sec + 500;  // Far in the future.
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    dev_io_next_output_wake(&dev_list_, &dev_time, &now);
    return dev_time;
  }
};

extern "C" {
//  From librt. Fix now at this time.
int clock_gettime(clockid_t clk_id, struct timespec* tp) {
  tp->tv_sec = 12345;
  tp->tv_nsec = 987654321;
  return 0;
}
};

// Add a new input stream, make sure the initial next_cb_ts is 0.
TEST_F(TimingSuite, NewInputStreamInit) {
  struct open_dev* dev_list_ = NULL;

  cras_audio_format format;
  fill_audio_format(&format, 48000);
  DevicePtr dev =
      create_device(CRAS_STREAM_INPUT, 1024, &format, CRAS_NODE_TYPE_MIC);
  DL_APPEND(dev_list_, dev->odev.get());
  struct cras_iodev* iodev = dev->odev->dev;

  ShmPtr shm = create_shm(480);
  RstreamPtr rstream =
      create_rstream(1, CRAS_STREAM_INPUT, 480, &format, shm.get());

  dev_io_append_stream(&dev_list_, rstream.get(), &iodev, 1);

  EXPECT_EQ(0, rstream->next_cb_ts.tv_sec);
  EXPECT_EQ(0, rstream->next_cb_ts.tv_nsec);

  dev_stream_destroy(iodev->streams);
}

// There is the pseudo code about wake up time for an input device.
//
// function set_input_dev_wake_ts(dev):
//   wake_ts = now + 20s #rule_1
//
//   cap_limit = MIN(dev_stream_capture_avail(stream)) for stream on dev
//
//   for stream in dev:
//   wake_ts = MIN(get_input_wake_time(stream, cap_limit), wake_ts)
//             for stream on dev #rule_2
//   if cap_limit:
//     wake_ts = MIN(get_input_dev_max_wake_ts(dev), wake_ts) #rule_3
//
//   device.wake_ts = wake_ts
//
// function get_input_wake_time(stream, cap_limit):
//   needed_frames_from_device = dev_stream_capture_avail(stream)
//
//   if needed_frames_from_device > cap_limit: #rule_4
//     return None
//
//   if stream is USE_DEV_TIMING and stream is pending reply: #rule_5
//     return None
//
//   time_for_sample = The time when device gets enough samples #rule_6
//
//   wake_time_out = MAX(stream.next_cb_ts, time_for_sample) #rule_7
//
//   if stream is USE_DEV_TIMING:
//     wake_time_out =  time_for_sample #rule_8
//
//   return wake_time_out
//
// function get_input_dev_max_wake_ts(dev):
//   return MAX(5ms, The time when hw_level = buffer_size / 2) #rule_9
//
//
// dev_stream_capture_avail: The number of frames free to be written to in a
//                           capture stream.
//
// The following unittests will check these logics.

// Test rule_1.
// The device wake up time should be 20s from now.
TEST_F(TimingSuite, InputWakeTimeNoStreamWithBigBufferDevice) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  std::vector<StreamPtr> streams;
  timespec dev_time =
      SingleInputDevNextWake(4800000, 0, &start, &format, streams);

  const timespec add_millis = {20, 0};
  add_timespecs(&start, &add_millis);
  EXPECT_EQ(start.tv_sec, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_2, rule_4(Stream 1), rule_7(Stream 2)
// Stream 1: next_cb_ts = now, cb_threshold = 480, dev_offset = 0
// Stream 2: next_cb_ts = now + 5s, cb_threshold = 480, dev_offset = 200
// Stream 1 need 480 frames and Stream 2 need 240 frames. So 240 will be the
// cap_limit and Stream 1 will be ignored. The next wake up time should be
// the next_cb_ts of stream2.
TEST_F(TimingSuite, InputWakeTimeTwoStreamsWithFramesInside) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream1 = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &format);
  stream1->rstream->next_cb_ts = start;

  StreamPtr stream2 = create_stream(1, 2, CRAS_STREAM_INPUT, 480, &format);
  stream2->rstream->next_cb_ts = start;
  stream2->rstream->next_cb_ts.tv_sec += 5;
  rstream_stub_dev_offset(stream2->rstream.get(), 1, 200);

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream1));
  streams.emplace_back(std::move(stream2));
  timespec dev_time =
      SingleInputDevNextWake(480000, 0, &start, &format, streams);

  EXPECT_EQ(start.tv_sec + 5, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_2, rule_7
// Stream 1: next_cb_ts = now + 2s, cb_threshold = 480, dev_offset = 0
// Stream 2: next_cb_ts = now + 5s, cb_threshold = 480, dev_offset = 0
// The audio thread will choose the earliest next_cb_ts because the they have
// the same value of needed_frames_from_device. The next wake up time should
// be the next_cb_ts of stream1.
TEST_F(TimingSuite, InputWakeTimeTwoEmptyStreams) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream1 = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &format);
  stream1->rstream->next_cb_ts = start;
  stream1->rstream->next_cb_ts.tv_sec += 2;

  StreamPtr stream2 = create_stream(1, 2, CRAS_STREAM_INPUT, 480, &format);
  stream2->rstream->next_cb_ts = start;
  stream2->rstream->next_cb_ts.tv_sec += 5;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream1));
  streams.emplace_back(std::move(stream2));
  timespec dev_time =
      SingleInputDevNextWake(480000, 0, &start, &format, streams);

  EXPECT_EQ(start.tv_sec + 2, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_3.
// If cap_limit is zero from stream, input_dev_max_wake_ts should not
// be taken into account.
TEST_F(TimingSuite, InputWakeTimeOneFullStreamWithDeviceWakeUp) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &format);

  struct timespec start, stream_wake;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  // Set the next stream wake to be 10ms from now.
  const timespec ten_millis = {0, 10 * 1000 * 1000};
  stream_wake = start;
  add_timespecs(&stream_wake, &ten_millis);
  stream->rstream->next_cb_ts = stream_wake;

  // Add fake data so the stream has no room for more data.
  AddFakeDataToStream(stream.get(), 480);

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec wake_time = SingleInputDevNextWake(240, 0, &start, &format, streams);

  // Input device would wake at 5ms from now, but since stream cap_limit == 0
  // the final wake_time is determined by stream.
  EXPECT_EQ(stream_wake.tv_sec, wake_time.tv_sec);
  EXPECT_EQ(stream_wake.tv_nsec, wake_time.tv_nsec);
}

// Test rule_3 and rule_9.
// One empty stream with small device buffer. It should wake up when there are
// buffer_size / 2 frames in device buffer.
TEST_F(TimingSuite, InputWakeTimeOneStreamWithDeviceWakeUp) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &format);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  // The next callback of the new stream is 0.
  stream->rstream->next_cb_ts.tv_sec = 0;
  stream->rstream->next_cb_ts.tv_nsec = 0;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time = SingleInputDevNextWake(240, 0, &start, &format, streams);
  // The device wake up time should be 5ms from now. At that time there are
  // 240 frames in the device.
  const timespec add_millis = {0, 5 * 1000 * 1000};
  add_timespecs(&start, &add_millis);
  EXPECT_EQ(start.tv_sec, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_5.
// The stream with USE_DEV_TIMING flag will be ignore if it is pending reply.
TEST_F(TimingSuite, InputWakeTimeOneStreamUsingDevTimingWithPendingReply) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &format);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  // The next callback should be ignored.
  stream->rstream->next_cb_ts = start;
  stream->rstream->next_cb_ts.tv_sec += 10;
  stream->rstream->flags = USE_DEV_TIMING;
  rstream_stub_pending_reply(stream->rstream.get(), 1);

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time = SingleInputDevNextWake(4800, 0, &start, &format, streams);

  // The device wake up time should be 100ms from now. At that time the hw_level
  // is buffer_size / 2.
  const timespec add_millis = {0, 100 * 1000 * 1000};
  add_timespecs(&start, &add_millis);
  EXPECT_EQ(start.tv_sec, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_6.
// Add a new stream, the wake up time is the time when it has enough data to
// post.
TEST_F(TimingSuite, InputWakeTimeOneStreamWithEmptyDevice) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &format);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  // The next callback of the new stream is 0.
  stream->rstream->next_cb_ts.tv_sec = 0;
  stream->rstream->next_cb_ts.tv_nsec = 0;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time = SingleInputDevNextWake(600, 0, &start, &format, streams);

  // The device wake up time should be 10ms from now. At that time the
  // stream will have 480 samples to post.
  const timespec ten_millis = {0, 10 * 1000 * 1000};
  add_timespecs(&start, &ten_millis);
  EXPECT_EQ(0, streams[0]->rstream->next_cb_ts.tv_sec);
  EXPECT_EQ(0, streams[0]->rstream->next_cb_ts.tv_nsec);
  EXPECT_EQ(start.tv_sec, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_6.
// Add a new stream with enough frames in device, check the wake up time is
// right now.
TEST_F(TimingSuite, InputWakeTimeOneStreamWithFullDevice) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &format);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  // The next callback of the new stream is 0.
  stream->rstream->next_cb_ts.tv_sec = 0;
  stream->rstream->next_cb_ts.tv_nsec = 0;

  // If there are enough frames in the device, we should wake up immediately.
  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time =
      SingleInputDevNextWake(480, 480, &start, &format, streams);
  EXPECT_EQ(0, streams[0]->rstream->next_cb_ts.tv_sec);
  EXPECT_EQ(0, streams[0]->rstream->next_cb_ts.tv_nsec);
  EXPECT_EQ(start.tv_sec, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_8.
// The stream with USE_DEV_TIMING flag should wake up when it has enough frames
// to post.
TEST_F(TimingSuite, InputWakeTimeOneStreamUsingDevTiming) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &format);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  // The next callback should be ignored.
  stream->rstream->next_cb_ts = start;
  stream->rstream->next_cb_ts.tv_sec += 10;
  stream->rstream->flags = USE_DEV_TIMING;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time = SingleInputDevNextWake(600, 0, &start, &format, streams);

  // The device wake up time should be 10ms from now. At that time the
  // stream will have 480 samples to post.
  const timespec add_millis = {0, 10 * 1000 * 1000};
  add_timespecs(&start, &add_millis);
  EXPECT_EQ(start.tv_sec, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_9.
// The device wake up time should be 10ms from now. At that time the hw_level
// is buffer_size / 2.
TEST_F(TimingSuite, InputWakeTimeNoStreamSmallBufferDevice) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  std::vector<StreamPtr> streams;
  timespec dev_time = SingleInputDevNextWake(480, 0, &start, &format, streams);

  const timespec add_millis = {0, 10 * 1000 * 1000};
  add_timespecs(&start, &add_millis);
  EXPECT_EQ(start.tv_sec, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_9.
// There are more than buffer_size / 2 frames in the device. The device needs
// to sleep at least 5ms.
TEST_F(TimingSuite, InputWakeTimeOneStreamWithEnoughFramesInDevice) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &format);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  // Make next_cb_ts far from now.
  stream->rstream->next_cb_ts = start;
  stream->rstream->next_cb_ts.tv_sec += 10;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time =
      SingleInputDevNextWake(480, 480, &start, &format, streams);

  const timespec add_millis = {0, 5 * 1000 * 1000};
  add_timespecs(&start, &add_millis);
  EXPECT_EQ(start.tv_sec, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// One device, one stream, write a callback of data and check the sleep time is
// one more wakeup interval.
TEST_F(TimingSuite, WaitAfterFill) {
  const size_t cb_threshold = 480;

  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream =
      create_stream(1, 1, CRAS_STREAM_INPUT, cb_threshold, &format);
  // rstream's next callback is now and there is enough data to fill.
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  stream->rstream->next_cb_ts = start;
  AddFakeDataToStream(stream.get(), 480);

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time =
      SingleInputDevNextWake(cb_threshold, 0, &start, &format, streams);

  // The next callback should be scheduled 10ms in the future.
  // And the next wake up should reflect the only attached stream.
  const timespec ten_millis = {0, 10 * 1000 * 1000};
  add_timespecs(&start, &ten_millis);
  EXPECT_EQ(start.tv_sec, streams[0]->rstream->next_cb_ts.tv_sec);
  EXPECT_EQ(start.tv_nsec, streams[0]->rstream->next_cb_ts.tv_nsec);
  EXPECT_EQ(dev_time.tv_sec, streams[0]->rstream->next_cb_ts.tv_sec);
  EXPECT_EQ(dev_time.tv_nsec, streams[0]->rstream->next_cb_ts.tv_nsec);
}

// One device with one stream which has block_size larger than the device buffer
// level. If the device buffer level = 0, the input device wake time should be
// set to (buffer_size / 2) / device_rate secs.
TEST_F(TimingSuite, LargeCallbackStreamWithEmptyBuffer) {
  const size_t cb_threshold = 3000;
  const size_t dev_cb_threshold = 1200;
  const size_t dev_level = 0;

  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream =
      create_stream(1, 1, CRAS_STREAM_INPUT, cb_threshold, &format);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  stream->rstream->next_cb_ts = start;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time = SingleInputDevNextWake(dev_cb_threshold, dev_level,
                                             &start, &format, streams);

  struct timespec delta;
  subtract_timespecs(&dev_time, &start, &delta);
  // The next dev wake ts should be 25ms since the buffer level is empty and
  // 1200 / 48000 = 0.025.
  EXPECT_EQ(delta.tv_sec, 0);
  EXPECT_LT(delta.tv_nsec, 25000000 + 5000 * 1000);
  EXPECT_GT(delta.tv_nsec, 25000000 - 5000 * 1000);
}

// One device with one stream which has block_size larger than the device buffer
// level. If the device buffer level = buffer_size / 2, the input device wake
// time should be set to max(0, 5ms) = 5ms to prevent busy loop occurs.
TEST_F(TimingSuite, LargeCallbackStreamWithHalfFullBuffer) {
  const size_t cb_threshold = 3000;
  const size_t dev_cb_threshold = 1200;
  const size_t dev_level = 1200;

  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream =
      create_stream(1, 1, CRAS_STREAM_INPUT, cb_threshold, &format);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  stream->rstream->next_cb_ts = start;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time = SingleInputDevNextWake(dev_cb_threshold, dev_level,
                                             &start, &format, streams);

  struct timespec delta;
  subtract_timespecs(&dev_time, &start, &delta);
  // The next dev wake ts should be 5ms since the buffer level is half full.
  EXPECT_EQ(delta.tv_sec, 0);
  EXPECT_LT(delta.tv_nsec, 5000000 + 5000 * 1000);
  EXPECT_GT(delta.tv_nsec, 5000000 - 5000 * 1000);
}

// One device(48k), one stream(44.1k), write a callback of data and check that
// the sleep time is correct when doing SRC.
TEST_F(TimingSuite, WaitAfterFillSRC) {
  cras_audio_format dev_format;
  fill_audio_format(&dev_format, 48000);
  cras_audio_format stream_format;
  fill_audio_format(&stream_format, 44100);

  StreamPtr stream =
      create_stream(1, 1, CRAS_STREAM_INPUT, 441, &stream_format);
  // rstream's next callback is now and there is enough data to fill.
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  stream->rstream->next_cb_ts = start;
  AddFakeDataToStream(stream.get(), 441);

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time =
      SingleInputDevNextWake(480, 0, &start, &dev_format, streams);

  // The next callback should be scheduled 10ms in the future.
  struct timespec delta;
  subtract_timespecs(&dev_time, &start, &delta);
  EXPECT_LT(9900 * 1000, delta.tv_nsec);
  EXPECT_GT(10100 * 1000, delta.tv_nsec);
}

// One device, two streams. One stream is ready the other still needs data.
// Checks that the sleep interval is based on the time the device will take to
// supply the needed samples for stream2.
TEST_F(TimingSuite, WaitTwoStreamsSameFormat) {
  const size_t cb_threshold = 480;

  cras_audio_format format;
  fill_audio_format(&format, 48000);

  // stream1's next callback is now and there is enough data to fill.
  StreamPtr stream1 =
      create_stream(1, 1, CRAS_STREAM_INPUT, cb_threshold, &format);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  stream1->rstream->next_cb_ts = start;
  AddFakeDataToStream(stream1.get(), cb_threshold);

  // stream2 is only half full.
  StreamPtr stream2 =
      create_stream(1, 1, CRAS_STREAM_INPUT, cb_threshold, &format);
  stream2->rstream->next_cb_ts = start;
  AddFakeDataToStream(stream2.get(), 240);

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream1));
  streams.emplace_back(std::move(stream2));
  timespec dev_time =
      SingleInputDevNextWake(cb_threshold, 0, &start, &format, streams);

  // Should wait for approximately 5 milliseconds for 240 samples at 48k.
  struct timespec delta2;
  subtract_timespecs(&dev_time, &start, &delta2);
  EXPECT_LT(4900 * 1000, delta2.tv_nsec);
  EXPECT_GT(5100 * 1000, delta2.tv_nsec);
}

// One device(44.1), two streams(44.1, 48). One stream is ready the other still
// needs data. Checks that the sleep interval is based on the time the device
// will take to supply the needed samples for stream2, stream2 is sample rate
// converted from the 44.1k device to the 48k stream.
TEST_F(TimingSuite, WaitTwoStreamsDifferentRates) {
  cras_audio_format s1_format, s2_format;
  fill_audio_format(&s1_format, 44100);
  fill_audio_format(&s2_format, 48000);

  // stream1's next callback is now and there is enough data to fill.
  StreamPtr stream1 = create_stream(1, 1, CRAS_STREAM_INPUT, 441, &s1_format);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  stream1->rstream->next_cb_ts = start;
  AddFakeDataToStream(stream1.get(), 441);
  // stream2's next callback is now but there is only half a callback of data.
  StreamPtr stream2 = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &s2_format);
  stream2->rstream->next_cb_ts = start;
  AddFakeDataToStream(stream2.get(), 240);

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream1));
  streams.emplace_back(std::move(stream2));
  timespec dev_time =
      SingleInputDevNextWake(441, 0, &start, &s1_format, streams);

  // Should wait for approximately 5 milliseconds for 240 48k samples from the
  // 44.1k device.
  struct timespec delta2;
  subtract_timespecs(&dev_time, &start, &delta2);
  EXPECT_LT(4900 * 1000, delta2.tv_nsec);
  EXPECT_GT(5100 * 1000, delta2.tv_nsec);
}

// One device, two streams. Both streams get a full callback of data and the
// device has enough samples for the next callback already. Checks that the
// shorter of the two streams times is used for the next sleep interval.
TEST_F(TimingSuite, WaitTwoStreamsDifferentWakeupTimes) {
  cras_audio_format s1_format, s2_format;
  fill_audio_format(&s1_format, 44100);
  fill_audio_format(&s2_format, 48000);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  // stream1's next callback is in 3ms.
  StreamPtr stream1 = create_stream(1, 1, CRAS_STREAM_INPUT, 441, &s1_format);
  stream1->rstream->next_cb_ts = start;
  const timespec three_millis = {0, 3 * 1000 * 1000};
  add_timespecs(&stream1->rstream->next_cb_ts, &three_millis);
  AddFakeDataToStream(stream1.get(), 441);
  // stream2 is also ready next cb in 5ms..
  StreamPtr stream2 = create_stream(1, 1, CRAS_STREAM_INPUT, 480, &s2_format);
  stream2->rstream->next_cb_ts = start;
  const timespec five_millis = {0, 5 * 1000 * 1000};
  add_timespecs(&stream2->rstream->next_cb_ts, &five_millis);
  AddFakeDataToStream(stream1.get(), 480);

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream1));
  streams.emplace_back(std::move(stream2));
  timespec dev_time =
      SingleInputDevNextWake(441, 441, &start, &s1_format, streams);

  // Should wait for approximately 3 milliseconds for stream 1 first.
  struct timespec delta2;
  subtract_timespecs(&dev_time, &start, &delta2);
  EXPECT_LT(2900 * 1000, delta2.tv_nsec);
  EXPECT_GT(3100 * 1000, delta2.tv_nsec);
}

// One hotword stream attaches to hotword device. Input data has copied from
// device to stream but total number is less than cb_threshold. Hotword stream
// should be scheduled wake base on the samples needed to fill full shm.
TEST_F(TimingSuite, HotwordStreamUseDevTiming) {
  cras_audio_format fmt;
  fill_audio_format(&fmt, 48000);

  struct timespec start, delay;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 240, &fmt);
  stream->rstream->flags = HOTWORD_STREAM;
  stream->rstream->next_cb_ts = start;
  delay.tv_sec = 0;
  delay.tv_nsec = 3 * 1000 * 1000;
  add_timespecs(&stream->rstream->next_cb_ts, &delay);

  // Add fake data to stream and device so its slightly less than cb_threshold.
  // Expect to wait for samples to fill the full buffer (480 - 192) frames
  // instead of using the next_cb_ts.
  AddFakeDataToStream(stream.get(), 192);
  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  timespec dev_time = SingleInputDevNextWake(4096, 0, &start, &fmt, streams);
  struct timespec delta;
  subtract_timespecs(&dev_time, &start, &delta);
  // 288 frames worth of time = 6 ms.
  EXPECT_EQ(6 * 1000 * 1000, delta.tv_nsec);
}

// One hotword stream attaches to hotword device. Input data burst to a number
// larger than cb_threshold. Also, stream is pending client reply.
// In this case stream fd is used to poll for next wake.
// And the dev wake time is unchanged from the default 20 seconds limit.
TEST_F(TimingSuite, HotwordStreamBulkDataIsPending) {
  int poll_fd = 0;
  cras_audio_format fmt;
  fill_audio_format(&fmt, 48000);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 240, &fmt);
  stream->rstream->flags = HOTWORD_STREAM;
  stream->rstream->next_cb_ts = start;

  AddFakeDataToStream(stream.get(), 480);
  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  // Stream is pending the reply from client.
  rstream_stub_pending_reply(streams[0]->rstream.get(), 1);

  // There is more than 1 cb_threshold of data in device.
  timespec dev_time = SingleInputDevNextWake(4096, 7000, &start, &fmt, streams,
                                             CRAS_NODE_TYPE_HOTWORD);

  // Need to wait for stream fd in the next ppoll.
  poll_fd = dev_stream_poll_stream_fd(streams[0]->dstream.get());
  EXPECT_EQ(FAKE_POLL_FD, poll_fd);

  struct timespec delta;
  subtract_timespecs(&dev_time, &start, &delta);
  // Wake up time should be default 20 seconds because audio thread
  // depends on reply from client to wake it up.
  EXPECT_LT(19, delta.tv_sec);
  EXPECT_GT(21, delta.tv_sec);
}

// One hotword stream attaches to hotword device. Input data burst to a number
// larger than cb_threshold. However, stream is not pending client reply.
// This happens if there was no data during capture_to_stream.
// In this case stream fd is NOT used to poll for next wake.
// And the dev wake time is changed to a 0 instead of default 20 seconds.
TEST_F(TimingSuite, HotwordStreamBulkDataIsNotPending) {
  int poll_fd = 0;
  cras_audio_format fmt;
  fill_audio_format(&fmt, 48000);

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_INPUT, 240, &fmt);
  stream->rstream->flags = HOTWORD_STREAM;
  stream->rstream->next_cb_ts = start;

  AddFakeDataToStream(stream.get(), 480);
  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));
  // Stream is not pending the reply from client.
  rstream_stub_pending_reply(streams[0]->rstream.get(), 0);

  // There is more than 1 cb_threshold of data in device.
  timespec dev_time = SingleInputDevNextWake(4096, 7000, &start, &fmt, streams);

  // Does not need to wait for stream fd in the next ppoll.
  poll_fd = dev_stream_poll_stream_fd(streams[0]->dstream.get());
  EXPECT_EQ(-1, poll_fd);

  struct timespec delta;
  subtract_timespecs(&dev_time, &start, &delta);
  // Wake up time should be very small because there is enough
  // data to be send to client.
  EXPECT_LT(delta.tv_sec, 0.1);
}

// When a new output stream is added, there are two rules to determine the
// initial next_cb_ts.
// 1. If the device already has streams, the next_cb_ts will be the earliest
// next callback time from these streams.
// 2. If there are no other streams, the next_cb_ts will be set to the time
// when the valid frames in device is lower than cb_threshold. (If it is
// already lower than cb_threshold, set next_cb_ts to now.)

// Test rule 1.
// The device already has streams, the next_cb_ts will be the earliest
// next_cb_ts from these streams.
TEST_F(TimingSuite, NewOutputStreamInitStreamInDevice) {
  struct open_dev* dev_list_ = NULL;

  cras_audio_format format;
  fill_audio_format(&format, 48000);
  DevicePtr dev = create_device(CRAS_STREAM_OUTPUT, 1024, &format,
                                CRAS_NODE_TYPE_HEADPHONE);
  DL_APPEND(dev_list_, dev->odev.get());
  struct cras_iodev* iodev = dev->odev->dev;

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_OUTPUT, 480, &format);
  add_stream_to_dev(dev->dev, stream);
  stream->rstream->next_cb_ts.tv_sec = 54321;
  stream->rstream->next_cb_ts.tv_nsec = 12345;

  ShmPtr shm = create_shm(480);
  RstreamPtr rstream =
      create_rstream(1, CRAS_STREAM_OUTPUT, 480, &format, shm.get());

  dev_io_append_stream(&dev_list_, rstream.get(), &iodev, 1);

  EXPECT_EQ(stream->rstream->next_cb_ts.tv_sec, rstream->next_cb_ts.tv_sec);
  EXPECT_EQ(stream->rstream->next_cb_ts.tv_nsec, rstream->next_cb_ts.tv_nsec);

  dev_stream_destroy(iodev->streams->next);
}

// Test rule 2.
// The there are no streams and no frames in device buffer. The next_cb_ts
// will be set to now.
TEST_F(TimingSuite, NewOutputStreamInitNoStreamNoFramesInDevice) {
  struct open_dev* dev_list_ = NULL;

  cras_audio_format format;
  fill_audio_format(&format, 48000);
  DevicePtr dev = create_device(CRAS_STREAM_OUTPUT, 1024, &format,
                                CRAS_NODE_TYPE_HEADPHONE);
  DL_APPEND(dev_list_, dev->odev.get());
  struct cras_iodev* iodev = dev->odev->dev;

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  ShmPtr shm = create_shm(480);
  RstreamPtr rstream =
      create_rstream(1, CRAS_STREAM_OUTPUT, 480, &format, shm.get());

  dev_io_append_stream(&dev_list_, rstream.get(), &iodev, 1);

  EXPECT_EQ(start.tv_sec, rstream->next_cb_ts.tv_sec);
  EXPECT_EQ(start.tv_nsec, rstream->next_cb_ts.tv_nsec);

  dev_stream_destroy(iodev->streams);
}

// Test rule 2.
// The there are no streams and some valid frames in device buffer. The
// next_cb_ts will be set to the time that valid frames in device is lower
// than cb_threshold.
TEST_F(TimingSuite, NewOutputStreamInitNoStreamSomeFramesInDevice) {
  struct open_dev* dev_list_ = NULL;

  cras_audio_format format;
  fill_audio_format(&format, 48000);
  DevicePtr dev = create_device(CRAS_STREAM_OUTPUT, 1024, &format,
                                CRAS_NODE_TYPE_HEADPHONE);
  DL_APPEND(dev_list_, dev->odev.get());
  struct cras_iodev* iodev = dev->odev->dev;

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  iodev_stub_valid_frames(iodev, 960, start);

  ShmPtr shm = create_shm(480);
  RstreamPtr rstream =
      create_rstream(1, CRAS_STREAM_OUTPUT, 480, &format, shm.get());

  dev_io_append_stream(&dev_list_, rstream.get(), &iodev, 1);

  // The next_cb_ts should be 10ms from now. At that time there are
  // only 480 valid frames in the device.
  const timespec add_millis = {0, 10 * 1000 * 1000};
  add_timespecs(&start, &add_millis);
  EXPECT_EQ(start.tv_sec, rstream->next_cb_ts.tv_sec);
  EXPECT_EQ(start.tv_nsec, rstream->next_cb_ts.tv_nsec);

  dev_stream_destroy(iodev->streams);
}

// There is the pseudo code about wake up time for a output device.
//
// function dev_io_next_output_wake(dev):
//   wake_ts = get_next_stream_wake_from_list(dev.streams)
//   if cras_iodev_odev_should_wake(dev):
//     wake_ts = MIN(wake_ts, dev.wake_ts) # rule_1
//
// function get_next_stream_wake_from_list(streams):
//   for stream in streams:
//     if stream is draining: # rule_2
//     	 continue
//     if stream is pending reply: # rule_3
//       continue
//     if stream is USE_DEV_TIMING: # rule_4
//       continue
//     min_ts = MIN(min_ts, stream.next_cb_ts) # rule_5
//   return min_ts
//
// # This function is in iodev so we don't test its logic here.
// function cras_iodev_odev_should_wake(dev):
//   if dev.is_free_running:
//     return False
//   if dev.state == NORMAL_RUN or dev.state == NO_STREAM_RUN:
//     return True
//   return False

// Test rule_1.
// The wake up time should be the earlier time amoung streams and devices.
TEST_F(TimingSuite, OutputWakeTimeOneStreamWithEarlierStreamWakeTime) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_OUTPUT, 480, &format);
  stream->rstream->next_cb_ts = start;
  stream->rstream->next_cb_ts.tv_sec += 1;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));

  struct timespec dev_wake_ts = start;
  dev_wake_ts.tv_sec += 2;

  timespec dev_time =
      SingleOutputDevNextWake(48000, 0, &start, &format, streams, &dev_wake_ts);

  EXPECT_EQ(start.tv_sec + 1, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_1.
// The wake up time should be the earlier time amoung streams and devices.
TEST_F(TimingSuite, OutputWakeTimeOneStreamWithEarlierDeviceWakeTime) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_OUTPUT, 480, &format);
  stream->rstream->next_cb_ts = start;
  stream->rstream->next_cb_ts.tv_sec += 2;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream));

  struct timespec dev_wake_ts = start;
  dev_wake_ts.tv_sec += 1;

  timespec dev_time =
      SingleOutputDevNextWake(48000, 0, &start, &format, streams, &dev_wake_ts);

  EXPECT_EQ(start.tv_sec + 1, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_2.
// The stream 1 is draining so it will be ignored. The wake up time should be
// the next_cb_ts of stream 2.
TEST_F(TimingSuite, OutputWakeTimeTwoStreamsWithOneIsDraining) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream1 = create_stream(1, 1, CRAS_STREAM_OUTPUT, 480, &format);
  stream1->rstream->next_cb_ts = start;
  stream1->rstream->next_cb_ts.tv_sec += 2;
  stream1->rstream->is_draining = 1;
  stream1->rstream->queued_frames = 480;

  StreamPtr stream2 = create_stream(1, 2, CRAS_STREAM_OUTPUT, 480, &format);
  stream2->rstream->next_cb_ts = start;
  stream2->rstream->next_cb_ts.tv_sec += 5;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream1));
  streams.emplace_back(std::move(stream2));

  struct timespec dev_wake_ts = start;
  dev_wake_ts.tv_sec += 10;

  timespec dev_time =
      SingleOutputDevNextWake(48000, 0, &start, &format, streams, &dev_wake_ts);

  EXPECT_EQ(start.tv_sec + 5, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_3.
// The stream 1 is pending reply so it will be ignored. The wake up time should
// be the next_cb_ts of stream 2.
TEST_F(TimingSuite, OutputWakeTimeTwoStreamsWithOneIsPendingReply) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream1 = create_stream(1, 1, CRAS_STREAM_OUTPUT, 480, &format);
  stream1->rstream->next_cb_ts = start;
  stream1->rstream->next_cb_ts.tv_sec += 2;
  rstream_stub_pending_reply(stream1->rstream.get(), 1);

  StreamPtr stream2 = create_stream(1, 2, CRAS_STREAM_OUTPUT, 480, &format);
  stream2->rstream->next_cb_ts = start;
  stream2->rstream->next_cb_ts.tv_sec += 5;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream1));
  streams.emplace_back(std::move(stream2));

  struct timespec dev_wake_ts = start;
  dev_wake_ts.tv_sec += 10;

  timespec dev_time =
      SingleOutputDevNextWake(48000, 0, &start, &format, streams, &dev_wake_ts);

  EXPECT_EQ(start.tv_sec + 5, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_4.
// The stream 1 is uning device timing so it will be ignored. The wake up time
// should be the next_cb_ts of stream 2.
TEST_F(TimingSuite, OutputWakeTimeTwoStreamsWithOneIsUsingDevTiming) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream1 = create_stream(1, 1, CRAS_STREAM_OUTPUT, 480, &format);
  stream1->rstream->next_cb_ts = start;
  stream1->rstream->next_cb_ts.tv_sec += 2;
  stream1->rstream->flags = USE_DEV_TIMING;

  StreamPtr stream2 = create_stream(1, 2, CRAS_STREAM_OUTPUT, 480, &format);
  stream2->rstream->next_cb_ts = start;
  stream2->rstream->next_cb_ts.tv_sec += 5;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream1));
  streams.emplace_back(std::move(stream2));

  struct timespec dev_wake_ts = start;
  dev_wake_ts.tv_sec += 10;

  timespec dev_time =
      SingleOutputDevNextWake(48000, 0, &start, &format, streams, &dev_wake_ts);

  EXPECT_EQ(start.tv_sec + 5, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// Test rule_5.
// The wake up time should be the next_cb_ts of streams.
TEST_F(TimingSuite, OutputWakeTimeTwoStreams) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  StreamPtr stream1 = create_stream(1, 1, CRAS_STREAM_OUTPUT, 480, &format);
  stream1->rstream->next_cb_ts = start;
  stream1->rstream->next_cb_ts.tv_sec += 2;

  StreamPtr stream2 = create_stream(1, 2, CRAS_STREAM_OUTPUT, 480, &format);
  stream2->rstream->next_cb_ts = start;
  stream2->rstream->next_cb_ts.tv_sec += 5;

  std::vector<StreamPtr> streams;
  streams.emplace_back(std::move(stream1));
  streams.emplace_back(std::move(stream2));

  struct timespec dev_wake_ts = start;
  dev_wake_ts.tv_sec += 10;

  timespec dev_time =
      SingleOutputDevNextWake(48000, 0, &start, &format, streams, &dev_wake_ts);

  EXPECT_EQ(start.tv_sec + 2, dev_time.tv_sec);
  EXPECT_EQ(start.tv_nsec, dev_time.tv_nsec);
}

// One device, one stream, fetch stream and check the sleep time is one more
// wakeup interval.
TEST_F(TimingSuite, OutputStreamsUpdateAfterFetching) {
  cras_audio_format format;
  fill_audio_format(&format, 48000);

  StreamPtr stream = create_stream(1, 1, CRAS_STREAM_OUTPUT, 480, &format);

  // rstream's next callback is now.
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  stream->rstream->next_cb_ts = start;

  struct open_dev* dev_list_ = NULL;

  DevicePtr dev = create_device(CRAS_STREAM_OUTPUT, 1024, &format,
                                CRAS_NODE_TYPE_HEADPHONE);
  DL_APPEND(dev_list_, dev->odev.get());

  add_stream_to_dev(dev->dev, stream);

  dev_io_playback_fetch(dev_list_);

  // The next callback should be scheduled 10ms in the future.
  const timespec ten_millis = {0, 10 * 1000 * 1000};
  add_timespecs(&start, &ten_millis);
  EXPECT_EQ(start.tv_sec, stream->rstream->next_cb_ts.tv_sec);
  EXPECT_EQ(start.tv_nsec, stream->rstream->next_cb_ts.tv_nsec);
}

// TODO(yuhsuan): There are some time scheduling rules in cras_iodev. Maybe we
// can move them into dev_io so that all timing related codes are in the same
// file or leave them in iodev_unittest like now.
// 1. Device's wake_ts update: cras_iodev_frames_to_play_in_sleep.
// 2. wake_ts update when removing stream: cras_iodev_rm_stream.

/* Stubs */
extern "C" {

int input_data_get_for_stream(struct input_data* data,
                              struct cras_rstream* stream,
                              struct buffer_share* offsets,
                              struct cras_audio_area** area,
                              unsigned int* offset) {
  return 0;
}

int input_data_put_for_stream(struct input_data* data,
                              struct cras_rstream* stream,
                              struct buffer_share* offsets,
                              unsigned int frames) {
  return 0;
}

struct cras_audio_format* cras_rstream_post_processing_format(
    const struct cras_rstream* stream,
    void* dev_ptr) {
  return NULL;
}

int cras_audio_thread_event_drop_samples() {
  return 0;
}

}  // extern "C"

}  //  namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  openlog(NULL, LOG_PERROR, LOG_USER);
  return RUN_ALL_TESTS();
}
