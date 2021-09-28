/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * `dev_io` Handles playback to and capture from open devices.  It runs only on
 * the audio thread.
 */

#ifndef DEV_IO_H_
#define DEV_IO_H_

#include "cras_iodev.h"
#include "cras_types.h"
#include "polled_interval_checker.h"

/*
 * Open input/output devices.
 *    dev - The device.
 *    last_wake - The last timestamp audio thread woke up and there is stream
 *        on this open device.
 *    longest_wake - The longest time between consecutive audio thread wakes
 *        in this open_dev's life cycle.
 *    wake_ts - When callback is needed to avoid xrun.
 *    last_non_empty_ts - The last time we know the device played/captured
 *        non-empty (zero) audio.
 *    coarse_rate_adjust - Hack for when the sample rate needs heavy correction.
 */
struct open_dev {
	struct cras_iodev *dev;
	struct timespec last_wake;
	struct timespec longest_wake;
	struct timespec wake_ts;
	struct polled_interval *non_empty_check_pi;
	struct polled_interval *empty_pi;
	int coarse_rate_adjust;
	struct open_dev *prev, *next;
};

/*
 * Fetches streams from each device in `odev_list`.
 *    odev_list - The list of open devices.
 */
void dev_io_playback_fetch(struct open_dev *odev_list);

/*
 * Writes the samples fetched from the streams to the playback devices.
 *    odev_list - The list of open devices.  Devices will be removed when
 *                writing returns an error.
 */
int dev_io_playback_write(struct open_dev **odevs,
			  struct cras_fmt_conv *output_converter);

/* Only public for testing. */
int write_output_samples(struct open_dev **odevs, struct open_dev *adev,
			 struct cras_fmt_conv *output_converter);

/*
 * Captures samples from each device in the list.
 *    list - Pointer to the list of input devices.  Devices that fail to read
 *           will be removed from the list.
 */
int dev_io_capture(struct open_dev **list);

/*
 * Send samples that have been captured to their streams.
 */
int dev_io_send_captured_samples(struct open_dev *idev_list);

/* Reads and/or writes audio samples from/to the devices. */
void dev_io_run(struct open_dev **odevs, struct open_dev **idevs,
		struct cras_fmt_conv *output_converter);

/*
 * Fills min_ts with the next time the system should wake to service input.
 * Returns the number of devices waiting.
 */
int dev_io_next_input_wake(struct open_dev **idevs, struct timespec *min_ts);

/*
 * Fills min_ts with the next time the system should wake to service output.
 * Returns the number of devices waiting.
 */
int dev_io_next_output_wake(struct open_dev **odevs, struct timespec *min_ts,
			    const struct timespec *now);

/*
 * Removes a device from a list of devices.
 *    odev_list - A pointer to the list to modify.
 *    dev_to_rm - Find this device in the list and remove it.
 */
void dev_io_rm_open_dev(struct open_dev **odev_list,
			struct open_dev *dev_to_rm);

/* Returns a pointer to an open_dev if it is in the list, otherwise NULL. */
struct open_dev *dev_io_find_open_dev(struct open_dev *odev_list,
				      unsigned int dev_idx);

/* Append a new stream to a specified set of iodevs. */
int dev_io_append_stream(struct open_dev **dev_list,
			 struct cras_rstream *stream,
			 struct cras_iodev **iodevs, unsigned int num_iodevs);

/* Remove a stream from the provided list of devices. */
int dev_io_remove_stream(struct open_dev **dev_list,
			 struct cras_rstream *stream, struct cras_iodev *dev);

#endif /* DEV_IO_H_ */
