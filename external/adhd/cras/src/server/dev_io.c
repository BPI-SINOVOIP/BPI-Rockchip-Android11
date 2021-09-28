/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <poll.h>
#include <stdbool.h>
#include <syslog.h>

#include "audio_thread_log.h"
#include "cras_audio_area.h"
#include "cras_audio_thread_monitor.h"
#include "cras_iodev.h"
#include "cras_non_empty_audio_handler.h"
#include "cras_rstream.h"
#include "cras_server_metrics.h"
#include "dev_stream.h"
#include "input_data.h"
#include "polled_interval_checker.h"
#include "rate_estimator.h"
#include "utlist.h"

#include "dev_io.h"

static const struct timespec playback_wake_fuzz_ts = {
	0, 500 * 1000 /* 500 usec. */
};

/* The maximum time to wait before checking the device's non-empty status. */
static const int NON_EMPTY_UPDATE_INTERVAL_SEC = 5;

/*
 * The minimum number of consecutive seconds of empty audio that must be
 * played before a device is considered to be playing empty audio.
 */
static const int MIN_EMPTY_PERIOD_SEC = 30;

/*
 * When the hw_level is less than this time, do not drop frames.
 * (unit: millisecond).
 * TODO(yuhsuan): Reduce the threshold when we create the other overrun op for
 * boards which captures a lot of frames at one time.
 * e.g. Input devices on grunt reads 1024 frames each time.
 */
static const int DROP_FRAMES_THRESHOLD_MS = 50;

/* The number of devices playing/capturing non-empty stream(s). */
static int non_empty_device_count = 0;

/* Gets the master device which the stream is attached to. */
static inline struct cras_iodev *get_master_dev(const struct dev_stream *stream)
{
	return (struct cras_iodev *)stream->stream->master_dev.dev_ptr;
}

/* Updates the estimated sample rate of open device to all attached
 * streams.
 */
static void update_estimated_rate(struct open_dev *adev)
{
	struct cras_iodev *master_dev;
	struct cras_iodev *dev = adev->dev;
	struct dev_stream *dev_stream;

	DL_FOREACH (dev->streams, dev_stream) {
		master_dev = get_master_dev(dev_stream);
		if (master_dev == NULL) {
			syslog(LOG_ERR, "Fail to find master open dev.");
			continue;
		}

		dev_stream_set_dev_rate(
			dev_stream, dev->format->frame_rate,
			cras_iodev_get_est_rate_ratio(dev),
			cras_iodev_get_est_rate_ratio(master_dev),
			adev->coarse_rate_adjust);
	}
}

/*
 * Counts the number of devices which are currently playing/capturing non-empty
 * audio.
 */
static inline int count_non_empty_dev(struct open_dev *adevs)
{
	int count = 0;
	struct open_dev *adev;
	DL_FOREACH (adevs, adev) {
		if (!adev->empty_pi || !pic_interval_elapsed(adev->empty_pi))
			count++;
	}
	return count;
}

static void check_non_empty_state_transition(struct open_dev *adevs)
{
	int new_non_empty_dev_count = count_non_empty_dev(adevs);

	// If we have transitioned to or from a state with 0 non-empty devices,
	// notify the main thread to update system state.
	if ((non_empty_device_count == 0) != (new_non_empty_dev_count == 0))
		cras_non_empty_audio_send_msg(new_non_empty_dev_count > 0 ? 1 :
									    0);

	non_empty_device_count = new_non_empty_dev_count;
}

/* Checks whether it is time to fetch. */
static bool is_time_to_fetch(const struct dev_stream *dev_stream,
			     struct timespec now)
{
	const struct timespec *next_cb_ts;
	next_cb_ts = dev_stream_next_cb_ts(dev_stream);
	if (!next_cb_ts)
		return 0;

	/*
	 * Check if it's time to get more data from this stream.
	 * Allow for waking up a little early.
	 */
	add_timespecs(&now, &playback_wake_fuzz_ts);
	if (timespec_after(&now, next_cb_ts))
		return 1;

	return 0;
}

/* Asks any stream with room for more data. Sets the time stamp for all streams.
 * Args:
 *    adev - The output device streams are attached to.
 * Returns:
 *    0 on success, negative error on failure. If failed, can assume that all
 *    streams have been removed from the device.
 */
static int fetch_streams(struct open_dev *adev)
{
	struct dev_stream *dev_stream;
	struct cras_iodev *odev = adev->dev;
	int rc;
	int delay;

	delay = cras_iodev_delay_frames(odev);
	if (delay < 0)
		return delay;

	DL_FOREACH (adev->dev->streams, dev_stream) {
		struct cras_rstream *rstream = dev_stream->stream;
		struct cras_audio_shm *shm = cras_rstream_shm(rstream);
		struct timespec now;

		clock_gettime(CLOCK_MONOTONIC_RAW, &now);

		if (dev_stream_is_pending_reply(dev_stream)) {
			dev_stream_flush_old_audio_messages(dev_stream);
			cras_rstream_record_fetch_interval(dev_stream->stream,
							   &now);
		}

		if (!dev_stream_is_running(dev_stream))
			continue;

		if (!is_time_to_fetch(dev_stream, now))
			continue;

		if (cras_shm_get_frames(shm) < 0)
			cras_rstream_set_is_draining(rstream, 1);

		if (cras_rstream_get_is_draining(dev_stream->stream))
			continue;

		/*
		 * Skip fetching if client still has not replied yet.
		 */
		if (cras_rstream_is_pending_reply(rstream)) {
			ATLOG(atlog, AUDIO_THREAD_STREAM_FETCH_PENDING,
			      cras_rstream_id(rstream), 0, 0);
			continue;
		}

		/*
		 * Skip fetching if there are enough frames in shared memory.
		 */
		if (!cras_shm_is_buffer_available(shm)) {
			ATLOG(atlog, AUDIO_THREAD_STREAM_SKIP_CB,
			      cras_rstream_id(rstream),
			      shm->header->write_offset[0],
			      shm->header->write_offset[1]);
			dev_stream_update_next_wake_time(dev_stream);
			continue;
		}

		dev_stream_set_delay(dev_stream, delay);

		ATLOG(atlog, AUDIO_THREAD_FETCH_STREAM, rstream->stream_id,
		      cras_rstream_get_cb_threshold(rstream), delay);

		rc = dev_stream_request_playback_samples(dev_stream, &now);
		if (rc < 0) {
			syslog(LOG_ERR, "fetch err: %d for %x", rc,
			       cras_rstream_id(rstream));
			cras_rstream_set_is_draining(rstream, 1);
		}
	}

	return 0;
}

