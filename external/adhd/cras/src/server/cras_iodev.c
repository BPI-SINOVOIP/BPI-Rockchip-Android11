/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/time.h>
#include <syslog.h>
#include <time.h>

#include "audio_thread.h"
#include "audio_thread_log.h"
#include "buffer_share.h"
#include "cras_audio_area.h"
#include "cras_audio_thread_monitor.h"
#include "cras_device_monitor.h"
#include "cras_dsp.h"
#include "cras_dsp_pipeline.h"
#include "cras_fmt_conv.h"
#include "cras_iodev.h"
#include "cras_iodev_list.h"
#include "cras_mix.h"
#include "cras_ramp.h"
#include "cras_rstream.h"
#include "cras_server_metrics.h"
#include "cras_system_state.h"
#include "cras_util.h"
#include "dev_stream.h"
#include "input_data.h"
#include "utlist.h"
#include "rate_estimator.h"
#include "softvol_curve.h"

static const float RAMP_UNMUTE_DURATION_SECS = 0.5;
static const float RAMP_NEW_STREAM_DURATION_SECS = 0.01;
static const float RAMP_MUTE_DURATION_SECS = 0.1;
static const float RAMP_VOLUME_CHANGE_DURATION_SECS = 0.1;

/*
 * It is the lastest time for the device to wake up when it is in the normal
 * run state. It represents how many remaining frames in the device buffer.
 */
static const struct timespec dev_normal_run_wake_up_time = {
	0, 1 * 1000 * 1000 /* 1 msec. */
};

/*
 * It is the lastest time for the device to wake up when it is in the no stream
 * state. It represents how many remaining frames in the device buffer.
 */
static const struct timespec dev_no_stream_wake_up_time = {
	0, 5 * 1000 * 1000 /* 5 msec. */
};

/*
 * Check issu b/72496547 and commit message for the history of
 * rate estimator tuning.
 */
static const struct timespec rate_estimation_window_sz = {
	5, 0 /* 5 sec. */
};
static const double rate_estimation_smooth_factor = 0.3f;

static void cras_iodev_alloc_dsp(struct cras_iodev *iodev);

static int default_no_stream_playback(struct cras_iodev *odev)
{
	int rc;
	unsigned int hw_level, fr_to_write;
	unsigned int target_hw_level = odev->min_cb_level * 2;
	struct timespec hw_tstamp;

	/* The default action for no stream playback is to fill zeros. */
	rc = cras_iodev_frames_queued(odev, &hw_tstamp);
	if (rc < 0)
		return rc;
	hw_level = rc;

	/* If underrun happened, handle underrun and get hw_level again. */
	if (hw_level == 0) {
		rc = cras_iodev_output_underrun(odev);
		if (rc < 0)
			return rc;

		rc = cras_iodev_frames_queued(odev, &hw_tstamp);
		if (rc < 0)
			return rc;
		hw_level = rc;
	}

	ATLOG(atlog, AUDIO_THREAD_ODEV_DEFAULT_NO_STREAMS, odev->info.idx,
	      hw_level, target_hw_level);

	fr_to_write = cras_iodev_buffer_avail(odev, hw_level);
	if (hw_level <= target_hw_level) {
		fr_to_write = MIN(target_hw_level - hw_level, fr_to_write);
		return cras_iodev_fill_odev_zeros(odev, fr_to_write);
	}
	return 0;
}

static int cras_iodev_start(struct cras_iodev *iodev)
{
	int rc;
	if (!cras_iodev_is_open(iodev))
		return -EPERM;
	if (!iodev->start) {
		syslog(LOG_ERR,
		       "start called on device %s not supporting start ops",
		       iodev->info.name);
		return -EINVAL;
	}
	rc = iodev->start(iodev);
	if (rc)
		return rc;
	iodev->state = CRAS_IODEV_STATE_NORMAL_RUN;
	return 0;
}

/* Gets the number of frames ready for this device to play.
 * It is the minimum number of available samples in dev_streams.
 */
static unsigned int dev_playback_frames(struct cras_iodev *odev)
{
	struct dev_stream *curr;
	int frames = 0;

	DL_FOREACH (odev->streams, curr) {
		int dev_frames;

		/* Skip stream which hasn't started running yet. */
		if (!dev_stream_is_running(curr))
			continue;

		/* If this is a single output dev stream, updates the latest
		 * number of frames for playback. */
		if (dev_stream_attached_devs(curr) == 1)
			dev_stream_update_frames(curr);

		dev_frames = dev_stream_playback_frames(curr);
		/* Do not handle stream error or end of draining in this
		 * function because they should be handled in write_streams. */
		if (dev_frames < 0)
			continue;
		if (!dev_frames) {
			if (cras_rstream_get_is_draining(curr->stream))
				continue;
			else
				return 0;
		}
		if (frames == 0)
			frames = dev_frames;
		else
			frames = MIN(dev_frames, frames);
	}
	return frames;
}

/* Let device enter/leave no stream playback.
 * Args:
 *    iodev[in] - The output device.
 *    enable[in] - 1 to enter no stream playback, 0 to leave.
 * Returns:
 *    0 on success. Negative error code on failure.
 */
static int cras_iodev_no_stream_playback_transition(struct cras_iodev *odev,
						    int enable)
{
	int rc;

	if (odev->direction != CRAS_STREAM_OUTPUT)
		return -EINVAL;

	/* This function is for transition between normal run and
	 * no stream run state.
	 */
	if ((odev->state != CRAS_IODEV_STATE_NORMAL_RUN) &&
	    (odev->state != CRAS_IODEV_STATE_NO_STREAM_RUN))
		return -EINVAL;

	if (enable) {
		ATLOG(atlog, AUDIO_THREAD_ODEV_NO_STREAMS, odev->info.idx, 0,
		      0);
	} else {
		ATLOG(atlog, AUDIO_THREAD_ODEV_LEAVE_NO_STREAMS, odev->info.idx,
		      0, 0);
	}

	rc = odev->no_stream(odev, enable);
	if (rc < 0)
		return rc;
	if (enable)
		odev->state = CRAS_IODEV_STATE_NO_STREAM_RUN;
	else
		odev->state = CRAS_IODEV_STATE_NORMAL_RUN;
	return 0;
}

/* Determines if the output device should mute. It considers system mute,
 * system volume, and active node volume on the device. */
static int output_should_mute(struct cras_iodev *odev)
{
	/* System mute has highest priority. */
	if (cras_system_get_mute())
		return 1;

	/* consider system volume and active node volume. */
	return cras_iodev_is_zero_volume(odev);
}

