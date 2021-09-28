/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#ifdef CRAS_DBUS
#include "cras_bt_io.h"
#endif
#include "cras_iodev.h"
#include "cras_metrics.h"
#include "cras_main_message.h"
#include "cras_rstream.h"
#include "cras_system_state.h"

#define METRICS_NAME_BUFFER_SIZE 50

const char kBusyloop[] = "Cras.Busyloop";
const char kDeviceTypeInput[] = "Cras.DeviceTypeInput";
const char kDeviceTypeOutput[] = "Cras.DeviceTypeOutput";
const char kHighestDeviceDelayInput[] = "Cras.HighestDeviceDelayInput";
const char kHighestDeviceDelayOutput[] = "Cras.HighestDeviceDelayOutput";
const char kHighestInputHardwareLevel[] = "Cras.HighestInputHardwareLevel";
const char kHighestOutputHardwareLevel[] = "Cras.HighestOutputHardwareLevel";
const char kMissedCallbackFirstTimeInput[] =
	"Cras.MissedCallbackFirstTimeInput";
const char kMissedCallbackFirstTimeOutput[] =
	"Cras.MissedCallbackFirstTimeOutput";
const char kMissedCallbackFrequencyInput[] =
	"Cras.MissedCallbackFrequencyInput";
const char kMissedCallbackFrequencyOutput[] =
	"Cras.MissedCallbackFrequencyOutput";
const char kMissedCallbackFrequencyAfterReschedulingInput[] =
	"Cras.MissedCallbackFrequencyAfterReschedulingInput";
const char kMissedCallbackFrequencyAfterReschedulingOutput[] =
	"Cras.MissedCallbackFrequencyAfterReschedulingOutput";
const char kMissedCallbackSecondTimeInput[] =
	"Cras.MissedCallbackSecondTimeInput";
const char kMissedCallbackSecondTimeOutput[] =
	"Cras.MissedCallbackSecondTimeOutput";
const char kNoCodecsFoundMetric[] = "Cras.NoCodecsFoundAtBoot";
const char kStreamTimeoutMilliSeconds[] = "Cras.StreamTimeoutMilliSeconds";
const char kStreamCallbackThreshold[] = "Cras.StreamCallbackThreshold";
const char kStreamClientTypeInput[] = "Cras.StreamClientTypeInput";
const char kStreamClientTypeOutput[] = "Cras.StreamClientTypeOutput";
const char kStreamFlags[] = "Cras.StreamFlags";
const char kStreamSamplingFormat[] = "Cras.StreamSamplingFormat";
const char kStreamSamplingRate[] = "Cras.StreamSamplingRate";
const char kUnderrunsPerDevice[] = "Cras.UnderrunsPerDevice";
const char kHfpWidebandSpeechSupported[] = "Cras.HfpWidebandSpeechSupported";
const char kHfpWidebandSpeechPacketLoss[] = "Cras.HfpWidebandSpeechPacketLoss";

/*
 * Records missed callback frequency only when the runtime of stream is larger
 * than the threshold.
 */
const double MISSED_CB_FREQUENCY_SECONDS_MIN = 10.0;

const time_t CRAS_METRICS_SHORT_PERIOD_THRESHOLD_SECONDS = 600;
const time_t CRAS_METRICS_LONG_PERIOD_THRESHOLD_SECONDS = 3600;

static const char *get_timespec_period_str(struct timespec ts)
{
	if (ts.tv_sec < CRAS_METRICS_SHORT_PERIOD_THRESHOLD_SECONDS)
		return "ShortPeriod";
	if (ts.tv_sec < CRAS_METRICS_LONG_PERIOD_THRESHOLD_SECONDS)
		return "MediumPeriod";
	return "LongPeriod";
}

/* Type of metrics to log. */
enum CRAS_SERVER_METRICS_TYPE {
	BT_WIDEBAND_PACKET_LOSS,
	BT_WIDEBAND_SUPPORTED,
	BUSYLOOP,
	DEVICE_RUNTIME,
	HIGHEST_DEVICE_DELAY_INPUT,
	HIGHEST_DEVICE_DELAY_OUTPUT,
	HIGHEST_INPUT_HW_LEVEL,
	HIGHEST_OUTPUT_HW_LEVEL,
	LONGEST_FETCH_DELAY,
	MISSED_CB_FIRST_TIME_INPUT,
	MISSED_CB_FIRST_TIME_OUTPUT,
	MISSED_CB_FREQUENCY_INPUT,
	MISSED_CB_FREQUENCY_OUTPUT,
	MISSED_CB_FREQUENCY_AFTER_RESCHEDULING_INPUT,
	MISSED_CB_FREQUENCY_AFTER_RESCHEDULING_OUTPUT,
	MISSED_CB_SECOND_TIME_INPUT,
	MISSED_CB_SECOND_TIME_OUTPUT,
	NUM_UNDERRUNS,
	STREAM_CONFIG
};