/* Gets the max delay frames of open input devices. */
static int input_delay_frames(struct open_dev *adevs)
{
	struct open_dev *adev;
	int delay;
	int max_delay = 0;

	DL_FOREACH (adevs, adev) {
		if (!cras_iodev_is_open(adev->dev))
			continue;
		delay = cras_iodev_delay_frames(adev->dev);
		if (delay < 0)
			return delay;
		if (delay > max_delay)
			max_delay = delay;
	}
	return max_delay;
}

/* Sets the stream delay.
 * Args:
 *    adev[in] - The device to capture from.
 */
static unsigned int set_stream_delay(struct open_dev *adev)
{
	struct dev_stream *stream;
	int delay;

	/* TODO(dgreid) - Setting delay from last dev only. */
	delay = input_delay_frames(adev);

	DL_FOREACH (adev->dev->streams, stream) {
		if (stream->stream->flags & TRIGGER_ONLY)
			continue;

		dev_stream_set_delay(stream, delay);
	}

	return 0;
}

/* Gets the minimum amount of space available for writing across all streams.
 * Args:
 *    adev[in] - The device to capture from.
 *    write_limit[in] - Initial limit to number of frames to capture.
 *    limit_stream[out] - The pointer to the pointer of stream which
 *                        causes capture limit.
 *                        Output NULL if there is no stream that causes
 *                        capture limit less than the initial limit.
 */
static unsigned int get_stream_limit(struct open_dev *adev,
				     unsigned int write_limit,
				     struct dev_stream **limit_stream)
{
	struct cras_rstream *rstream;
	struct cras_audio_shm *shm;
	struct dev_stream *stream;
	unsigned int avail;

	*limit_stream = NULL;

	DL_FOREACH (adev->dev->streams, stream) {
		rstream = stream->stream;
		if (rstream->flags & TRIGGER_ONLY)
			continue;

		shm = cras_rstream_shm(rstream);
		if (cras_shm_check_write_overrun(shm))
			ATLOG(atlog, AUDIO_THREAD_READ_OVERRUN,
			      adev->dev->info.idx, rstream->stream_id,
			      shm->header->num_overruns);
		avail = dev_stream_capture_avail(stream);
		if (avail < write_limit) {
			write_limit = avail;
			*limit_stream = stream;
		}
	}

	return write_limit;
}

/*
 * The minimum wake time for a input device, which is 5ms. It's only used by
 * function get_input_dev_max_wake_ts.
 */
static const struct timespec min_input_dev_wake_ts = {
	0, 5 * 1000 * 1000 /* 5 ms. */
};

/*
 * Get input device maximum sleep time, which is the approximate time that the
 * device will have hw_level = buffer_size / 2 samples. Some devices have
 * capture period = 2 so the audio_thread should wake up and consume some
 * samples from hardware at that time. To prevent busy loop occurs, the returned
 * sleep time should be >= 5ms.
 *
 * Returns: 0 on success negative error on device failure.
 */
static int get_input_dev_max_wake_ts(struct open_dev *adev,
				     unsigned int curr_level,
				     struct timespec *res_ts)
{
	struct timespec dev_wake_ts, now;
	unsigned int dev_rate, half_buffer_size, target_frames;

	if (!adev || !adev->dev || !adev->dev->format ||
	    !adev->dev->format->frame_rate || !adev->dev->buffer_size)
		return -EINVAL;

	*res_ts = min_input_dev_wake_ts;

	dev_rate = adev->dev->format->frame_rate;
	half_buffer_size = adev->dev->buffer_size / 2;
	if (curr_level < half_buffer_size)
		target_frames = half_buffer_size - curr_level;
	else
		target_frames = 0;

	cras_frames_to_time(target_frames, dev_rate, &dev_wake_ts);

