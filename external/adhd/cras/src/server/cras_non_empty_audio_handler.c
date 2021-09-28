/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include "cras_main_message.h"
#include "cras_observer.h"
#include "cras_system_state.h"

struct non_empty_audio_msg {
	struct cras_main_message header;
	int32_t non_empty;
};

/* The following functions are called from audio thread. */

static void init_non_empty_audio_msg(struct non_empty_audio_msg *msg)
{
	memset(msg, 0, sizeof(*msg));
	msg->header.type = CRAS_MAIN_NON_EMPTY_AUDIO_STATE;
	msg->header.length = sizeof(*msg);
}

int cras_non_empty_audio_send_msg(int32_t non_empty)
{
	struct non_empty_audio_msg msg;
	int rc;

	init_non_empty_audio_msg(&msg);
	msg.non_empty = non_empty;

	rc = cras_main_message_send((struct cras_main_message *)&msg);
	if (rc < 0)
		syslog(LOG_ERR, "Failed to send non-empty audio message!");

	return rc;
}

/* The following functions are called from main thread. */

static void handle_non_empty_audio_message(struct cras_main_message *msg,
					   void *arg)
{
	struct non_empty_audio_msg *audio_msg =
		(struct non_empty_audio_msg *)msg;

	cras_system_state_set_non_empty_status(audio_msg->non_empty);
	cras_observer_notify_non_empty_audio_state_changed(
		audio_msg->non_empty);
}

int cras_non_empty_audio_handler_init()
{
	cras_main_message_add_handler(CRAS_MAIN_NON_EMPTY_AUDIO_STATE,
				      handle_non_empty_audio_message, NULL);
	return 0;
}