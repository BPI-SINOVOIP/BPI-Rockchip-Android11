/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <syslog.h>

#include "cras_audio_area.h"
#include "cras_config.h"
#include "cras_messages.h"
#include "cras_rclient.h"
#include "cras_rstream.h"
#include "cras_server_metrics.h"
#include "cras_shm.h"
#include "cras_types.h"
#include "buffer_share.h"
#include "cras_system_state.h"

void cras_rstream_config_init(
	struct cras_rclient *client, cras_stream_id_t stream_id,
	enum CRAS_STREAM_TYPE stream_type, enum CRAS_CLIENT_TYPE client_type,
	enum CRAS_STREAM_DIRECTION direction, uint32_t dev_idx, uint32_t flags,
	uint32_t effects, const struct cras_audio_format *format,
	size_t buffer_frames, size_t cb_threshold, int *audio_fd,
	int *client_shm_fd, size_t client_shm_size,
	struct cras_rstream_config *stream_config)
{
	stream_config->stream_id = stream_id;
	stream_config->stream_type = stream_type;
	stream_config->client_type = client_type;
	stream_config->direction = direction;
	stream_config->dev_idx = dev_idx;
	stream_config->flags = flags;
	stream_config->effects = effects;
	stream_config->format = format;
	stream_config->buffer_frames = buffer_frames;
	stream_config->cb_threshold = cb_threshold;
	stream_config->audio_fd = *audio_fd;
	*audio_fd = -1;
	stream_config->client_shm_fd = *client_shm_fd;
	*client_shm_fd = -1;
	stream_config->client_shm_size = client_shm_size;
	stream_config->client = client;
}

void cras_rstream_config_init_with_message(
	struct cras_rclient *client, const struct cras_connect_message *msg,
	int *aud_fd, int *client_shm_fd,
	const struct cras_audio_format *remote_fmt,
	struct cras_rstream_config *stream_config)
{
	cras_rstream_config_init(client, msg->stream_id, msg->stream_type,
				 msg->client_type, msg->direction, msg->dev_idx,
				 msg->flags, msg->effects, remote_fmt,
				 msg->buffer_frames, msg->cb_threshold, aud_fd,
				 client_shm_fd, msg->client_shm_size,
				 stream_config);
}

void cras_rstream_config_cleanup(struct cras_rstream_config *stream_config)
{
	if (stream_config->audio_fd >= 0)
		close(stream_config->audio_fd);
	if (stream_config->client_shm_fd >= 0)
		close(stream_config->client_shm_fd);
}

/* Setup the shared memory area used for audio samples. client_shm_fd must be
 * closed after calling this function.
 */
static inline int setup_shm_area(struct cras_rstream *stream, int client_shm_fd,
				 size_t client_shm_size)
{
	const struct cras_audio_format *fmt = &stream->format;
	char header_name[NAME_MAX];
	char samples_name[NAME_MAX];
	struct cras_shm_info header_info, samples_info;
	uint32_t frame_bytes, used_size;
	int rc;

	if (stream->shm) {
		/* already setup */
		return -EEXIST;
	}

	snprintf(header_name, sizeof(header_name),
		 "/cras-%d-stream-%08x-header", getpid(), stream->stream_id);

	rc = cras_shm_info_init(header_name, cras_shm_header_size(),
				&header_info);
	if (rc)
		return rc;

	frame_bytes = snd_pcm_format_physical_width(fmt->format) / 8 *
		      fmt->num_channels;
	used_size = stream->buffer_frames * frame_bytes;

	if (client_shm_fd >= 0 && client_shm_size > 0) {
		rc = cras_shm_info_init_with_fd(client_shm_fd, client_shm_size,
						&samples_info);
	} else {
		snprintf(samples_name, sizeof(samples_name),
			 "/cras-%d-stream-%08x-samples", getpid(),
			 stream->stream_id);
		rc = cras_shm_info_init(
			samples_name,
			cras_shm_calculate_samples_size(used_size),
			&samples_info);
	}
	if (rc) {
		cras_shm_info_cleanup(&header_info);
		return rc;
	}

	int samples_prot = 0;
	if (stream->direction == CRAS_STREAM_OUTPUT)
		samples_prot = PROT_READ;
	else
		samples_prot = PROT_WRITE;

	rc = cras_audio_shm_create(&header_info, &samples_info, samples_prot,
				   &stream->shm);
	if (rc)
		return rc;

	cras_shm_set_frame_bytes(stream->shm, frame_bytes);
	cras_shm_set_used_size(stream->shm, used_size);

	stream->audio_area =
		cras_audio_area_create(stream->format.num_channels);
	cras_audio_area_config_channels(stream->audio_area, &stream->format);

	return 0;
}