	if (timespec_after(&dev_wake_ts, res_ts)) {
		*res_ts = dev_wake_ts;
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	add_timespecs(res_ts, &now);
	return 0;
}

/* Returns whether a device can drop samples. */
static bool input_devices_can_drop_samples(struct cras_iodev *iodev)
{
	if (!cras_iodev_is_open(iodev))
		return false;
	if (!iodev->streams)
		return false;
	if (!iodev->active_node ||
	    iodev->active_node->type == CRAS_NODE_TYPE_HOTWORD)
		return false;
	return true;
}

/*
 * Set wake_ts for this device to be the earliest wake up time for
 * dev_streams. Default value for adev->wake_ts will be now + 20s even if
 * any error occurs in this function.
 * Args:
 *    adev - The input device.
 *    need_to_drop - The pointer to store whether we need to drop samples from
 *                   a device in order to keep the lower hw_level.
 * Returns:
 *    0 on success. Negative error code on failure.
 */
static int set_input_dev_wake_ts(struct open_dev *adev, bool *need_to_drop)
{
	int rc;
	struct timespec level_tstamp, wake_time_out, min_ts, now, dev_wake_ts;
	unsigned int curr_level, cap_limit;
	struct dev_stream *stream;
	struct dev_stream *cap_limit_stream;

	/* Limit the sleep time to 20 seconds. */
	min_ts.tv_sec = 20;
	min_ts.tv_nsec = 0;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	add_timespecs(&min_ts, &now);
	/* Set default value for device wake_ts. */
	adev->wake_ts = min_ts;

	rc = cras_iodev_frames_queued(adev->dev, &level_tstamp);
	if (rc < 0)
		return rc;
	curr_level = rc;
	if (!timespec_is_nonzero(&level_tstamp))
		clock_gettime(CLOCK_MONOTONIC_RAW, &level_tstamp);

	/*
	 * If any input device has more than largest_cb_level * 1.5 frames, need to
	 * drop frames from all devices.
	 */
	if (input_devices_can_drop_samples(adev->dev) &&
	    rc >= adev->dev->largest_cb_level * 1.5 &&
	    cras_frames_to_ms(rc, adev->dev->format->frame_rate) >=
		    DROP_FRAMES_THRESHOLD_MS)
		*need_to_drop = true;

	cap_limit = get_stream_limit(adev, UINT_MAX, &cap_limit_stream);

	/*
	 * Loop through streams to find the earliest time audio thread
	 * should wake up.
	 */
	DL_FOREACH (adev->dev->streams, stream) {
		wake_time_out = min_ts;
		rc = dev_stream_wake_time(stream, curr_level, &level_tstamp,
					  cap_limit, cap_limit_stream == stream,
					  &wake_time_out);

		/*
		 * rc > 0 means there is no need to set wake up time for this
		 * stream.
		 */
		if (rc > 0)
			continue;

		if (rc < 0)
			return rc;

		if (timespec_after(&min_ts, &wake_time_out)) {
			min_ts = wake_time_out;
		}
	}

	/* If there's no room in streams, don't bother schedule wake for more
	 * input data. */
	if (adev->dev->active_node &&
	    adev->dev->active_node->type != CRAS_NODE_TYPE_HOTWORD &&
	    cap_limit) {
		rc = get_input_dev_max_wake_ts(adev, curr_level, &dev_wake_ts);
		if (rc < 0) {
			syslog(LOG_ERR,
			       "Failed to call get_input_dev_max_wake_ts."
			       "rc = %d",
			       rc);
		} else if (timespec_after(&min_ts, &dev_wake_ts)) {
			min_ts = dev_wake_ts;
		}
	}

	adev->wake_ts = min_ts;
	return rc;
}

/* Read samples from an input device to the specified stream.
 * Args:
 *    adev - The device to capture samples from.
 * Returns 0 on success.
 */
static int capture_to_streams(struct open_dev *adev)
{
	struct cras_iodev *idev = adev->dev;
	snd_pcm_uframes_t remainder, hw_level, cap_limit;
	struct timespec hw_tstamp;
	int rc;
	struct dev_stream *cap_limit_stream;
	struct dev_stream *stream;

	DL_FOREACH (adev->dev->streams, stream)
		dev_stream_flush_old_audio_messages(stream);

	rc = cras_iodev_frames_queued(idev, &hw_tstamp);
	if (rc < 0)
		return rc;
	hw_level = rc;

	cras_iodev_update_highest_hw_level(idev, hw_level);

	ATLOG(atlog, AUDIO_THREAD_READ_AUDIO_TSTAMP, idev->info.idx,
	      hw_tstamp.tv_sec, hw_tstamp.tv_nsec);
	if (timespec_is_nonzero(&hw_tstamp)) {
		if (hw_level < idev->min_cb_level / 2)
			adev->coarse_rate_adjust = 1;
		else if (hw_level > idev->max_cb_level * 2)
			adev->coarse_rate_adjust = -1;
		else
			adev->coarse_rate_adjust = 0;
		if (cras_iodev_update_rate(idev, hw_level, &hw_tstamp))
			update_estimated_rate(adev);
	}

	cap_limit = get_stream_limit(adev, hw_level, &cap_limit_stream);
	set_stream_delay(adev);

	remainder = MIN(hw_level, cap_limit);

	ATLOG(atlog, AUDIO_THREAD_READ_AUDIO, idev->info.idx, hw_level,
	      remainder);

	if (cras_iodev_state(idev) != CRAS_IODEV_STATE_NORMAL_RUN)
		return 0;

	while (remainder > 0) {
		struct cras_audio_area *area = NULL;
		unsigned int nread, total_read;

		nread = remainder;

		rc = cras_iodev_get_input_buffer(idev, &nread);
		if (rc < 0 || nread == 0)
			return rc;

		DL_FOREACH (adev->dev->streams, stream) {
			unsigned int this_read;
			unsigned int area_offset;
			float software_gain_scaler;

			if ((stream->stream->flags & TRIGGER_ONLY) &&
			    stream->stream->triggered)
				continue;

			input_data_get_for_stream(idev->input_data,
						  stream->stream,
						  idev->buf_state, &area,
						  &area_offset);
			/*
			 * The software gain scaler consist of two parts:
			 * (1) The device gain scaler used when lack of hardware
			 * gain control. Configured by the DefaultNodeGain label
			 * in alsa UCM config.
			 * (2) The gain scaler in cras_rstream set by app, for
			 * example the AGC module in Chrome.
			 *
			 * APM has more advanced gain control mechanism, we shall
			 * give APM total control of the captured samples without
			 * additional gain scaler at all.
			 */
			software_gain_scaler =
				stream->stream->apm_list ?
					1.0f :
					idev->software_gain_scaler *
						cras_rstream_get_volume_scaler(
							stream->stream);

			this_read =
				dev_stream_capture(stream, area, area_offset,
						   software_gain_scaler);

			input_data_put_for_stream(idev->input_data,
						  stream->stream,
						  idev->buf_state, this_read);
		}

		rc = cras_iodev_put_input_buffer(idev);
		if (rc < 0)
			return rc;

		total_read = rc;
		remainder -= nread;

		if (total_read < nread)
			break;
	}

	ATLOG(atlog, AUDIO_THREAD_READ_AUDIO_DONE, remainder, 0, 0);

	return 0;
}

/* Fill the buffer with samples from the attached streams.
 * Args:
 *    odevs - The list of open output devices, provided so streams can be
 *            removed from all devices on error.
 *    adev - The device to write to.
 *    dst - The buffer to put the samples in (returned from snd_pcm_mmap_begin)
 *    write_limit - The maximum number of frames to write to dst.
 *
 * Returns:
 *    The number of frames rendered on success, a negative error code otherwise.
 *    This number of frames is the minimum of the amount of frames each stream
 *    could provide which is the maximum that can currently be rendered.
 */
static int write_streams(struct open_dev **odevs, struct open_dev *adev,
			 uint8_t *dst, size_t write_limit)
{
	struct cras_iodev *odev = adev->dev;
	struct dev_stream *curr;
	unsigned int max_offset = 0;
	unsigned int frame_bytes = cras_get_format_bytes(odev->format);
	unsigned int num_playing = 0;
	unsigned int drain_limit = write_limit;

	/* Mix as much as we can, the minimum fill level of any stream. */
	max_offset = cras_iodev_max_stream_offset(odev);

	/* Mix as much as we can, the minimum fill level of any stream. */
	DL_FOREACH (adev->dev->streams, curr) {
		int dev_frames;

		/* Skip stream which hasn't started running yet. */
		if (!dev_stream_is_running(curr))
			continue;

		/* If this is a single output dev stream, updates the latest
		 * number of frames for playback. */
		if (dev_stream_attached_devs(curr) == 1)
			dev_stream_update_frames(curr);

		dev_frames = dev_stream_playback_frames(curr);
		if (dev_frames < 0) {
			dev_io_remove_stream(odevs, curr->stream, NULL);
			continue;
		}
		ATLOG(atlog, AUDIO_THREAD_WRITE_STREAMS_STREAM,
		      curr->stream->stream_id, dev_frames,
		      dev_stream_is_pending_reply(curr));
		if (cras_rstream_get_is_draining(curr->stream)) {
			drain_limit = MIN((size_t)dev_frames, drain_limit);
			if (!dev_frames)
				dev_io_remove_stream(odevs, curr->stream, NULL);
		} else {
			write_limit = MIN((size_t)dev_frames, write_limit);
			num_playing++;
		}
	}

	if (!num_playing)
		write_limit = drain_limit;

	if (write_limit > max_offset)
		memset(dst + max_offset * frame_bytes, 0,
		       (write_limit - max_offset) * frame_bytes);

	ATLOG(atlog, AUDIO_THREAD_WRITE_STREAMS_MIX, write_limit, max_offset,
	      0);

	DL_FOREACH (adev->dev->streams, curr) {
		unsigned int offset;
		int nwritten;

		if (!dev_stream_is_running(curr))
			continue;

		offset = cras_iodev_stream_offset(odev, curr);
		if (offset >= write_limit)
			continue;
		nwritten = dev_stream_mix(curr, odev->format,
					  dst + frame_bytes * offset,
					  write_limit - offset);

		if (nwritten < 0) {
			dev_io_remove_stream(odevs, curr->stream, NULL);
			continue;
		}

		cras_iodev_stream_written(odev, curr, nwritten);
	}

	write_limit = cras_iodev_all_streams_written(odev);

	ATLOG(atlog, AUDIO_THREAD_WRITE_STREAMS_MIXED, write_limit, 0, 0);

	return write_limit;
}

/* Update next wake up time of the device.
 * Args:
 *    adev[in] - The device to update to.
 *    hw_level[out] - Pointer to number of frames in hardware.
 */
void update_dev_wakeup_time(struct open_dev *adev, unsigned int *hw_level)
{
	struct timespec now;
	struct timespec sleep_time;
	double est_rate;
	unsigned int frames_to_play_in_sleep;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);

