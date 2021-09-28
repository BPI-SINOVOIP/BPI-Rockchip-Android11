/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <syslog.h>

#include "audio_thread.h"
#include "cras_empty_iodev.h"
#include "cras_iodev.h"
#include "cras_iodev_info.h"
#include "cras_iodev_list.h"
#include "cras_loopback_iodev.h"
#include "cras_observer.h"
#include "cras_rstream.h"
#include "cras_server.h"
#include "cras_tm.h"
#include "cras_types.h"
#include "cras_system_state.h"
#include "server_stream.h"
#include "stream_list.h"
#include "test_iodev.h"
#include "utlist.h"

const struct timespec idle_timeout_interval = { .tv_sec = 10, .tv_nsec = 0 };

/* Linked list of available devices. */
struct iodev_list {
	struct cras_iodev *iodevs;
	size_t size;
};

/* List of enabled input/output devices.
 *    dev - The device.
 *    init_timer - Timer for a delayed call to init this iodev.
 */
struct enabled_dev {
	struct cras_iodev *dev;
	struct enabled_dev *prev, *next;
};

struct dev_init_retry {
	int dev_idx;
	struct cras_timer *init_timer;
	struct dev_init_retry *next, *prev;
};

struct device_enabled_cb {
	device_enabled_callback_t enabled_cb;
	device_disabled_callback_t disabled_cb;
	void *cb_data;
	struct device_enabled_cb *next, *prev;
};

/* Lists for devs[CRAS_STREAM_INPUT] and devs[CRAS_STREAM_OUTPUT]. */
static struct iodev_list devs[CRAS_NUM_DIRECTIONS];
/* The observer client iodev_list used to listen on various events. */
static struct cras_observer_client *list_observer;
/* Keep a list of enabled inputs and outputs. */
static struct enabled_dev *enabled_devs[CRAS_NUM_DIRECTIONS];
/* Keep an empty device per direction. */
static struct cras_iodev *fallback_devs[CRAS_NUM_DIRECTIONS];
/* Special empty device for hotword streams. */
static struct cras_iodev *empty_hotword_dev;
/* Loopback devices. */
static struct cras_iodev *loopdev_post_mix;
static struct cras_iodev *loopdev_post_dsp;
/* List of pending device init retries. */
static struct dev_init_retry *init_retries;

/* Keep a constantly increasing index for iodevs. Index 0 is reserved
 * to mean "no device". */
static uint32_t next_iodev_idx = MAX_SPECIAL_DEVICE_IDX;

/* Call when a device is enabled or disabled. */
struct device_enabled_cb *device_enable_cbs;

/* Thread that handles audio input and output. */
static struct audio_thread *audio_thread;
/* List of all streams. */
static struct stream_list *stream_list;
/* Idle device timer. */
static struct cras_timer *idle_timer;
/* Flag to indicate that the stream list is disconnected from audio thread. */
static int stream_list_suspended = 0;
/* If init device failed, retry after 1 second. */
static const unsigned int INIT_DEV_DELAY_MS = 1000;
/* Flag to indicate that hotword streams are suspended. */
static int hotword_suspended = 0;

static void idle_dev_check(struct cras_timer *timer, void *data);

static struct cras_iodev *find_dev(size_t dev_index)
{
	struct cras_iodev *dev;

	DL_FOREACH (devs[CRAS_STREAM_OUTPUT].iodevs, dev)
		if (dev->info.idx == dev_index)
			return dev;

	DL_FOREACH (devs[CRAS_STREAM_INPUT].iodevs, dev)
		if (dev->info.idx == dev_index)
			return dev;

	return NULL;
}

static struct cras_ionode *find_node(struct cras_iodev *iodev,
				     unsigned int node_idx)
{
	struct cras_ionode *node;
	DL_SEARCH_SCALAR(iodev->nodes, node, idx, node_idx);
	return node;
}

/* Adds a device to the list.  Used from add_input and add_output. */
static int add_dev_to_list(struct cras_iodev *dev)
{
	struct cras_iodev *tmp;
	uint32_t new_idx;
	struct iodev_list *list = &devs[dev->direction];

	DL_FOREACH (list->iodevs, tmp)
		if (tmp == dev)
			return -EEXIST;

	dev->format = NULL;
	dev->format = NULL;
	dev->prev = dev->next = NULL;

	/* Move to the next index and make sure it isn't taken. */
	new_idx = next_iodev_idx;
	while (1) {
		if (new_idx < MAX_SPECIAL_DEVICE_IDX)
			new_idx = MAX_SPECIAL_DEVICE_IDX;
		DL_SEARCH_SCALAR(list->iodevs, tmp, info.idx, new_idx);
		if (tmp == NULL)
			break;
		new_idx++;
	}
	dev->info.idx = new_idx;
	next_iodev_idx = new_idx + 1;
	list->size++;

	syslog(LOG_INFO, "Adding %s dev at index %u.",
	       dev->direction == CRAS_STREAM_OUTPUT ? "output" : "input",
	       dev->info.idx);
	DL_PREPEND(list->iodevs, dev);

	cras_iodev_list_update_device_list();
	return 0;
}

/* Removes a device to the list.  Used from rm_input and rm_output. */
static int rm_dev_from_list(struct cras_iodev *dev)
{
	struct cras_iodev *tmp;

	DL_FOREACH (devs[dev->direction].iodevs, tmp)
		if (tmp == dev) {
			if (cras_iodev_is_open(dev))
				return -EBUSY;
			DL_DELETE(devs[dev->direction].iodevs, dev);
			devs[dev->direction].size--;
			return 0;
		}

	/* Device not found. */
	return -EINVAL;
}

/* Fills a dev_info array from the iodev_list. */
static void fill_dev_list(struct iodev_list *list,
			  struct cras_iodev_info *dev_info, size_t out_size)
{
	int i = 0;
	struct cras_iodev *tmp;
	DL_FOREACH (list->iodevs, tmp) {
		memcpy(&dev_info[i], &tmp->info, sizeof(dev_info[0]));
		i++;
		if (i == out_size)
			return;
	}
}

static const char *node_type_to_str(struct cras_ionode *node)
{
	switch (node->type) {
	case CRAS_NODE_TYPE_INTERNAL_SPEAKER:
		return "INTERNAL_SPEAKER";
	case CRAS_NODE_TYPE_HEADPHONE:
		return "HEADPHONE";
	case CRAS_NODE_TYPE_HDMI:
		return "HDMI";
	case CRAS_NODE_TYPE_HAPTIC:
		return "HAPTIC";
	case CRAS_NODE_TYPE_MIC:
		switch (node->position) {
		case NODE_POSITION_INTERNAL:
			return "INTERNAL_MIC";
		case NODE_POSITION_FRONT:
			return "FRONT_MIC";
		case NODE_POSITION_REAR:
			return "REAR_MIC";
		case NODE_POSITION_KEYBOARD:
			return "KEYBOARD_MIC";
		case NODE_POSITION_EXTERNAL:
		default:
			return "MIC";
		}
	case CRAS_NODE_TYPE_HOTWORD:
		return "HOTWORD";
	case CRAS_NODE_TYPE_LINEOUT:
		return "LINEOUT";
	case CRAS_NODE_TYPE_POST_MIX_PRE_DSP:
		return "POST_MIX_LOOPBACK";
	case CRAS_NODE_TYPE_POST_DSP:
		return "POST_DSP_LOOPBACK";
	case CRAS_NODE_TYPE_USB:
		return "USB";
	case CRAS_NODE_TYPE_BLUETOOTH:
		return "BLUETOOTH";
	case CRAS_NODE_TYPE_FALLBACK_NORMAL:
		return "FALLBACK_NORMAL";
	case CRAS_NODE_TYPE_FALLBACK_ABNORMAL:
		return "FALLBACK_ABNORMAL";
	case CRAS_NODE_TYPE_UNKNOWN:
	default:
		return "UNKNOWN";
	}
}

