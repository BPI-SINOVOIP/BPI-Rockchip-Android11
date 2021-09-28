// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "cras_shm.h"
#include "cras_types.h"
}

namespace {

class ShmTestSuite : public testing::Test {
 protected:
  virtual void SetUp() {
    memset(&shm_, 0, sizeof(shm_));
    shm_.header =
        static_cast<cras_audio_shm_header*>(calloc(1, sizeof(*shm_.header)));
    shm_.samples = static_cast<uint8_t*>(calloc(1, 2048));
    shm_.samples_info.length = 2048;
    cras_shm_set_frame_bytes(&shm_, 4);

    cras_shm_set_used_size(&shm_, 1024);
    memcpy(&shm_.header->config, &shm_.config, sizeof(shm_.config));

    frames_ = 0;
  }

  virtual void TearDown() {
    free(shm_.header);
    free(shm_.samples);
  }

  struct cras_audio_shm shm_;
  uint8_t* buf_;
  size_t frames_;
};

// Test that and empty buffer returns 0 readable bytes.
TEST_F(ShmTestSuite, NoneReadableWhenEmpty) {
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(0, frames_);
  cras_shm_buffer_read(&shm_, frames_);
  EXPECT_EQ(0, shm_.header->read_offset[0]);
}

// Buffer with 100 frames filled.
TEST_F(ShmTestSuite, OneHundredFilled) {
  shm_.header->write_offset[0] = 100 * shm_.header->config.frame_bytes;
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(100, frames_);
  EXPECT_EQ(shm_.samples, buf_);
  cras_shm_buffer_read(&shm_, frames_ - 9);
  EXPECT_EQ((frames_ - 9) * shm_.config.frame_bytes,
            shm_.header->read_offset[0]);
  cras_shm_buffer_read(&shm_, 9);
  EXPECT_EQ(0, shm_.header->read_offset[0]);
  EXPECT_EQ(1, shm_.header->read_buf_idx);
}

// Buffer with 100 frames filled, 50 read.
TEST_F(ShmTestSuite, OneHundredFilled50Read) {
  shm_.header->write_offset[0] = 100 * shm_.config.frame_bytes;
  shm_.header->read_offset[0] = 50 * shm_.config.frame_bytes;
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(50, frames_);
  EXPECT_EQ((shm_.samples + shm_.header->read_offset[0]), buf_);
  cras_shm_buffer_read(&shm_, frames_ - 10);
  EXPECT_EQ(shm_.header->write_offset[0] - 10 * shm_.config.frame_bytes,
            shm_.header->read_offset[0]);
  cras_shm_buffer_read(&shm_, 10);
  EXPECT_EQ(0, shm_.header->read_offset[0]);
}

// Buffer with 100 frames filled, 50 read, offset by 25.
TEST_F(ShmTestSuite, OneHundredFilled50Read25offset) {
  shm_.header->write_offset[0] = 100 * shm_.config.frame_bytes;
  shm_.header->read_offset[0] = 50 * shm_.config.frame_bytes;
  buf_ = cras_shm_get_readable_frames(&shm_, 25, &frames_);
  EXPECT_EQ(25, frames_);
  EXPECT_EQ(shm_.samples + shm_.header->read_offset[0] +
                25 * shm_.header->config.frame_bytes,
            (uint8_t*)buf_);
}

// Test wrapping across buffers.
TEST_F(ShmTestSuite, WrapToNextBuffer) {
  uint32_t used_size = cras_shm_used_size(&shm_);
  uint32_t used_frames = used_size / cras_shm_frame_bytes(&shm_);
  shm_.header->write_offset[0] = (used_size / 2);
  shm_.header->read_offset[0] = (used_size / 4);
  shm_.header->write_offset[1] = (used_size / 2);
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(used_frames / 4, frames_);
  EXPECT_EQ(cras_shm_buff_for_idx(&shm_, 0) + (used_size / 4), (uint8_t*)buf_);
  buf_ = cras_shm_get_readable_frames(&shm_, frames_, &frames_);
  EXPECT_EQ(used_frames / 2, frames_);
  EXPECT_EQ(cras_shm_buff_for_idx(&shm_, 1), (uint8_t*)buf_);
  /* Mark all but 10 frames as read */
  cras_shm_buffer_read(&shm_, (used_frames / 2) + (used_frames / 4) - 10);
  EXPECT_EQ(0, shm_.header->read_offset[0]);
  EXPECT_EQ(((used_frames / 2) - 10) * shm_.config.frame_bytes,
            shm_.header->read_offset[1]);
  EXPECT_EQ(1, shm_.header->read_buf_idx);
}

// Test wrapping across buffers reading all samples.
TEST_F(ShmTestSuite, WrapToNextBufferReadAll) {
  uint32_t used_size = cras_shm_used_size(&shm_);
  uint32_t used_frames = used_size / cras_shm_frame_bytes(&shm_);
  shm_.header->write_offset[0] = (used_size / 2);
  shm_.header->read_offset[0] = (used_size / 4);
  shm_.header->write_offset[1] = (used_size / 2);
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(used_frames / 4, frames_);
  EXPECT_EQ(cras_shm_buff_for_idx(&shm_, 0) + (used_size / 4), (uint8_t*)buf_);
  buf_ = cras_shm_get_readable_frames(&shm_, frames_, &frames_);
  EXPECT_EQ(used_frames / 2, frames_);
  EXPECT_EQ(cras_shm_buff_for_idx(&shm_, 1), (uint8_t*)buf_);
  /* Mark all frames as read */
  cras_shm_buffer_read(&shm_, (used_frames / 2) + (used_frames / 4));
  EXPECT_EQ(0, shm_.header->read_offset[0]);
  EXPECT_EQ(0, shm_.header->read_offset[1]);
  EXPECT_EQ(0, shm_.header->read_buf_idx);
}

// Test wrapping last buffer.
TEST_F(ShmTestSuite, WrapFromFinalBuffer) {
  uint32_t used_size = cras_shm_used_size(&shm_);
  uint32_t used_frames = used_size / cras_shm_frame_bytes(&shm_);
  uint32_t buf_idx = CRAS_NUM_SHM_BUFFERS - 1;

  shm_.header->read_buf_idx = buf_idx;
  shm_.header->write_offset[buf_idx] = (used_size / 2);
  shm_.header->read_offset[buf_idx] = (used_size / 4);
  shm_.header->write_offset[0] = (used_size / 2);
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(used_frames / 4, frames_);
  EXPECT_EQ(cras_shm_buff_for_idx(&shm_, buf_idx) + (used_size / 4),
            (uint8_t*)buf_);

  buf_ = cras_shm_get_readable_frames(&shm_, frames_, &frames_);
  EXPECT_EQ(used_frames / 2, frames_);
  EXPECT_EQ(cras_shm_buff_for_idx(&shm_, 0), (uint8_t*)buf_);
  /* Mark all but 10 frames as read */
  cras_shm_buffer_read(&shm_, (used_frames / 2) + (used_frames / 4) - 10);
  EXPECT_EQ(0, shm_.header->read_offset[buf_idx]);
  EXPECT_EQ(((used_frames / 2) - 10) * shm_.config.frame_bytes,
            shm_.header->read_offset[0]);
  EXPECT_EQ(0, shm_.header->read_buf_idx);
}

// Test Check available to write returns 0 if not free buffer.
TEST_F(ShmTestSuite, WriteAvailNotFree) {
  size_t ret;
  shm_.header->write_buf_idx = 0;
  shm_.header->write_offset[0] = 100 * shm_.config.frame_bytes;
  shm_.header->read_offset[0] = 50 * shm_.config.frame_bytes;
  ret = cras_shm_get_num_writeable(&shm_);
  EXPECT_EQ(0, ret);
}

// Test Check available to write returns num_frames if free buffer.
TEST_F(ShmTestSuite, WriteAvailValid) {
  size_t ret;
  shm_.header->write_buf_idx = 0;
  shm_.config.used_size = 480 * shm_.config.frame_bytes;
  shm_.header->write_offset[0] = 0;
  shm_.header->read_offset[0] = 0;
  ret = cras_shm_get_num_writeable(&shm_);
  EXPECT_EQ(480, ret);
}

// Test get frames_written returns the number of frames written.
TEST_F(ShmTestSuite, GetNumWritten) {
  size_t ret;
  shm_.header->write_buf_idx = 0;
  shm_.config.used_size = 480 * shm_.config.frame_bytes;
  shm_.header->write_offset[0] = 200 * shm_.config.frame_bytes;
  shm_.header->read_offset[0] = 0;
  ret = cras_shm_frames_written(&shm_);
  EXPECT_EQ(200, ret);
}

// Test that getting the base of the write buffer returns the correct pointer.
TEST_F(ShmTestSuite, GetWriteBufferBase) {
  uint8_t* ret;

  shm_.header->write_buf_idx = 0;
  shm_.header->write_offset[0] = 128 * shm_.config.frame_bytes;
  shm_.header->write_offset[1] = 128 * shm_.config.frame_bytes;
  shm_.header->read_offset[0] = 0;
  shm_.header->read_offset[1] = 0;
  ret = cras_shm_get_write_buffer_base(&shm_);
  EXPECT_EQ(shm_.samples, ret);

  shm_.header->write_buf_idx = 1;
  ret = cras_shm_get_write_buffer_base(&shm_);
  EXPECT_EQ(shm_.samples + shm_.config.used_size, ret);
}

TEST_F(ShmTestSuite, SetVolume) {
  cras_shm_set_volume_scaler(&shm_, 1.0);
  EXPECT_EQ(shm_.header->volume_scaler, 1.0);
  cras_shm_set_volume_scaler(&shm_, 1.4);
  EXPECT_EQ(shm_.header->volume_scaler, 1.0);
  cras_shm_set_volume_scaler(&shm_, -0.5);
  EXPECT_EQ(shm_.header->volume_scaler, 0.0);
  cras_shm_set_volume_scaler(&shm_, 0.5);
  EXPECT_EQ(shm_.header->volume_scaler, 0.5);
}

// Test that invalid read/write offsets are detected.

TEST_F(ShmTestSuite, InvalidWriteOffset) {
  shm_.header->write_offset[0] = shm_.config.used_size + 50;
  shm_.header->read_offset[0] = 0;
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(shm_.config.used_size / 4, frames_);
}

TEST_F(ShmTestSuite, InvalidReadOffset) {
  // Should ignore read+_offset and assume 0.
  shm_.header->write_offset[0] = 44;
  shm_.header->read_offset[0] = shm_.config.used_size + 25;
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(shm_.header->write_offset[0] / shm_.config.frame_bytes, frames_);
  EXPECT_EQ(shm_.samples, (uint8_t*)buf_);
}

TEST_F(ShmTestSuite, InvalidReadAndWriteOffset) {
  shm_.header->write_offset[0] = shm_.config.used_size + 50;
  shm_.header->read_offset[0] = shm_.config.used_size + 25;
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(shm_.config.used_size / 4, frames_);
}

TEST_F(ShmTestSuite, InputBufferOverrun) {
  int rc;
  shm_.header->write_offset[0] = 0;
  shm_.header->read_offset[0] = 0;
  shm_.header->write_offset[1] = 0;
  shm_.header->read_offset[1] = 0;

  shm_.header->write_buf_idx = 0;
  shm_.header->read_buf_idx = 0;

  EXPECT_EQ(0, cras_shm_num_overruns(&shm_));
  rc = cras_shm_check_write_overrun(&shm_);
  EXPECT_EQ(0, rc);
  cras_shm_buffer_written(&shm_, 100);
  cras_shm_buffer_write_complete(&shm_);

  rc = cras_shm_check_write_overrun(&shm_);
  EXPECT_EQ(0, rc);
  cras_shm_buffer_written(&shm_, 100);
  cras_shm_buffer_write_complete(&shm_);

  // Assert two consecutive writes causes overrun.
  rc = cras_shm_check_write_overrun(&shm_);
  EXPECT_EQ(1, rc);
  EXPECT_EQ(1, cras_shm_num_overruns(&shm_));
}

TEST_F(ShmTestSuite, GetWritableFramesNeedToWrite) {
  unsigned buffer_size = 480;
  unsigned limit = 480;
  unsigned written = 200;
  unsigned frames;
  shm_.header->write_buf_idx = 0;
  shm_.config.used_size = buffer_size * shm_.config.frame_bytes;
  shm_.header->write_offset[0] = written * shm_.config.frame_bytes;
  buf_ = cras_shm_get_writeable_frames(&shm_, limit, &frames);
  EXPECT_EQ(limit - written, frames);
  EXPECT_EQ(shm_.samples + shm_.header->write_offset[0], buf_);
}

TEST_F(ShmTestSuite, GetWritableFramesNoNeedToWrite) {
  unsigned buffer_size = 480;
  unsigned limit = 240;
  unsigned written = 300;
  unsigned frames;
  shm_.header->write_buf_idx = 0;
  shm_.config.used_size = buffer_size * shm_.config.frame_bytes;
  shm_.header->write_offset[0] = written * shm_.config.frame_bytes;
  buf_ = cras_shm_get_writeable_frames(&shm_, limit, &frames);
  EXPECT_EQ(0, frames);
  EXPECT_EQ(shm_.samples + shm_.header->write_offset[0], buf_);
}

// Test wrapping of buffers that don't start at normal offsets.
TEST_F(ShmTestSuite, WrapWithNonstandardBufferLocations) {
  uint32_t frame_bytes = cras_shm_frame_bytes(&shm_);
  uint32_t used_frames = 24;
  uint32_t used_size = used_frames * frame_bytes;
  cras_shm_set_used_size(&shm_, used_size);
  cras_shm_set_buffer_offset(&shm_, 0, 15);
  cras_shm_set_buffer_offset(&shm_, 1, 479);

  shm_.header->read_offset[0] = (used_frames / 4) * shm_.config.frame_bytes;
  shm_.header->write_offset[0] = (used_frames / 2) * shm_.config.frame_bytes;
  shm_.header->read_offset[1] = 0;
  shm_.header->write_offset[1] = (used_frames / 3) * shm_.config.frame_bytes;
  buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_);
  EXPECT_EQ(used_frames / 4, frames_);
  EXPECT_EQ(cras_shm_buff_for_idx(&shm_, 0) + shm_.header->read_offset[0],
            (uint8_t*)buf_);
  buf_ = cras_shm_get_readable_frames(&shm_, frames_, &frames_);
  EXPECT_EQ(used_frames / 3, frames_);
  EXPECT_EQ(cras_shm_buff_for_idx(&shm_, 1), (uint8_t*)buf_);
  /* Mark all but 5 frames as read */
  cras_shm_buffer_read(&shm_, (used_frames / 4) + (used_frames / 3) - 5);
  EXPECT_EQ(0, shm_.header->read_offset[0]);
  EXPECT_EQ(((used_frames / 3) - 5) * shm_.config.frame_bytes,
            shm_.header->read_offset[1]);
}