	frames_to_play_in_sleep = cras_iodev_frames_to_play_in_sleep(
		adev->dev, hw_level, &adev->wake_ts);
	if (!timespec_is_nonzero(&adev->wake_ts))
		adev->wake_ts = now;

	if (cras_iodev_state(adev->dev) == CRAS_IODEV_STATE_NORMAL_RUN)
		cras_iodev_update_highest_hw_level(adev->dev, *hw_level);

	est_rate = adev->dev->format->frame_rate *
		   cras_iodev_get_est_rate_ratio(adev->dev);

	ATLOG(atlog, AUDIO_THREAD_SET_DEV_WAKE, adev->dev->info.idx, *hw_level,
	      frames_to_play_in_sleep);

	cras_frames_to_time_precise(frames_to_play_in_sleep, est_rate,
				    &sleep_time);

	add_timespecs(&adev->wake_ts, &sleep_time);

	ATLOG(atlog, AUDIO_THREAD_DEV_SLEEP_TIME, adev->dev->info.idx,
	      adev->wake_ts.tv_sec, adev->wake_ts.tv_nsec);
}

/* Returns 0 on success negative error on device failure. */
int write_output_samples(struct open_dev **odevs, struct open_dev *adev,
			 struct cras_fmt_conv *output_converter)
{
	struct cras_iodev *odev = adev->dev;
	unsigned int hw_level;
	struct timespec hw_tstamp;
	unsigned int frames, fr_to_req;
	snd_pcm_sframes_t written;
	snd_pcm_uframes_t total_written = 0;
	int rc;
	int non_empty = 0;
	int *non_empty_ptr = NULL;
	uint8_t *dst = NULL;
	struct cras_audio_area *area = NULL;

