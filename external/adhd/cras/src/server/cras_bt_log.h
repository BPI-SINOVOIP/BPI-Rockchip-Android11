/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_BT_LOG_H_
#define CRAS_BT_LOG_H_

#include <stdint.h>

#include "cras_types.h"

#define CRAS_BT_LOGGING 1

#if (CRAS_BT_LOGGING)
#define BTLOG(log, event, data1, data2)                                        \
	cras_bt_event_log_data(log, event, data1, data2);
#else
#define BTLOG(log, event, data1, data2)
#endif

extern struct cras_bt_event_log *btlog;

static inline struct cras_bt_event_log *cras_bt_event_log_init()
{
	struct cras_bt_event_log *log;
	log = (struct cras_bt_event_log *)calloc(
		1, sizeof(struct cras_bt_event_log));
	log->len = CRAS_BT_EVENT_LOG_SIZE;

	return log;
}

static inline void cras_bt_event_log_deinit(struct cras_bt_event_log *log)
{
	free(log);
}

static inline void cras_bt_event_log_data(struct cras_bt_event_log *log,
					  enum CRAS_BT_LOG_EVENTS event,
					  uint32_t data1, uint32_t data2)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	log->log[log->write_pos].tag_sec =
		(event << 24) | (now.tv_sec & 0x00ffffff);
	log->log[log->write_pos].nsec = now.tv_nsec;
	log->log[log->write_pos].data1 = data1;
	log->log[log->write_pos].data2 = data2;

	log->write_pos++;
	log->write_pos %= CRAS_BT_EVENT_LOG_SIZE;
}

#endif /* CRAS_BT_LOG_H_ */