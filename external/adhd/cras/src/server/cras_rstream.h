/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Remote Stream - An audio steam from/to a client.
 */
#ifndef CRAS_RSTREAM_H_
#define CRAS_RSTREAM_H_

#include "cras_apm_list.h"
#include "cras_shm.h"
#include "cras_types.h"

struct cras_connect_message;
struct cras_rclient;
struct dev_mix;

/* Holds informations about the master active device.
 * Members:
 *    dev_id - id of the master device.
 *    dev_ptr - pointer to the master device.
 */
struct master_dev_info {
	int dev_id;
	void *dev_ptr;
};

/* cras_rstream is used to manage an active audio stream from
 * a client.  Each client can have any number of open streams for
 * playing or recording.
 * Members:
 *    stream_id - identifier for this stream.
 *    stream_type - not used.
 *    client_type - The client type of this stream, like Chrome, ARC++.
 *    direction - input or output.
 *    flags - Indicative of what special handling is needed.
 *    fd - Socket for requesting and sending audio buffer events.
 *    buffer_frames - Buffer size in frames.
 *    cb_threshold - Callback client when this much is left.
 *    master_dev_info - The info of the master device this stream attaches to.
 *    is_draining - The stream is draining and waiting to be removed.
 *    client - The client who uses this stream.
 *    shm - shared memory
 *    audio_area - space for playback/capture audio
 *    format - format of the stream
 *    next_cb_ts - Next callback time for this stream.
 *    sleep_interval_ts - Time between audio callbacks.
 *    last_fetch_ts - The time of the last stream fetch.
 *    longest_fetch_interval_ts - Longest interval between two fetches.
 *    start_ts - The time when the stream started.
 *    first_missed_cb_ts - The time when the first missed callback happens.
 *    buf_state - State of the buffer from all devices for this stream.
 *    apm_list - List of audio processing module instances.
 *    num_attached_devs - Number of iodevs this stream has attached to.
 *    num_missed_cb - Number of callback schedules have been missed.
 *    queued_frames - Cached value of the number of queued frames in shm.
 *    is_pinned - True if the stream is a pinned stream, false otherwise.
 *    pinned_dev_idx - device the stream is pinned, 0 if none.
 *    triggered - True if already notified TRIGGER_ONLY stream, false otherwise.
 */
struct cras_rstream {
	cras_stream_id_t stream_id;
	enum CRAS_STREAM_TYPE stream_type;
	enum CRAS_CLIENT_TYPE client_type;
	enum CRAS_STREAM_DIRECTION direction;
	uint32_t flags;
	int fd;
	size_t buffer_frames;
	size_t cb_threshold;
	int is_draining;
	struct master_dev_info master_dev;
	struct cras_rclient *client;
	struct cras_audio_shm *shm;
	struct cras_audio_area *audio_area;
	struct cras_audio_format format;
	struct timespec next_cb_ts;
	struct timespec sleep_interval_ts;
	struct timespec last_fetch_ts;
	struct timespec longest_fetch_interval;
	struct timespec start_ts;
	struct timespec first_missed_cb_ts;
	struct buffer_share *buf_state;
	struct cras_apm_list *apm_list;
	int num_attached_devs;
	int num_missed_cb;
	int queued_frames;
	int is_pinned;
	uint32_t pinned_dev_idx;
	int triggered;
	struct cras_rstream *prev, *next;
};

/* Config for creating an rstream.
 *    stream_type - CRAS_STREAM_TYPE.
 *    client_type - CRAS_CLIENT_TYPE.
 *    direction - CRAS_STREAM_OUTPUT or CRAS_STREAM_INPUT.
 *    dev_idx - Pin to this device if != NO_DEVICE.
 *    flags - Any special handling for this stream.
 *    effects - Bit map of effects to be enabled on this stream.
 *    format - The audio format the stream wishes to use.
 *    buffer_frames - Total number of audio frames to buffer.
 *    cb_threshold - # of frames when to request more from the client.
 *    audio_fd - The fd to read/write audio signals to. May be -1 for server
 *               stream. Some functions may mutably borrow the config and move
 *               the fd ownership.
 *    client_shm_fd - The shm fd to use to back the samples area. May be -1.
 *                    Some functions may dup this fd while borrowing the config.
 *    client_shm_size - The size of shm area backed by client_shm_fd.
 *    client - The client that owns this stream.
 */
struct cras_rstream_config {
	cras_stream_id_t stream_id;
	enum CRAS_STREAM_TYPE stream_type;
	enum CRAS_CLIENT_TYPE client_type;
	enum CRAS_STREAM_DIRECTION direction;
	uint32_t dev_idx;
	uint32_t flags;
	uint32_t effects;
	const struct cras_audio_format *format;
	size_t buffer_frames;
	size_t cb_threshold;
	int audio_fd;
	int client_shm_fd;
	size_t client_shm_size;
	struct cras_rclient *client;
};

