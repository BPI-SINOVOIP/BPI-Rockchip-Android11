/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>
#include <sys/param.h>
#include <syslog.h>

#include "cras_audio_area.h"
#include "cras_config.h"
#include "cras_iodev.h"
#include "cras_iodev_list.h"
#include "cras_rstream.h"
#include "cras_types.h"
#include "utlist.h"

#define EMPTY_BUFFER_SIZE (16 * 1024)
#define EMPTY_FRAME_SIZE 4
#define EMPTY_FRAMES (EMPTY_BUFFER_SIZE / EMPTY_FRAME_SIZE)

static size_t empty_supported_rates[] = { 44100, 48000, 0 };

static size_t empty_supported_channel_counts[] = { 1, 2, 0 };

static snd_pcm_format_t empty_supported_formats[] = {
	SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S24_LE, SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_S24_3LE, 0
};

struct empty_iodev {
	struct cras_iodev base;
	uint8_t *audio_buffer;
	uint64_t read_frames, written_frames;
	struct timespec dev_start_time;
};

/*
 * Current level of the audio buffer.  This is made up based on what has been
 * read/written and how long it has been since the start. Simulates audio
 * hardware running at the given sample rate.
 */
static unsigned int current_level(const struct cras_iodev *iodev)
{
	struct empty_iodev *empty_iodev = (struct empty_iodev *)iodev;
	uint64_t frames_since_start, nframes;

	if (iodev->active_node->type == CRAS_NODE_TYPE_HOTWORD)
		return 0;

	frames_since_start = cras_frames_since_time(
		&empty_iodev->dev_start_time, iodev->format->frame_rate);

	if (iodev->direction == CRAS_STREAM_INPUT) {
		nframes = frames_since_start - empty_iodev->read_frames;
		return MIN(nframes, EMPTY_FRAMES);
	}

	/* output */
	if (empty_iodev->written_frames <= frames_since_start)
		return 0;
	return empty_iodev->written_frames - frames_since_start;
}

/*
 * iodev callbacks.
 */

static int frames_queued(const struct cras_iodev *iodev,
			 struct timespec *tstamp)
{
	clock_gettime(CLOCK_MONOTONIC_RAW, tstamp);
	return current_level(iodev);
}

static int delay_frames(const struct cras_iodev *iodev)
{
	return 0;
}

static int close_dev(struct cras_iodev *iodev)
{
	struct empty_iodev *empty_iodev = (struct empty_iodev *)iodev;

	free(empty_iodev->audio_buffer);
	empty_iodev->audio_buffer = NULL;
	cras_iodev_free_audio_area(iodev);
	return 0;
}

static int configure_dev(struct cras_iodev *iodev)
{
	struct empty_iodev *empty_iodev = (struct empty_iodev *)iodev;

	if (iodev->format == NULL)
		return -EINVAL;

	cras_iodev_init_audio_area(iodev, iodev->format->num_channels);
	empty_iodev->audio_buffer = calloc(1, EMPTY_BUFFER_SIZE);
	empty_iodev->read_frames = 0;
	empty_iodev->written_frames = 0;

	clock_gettime(CLOCK_MONOTONIC_RAW, &empty_iodev->dev_start_time);

	return 0;
}

static int get_buffer(struct cras_iodev *iodev, struct cras_audio_area **area,
		      unsigned *frames)
{
	struct empty_iodev *empty_iodev = (struct empty_iodev *)iodev;
	unsigned int avail, current;

	if (iodev->direction == CRAS_STREAM_OUTPUT) {
		avail = EMPTY_FRAMES - current_level(iodev);
		*frames = MIN(*frames, avail);
	} else {
		current = current_level(iodev);
		*frames = MIN(*frames, current);
	}

	iodev->area->frames = *frames;
	cras_audio_area_config_buf_pointers(iodev->area, iodev->format,
					    empty_iodev->audio_buffer);
	*area = iodev->area;
	return 0;
}

/*
 * Returns -EPIPE if there are not enough frames or spaces in device buffer.
 * It matches other alsa-based devices.
 */
