/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>
#include <syslog.h>
#include "audio_thread.h"
#include "cras_iodev_list.h"
#include "cras_main_message.h"
#include "cras_system_state.h"
#include "cras_types.h"
#include "cras_util.h"

#define MIN_WAIT_SECOND 30

struct cras_audio_thread_event_message {
	struct cras_main_message header;
	enum CRAS_AUDIO_THREAD_EVENT_TYPE event_type;
};

static void take_snapshot(enum CRAS_AUDIO_THREAD_EVENT_TYPE event_type)
{
	struct cras_audio_thread_snapshot *snapshot;

	snapshot = (struct cras_audio_thread_snapshot *)calloc(
		1, sizeof(struct cras_audio_thread_snapshot));
	struct timespec now_time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now_time);
	snapshot->timestamp = now_time;
	snapshot->event_type = event_type;
	audio_thread_dump_thread_info(cras_iodev_list_get_audio_thread(),
				      &snapshot->audio_debug_info);
	cras_system_state_add_snapshot(snapshot);
}

static void cras_audio_thread_event_message_init(
	struct cras_audio_thread_event_message *msg,
	enum CRAS_AUDIO_THREAD_EVENT_TYPE event_type)
{
	msg->header.type = CRAS_MAIN_AUDIO_THREAD_EVENT;
	msg->header.length = sizeof(*msg);
	msg->event_type = event_type;
}

int cras_audio_thread_event_send(enum CRAS_AUDIO_THREAD_EVENT_TYPE event_type)
{
	struct cras_audio_thread_event_message msg;
	cras_audio_thread_event_message_init(&msg, event_type);
	return cras_main_message_send(&msg.header);
}

int cras_audio_thread_event_debug()
{
	return cras_audio_thread_event_send(AUDIO_THREAD_EVENT_DEBUG);
}

int cras_audio_thread_event_busyloop()
{
	return cras_audio_thread_event_send(AUDIO_THREAD_EVENT_BUSYLOOP);
}

int cras_audio_thread_event_underrun()
{
	return cras_audio_thread_event_send(AUDIO_THREAD_EVENT_UNDERRUN);
}

int cras_audio_thread_event_severe_underrun()
{
	return cras_audio_thread_event_send(AUDIO_THREAD_EVENT_SEVERE_UNDERRUN);
}

int cras_audio_thread_event_drop_samples()
{
	return cras_audio_thread_event_send(AUDIO_THREAD_EVENT_DROP_SAMPLES);
}

static struct timespec last_event_snapshot_time[AUDIO_THREAD_EVENT_TYPE_COUNT];

/*
 * Callback function for handling audio thread events in main thread,
 * which takes a snapshot of the audio thread and waits at least 30 seconds
 * for the same event type. Events with the same event type within 30 seconds
 * will be ignored by the handle function.
 */
static void handle_audio_thread_event_message(struct cras_main_message *msg,
					      void *arg)
{
	struct cras_audio_thread_event_message *audio_thread_msg =
		(struct cras_audio_thread_event_message *)msg;
	struct timespec now_time;

	/*
	 * Skip invalid event types
	 */
	if (audio_thread_msg->event_type >= AUDIO_THREAD_EVENT_TYPE_COUNT)
		return;

	struct timespec *last_snapshot_time =
		&last_event_snapshot_time[audio_thread_msg->event_type];

	clock_gettime(CLOCK_REALTIME, &now_time);

	/*
	 * Wait at least 30 seconds for the same event type
	 */
	struct timespec diff_time;
	subtract_timespecs(&now_time, last_snapshot_time, &diff_time);
	if ((last_snapshot_time->tv_sec == 0 &&
	     last_snapshot_time->tv_nsec == 0) ||
	    diff_time.tv_sec >= MIN_WAIT_SECOND) {
		take_snapshot(audio_thread_msg->event_type);
		*last_snapshot_time = now_time;
	}
}

int cras_audio_thread_monitor_init()
{
	memset(last_event_snapshot_time, 0,
	       sizeof(struct timespec) * AUDIO_THREAD_EVENT_TYPE_COUNT);
	cras_main_message_add_handler(CRAS_MAIN_AUDIO_THREAD_EVENT,
				      handle_audio_thread_event_message, NULL);
	return 0;
}