TEST_F(ShmTestSuite, PlaybackWithDifferentSequentialBufferLocations) {
  uint32_t frame_bytes = cras_shm_frame_bytes(&shm_);
  uint32_t used_frames = 24;
  uint32_t used_size = used_frames * frame_bytes;
  cras_shm_set_used_size(&shm_, used_size);

  uint32_t first_offset = 2.7 * used_size;
  /* Make samples area long enough to hold all of the buffers starting from
   * first_offset, with an extra 'used_size' bytes of free space at the end. */
  uint32_t samples_length =
      first_offset + used_size * (CRAS_NUM_SHM_BUFFERS + 1);
  free(shm_.samples);
  shm_.samples = static_cast<uint8_t*>(calloc(1, samples_length));
  shm_.samples_info.length = samples_length;
  uint32_t total_frames_written = 0;

  // Fill all of the buffers.
  for (unsigned int i = 0; i < CRAS_NUM_SHM_BUFFERS; i++) {
    cras_shm_set_buffer_offset(&shm_, i, first_offset + i * used_size);
    shm_.header->write_in_progress[i] = 1;
    shm_.header->write_offset[i] = 0;
    frames_ = MIN(10 + i, used_frames);
    cras_shm_buffer_written(&shm_, frames_);
    total_frames_written += frames_;
    cras_shm_buffer_write_complete(&shm_);
  }

  uint32_t total_frames_available = 0;

  // Consume all available frames.
  while ((buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_))) {
    total_frames_available += frames_;

    EXPECT_GE(buf_, shm_.samples);
    EXPECT_LE(buf_ + frames_, shm_.samples + samples_length);
    cras_shm_buffer_read(&shm_, frames_);
  }

  EXPECT_EQ(total_frames_written, total_frames_available);

  uint32_t second_offset = 1.2 * used_size;

  // Fill half of the buffers.
  for (unsigned int i = 0; i < CRAS_NUM_SHM_BUFFERS / 2; i++) {
    cras_shm_set_buffer_offset(&shm_, i, second_offset + i * used_size);
    shm_.header->write_in_progress[i] = 1;
    shm_.header->write_offset[i] = 0;
    frames_ = MIN(3 + 2 * i, used_frames);
    cras_shm_buffer_written(&shm_, frames_);
    total_frames_written += frames_;
    cras_shm_buffer_write_complete(&shm_);
  }

  // Consume all available frames.
  while ((buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_))) {
    total_frames_available += frames_;

    EXPECT_GE(buf_, shm_.samples);
    EXPECT_LE(buf_ + frames_, shm_.samples + samples_length);
    cras_shm_buffer_read(&shm_, frames_);
  }

  EXPECT_EQ(total_frames_written, total_frames_available);

  // Fill rest of the buffers.
  for (unsigned int i = CRAS_NUM_SHM_BUFFERS / 2; i < CRAS_NUM_SHM_BUFFERS;
       i++) {
    cras_shm_set_buffer_offset(&shm_, i, second_offset + i * used_size);
    shm_.header->write_in_progress[i] = 1;
    shm_.header->write_offset[i] = 0;
    frames_ = MIN(3 + 2 * i, used_frames);
    cras_shm_buffer_written(&shm_, frames_);
    total_frames_written += frames_;
    cras_shm_buffer_write_complete(&shm_);
  }

  // Consume all available frames.
  while ((buf_ = cras_shm_get_readable_frames(&shm_, 0, &frames_))) {
    total_frames_available += frames_;

    EXPECT_GE(buf_, shm_.samples);
    EXPECT_LE(buf_ + frames_, shm_.samples + samples_length);
    cras_shm_buffer_read(&shm_, frames_);
  }

  EXPECT_EQ(total_frames_written, total_frames_available);
}