enum CRAS_METRICS_DEVICE_TYPE {
	/* Output devices. */
	CRAS_METRICS_DEVICE_INTERNAL_SPEAKER,
	CRAS_METRICS_DEVICE_HEADPHONE,
	CRAS_METRICS_DEVICE_HDMI,
	CRAS_METRICS_DEVICE_HAPTIC,
	CRAS_METRICS_DEVICE_LINEOUT,
	/* Input devices. */
	CRAS_METRICS_DEVICE_INTERNAL_MIC,
	CRAS_METRICS_DEVICE_FRONT_MIC,
	CRAS_METRICS_DEVICE_REAR_MIC,
	CRAS_METRICS_DEVICE_KEYBOARD_MIC,
	CRAS_METRICS_DEVICE_MIC,
	CRAS_METRICS_DEVICE_HOTWORD,
	CRAS_METRICS_DEVICE_POST_MIX_LOOPBACK,
	CRAS_METRICS_DEVICE_POST_DSP_LOOPBACK,
	/* Devices supporting input and output function. */
	CRAS_METRICS_DEVICE_USB,
	CRAS_METRICS_DEVICE_A2DP,
	CRAS_METRICS_DEVICE_HFP,
	CRAS_METRICS_DEVICE_HSP,
	CRAS_METRICS_DEVICE_BLUETOOTH,
	CRAS_METRICS_DEVICE_NO_DEVICE,
	CRAS_METRICS_DEVICE_NORMAL_FALLBACK,
	CRAS_METRICS_DEVICE_ABNORMAL_FALLBACK,
	CRAS_METRICS_DEVICE_SILENT_HOTWORD,
	CRAS_METRICS_DEVICE_UNKNOWN,
};

struct cras_server_metrics_stream_config {
	enum CRAS_STREAM_DIRECTION direction;
	unsigned cb_threshold;
	unsigned flags;
	int format;
	unsigned rate;
	enum CRAS_CLIENT_TYPE client_type;
};

struct cras_server_metrics_device_data {
	enum CRAS_METRICS_DEVICE_TYPE type;
	enum CRAS_STREAM_DIRECTION direction;
	struct timespec runtime;
};

struct cras_server_metrics_timespec_data {
	struct timespec runtime;
	unsigned count;
};

union cras_server_metrics_data {
	unsigned value;
	struct cras_server_metrics_stream_config stream_config;
	struct cras_server_metrics_device_data device_data;
	struct cras_server_metrics_timespec_data timespec_data;
};

/*
 * Make sure the size of message in the acceptable range. Otherwise, it may
 * be split into mutiple packets while sending.
 */
static_assert(sizeof(union cras_server_metrics_data) <= 256,
	      "The size is too large.");

struct cras_server_metrics_message {
	struct cras_main_message header;
	enum CRAS_SERVER_METRICS_TYPE metrics_type;
	union cras_server_metrics_data data;
};

static void init_server_metrics_msg(struct cras_server_metrics_message *msg,
				    enum CRAS_SERVER_METRICS_TYPE type,
				    union cras_server_metrics_data data)
{
	memset(msg, 0, sizeof(*msg));
	msg->header.type = CRAS_MAIN_METRICS;
	msg->header.length = sizeof(*msg);
	msg->metrics_type = type;
	msg->data = data;
}

static void handle_metrics_message(struct cras_main_message *msg, void *arg);

/* The wrapper function of cras_main_message_send. */
static int cras_server_metrics_message_send(struct cras_main_message *msg)
{
	/* If current function is in the main thread, call handler directly. */
	if (cras_system_state_in_main_thread()) {
		handle_metrics_message(msg, NULL);
		return 0;
	}
	return cras_main_message_send(msg);
}

