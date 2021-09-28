// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <limits.h>
#include <sys/param.h>

#include <memory>

extern "C" {
#include "cras_fmt_conv_ops.h"
#include "cras_types.h"
}

static uint8_t* AllocateRandomBytes(size_t size) {
  uint8_t* buf = (uint8_t*)malloc(size);
  while (size--)
    buf[size] = rand() & 0xff;
  return buf;
}

using U8Ptr = std::unique_ptr<uint8_t[], decltype(free)*>;
using S16LEPtr = std::unique_ptr<int16_t[], decltype(free)*>;
using S243LEPtr = std::unique_ptr<uint8_t[], decltype(free)*>;
using S24LEPtr = std::unique_ptr<int32_t[], decltype(free)*>;
using S32LEPtr = std::unique_ptr<int32_t[], decltype(free)*>;
using FloatPtr = std::unique_ptr<float[], decltype(free)*>;

static U8Ptr CreateU8(size_t size) {
  uint8_t* buf = AllocateRandomBytes(size * sizeof(uint8_t));
  U8Ptr ret(buf, free);
  return ret;
}

static S16LEPtr CreateS16LE(size_t size) {
  uint8_t* buf = AllocateRandomBytes(size * sizeof(int16_t));
  S16LEPtr ret(reinterpret_cast<int16_t*>(buf), free);
  return ret;
}

static S243LEPtr CreateS243LE(size_t size) {
  uint8_t* buf = AllocateRandomBytes(size * sizeof(uint8_t) * 3);
  S243LEPtr ret(buf, free);
  return ret;
}

static S24LEPtr CreateS24LE(size_t size) {
  uint8_t* buf = AllocateRandomBytes(size * sizeof(int32_t));
  S24LEPtr ret(reinterpret_cast<int32_t*>(buf), free);
  return ret;
}

static S32LEPtr CreateS32LE(size_t size) {
  uint8_t* buf = AllocateRandomBytes(size * sizeof(int32_t));
  S32LEPtr ret(reinterpret_cast<int32_t*>(buf), free);
  return ret;
}

static FloatPtr CreateFloat(size_t size) {
  float* buf = (float*)malloc(size * sizeof(float));
  while (size--)
    buf[size] = (float)(rand() & 0xff) / 0xfff;
  FloatPtr ret(buf, free);
  return ret;
}

static int32_t ToS243LE(const uint8_t* in) {
  int32_t ret = 0;

  ret |= in[2];
  ret <<= 8;
  ret |= in[1];
  ret <<= 8;
  ret |= in[0];
  return ret;
}

static int16_t S16AddAndClip(int16_t a, int16_t b) {
  int32_t sum;

  sum = (int32_t)a + (int32_t)b;
  sum = MAX(sum, SHRT_MIN);
  sum = MIN(sum, SHRT_MAX);
  return sum;
}