TEST_F(ShmTestSuite, GetCheckedBufferOffset) {
  uint32_t used_size = cras_shm_used_size(&shm_);
  uint32_t samples_length = used_size * 8;
  shm_.samples_info.length = samples_length;

  uint32_t offset;

  for (unsigned int i = 0; i < CRAS_NUM_SHM_BUFFERS; i++) {
    shm_.header->buffer_offset[i] = 0;
    offset = cras_shm_get_checked_buffer_offset(&shm_, i);
    EXPECT_EQ(0, offset) << "Expected valid buffer offset for buffer " << i;

    shm_.header->buffer_offset[i] = used_size;
    offset = cras_shm_get_checked_buffer_offset(&shm_, i);
    EXPECT_EQ(used_size, offset)
        << "Expected valid buffer offset for buffer " << i;

    shm_.header->buffer_offset[i] = samples_length - 1;
    offset = cras_shm_get_checked_buffer_offset(&shm_, i);
    EXPECT_EQ(samples_length - 1, offset)
        << "Expected valid buffer offset for buffer " << i;

    shm_.header->buffer_offset[i] = samples_length;
    offset = cras_shm_get_checked_buffer_offset(&shm_, i);
    EXPECT_EQ(samples_length, offset)
        << "Expected valid buffer offset for buffer " << i;

    shm_.header->buffer_offset[i] = samples_length + 1;
    offset = cras_shm_get_checked_buffer_offset(&shm_, i);
    EXPECT_EQ(samples_length, offset)
        << "Expected invalid buffer offset for buffer " << i;

    shm_.header->buffer_offset[i] = samples_length + used_size;
    offset = cras_shm_get_checked_buffer_offset(&shm_, i);
    EXPECT_EQ(samples_length, offset)
        << "Expected invalid buffer offset for buffer " << i;
  }
}

