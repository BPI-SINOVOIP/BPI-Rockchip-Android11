/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>
#include <sys/param.h>
#include <syslog.h>

#include "byte_buffer.h"
#include "cras_audio_area.h"
#include "cras_config.h"
#include "cras_iodev.h"
#include "cras_iodev_list.h"
#include "cras_types.h"
#include "cras_util.h"
#include "sfh.h"
#include "utlist.h"

#define LOOPBACK_BUFFER_SIZE 8192

static const char *loopdev_names[LOOPBACK_NUM_TYPES] = {
	"Post Mix Pre DSP Loopback",
	"Post DSP Loopback",
};

static size_t loopback_supported_rates[] = { 48000, 0 };

static size_t loopback_supported_channel_counts[] = { 2, 0 };

static snd_pcm_format_t loopback_supported_formats[] = {
	SND_PCM_FORMAT_S16_LE,
	0,
};

/* loopack iodev.  Keep state of a loopback device.
 *    loopback_type - Pre-dsp or post-dsp.
 *    read_frames - Frames of audio data read since last dev start.
 *    started - True to indicate the target device is running, otherwise false.
 *    dev_start_time - The timestamp of the last call to configure_dev.
 *    sample_buffer - Pointer to sample buffer.
 *    sender_idx - Index of the output device to read loopback audio.
 */
struct loopback_iodev {
	struct cras_iodev base;
	enum CRAS_LOOPBACK_TYPE loopback_type;
	uint64_t read_frames;
	bool started;
	struct timespec dev_start_time;
	struct byte_buffer *sample_buffer;
	unsigned int sender_idx;
};

static int sample_hook_start(bool start, void *cb_data)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)cb_data;
	loopdev->started = start;
	return 0;
}

/*
 * Called in the put buffer function of the sender that hooked to.
 */
static int sample_hook(const uint8_t *frames, unsigned int nframes,
		       const struct cras_audio_format *fmt, void *cb_data)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)cb_data;
	struct byte_buffer *sbuf = loopdev->sample_buffer;
	unsigned int frame_bytes = cras_get_format_bytes(fmt);
	unsigned int frames_to_copy, bytes_to_copy;

	frames_to_copy = MIN(buf_writable(sbuf) / frame_bytes, nframes);
	if (!frames_to_copy)
		return 0;

	bytes_to_copy = frames_to_copy * frame_bytes;
	memcpy(buf_write_pointer(sbuf), frames, bytes_to_copy);
	buf_increment_write(sbuf, bytes_to_copy);

	return frames_to_copy;
}

static void update_first_output_to_loopback(struct loopback_iodev *loopdev)
{
	struct cras_iodev *edev;

	/* Register loopback hook onto first enabled iodev. */
	edev = cras_iodev_list_get_first_enabled_iodev(CRAS_STREAM_OUTPUT);
	if (edev) {
		loopdev->sender_idx = edev->info.idx;
		cras_iodev_list_register_loopback(
			loopdev->loopback_type, loopdev->sender_idx,
			sample_hook, sample_hook_start, loopdev->base.info.idx);
	}
}

static void device_enabled_hook(struct cras_iodev *iodev, void *cb_data)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)cb_data;

	if (iodev->direction != CRAS_STREAM_OUTPUT)
		return;

	update_first_output_to_loopback(loopdev);
}

static void device_disabled_hook(struct cras_iodev *iodev, void *cb_data)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)cb_data;

	if (loopdev->sender_idx != iodev->info.idx)
		return;

	/* Unregister loopback hook from disabled iodev. */
	cras_iodev_list_unregister_loopback(loopdev->loopback_type,
					    loopdev->sender_idx,
					    loopdev->base.info.idx);
	update_first_output_to_loopback(loopdev);
}

/*
 * iodev callbacks.
 */

static int frames_queued(const struct cras_iodev *iodev,
			 struct timespec *hw_tstamp)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)iodev;
	struct byte_buffer *sbuf = loopdev->sample_buffer;
	unsigned int frame_bytes = cras_get_format_bytes(iodev->format);

	if (!loopdev->started) {
		unsigned int frames_since_start, frames_to_fill, bytes_to_fill;

		frames_since_start = cras_frames_since_time(
			&loopdev->dev_start_time, iodev->format->frame_rate);
		frames_to_fill =
			frames_since_start > loopdev->read_frames ?
				frames_since_start - loopdev->read_frames :
				0;
		frames_to_fill =
			MIN(buf_writable(sbuf) / frame_bytes, frames_to_fill);
		if (frames_to_fill > 0) {
			bytes_to_fill = frames_to_fill * frame_bytes;
			memset(buf_write_pointer(sbuf), 0, bytes_to_fill);
			buf_increment_write(sbuf, bytes_to_fill);
		}
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, hw_tstamp);
	return buf_queued(sbuf) / frame_bytes;
}

static int delay_frames(const struct cras_iodev *iodev)
{
	struct timespec tstamp;

	return frames_queued(iodev, &tstamp);
}

static int close_record_dev(struct cras_iodev *iodev)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)iodev;
	struct byte_buffer *sbuf = loopdev->sample_buffer;

	cras_iodev_free_format(iodev);
	cras_iodev_free_audio_area(iodev);
	buf_reset(sbuf);

	cras_iodev_list_unregister_loopback(loopdev->loopback_type,
					    loopdev->sender_idx,
					    loopdev->base.info.idx);
	loopdev->sender_idx = NO_DEVICE;
	cras_iodev_list_set_device_enabled_callback(NULL, NULL, (void *)iodev);

	return 0;
}