/* Fills an ionode_info array from the iodev_list. */
static int fill_node_list(struct iodev_list *list,
			  struct cras_ionode_info *node_info, size_t out_size)
{
	int i = 0;
	struct cras_iodev *dev;
	struct cras_ionode *node;
	DL_FOREACH (list->iodevs, dev) {
		DL_FOREACH (dev->nodes, node) {
			node_info->iodev_idx = dev->info.idx;
			node_info->ionode_idx = node->idx;
			node_info->plugged = node->plugged;
			node_info->plugged_time.tv_sec =
				node->plugged_time.tv_sec;
			node_info->plugged_time.tv_usec =
				node->plugged_time.tv_usec;
			node_info->active =
				dev->is_enabled && (dev->active_node == node);
			node_info->volume = node->volume;
			node_info->capture_gain = node->capture_gain;
			node_info->left_right_swapped =
				node->left_right_swapped;
			node_info->stable_id = node->stable_id;
			strcpy(node_info->mic_positions, node->mic_positions);
			strcpy(node_info->name, node->name);
			strcpy(node_info->active_hotword_model,
			       node->active_hotword_model);
			snprintf(node_info->type, sizeof(node_info->type), "%s",
				 node_type_to_str(node));
			node_info->type_enum = node->type;
			node_info++;
			i++;
			if (i == out_size)
				return i;
		}
	}
	return i;
}

/* Copies the info for each device in the list to "list_out". */
static int get_dev_list(struct iodev_list *list,
			struct cras_iodev_info **list_out)
{
	struct cras_iodev_info *dev_info;

	if (!list_out)
		return list->size;

	*list_out = NULL;
	if (list->size == 0)
		return 0;

	dev_info = malloc(sizeof(*list_out[0]) * list->size);
	if (dev_info == NULL)
		return -ENOMEM;

	fill_dev_list(list, dev_info, list->size);

	*list_out = dev_info;
	return list->size;
}

/* Called when the system volume changes.  Pass the current volume setting to
 * the default output if it is active. */
static void sys_vol_change(void *context, int32_t volume)
{
	struct cras_iodev *dev;

	DL_FOREACH (devs[CRAS_STREAM_OUTPUT].iodevs, dev) {
		if (dev->set_volume && cras_iodev_is_open(dev))
			dev->set_volume(dev);
	}
}

/* Called when the system mute state changes.  Pass the current mute setting
 * to the default output if it is active. */
static void sys_mute_change(void *context, int muted, int user_muted,
			    int mute_locked)
{
	struct cras_iodev *dev;
	int should_mute = muted || user_muted;

	DL_FOREACH (devs[CRAS_STREAM_OUTPUT].iodevs, dev) {
		if (!cras_iodev_is_open(dev)) {
			/* For closed devices, just set its mute state. */
			cras_iodev_set_mute(dev);
		} else {
			audio_thread_dev_start_ramp(
				audio_thread, dev->info.idx,
				(should_mute ?
					 CRAS_IODEV_RAMP_REQUEST_DOWN_MUTE :
					 CRAS_IODEV_RAMP_REQUEST_UP_UNMUTE));
		}
	}
}

static void remove_all_streams_from_dev(struct cras_iodev *dev)
{
	struct cras_rstream *rstream;

	audio_thread_rm_open_dev(audio_thread, dev->direction, dev->info.idx);

	DL_FOREACH (stream_list_get(stream_list), rstream) {
		if (rstream->apm_list == NULL)
			continue;
		cras_apm_list_remove(rstream->apm_list, dev);
	}
}

/*
 * If output dev has an echo reference dev associated, add a server
 * stream to read audio data from it so APM can analyze.
 */
static void possibly_enable_echo_reference(struct cras_iodev *dev)
{
	if (dev->direction != CRAS_STREAM_OUTPUT)
		return;

	if (dev->echo_reference_dev == NULL)
		return;

	server_stream_create(stream_list, dev->echo_reference_dev->info.idx);
}

/*
 * If output dev has an echo reference dev associated, check if there
 * is server stream opened for it and remove it.
 */
static void possibly_disable_echo_reference(struct cras_iodev *dev)
{
	if (dev->echo_reference_dev == NULL)
		return;

	server_stream_destroy(stream_list, dev->echo_reference_dev->info.idx);
}

/*
 * Close dev if it's opened, without the extra call to idle_dev_check.
 * This is useful for closing a dev inside idle_dev_check function to
 * avoid infinite recursive call.
 *
 * Returns:
 *    -EINVAL if device was not opened, otherwise return 0.
 */
static int close_dev_without_idle_check(struct cras_iodev *dev)
{
	if (!cras_iodev_is_open(dev))
		return -EINVAL;

	remove_all_streams_from_dev(dev);
	dev->idle_timeout.tv_sec = 0;
	cras_iodev_close(dev);
	possibly_disable_echo_reference(dev);
	return 0;
}

static void close_dev(struct cras_iodev *dev)
{
	if (close_dev_without_idle_check(dev))
		return;

	if (idle_timer)
		cras_tm_cancel_timer(cras_system_state_get_tm(), idle_timer);
	idle_dev_check(NULL, NULL);
}

static void idle_dev_check(struct cras_timer *timer, void *data)
{
	struct enabled_dev *edev;
	struct timespec now;
	struct timespec min_idle_expiration;
	unsigned int num_idle_devs = 0;
	unsigned int min_idle_timeout_ms;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	min_idle_expiration.tv_sec = 0;
	min_idle_expiration.tv_nsec = 0;

	DL_FOREACH (enabled_devs[CRAS_STREAM_OUTPUT], edev) {
		if (edev->dev->idle_timeout.tv_sec == 0)
			continue;
		if (timespec_after(&now, &edev->dev->idle_timeout)) {
			close_dev_without_idle_check(edev->dev);
			continue;
		}
		num_idle_devs++;
		if (min_idle_expiration.tv_sec == 0 ||
		    timespec_after(&min_idle_expiration,
				   &edev->dev->idle_timeout))
			min_idle_expiration = edev->dev->idle_timeout;
	}

	idle_timer = NULL;
	if (!num_idle_devs)
		return;
	if (timespec_after(&now, &min_idle_expiration)) {
		min_idle_timeout_ms = 0;
	} else {
		struct timespec timeout;
		subtract_timespecs(&min_idle_expiration, &now, &timeout);
		min_idle_timeout_ms = timespec_to_ms(&timeout);
	}
	/* Wake up when it is time to close the next idle device.  Sleep for a
	 * minimum of 10 milliseconds. */
	idle_timer = cras_tm_create_timer(cras_system_state_get_tm(),
					  MAX(min_idle_timeout_ms, 10),
					  idle_dev_check, NULL);
}