	/* Possibly fill zeros for no_stream state and possibly transit state.
	 */
	rc = cras_iodev_prepare_output_before_write_samples(odev);
	if (rc < 0) {
		syslog(LOG_ERR, "Failed to prepare output dev for write");
		return rc;
	}

	if (cras_iodev_state(odev) != CRAS_IODEV_STATE_NORMAL_RUN)
		return 0;

	rc = cras_iodev_frames_queued(odev, &hw_tstamp);
	if (rc < 0)
		return rc;
	hw_level = rc;

	ATLOG(atlog, AUDIO_THREAD_FILL_AUDIO_TSTAMP, adev->dev->info.idx,
	      hw_tstamp.tv_sec, hw_tstamp.tv_nsec);
	if (timespec_is_nonzero(&hw_tstamp)) {
		if (hw_level < odev->min_cb_level / 2)
			adev->coarse_rate_adjust = 1;
		else if (hw_level > odev->max_cb_level * 2)
			adev->coarse_rate_adjust = -1;
		else
			adev->coarse_rate_adjust = 0;

		if (cras_iodev_update_rate(odev, hw_level, &hw_tstamp))
			update_estimated_rate(adev);
	}
	ATLOG(atlog, AUDIO_THREAD_FILL_AUDIO, adev->dev->info.idx, hw_level, 0);

	/* Don't request more than hardware can hold. Note that min_buffer_level
	 * has been subtracted from the actual hw_level so we need to take it
	 * into account here. */
	fr_to_req = cras_iodev_buffer_avail(odev, hw_level);

	/* Have to loop writing to the device, will be at most 2 loops, this
	 * only happens when the circular buffer is at the end and returns us a
	 * partial area to write to from mmap_begin */
	while (total_written < fr_to_req) {
		frames = fr_to_req - total_written;
		rc = cras_iodev_get_output_buffer(odev, &area, &frames);
		if (rc < 0)
			return rc;

		/* TODO(dgreid) - This assumes interleaved audio. */
		dst = area->channels[0].buf;
		written = write_streams(odevs, adev, dst, frames);
		if (written < 0) /* pcm has been closed */
			return (int)written;

		if (written < (snd_pcm_sframes_t)frames)
			/* Got all the samples from client that we can, but it
			 * won't fill the request. */
			fr_to_req = 0; /* break out after committing samples */

		// This interval is lazily initialized once per device.
		// Note that newly opened devices are considered non-empty
		// (until their status is updated through the normal flow).
		if (!adev->non_empty_check_pi) {
			adev->non_empty_check_pi = pic_polled_interval_create(
				NON_EMPTY_UPDATE_INTERVAL_SEC);
		}

		// If we were empty last iteration, or the sampling interval
		// has elapsed, check for emptiness.
		if (adev->empty_pi ||
		    pic_interval_elapsed(adev->non_empty_check_pi)) {
			non_empty_ptr = &non_empty;
			pic_interval_reset(adev->non_empty_check_pi);
		}

		rc = cras_iodev_put_output_buffer(
			odev, dst, written, non_empty_ptr, output_converter);

		if (rc < 0)
			return rc;
		total_written += written;

		if (non_empty && adev->empty_pi) {
			// We're not empty, but we were previously.
			// Reset the empty period.
			pic_polled_interval_destroy(&adev->empty_pi);
		}

		if (non_empty_ptr && !non_empty && !adev->empty_pi)
			// We checked for emptiness, we were empty, and we
			// previously weren't. Start the empty period.
			adev->empty_pi = pic_polled_interval_create(
				MIN_EMPTY_PERIOD_SEC);
	}

	ATLOG(atlog, AUDIO_THREAD_FILL_AUDIO_DONE, hw_level, total_written,
	      odev->min_cb_level);

	return total_written;
}

/*
 * Chooses the smallest difference between hw_level and min_cb_level as the
 * drop time.
 */
static void get_input_devices_drop_time(struct open_dev *idev_list,
					struct timespec *reset_ts)
{
	struct open_dev *adev;
	struct cras_iodev *iodev;
	struct timespec tmp;
	struct timespec hw_tstamp;
	double est_rate;
	unsigned int target_level;
	bool is_set = false;
	int rc;

	DL_FOREACH (idev_list, adev) {
		iodev = adev->dev;
		if (!input_devices_can_drop_samples(iodev))
			continue;

		rc = cras_iodev_frames_queued(iodev, &hw_tstamp);
		if (rc < 0) {
			syslog(LOG_ERR, "Get frames from device %d, rc = %d",
			       iodev->info.idx, rc);
			continue;
		}

		target_level = iodev->min_cb_level;
		if (rc <= target_level) {
			reset_ts->tv_sec = 0;
			reset_ts->tv_nsec = 0;
			return;
		}
		est_rate = iodev->format->frame_rate *
			   cras_iodev_get_est_rate_ratio(iodev);
		cras_frames_to_time(rc - target_level, est_rate, &tmp);

		if (!is_set || timespec_after(reset_ts, &tmp)) {
			*reset_ts = tmp;
			is_set = true;
		}
	}
}

/*
 * Drop samples from all input devices.
 */
static void dev_io_drop_samples(struct open_dev *idev_list)
{
	struct open_dev *adev;
	struct timespec drop_time;
	int rc;

	get_input_devices_drop_time(idev_list, &drop_time);
	ATLOG(atlog, AUDIO_THREAD_CAPTURE_DROP_TIME, drop_time.tv_sec,
	      drop_time.tv_nsec, 0);

	if (timespec_is_zero(&drop_time))
		return;

	DL_FOREACH (idev_list, adev) {
		if (!input_devices_can_drop_samples(adev->dev))
			continue;

		rc = cras_iodev_drop_frames_by_time(adev->dev, drop_time);
		if (rc < 0) {
			syslog(LOG_ERR,
			       "Failed to drop frames from device %d, rc = %d",
			       adev->dev->info.idx, rc);
			continue;
		}
	}

	cras_audio_thread_event_drop_samples();

	return;
}