static int configure_record_dev(struct cras_iodev *iodev)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)iodev;
	struct cras_iodev *edev;

	cras_iodev_init_audio_area(iodev, iodev->format->num_channels);
	clock_gettime(CLOCK_MONOTONIC_RAW, &loopdev->dev_start_time);
	loopdev->read_frames = 0;
	loopdev->started = 0;

	edev = cras_iodev_list_get_first_enabled_iodev(CRAS_STREAM_OUTPUT);
	if (edev) {
		loopdev->sender_idx = edev->info.idx;
		cras_iodev_list_register_loopback(
			loopdev->loopback_type, loopdev->sender_idx,
			sample_hook, sample_hook_start, iodev->info.idx);
	}
	cras_iodev_list_set_device_enabled_callback(
		device_enabled_hook, device_disabled_hook, (void *)iodev);

	return 0;
}

static int get_record_buffer(struct cras_iodev *iodev,
			     struct cras_audio_area **area, unsigned *frames)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)iodev;
	struct byte_buffer *sbuf = loopdev->sample_buffer;
	unsigned int frame_bytes = cras_get_format_bytes(iodev->format);
	unsigned int avail_frames = buf_readable(sbuf) / frame_bytes;

	*frames = MIN(avail_frames, *frames);
	iodev->area->frames = *frames;
	cras_audio_area_config_buf_pointers(iodev->area, iodev->format,
					    buf_read_pointer(sbuf));
	*area = iodev->area;

	return 0;
}

static int put_record_buffer(struct cras_iodev *iodev, unsigned nframes)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)iodev;
	struct byte_buffer *sbuf = loopdev->sample_buffer;
	unsigned int frame_bytes = cras_get_format_bytes(iodev->format);

	buf_increment_read(sbuf, nframes * frame_bytes);
	loopdev->read_frames += nframes;
	return 0;
}

static int flush_record_buffer(struct cras_iodev *iodev)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)iodev;
	struct byte_buffer *sbuf = loopdev->sample_buffer;
	unsigned int queued_bytes = buf_queued(sbuf);
	buf_increment_read(sbuf, queued_bytes);
	loopdev->read_frames = 0;
	return 0;
}

static void update_active_node(struct cras_iodev *iodev, unsigned node_idx,
			       unsigned dev_enabled)
{
}

static struct cras_iodev *create_loopback_iodev(enum CRAS_LOOPBACK_TYPE type)
{
	struct loopback_iodev *loopback_iodev;
	struct cras_iodev *iodev;

	loopback_iodev = calloc(1, sizeof(*loopback_iodev));
	if (loopback_iodev == NULL)
		return NULL;

	loopback_iodev->sample_buffer = byte_buffer_create(1024 * 16 * 4);
	if (loopback_iodev->sample_buffer == NULL) {
		free(loopback_iodev);
		return NULL;
	}

	loopback_iodev->loopback_type = type;

	iodev = &loopback_iodev->base;
	iodev->direction = CRAS_STREAM_INPUT;
	snprintf(iodev->info.name, ARRAY_SIZE(iodev->info.name), "%s",
		 loopdev_names[type]);
	iodev->info.name[ARRAY_SIZE(iodev->info.name) - 1] = '\0';
	iodev->info.stable_id =
		SuperFastHash(iodev->info.name, strlen(iodev->info.name),
			      strlen(iodev->info.name));

	iodev->supported_rates = loopback_supported_rates;
	iodev->supported_channel_counts = loopback_supported_channel_counts;
	iodev->supported_formats = loopback_supported_formats;
	iodev->buffer_size = LOOPBACK_BUFFER_SIZE;

	iodev->frames_queued = frames_queued;
	iodev->delay_frames = delay_frames;
	iodev->update_active_node = update_active_node;
	iodev->configure_dev = configure_record_dev;
	iodev->close_dev = close_record_dev;
	iodev->get_buffer = get_record_buffer;
	iodev->put_buffer = put_record_buffer;
	iodev->flush_buffer = flush_record_buffer;

	return iodev;
}

/*
 * Exported Interface.
 */

struct cras_iodev *loopback_iodev_create(enum CRAS_LOOPBACK_TYPE type)
{
	struct cras_iodev *iodev;
	struct cras_ionode *node;
	enum CRAS_NODE_TYPE node_type;

	switch (type) {
	case LOOPBACK_POST_MIX_PRE_DSP:
		node_type = CRAS_NODE_TYPE_POST_MIX_PRE_DSP;
		break;
	case LOOPBACK_POST_DSP:
		node_type = CRAS_NODE_TYPE_POST_DSP;
		break;
	default:
		return NULL;
	}

	iodev = create_loopback_iodev(type);
	if (iodev == NULL)
		return NULL;

	/* Create a dummy ionode */
	node = (struct cras_ionode *)calloc(1, sizeof(*node));
	node->dev = iodev;
	node->type = node_type;
	node->plugged = 1;
	node->volume = 100;
	node->stable_id = iodev->info.stable_id;
	node->software_volume_needed = 0;
	node->max_software_gain = 0;
	strcpy(node->name, loopdev_names[type]);
	cras_iodev_add_node(iodev, node);
	cras_iodev_set_active_node(iodev, node);

	cras_iodev_list_add_input(iodev);

	return iodev;
}

void loopback_iodev_destroy(struct cras_iodev *iodev)
{
	struct loopback_iodev *loopdev = (struct loopback_iodev *)iodev;
	struct byte_buffer *sbuf = loopdev->sample_buffer;

	cras_iodev_list_rm_input(iodev);
	free(iodev->nodes);

	byte_buffer_destroy(&sbuf);
	free(loopdev);
}