int cras_iodev_is_zero_volume(const struct cras_iodev *odev)
{
	size_t system_volume;
	unsigned int adjusted_node_volume;

	system_volume = cras_system_get_volume();
	if (odev->active_node) {
		adjusted_node_volume = cras_iodev_adjust_node_volume(
			odev->active_node, system_volume);
		return (adjusted_node_volume == 0);
	}
	return (system_volume == 0);
}

/* Output device state transition diagram:
 *
 *                           ----------------
 *  -------------<-----------| S0  Closed   |------<-------.
 *  |                        ----------------              |
 *  |                           |   iodev_list enables     |
 *  |                           |   device and adds to     |
 *  |                           V   audio thread           | iodev_list removes
 *  |                        ----------------              | device from
 *  |                        | S1  Open     |              | audio_thread and
 *  |                        ----------------              | closes device
 *  | Device with dummy start       |                      |
 *  | ops transits into             | Sample is ready      |
 *  | no stream state right         V                      |
 *  | after open.            ----------------              |
 *  |                        | S2  Normal   |              |
 *  |                        ----------------              |
 *  |                           |        ^                 |
 *  |       There is no stream  |        | Sample is ready |
 *  |                           V        |                 |
 *  |                        ----------------              |
 *  ------------->-----------| S3 No Stream |------->------
 *                           ----------------
 *
 *  Device in open_devs can be in one of S1, S2, S3.
 *
 * cras_iodev_output_event_sample_ready change device state from S1 or S3 into
 * S2.
 */
static int cras_iodev_output_event_sample_ready(struct cras_iodev *odev)
{
	if (odev->state == CRAS_IODEV_STATE_OPEN ||
	    odev->state == CRAS_IODEV_STATE_NO_STREAM_RUN) {
		/* Starts ramping up if device should not be muted.
		 * Both mute and volume are taken into consideration.
		 */
		if (odev->ramp && !output_should_mute(odev))
			cras_iodev_start_ramp(
				odev,
				CRAS_IODEV_RAMP_REQUEST_UP_START_PLAYBACK);
	}

	if (odev->state == CRAS_IODEV_STATE_OPEN) {
		/* S1 => S2:
		 * If device is not started yet, and there is sample ready from
		 * stream, fill 1 min_cb_level of zeros first and fill sample
		 * from stream later.
		 * Starts the device here to finish state transition. */
		cras_iodev_fill_odev_zeros(odev, odev->min_cb_level);
		ATLOG(atlog, AUDIO_THREAD_ODEV_START, odev->info.idx,
		      odev->min_cb_level, 0);
		return cras_iodev_start(odev);
	} else if (odev->state == CRAS_IODEV_STATE_NO_STREAM_RUN) {
		/* S3 => S2:
		 * Device in no stream state get sample ready. Leave no stream
		 * state and transit to normal run state.*/
		return cras_iodev_no_stream_playback_transition(odev, 0);
	} else {
		syslog(LOG_ERR,
		       "Device %s in state %d received sample ready event",
		       odev->info.name, odev->state);
		return -EINVAL;
	}
	return 0;
}

/*
 * Exported Interface.
 */

/* Finds the supported sample rate that best suits the requested rate, "rrate".
 * Exact matches have highest priority, then integer multiples, then the default
 * rate for the device. */
static size_t get_best_rate(struct cras_iodev *iodev, size_t rrate)
{
	size_t i;
	size_t best;

	if (iodev->supported_rates[0] == 0) /* No rates supported */
		return 0;

	for (i = 0, best = 0; iodev->supported_rates[i] != 0; i++) {
		if (rrate == iodev->supported_rates[i] && rrate >= 44100)
			return rrate;
		if (best == 0 && (rrate % iodev->supported_rates[i] == 0 ||
				  iodev->supported_rates[i] % rrate == 0))
			best = iodev->supported_rates[i];
	}

	if (best)
		return best;
	return iodev->supported_rates[0];
}

/* Finds the best match for the channel count.  The following match rules
 * will apply in order and return the value once matched:
 * 1. Match the exact given channel count.
 * 2. Match the preferred channel count.
 * 3. The first channel count in the list.
 */
static size_t get_best_channel_count(struct cras_iodev *iodev, size_t count)
{
	static const size_t preferred_channel_count = 2;
	size_t i;

	assert(iodev->supported_channel_counts[0] != 0);

	for (i = 0; iodev->supported_channel_counts[i] != 0; i++) {
		if (iodev->supported_channel_counts[i] == count)
			return count;
	}

	/* If provided count is not supported, search for preferred
	 * channel count to which we're good at converting.
	 */
	for (i = 0; iodev->supported_channel_counts[i] != 0; i++) {
		if (iodev->supported_channel_counts[i] ==
		    preferred_channel_count)
			return preferred_channel_count;
	}

	return iodev->supported_channel_counts[0];
}

/* finds the best match for the current format. If no exact match is
 * found, use the first. */
static snd_pcm_format_t get_best_pcm_format(struct cras_iodev *iodev,
					    snd_pcm_format_t fmt)
{
	size_t i;

	for (i = 0; iodev->supported_formats[i] != 0; i++) {
		if (fmt == iodev->supported_formats[i])
			return fmt;
	}

	return iodev->supported_formats[0];
}

/* Applies the DSP to the samples for the iodev if applicable. */
static int apply_dsp(struct cras_iodev *iodev, uint8_t *buf, size_t frames)
{
	struct cras_dsp_context *ctx;
	struct pipeline *pipeline;
	int rc;

	ctx = iodev->dsp_context;
	if (!ctx)
		return 0;

	pipeline = cras_dsp_get_pipeline(ctx);
	if (!pipeline)
		return 0;

	rc = cras_dsp_pipeline_apply(pipeline, buf, iodev->format->format,
				     frames);

	cras_dsp_put_pipeline(ctx);
	return rc;
}

static void cras_iodev_free_dsp(struct cras_iodev *iodev)
{
	if (iodev->dsp_context) {
		cras_dsp_context_free(iodev->dsp_context);
		iodev->dsp_context = NULL;
	}
}

/* Modifies the number of channels in device format to the one that will be
 * presented to the device after any channel changes from the DSP. */
static inline void adjust_dev_channel_for_dsp(const struct cras_iodev *iodev)
{
	struct cras_dsp_context *ctx = iodev->dsp_context;

	if (!ctx || !cras_dsp_get_pipeline(ctx))
		return;

	if (iodev->direction == CRAS_STREAM_OUTPUT)
		iodev->format->num_channels = cras_dsp_num_output_channels(ctx);
	else
		iodev->format->num_channels = cras_dsp_num_input_channels(ctx);

	cras_dsp_put_pipeline(ctx);
}