/* Fills cras_rstream_config with given parameters.
 *
 * Args:
 *   audio_fd - The audio fd pointer from client. Its ownership will be moved to
 *              stream_config.
 *   client_shm_fd - The shared memory fd pointer for samples from client. Its
 *                   ownership will be moved to stream_config.
 *   Other args - See comments in struct cras_rstream_config.
 */
void cras_rstream_config_init(
	struct cras_rclient *client, cras_stream_id_t stream_id,
	enum CRAS_STREAM_TYPE stream_type, enum CRAS_CLIENT_TYPE client_type,
	enum CRAS_STREAM_DIRECTION direction, uint32_t dev_idx, uint32_t flags,
	uint32_t effects, const struct cras_audio_format *format,
	size_t buffer_frames, size_t cb_threshold, int *audio_fd,
	int *client_shm_fd, size_t client_shm_size,
	struct cras_rstream_config *stream_config);

/* Fills cras_rstream_config with given parameters and a cras_connect_message.
 *
 * Args:
 *   client - The rclient which handles the connect message.
 *   msg - The cras_connect_message from client.
 *   aud_fd - The audio fd pointer from client. Its ownership will be moved to
 *            stream_config.
 *   client_shm_fd - The shared memory fd pointer for samples from client. Its
 *                   ownership will be moved to stream_config.
 *   remote_format - The remote_format for the config.
 *   stream_config - The cras_rstream_config to be filled.
 */
void cras_rstream_config_init_with_message(
	struct cras_rclient *client, const struct cras_connect_message *msg,
	int *aud_fd, int *client_shm_fd,
	const struct cras_audio_format *remote_format,
	struct cras_rstream_config *stream_config);

/* Cleans up given cras_rstream_config. All fds inside the config will be
 * closed.
 *
 * Args:
 *   stream_config - The config to be cleaned up.
 */
void cras_rstream_config_cleanup(struct cras_rstream_config *stream_config);

/* Creates an rstream.
 * Args:
 *    config - Params for configuration of the new rstream. It's a mutable
 *             borrow.
 *    stream_out - Filled with the newly created stream pointer.
 * Returns:
 *    0 on success, EINVAL if an invalid argument is passed, or ENOMEM if out of
 *    memory.
 */
int cras_rstream_create(struct cras_rstream_config *config,
			struct cras_rstream **stream_out);

/* Destroys an rstream. */
void cras_rstream_destroy(struct cras_rstream *stream);

/* Gets the id of the stream */
static inline cras_stream_id_t
cras_rstream_id(const struct cras_rstream *stream)
{
	return stream->stream_id;
}

/* Gets the total buffer size in frames for the given client stream. */
static inline size_t
cras_rstream_get_buffer_frames(const struct cras_rstream *stream)
{
	return stream->buffer_frames;
}

/* Gets the callback threshold in frames for the given client stream. */
static inline size_t
cras_rstream_get_cb_threshold(const struct cras_rstream *stream)
{
	return stream->cb_threshold;
}

/* Gets the max write size for the stream. */
static inline size_t
cras_rstream_get_max_write_frames(const struct cras_rstream *stream)
{
	if (stream->flags & BULK_AUDIO_OK)
		return cras_rstream_get_buffer_frames(stream);
	return cras_rstream_get_cb_threshold(stream);
}

/* Gets the stream type of this stream. */
static inline enum CRAS_STREAM_TYPE
cras_rstream_get_type(const struct cras_rstream *stream)
{
	return stream->stream_type;
}

/* Gets the direction (input/output/loopback) of the stream. */
static inline enum CRAS_STREAM_DIRECTION
cras_rstream_get_direction(const struct cras_rstream *stream)
{
	return stream->direction;
}

/* Gets the format for the stream. */
static inline void cras_rstream_set_format(struct cras_rstream *stream,
					   const struct cras_audio_format *fmt)
{
	stream->format = *fmt;
}

/* Sets the format for the stream. */
static inline int cras_rstream_get_format(const struct cras_rstream *stream,
					  struct cras_audio_format *fmt)
{
	*fmt = stream->format;
	return 0;
}

/* Gets the fd to be used to poll this client for audio. */
static inline int cras_rstream_get_audio_fd(const struct cras_rstream *stream)
{
	return stream->fd;
}

/* Gets the is_draning flag. */
static inline int
cras_rstream_get_is_draining(const struct cras_rstream *stream)
{
	return stream->is_draining;
}