static inline const char *
metrics_device_type_str(enum CRAS_METRICS_DEVICE_TYPE device_type)
{
	switch (device_type) {
	case CRAS_METRICS_DEVICE_INTERNAL_SPEAKER:
		return "InternalSpeaker";
	case CRAS_METRICS_DEVICE_HEADPHONE:
		return "Headphone";
	case CRAS_METRICS_DEVICE_HDMI:
		return "HDMI";
	case CRAS_METRICS_DEVICE_HAPTIC:
		return "Haptic";
	case CRAS_METRICS_DEVICE_LINEOUT:
		return "Lineout";
	/* Input devices. */
	case CRAS_METRICS_DEVICE_INTERNAL_MIC:
		return "InternalMic";
	case CRAS_METRICS_DEVICE_FRONT_MIC:
		return "FrontMic";
	case CRAS_METRICS_DEVICE_REAR_MIC:
		return "RearMic";
	case CRAS_METRICS_DEVICE_KEYBOARD_MIC:
		return "KeyboardMic";
	case CRAS_METRICS_DEVICE_MIC:
		return "Mic";
	case CRAS_METRICS_DEVICE_HOTWORD:
		return "Hotword";
	case CRAS_METRICS_DEVICE_POST_MIX_LOOPBACK:
		return "PostMixLoopback";
	case CRAS_METRICS_DEVICE_POST_DSP_LOOPBACK:
		return "PostDspLoopback";
	/* Devices supporting input and output function. */
	case CRAS_METRICS_DEVICE_USB:
		return "USB";
	case CRAS_METRICS_DEVICE_A2DP:
		return "A2DP";
	case CRAS_METRICS_DEVICE_HFP:
		return "HFP";
	case CRAS_METRICS_DEVICE_HSP:
		return "HSP";
	case CRAS_METRICS_DEVICE_BLUETOOTH:
		return "Bluetooth";
	case CRAS_METRICS_DEVICE_NO_DEVICE:
		return "NoDevice";
	/* Other dummy devices. */
	case CRAS_METRICS_DEVICE_NORMAL_FALLBACK:
		return "NormalFallback";
	case CRAS_METRICS_DEVICE_ABNORMAL_FALLBACK:
		return "AbnormalFallback";
	case CRAS_METRICS_DEVICE_SILENT_HOTWORD:
		return "SilentHotword";
	case CRAS_METRICS_DEVICE_UNKNOWN:
		return "Unknown";
	default:
		return "InvalidType";
	}
}

static enum CRAS_METRICS_DEVICE_TYPE
get_metrics_device_type(struct cras_iodev *iodev)
{
	/* Check whether it is a special device. */
	if (iodev->info.idx < MAX_SPECIAL_DEVICE_IDX) {
		switch (iodev->info.idx) {
		case NO_DEVICE:
			syslog(LOG_ERR, "The invalid device has been used.");
			return CRAS_METRICS_DEVICE_NO_DEVICE;
		case SILENT_RECORD_DEVICE:
		case SILENT_PLAYBACK_DEVICE:
			if (iodev->active_node->type ==
			    CRAS_NODE_TYPE_FALLBACK_NORMAL)
				return CRAS_METRICS_DEVICE_NORMAL_FALLBACK;
			else
				return CRAS_METRICS_DEVICE_ABNORMAL_FALLBACK;
		case SILENT_HOTWORD_DEVICE:
			return CRAS_METRICS_DEVICE_SILENT_HOTWORD;
		}
	}

	switch (iodev->active_node->type) {
	case CRAS_NODE_TYPE_INTERNAL_SPEAKER:
		return CRAS_METRICS_DEVICE_INTERNAL_SPEAKER;
	case CRAS_NODE_TYPE_HEADPHONE:
		return CRAS_METRICS_DEVICE_HEADPHONE;
	case CRAS_NODE_TYPE_HDMI:
		return CRAS_METRICS_DEVICE_HDMI;
	case CRAS_NODE_TYPE_HAPTIC:
		return CRAS_METRICS_DEVICE_HAPTIC;
	case CRAS_NODE_TYPE_LINEOUT:
		return CRAS_METRICS_DEVICE_LINEOUT;
	case CRAS_NODE_TYPE_MIC:
		switch (iodev->active_node->position) {
		case NODE_POSITION_INTERNAL:
			return CRAS_METRICS_DEVICE_INTERNAL_MIC;
		case NODE_POSITION_FRONT:
			return CRAS_METRICS_DEVICE_FRONT_MIC;
		case NODE_POSITION_REAR:
			return CRAS_METRICS_DEVICE_REAR_MIC;
		case NODE_POSITION_KEYBOARD:
			return CRAS_METRICS_DEVICE_KEYBOARD_MIC;
		case NODE_POSITION_EXTERNAL:
		default:
			return CRAS_METRICS_DEVICE_MIC;
		}
	case CRAS_NODE_TYPE_HOTWORD:
		return CRAS_METRICS_DEVICE_HOTWORD;
	case CRAS_NODE_TYPE_POST_MIX_PRE_DSP:
		return CRAS_METRICS_DEVICE_POST_MIX_LOOPBACK;
	case CRAS_NODE_TYPE_POST_DSP:
		return CRAS_METRICS_DEVICE_POST_DSP_LOOPBACK;
	case CRAS_NODE_TYPE_USB:
		return CRAS_METRICS_DEVICE_USB;
	case CRAS_NODE_TYPE_BLUETOOTH:
#ifdef CRAS_DBUS
		if (cras_bt_io_on_profile(iodev,
					  CRAS_BT_DEVICE_PROFILE_A2DP_SOURCE))
			return CRAS_METRICS_DEVICE_A2DP;
		if (cras_bt_io_on_profile(
			    iodev, CRAS_BT_DEVICE_PROFILE_HFP_AUDIOGATEWAY))
			return CRAS_METRICS_DEVICE_HFP;
		if (cras_bt_io_on_profile(
			    iodev, CRAS_BT_DEVICE_PROFILE_HSP_AUDIOGATEWAY))
			return CRAS_METRICS_DEVICE_HSP;
#endif
		return CRAS_METRICS_DEVICE_BLUETOOTH;
	case CRAS_NODE_TYPE_UNKNOWN:
	default:
		return CRAS_METRICS_DEVICE_UNKNOWN;
	}
}