/* Updates channel layout based on the number of channels set by a
 * client stream. Set a default value to format if the update call
 * fails.
 */
static void update_channel_layout(struct cras_iodev *iodev)
{
	int rc;

	if (iodev->update_channel_layout == NULL)
		return;

	rc = iodev->update_channel_layout(iodev);
	if (rc < 0)
		cras_audio_format_set_default_channel_layout(iodev->format);
}

/*
 * For the specified format, removes any channels from the channel layout that
 * are higher than the supported number of channels. Should be used when the
 * number of channels of the format been reduced.
 */
static void trim_channel_layout(struct cras_audio_format *fmt)
{
	int i;
	for (i = 0; i < CRAS_CH_MAX; i++)
		if (fmt->channel_layout[i] >= fmt->num_channels)
			fmt->channel_layout[i] = -1;
}

int cras_iodev_set_format(struct cras_iodev *iodev,
			  const struct cras_audio_format *fmt)
{
	size_t actual_rate, actual_num_channels;
	snd_pcm_format_t actual_format;
	int rc;

	/* If this device isn't already using a format, try to match the one
	 * requested in "fmt". */
	if (iodev->format == NULL) {
		iodev->format = malloc(sizeof(struct cras_audio_format));
		if (!iodev->format)
			return -ENOMEM;
		*iodev->format = *fmt;

		if (iodev->update_supported_formats) {
			rc = iodev->update_supported_formats(iodev);
			if (rc) {
				syslog(LOG_ERR, "Failed to update formats");
				goto error;
			}
		}

		/* Finds the actual rate of device before allocating DSP
		 * because DSP needs to use the rate of device, not rate of
		 * stream. */
		actual_rate = get_best_rate(iodev, fmt->frame_rate);
		iodev->format->frame_rate = actual_rate;

		cras_iodev_alloc_dsp(iodev);
		cras_iodev_update_dsp(iodev);
		if (iodev->dsp_context)
			adjust_dev_channel_for_dsp(iodev);

		actual_num_channels = get_best_channel_count(
			iodev, iodev->format->num_channels);
		actual_format = get_best_pcm_format(iodev, fmt->format);
		if (actual_rate == 0 || actual_num_channels == 0 ||
		    actual_format == 0) {
			/* No compatible frame rate found. */
			rc = -EINVAL;
			goto error;
		}
		iodev->format->format = actual_format;
		if (iodev->format->num_channels != actual_num_channels) {
			/* If the DSP for this device doesn't match, drop it. */
			iodev->format->num_channels = actual_num_channels;
			trim_channel_layout(iodev->format);
			cras_iodev_free_dsp(iodev);
		}

		update_channel_layout(iodev);

		if (!iodev->rate_est)
			iodev->rate_est = rate_estimator_create(
				actual_rate, &rate_estimation_window_sz,
				rate_estimation_smooth_factor);
		else
			rate_estimator_reset_rate(iodev->rate_est, actual_rate);
	}

	return 0;

error:
	free(iodev->format);
	iodev->format = NULL;
	return rc;
}

/*
 * Configures the external dsp module and adds it to the existing dsp pipeline.
 */
static void add_ext_dsp_module_to_pipeline(struct cras_iodev *iodev)
{
	struct pipeline *pipeline;

	pipeline = iodev->dsp_context ?
			   cras_dsp_get_pipeline(iodev->dsp_context) :
			   NULL;

	if (!pipeline) {
		cras_iodev_alloc_dsp(iodev);
		cras_dsp_load_dummy_pipeline(iodev->dsp_context,
					     iodev->format->num_channels);
		pipeline = cras_dsp_get_pipeline(iodev->dsp_context);
	}
	/* dsp_context mutex locked. Now it's safe to modify dsp
	 * pipeline resources. */

	if (iodev->ext_dsp_module)
		iodev->ext_dsp_module->configure(iodev->ext_dsp_module,
						 iodev->buffer_size,
						 iodev->format->num_channels,
						 iodev->format->frame_rate);

	cras_dsp_pipeline_set_sink_ext_module(pipeline, iodev->ext_dsp_module);

	/* Unlock dsp_context mutex. */
	cras_dsp_put_pipeline(iodev->dsp_context);
}

/*
 * Releases the ext_dsp_module if it ever added to iodev's dsp pipeline.
 */
static void release_ext_dsp_module_from_pipeline(struct cras_iodev *iodev)
{
	struct pipeline *pipeline;

	if (iodev->dsp_context == NULL)
		return;

	pipeline = cras_dsp_get_pipeline(iodev->dsp_context);
	if (pipeline == NULL)
		return;
	/* dsp_context mutex locked. */

	cras_dsp_pipeline_set_sink_ext_module(pipeline, NULL);

	/* Unlock dsp_context mutex. */
	cras_dsp_put_pipeline(iodev->dsp_context);
}

void cras_iodev_set_ext_dsp_module(struct cras_iodev *iodev,
				   struct ext_dsp_module *ext)
{
	iodev->ext_dsp_module = ext;

	if (!cras_iodev_is_open(iodev))
		return;

	if (iodev->ext_dsp_module)
		add_ext_dsp_module_to_pipeline(iodev);
	else
		release_ext_dsp_module_from_pipeline(iodev);
}

void cras_iodev_update_dsp(struct cras_iodev *iodev)
{
	char swap_lr_disabled = 1;

	if (!iodev->dsp_context)
		return;

	cras_dsp_set_variable_string(iodev->dsp_context, "dsp_name",
				     iodev->dsp_name ?: "");

	if (iodev->active_node && iodev->active_node->left_right_swapped)
		swap_lr_disabled = 0;

	cras_dsp_set_variable_boolean(iodev->dsp_context, "swap_lr_disabled",
				      swap_lr_disabled);

	cras_dsp_load_pipeline(iodev->dsp_context);
}

int cras_iodev_dsp_set_swap_mode_for_node(struct cras_iodev *iodev,
					  struct cras_ionode *node, int enable)
{
	if (node->left_right_swapped == enable)
		return 0;

	/* Sets left_right_swapped property on the node. It will be used
	 * when cras_iodev_update_dsp is called. */
	node->left_right_swapped = enable;

	/* Possibly updates dsp if the node is active on the device and there
	 * is dsp context. If dsp context is not created yet,
	 * cras_iodev_update_dsp returns right away. */
	if (iodev->active_node == node)
		cras_iodev_update_dsp(iodev);
	return 0;
}