// Test U8 to S16_LE conversion.
TEST(FormatConverterOpsTest, ConvertU8ToS16LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 2;

  U8Ptr src = CreateU8(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  convert_u8_to_s16le(src.get(), frames * in_ch, (uint8_t*)dst.get());

  for (size_t i = 0; i < frames * in_ch; ++i) {
    EXPECT_EQ((int16_t)((uint16_t)((int16_t)(int8_t)src[i] - 0x80) << 8),
              dst[i]);
  }
}

// Test S24_3LE to S16_LE conversion.
TEST(FormatConverterOpsTest, ConvertS243LEToS16LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 2;

  S243LEPtr src = CreateS243LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  convert_s243le_to_s16le(src.get(), frames * in_ch, (uint8_t*)dst.get());

  uint8_t* p = src.get();
  for (size_t i = 0; i < frames * in_ch; ++i) {
    EXPECT_EQ((int16_t)(ToS243LE(p) >> 8), dst[i]);
    p += 3;
  }
}

// Test S24_LE to S16_LE conversion.
TEST(FormatConverterOpsTest, ConvertS24LEToS16LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 2;

  S24LEPtr src = CreateS24LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  convert_s24le_to_s16le((uint8_t*)src.get(), frames * in_ch,
                         (uint8_t*)dst.get());

  for (size_t i = 0; i < frames * in_ch; ++i) {
    EXPECT_EQ((int16_t)(src[i] >> 8), dst[i]);
  }
}

// Test S32_LE to S16_LE conversion.
TEST(FormatConverterOpsTest, ConvertS32LEToS16LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 2;

  S32LEPtr src = CreateS32LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  convert_s32le_to_s16le((uint8_t*)src.get(), frames * in_ch,
                         (uint8_t*)dst.get());

  for (size_t i = 0; i < frames * in_ch; ++i) {
    EXPECT_EQ((int16_t)(src[i] >> 16), dst[i]);
  }
}

// Test S16_LE to U8 conversion.
TEST(FormatConverterOpsTest, ConvertS16LEToU8) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 2;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  U8Ptr dst = CreateU8(frames * out_ch);

  convert_s16le_to_u8((uint8_t*)src.get(), frames * in_ch, dst.get());

  for (size_t i = 0; i < frames * in_ch; ++i) {
    EXPECT_EQ((uint8_t)(int8_t)((src[i] >> 8) + 0x80), dst[i]);
  }
}

// Test S16_LE to S24_3LE conversion.
TEST(FormatConverterOpsTest, ConvertS16LEToS243LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 2;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S243LEPtr dst = CreateS243LE(frames * out_ch);

  convert_s16le_to_s243le((uint8_t*)src.get(), frames * in_ch, dst.get());

  uint8_t* p = dst.get();
  for (size_t i = 0; i < frames * in_ch; ++i) {
    EXPECT_EQ((int32_t)((uint32_t)src[i] << 8) & 0x00ffffff,
              ToS243LE(p) & 0x00ffffff);
    p += 3;
  }
}

// Test S16_LE to S24_LE conversion.
TEST(FormatConverterOpsTest, ConvertS16LEToS24LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 2;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S24LEPtr dst = CreateS24LE(frames * out_ch);

  convert_s16le_to_s24le((uint8_t*)src.get(), frames * in_ch,
                         (uint8_t*)dst.get());

  for (size_t i = 0; i < frames * in_ch; ++i) {
    EXPECT_EQ((int32_t)((uint32_t)src[i] << 8) & 0x00ffffff,
              dst[i] & 0x00ffffff);
  }
}

// Test S16_LE to S32_LE conversion.
TEST(FormatConverterOpsTest, ConvertS16LEToS32LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 2;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S32LEPtr dst = CreateS32LE(frames * out_ch);

  convert_s16le_to_s32le((uint8_t*)src.get(), frames * in_ch,
                         (uint8_t*)dst.get());

  for (size_t i = 0; i < frames * in_ch; ++i) {
    EXPECT_EQ((int32_t)((uint32_t)src[i] << 16) & 0xffffff00,
              dst[i] & 0xffffff00);
  }
}

// Test Mono to Stereo conversion.  S16_LE.
TEST(FormatConverterOpsTest, MonoToStereoS16LE) {
  const size_t frames = 4096;
  const size_t in_ch = 1;
  const size_t out_ch = 2;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret =
      s16_mono_to_stereo((uint8_t*)src.get(), frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    EXPECT_EQ(src[i], dst[i * 2 + 0]);
    EXPECT_EQ(src[i], dst[i * 2 + 1]);
  }
}

// Test Stereo to Mono conversion.  S16_LE.
TEST(FormatConverterOpsTest, StereoToMonoS16LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);
  for (size_t i = 0; i < frames; ++i) {
    src[i * 2 + 0] = 13450;
    src[i * 2 + 1] = -13449;
  }

  size_t ret =
      s16_stereo_to_mono((uint8_t*)src.get(), frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    EXPECT_EQ(1, dst[i]);
  }
}