/*
 * Public funcitons.
 */

int dev_io_send_captured_samples(struct open_dev *idev_list)
{
	struct open_dev *adev;
	bool need_to_drop = false;
	int rc;

	// TODO(dgreid) - once per rstream, not once per dev_stream.
	DL_FOREACH (idev_list, adev) {
		struct dev_stream *stream;

		if (!cras_iodev_is_open(adev->dev))
			continue;

		/* Post samples to rstream if there are enough samples. */
		DL_FOREACH (adev->dev->streams, stream) {
			dev_stream_capture_update_rstream(stream);
		}

		/* Set wake_ts for this device. */
		rc = set_input_dev_wake_ts(adev, &need_to_drop);
		if (rc < 0)
			return rc;
	}

	if (need_to_drop)
		dev_io_drop_samples(idev_list);

	return 0;
}

static void handle_dev_err(int err_rc, struct open_dev **odevs,
			   struct open_dev *adev)
{
	if (err_rc == -EPIPE) {
		/* Handle severe underrun. */
		ATLOG(atlog, AUDIO_THREAD_SEVERE_UNDERRUN, adev->dev->info.idx,
		      0, 0);
		cras_iodev_reset_request(adev->dev);
	}
	/* Device error, remove it. */
	dev_io_rm_open_dev(odevs, adev);
}

int dev_io_capture(struct open_dev **list)
{
	struct open_dev *idev_list = *list;
	struct open_dev *adev;
	int rc;

	DL_FOREACH (idev_list, adev) {
		if (!cras_iodev_is_open(adev->dev))
			continue;
		rc = capture_to_streams(adev);
		if (rc < 0)
			handle_dev_err(rc, list, adev);
	}

	return 0;
}

/* If it is the time to fetch, start dev_stream. */
static void dev_io_check_dev_stream_start(struct open_dev *adev)
{
	struct dev_stream *dev_stream;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);

	DL_FOREACH (adev->dev->streams, dev_stream) {
		if (!is_time_to_fetch(dev_stream, now))
			continue;
		if (!dev_stream_is_running(dev_stream))
			cras_iodev_start_stream(adev->dev, dev_stream);
	}
}

void dev_io_playback_fetch(struct open_dev *odev_list)
{
	struct open_dev *adev;

	/* Check whether it is the time to start dev_stream before fetching. */
	DL_FOREACH (odev_list, adev) {
		if (!cras_iodev_is_open(adev->dev))
			continue;
		dev_io_check_dev_stream_start(adev);
	}

	DL_FOREACH (odev_list, adev) {
		if (!cras_iodev_is_open(adev->dev))
			continue;
		fetch_streams(adev);
	}
}

int dev_io_playback_write(struct open_dev **odevs,
			  struct cras_fmt_conv *output_converter)
{
	struct open_dev *adev;
	struct dev_stream *curr;
	int rc;
	unsigned int hw_level, total_written;

	/* For multiple output case, update the number of queued frames in shm
	 * of all streams before starting write output samples. */
	adev = *odevs;
	if (adev && adev->next) {
		DL_FOREACH (*odevs, adev) {
			DL_FOREACH (adev->dev->streams, curr)
				dev_stream_update_frames(curr);
		}
	}

	DL_FOREACH (*odevs, adev) {
		if (!cras_iodev_is_open(adev->dev))
			continue;

		rc = write_output_samples(odevs, adev, output_converter);
		if (rc < 0) {
			handle_dev_err(rc, odevs, adev);
		} else {
			total_written = rc;

			/*
			 * Skip the underrun check and device wake up time update if
			 * device should not wake up.
			 */
			if (!cras_iodev_odev_should_wake(adev->dev))
				continue;

			/*
			 * Update device wake up time and get the new hardware
			 * level.
			 */
			update_dev_wakeup_time(adev, &hw_level);

			/*
			 * If new hardware level is less than or equal to the
			 * written frames, we can suppose underrun happened. But
			 * keep in mind there may have a false positive. If
			 * hardware level changed just after frames being
			 * written, we may get hw_level <= total_written here
			 * without underrun happened. However, we can still
			 * treat it as underrun because it is an abnormal state
			 * we should handle it.
			 */
			if (hw_level <= total_written) {
				ATLOG(atlog, AUDIO_THREAD_UNDERRUN,
				      adev->dev->info.idx, hw_level,
				      total_written);
				rc = cras_iodev_output_underrun(adev->dev);
				if (rc < 0) {
					handle_dev_err(rc, odevs, adev);
				} else {
					update_dev_wakeup_time(adev, &hw_level);
				}
			}
		}
	}

	/* TODO(dgreid) - once per rstream, not once per dev_stream. */
	DL_FOREACH (*odevs, adev) {
		struct dev_stream *stream;
		if (!cras_iodev_is_open(adev->dev))
			continue;
		DL_FOREACH (adev->dev->streams, stream) {
			dev_stream_playback_update_rstream(stream);
		}
	}

	return 0;
}

static void update_longest_wake(struct open_dev *dev_list,
				const struct timespec *ts)
{
	struct open_dev *adev;
	struct timespec wake_interval;

	DL_FOREACH (dev_list, adev) {
		if (adev->dev->streams == NULL)
			continue;
		/*
		 * Calculate longest wake only when there's stream attached
		 * and the last wake time has been set.
		 */
		if (adev->last_wake.tv_sec) {
			subtract_timespecs(ts, &adev->last_wake,
					   &wake_interval);
			if (timespec_after(&wake_interval, &adev->longest_wake))
				adev->longest_wake = wake_interval;
		}
		adev->last_wake = *ts;
	}
}