int cras_server_metrics_hfp_packet_loss(float packet_loss_ratio)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	int err;

	/* Percentage is too coarse for packet loss, so we use number of bad
	 * packets per thousand packets instead. */
	data.value = (unsigned)(round(packet_loss_ratio * 1000));
	init_server_metrics_msg(&msg, BT_WIDEBAND_PACKET_LOSS, data);

	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: BT_WIDEBAND_PACKET_LOSS");
		return err;
	}
	return 0;
}

int cras_server_metrics_hfp_wideband_support(bool supported)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	int err;

	data.value = supported;
	init_server_metrics_msg(&msg, BT_WIDEBAND_SUPPORTED, data);

	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: BT_WIDEBAND_SUPPORTED");
		return err;
	}
	return 0;
}

int cras_server_metrics_device_runtime(struct cras_iodev *iodev)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	struct timespec now;
	int err;

	data.device_data.type = get_metrics_device_type(iodev);
	data.device_data.direction = iodev->direction;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	subtract_timespecs(&now, &iodev->open_ts, &data.device_data.runtime);

	init_server_metrics_msg(&msg, DEVICE_RUNTIME, data);

	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: DEVICE_RUNTIME");
		return err;
	}

	return 0;
}

int cras_server_metrics_highest_device_delay(
	unsigned int hw_level, unsigned int largest_cb_level,
	enum CRAS_STREAM_DIRECTION direction)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	int err;

	if (largest_cb_level == 0) {
		syslog(LOG_ERR,
		       "Failed to record device delay: devided by zero");
		return -1;
	}

	/*
	 * Because the latency depends on the callback threshold of streams, it
	 * should be calculated as dividing the highest hardware level by largest
	 * callback threshold of streams. For output device, this value should fall
	 * around 2 because CRAS 's scheduling maintain device buffer level around
	 * 1~2 minimum callback level. For input device, this value should be around
	 * 1 because the device buffer level is around 0~1 minimum callback level.
	 * Besides, UMA cannot record float so this ratio is multiplied by 1000.
	 */
	data.value = hw_level * 1000 / largest_cb_level;

	switch (direction) {
	case CRAS_STREAM_INPUT:
		init_server_metrics_msg(&msg, HIGHEST_DEVICE_DELAY_INPUT, data);
		break;
	case CRAS_STREAM_OUTPUT:
		init_server_metrics_msg(&msg, HIGHEST_DEVICE_DELAY_OUTPUT,
					data);
		break;
	default:
		return 0;
	}

	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: HIGHEST_DEVICE_DELAY");
		return err;
	}

	return 0;
}