static inline int buffer_meets_size_limit(size_t buffer_size, size_t rate)
{
	return buffer_size > (CRAS_MIN_BUFFER_TIME_IN_US * rate) / 1000000;
}

/* Verifies that the given stream parameters are valid. */
static int verify_rstream_parameters(enum CRAS_STREAM_DIRECTION direction,
				     const struct cras_audio_format *format,
				     enum CRAS_STREAM_TYPE stream_type,
				     size_t buffer_frames, size_t cb_threshold,
				     int client_shm_fd, size_t client_shm_size,
				     struct cras_rclient *client,
				     struct cras_rstream **stream_out)
{
	if (!buffer_meets_size_limit(buffer_frames, format->frame_rate)) {
		syslog(LOG_ERR, "rstream: invalid buffer_frames %zu\n",
		       buffer_frames);
		return -EINVAL;
	}
	if (stream_out == NULL) {
		syslog(LOG_ERR, "rstream: stream_out can't be NULL\n");
		return -EINVAL;
	}
	if (format == NULL) {
		syslog(LOG_ERR, "rstream: format can't be NULL\n");
		return -EINVAL;
	}
	if ((format->format != SND_PCM_FORMAT_S16_LE) &&
	    (format->format != SND_PCM_FORMAT_S32_LE) &&
	    (format->format != SND_PCM_FORMAT_U8) &&
	    (format->format != SND_PCM_FORMAT_S24_LE)) {
		syslog(LOG_ERR, "rstream: format %d not supported\n",
		       format->format);
		return -EINVAL;
	}
	if (direction != CRAS_STREAM_OUTPUT && direction != CRAS_STREAM_INPUT) {
		syslog(LOG_ERR, "rstream: Invalid direction.\n");
		return -EINVAL;
	}
	if (stream_type < CRAS_STREAM_TYPE_DEFAULT ||
	    stream_type >= CRAS_STREAM_NUM_TYPES) {
		syslog(LOG_ERR, "rstream: Invalid stream type.\n");
		return -EINVAL;
	}
	if (!buffer_meets_size_limit(cb_threshold, format->frame_rate)) {
		syslog(LOG_ERR, "rstream: cb_threshold too low\n");
		return -EINVAL;
	}
	if ((client_shm_size > 0 && client_shm_fd < 0) ||
	    (client_shm_size == 0 && client_shm_fd >= 0)) {
		syslog(LOG_ERR, "rstream: invalid client-provided shm info\n");
		return -EINVAL;
	}
	return 0;
}

/*
 * Setting pending reply is only needed inside this module.
 */
static void set_pending_reply(struct cras_rstream *stream)
{
	cras_shm_set_callback_pending(stream->shm, 1);
}

/*
 * Clearing pending reply is only needed inside this module.
 */
static void clear_pending_reply(struct cras_rstream *stream)
{
	cras_shm_set_callback_pending(stream->shm, 0);
}

/*
 * Reads one response of audio request from client.
 * Args:
 *   stream[in]: A pointer to cras_rstream.
 *   msg[out]: A pointer to audio_message to hold the message.
 * Returns:
 *   Number of bytes read from the socket.
 *   A negative error code if read fails or the message from client
 *   has errors.
 */
static int get_audio_request_reply(const struct cras_rstream *stream,
				   struct audio_message *msg)
{
	int rc;

	rc = read(stream->fd, msg, sizeof(*msg));
	if (rc < 0)
		return -errno;
	if (rc == 0)
		return rc;
	if (msg->error < 0)
		return msg->error;
	return rc;
}

/*
 * Reads and handles one audio message from client.
 * Returns:
 *   Number of bytes read from the socket.
 *   A negative error code if read fails or the message from client
 *   has errors.
 */
static int read_and_handle_client_message(struct cras_rstream *stream)
{
	struct audio_message msg;
	int rc;

	rc = get_audio_request_reply(stream, &msg);
	if (rc <= 0) {
		syslog(LOG_ERR, "Got error from client: rc: %d", rc);
		clear_pending_reply(stream);
		return rc;
	}

	/*
	 * Got client reply that data in the input stream is captured.
	 */
	if (stream->direction == CRAS_STREAM_INPUT &&
	    msg.id == AUDIO_MESSAGE_DATA_CAPTURED) {
		clear_pending_reply(stream);
	}

	/*
	 * Got client reply that data for output stream is ready in shm.
	 */
	if (stream->direction == CRAS_STREAM_OUTPUT &&
	    msg.id == AUDIO_MESSAGE_DATA_READY) {
		clear_pending_reply(stream);
	}

	return rc;
}