// Test Stereo to Mono conversion.  S16_LE, Overflow.
TEST(FormatConverterOpsTest, StereoToMonoS16LEOverflow) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);
  for (size_t i = 0; i < frames; ++i) {
    src[i * 2 + 0] = 0x7fff;
    src[i * 2 + 1] = 1;
  }

  size_t ret =
      s16_stereo_to_mono((uint8_t*)src.get(), frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    EXPECT_EQ(0x7fff, dst[i]);
  }
}

// Test Stereo to Mono conversion.  S16_LE, Underflow.
TEST(FormatConverterOpsTest, StereoToMonoS16LEUnderflow) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);
  for (size_t i = 0; i < frames; ++i) {
    src[i * 2 + 0] = -0x8000;
    src[i * 2 + 1] = -0x1;
  }

  size_t ret =
      s16_stereo_to_mono((uint8_t*)src.get(), frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    EXPECT_EQ(-0x8000, dst[i]);
  }
}

// Test Mono to 5.1 conversion.  S16_LE, Center.
TEST(FormatConverterOpsTest, MonoTo51S16LECenter) {
  const size_t frames = 4096;
  const size_t in_ch = 1;
  const size_t out_ch = 6;
  const size_t left = 0;
  const size_t right = 1;
  const size_t center = 4;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret = s16_mono_to_51(left, right, center, (uint8_t*)src.get(), frames,
                              (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    for (size_t k = 0; k < 6; ++k) {
      if (k == center)
        EXPECT_EQ(src[i], dst[i * 6 + k]);
      else
        EXPECT_EQ(0, dst[i * 6 + k]);
    }
  }
}

// Test Mono to 5.1 conversion.  S16_LE, LeftRight.
TEST(FormatConverterOpsTest, MonoTo51S16LELeftRight) {
  const size_t frames = 4096;
  const size_t in_ch = 1;
  const size_t out_ch = 6;
  const size_t left = 0;
  const size_t right = 1;
  const size_t center = -1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret = s16_mono_to_51(left, right, center, (uint8_t*)src.get(), frames,
                              (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    for (size_t k = 0; k < 6; ++k) {
      if (k == left)
        EXPECT_EQ(src[i] / 2, dst[i * 6 + k]);
      else if (k == right)
        EXPECT_EQ(src[i] / 2, dst[i * 6 + k]);
      else
        EXPECT_EQ(0, dst[i * 6 + k]);
    }
  }
}

// Test Mono to 5.1 conversion.  S16_LE, Unknown.
TEST(FormatConverterOpsTest, MonoTo51S16LEUnknown) {
  const size_t frames = 4096;
  const size_t in_ch = 1;
  const size_t out_ch = 6;
  const size_t left = -1;
  const size_t right = -1;
  const size_t center = -1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret = s16_mono_to_51(left, right, center, (uint8_t*)src.get(), frames,
                              (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    for (size_t k = 0; k < 6; ++k) {
      if (k == 0)
        EXPECT_EQ(src[i], dst[i * 6 + k]);
      else
        EXPECT_EQ(0, dst[6 * i + k]);
    }
  }
}

// Test Stereo to 5.1 conversion.  S16_LE, Center.
TEST(FormatConverterOpsTest, StereoTo51S16LECenter) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 6;
  const size_t left = -1;
  const size_t right = 1;
  const size_t center = 4;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret = s16_stereo_to_51(left, right, center, (uint8_t*)src.get(),
                                frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    for (size_t k = 0; k < 6; ++k) {
      if (k == center)
        EXPECT_EQ(S16AddAndClip(src[i * 2], src[i * 2 + 1]), dst[i * 6 + k]);
      else
        EXPECT_EQ(0, dst[i * 6 + k]);
    }
  }
}

// Test Stereo to 5.1 conversion.  S16_LE, LeftRight.
TEST(FormatConverterOpsTest, StereoTo51S16LELeftRight) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 6;
  const size_t left = 0;
  const size_t right = 1;
  const size_t center = -1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret = s16_stereo_to_51(left, right, center, (uint8_t*)src.get(),
                                frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    for (size_t k = 0; k < 6; ++k) {
      if (k == left)
        EXPECT_EQ(src[i * 2 + 0], dst[i * 6 + k]);
      else if (k == right)
        EXPECT_EQ(src[i * 2 + 1], dst[i * 6 + k]);
      else
        EXPECT_EQ(0, dst[i * 6 + k]);
    }
  }
}

// Test Stereo to 5.1 conversion.  S16_LE, Unknown.
TEST(FormatConverterOpsTest, StereoTo51S16LEUnknown) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 6;
  const size_t left = -1;
  const size_t right = -1;
  const size_t center = -1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret = s16_stereo_to_51(left, right, center, (uint8_t*)src.get(),
                                frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    for (size_t k = 0; k < 6; ++k) {
      if (k == 0 || k == 1)
        EXPECT_EQ(src[i * 2 + k], dst[i * 6 + k]);
      else
        EXPECT_EQ(0, dst[i * 6 + k]);
    }
  }
}