/* Sets the is_draning flag. */
static inline void cras_rstream_set_is_draining(struct cras_rstream *stream,
						int is_draining)
{
	stream->is_draining = is_draining;
}

/* Gets the shm fds used for the stream shm */
static inline int cras_rstream_get_shm_fds(const struct cras_rstream *stream,
					   int *header_fd, int *samples_fd)
{
	if (!header_fd || !samples_fd)
		return -EINVAL;

	*header_fd = stream->shm->header_info.fd;
	*samples_fd = stream->shm->samples_info.fd;

	return 0;
}

/* Gets the size of the shm area used for samples for this stream. */
static inline size_t
cras_rstream_get_samples_shm_size(const struct cras_rstream *stream)
{
	return cras_shm_samples_size(stream->shm);
}

/* Gets shared memory region for this stream. */
static inline struct cras_audio_shm *
cras_rstream_shm(struct cras_rstream *stream)
{
	return stream->shm;
}

/* Checks if the stream uses an output device. */
static inline int stream_uses_output(const struct cras_rstream *s)
{
	return cras_stream_uses_output_hw(s->direction);
}

/* Checks if the stream uses an input device. */
static inline int stream_uses_input(const struct cras_rstream *s)
{
	return cras_stream_uses_input_hw(s->direction);
}

static inline int stream_is_server_only(const struct cras_rstream *s)
{
	return s->flags & SERVER_ONLY;
}

/* Gets the enabled effects of this stream. */
unsigned int cras_rstream_get_effects(const struct cras_rstream *stream);

/* Gets the format of data after stream specific processing. */
struct cras_audio_format *
cras_rstream_post_processing_format(const struct cras_rstream *stream,
				    void *dev_ptr);

/* Checks how much time has passed since last stream fetch and records
 * the longest fetch interval. */
void cras_rstream_record_fetch_interval(struct cras_rstream *rstream,
					const struct timespec *now);

/* Requests min_req frames from the client. */
int cras_rstream_request_audio(struct cras_rstream *stream,
			       const struct timespec *now);

/* Tells a capture client that count frames are ready. */
int cras_rstream_audio_ready(struct cras_rstream *stream, size_t count);

/* Let the rstream know when a device is added or removed. */
void cras_rstream_dev_attach(struct cras_rstream *rstream, unsigned int dev_id,
			     void *dev_ptr);
void cras_rstream_dev_detach(struct cras_rstream *rstream, unsigned int dev_id);

/* A device using this stream has read or written samples. */
void cras_rstream_dev_offset_update(struct cras_rstream *rstream,
				    unsigned int frames, unsigned int dev_id);

void cras_rstream_update_input_write_pointer(struct cras_rstream *rstream);
void cras_rstream_update_output_read_pointer(struct cras_rstream *rstream);

unsigned int cras_rstream_dev_offset(const struct cras_rstream *rstream,
				     unsigned int dev_id);

static inline unsigned int cras_rstream_level(struct cras_rstream *rstream)
{
	const struct cras_audio_shm *shm = cras_rstream_shm(rstream);
	return cras_shm_frames_written(shm);
}

static inline int cras_rstream_input_level_met(struct cras_rstream *rstream)
{
	const struct cras_audio_shm *shm = cras_rstream_shm(rstream);
	return cras_shm_frames_written(shm) >= rstream->cb_threshold;
}

/* Updates the number of queued frames in shm. The queued frames should be
 * updated everytime before calling cras_rstream_playable_frames.
 */
void cras_rstream_update_queued_frames(struct cras_rstream *rstream);

/* Returns the number of playable samples in shm for the given device id. */
unsigned int cras_rstream_playable_frames(struct cras_rstream *rstream,
					  unsigned int dev_id);

/* Returns the volume scaler for this stream. */
float cras_rstream_get_volume_scaler(struct cras_rstream *rstream);

/* Returns a pointer to readable frames, fills frames with the number of frames
 * available. */
uint8_t *cras_rstream_get_readable_frames(struct cras_rstream *rstream,
					  unsigned int offset, size_t *frames);

/* Returns non-zero if the stream is muted. */
int cras_rstream_get_mute(const struct cras_rstream *rstream);

/*
 * Returns non-zero if the stream is pending a reply from client.
 * - For playback, stream is waiting for AUDIO_MESSAGE_DATA_READY message from
 *   client.
 * - For capture, stream is waiting for AUDIO_MESSAGE_DATA_CAPTURED message
 *   from client.
 */
int cras_rstream_is_pending_reply(const struct cras_rstream *stream);

/*
 * Reads any pending audio message from the socket.
 */
int cras_rstream_flush_old_audio_messages(struct cras_rstream *stream);

#endif /* CRAS_RSTREAM_H_ */