void cras_iodev_free_format(struct cras_iodev *iodev)
{
	free(iodev->format);
	iodev->format = NULL;
}

void cras_iodev_init_audio_area(struct cras_iodev *iodev, int num_channels)
{
	if (iodev->area)
		cras_iodev_free_audio_area(iodev);

	iodev->area = cras_audio_area_create(num_channels);
	cras_audio_area_config_channels(iodev->area, iodev->format);
}

void cras_iodev_free_audio_area(struct cras_iodev *iodev)
{
	if (!iodev->area)
		return;

	cras_audio_area_destroy(iodev->area);
	iodev->area = NULL;
}

void cras_iodev_free_resources(struct cras_iodev *iodev)
{
	cras_iodev_free_dsp(iodev);
	rate_estimator_destroy(iodev->rate_est);
	if (iodev->ramp)
		cras_ramp_destroy(iodev->ramp);
}

static void cras_iodev_alloc_dsp(struct cras_iodev *iodev)
{
	const char *purpose;

	if (iodev->direction == CRAS_STREAM_OUTPUT)
		purpose = "playback";
	else
		purpose = "capture";

	cras_iodev_free_dsp(iodev);
	iodev->dsp_context =
		cras_dsp_context_new(iodev->format->frame_rate, purpose);
}

void cras_iodev_fill_time_from_frames(size_t frames, size_t frame_rate,
				      struct timespec *ts)
{
	uint64_t to_play_usec;

	ts->tv_sec = 0;
	/* adjust sleep time to target our callback threshold */
	to_play_usec = (uint64_t)frames * 1000000L / (uint64_t)frame_rate;

	while (to_play_usec > 1000000) {
		ts->tv_sec++;
		to_play_usec -= 1000000;
	}
	ts->tv_nsec = to_play_usec * 1000;
}

/* This is called when a node is plugged/unplugged */
void cras_iodev_set_node_plugged(struct cras_ionode *node, int plugged)
{
	if (node->plugged == plugged)
		return;
	node->plugged = plugged;
	if (plugged) {
		gettimeofday(&node->plugged_time, NULL);
	} else if (node == node->dev->active_node) {
		cras_iodev_list_disable_dev(node->dev, false);
	}
	cras_iodev_list_notify_nodes_changed();
}

void cras_iodev_add_node(struct cras_iodev *iodev, struct cras_ionode *node)
{
	DL_APPEND(iodev->nodes, node);
	cras_iodev_list_notify_nodes_changed();
}

void cras_iodev_rm_node(struct cras_iodev *iodev, struct cras_ionode *node)
{
	DL_DELETE(iodev->nodes, node);
	cras_iodev_list_notify_nodes_changed();
}

void cras_iodev_set_active_node(struct cras_iodev *iodev,
				struct cras_ionode *node)
{
	iodev->active_node = node;
	cras_iodev_list_notify_active_node_changed(iodev->direction);
}

float cras_iodev_get_software_volume_scaler(struct cras_iodev *iodev)
{
	unsigned int volume;

	volume = cras_iodev_adjust_active_node_volume(iodev,
						      cras_system_get_volume());

	if (iodev->active_node && iodev->active_node->softvol_scalers)
		return iodev->active_node->softvol_scalers[volume];
	return softvol_get_scaler(volume);
}

float cras_iodev_get_software_gain_scaler(const struct cras_iodev *iodev)
{
	float scaler = 1.0f;
	if (cras_iodev_software_volume_needed(iodev)) {
		long gain = cras_iodev_adjust_active_node_gain(
			iodev, cras_system_get_capture_gain());
		scaler = convert_softvol_scaler_from_dB(gain);
	}
	return scaler;
}

int cras_iodev_get_valid_frames(struct cras_iodev *odev,
				struct timespec *hw_tstamp)
{
	int rc;

	if (odev->direction != CRAS_STREAM_OUTPUT)
		return -EINVAL;

	if (odev->get_valid_frames) {
		rc = odev->get_valid_frames(odev, hw_tstamp);
		if (rc < 0)
			return rc;

		if (rc < odev->min_buffer_level)
			return 0;

		return rc - odev->min_buffer_level;
	} else {
		return cras_iodev_frames_queued(odev, hw_tstamp);
	}
}

int cras_iodev_add_stream(struct cras_iodev *iodev, struct dev_stream *stream)
{
	/*
	 * For input stream, start stream right after adding stream.
	 * For output stream, start stream after its first fetch such that it does not
	 * block other existing streams.
	 */
	DL_APPEND(iodev->streams, stream);
	if (!iodev->buf_state)
		iodev->buf_state = buffer_share_create(iodev->buffer_size);
	if (stream->stream->direction == CRAS_STREAM_INPUT)
		cras_iodev_start_stream(iodev, stream);
	return 0;
}

void cras_iodev_start_stream(struct cras_iodev *iodev,
			     struct dev_stream *stream)
{
	unsigned int cb_threshold = dev_stream_cb_threshold(stream);

	if (dev_stream_is_running(stream))
		return;
	/*
	 * TRIGGER_ONLY streams do not want to receive data, so do not add them
	 * to buffer_share, otherwise they'll affect other streams to receive.
	 */
	if (!(stream->stream->flags & TRIGGER_ONLY))
		buffer_share_add_id(iodev->buf_state, stream->stream->stream_id,
				    NULL);
	iodev->min_cb_level = MIN(iodev->min_cb_level, cb_threshold);
	iodev->max_cb_level = MAX(iodev->max_cb_level, cb_threshold);
	iodev->largest_cb_level = MAX(iodev->largest_cb_level, cb_threshold);
	dev_stream_set_running(stream);
}