/* Exported functions */

int cras_rstream_create(struct cras_rstream_config *config,
			struct cras_rstream **stream_out)
{
	struct cras_rstream *stream;
	int rc;

	rc = verify_rstream_parameters(
		config->direction, config->format, config->stream_type,
		config->buffer_frames, config->cb_threshold,
		config->client_shm_fd, config->client_shm_size, config->client,
		stream_out);
	if (rc < 0)
		return rc;

	stream = calloc(1, sizeof(*stream));
	if (stream == NULL)
		return -ENOMEM;

	stream->stream_id = config->stream_id;
	stream->stream_type = config->stream_type;
	stream->client_type = config->client_type;
	stream->direction = config->direction;
	stream->flags = config->flags;
	stream->format = *config->format;
	stream->buffer_frames = config->buffer_frames;
	stream->cb_threshold = config->cb_threshold;
	stream->client = config->client;
	stream->shm = NULL;
	stream->master_dev.dev_id = NO_DEVICE;
	stream->master_dev.dev_ptr = NULL;
	stream->num_missed_cb = 0;
	stream->is_pinned = (config->dev_idx != NO_DEVICE);
	stream->pinned_dev_idx = config->dev_idx;

	rc = setup_shm_area(stream, config->client_shm_fd,
			    config->client_shm_size);
	if (rc < 0) {
		syslog(LOG_ERR, "failed to setup shm %d\n", rc);
		free(stream);
		return rc;
	}

	stream->fd = config->audio_fd;
	config->audio_fd = -1;
	stream->buf_state = buffer_share_create(stream->buffer_frames);
	stream->apm_list =
		(stream->direction == CRAS_STREAM_INPUT) ?
			cras_apm_list_create(stream, config->effects) :
			NULL;

	syslog(LOG_DEBUG, "stream %x frames %zu, cb_thresh %zu",
	       config->stream_id, config->buffer_frames, config->cb_threshold);
	*stream_out = stream;

	cras_system_state_stream_added(stream->direction);

	clock_gettime(CLOCK_MONOTONIC_RAW, &stream->start_ts);

	return 0;
}

void cras_rstream_destroy(struct cras_rstream *stream)
{
	cras_server_metrics_missed_cb_frequency(stream);
	cras_system_state_stream_removed(stream->direction);
	close(stream->fd);
	cras_audio_shm_destroy(stream->shm);
	cras_audio_area_destroy(stream->audio_area);
	buffer_share_destroy(stream->buf_state);
	if (stream->apm_list)
		cras_apm_list_destroy(stream->apm_list);
	free(stream);
}

unsigned int cras_rstream_get_effects(const struct cras_rstream *stream)
{
	return stream->apm_list ? cras_apm_list_get_effects(stream->apm_list) :
				  0;
}

struct cras_audio_format *
cras_rstream_post_processing_format(const struct cras_rstream *stream,
				    void *dev_ptr)
{
	struct cras_apm *apm;

	if (NULL == stream->apm_list)
		return NULL;

	apm = cras_apm_list_get(stream->apm_list, dev_ptr);
	if (NULL == apm)
		return NULL;
	return cras_apm_list_get_format(apm);
}

void cras_rstream_record_fetch_interval(struct cras_rstream *rstream,
					const struct timespec *now)
{
	struct timespec ts;

	if (rstream->last_fetch_ts.tv_sec || rstream->last_fetch_ts.tv_nsec) {
		subtract_timespecs(now, &rstream->last_fetch_ts, &ts);
		if (timespec_after(&ts, &rstream->longest_fetch_interval))
			rstream->longest_fetch_interval = ts;
	}
}

static void init_audio_message(struct audio_message *msg,
			       enum CRAS_AUDIO_MESSAGE_ID id, uint32_t frames)
{
	memset(msg, 0, sizeof(*msg));
	msg->id = id;
	msg->frames = frames;
}

int cras_rstream_request_audio(struct cras_rstream *stream,
			       const struct timespec *now)
{
	struct audio_message msg;
	int rc;

	/* Only request samples from output streams. */
	if (stream->direction != CRAS_STREAM_OUTPUT)
		return 0;

	stream->last_fetch_ts = *now;

	init_audio_message(&msg, AUDIO_MESSAGE_REQUEST_DATA,
			   stream->cb_threshold);
	rc = write(stream->fd, &msg, sizeof(msg));
	if (rc < 0)
		return -errno;

	set_pending_reply(stream);

	return rc;
}