/*
 * Cancel pending init tries. Called at device initialization or when device
 * is disabled.
 */
static void cancel_pending_init_retries(unsigned int dev_idx)
{
	struct dev_init_retry *retry;

	DL_FOREACH (init_retries, retry) {
		if (retry->dev_idx != dev_idx)
			continue;
		cras_tm_cancel_timer(cras_system_state_get_tm(),
				     retry->init_timer);
		DL_DELETE(init_retries, retry);
		free(retry);
	}
}

/* Open the device potentially filling the output with a pre buffer. */
static int init_device(struct cras_iodev *dev, struct cras_rstream *rstream)
{
	int rc;

	cras_iodev_exit_idle(dev);

	if (cras_iodev_is_open(dev))
		return 0;
	cancel_pending_init_retries(dev->info.idx);

	rc = cras_iodev_open(dev, rstream->cb_threshold, &rstream->format);
	if (rc)
		return rc;

	rc = audio_thread_add_open_dev(audio_thread, dev);
	if (rc)
		cras_iodev_close(dev);

	possibly_enable_echo_reference(dev);

	return rc;
}

static void suspend_devs()
{
	struct enabled_dev *edev;
	struct cras_rstream *rstream;

	DL_FOREACH (stream_list_get(stream_list), rstream) {
		if (rstream->is_pinned) {
			struct cras_iodev *dev;

			if ((rstream->flags & HOTWORD_STREAM) == HOTWORD_STREAM)
				continue;

			dev = find_dev(rstream->pinned_dev_idx);
			if (dev) {
				audio_thread_disconnect_stream(audio_thread,
							       rstream, dev);
				if (!cras_iodev_list_dev_is_enabled(dev))
					close_dev(dev);
			}
		} else {
			audio_thread_disconnect_stream(audio_thread, rstream,
						       NULL);
		}
	}
	stream_list_suspended = 1;

	DL_FOREACH (enabled_devs[CRAS_STREAM_OUTPUT], edev) {
		close_dev(edev->dev);
	}
	DL_FOREACH (enabled_devs[CRAS_STREAM_INPUT], edev) {
		close_dev(edev->dev);
	}
}

static int stream_added_cb(struct cras_rstream *rstream);

static void resume_devs()
{
	struct cras_rstream *rstream;

	stream_list_suspended = 0;
	DL_FOREACH (stream_list_get(stream_list), rstream) {
		if ((rstream->flags & HOTWORD_STREAM) == HOTWORD_STREAM)
			continue;
		stream_added_cb(rstream);
	}
}

/* Called when the system audio is suspended or resumed. */
void sys_suspend_change(void *arg, int suspended)
{
	if (suspended)
		suspend_devs();
	else
		resume_devs();
}

/* Called when the system capture gain changes.  Pass the current capture_gain
 * setting to the default input if it is active. */
void sys_cap_gain_change(void *context, int32_t gain)
{
	struct cras_iodev *dev;

	DL_FOREACH (devs[CRAS_STREAM_INPUT].iodevs, dev) {
		if (dev->set_capture_gain && cras_iodev_is_open(dev))
			dev->set_capture_gain(dev);
	}
}

/* Called when the system capture mute state changes.  Pass the current capture
 * mute setting to the default input if it is active. */
static void sys_cap_mute_change(void *context, int muted, int mute_locked)
{
	struct cras_iodev *dev;

	DL_FOREACH (devs[CRAS_STREAM_INPUT].iodevs, dev) {
		if (dev->set_capture_mute && cras_iodev_is_open(dev))
			dev->set_capture_mute(dev);
	}
}

static int disable_device(struct enabled_dev *edev, bool force);
static int enable_device(struct cras_iodev *dev);

static void possibly_disable_fallback(enum CRAS_STREAM_DIRECTION dir)
{
	struct enabled_dev *edev;

	DL_FOREACH (enabled_devs[dir], edev) {
		if (edev->dev == fallback_devs[dir])
			disable_device(edev, false);
	}
}

/*
 * Possibly enables fallback device to handle streams.
 * dir - output or input.
 * error - true if enable fallback device because no other iodevs can be
 * initialized successfully.
 */
static void possibly_enable_fallback(enum CRAS_STREAM_DIRECTION dir, bool error)
{
	if (fallback_devs[dir] == NULL)
		return;

	/*
	 * The fallback device is a special device. It doesn't have a real
	 * device to get a correct node type. Therefore, we need to set it by
	 * ourselves, which indicates the reason to use this device.
	 * NORMAL - Use it because of nodes changed.
	 * ABNORMAL - Use it because there are no other usable devices.
	 */
	if (error)
		syslog(LOG_ERR,
		       "Enable fallback device because there are no other usable devices.");

	fallback_devs[dir]->active_node->type =
		error ? CRAS_NODE_TYPE_FALLBACK_ABNORMAL :
			CRAS_NODE_TYPE_FALLBACK_NORMAL;
	if (!cras_iodev_list_dev_is_enabled(fallback_devs[dir]))
		enable_device(fallback_devs[dir]);
}

/*
 * Adds stream to one or more open iodevs. If the stream has processing effect
 * turned on, create new APM instance and add to the list. This makes sure the
 * time consuming APM creation happens in main thread.
 */
static int add_stream_to_open_devs(struct cras_rstream *stream,
				   struct cras_iodev **iodevs,
				   unsigned int num_iodevs)
{
	int i;
	if (stream->apm_list) {
		for (i = 0; i < num_iodevs; i++)
			cras_apm_list_add(stream->apm_list, iodevs[i],
					  iodevs[i]->format);
	}
	return audio_thread_add_stream(audio_thread, stream, iodevs,
				       num_iodevs);
}

static int init_and_attach_streams(struct cras_iodev *dev)
{
	int rc;
	enum CRAS_STREAM_DIRECTION dir = dev->direction;
	struct cras_rstream *stream;
	int dev_enabled = cras_iodev_list_dev_is_enabled(dev);

	/* If called after suspend, for example bluetooth
	 * profile switching, don't add back the stream list. */
	if (stream_list_suspended)
		return 0;

	/* If there are active streams to attach to this device,
	 * open it. */
	DL_FOREACH (stream_list_get(stream_list), stream) {
		if (stream->direction != dir)
			continue;
		/*
		 * Don't attach this stream if (1) this stream pins to a
		 * different device, or (2) this is a normal stream, but
		 * device is not enabled.
		 */
		if (stream->is_pinned) {
			if (stream->pinned_dev_idx != dev->info.idx)
				continue;
		} else if (!dev_enabled) {
			continue;
		}

		rc = init_device(dev, stream);
		if (rc) {
			syslog(LOG_ERR, "Enable %s failed, rc = %d",
			       dev->info.name, rc);
			return rc;
		}
		add_stream_to_open_devs(stream, &dev, 1);
	}
	return 0;
}