struct dev_stream *cras_iodev_rm_stream(struct cras_iodev *iodev,
					const struct cras_rstream *rstream)
{
	struct dev_stream *out;
	struct dev_stream *ret = NULL;
	unsigned int cb_threshold;
	struct timespec earliest_next_cb_ts;
	int set_earliest = 0;

	iodev->min_cb_level = iodev->buffer_size / 2;
	iodev->max_cb_level = 0;
	DL_FOREACH (iodev->streams, out) {
		if (out->stream == rstream) {
			buffer_share_rm_id(iodev->buf_state,
					   rstream->stream_id);
			ret = out;
			DL_DELETE(iodev->streams, out);
			continue;
		}
		if (!dev_stream_is_running(out))
			continue;
		cb_threshold = dev_stream_cb_threshold(out);
		iodev->min_cb_level = MIN(iodev->min_cb_level, cb_threshold);
		iodev->max_cb_level = MAX(iodev->max_cb_level, cb_threshold);
		if (!set_earliest) {
			set_earliest = 1;
			earliest_next_cb_ts = out->stream->next_cb_ts;
		}
		if (timespec_after(&earliest_next_cb_ts,
				   &out->stream->next_cb_ts))
			earliest_next_cb_ts = out->stream->next_cb_ts;
	}

	if (!iodev->streams) {
		buffer_share_destroy(iodev->buf_state);
		iodev->buf_state = NULL;
		iodev->min_cb_level = iodev->buffer_size / 2;
		/* Let output device transit into no stream state if it's
		 * in normal run state now. Leave input device in normal
		 * run state. */
		if ((iodev->direction == CRAS_STREAM_OUTPUT) &&
		    (iodev->state == CRAS_IODEV_STATE_NORMAL_RUN))
			cras_iodev_no_stream_playback_transition(iodev, 1);
	}

	if (!set_earliest)
		return ret;

	DL_FOREACH (iodev->streams, out) {
		if (!dev_stream_is_running(out))
			out->stream->next_cb_ts = earliest_next_cb_ts;
	}

	return ret;
}

unsigned int cras_iodev_stream_offset(struct cras_iodev *iodev,
				      struct dev_stream *stream)
{
	return buffer_share_id_offset(iodev->buf_state,
				      stream->stream->stream_id);
}

void cras_iodev_stream_written(struct cras_iodev *iodev,
			       struct dev_stream *stream, unsigned int nwritten)
{
	buffer_share_offset_update(iodev->buf_state, stream->stream->stream_id,
				   nwritten);
}

unsigned int cras_iodev_all_streams_written(struct cras_iodev *iodev)
{
	if (!iodev->buf_state)
		return 0;
	return buffer_share_get_new_write_point(iodev->buf_state);
}

unsigned int cras_iodev_max_stream_offset(const struct cras_iodev *iodev)
{
	unsigned int max = 0;
	struct dev_stream *curr;

	DL_FOREACH (iodev->streams, curr) {
		/* Skip stream which hasn't started running yet. */
		if (!dev_stream_is_running(curr))
			continue;

		max = MAX(max, buffer_share_id_offset(iodev->buf_state,
						      curr->stream->stream_id));
	}

	return max;
}

int cras_iodev_open(struct cras_iodev *iodev, unsigned int cb_level,
		    const struct cras_audio_format *fmt)
{
	struct cras_loopback *loopback;
	int rc;

	if (iodev->pre_open_iodev_hook)
		iodev->pre_open_iodev_hook();

	DL_FOREACH (iodev->loopbacks, loopback) {
		if (loopback->hook_control)
			loopback->hook_control(true, loopback->cb_data);
	}

	if (iodev->open_dev) {
		rc = iodev->open_dev(iodev);
		if (rc)
			return rc;
	}

	if (iodev->format == NULL) {
		rc = cras_iodev_set_format(iodev, fmt);
		if (rc) {
			iodev->close_dev(iodev);
			return rc;
		}
	}

	rc = iodev->configure_dev(iodev);
	if (rc < 0) {
		iodev->close_dev(iodev);
		return rc;
	}

	/*
	 * Convert cb_level from input format to device format
	 */
	cb_level = cras_frames_at_rate(fmt->frame_rate, cb_level,
				       iodev->format->frame_rate);
	/* Make sure the min_cb_level doesn't get too large. */
	iodev->min_cb_level = MIN(iodev->buffer_size / 2, cb_level);
	iodev->max_cb_level = 0;
	iodev->largest_cb_level = 0;

	iodev->reset_request_pending = 0;
	iodev->state = CRAS_IODEV_STATE_OPEN;
	iodev->highest_hw_level = 0;
	iodev->input_dsp_offset = 0;

	if (iodev->direction == CRAS_STREAM_OUTPUT) {
		/* If device supports start ops, device can be in open state.
		 * Otherwise, device starts running right after opening. */
		if (iodev->start)
			iodev->state = CRAS_IODEV_STATE_OPEN;
		else
			iodev->state = CRAS_IODEV_STATE_NO_STREAM_RUN;
	} else {
		iodev->input_data = input_data_create(iodev);
		/* If this is the echo reference dev, its ext_dsp_module will
		 * be set to APM reverse module. Do not override it to its
		 * input data. */
		if (iodev->ext_dsp_module == NULL)
			iodev->ext_dsp_module = &iodev->input_data->ext;

		/* Input device starts running right after opening.
		 * No stream state is only for output device. Input device
		 * should be in normal run state. */
		iodev->state = CRAS_IODEV_STATE_NORMAL_RUN;
		/* Initialize the input_streaming flag to zero.*/
		iodev->input_streaming = 0;

		/*
		 * The device specific gain scaler to be used in audio thread.
		 * It's expected to stick to 1.0f if device has hardware gain
		 * control. For alsa device, this gain value can be configured
		 * through UCM labels DefaultNodeGain.
		 */
		iodev->software_gain_scaler =
			cras_iodev_get_software_gain_scaler(iodev);
	}

	add_ext_dsp_module_to_pipeline(iodev);
	clock_gettime(CLOCK_MONOTONIC_RAW, &iodev->open_ts);

	return 0;
}

enum CRAS_IODEV_STATE cras_iodev_state(const struct cras_iodev *iodev)
{
	return iodev->state;
}

int cras_iodev_close(struct cras_iodev *iodev)
{
	struct cras_loopback *loopback;
	int rc;

	if (!cras_iodev_is_open(iodev))
		return 0;

	cras_server_metrics_device_runtime(iodev);

	if (iodev->input_data) {
		if (iodev->ext_dsp_module == &iodev->input_data->ext)
			iodev->ext_dsp_module = NULL;
		input_data_destroy(&iodev->input_data);
	}

	rc = iodev->close_dev(iodev);
	if (rc)
		return rc;
	iodev->state = CRAS_IODEV_STATE_CLOSE;
	if (iodev->ramp)
		cras_ramp_reset(iodev->ramp);

	if (iodev->post_close_iodev_hook)
		iodev->post_close_iodev_hook();

	DL_FOREACH (iodev->loopbacks, loopback) {
		if (loopback->hook_control)
			loopback->hook_control(false, loopback->cb_data);
	}

	return 0;
}

