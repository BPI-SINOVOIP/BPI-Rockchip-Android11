/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/socket.h>
#include <sys/time.h>
#include <syslog.h>

#include "cras_audio_area.h"
#include "cras_hfp_slc.h"
#include "cras_iodev.h"
#include "cras_system_state.h"
#include "cras_util.h"
#include "sfh.h"
#include "utlist.h"
#include "cras_bt_device.h"

#include "cras_hfp_alsa_iodev.h"

/* Object to represent a special HFP iodev which would be managed by bt_io but
 * playback/capture via an inner ALSA iodev.
 * Members:
 *    base - The base class cras_iodev.
 *    device - The corresponding remote BT device.
 *    slc - The service level connection.
 *    aio - The effective iodev for playback/capture.
 */
struct hfp_alsa_io {
	struct cras_iodev base;
	struct cras_bt_device *device;
	struct hfp_slc_handle *slc;
	struct cras_iodev *aio;
};

static int hfp_alsa_open_dev(struct cras_iodev *iodev)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	return aio->open_dev(aio);
}

static int hfp_alsa_update_supported_formats(struct cras_iodev *iodev)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;
	int rc, i;

	/* 16 bit, mono, 8kHz (narrow band speech); */
	rc = aio->update_supported_formats(aio);
	if (rc)
		return rc;

	for (i = 0; aio->supported_rates[i]; ++i)
		if (aio->supported_rates[i] == 8000)
			break;
	if (aio->supported_rates[i] != 8000)
		return -EINVAL;

	for (i = 0; aio->supported_channel_counts[i]; ++i)
		if (aio->supported_channel_counts[i] == 1)
			break;
	if (aio->supported_channel_counts[i] != 1)
		return -EINVAL;

	for (i = 0; aio->supported_formats[i]; ++i)
		if (aio->supported_formats[i] == SND_PCM_FORMAT_S16_LE)
			break;
	if (aio->supported_formats[i] != SND_PCM_FORMAT_S16_LE)
		return -EINVAL;

	free(aio->format);
	aio->format = malloc(sizeof(struct cras_audio_format));
	if (!aio->format)
		return -ENOMEM;
	aio->format->format = SND_PCM_FORMAT_S16_LE;
	aio->format->frame_rate = 8000;
	aio->format->num_channels = 1;

	free(iodev->supported_rates);
	iodev->supported_rates = malloc(2 * sizeof(*iodev->supported_rates));
	if (!iodev->supported_rates)
		return -ENOMEM;
	iodev->supported_rates[0] = 8000;
	iodev->supported_rates[1] = 0;

	free(iodev->supported_channel_counts);
	iodev->supported_channel_counts =
		malloc(2 * sizeof(*iodev->supported_channel_counts));
	if (!iodev->supported_channel_counts)
		return -ENOMEM;
	iodev->supported_channel_counts[0] = 1;
	iodev->supported_channel_counts[1] = 0;

	free(iodev->supported_formats);
	iodev->supported_formats =
		malloc(2 * sizeof(*iodev->supported_formats));
	if (!iodev->supported_formats)
		return -ENOMEM;
	iodev->supported_formats[0] = SND_PCM_FORMAT_S16_LE;
	iodev->supported_formats[1] = 0;

	return 0;
}

static int hfp_alsa_configure_dev(struct cras_iodev *iodev)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;
	int rc;

	rc = aio->configure_dev(aio);
	if (rc) {
		syslog(LOG_ERR, "Failed to configure aio: %d\n", rc);
		return rc;
	}

	rc = cras_bt_device_get_sco(
		hfp_alsa_io->device,
		hfp_slc_get_selected_codec(hfp_alsa_io->slc));
	if (rc < 0) {
		syslog(LOG_ERR, "Failed to get sco: %d\n", rc);
		return rc;
	}

	hfp_set_call_status(hfp_alsa_io->slc, 1);
	iodev->buffer_size = aio->buffer_size;

	return 0;
}

static int hfp_alsa_close_dev(struct cras_iodev *iodev)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	cras_bt_device_put_sco(hfp_alsa_io->device);
	cras_iodev_free_format(iodev);
	return aio->close_dev(aio);
}

static int hfp_alsa_frames_queued(const struct cras_iodev *iodev,
				  struct timespec *tstamp)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	return aio->frames_queued(aio, tstamp);
}

static int hfp_alsa_delay_frames(const struct cras_iodev *iodev)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	return aio->delay_frames(aio);
}

static int hfp_alsa_get_buffer(struct cras_iodev *iodev,
			       struct cras_audio_area **area, unsigned *frames)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	return aio->get_buffer(aio, area, frames);
}

static int hfp_alsa_put_buffer(struct cras_iodev *iodev, unsigned nwritten)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	return aio->put_buffer(aio, nwritten);
}

