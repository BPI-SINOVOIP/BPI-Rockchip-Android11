/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdlib.h>
#include <syslog.h>

#include "audio_thread.h"
#include "cras_apm_list.h"
#include "cras_bt_log.h"
#include "cras_capture_rclient.h"
#include "cras_config.h"
#include "cras_control_rclient.h"
#include "cras_dsp.h"
#include "cras_iodev.h"
#include "cras_iodev_list.h"
#include "cras_messages.h"
#include "cras_observer.h"
#include "cras_playback_rclient.h"
#include "cras_rclient.h"
#include "cras_rstream.h"
#include "cras_server_metrics.h"
#include "cras_system_state.h"
#include "cras_types.h"
#include "cras_util.h"
#include "stream_list.h"
#include "utlist.h"

/* Removes all streams that the client owns and destroys it. */
void cras_rclient_destroy(struct cras_rclient *client)
{
	client->ops->destroy(client);
}

/* Entry point for handling a message from the client.  Called from the main
 * server context. */
int cras_rclient_buffer_from_client(struct cras_rclient *client,
				    const uint8_t *buf, size_t buf_len,
				    int *fds, int num_fds)
{
	struct cras_server_message *msg = (struct cras_server_message *)buf;

	if (buf_len < sizeof(*msg))
		return -EINVAL;
	if (msg->length != buf_len)
		return -EINVAL;
	return client->ops->handle_message_from_client(client, msg, fds,
						       num_fds);
}

/* Sends a message to the client. */
int cras_rclient_send_message(const struct cras_rclient *client,
			      const struct cras_client_message *msg, int *fds,
			      unsigned int num_fds)
{
	return client->ops->send_message_to_client(client, msg, fds, num_fds);
}

struct cras_rclient *cras_rclient_create(int fd, size_t id,
					 enum CRAS_CONNECTION_TYPE conn_type)
{
	if (!cras_validate_connection_type(conn_type))
		goto error;

	switch (conn_type) {
	case CRAS_CONTROL:
		return cras_control_rclient_create(fd, id);
	case CRAS_PLAYBACK:
		return cras_playback_rclient_create(fd, id);
	case CRAS_CAPTURE:
		return cras_capture_rclient_create(fd, id);
	default:
		goto error;
	}

error:
	syslog(LOG_ERR, "unsupported connection type");
	return NULL;
}