static void init_device_cb(struct cras_timer *timer, void *arg)
{
	int rc;
	struct dev_init_retry *retry = (struct dev_init_retry *)arg;
	struct cras_iodev *dev = find_dev(retry->dev_idx);

	/*
	 * First of all, remove retry record to avoid confusion to the
	 * actual device init work.
	 */
	DL_DELETE(init_retries, retry);
	free(retry);

	if (!dev || cras_iodev_is_open(dev))
		return;

	rc = init_and_attach_streams(dev);
	if (rc < 0)
		syslog(LOG_ERR, "Init device retry failed");
	else
		possibly_disable_fallback(dev->direction);
}

static int schedule_init_device_retry(struct cras_iodev *dev)
{
	struct dev_init_retry *retry;
	struct cras_tm *tm = cras_system_state_get_tm();

	retry = (struct dev_init_retry *)calloc(1, sizeof(*retry));
	if (!retry)
		return -ENOMEM;

	retry->dev_idx = dev->info.idx;
	retry->init_timer = cras_tm_create_timer(tm, INIT_DEV_DELAY_MS,
						 init_device_cb, retry);
	DL_APPEND(init_retries, retry);
	return 0;
}

static int init_pinned_device(struct cras_iodev *dev,
			      struct cras_rstream *rstream)
{
	int rc;

	cras_iodev_exit_idle(dev);

	if (audio_thread_is_dev_open(audio_thread, dev))
		return 0;

	/* Make sure the active node is configured properly, it could be
	 * disabled when last normal stream removed. */
	dev->update_active_node(dev, dev->active_node->idx, 1);

	/* Negative EAGAIN code indicates dev will be opened later. */
	rc = init_device(dev, rstream);
	if (rc)
		return rc;
	return 0;
}

static int close_pinned_device(struct cras_iodev *dev)
{
	close_dev(dev);
	dev->update_active_node(dev, dev->active_node->idx, 0);
	return 0;
}

static struct cras_iodev *find_pinned_device(struct cras_rstream *rstream)
{
	struct cras_iodev *dev;
	if (!rstream->is_pinned)
		return NULL;

	dev = find_dev(rstream->pinned_dev_idx);

	if ((rstream->flags & HOTWORD_STREAM) != HOTWORD_STREAM)
		return dev;

	/* Double check node type for hotword stream */
	if (dev && dev->active_node->type != CRAS_NODE_TYPE_HOTWORD) {
		syslog(LOG_ERR, "Hotword stream pinned to invalid dev %u",
		       dev->info.idx);
		return NULL;
	}

	return hotword_suspended ? empty_hotword_dev : dev;
}

static int pinned_stream_added(struct cras_rstream *rstream)
{
	struct cras_iodev *dev;
	int rc;

	/* Check that the target device is valid for pinned streams. */
	dev = find_pinned_device(rstream);
	if (!dev)
		return -EINVAL;

	rc = init_pinned_device(dev, rstream);
	if (rc) {
		syslog(LOG_INFO, "init_pinned_device failed, rc %d", rc);
		return schedule_init_device_retry(dev);
	}

	return add_stream_to_open_devs(rstream, &dev, 1);
}

static int stream_added_cb(struct cras_rstream *rstream)
{
	struct enabled_dev *edev;
	struct cras_iodev *iodevs[10];
	unsigned int num_iodevs;
	int rc;

	if (stream_list_suspended)
		return 0;

	if (rstream->is_pinned)
		return pinned_stream_added(rstream);

	/* Add the new stream to all enabled iodevs at once to avoid offset
	 * in shm level between different ouput iodevs. */
	num_iodevs = 0;
	DL_FOREACH (enabled_devs[rstream->direction], edev) {
		if (num_iodevs >= ARRAY_SIZE(iodevs)) {
			syslog(LOG_ERR, "too many enabled devices");
			break;
		}

		rc = init_device(edev->dev, rstream);
		if (rc) {
			/* Error log but don't return error here, because
			 * stopping audio could block video playback.
			 */
			syslog(LOG_ERR, "Init %s failed, rc = %d",
			       edev->dev->info.name, rc);
			schedule_init_device_retry(edev->dev);
			continue;
		}

		iodevs[num_iodevs++] = edev->dev;
	}
	if (num_iodevs) {
		rc = add_stream_to_open_devs(rstream, iodevs, num_iodevs);
		if (rc) {
			syslog(LOG_ERR, "adding stream to thread fail");
			return rc;
		}
	} else {
		/* Enable fallback device if no other iodevs can be initialized
		 * successfully.
		 * For error codes like EAGAIN and ENOENT, a new iodev will be
		 * enabled soon so streams are going to route there. As for the
		 * rest of the error cases, silence will be played or recorded
		 * so client won't be blocked.
		 * The enabled fallback device will be disabled when
		 * cras_iodev_list_select_node() is called to re-select the
		 * active node.
		 */
		possibly_enable_fallback(rstream->direction, true);
	}
	return 0;
}

static int possibly_close_enabled_devs(enum CRAS_STREAM_DIRECTION dir)
{
	struct enabled_dev *edev;
	const struct cras_rstream *s;

	/* Check if there are still default streams attached. */
	DL_FOREACH (stream_list_get(stream_list), s) {
		if (s->direction == dir && !s->is_pinned)
			return 0;
	}

	/* No more default streams, close any device that doesn't have a stream
	 * pinned to it. */
	DL_FOREACH (enabled_devs[dir], edev) {
		if (stream_list_has_pinned_stream(stream_list,
						  edev->dev->info.idx))
			continue;
		if (dir == CRAS_STREAM_INPUT) {
			close_dev(edev->dev);
			continue;
		}
		/* Allow output devs to drain before closing. */
		clock_gettime(CLOCK_MONOTONIC_RAW, &edev->dev->idle_timeout);
		add_timespecs(&edev->dev->idle_timeout, &idle_timeout_interval);
		idle_dev_check(NULL, NULL);
	}

	return 0;
}

static void pinned_stream_removed(struct cras_rstream *rstream)
{
	struct cras_iodev *dev;

	dev = find_pinned_device(rstream);
	if (!dev)
		return;
	if (!cras_iodev_list_dev_is_enabled(dev) &&
	    !stream_list_has_pinned_stream(stream_list, dev->info.idx))
		close_pinned_device(dev);
}

/* Returns the number of milliseconds left to drain this stream.  This is passed
 * directly from the audio thread. */
static int stream_removed_cb(struct cras_rstream *rstream)
{
	enum CRAS_STREAM_DIRECTION direction = rstream->direction;
	int rc;

	rc = audio_thread_drain_stream(audio_thread, rstream);
	if (rc)
		return rc;

	if (rstream->is_pinned)
		pinned_stream_removed(rstream);

	possibly_close_enabled_devs(direction);

	return 0;
}

static int enable_device(struct cras_iodev *dev)
{
	int rc;
	struct enabled_dev *edev;
	enum CRAS_STREAM_DIRECTION dir = dev->direction;
	struct device_enabled_cb *callback;

	DL_FOREACH (enabled_devs[dir], edev) {
		if (edev->dev == dev)
			return -EEXIST;
	}

	edev = calloc(1, sizeof(*edev));
	edev->dev = dev;
	DL_APPEND(enabled_devs[dir], edev);
	dev->is_enabled = 1;

	rc = init_and_attach_streams(dev);
	if (rc < 0) {
		syslog(LOG_INFO, "Enable device fail, rc %d", rc);
		schedule_init_device_retry(dev);
		return rc;
	}

	DL_FOREACH (device_enable_cbs, callback)
		callback->enabled_cb(dev, callback->cb_data);

	return 0;
}