int cras_iodev_put_input_buffer(struct cras_iodev *iodev)
{
	unsigned int min_frames;
	unsigned int dsp_frames;
	struct input_data *data = iodev->input_data;
	int rc;

	if (iodev->streams)
		min_frames = buffer_share_get_new_write_point(iodev->buf_state);
	else
		min_frames = data->area->frames;

	// Update the max number of frames has applied input dsp.
	dsp_frames = MAX(iodev->input_frames_read, iodev->input_dsp_offset);
	iodev->input_dsp_offset = dsp_frames - min_frames;

	input_data_set_all_streams_read(data, min_frames);
	rate_estimator_add_frames(iodev->rate_est, -min_frames);
	rc = iodev->put_buffer(iodev, min_frames);
	if (rc < 0)
		return rc;
	return min_frames;
}

int cras_iodev_put_output_buffer(struct cras_iodev *iodev, uint8_t *frames,
				 unsigned int nframes, int *is_non_empty,
				 struct cras_fmt_conv *remix_converter)
{
	const struct cras_audio_format *fmt = iodev->format;
	struct cras_ramp_action ramp_action = {
		.type = CRAS_RAMP_ACTION_NONE,
		.scaler = 0.0f,
		.increment = 0.0f,
		.target = 1.0f,
	};
	float software_volume_scaler = 1.0;
	int software_volume_needed = cras_iodev_software_volume_needed(iodev);
	int rc;
	struct cras_loopback *loopback;

	/* Calculate whether the final output was non-empty, if requested. */
	if (is_non_empty) {
		unsigned int i;
		for (i = 0; i < nframes * cras_get_format_bytes(fmt); i++) {
			if (frames[i]) {
				*is_non_empty = 1;
				break;
			}
		}
	}

	DL_FOREACH (iodev->loopbacks, loopback) {
		if (loopback->type == LOOPBACK_POST_MIX_PRE_DSP)
			loopback->hook_data(frames, nframes, iodev->format,
					    loopback->cb_data);
	}

	rc = apply_dsp(iodev, frames, nframes);
	if (rc)
		return rc;

	DL_FOREACH (iodev->loopbacks, loopback) {
		if (loopback->type == LOOPBACK_POST_DSP)
			loopback->hook_data(frames, nframes, iodev->format,
					    loopback->cb_data);
	}

	if (iodev->ramp) {
		ramp_action = cras_ramp_get_current_action(iodev->ramp);
	}

	/* Mute samples if adjusted volume is 0 or system is muted, plus
	 * that this device is not ramping. */
	if (output_should_mute(iodev) &&
	    ramp_action.type != CRAS_RAMP_ACTION_PARTIAL) {
		const unsigned int frame_bytes = cras_get_format_bytes(fmt);
		cras_mix_mute_buffer(frames, frame_bytes, nframes);
	}

	/* Compute scaler for software volume if needed. */
	if (software_volume_needed) {
		software_volume_scaler =
			cras_iodev_get_software_volume_scaler(iodev);
	}

	if (ramp_action.type == CRAS_RAMP_ACTION_PARTIAL) {
		/* Scale with increment for ramp and possibly
		 * software volume using cras_scale_buffer_increment.*/
		float starting_scaler = ramp_action.scaler;
		float increment = ramp_action.increment;
		float target = ramp_action.target;

		if (software_volume_needed) {
			starting_scaler *= software_volume_scaler;
			increment *= software_volume_scaler;
			target *= software_volume_scaler;
		}

		cras_scale_buffer_increment(fmt->format, frames, nframes,
					    starting_scaler, increment, target,
					    fmt->num_channels);
		cras_ramp_update_ramped_frames(iodev->ramp, nframes);
	} else if (!output_should_mute(iodev) && software_volume_needed) {
		/* Just scale for software volume using
		 * cras_scale_buffer. */
		unsigned int nsamples = nframes * fmt->num_channels;
		cras_scale_buffer(fmt->format, frames, nsamples,
				  software_volume_scaler);
	}

	if (remix_converter)
		cras_channel_remix_convert(remix_converter, iodev->format,
					   frames, nframes);
	if (iodev->rate_est)
		rate_estimator_add_frames(iodev->rate_est, nframes);

	return iodev->put_buffer(iodev, nframes);
}

int cras_iodev_get_input_buffer(struct cras_iodev *iodev, unsigned int *frames)
{
	const unsigned int frame_bytes = cras_get_format_bytes(iodev->format);
	struct input_data *data = iodev->input_data;
	int rc;
	uint8_t *hw_buffer;
	unsigned frame_requested = *frames;

	rc = iodev->get_buffer(iodev, &data->area, frames);
	if (rc < 0 || *frames == 0)
		return rc;

	if (*frames > frame_requested) {
		syslog(LOG_ERR,
		       "frames returned from get_buffer is greater than "
		       "requested: %u > %u",
		       *frames, frame_requested);
		return -EINVAL;
	}

	iodev->input_frames_read = *frames;

	/* TODO(hychao) - This assumes interleaved audio. */
	hw_buffer = data->area->channels[0].buf;

	/*
	 * input_dsp_offset records the position where input dsp has applied to
	 * last time. It's possible the requested |frames| count is smaller
	 * than the tracked offset. That could happen when client stream uses
	 * small buffer size and runs APM processing (which requires 10 ms
	 * equivalent of data to process).
	 * Only apply input dsp to the part of read buffer beyond where we've
	 * already applied dsp.
	 */
	if (*frames > iodev->input_dsp_offset) {
		rc = apply_dsp(iodev,
			       hw_buffer +
				       iodev->input_dsp_offset * frame_bytes,
			       *frames - iodev->input_dsp_offset);
		if (rc)
			return rc;
	}

	if (cras_system_get_capture_mute())
		cras_mix_mute_buffer(hw_buffer, frame_bytes, *frames);

	return rc;
}

int cras_iodev_get_output_buffer(struct cras_iodev *iodev,
				 struct cras_audio_area **area,
				 unsigned *frames)
{
	int rc;
	unsigned frame_requested = *frames;

	rc = iodev->get_buffer(iodev, area, frames);
	if (*frames > frame_requested) {
		syslog(LOG_ERR,
		       "frames returned from get_buffer is greater than "
		       "requested: %u > %u",
		       *frames, frame_requested);
		return -EINVAL;
	}
	return rc;
}

int cras_iodev_update_rate(struct cras_iodev *iodev, unsigned int level,
			   struct timespec *level_tstamp)
{
	/* If output underruns, reset to avoid incorrect estimated rate. */
	if ((iodev->direction == CRAS_STREAM_OUTPUT) && !level)
		rate_estimator_reset_rate(iodev->rate_est,
					  iodev->format->frame_rate);