void dev_io_run(struct open_dev **odevs, struct open_dev **idevs,
		struct cras_fmt_conv *output_converter)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	pic_update_current_time();
	update_longest_wake(*odevs, &now);
	update_longest_wake(*idevs, &now);

	dev_io_playback_fetch(*odevs);
	dev_io_capture(idevs);
	dev_io_send_captured_samples(*idevs);
	dev_io_playback_write(odevs, output_converter);

	check_non_empty_state_transition(*odevs);
}

static int input_adev_ignore_wake(const struct open_dev *adev)
{
	if (!cras_iodev_is_open(adev->dev))
		return 1;

	if (!adev->dev->active_node)
		return 1;

	if (adev->dev->active_node->type == CRAS_NODE_TYPE_HOTWORD &&
	    !cras_iodev_input_streaming(adev->dev))
		return 1;

	return 0;
}

int dev_io_next_input_wake(struct open_dev **idevs, struct timespec *min_ts)
{
	struct open_dev *adev;
	int ret = 0; /* The total number of devices to wait on. */

	DL_FOREACH (*idevs, adev) {
		if (input_adev_ignore_wake(adev))
			continue;
		ret++;
		ATLOG(atlog, AUDIO_THREAD_DEV_SLEEP_TIME, adev->dev->info.idx,
		      adev->wake_ts.tv_sec, adev->wake_ts.tv_nsec);
		if (timespec_after(min_ts, &adev->wake_ts))
			*min_ts = adev->wake_ts;
	}

	return ret;
}

/* Fills the time that the next stream needs to be serviced. */
static int get_next_stream_wake_from_list(struct dev_stream *streams,
					  struct timespec *min_ts)
{
	struct dev_stream *dev_stream;
	int ret = 0; /* The total number of streams to wait on. */

	DL_FOREACH (streams, dev_stream) {
		const struct timespec *next_cb_ts;

		if (cras_rstream_get_is_draining(dev_stream->stream))
			continue;

		if (cras_rstream_is_pending_reply(dev_stream->stream))
			continue;

		next_cb_ts = dev_stream_next_cb_ts(dev_stream);
		if (!next_cb_ts)
			continue;

		ATLOG(atlog, AUDIO_THREAD_STREAM_SLEEP_TIME,
		      dev_stream->stream->stream_id, next_cb_ts->tv_sec,
		      next_cb_ts->tv_nsec);
		if (timespec_after(min_ts, next_cb_ts))
			*min_ts = *next_cb_ts;
		ret++;
	}

	return ret;
}

int dev_io_next_output_wake(struct open_dev **odevs, struct timespec *min_ts,
			    const struct timespec *now)
{
	struct open_dev *adev;
	int ret = 0;

	DL_FOREACH (*odevs, adev)
		ret += get_next_stream_wake_from_list(adev->dev->streams,
						      min_ts);

	DL_FOREACH (*odevs, adev) {
		if (!cras_iodev_odev_should_wake(adev->dev))
			continue;

		ret++;
		if (timespec_after(min_ts, &adev->wake_ts))
			*min_ts = adev->wake_ts;
	}

	return ret;
}

struct open_dev *dev_io_find_open_dev(struct open_dev *odev_list,
				      unsigned int dev_idx)
{
	struct open_dev *odev;
	DL_FOREACH (odev_list, odev)
		if (odev->dev->info.idx == dev_idx)
			return odev;
	return NULL;
}

void dev_io_rm_open_dev(struct open_dev **odev_list, struct open_dev *dev_to_rm)
{
	struct open_dev *odev;
	struct dev_stream *dev_stream;

	/* Do nothing if dev_to_rm wasn't already in the active dev list. */
	DL_FOREACH (*odev_list, odev) {
		if (odev == dev_to_rm)
			break;
	}
	if (!odev)
		return;

	DL_DELETE(*odev_list, dev_to_rm);

	/* Metrics logs the number of underruns of this device. */
	cras_server_metrics_num_underruns(
		cras_iodev_get_num_underruns(dev_to_rm->dev));

	/* Metrics logs the delay of this device. */
	cras_server_metrics_highest_device_delay(
		dev_to_rm->dev->highest_hw_level,
		dev_to_rm->dev->largest_cb_level, dev_to_rm->dev->direction);

	/* Metrics logs the highest_hw_level of this device. */
	cras_server_metrics_highest_hw_level(dev_to_rm->dev->highest_hw_level,
					     dev_to_rm->dev->direction);

	check_non_empty_state_transition(*odev_list);

	ATLOG(atlog, AUDIO_THREAD_DEV_REMOVED, dev_to_rm->dev->info.idx, 0, 0);

	DL_FOREACH (dev_to_rm->dev->streams, dev_stream) {
		cras_iodev_rm_stream(dev_to_rm->dev, dev_stream->stream);
		dev_stream_destroy(dev_stream);
	}

	if (dev_to_rm->empty_pi)
		pic_polled_interval_destroy(&dev_to_rm->empty_pi);
	if (dev_to_rm->non_empty_check_pi)
		pic_polled_interval_destroy(&dev_to_rm->non_empty_check_pi);
	free(dev_to_rm);
}

static void delete_stream_from_dev(struct cras_iodev *dev,
				   struct cras_rstream *stream)
{
	struct dev_stream *out;

	out = cras_iodev_rm_stream(dev, stream);
	if (out)
		dev_stream_destroy(out);
}

int dev_io_append_stream(struct open_dev **dev_list,
			 struct cras_rstream *stream,
			 struct cras_iodev **iodevs, unsigned int num_iodevs)
{
	struct open_dev *open_dev;
	struct cras_iodev *dev;
	struct dev_stream *out;
	struct timespec init_cb_ts;
	struct timespec extra_sleep;
	const struct timespec *stream_ts;
	unsigned int i;
	bool cb_ts_set = false;
	int level;
	int rc = 0;