TEST_F(ShmTestSuite, GetCheckedReadOffset) {
  uint32_t used_size = cras_shm_used_size(&shm_);
  uint32_t samples_length = used_size * 8;
  shm_.samples_info.length = samples_length;

  uint32_t offset;

  for (unsigned int i = 0; i < CRAS_NUM_SHM_BUFFERS; i++) {
    shm_.header->read_offset[i] = 0;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(0, offset) << "Expected valid read offset for buffer " << i;

    shm_.header->read_offset[i] = used_size / 2;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(used_size / 2, offset)
        << "Expected valid read offset for buffer " << i;

    shm_.header->read_offset[i] = used_size;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(used_size, offset)
        << "Expected valid read offset for buffer " << i;

    // Read offsets should not be greater than used_size.
    shm_.header->read_offset[i] = used_size + 1;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(0, offset) << "Expected invalid read offset for buffer " << i;

    shm_.header->buffer_offset[i] = samples_length - used_size / 2;

    shm_.header->read_offset[i] = 0;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(0, offset) << "Expected valid read offset for buffer " << i;

    shm_.header->read_offset[i] = used_size / 4;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(used_size / 4, offset)
        << "Expected valid read offset for buffer " << i;

    shm_.header->read_offset[i] = used_size / 2;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(used_size / 2, offset)
        << "Expected valid read offset for buffer " << i;

    shm_.header->read_offset[i] = used_size / 2 + 1;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(0, offset) << "Expected invalid read offset for buffer " << i;

    shm_.header->read_offset[i] = used_size;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(0, offset) << "Expected invalid read offset for buffer " << i;

    shm_.header->read_offset[i] = used_size + 1;
    offset = cras_shm_get_checked_read_offset(&shm_, i);
    EXPECT_EQ(0, offset) << "Expected invalid read offset for buffer " << i;
  }
}