/* Set `force to true to flush any pinned streams before closing the device. */
static int disable_device(struct enabled_dev *edev, bool force)
{
	struct cras_iodev *dev = edev->dev;
	enum CRAS_STREAM_DIRECTION dir = dev->direction;
	struct cras_rstream *stream;
	struct device_enabled_cb *callback;

	/*
	 * Remove from enabled dev list. However this dev could have a stream
	 * pinned to it, only cancel pending init timers when force flag is set.
	 */
	DL_DELETE(enabled_devs[dir], edev);
	free(edev);
	dev->is_enabled = 0;
	if (force)
		cancel_pending_init_retries(dev->info.idx);

	/*
	 * Pull all default streams off this device.
	 * Pull all pinned streams off as well if force is true.
	 */
	DL_FOREACH (stream_list_get(stream_list), stream) {
		if (stream->direction != dev->direction)
			continue;
		if (stream->is_pinned && !force)
			continue;
		audio_thread_disconnect_stream(audio_thread, stream, dev);
	}
	/* If this is a force disable call, that guarantees pinned streams have
	 * all been detached. Otherwise check with stream_list to see if
	 * there's still a pinned stream using this device.
	 */
	if (!force && stream_list_has_pinned_stream(stream_list, dev->info.idx))
		return 0;
	DL_FOREACH (device_enable_cbs, callback)
		callback->disabled_cb(dev, callback->cb_data);
	close_dev(dev);
	dev->update_active_node(dev, dev->active_node->idx, 0);

	return 0;
}

/*
 * Assume the device is not in enabled_devs list.
 * Assume there is no default stream on the device.
 * An example is that this device is unplugged while it is playing
 * a pinned stream. The device and stream may have been removed in
 * audio thread due to I/O error handling.
 */
static int force_close_pinned_only_device(struct cras_iodev *dev)
{
	struct cras_rstream *rstream;

	/* Pull pinned streams off this device. Note that this is initiated
	 * from server side, so the pin stream still exist in stream_list
	 * pending client side to actually remove it.
	 */
	DL_FOREACH (stream_list_get(stream_list), rstream) {
		if (rstream->direction != dev->direction)
			continue;
		if (!rstream->is_pinned)
			continue;
		if (dev->info.idx != rstream->pinned_dev_idx)
			continue;
		audio_thread_disconnect_stream(audio_thread, rstream, dev);
	}

	close_dev(dev);
	dev->update_active_node(dev, dev->active_node->idx, 0);
	return 0;
}

/*
 * Exported Interface.
 */

void cras_iodev_list_init()
{
	struct cras_observer_ops observer_ops;

	memset(&observer_ops, 0, sizeof(observer_ops));
	observer_ops.output_volume_changed = sys_vol_change;
	observer_ops.output_mute_changed = sys_mute_change;
	observer_ops.capture_gain_changed = sys_cap_gain_change;
	observer_ops.capture_mute_changed = sys_cap_mute_change;
	observer_ops.suspend_changed = sys_suspend_change;
	list_observer = cras_observer_add(&observer_ops, NULL);
	idle_timer = NULL;

	/* Create the audio stream list for the system. */
	stream_list =
		stream_list_create(stream_added_cb, stream_removed_cb,
				   cras_rstream_create, cras_rstream_destroy,
				   cras_system_state_get_tm());

	/* Add an empty device so there is always something to play to or
	 * capture from. */
	fallback_devs[CRAS_STREAM_OUTPUT] = empty_iodev_create(
		CRAS_STREAM_OUTPUT, CRAS_NODE_TYPE_FALLBACK_NORMAL);
	fallback_devs[CRAS_STREAM_INPUT] = empty_iodev_create(
		CRAS_STREAM_INPUT, CRAS_NODE_TYPE_FALLBACK_NORMAL);
	enable_device(fallback_devs[CRAS_STREAM_OUTPUT]);
	enable_device(fallback_devs[CRAS_STREAM_INPUT]);

	empty_hotword_dev =
		empty_iodev_create(CRAS_STREAM_INPUT, CRAS_NODE_TYPE_HOTWORD);

	/* Create loopback devices. */
	loopdev_post_mix = loopback_iodev_create(LOOPBACK_POST_MIX_PRE_DSP);
	loopdev_post_dsp = loopback_iodev_create(LOOPBACK_POST_DSP);

	audio_thread = audio_thread_create();
	if (!audio_thread) {
		syslog(LOG_ERR, "Fatal: audio thread init");
		exit(-ENOMEM);
	}
	audio_thread_start(audio_thread);

	cras_iodev_list_update_device_list();
}

void cras_iodev_list_deinit()
{
	audio_thread_destroy(audio_thread);
	loopback_iodev_destroy(loopdev_post_dsp);
	loopback_iodev_destroy(loopdev_post_mix);
	empty_iodev_destroy(empty_hotword_dev);
	empty_iodev_destroy(fallback_devs[CRAS_STREAM_INPUT]);
	empty_iodev_destroy(fallback_devs[CRAS_STREAM_OUTPUT]);
	stream_list_destroy(stream_list);
	if (list_observer) {
		cras_observer_remove(list_observer);
		list_observer = NULL;
	}
}

int cras_iodev_list_dev_is_enabled(const struct cras_iodev *dev)
{
	struct enabled_dev *edev;

	DL_FOREACH (enabled_devs[dev->direction], edev) {
		if (edev->dev == dev)
			return 1;
	}

	return 0;
}

void cras_iodev_list_enable_dev(struct cras_iodev *dev)
{
	possibly_disable_fallback(dev->direction);
	/* Enable ucm setting of active node. */
	dev->update_active_node(dev, dev->active_node->idx, 1);
	enable_device(dev);
	cras_iodev_list_notify_active_node_changed(dev->direction);
}

void cras_iodev_list_add_active_node(enum CRAS_STREAM_DIRECTION dir,
				     cras_node_id_t node_id)
{
	struct cras_iodev *new_dev;
	new_dev = find_dev(dev_index_of(node_id));
	if (!new_dev || new_dev->direction != dir)
		return;

	/* If the new dev is already enabled but its active node needs to be
	 * changed. Disable new dev first, update active node, and then
	 * re-enable it again.
	 */
	if (cras_iodev_list_dev_is_enabled(new_dev)) {
		if (node_index_of(node_id) == new_dev->active_node->idx)
			return;
		else
			cras_iodev_list_disable_dev(new_dev, true);
	}

	new_dev->update_active_node(new_dev, node_index_of(node_id), 1);
	cras_iodev_list_enable_dev(new_dev);
}

/*
 * Disables device which may or may not be in enabled_devs list.
 */
