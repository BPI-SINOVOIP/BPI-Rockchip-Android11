/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_SERVER_METRICS_H_
#define CRAS_SERVER_METRICS_H_

#include <stdbool.h>

#include "cras_iodev.h"
#include "cras_rstream.h"

extern const char kNoCodecsFoundMetric[];

/* Logs if connected HFP headset supports wideband speech. */
int cras_server_metrics_hfp_wideband_support(bool supported);

/* Logs the number of packet loss per 1000 packets under HFP capture. */
int cras_server_metrics_hfp_packet_loss(float packet_loss_ratio);

/* Logs runtime of a device. */
int cras_server_metrics_device_runtime(struct cras_iodev *iodev);

/* Logs the highest delay time of a device. */
int cras_server_metrics_highest_device_delay(
	unsigned int hw_level, unsigned int largest_cb_level,
	enum CRAS_STREAM_DIRECTION direction);

/* Logs the highest hardware level of a device. */
int cras_server_metrics_highest_hw_level(unsigned hw_level,
					 enum CRAS_STREAM_DIRECTION direction);

/* Logs the longest fetch delay of a stream in millisecond. */
int cras_server_metrics_longest_fetch_delay(unsigned delay_msec);

/* Logs the number of underruns of a device. */
int cras_server_metrics_num_underruns(unsigned num_underruns);

/* Logs the frequency of missed callback. */
int cras_server_metrics_missed_cb_frequency(const struct cras_rstream *stream);

/* Logs the missed callback event. */
int cras_server_metrics_missed_cb_event(const struct cras_rstream *stream);

/* Logs the stream configurations from clients. */
int cras_server_metrics_stream_config(struct cras_rstream_config *config);

/* Logs the number of busyloops for different time periods. */
int cras_server_metrics_busyloop(struct timespec *ts, unsigned count);

/* Initialize metrics logging stuff. */
int cras_server_metrics_init();

#endif /* CRAS_SERVER_METRICS_H_ */