int cras_server_metrics_highest_hw_level(unsigned hw_level,
					 enum CRAS_STREAM_DIRECTION direction)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	int err;

	data.value = hw_level;

	switch (direction) {
	case CRAS_STREAM_INPUT:
		init_server_metrics_msg(&msg, HIGHEST_INPUT_HW_LEVEL, data);
		break;
	case CRAS_STREAM_OUTPUT:
		init_server_metrics_msg(&msg, HIGHEST_OUTPUT_HW_LEVEL, data);
		break;
	default:
		return 0;
	}

	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: HIGHEST_HW_LEVEL");
		return err;
	}

	return 0;
}

int cras_server_metrics_longest_fetch_delay(unsigned delay_msec)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	int err;

	data.value = delay_msec;
	init_server_metrics_msg(&msg, LONGEST_FETCH_DELAY, data);
	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: LONGEST_FETCH_DELAY");
		return err;
	}

	return 0;
}

int cras_server_metrics_num_underruns(unsigned num_underruns)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	int err;

	data.value = num_underruns;
	init_server_metrics_msg(&msg, NUM_UNDERRUNS, data);
	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: NUM_UNDERRUNS");
		return err;
	}

	return 0;
}

int cras_server_metrics_missed_cb_frequency(const struct cras_rstream *stream)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	struct timespec now, time_since;
	double seconds, frequency;
	int err;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	subtract_timespecs(&now, &stream->start_ts, &time_since);
	seconds = (double)time_since.tv_sec + time_since.tv_nsec / 1000000000.0;

	/* Ignore streams which do not have enough runtime. */
	if (seconds < MISSED_CB_FREQUENCY_SECONDS_MIN)
		return 0;

	/* Compute how many callbacks are missed in a day. */
	frequency = (double)stream->num_missed_cb * 86400.0 / seconds;
	data.value = (unsigned)(round(frequency) + 1e-9);

	if (stream->direction == CRAS_STREAM_INPUT)
		init_server_metrics_msg(&msg, MISSED_CB_FREQUENCY_INPUT, data);
	else
		init_server_metrics_msg(&msg, MISSED_CB_FREQUENCY_OUTPUT, data);

	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: MISSED_CB_FREQUENCY");
		return err;
	}

	/*
	 * If missed callback happened at least once, also record frequncy after
	 * rescheduling.
	 */
	if (!stream->num_missed_cb)
		return 0;

	subtract_timespecs(&now, &stream->first_missed_cb_ts, &time_since);
	seconds = (double)time_since.tv_sec + time_since.tv_nsec / 1000000000.0;

	/* Compute how many callbacks are missed in a day. */
	frequency = (double)(stream->num_missed_cb - 1) * 86400.0 / seconds;
	data.value = (unsigned)(round(frequency) + 1e-9);

	if (stream->direction == CRAS_STREAM_INPUT) {
		init_server_metrics_msg(
			&msg, MISSED_CB_FREQUENCY_AFTER_RESCHEDULING_INPUT,
			data);
	} else {
		init_server_metrics_msg(
			&msg, MISSED_CB_FREQUENCY_AFTER_RESCHEDULING_OUTPUT,
			data);
	}

	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: MISSED_CB_FREQUENCY");
		return err;
	}

	return 0;
}

/*
 * Logs the duration between stream starting time and the first missed
 * callback.
 */
static int
cras_server_metrics_missed_cb_first_time(const struct cras_rstream *stream)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	struct timespec time_since;
	int err;

	subtract_timespecs(&stream->first_missed_cb_ts, &stream->start_ts,
			   &time_since);
	data.value = (unsigned)time_since.tv_sec;

	if (stream->direction == CRAS_STREAM_INPUT) {
		init_server_metrics_msg(&msg, MISSED_CB_FIRST_TIME_INPUT, data);
	} else {
		init_server_metrics_msg(&msg, MISSED_CB_FIRST_TIME_OUTPUT,
					data);
	}
	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR, "Failed to send metrics message: "
				"MISSED_CB_FIRST_TIME");
		return err;
	}

	return 0;
}

/* Logs the duration between the first and the second missed callback events. */
static int
cras_server_metrics_missed_cb_second_time(const struct cras_rstream *stream)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	struct timespec now, time_since;
	int err;

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	subtract_timespecs(&now, &stream->first_missed_cb_ts, &time_since);
	data.value = (unsigned)time_since.tv_sec;

	if (stream->direction == CRAS_STREAM_INPUT) {
		init_server_metrics_msg(&msg, MISSED_CB_SECOND_TIME_INPUT,
					data);
	} else {
		init_server_metrics_msg(&msg, MISSED_CB_SECOND_TIME_OUTPUT,
					data);
	}
	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR, "Failed to send metrics message: "
				"MISSED_CB_SECOND_TIME");
		return err;
	}

	return 0;
}