	return rate_estimator_check(iodev->rate_est, level, level_tstamp);
}

int cras_iodev_reset_rate_estimator(const struct cras_iodev *iodev)
{
	rate_estimator_reset_rate(iodev->rate_est, iodev->format->frame_rate);
	return 0;
}

double cras_iodev_get_est_rate_ratio(const struct cras_iodev *iodev)
{
	return rate_estimator_get_rate(iodev->rate_est) /
	       iodev->format->frame_rate;
}

int cras_iodev_get_dsp_delay(const struct cras_iodev *iodev)
{
	struct cras_dsp_context *ctx;
	struct pipeline *pipeline;
	int delay;

	ctx = iodev->dsp_context;
	if (!ctx)
		return 0;

	pipeline = cras_dsp_get_pipeline(ctx);
	if (!pipeline)
		return 0;

	delay = cras_dsp_pipeline_get_delay(pipeline);

	cras_dsp_put_pipeline(ctx);
	return delay;
}

int cras_iodev_frames_queued(struct cras_iodev *iodev,
			     struct timespec *hw_tstamp)
{
	int rc;

	rc = iodev->frames_queued(iodev, hw_tstamp);
	if (rc == -EPIPE)
		cras_audio_thread_event_severe_underrun();

	if (rc < 0)
		return rc;

	if (iodev->direction == CRAS_STREAM_INPUT) {
		if (rc > 0)
			iodev->input_streaming = 1;
		return rc;
	}

	if (rc < iodev->min_buffer_level)
		return 0;

	return rc - iodev->min_buffer_level;
}

int cras_iodev_buffer_avail(struct cras_iodev *iodev, unsigned hw_level)
{
	if (iodev->direction == CRAS_STREAM_INPUT)
		return hw_level;

	if (hw_level + iodev->min_buffer_level > iodev->buffer_size)
		return 0;

	return iodev->buffer_size - iodev->min_buffer_level - hw_level;
}

int cras_iodev_fill_odev_zeros(struct cras_iodev *odev, unsigned int frames)
{
	struct cras_audio_area *area = NULL;
	unsigned int frame_bytes, frames_written;
	int rc;
	uint8_t *buf;

	if (odev->direction != CRAS_STREAM_OUTPUT)
		return -EINVAL;

	ATLOG(atlog, AUDIO_THREAD_FILL_ODEV_ZEROS, odev->info.idx, frames, 0);

	frame_bytes = cras_get_format_bytes(odev->format);
	while (frames > 0) {
		frames_written = frames;
		rc = cras_iodev_get_output_buffer(odev, &area, &frames_written);
		if (rc < 0) {
			syslog(LOG_ERR, "fill zeros fail: %d", rc);
			return rc;
		}

		/* This assumes consecutive channel areas. */
		buf = area->channels[0].buf;
		memset(buf, 0, frames_written * frame_bytes);
		cras_iodev_put_output_buffer(odev, buf, frames_written, NULL,
					     NULL);
		frames -= frames_written;
	}

	return 0;
}

int cras_iodev_output_underrun(struct cras_iodev *odev)
{
	cras_audio_thread_event_underrun();
	if (odev->output_underrun)
		return odev->output_underrun(odev);
	else
		return cras_iodev_fill_odev_zeros(odev, odev->min_cb_level);
}

int cras_iodev_odev_should_wake(const struct cras_iodev *odev)
{
	if (odev->direction != CRAS_STREAM_OUTPUT)
		return 0;

	if (odev->is_free_running && odev->is_free_running(odev))
		return 0;

	/* Do not wake up for device not started yet. */
	return (odev->state == CRAS_IODEV_STATE_NORMAL_RUN ||
		odev->state == CRAS_IODEV_STATE_NO_STREAM_RUN);
}

unsigned int cras_iodev_frames_to_play_in_sleep(struct cras_iodev *odev,
						unsigned int *hw_level,
						struct timespec *hw_tstamp)
{
	int rc = cras_iodev_frames_queued(odev, hw_tstamp);
	unsigned int level = (rc < 0) ? 0 : rc;
	unsigned int wakeup_frames;
	*hw_level = level;

	if (odev->streams) {
		/*
		 * We have two cases in this scope. The first one is if there are frames
		 * waiting to be played, audio thread will wake up when hw_level drops
		 * to min_cb_level. This situation only happens when hardware buffer is
		 * smaller than the client stream buffer. The second one is waking up
		 * when hw_level drops to dev_normal_run_wake_up_time. It is a default
		 * behavior. This wake up time is the bottom line to avoid underrun.
		 * Normally, the audio thread does not wake up at that time because the
		 * streams should wake it up before then.
		 */
		if (*hw_level > odev->min_cb_level && dev_playback_frames(odev))
			return *hw_level - odev->min_cb_level;

		wakeup_frames = cras_time_to_frames(
			&dev_normal_run_wake_up_time, odev->format->frame_rate);
		if (level > wakeup_frames)
			return level - wakeup_frames;
		else
			return level;
	}

	/*
	 * When this device has no stream, schedule audio thread to wake up when
	 * hw_level drops to dev_no_stream_wake_up_time so audio thread can
	 * fill zeros to it. We also need to consider min_cb_level in order to avoid
	 * busyloop when device buffer size is smaller than wake up time.
	 */
	wakeup_frames = cras_time_to_frames(&dev_no_stream_wake_up_time,
					    odev->format->frame_rate);
	if (level > MIN(odev->min_cb_level, wakeup_frames))
		return level - MIN(odev->min_cb_level, wakeup_frames);
	else
		return 0;
}

int cras_iodev_default_no_stream_playback(struct cras_iodev *odev, int enable)
{
	if (enable)
		return default_no_stream_playback(odev);
	return 0;
}

int cras_iodev_prepare_output_before_write_samples(struct cras_iodev *odev)
{
	int may_enter_normal_run;
	enum CRAS_IODEV_STATE state;

	if (odev->direction != CRAS_STREAM_OUTPUT)
		return -EINVAL;

	state = cras_iodev_state(odev);

	may_enter_normal_run = (state == CRAS_IODEV_STATE_OPEN ||
				state == CRAS_IODEV_STATE_NO_STREAM_RUN);

	if (may_enter_normal_run && dev_playback_frames(odev))
		return cras_iodev_output_event_sample_ready(odev);

	/* no_stream ops is called every cycle in no_stream state. */
	if (state == CRAS_IODEV_STATE_NO_STREAM_RUN)
		return odev->no_stream(odev, 1);

	return 0;
}