int cras_rstream_audio_ready(struct cras_rstream *stream, size_t count)
{
	struct audio_message msg;
	int rc;

	cras_shm_buffer_write_complete(stream->shm);

	/* Mark shm as used. */
	if (stream_is_server_only(stream)) {
		cras_shm_buffer_read_current(stream->shm, count);
		return 0;
	}

	init_audio_message(&msg, AUDIO_MESSAGE_DATA_READY, count);
	rc = write(stream->fd, &msg, sizeof(msg));
	if (rc < 0)
		return -errno;

	set_pending_reply(stream);

	return rc;
}

void cras_rstream_dev_attach(struct cras_rstream *rstream, unsigned int dev_id,
			     void *dev_ptr)
{
	if (buffer_share_add_id(rstream->buf_state, dev_id, dev_ptr) == 0)
		rstream->num_attached_devs++;

	/* TODO(hychao): Handle master device assignment for complicated
	 * routing case.
	 */
	if (rstream->master_dev.dev_id == NO_DEVICE) {
		rstream->master_dev.dev_id = dev_id;
		rstream->master_dev.dev_ptr = dev_ptr;
	}
}

void cras_rstream_dev_detach(struct cras_rstream *rstream, unsigned int dev_id)
{
	if (buffer_share_rm_id(rstream->buf_state, dev_id) == 0)
		rstream->num_attached_devs--;

	if (rstream->master_dev.dev_id == dev_id) {
		int i;
		struct id_offset *o;

		/* Choose the first device id as master. */
		rstream->master_dev.dev_id = NO_DEVICE;
		rstream->master_dev.dev_ptr = NULL;
		for (i = 0; i < rstream->buf_state->id_sz; i++) {
			o = &rstream->buf_state->wr_idx[i];
			if (o->used) {
				rstream->master_dev.dev_id = o->id;
				rstream->master_dev.dev_ptr = o->data;
				break;
			}
		}
	}
}

void cras_rstream_dev_offset_update(struct cras_rstream *rstream,
				    unsigned int frames, unsigned int dev_id)
{
	buffer_share_offset_update(rstream->buf_state, dev_id, frames);
}

void cras_rstream_update_input_write_pointer(struct cras_rstream *rstream)
{
	unsigned int nwritten =
		buffer_share_get_new_write_point(rstream->buf_state);

	cras_shm_buffer_written(rstream->shm, nwritten);
}

void cras_rstream_update_output_read_pointer(struct cras_rstream *rstream)
{
	unsigned int nwritten =
		buffer_share_get_new_write_point(rstream->buf_state);

	cras_shm_buffer_read(rstream->shm, nwritten);
}

unsigned int cras_rstream_dev_offset(const struct cras_rstream *rstream,
				     unsigned int dev_id)
{
	return buffer_share_id_offset(rstream->buf_state, dev_id);
}

void cras_rstream_update_queued_frames(struct cras_rstream *rstream)
{
	rstream->queued_frames =
		MIN(cras_shm_get_frames(rstream->shm), rstream->buffer_frames);
}

unsigned int cras_rstream_playable_frames(struct cras_rstream *rstream,
					  unsigned int dev_id)
{
	return rstream->queued_frames -
	       cras_rstream_dev_offset(rstream, dev_id);
}

float cras_rstream_get_volume_scaler(struct cras_rstream *rstream)
{
	return cras_shm_get_volume_scaler(rstream->shm);
}

uint8_t *cras_rstream_get_readable_frames(struct cras_rstream *rstream,
					  unsigned int offset, size_t *frames)
{
	return cras_shm_get_readable_frames(rstream->shm, offset, frames);
}

int cras_rstream_get_mute(const struct cras_rstream *rstream)
{
	return cras_shm_get_mute(rstream->shm);
}

int cras_rstream_is_pending_reply(const struct cras_rstream *stream)
{
	return cras_shm_callback_pending(stream->shm);
}

int cras_rstream_flush_old_audio_messages(struct cras_rstream *stream)
{
	struct pollfd pollfd;
	int err;

	if (!stream->fd)
		return 0;

	if (stream_is_server_only(stream))
		return 0;

	pollfd.fd = stream->fd;
	pollfd.events = POLLIN;

	do {
		err = poll(&pollfd, 1, 0);
		if (pollfd.revents & POLLIN) {
			err = read_and_handle_client_message(stream);
		}
	} while (err > 0);

	return 0;
}