static int put_buffer(struct cras_iodev *iodev, unsigned frames)
{
	struct empty_iodev *empty_iodev = (struct empty_iodev *)iodev;

	if (iodev->direction == CRAS_STREAM_INPUT) {
		if (current_level(iodev) < frames)
			return -EPIPE;
		empty_iodev->read_frames += frames;
	} else {
		if (EMPTY_FRAMES - current_level(iodev) < frames)
			return -EPIPE;
		empty_iodev->written_frames += frames;
	}
	return 0;
}

static int flush_buffer(struct cras_iodev *iodev)
{
	struct empty_iodev *empty_iodev = (struct empty_iodev *)iodev;

	if (iodev->direction == CRAS_STREAM_INPUT)
		empty_iodev->read_frames = 0;
	else
		empty_iodev->written_frames = 0;

	clock_gettime(CLOCK_MONOTONIC_RAW, &empty_iodev->dev_start_time);
	return 0;
}

static void update_active_node(struct cras_iodev *iodev, unsigned node_idx,
			       unsigned dev_enabled)
{
}

/*
 * Exported Interface.
 */

struct cras_iodev *empty_iodev_create(enum CRAS_STREAM_DIRECTION direction,
				      enum CRAS_NODE_TYPE node_type)
{
	struct empty_iodev *empty_iodev;
	struct cras_iodev *iodev;
	struct cras_ionode *node;

	if (direction != CRAS_STREAM_INPUT && direction != CRAS_STREAM_OUTPUT)
		return NULL;

	empty_iodev = calloc(1, sizeof(*empty_iodev));
	if (empty_iodev == NULL)
		return NULL;
	iodev = &empty_iodev->base;
	iodev->direction = direction;

	iodev->supported_rates = empty_supported_rates;
	iodev->supported_channel_counts = empty_supported_channel_counts;
	iodev->supported_formats = empty_supported_formats;
	iodev->buffer_size = EMPTY_FRAMES;

	iodev->configure_dev = configure_dev;
	iodev->close_dev = close_dev;
	iodev->frames_queued = frames_queued;
	iodev->delay_frames = delay_frames;
	iodev->get_buffer = get_buffer;
	iodev->put_buffer = put_buffer;
	iodev->flush_buffer = flush_buffer;
	iodev->update_active_node = update_active_node;
	iodev->no_stream = cras_iodev_default_no_stream_playback;

	/* Create a dummy ionode */
	node = (struct cras_ionode *)calloc(1, sizeof(*node));
	node->dev = iodev;
	node->type = node_type;
	node->volume = 100;
	strcpy(node->name, "(default)");
	cras_iodev_add_node(iodev, node);
	cras_iodev_set_active_node(iodev, node);

	/* Finally add it to the appropriate iodev list. */
	if (direction == CRAS_STREAM_INPUT) {
		if (node->type == CRAS_NODE_TYPE_HOTWORD) {
			snprintf(iodev->info.name, ARRAY_SIZE(iodev->info.name),
				 "Silent hotword device.");
			iodev->info.name[ARRAY_SIZE(iodev->info.name) - 1] =
				'\0';
			iodev->info.idx = SILENT_HOTWORD_DEVICE;
		} else {
			snprintf(iodev->info.name, ARRAY_SIZE(iodev->info.name),
				 "Silent record device.");
			iodev->info.name[ARRAY_SIZE(iodev->info.name) - 1] =
				'\0';
			iodev->info.idx = SILENT_RECORD_DEVICE;
		}
	} else {
		snprintf(iodev->info.name, ARRAY_SIZE(iodev->info.name),
			 "Silent playback device.");
		iodev->info.name[ARRAY_SIZE(iodev->info.name) - 1] = '\0';
		iodev->info.idx = SILENT_PLAYBACK_DEVICE;
	}

	return iodev;
}

void empty_iodev_destroy(struct cras_iodev *iodev)
{
	struct empty_iodev *empty_iodev = (struct empty_iodev *)iodev;

	if (iodev->direction == CRAS_STREAM_INPUT)
		cras_iodev_list_rm_input(iodev);
	else
		cras_iodev_list_rm_output(iodev);
	free(iodev->active_node);
	cras_iodev_free_resources(iodev);
	free(empty_iodev);
}