int cras_server_metrics_missed_cb_event(struct cras_rstream *stream)
{
	int rc = 0;

	stream->num_missed_cb += 1;
	if (stream->num_missed_cb == 1)
		clock_gettime(CLOCK_MONOTONIC_RAW, &stream->first_missed_cb_ts);

	/* Do not record missed cb if the stream has these flags. */
	if (stream->flags & (BULK_AUDIO_OK | USE_DEV_TIMING | TRIGGER_ONLY))
		return 0;

	/* Only record the first and the second events. */
	if (stream->num_missed_cb == 1)
		rc = cras_server_metrics_missed_cb_first_time(stream);
	else if (stream->num_missed_cb == 2)
		rc = cras_server_metrics_missed_cb_second_time(stream);

	return rc;
}

int cras_server_metrics_stream_config(struct cras_rstream_config *config)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	int err;

	data.stream_config.direction = config->direction;
	data.stream_config.cb_threshold = (unsigned)config->cb_threshold;
	data.stream_config.flags = (unsigned)config->flags;
	data.stream_config.format = (int)config->format->format;
	data.stream_config.rate = (unsigned)config->format->frame_rate;
	data.stream_config.client_type = config->client_type;

	init_server_metrics_msg(&msg, STREAM_CONFIG, data);
	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR,
		       "Failed to send metrics message: STREAM_CONFIG");
		return err;
	}

	return 0;
}

int cras_server_metrics_busyloop(struct timespec *ts, unsigned count)
{
	struct cras_server_metrics_message msg;
	union cras_server_metrics_data data;
	int err;

	data.timespec_data.runtime = *ts;
	data.timespec_data.count = count;

	init_server_metrics_msg(&msg, BUSYLOOP, data);

	err = cras_server_metrics_message_send(
		(struct cras_main_message *)&msg);
	if (err < 0) {
		syslog(LOG_ERR, "Failed to send metrics message: BUSYLOOP");
		return err;
	}
	return 0;
}

static void metrics_device_runtime(struct cras_server_metrics_device_data data)
{
	char metrics_name[METRICS_NAME_BUFFER_SIZE];

	snprintf(metrics_name, METRICS_NAME_BUFFER_SIZE,
		 "Cras.%sDevice%sRuntime",
		 data.direction == CRAS_STREAM_INPUT ? "Input" : "Output",
		 metrics_device_type_str(data.type));
	cras_metrics_log_histogram(metrics_name, (unsigned)data.runtime.tv_sec,
				   0, 10000, 20);

	/* Logs the usage of each device. */
	if (data.direction == CRAS_STREAM_INPUT)
		cras_metrics_log_sparse_histogram(kDeviceTypeInput, data.type);
	else
		cras_metrics_log_sparse_histogram(kDeviceTypeOutput, data.type);
}

static void metrics_busyloop(struct cras_server_metrics_timespec_data data)
{
	char metrics_name[METRICS_NAME_BUFFER_SIZE];

	snprintf(metrics_name, METRICS_NAME_BUFFER_SIZE, "%s.%s", kBusyloop,
		 get_timespec_period_str(data.runtime));

	cras_metrics_log_histogram(metrics_name, data.count, 0, 1000, 20);
}

static void
metrics_stream_config(struct cras_server_metrics_stream_config config)
{
	/* Logs stream callback threshold. */
	cras_metrics_log_sparse_histogram(kStreamCallbackThreshold,
					  config.cb_threshold);

	/* Logs stream flags. */
	cras_metrics_log_sparse_histogram(kStreamFlags, config.flags);

	/* Logs stream sampling format. */
	cras_metrics_log_sparse_histogram(kStreamSamplingFormat, config.format);

	/* Logs stream sampling rate. */
	cras_metrics_log_sparse_histogram(kStreamSamplingRate, config.rate);

	/* Logs stream client type. */
	if (config.direction == CRAS_STREAM_INPUT)
		cras_metrics_log_sparse_histogram(kStreamClientTypeInput,
						  config.client_type);
	else
		cras_metrics_log_sparse_histogram(kStreamClientTypeOutput,
						  config.client_type);
}