	for (i = 0; i < num_iodevs; i++) {
		DL_SEARCH_SCALAR(*dev_list, open_dev, dev, iodevs[i]);
		if (!open_dev)
			continue;

		dev = iodevs[i];
		DL_SEARCH_SCALAR(dev->streams, out, stream, stream);
		if (out)
			continue;

		/*
		 * When dev transitions from no stream to the 1st stream, reset
		 * last_wake and longest_wake so it can start over the tracking.
		 */
		if (dev->streams == NULL) {
			open_dev->last_wake.tv_sec = 0;
			open_dev->last_wake.tv_nsec = 0;
			open_dev->longest_wake.tv_sec = 0;
			open_dev->longest_wake.tv_nsec = 0;
		}

		/*
		 * When the first input stream is added, flush the input buffer
		 * so that we can read from multiple input devices of the same
		 * buffer level.
		 */
		if ((stream->direction == CRAS_STREAM_INPUT) && !dev->streams) {
			int num_flushed = dev->flush_buffer(dev);
			if (num_flushed < 0) {
				rc = num_flushed;
				break;
			}
		}

		/*
		 * For output, if open device already has stream, get the earliest next
		 * callback time from these streams to align with. Otherwise, check whether
		 * there are remaining frames in the device. Set the initial callback time to
		 * the time when hw_level of device is close to min_cb_level.
		 * If next callback time is too far from now, it will block writing and
		 * lower hardware level. Else if we fetch the new stream immediately, it
		 * may cause device buffer level stack up.
		 */
		if (stream->direction == CRAS_STREAM_OUTPUT) {
			DL_FOREACH (dev->streams, out) {
				stream_ts = dev_stream_next_cb_ts(out);
				if (stream_ts &&
				    (!cb_ts_set ||
				     timespec_after(&init_cb_ts, stream_ts))) {
					init_cb_ts = *stream_ts;
					cb_ts_set = true;
				}
			}
			if (!cb_ts_set) {
				level = cras_iodev_get_valid_frames(
					dev, &init_cb_ts);
				if (level < 0) {
					syslog(LOG_ERR,
					       "Failed to set output init_cb_ts, rc = %d",
					       level);
					rc = -EINVAL;
					break;
				}
				level -= cras_frames_at_rate(
					stream->format.frame_rate,
					cras_rstream_get_cb_threshold(stream),
					dev->format->frame_rate);
				if (level < 0)
					level = 0;
				cras_frames_to_time(level,
						    dev->format->frame_rate,
						    &extra_sleep);
				add_timespecs(&init_cb_ts, &extra_sleep);
			}
		} else {
			/*
			 * For input streams, because audio thread can calculate wake up time
			 * by hw_level of input device, set the first cb_ts to zero. The stream
			 * will wake up when it gets enough samples to post. The next_cb_ts will
			 * be updated after its first post.
			 *
			 * TODO(yuhsuan) - Align the new stream fetch time to avoid getting a large
			 * delay. If a new stream with smaller block size starts when the hardware
			 * level is high, the hardware level will keep high after removing other
			 * old streams.
			 */
			init_cb_ts.tv_sec = 0;
			init_cb_ts.tv_nsec = 0;
		}

		out = dev_stream_create(stream, dev->info.idx, dev->format, dev,
					&init_cb_ts);
		if (!out) {
			rc = -EINVAL;
			break;
		}

		cras_iodev_add_stream(dev, out);

		/*
		 * For multiple inputs case, if the new stream is not the first
		 * one to append, copy the 1st stream's offset to it so that
		 * future read offsets can be aligned across all input streams
		 * to avoid the deadlock scenario when multiple streams reading
		 * from multiple devices.
		 */
		if ((stream->direction == CRAS_STREAM_INPUT) &&
		    (dev->streams != out)) {
			unsigned int offset =
				cras_iodev_stream_offset(dev, dev->streams);
			if (offset > stream->cb_threshold)
				offset = stream->cb_threshold;
			cras_iodev_stream_written(dev, out, offset);

			offset = cras_rstream_dev_offset(dev->streams->stream,
							 dev->info.idx);
			if (offset > stream->cb_threshold)
				offset = stream->cb_threshold;
			cras_rstream_dev_offset_update(stream, offset,
						       dev->info.idx);
		}
		ATLOG(atlog, AUDIO_THREAD_STREAM_ADDED, stream->stream_id,
		      dev->info.idx, 0);
	}

	if (rc) {
		DL_FOREACH (*dev_list, open_dev) {
			dev = open_dev->dev;
			DL_SEARCH_SCALAR(dev->streams, out, stream, stream);
			if (!out)
				continue;

			cras_iodev_rm_stream(dev, stream);
			dev_stream_destroy(out);
		}
	}

	return rc;
}

int dev_io_remove_stream(struct open_dev **dev_list,
			 struct cras_rstream *stream, struct cras_iodev *dev)
{
	struct open_dev *open_dev;
	struct timespec delay;
	unsigned fetch_delay_msec;

	/* Metrics log the longest fetch delay of this stream. */
	if (timespec_after(&stream->longest_fetch_interval,
			   &stream->sleep_interval_ts)) {
		subtract_timespecs(&stream->longest_fetch_interval,
				   &stream->sleep_interval_ts, &delay);
		fetch_delay_msec =
			delay.tv_sec * 1000 + delay.tv_nsec / 1000000;
		if (fetch_delay_msec)
			cras_server_metrics_longest_fetch_delay(
				fetch_delay_msec);
	}

	ATLOG(atlog, AUDIO_THREAD_STREAM_REMOVED, stream->stream_id, 0, 0);

	if (dev == NULL) {
		DL_FOREACH (*dev_list, open_dev) {
			delete_stream_from_dev(open_dev->dev, stream);
		}
	} else {
		delete_stream_from_dev(dev, stream);
	}

	return 0;
}
