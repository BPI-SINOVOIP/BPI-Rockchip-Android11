// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biquad.h"

#include <assert.h>
#include <gtest/gtest.h>
#include <math.h>

namespace {

TEST(InvalidFrequencyTest, All) {
  struct biquad bq, test_bq;
  float f_over = 1.5;
  float f_under = -0.1;
  double db_gain = 2;
  double A = pow(10.0, db_gain / 40);

  // check response to freq >= 1
  biquad_set(&bq, BQ_LOWPASS, f_over, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_HIGHPASS, f_over, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_BANDPASS, f_over, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_LOWSHELF, f_over, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = A * A;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_HIGHSHELF, f_over, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_PEAKING, f_over, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_NOTCH, f_over, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_ALLPASS, f_over, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  // check response to frew <= 0
  biquad_set(&bq, BQ_LOWPASS, f_under, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_HIGHPASS, f_under, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_BANDPASS, f_under, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_LOWSHELF, f_under, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_HIGHSHELF, f_under, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = A * A;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_PEAKING, f_under, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_NOTCH, f_under, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_ALLPASS, f_under, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);
}

TEST(InvalidQTest, All) {
  struct biquad bq, test_bq;
  float f = 0.5;
  float Q = -0.1;
  double db_gain = 2;
  double A = pow(10.0, db_gain / 40);

  // check response to Q <= 0
  // Low and High pass filters scope Q making the test mute

  biquad_set(&bq, BQ_BANDPASS, f, Q, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = 1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  // Low and high shelf do not compute resonance

  biquad_set(&bq, BQ_PEAKING, f, Q, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = A * A;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_NOTCH, f, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);

  biquad_set(&bq, BQ_ALLPASS, f, 0, db_gain);
  test_bq = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  test_bq.b0 = -1;
  EXPECT_EQ(bq.b0, test_bq.b0);
  EXPECT_EQ(bq.b1, test_bq.b1);
  EXPECT_EQ(bq.b2, test_bq.b2);
  EXPECT_EQ(bq.a1, test_bq.a1);
  EXPECT_EQ(bq.a2, test_bq.a2);
}

}  //  namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
