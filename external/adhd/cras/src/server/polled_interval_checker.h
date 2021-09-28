/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef POLLED_ACTIVITY_CHECKER_H_
#define POLLED_ACTIVITY_CHECKER_H_

#include <time.h>

/* Represents a time interval, in seconds, which can be checked periodically. */
struct polled_interval;

/*
 * Creates a new polled_interval, of the specified duration. The interval will
 * first elapse interval_sec after it was created.
 *
 * Call pic_update_current_time() shortly before this function.
 */
struct polled_interval *pic_polled_interval_create(int interval_sec);

/*
 * Destroys the specified polled_interval, and set's the pointer to it to NULL.
 */
void pic_polled_interval_destroy(struct polled_interval **interval);

/*
 * Whether the interval's duration has elapsed (since the interval was created
 * or reset).
 *
 * Call pic_update_current_time() shortly before this function.
 */
int pic_interval_elapsed(const struct polled_interval *interval);

/*
 * Resets the interval; it will elapse it's specified duration from now.
 *
 * Call pic_update_current_time() shortly before this function.
 */
void pic_interval_reset(struct polled_interval *pi);

/*
 * Updates the current time, which is used in all other pic_* functions (which
 * will never update the current time). This update is pulled out separately to
 * allow the caller to control when and how often the time is updated.
 */
void pic_update_current_time();

#endif /* POLLED_ACTIVITY_CHECKER_H_ */