void cras_iodev_list_disable_dev(struct cras_iodev *dev, bool force_close)
{
	struct enabled_dev *edev, *edev_to_disable = NULL;

	int is_the_only_enabled_device = 1;

	DL_FOREACH (enabled_devs[dev->direction], edev) {
		if (edev->dev == dev)
			edev_to_disable = edev;
		else
			is_the_only_enabled_device = 0;
	}

	/*
	 * Disables the device for these two cases:
	 * 1. Disable a device in the enabled_devs list.
	 * 2. Force close a device that is not in the enabled_devs list,
	 *    but it is running a pinned stream.
	 */
	if (!edev_to_disable) {
		if (force_close)
			force_close_pinned_only_device(dev);
		return;
	}

	/* If the device to be closed is the only enabled device, we should
	 * enable the fallback device first then disable the target
	 * device. */
	if (is_the_only_enabled_device && fallback_devs[dev->direction])
		enable_device(fallback_devs[dev->direction]);

	disable_device(edev_to_disable, force_close);

	cras_iodev_list_notify_active_node_changed(dev->direction);
	return;
}

void cras_iodev_list_suspend_dev(unsigned int dev_idx)
{
	struct cras_rstream *rstream;
	struct cras_iodev *dev = find_dev(dev_idx);

	if (!dev)
		return;

	DL_FOREACH (stream_list_get(stream_list), rstream) {
		if (rstream->direction != dev->direction)
			continue;
		/* Disconnect all streams that are either:
		 * (1) normal stream while dev is enabled by UI, or
		 * (2) stream specifically pins to this dev.
		 */
		if ((dev->is_enabled && !rstream->is_pinned) ||
		    (rstream->is_pinned &&
		     (dev->info.idx != rstream->pinned_dev_idx)))
			audio_thread_disconnect_stream(audio_thread, rstream,
						       dev);
	}
	close_dev(dev);
	dev->update_active_node(dev, dev->active_node->idx, 0);
}

void cras_iodev_list_resume_dev(unsigned int dev_idx)
{
	struct cras_iodev *dev = find_dev(dev_idx);
	int rc;

	if (!dev)
		return;

	dev->update_active_node(dev, dev->active_node->idx, 1);
	rc = init_and_attach_streams(dev);
	if (rc == 0) {
		/* If dev initialize succeeded and this is not a pinned device,
		 * disable the silent fallback device because it's just
		 * unnecessary. */
		if (!stream_list_has_pinned_stream(stream_list, dev_idx))
			possibly_disable_fallback(dev->direction);
	} else {
		syslog(LOG_INFO, "Enable dev fail at resume, rc %d", rc);
		schedule_init_device_retry(dev);
	}
}

void cras_iodev_list_set_dev_mute(unsigned int dev_idx)
{
	struct cras_iodev *dev;

	dev = find_dev(dev_idx);
	if (!dev)
		return;

	cras_iodev_set_mute(dev);
}

void cras_iodev_list_rm_active_node(enum CRAS_STREAM_DIRECTION dir,
				    cras_node_id_t node_id)
{
	struct cras_iodev *dev;

	dev = find_dev(dev_index_of(node_id));
	if (!dev)
		return;

	cras_iodev_list_disable_dev(dev, false);
}

int cras_iodev_list_add_output(struct cras_iodev *output)
{
	int rc;

	if (output->direction != CRAS_STREAM_OUTPUT)
		return -EINVAL;

	rc = add_dev_to_list(output);
	if (rc)
		return rc;

	return 0;
}

int cras_iodev_list_add_input(struct cras_iodev *input)
{
	int rc;

	if (input->direction != CRAS_STREAM_INPUT)
		return -EINVAL;

	rc = add_dev_to_list(input);
	if (rc)
		return rc;

	return 0;
}

int cras_iodev_list_rm_output(struct cras_iodev *dev)
{
	int res;

	/* Retire the current active output device before removing it from
	 * list, otherwise it could be busy and remain in the list.
	 */
	cras_iodev_list_disable_dev(dev, true);
	res = rm_dev_from_list(dev);
	if (res == 0)
		cras_iodev_list_update_device_list();
	return res;
}

int cras_iodev_list_rm_input(struct cras_iodev *dev)
{
	int res;

	/* Retire the current active input device before removing it from
	 * list, otherwise it could be busy and remain in the list.
	 */
	cras_iodev_list_disable_dev(dev, true);
	res = rm_dev_from_list(dev);
	if (res == 0)
		cras_iodev_list_update_device_list();
	return res;
}

int cras_iodev_list_get_outputs(struct cras_iodev_info **list_out)
{
	return get_dev_list(&devs[CRAS_STREAM_OUTPUT], list_out);
}

int cras_iodev_list_get_inputs(struct cras_iodev_info **list_out)
{
	return get_dev_list(&devs[CRAS_STREAM_INPUT], list_out);
}

struct cras_iodev *
cras_iodev_list_get_first_enabled_iodev(enum CRAS_STREAM_DIRECTION direction)
{
	struct enabled_dev *edev = enabled_devs[direction];

	return edev ? edev->dev : NULL;
}

struct cras_iodev *
cras_iodev_list_get_sco_pcm_iodev(enum CRAS_STREAM_DIRECTION direction)
{
	struct cras_iodev *dev;
	struct cras_ionode *node;

	DL_FOREACH (devs[direction].iodevs, dev) {
		DL_FOREACH (dev->nodes, node) {
			if (node->is_sco_pcm)
				return dev;
		}
	}

	return NULL;
}

cras_node_id_t
cras_iodev_list_get_active_node_id(enum CRAS_STREAM_DIRECTION direction)
{
	struct enabled_dev *edev = enabled_devs[direction];

	if (!edev || !edev->dev || !edev->dev->active_node)
		return 0;

	return cras_make_node_id(edev->dev->info.idx,
				 edev->dev->active_node->idx);
}

void cras_iodev_list_update_device_list()
{
	struct cras_server_state *state;

	state = cras_system_state_update_begin();
	if (!state)
		return;

	state->num_output_devs = devs[CRAS_STREAM_OUTPUT].size;
	state->num_input_devs = devs[CRAS_STREAM_INPUT].size;
	fill_dev_list(&devs[CRAS_STREAM_OUTPUT], &state->output_devs[0],
		      CRAS_MAX_IODEVS);
	fill_dev_list(&devs[CRAS_STREAM_INPUT], &state->input_devs[0],
		      CRAS_MAX_IODEVS);

	state->num_output_nodes =
		fill_node_list(&devs[CRAS_STREAM_OUTPUT],
			       &state->output_nodes[0], CRAS_MAX_IONODES);
	state->num_input_nodes =
		fill_node_list(&devs[CRAS_STREAM_INPUT], &state->input_nodes[0],
			       CRAS_MAX_IONODES);

	cras_system_state_update_complete();
}

/* Look up the first hotword stream and the device it pins to. */
int find_hotword_stream_dev(struct cras_iodev **dev,
			    struct cras_rstream **stream)
{
	DL_FOREACH (stream_list_get(stream_list), *stream) {
		if (((*stream)->flags & HOTWORD_STREAM) != HOTWORD_STREAM)
			continue;

		*dev = find_dev((*stream)->pinned_dev_idx);
		if (*dev == NULL)
			return -ENOENT;
		break;
	}
	return 0;
}