// Test 5.1 to Stereo conversion.  S16_LE.
TEST(FormatConverterOpsTest, _51ToStereoS16LE) {
  const size_t frames = 4096;
  const size_t in_ch = 6;
  const size_t out_ch = 2;
  const size_t left = 0;
  const size_t right = 1;
  const size_t center = 4;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret =
      s16_51_to_stereo((uint8_t*)src.get(), frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    int16_t half_center = src[i * 6 + center] / 2;
    EXPECT_EQ(S16AddAndClip(src[i * 6 + left], half_center), dst[i * 2 + left]);
    EXPECT_EQ(S16AddAndClip(src[i * 6 + right], half_center),
              dst[i * 2 + right]);
  }
}

// Test Stereo to Quad conversion.  S16_LE, Specify.
TEST(FormatConverterOpsTest, StereoToQuadS16LESpecify) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 4;
  const size_t front_left = 2;
  const size_t front_right = 3;
  const size_t rear_left = 0;
  const size_t rear_right = 1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret =
      s16_stereo_to_quad(front_left, front_right, rear_left, rear_right,
                         (uint8_t*)src.get(), frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    EXPECT_EQ(src[i * 2 + 0], dst[i * 4 + front_left]);
    EXPECT_EQ(src[i * 2 + 0], dst[i * 4 + rear_left]);
    EXPECT_EQ(src[i * 2 + 1], dst[i * 4 + front_right]);
    EXPECT_EQ(src[i * 2 + 1], dst[i * 4 + rear_right]);
  }
}

// Test Stereo to Quad conversion.  S16_LE, Default.
TEST(FormatConverterOpsTest, StereoToQuadS16LEDefault) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 4;
  const size_t front_left = -1;
  const size_t front_right = -1;
  const size_t rear_left = -1;
  const size_t rear_right = -1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret =
      s16_stereo_to_quad(front_left, front_right, rear_left, rear_right,
                         (uint8_t*)src.get(), frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    EXPECT_EQ(src[i * 2 + 0], dst[i * 4 + 0]);
    EXPECT_EQ(src[i * 2 + 0], dst[i * 4 + 2]);
    EXPECT_EQ(src[i * 2 + 1], dst[i * 4 + 1]);
    EXPECT_EQ(src[i * 2 + 1], dst[i * 4 + 3]);
  }
}

// Test Quad to Stereo conversion.  S16_LE, Specify.
TEST(FormatConverterOpsTest, QuadToStereoS16LESpecify) {
  const size_t frames = 4096;
  const size_t in_ch = 4;
  const size_t out_ch = 2;
  const size_t front_left = 2;
  const size_t front_right = 3;
  const size_t rear_left = 0;
  const size_t rear_right = 1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret =
      s16_quad_to_stereo(front_left, front_right, rear_left, rear_right,
                         (uint8_t*)src.get(), frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    int16_t left =
        S16AddAndClip(src[i * 4 + front_left], src[i * 4 + rear_left] / 4);
    int16_t right =
        S16AddAndClip(src[i * 4 + front_right], src[i * 4 + rear_right] / 4);
    EXPECT_EQ(left, dst[i * 2 + 0]);
    EXPECT_EQ(right, dst[i * 2 + 1]);
  }
}