static void handle_metrics_message(struct cras_main_message *msg, void *arg)
{
	struct cras_server_metrics_message *metrics_msg =
		(struct cras_server_metrics_message *)msg;
	switch (metrics_msg->metrics_type) {
	case BT_WIDEBAND_PACKET_LOSS:
		cras_metrics_log_histogram(kHfpWidebandSpeechPacketLoss,
					   metrics_msg->data.value, 0, 1000,
					   20);
		break;
	case BT_WIDEBAND_SUPPORTED:
		cras_metrics_log_sparse_histogram(kHfpWidebandSpeechSupported,
						  metrics_msg->data.value);
		break;
	case DEVICE_RUNTIME:
		metrics_device_runtime(metrics_msg->data.device_data);
		break;
	case HIGHEST_DEVICE_DELAY_INPUT:
		cras_metrics_log_histogram(kHighestDeviceDelayInput,
					   metrics_msg->data.value, 1, 10000,
					   20);
		break;
	case HIGHEST_DEVICE_DELAY_OUTPUT:
		cras_metrics_log_histogram(kHighestDeviceDelayOutput,
					   metrics_msg->data.value, 1, 10000,
					   20);
		break;
	case HIGHEST_INPUT_HW_LEVEL:
		cras_metrics_log_histogram(kHighestInputHardwareLevel,
					   metrics_msg->data.value, 1, 10000,
					   20);
		break;
	case HIGHEST_OUTPUT_HW_LEVEL:
		cras_metrics_log_histogram(kHighestOutputHardwareLevel,
					   metrics_msg->data.value, 1, 10000,
					   20);
		break;
	case LONGEST_FETCH_DELAY:
		cras_metrics_log_histogram(kStreamTimeoutMilliSeconds,
					   metrics_msg->data.value, 1, 20000,
					   10);
		break;
	case MISSED_CB_FIRST_TIME_INPUT:
		cras_metrics_log_histogram(kMissedCallbackFirstTimeInput,
					   metrics_msg->data.value, 0, 90000,
					   20);
		break;
	case MISSED_CB_FIRST_TIME_OUTPUT:
		cras_metrics_log_histogram(kMissedCallbackFirstTimeOutput,
					   metrics_msg->data.value, 0, 90000,
					   20);
		break;
	case MISSED_CB_FREQUENCY_INPUT:
		cras_metrics_log_histogram(kMissedCallbackFrequencyInput,
					   metrics_msg->data.value, 0, 90000,
					   20);
		break;
	case MISSED_CB_FREQUENCY_OUTPUT:
		cras_metrics_log_histogram(kMissedCallbackFrequencyOutput,
					   metrics_msg->data.value, 0, 90000,
					   20);
		break;
	case MISSED_CB_FREQUENCY_AFTER_RESCHEDULING_INPUT:
		cras_metrics_log_histogram(
			kMissedCallbackFrequencyAfterReschedulingInput,
			metrics_msg->data.value, 0, 90000, 20);
		break;
	case MISSED_CB_FREQUENCY_AFTER_RESCHEDULING_OUTPUT:
		cras_metrics_log_histogram(
			kMissedCallbackFrequencyAfterReschedulingOutput,
			metrics_msg->data.value, 0, 90000, 20);
		break;
	case MISSED_CB_SECOND_TIME_INPUT:
		cras_metrics_log_histogram(kMissedCallbackSecondTimeInput,
					   metrics_msg->data.value, 0, 90000,
					   20);
		break;
	case MISSED_CB_SECOND_TIME_OUTPUT:
		cras_metrics_log_histogram(kMissedCallbackSecondTimeOutput,
					   metrics_msg->data.value, 0, 90000,
					   20);
		break;
	case NUM_UNDERRUNS:
		cras_metrics_log_histogram(kUnderrunsPerDevice,
					   metrics_msg->data.value, 0, 1000,
					   10);
		break;
	case STREAM_CONFIG:
		metrics_stream_config(metrics_msg->data.stream_config);
		break;
	case BUSYLOOP:
		metrics_busyloop(metrics_msg->data.timespec_data);
		break;
	default:
		syslog(LOG_ERR, "Unknown metrics type %u",
		       metrics_msg->metrics_type);
		break;
	}
}

int cras_server_metrics_init()
{
	cras_main_message_add_handler(CRAS_MAIN_METRICS, handle_metrics_message,
				      NULL);
	return 0;
}
