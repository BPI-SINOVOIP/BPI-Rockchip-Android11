/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cras_util.h"
#include "polled_interval_checker.h"

struct polled_interval {
	struct timespec last_interval_start_ts;
	int interval_sec;
};

static struct timespec now;

static inline int
get_sec_since_last_active(const struct timespec *last_active_ts)
{
	struct timespec diff;
	subtract_timespecs(&now, last_active_ts, &diff);
	return diff.tv_sec;
}

void pic_update_current_time()
{
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
}

struct polled_interval *pic_polled_interval_create(int interval_sec)
{
	struct polled_interval *pi;
	pi = malloc(sizeof(*pi));
	pi->last_interval_start_ts = now;
	pi->interval_sec = interval_sec;
	return pi;
}

void pic_polled_interval_destroy(struct polled_interval **interval)
{
	free(*interval);
	*interval = NULL;
}

int pic_interval_elapsed(const struct polled_interval *pi)
{
	return get_sec_since_last_active(&pi->last_interval_start_ts) >=
	       pi->interval_sec;
}

void pic_interval_reset(struct polled_interval *pi)
{
	pi->last_interval_start_ts = now;
}