TEST_F(ShmTestSuite, GetCheckedWriteOffset) {
  uint32_t used_size = cras_shm_used_size(&shm_);
  uint32_t samples_length = used_size * 8;
  shm_.samples_info.length = samples_length;

  uint32_t offset;

  for (unsigned int i = 0; i < CRAS_NUM_SHM_BUFFERS; i++) {
    shm_.header->write_offset[i] = 0;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(0, offset) << "Expected valid write offset for buffer " << i;

    shm_.header->write_offset[i] = used_size / 2;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(used_size / 2, offset)
        << "Expected valid write offset for buffer " << i;

    shm_.header->write_offset[i] = used_size;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(used_size, offset)
        << "Expected valid write offset for buffer " << i;

    // Write offsets should not be greater than used_size.
    shm_.header->write_offset[i] = used_size + 1;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(used_size, offset)
        << "Expected invalid write offset for buffer " << i;

    uint32_t buffer_offset = samples_length - used_size / 2;
    shm_.header->buffer_offset[i] = buffer_offset;

    shm_.header->write_offset[i] = 0;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(0, offset) << "Expected valid write offset for buffer " << i;

    shm_.header->write_offset[i] = used_size / 4;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(used_size / 4, offset)
        << "Expected valid write offset for buffer " << i;

    shm_.header->write_offset[i] = used_size / 2;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(used_size / 2, offset)
        << "Expected valid write offset for buffer " << i;

    // Write offsets should not be longer than the samples area.
    shm_.header->write_offset[i] = used_size / 2 + 1;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(samples_length - buffer_offset, offset)
        << "Expected invalid write offset for buffer " << i;

    shm_.header->write_offset[i] = used_size;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(samples_length - buffer_offset, offset)
        << "Expected invalid write offset for buffer " << i;

    shm_.header->write_offset[i] = used_size + 1;
    offset = cras_shm_get_checked_write_offset(&shm_, i);
    EXPECT_EQ(samples_length - buffer_offset, offset)
        << "Expected invalid write offset for buffer " << i;
  }
}

}  //  namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
