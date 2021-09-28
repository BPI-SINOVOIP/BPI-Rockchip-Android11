/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <syslog.h>

#include "cras_rstream.h"
#include "cras_server.h"
#include "cras_system_state.h"
#include "cras_types.h"
#include "server_stream.h"
#include "stream_list.h"

/* Parameters used for server stream. */
static unsigned int server_stream_block_size = 480;

/*
 * Server stream doesn't care what format is used, because no
 * client is reading data from stream. The main point is to
 * make pinned device open and let data flow through its dsp
 * pipeline.
 */
static struct cras_audio_format format = {
	SND_PCM_FORMAT_S16_LE,
	48000,
	2,
};

/*
 * Information of a stream created by server. Currently only
 * one server stream is allowed, for echo reference use.
 */
static struct cras_rstream_config *stream_config;

/* Actually create the server stream and add to stream list. */
static void server_stream_add_cb(void *data)
{
	struct stream_list *stream_list = (struct stream_list *)data;
	struct cras_rstream *stream;

	if (!stream_config)
		return;

	stream_list_add(stream_list, stream_config, &stream);
}

void server_stream_create(struct stream_list *stream_list, unsigned int dev_idx)
{
	int audio_fd = -1;
	int client_shm_fd = -1;

	if (stream_config) {
		syslog(LOG_ERR, "server stream already exists, dev %u",
		       stream_config->dev_idx);
		return;
	}

	stream_config =
		(struct cras_rstream_config *)calloc(1, sizeof(*stream_config));
	cras_rstream_config_init(
		/*client=*/NULL, cras_get_stream_id(SERVER_STREAM_CLIENT_ID, 0),
		CRAS_STREAM_TYPE_DEFAULT, CRAS_CLIENT_TYPE_SERVER_STREAM,
		CRAS_STREAM_INPUT, dev_idx,
		/*flags=*/SERVER_ONLY,
		/*effects=*/0, &format, server_stream_block_size,
		server_stream_block_size, &audio_fd, &client_shm_fd,
		/*client_shm_size=*/0, stream_config);

	/* Schedule add stream in next main thread loop. */
	cras_system_add_task(server_stream_add_cb, stream_list);
}

static void server_stream_rm_cb(void *data)
{
	struct stream_list *stream_list = (struct stream_list *)data;

	if (stream_config == NULL)
		return;

	if (stream_list_rm(stream_list, stream_config->stream_id))
		syslog(LOG_ERR, "Server stream %x no longer exist",
		       stream_config->stream_id);

	free(stream_config);
	stream_config = NULL;
}

void server_stream_destroy(struct stream_list *stream_list,
			   unsigned int dev_idx)
{
	if (!stream_config || stream_config->dev_idx != dev_idx) {
		syslog(LOG_ERR, "No server stream to destroy");
		return;
	}
	/* Schedule remove stream in next main thread loop. */
	cras_system_add_task(server_stream_rm_cb, stream_list);
}