/* Suspend/resume hotword streams functions are used to provide seamless
 * experience to cras clients when there's hardware limitation about concurrent
 * DSP and normal recording. The empty hotword iodev is used to hold all
 * hotword streams during suspend, so client side will not know about the
 * transition, and can still remove or add streams. At resume, the real hotword
 * device will be initialized and opened again to re-arm the DSP.
 */
int cras_iodev_list_suspend_hotword_streams()
{
	struct cras_iodev *hotword_dev;
	struct cras_rstream *stream = NULL;
	int rc;

	rc = find_hotword_stream_dev(&hotword_dev, &stream);
	if (rc)
		return rc;

	if (stream == NULL) {
		hotword_suspended = 1;
		return 0;
	}
	/* Move all existing hotword streams to the empty hotword iodev. */
	init_pinned_device(empty_hotword_dev, stream);
	DL_FOREACH (stream_list_get(stream_list), stream) {
		if ((stream->flags & HOTWORD_STREAM) != HOTWORD_STREAM)
			continue;
		if (stream->pinned_dev_idx != hotword_dev->info.idx) {
			syslog(LOG_ERR,
			       "Failed to suspend hotword stream on dev %u",
			       stream->pinned_dev_idx);
			continue;
		}

		audio_thread_disconnect_stream(audio_thread, stream,
					       hotword_dev);
		audio_thread_add_stream(audio_thread, stream,
					&empty_hotword_dev, 1);
	}
	close_pinned_device(hotword_dev);
	hotword_suspended = 1;
	return 0;
}

int cras_iodev_list_resume_hotword_stream()
{
	struct cras_iodev *hotword_dev;
	struct cras_rstream *stream = NULL;
	int rc;

	rc = find_hotword_stream_dev(&hotword_dev, &stream);
	if (rc)
		return rc;

	if (stream == NULL) {
		hotword_suspended = 0;
		return 0;
	}
	/* Move all existing hotword streams to the real hotword iodev. */
	init_pinned_device(hotword_dev, stream);
	DL_FOREACH (stream_list_get(stream_list), stream) {
		if ((stream->flags & HOTWORD_STREAM) != HOTWORD_STREAM)
			continue;
		if (stream->pinned_dev_idx != hotword_dev->info.idx) {
			syslog(LOG_ERR,
			       "Fail to resume hotword stream on dev %u",
			       stream->pinned_dev_idx);
			continue;
		}

		audio_thread_disconnect_stream(audio_thread, stream,
					       empty_hotword_dev);
		audio_thread_add_stream(audio_thread, stream, &hotword_dev, 1);
	}
	close_pinned_device(empty_hotword_dev);
	hotword_suspended = 0;
	return 0;
}

char *cras_iodev_list_get_hotword_models(cras_node_id_t node_id)
{
	struct cras_iodev *dev = NULL;

	dev = find_dev(dev_index_of(node_id));
	if (!dev || !dev->get_hotword_models ||
	    (dev->active_node->type != CRAS_NODE_TYPE_HOTWORD))
		return NULL;

	return dev->get_hotword_models(dev);
}

int cras_iodev_list_set_hotword_model(cras_node_id_t node_id,
				      const char *model_name)
{
	int ret;
	struct cras_iodev *dev = find_dev(dev_index_of(node_id));
	if (!dev || !dev->get_hotword_models ||
	    (dev->active_node->type != CRAS_NODE_TYPE_HOTWORD))
		return -EINVAL;

	ret = dev->set_hotword_model(dev, model_name);
	if (!ret)
		strncpy(dev->active_node->active_hotword_model, model_name,
			sizeof(dev->active_node->active_hotword_model) - 1);
	return ret;
}

void cras_iodev_list_notify_nodes_changed()
{
	cras_observer_notify_nodes();
}

void cras_iodev_list_notify_active_node_changed(
	enum CRAS_STREAM_DIRECTION direction)
{
	cras_observer_notify_active_node(
		direction, cras_iodev_list_get_active_node_id(direction));
}

void cras_iodev_list_select_node(enum CRAS_STREAM_DIRECTION direction,
				 cras_node_id_t node_id)
{
	struct cras_iodev *new_dev = NULL;
	struct enabled_dev *edev;
	int new_node_already_enabled = 0;
	int rc;

	/* find the devices for the id. */
	new_dev = find_dev(dev_index_of(node_id));

	/* Do nothing if the direction is mismatched. The new_dev == NULL case
	   could happen if node_id is 0 (no selection), or the client tries
	   to select a non-existing node (maybe it's unplugged just before
	   the client selects it). We will just behave like there is no selected
	   node. */
	if (new_dev && new_dev->direction != direction)
		return;

	/* Determine whether the new device and node are already enabled - if
	 * they are, the selection algorithm should avoid disabling the new
	 * device. */
	DL_FOREACH (enabled_devs[direction], edev) {
		if (edev->dev == new_dev &&
		    edev->dev->active_node->idx == node_index_of(node_id)) {
			new_node_already_enabled = 1;
			break;
		}
	}

	/* Enable fallback device during the transition so client will not be
	 * blocked in this duration, which is as long as 300 ms on some boards
	 * before new device is opened.
	 * Note that the fallback node is not needed if the new node is already
	 * enabled - the new node will remain enabled. */
	if (!new_node_already_enabled)
		possibly_enable_fallback(direction, false);

	/* Disable all devices except for fallback device, and the new device,
	 * provided it is already enabled. */
	DL_FOREACH (enabled_devs[direction], edev) {
		if (edev->dev != fallback_devs[direction] &&
		    !(new_node_already_enabled && edev->dev == new_dev)) {
			disable_device(edev, false);
		}
	}

	if (new_dev && !new_node_already_enabled) {
		new_dev->update_active_node(new_dev, node_index_of(node_id), 1);
		rc = enable_device(new_dev);
		if (rc == 0) {
			/* Disable fallback device after new device is enabled.
			 * Leave the fallback device enabled if new_dev failed
			 * to open, or the new_dev == NULL case. */
			possibly_disable_fallback(direction);
		}
	}

	cras_iodev_list_notify_active_node_changed(direction);
}

static int set_node_plugged(struct cras_iodev *iodev, unsigned int node_idx,
			    int plugged)
{
	struct cras_ionode *node;

	node = find_node(iodev, node_idx);
	if (!node)
		return -EINVAL;
	cras_iodev_set_node_plugged(node, plugged);
	return 0;
}

static int set_node_volume(struct cras_iodev *iodev, unsigned int node_idx,
			   int volume)
{
	struct cras_ionode *node;

	node = find_node(iodev, node_idx);
	if (!node)
		return -EINVAL;

	if (iodev->ramp && cras_iodev_software_volume_needed(iodev) &&
	    !cras_system_get_mute())
		cras_iodev_start_volume_ramp(iodev, node->volume, volume);

	node->volume = volume;
	if (iodev->set_volume)
		iodev->set_volume(iodev);
	cras_iodev_list_notify_node_volume(node);
	return 0;
}

static int set_node_capture_gain(struct cras_iodev *iodev,
				 unsigned int node_idx, int capture_gain)
{
	struct cras_ionode *node;

	node = find_node(iodev, node_idx);
	if (!node)
		return -EINVAL;

	node->capture_gain = capture_gain;
	if (iodev->set_capture_gain)
		iodev->set_capture_gain(iodev);
	cras_iodev_list_notify_node_capture_gain(node);
	return 0;
}

