/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The blow logging funcitons must only be called from the audio thread.
 */

#ifndef AUDIO_THREAD_LOG_H_
#define AUDIO_THREAD_LOG_H_

#include <sys/mman.h>
#include <pthread.h>
#include <stdint.h>
#include <syslog.h>

#include "cras_types.h"
#include "cras_shm.h"

#define AUDIO_THREAD_LOGGING 1

#if (AUDIO_THREAD_LOGGING)
#define ATLOG(log, event, data1, data2, data3)                                 \
	audio_thread_event_log_data(log, event, data1, data2, data3);
#else
#define ATLOG(log, event, data1, data2, data3)
#endif

extern struct audio_thread_event_log *atlog;
extern int atlog_rw_shm_fd;
extern int atlog_ro_shm_fd;

static inline struct audio_thread_event_log *
audio_thread_event_log_init(char *name)
{
	struct audio_thread_event_log *log;

	atlog_ro_shm_fd = -1;
	atlog_rw_shm_fd = -1;

	log = (struct audio_thread_event_log *)cras_shm_setup(
		name, sizeof(*log), &atlog_rw_shm_fd, &atlog_ro_shm_fd);
	/* Fallback to calloc if device shared memory resource is empty and
	 * cras_shm_setup fails.
	 */
	if (log == NULL) {
		syslog(LOG_ERR, "Failed to create atlog by cras_shm_setup");
		log = (struct audio_thread_event_log *)calloc(
			1, sizeof(struct audio_thread_event_log));
	}
	log->len = AUDIO_THREAD_EVENT_LOG_SIZE;

	return log;
}

static inline void
audio_thread_event_log_deinit(struct audio_thread_event_log *log, char *name)
{
	if (log) {
		if (atlog_rw_shm_fd >= 0) {
			munmap(log, sizeof(*log));
			cras_shm_close_unlink(name, atlog_rw_shm_fd);
		} else {
			free(log);
		}

		if (atlog_ro_shm_fd >= 0)
			close(atlog_ro_shm_fd);
	}
}

/* Log a tag and the current time, Uses two words, the first is split
 * 8 bits for tag and 24 for seconds, second word is micro seconds.
 */
static inline void
audio_thread_event_log_data(struct audio_thread_event_log *log,
			    enum AUDIO_THREAD_LOG_EVENTS event, uint32_t data1,
			    uint32_t data2, uint32_t data3)
{
	struct timespec now;
	uint64_t pos_mod_len = log->write_pos % AUDIO_THREAD_EVENT_LOG_SIZE;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);

	log->log[pos_mod_len].tag_sec =
		(event << 24) | (now.tv_sec & 0x00ffffff);
	log->log[pos_mod_len].nsec = now.tv_nsec;
	log->log[pos_mod_len].data1 = data1;
	log->log[pos_mod_len].data2 = data2;
	log->log[pos_mod_len].data3 = data3;

	log->write_pos++;
}

#endif /* AUDIO_THREAD_LOG_H_ */
