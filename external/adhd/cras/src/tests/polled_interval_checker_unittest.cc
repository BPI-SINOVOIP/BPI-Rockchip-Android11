// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "polled_interval_checker.h"
}

static const int INTERVAL_DURATION = 5;

static struct timespec time_now;

static struct polled_interval* create_interval() {
  struct polled_interval* interval =
      pic_polled_interval_create(INTERVAL_DURATION);
  EXPECT_NE(interval, static_cast<struct polled_interval*>(NULL));
  return interval;
}

TEST(PolledIntervalCheckerTest, CreateDestroy) {
  // Create an interval, checks it is non-null.
  struct polled_interval* interval = create_interval();

  pic_polled_interval_destroy(&interval);

  // Check it's been set to null.
  EXPECT_EQ(interval, static_cast<struct polled_interval*>(NULL));
}

TEST(PolledIntervalCheckerTest, BasicFlow) {
  // Set initial time.
  time_now.tv_sec = 1000;
  time_now.tv_nsec = 0;
  pic_update_current_time();

  // Create interval starting at initial time.
  struct polled_interval* interval = create_interval();

  // Check it hasn't elapsed.
  EXPECT_FALSE(pic_interval_elapsed(interval));

  // Increment time by less than the interval duration.
  time_now.tv_sec += INTERVAL_DURATION / 2;
  pic_update_current_time();

  // Check the interval hasn't elapsed yet.
  EXPECT_FALSE(pic_interval_elapsed(interval));

  // Increment time past the duration of the interval.
  time_now.tv_sec += INTERVAL_DURATION;

  // We haven't updated the current time, check the interval hasn't
  // elapsed (that it isn't calling clock_gettime without us asking it to).
  EXPECT_FALSE(pic_interval_elapsed(interval));

  // Update time, check the interval has elapsed.
  pic_update_current_time();
  EXPECT_TRUE(pic_interval_elapsed(interval));
  pic_polled_interval_destroy(&interval);
}

TEST(PolledIntervalCheckerTest, DoesNotResetAutomatically) {
  // Set initial time.
  time_now.tv_sec = 1000;
  time_now.tv_nsec = 0;
  pic_update_current_time();

  struct polled_interval* interval = create_interval();

  // Sanity check.
  EXPECT_FALSE(pic_interval_elapsed(interval));

  // Increment time so the interval elapses.
  time_now.tv_sec += INTERVAL_DURATION;
  pic_update_current_time();

  // Check the interval has elapsed.
  EXPECT_TRUE(pic_interval_elapsed(interval));

  // Increment time further.
  time_now.tv_sec += INTERVAL_DURATION * 2;
  pic_update_current_time();

  // Check the interval has still elapsed.
  EXPECT_TRUE(pic_interval_elapsed(interval));

  // Check repeated calls return true.
  EXPECT_TRUE(pic_interval_elapsed(interval));
  pic_polled_interval_destroy(&interval);
}

TEST(PolledIntervalCheckerTest, Reset) {
  // Set initial time.
  time_now.tv_sec = 1000;
  time_now.tv_nsec = 0;
  pic_update_current_time();

  struct polled_interval* interval = create_interval();

  // Sanity check.
  EXPECT_FALSE(pic_interval_elapsed(interval));

  // Increment time so the interval elapses.
  time_now.tv_sec += INTERVAL_DURATION;
  pic_update_current_time();

  // Check the interval has elapsed.
  EXPECT_TRUE(pic_interval_elapsed(interval));

  // Increment time further.
  time_now.tv_sec += INTERVAL_DURATION * 2;
  pic_update_current_time();

  // Check the interval has still elapsed.
  EXPECT_TRUE(pic_interval_elapsed(interval));

  // Reset the interval.
  pic_interval_reset(interval);

  // Check it's been reset.
  EXPECT_FALSE(pic_interval_elapsed(interval));

  // Increment time to just before it should elapse again.
  time_now.tv_sec += INTERVAL_DURATION - 1;
  pic_update_current_time();

  // Check it still has not elapsed.
  EXPECT_FALSE(pic_interval_elapsed(interval));

  // Increment time to one duration after we reset it.
  time_now.tv_sec += 1;
  pic_update_current_time();

  // Check the interval has elapsed now.
  EXPECT_TRUE(pic_interval_elapsed(interval));
  pic_polled_interval_destroy(&interval);
}

/* Stubs */
extern "C" {

int clock_gettime(clockid_t clk_id, struct timespec* tp) {
  *tp = time_now;
  return 0;
}

}  // extern "C"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