unsigned int cras_iodev_get_num_underruns(const struct cras_iodev *iodev)
{
	if (iodev->get_num_underruns)
		return iodev->get_num_underruns(iodev);
	return 0;
}

unsigned int cras_iodev_get_num_severe_underruns(const struct cras_iodev *iodev)
{
	if (iodev->get_num_severe_underruns)
		return iodev->get_num_severe_underruns(iodev);
	return 0;
}

int cras_iodev_reset_request(struct cras_iodev *iodev)
{
	/* Ignore requests if there is a pending request.
	 * This function sends the request from audio thread to main
	 * thread when audio thread finds a device is in a bad state
	 * e.g. severe underrun. Before main thread receives the
	 * request and resets device, audio thread might try to send
	 * multiple requests because it finds device is still in bad
	 * state. We should ignore requests in this cause. Otherwise,
	 * main thread will reset device multiple times.
	 * The flag is cleared in cras_iodev_open.
	 * */
	if (iodev->reset_request_pending)
		return 0;
	iodev->reset_request_pending = 1;
	return cras_device_monitor_reset_device(iodev->info.idx);
}

static void ramp_mute_callback(void *data)
{
	struct cras_iodev *odev = (struct cras_iodev *)data;
	cras_device_monitor_set_device_mute_state(odev->info.idx);
}

/* Used in audio thread. Check the docstrings of CRAS_IODEV_RAMP_REQUEST. */
int cras_iodev_start_ramp(struct cras_iodev *odev,
			  enum CRAS_IODEV_RAMP_REQUEST request)
{
	cras_ramp_cb cb = NULL;
	void *cb_data = NULL;
	int rc;
	float from, to, duration_secs;

	/* Ignores request if device is closed. */
	if (!cras_iodev_is_open(odev))
		return 0;

	switch (request) {
	case CRAS_IODEV_RAMP_REQUEST_UP_UNMUTE:
		from = 0.0;
		to = 1.0;
		duration_secs = RAMP_UNMUTE_DURATION_SECS;
		break;
	case CRAS_IODEV_RAMP_REQUEST_UP_START_PLAYBACK:
		from = 0.0;
		to = 1.0;
		duration_secs = RAMP_NEW_STREAM_DURATION_SECS;
		break;
	/* Unmute -> mute. Callback to set mute state should be called after
	 * ramping is done. */
	case CRAS_IODEV_RAMP_REQUEST_DOWN_MUTE:
		from = 1.0;
		to = 0.0;
		duration_secs = RAMP_MUTE_DURATION_SECS;
		cb = ramp_mute_callback;
		cb_data = (void *)odev;
		break;
	default:
		return -EINVAL;
	}

	/* Starts ramping. */
	rc = cras_mute_ramp_start(odev->ramp, from, to,
				  duration_secs * odev->format->frame_rate, cb,
				  cb_data);

	if (rc)
		return rc;

	/* Mute -> unmute case, unmute state should be set after ramping is
	 * started so device can start playing with samples close to 0. */
	if (request == CRAS_IODEV_RAMP_REQUEST_UP_UNMUTE)
		cras_device_monitor_set_device_mute_state(odev->info.idx);

	return 0;
}

int cras_iodev_start_volume_ramp(struct cras_iodev *odev,
				 unsigned int old_volume,
				 unsigned int new_volume)
{
	float old_scaler, new_scaler;
	float from, to;

	if (old_volume == new_volume)
		return 0;
	if (!cras_iodev_is_open(odev))
		return 0;
	if (!odev->format)
		return -EINVAL;
	if (odev->active_node && odev->active_node->softvol_scalers) {
		old_scaler = odev->active_node->softvol_scalers[old_volume];
		new_scaler = odev->active_node->softvol_scalers[new_volume];
	} else {
		old_scaler = softvol_get_scaler(old_volume);
		new_scaler = softvol_get_scaler(new_volume);
	}
	if (new_scaler == 0.0) {
		return -EINVAL;
	}
	/* We will soon set odev's volume to new_volume from old_volume.
	 * Because we're using softvol, we were previously scaling our volume by
	 * old_scaler. If we want to avoid a jump in volume, we need to start
	 * our ramp so that (from * new_scaler) = old_scaler. */
	from = old_scaler / new_scaler;
	to = 1.0;

	return cras_volume_ramp_start(odev->ramp, from, to,
				      RAMP_VOLUME_CHANGE_DURATION_SECS *
					      odev->format->frame_rate,
				      NULL, NULL);
}

int cras_iodev_set_mute(struct cras_iodev *iodev)
{
	if (!cras_iodev_is_open(iodev))
		return 0;

	if (iodev->set_mute)
		iodev->set_mute(iodev);
	return 0;
}

void cras_iodev_update_highest_hw_level(struct cras_iodev *iodev,
					unsigned int hw_level)
{
	iodev->highest_hw_level = MAX(iodev->highest_hw_level, hw_level);
}

/*
 * Makes an input device drop the given number of frames.
 * Args:
 *    iodev - The device.
 *    frames - How many frames will be dropped in a device.
 * Returns:
 *    The number of frames have been dropped. Negative error code on failure.
 */
static int cras_iodev_drop_frames(struct cras_iodev *iodev, unsigned int frames)
{
	struct timespec hw_tstamp;
	int rc;

	if (iodev->direction != CRAS_STREAM_INPUT)
		return -EINVAL;

	rc = cras_iodev_frames_queued(iodev, &hw_tstamp);
	if (rc < 0)
		return rc;

	frames = MIN(frames, rc);

	rc = iodev->get_buffer(iodev, &iodev->input_data->area, &frames);
	if (rc < 0)
		return rc;

	rc = iodev->put_buffer(iodev, frames);
	if (rc < 0)
		return rc;

	/*
	 * Tell rate estimator that some frames have been dropped to avoid calculating
	 * the wrong rate.
	 */
	rate_estimator_add_frames(iodev->rate_est, -frames);

	ATLOG(atlog, AUDIO_THREAD_DEV_DROP_FRAMES, iodev->info.idx, frames, 0);

	return frames;
}

int cras_iodev_drop_frames_by_time(struct cras_iodev *iodev, struct timespec ts)
{
	int frames_to_set;
	double est_rate;
	int rc;

	est_rate = iodev->format->frame_rate *
		   cras_iodev_get_est_rate_ratio(iodev);
	frames_to_set = cras_time_to_frames(&ts, est_rate);

	rc = cras_iodev_drop_frames(iodev, frames_to_set);

	return rc;
}