static int hfp_alsa_flush_buffer(struct cras_iodev *iodev)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	return aio->flush_buffer(aio);
}

static void hfp_alsa_update_active_node(struct cras_iodev *iodev,
					unsigned node_idx, unsigned dev_enabled)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	aio->update_active_node(aio, node_idx, dev_enabled);
}

static int hfp_alsa_start(const struct cras_iodev *iodev)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	return aio->start(aio);
}

static void hfp_alsa_set_volume(struct cras_iodev *iodev)
{
	size_t volume;
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;

	volume = cras_system_get_volume();
	if (iodev->active_node)
		volume = cras_iodev_adjust_node_volume(iodev->active_node,
						       volume);

	hfp_event_speaker_gain(hfp_alsa_io->slc, volume);
}

static int hfp_alsa_no_stream(struct cras_iodev *iodev, int enable)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	/*
	 * Copy iodev->min_cb_level and iodev->max_cb_level from the parent
	 * (i.e. hfp_alsa_iodev).  no_stream() of alsa_io will use them.
	 */
	aio->min_cb_level = iodev->min_cb_level;
	aio->max_cb_level = iodev->max_cb_level;
	return aio->no_stream(aio, enable);
}

static int hfp_alsa_is_free_running(const struct cras_iodev *iodev)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_iodev *aio = hfp_alsa_io->aio;

	return aio->is_free_running(aio);
}

struct cras_iodev *hfp_alsa_iodev_create(struct cras_iodev *aio,
					 struct cras_bt_device *device,
					 struct hfp_slc_handle *slc,
					 enum cras_bt_device_profile profile)
{
	struct hfp_alsa_io *hfp_alsa_io;
	struct cras_iodev *iodev;
	struct cras_ionode *node;
	const char *name;

	hfp_alsa_io = calloc(1, sizeof(*hfp_alsa_io));
	if (!hfp_alsa_io)
		return NULL;

	iodev = &hfp_alsa_io->base;
	iodev->direction = aio->direction;

	hfp_alsa_io->device = device;
	hfp_alsa_io->slc = slc;
	hfp_alsa_io->aio = aio;

	/* Set iodev's name to device readable name or the address. */
	name = cras_bt_device_name(device);
	if (!name)
		name = cras_bt_device_object_path(device);
	snprintf(iodev->info.name, sizeof(iodev->info.name), "%s.HFP_PCM",
		 name);
	iodev->info.name[ARRAY_SIZE(iodev->info.name) - 1] = 0;
	iodev->info.stable_id =
		SuperFastHash(cras_bt_device_object_path(device),
			      strlen(cras_bt_device_object_path(device)),
			      strlen(cras_bt_device_object_path(device)));

	iodev->open_dev = hfp_alsa_open_dev;
	iodev->update_supported_formats = hfp_alsa_update_supported_formats;
	iodev->configure_dev = hfp_alsa_configure_dev;
	iodev->close_dev = hfp_alsa_close_dev;

	iodev->frames_queued = hfp_alsa_frames_queued;
	iodev->delay_frames = hfp_alsa_delay_frames;
	iodev->get_buffer = hfp_alsa_get_buffer;
	iodev->put_buffer = hfp_alsa_put_buffer;
	iodev->flush_buffer = hfp_alsa_flush_buffer;

	iodev->update_active_node = hfp_alsa_update_active_node;
	iodev->start = hfp_alsa_start;
	iodev->set_volume = hfp_alsa_set_volume;
	iodev->no_stream = hfp_alsa_no_stream;
	iodev->is_free_running = hfp_alsa_is_free_running;

	iodev->min_buffer_level = aio->min_buffer_level;

	node = calloc(1, sizeof(*node));
	node->dev = iodev;
	strcpy(node->name, iodev->info.name);

	node->plugged = 1;
	node->type = CRAS_NODE_TYPE_BLUETOOTH;
	node->volume = 100;
	gettimeofday(&node->plugged_time, NULL);

	cras_bt_device_append_iodev(device, iodev, profile);
	cras_iodev_add_node(iodev, node);
	cras_iodev_set_active_node(iodev, node);

	return iodev;
}

void hfp_alsa_iodev_destroy(struct cras_iodev *iodev)
{
	struct hfp_alsa_io *hfp_alsa_io = (struct hfp_alsa_io *)iodev;
	struct cras_ionode *node;

	cras_bt_device_rm_iodev(hfp_alsa_io->device, iodev);

	node = iodev->active_node;
	if (node) {
		cras_iodev_rm_node(iodev, node);
		free(node);
	}

	free(iodev->supported_channel_counts);
	free(iodev->supported_rates);
	free(iodev->supported_formats);
	cras_iodev_free_resources(iodev);

	free(hfp_alsa_io);
}
