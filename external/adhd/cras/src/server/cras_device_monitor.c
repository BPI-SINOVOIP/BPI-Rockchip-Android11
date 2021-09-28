/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>
#include <syslog.h>

#include "cras_device_monitor.h"
#include "cras_iodev_list.h"
#include "cras_main_message.h"

enum CRAS_DEVICE_MONITOR_MSG_TYPE {
	RESET_DEVICE,
	SET_MUTE_STATE,
};

struct cras_device_monitor_message {
	struct cras_main_message header;
	enum CRAS_DEVICE_MONITOR_MSG_TYPE message_type;
	unsigned int dev_idx;
};

static void init_device_msg(struct cras_device_monitor_message *msg,
			    enum CRAS_DEVICE_MONITOR_MSG_TYPE type,
			    unsigned int dev_idx)
{
	memset(msg, 0, sizeof(*msg));
	msg->header.type = CRAS_MAIN_MONITOR_DEVICE;
	msg->header.length = sizeof(*msg);
	msg->message_type = type;
	msg->dev_idx = dev_idx;
}

int cras_device_monitor_reset_device(unsigned int dev_idx)
{
	struct cras_device_monitor_message msg;
	int err;

	init_device_msg(&msg, RESET_DEVICE, dev_idx);
	err = cras_main_message_send((struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR, "Failed to send device message %d",
		       RESET_DEVICE);
		return err;
	}
	return 0;
}

int cras_device_monitor_set_device_mute_state(unsigned int dev_idx)
{
	struct cras_device_monitor_message msg;
	int err;

	init_device_msg(&msg, SET_MUTE_STATE, dev_idx);
	err = cras_main_message_send((struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR, "Failed to send device message %d",
		       SET_MUTE_STATE);
		return err;
	}
	return 0;
}

/* When device is in a bad state, e.g. severe underrun,
 * it might break how audio thread works and cause busy wake up loop.
 * Resetting the device can bring device back to normal state.
 * Let main thread follow the disable/enable sequence in iodev_list
  * to properly close/open the device while enabling/disabling fallback
 * device.
 */
static void handle_device_message(struct cras_main_message *msg, void *arg)
{
	struct cras_device_monitor_message *device_msg =
		(struct cras_device_monitor_message *)msg;

	switch (device_msg->message_type) {
	case RESET_DEVICE:
		syslog(LOG_ERR, "trying to recover device 0x%x by resetting it",
		       device_msg->dev_idx);
		cras_iodev_list_suspend_dev(device_msg->dev_idx);
		cras_iodev_list_resume_dev(device_msg->dev_idx);
		break;
	case SET_MUTE_STATE:
		cras_iodev_list_set_dev_mute(device_msg->dev_idx);
		break;
	default:
		syslog(LOG_ERR, "Unknown device message type %u",
		       device_msg->message_type);
		break;
	}
}

int cras_device_monitor_init()
{
	cras_main_message_add_handler(CRAS_MAIN_MONITOR_DEVICE,
				      handle_device_message, NULL);
	return 0;
}
