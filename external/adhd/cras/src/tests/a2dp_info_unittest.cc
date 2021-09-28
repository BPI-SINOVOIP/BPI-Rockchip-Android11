// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#include <sbc/sbc.h>

#include "cras_a2dp_info.h"
#include "cras_sbc_codec.h"
#include "sbc_codec_stub.h"
}

static size_t a2dp_write_link_mtu_val;
static struct a2dp_info a2dp;
static a2dp_sbc_t sbc;

void ResetStubData() {
  sbc_codec_stub_reset();

  a2dp_write_link_mtu_val = 40;

  sbc.frequency = SBC_SAMPLING_FREQ_48000;
  sbc.channel_mode = SBC_CHANNEL_MODE_JOINT_STEREO;
  sbc.allocation_method = SBC_ALLOCATION_LOUDNESS;
  sbc.subbands = SBC_SUBBANDS_8;
  sbc.block_length = SBC_BLOCK_LENGTH_16;
  sbc.max_bitpool = 50;

  a2dp.a2dp_buf_used = 0;
  a2dp.frame_count = 0;
  a2dp.samples = 0;
  a2dp.nsamples = 0;
}

namespace {

TEST(A2dpInfoInit, InitA2dp) {
  ResetStubData();
  init_a2dp(&a2dp, &sbc);

  ASSERT_EQ(1, get_sbc_codec_create_called());
  ASSERT_EQ(SBC_FREQ_48000, get_sbc_codec_create_freq_val());
  ASSERT_EQ(SBC_MODE_JOINT_STEREO, get_sbc_codec_create_mode_val());
  ASSERT_EQ(SBC_AM_LOUDNESS, get_sbc_codec_create_alloc_val());
  ASSERT_EQ(SBC_SB_8, get_sbc_codec_create_subbands_val());
  ASSERT_EQ(SBC_BLK_16, get_sbc_codec_create_blocks_val());
  ASSERT_EQ(50, get_sbc_codec_create_bitpool_val());

  ASSERT_NE(a2dp.codec, (void*)NULL);
  ASSERT_EQ(a2dp.a2dp_buf_used, 13);
  ASSERT_EQ(a2dp.frame_count, 0);
  ASSERT_EQ(a2dp.seq_num, 0);
  ASSERT_EQ(a2dp.samples, 0);

  destroy_a2dp(&a2dp);
}

TEST(A2dpInfoInit, InitA2dpFail) {
  ResetStubData();
  int err;
  set_sbc_codec_create_fail(1);
  err = init_a2dp(&a2dp, &sbc);

  ASSERT_EQ(1, get_sbc_codec_create_called());
  ASSERT_NE(0, err);
  ASSERT_EQ(a2dp.codec, (void*)NULL);
}

TEST(A2dpInfoInit, DestroyA2dp) {
  ResetStubData();
  init_a2dp(&a2dp, &sbc);
  destroy_a2dp(&a2dp);

  ASSERT_EQ(1, get_sbc_codec_destroy_called());
}

TEST(A2dpInfoInit, DrainA2dp) {
  ResetStubData();
  init_a2dp(&a2dp, &sbc);
  a2dp.a2dp_buf_used = 99;
  a2dp.samples = 10;
  a2dp.seq_num = 11;
  a2dp.frame_count = 12;

  a2dp_drain(&a2dp);

  ASSERT_EQ(a2dp.a2dp_buf_used, 13);
  ASSERT_EQ(a2dp.frame_count, 0);
  ASSERT_EQ(a2dp.seq_num, 0);
  ASSERT_EQ(a2dp.samples, 0);

  destroy_a2dp(&a2dp);
}

TEST(A2dpEncode, WriteA2dp) {
  unsigned int processed;

  ResetStubData();
  init_a2dp(&a2dp, &sbc);

  set_sbc_codec_encoded_out(4);
  processed = a2dp_encode(&a2dp, NULL, 20, 4, (size_t)40);

  ASSERT_EQ(20, processed);
  ASSERT_EQ(4, a2dp.frame_count);

  // 13 + 4 used a2dp buffer still below half mtu unwritten
  ASSERT_EQ(17, a2dp.a2dp_buf_used);
  ASSERT_EQ(5, a2dp.samples);
  ASSERT_EQ(5, a2dp.nsamples);
  ASSERT_EQ(0, a2dp.seq_num);

  set_sbc_codec_encoded_out(15);
  processed = a2dp_encode(&a2dp, NULL, 20, 4, (size_t)40);

  ASSERT_EQ(32, a2dp.a2dp_buf_used);
  ASSERT_EQ(10, a2dp.samples);
  ASSERT_EQ(10, a2dp.nsamples);
  ASSERT_EQ(0, a2dp.seq_num);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