static int set_node_left_right_swapped(struct cras_iodev *iodev,
				       unsigned int node_idx,
				       int left_right_swapped)
{
	struct cras_ionode *node;
	int rc;

	if (!iodev->set_swap_mode_for_node)
		return -EINVAL;
	node = find_node(iodev, node_idx);
	if (!node)
		return -EINVAL;

	rc = iodev->set_swap_mode_for_node(iodev, node, left_right_swapped);
	if (rc) {
		syslog(LOG_ERR, "Failed to set swap mode on node %s to %d",
		       node->name, left_right_swapped);
		return rc;
	}
	node->left_right_swapped = left_right_swapped;
	cras_iodev_list_notify_node_left_right_swapped(node);
	return 0;
}

int cras_iodev_list_set_node_attr(cras_node_id_t node_id, enum ionode_attr attr,
				  int value)
{
	struct cras_iodev *iodev;
	int rc = 0;

	iodev = find_dev(dev_index_of(node_id));
	if (!iodev)
		return -EINVAL;

	switch (attr) {
	case IONODE_ATTR_PLUGGED:
		rc = set_node_plugged(iodev, node_index_of(node_id), value);
		break;
	case IONODE_ATTR_VOLUME:
		rc = set_node_volume(iodev, node_index_of(node_id), value);
		break;
	case IONODE_ATTR_CAPTURE_GAIN:
		rc = set_node_capture_gain(iodev, node_index_of(node_id),
					   value);
		break;
	case IONODE_ATTR_SWAP_LEFT_RIGHT:
		rc = set_node_left_right_swapped(iodev, node_index_of(node_id),
						 value);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

void cras_iodev_list_notify_node_volume(struct cras_ionode *node)
{
	cras_node_id_t id = cras_make_node_id(node->dev->info.idx, node->idx);
	cras_iodev_list_update_device_list();
	cras_observer_notify_output_node_volume(id, node->volume);
}

void cras_iodev_list_notify_node_left_right_swapped(struct cras_ionode *node)
{
	cras_node_id_t id = cras_make_node_id(node->dev->info.idx, node->idx);
	cras_iodev_list_update_device_list();
	cras_observer_notify_node_left_right_swapped(id,
						     node->left_right_swapped);
}

void cras_iodev_list_notify_node_capture_gain(struct cras_ionode *node)
{
	cras_node_id_t id = cras_make_node_id(node->dev->info.idx, node->idx);
	cras_iodev_list_update_device_list();
	cras_observer_notify_input_node_gain(id, node->capture_gain);
}

void cras_iodev_list_add_test_dev(enum TEST_IODEV_TYPE type)
{
	if (type != TEST_IODEV_HOTWORD)
		return;
	test_iodev_create(CRAS_STREAM_INPUT, type);
}

void cras_iodev_list_test_dev_command(unsigned int iodev_idx,
				      enum CRAS_TEST_IODEV_CMD command,
				      unsigned int data_len,
				      const uint8_t *data)
{
	struct cras_iodev *dev = find_dev(iodev_idx);

	if (!dev)
		return;

	test_iodev_command(dev, command, data_len, data);
}

struct audio_thread *cras_iodev_list_get_audio_thread()
{
	return audio_thread;
}

struct stream_list *cras_iodev_list_get_stream_list()
{
	return stream_list;
}

int cras_iodev_list_set_device_enabled_callback(
	device_enabled_callback_t enabled_cb,
	device_disabled_callback_t disabled_cb, void *cb_data)
{
	struct device_enabled_cb *callback;

	DL_FOREACH (device_enable_cbs, callback) {
		if (callback->cb_data != cb_data)
			continue;

		DL_DELETE(device_enable_cbs, callback);
		free(callback);
	}

	if (enabled_cb && disabled_cb) {
		callback = (struct device_enabled_cb *)calloc(
			1, sizeof(*callback));
		callback->enabled_cb = enabled_cb;
		callback->disabled_cb = disabled_cb;
		callback->cb_data = cb_data;
		DL_APPEND(device_enable_cbs, callback);
	}

	return 0;
}

void cras_iodev_list_register_loopback(enum CRAS_LOOPBACK_TYPE loopback_type,
				       unsigned int output_dev_idx,
				       loopback_hook_data_t hook_data,
				       loopback_hook_control_t hook_control,
				       unsigned int loopback_dev_idx)
{
	struct cras_iodev *iodev = find_dev(output_dev_idx);
	struct cras_iodev *loopback_dev;
	struct cras_loopback *loopback;
	bool dev_open;

	if (iodev == NULL) {
		syslog(LOG_ERR, "Output dev %u not found for loopback",
		       output_dev_idx);
		return;
	}

	loopback_dev = find_dev(loopback_dev_idx);
	if (loopback_dev == NULL) {
		syslog(LOG_ERR, "Loopback dev %u not found", loopback_dev_idx);
		return;
	}

	dev_open = cras_iodev_is_open(iodev);

	loopback = (struct cras_loopback *)calloc(1, sizeof(*loopback));
	if (NULL == loopback) {
		syslog(LOG_ERR, "Not enough memory for loopback");
		return;
	}

	loopback->type = loopback_type;
	loopback->hook_data = hook_data;
	loopback->hook_control = hook_control;
	loopback->cb_data = loopback_dev;
	if (loopback->hook_control && dev_open)
		loopback->hook_control(true, loopback->cb_data);

	DL_APPEND(iodev->loopbacks, loopback);
}

void cras_iodev_list_unregister_loopback(enum CRAS_LOOPBACK_TYPE type,
					 unsigned int output_dev_idx,
					 unsigned int loopback_dev_idx)
{
	struct cras_iodev *iodev = find_dev(output_dev_idx);
	struct cras_iodev *loopback_dev;
	struct cras_loopback *loopback;

	if (iodev == NULL)
		return;

	loopback_dev = find_dev(loopback_dev_idx);
	if (loopback_dev == NULL)
		return;

	DL_FOREACH (iodev->loopbacks, loopback) {
		if ((loopback->cb_data == loopback_dev) &&
		    (loopback->type == type)) {
			DL_DELETE(iodev->loopbacks, loopback);
			free(loopback);
		}
	}
}

void cras_iodev_list_reset()
{
	struct enabled_dev *edev;

	DL_FOREACH (enabled_devs[CRAS_STREAM_OUTPUT], edev) {
		DL_DELETE(enabled_devs[CRAS_STREAM_OUTPUT], edev);
		free(edev);
	}
	enabled_devs[CRAS_STREAM_OUTPUT] = NULL;
	DL_FOREACH (enabled_devs[CRAS_STREAM_INPUT], edev) {
		DL_DELETE(enabled_devs[CRAS_STREAM_INPUT], edev);
		free(edev);
	}
	enabled_devs[CRAS_STREAM_INPUT] = NULL;
	devs[CRAS_STREAM_OUTPUT].iodevs = NULL;
	devs[CRAS_STREAM_INPUT].iodevs = NULL;
	devs[CRAS_STREAM_OUTPUT].size = 0;
	devs[CRAS_STREAM_INPUT].size = 0;
}