// Test Quad to Stereo conversion.  S16_LE, Default.
TEST(FormatConverterOpsTest, QuadToStereoS16LEDefault) {
  const size_t frames = 4096;
  const size_t in_ch = 4;
  const size_t out_ch = 2;
  const size_t front_left = -1;
  const size_t front_right = -1;
  const size_t rear_left = -1;
  const size_t rear_right = -1;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret =
      s16_quad_to_stereo(front_left, front_right, rear_left, rear_right,
                         (uint8_t*)src.get(), frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    int16_t left = S16AddAndClip(src[i * 4 + 0], src[i * 4 + 2] / 4);
    int16_t right = S16AddAndClip(src[i * 4 + 1], src[i * 4 + 3] / 4);
    EXPECT_EQ(left, dst[i * 2 + 0]);
    EXPECT_EQ(right, dst[i * 2 + 1]);
  }
}

// Test Stereo to 3ch conversion.  S16_LE.
TEST(FormatConverterOpsTest, StereoTo3chS16LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 3;
  struct cras_audio_format fmt = {
      .format = SND_PCM_FORMAT_S16_LE,
      .frame_rate = 48000,
      .num_channels = 3,
  };

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);

  size_t ret = s16_default_all_to_all(&fmt, in_ch, out_ch, (uint8_t*)src.get(),
                                      frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t i = 0; i < frames; ++i) {
    for (size_t k = 0; k < in_ch; ++k)
      src[i * in_ch + k] /= in_ch;
    for (size_t k = 1; k < in_ch; ++k)
      src[i * in_ch + 0] += src[i * in_ch + k];
  }
  for (size_t i = 0; i < frames; ++i) {
    for (size_t k = 0; k < out_ch; ++k)
      EXPECT_EQ(src[i * in_ch + 0], dst[i * out_ch + k]);
  }
}

// Test Multiply with Coef.  S16_LE.
TEST(FormatConverterOpsTest, MultiplyWithCoefS16LE) {
  const size_t buf_size = 4096;

  S16LEPtr buf = CreateS16LE(buf_size);
  FloatPtr coef = CreateFloat(buf_size);

  int16_t ret = s16_multiply_buf_with_coef(coef.get(), buf.get(), buf_size);

  int32_t exp = 0;
  for (size_t i = 0; i < buf_size; ++i)
    exp += coef[i] * buf[i];
  exp = MIN(MAX(exp, SHRT_MIN), SHRT_MAX);

  EXPECT_EQ((int16_t)exp, ret);
}

// Test Convert Channels.  S16_LE.
TEST(FormatConverterOpsTest, ConvertChannelsS16LE) {
  const size_t frames = 4096;
  const size_t in_ch = 2;
  const size_t out_ch = 3;

  S16LEPtr src = CreateS16LE(frames * in_ch);
  S16LEPtr dst = CreateS16LE(frames * out_ch);
  FloatPtr ch_conv_mtx = CreateFloat(out_ch * in_ch);
  std::unique_ptr<float*[]> mtx(new float*[out_ch]);
  for (size_t i = 0; i < out_ch; ++i)
    mtx[i] = &ch_conv_mtx[i * in_ch];

  size_t ret =
      s16_convert_channels(mtx.get(), in_ch, out_ch, (uint8_t*)src.get(),
                           frames, (uint8_t*)dst.get());
  EXPECT_EQ(ret, frames);

  for (size_t fr = 0; fr < frames; ++fr) {
    for (size_t i = 0; i < out_ch; ++i) {
      int16_t exp = 0;
      for (size_t k = 0; k < in_ch; ++k)
        exp += mtx[i][k] * src[fr * in_ch + k];
      exp = MIN(MAX(exp, SHRT_MIN), SHRT_MAX);
      EXPECT_EQ(exp, dst[fr * out_ch + i]);
    }
  }
}

extern "C" {}  // extern "C"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
