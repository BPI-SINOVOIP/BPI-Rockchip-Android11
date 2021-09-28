/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cras_client.h"
#include "cras_types.h"
#include "cras_util.h"
#include "cras_version.h"

#define NOT_ASSIGNED (0)
#define PLAYBACK_BUFFERED_TIME_IN_US (5000)

#define BUF_SIZE 32768

static const size_t MAX_IODEVS = 10; /* Max devices to print out. */
static const size_t MAX_IONODES = 20; /* Max ionodes to print out. */
static const size_t MAX_ATTACHED_CLIENTS = 10; /* Max clients to print out. */

static int pipefd[2];
static struct timespec last_latency;
static int show_latency;
static float last_rms_sqr_sum;
static int last_rms_size;
static float total_rms_sqr_sum;
static int total_rms_size;
static int show_rms;
static int show_total_rms;
static int keep_looping = 1;
static int exit_after_done_playing = 1;
static size_t duration_frames;
static int pause_client = 0;
static int pause_a_reply = 0;
static int pause_in_playback_reply = 1000;

static char *channel_layout = NULL;
static int pin_device_id;

static int play_short_sound = 0;
static int play_short_sound_periods = 0;
static int play_short_sound_periods_left = 0;

static int effect_aec = 0;
static int effect_ns = 0;
static int effect_agc = 0;
static int effect_vad = 0;
static char *aecdump_file = NULL;
static char time_str[128];

/* Sleep interval between cras_client_read_atlog calls. */
static const struct timespec follow_atlog_sleep_ts = {
	0, 50 * 1000 * 1000 /* 50 ms. */
};

/* Conditional so the client thread can signal that main should exit. */
static pthread_mutex_t done_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t done_cond = PTHREAD_COND_INITIALIZER;

struct cras_audio_format *aud_format;
struct {
	char *name;
	snd_pcm_format_t format;
} supported_formats[] = {
	{ "S16_LE", SND_PCM_FORMAT_S16_LE },
	{ "S24_LE", SND_PCM_FORMAT_S24_LE },
	{ "S32_LE", SND_PCM_FORMAT_S32_LE },
	{ NULL, 0 },
};

static int terminate_stream_loop()
{
	keep_looping = 0;
	return write(pipefd[1], "1", 1);
}

static size_t get_block_size(uint64_t buffer_time_in_us, size_t rate)
{
	return (size_t)(buffer_time_in_us * rate / 1000000);
}

static void check_stream_terminate(size_t frames)
{
	if (duration_frames) {
		if (duration_frames <= frames)
			terminate_stream_loop();
		else
			duration_frames -= frames;
	}
}

static void fill_time_offset(time_t *sec_offset, int32_t *nsec_offset)
{
	struct timespec mono_time, real_time;

	clock_gettime(CLOCK_MONOTONIC_RAW, &mono_time);
	clock_gettime(CLOCK_REALTIME, &real_time);
	*sec_offset = real_time.tv_sec - mono_time.tv_sec;
	*nsec_offset = real_time.tv_nsec - mono_time.tv_nsec;
}

/* Compute square sum of samples (for calculation of RMS value). */
float compute_sqr_sum_16(const int16_t *samples, int size)
{
	unsigned i;
	float sqr_sum = 0;

	for (i = 0; i < size; i++)
		sqr_sum += samples[i] * samples[i];

	return sqr_sum;
}

/* Update the RMS values with the given samples. */
int update_rms(const uint8_t *samples, int size)
{
	switch (aud_format->format) {
	case SND_PCM_FORMAT_S16_LE: {
		last_rms_sqr_sum =
			compute_sqr_sum_16((int16_t *)samples, size / 2);
		last_rms_size = size / 2;
		break;
	}
	default:
		return -EINVAL;
	}

	total_rms_sqr_sum += last_rms_sqr_sum;
	total_rms_size += last_rms_size;

	return 0;
}

/* Parses a string with format <N>:<M> into a node id*/
static int parse_node_id(char *input, cras_node_id_t *id_out)
{
	const char *s;
	char *endptr;
	int dev_index;
	int node_index;

	if (!id_out)
		return -EINVAL;

	s = strtok(input, ":");
	if (!s)
		return -EINVAL;
	dev_index = strtol(s, &endptr, 10);
	if (*endptr)
		return -EINVAL;

	s = strtok(NULL, ":");
	if (!s)
		return -EINVAL;
	node_index = strtol(s, &endptr, 10);
	if (*endptr)
		return -EINVAL;

	*id_out = cras_make_node_id(dev_index, node_index);
	return 0;
}

/* Parses a string with format <N>:<M>:<0-100> into a node id and a value */
static int parse_node_id_with_value(char *input, cras_node_id_t *id_out,
				    int *value_out)
{
	const char *s;
	char *endptr;
	int dev_index;
	int node_index;
	long int value;

	if (!id_out || !value_out)
		return -EINVAL;

	s = strtok(input, ":");
	if (!s)
		return -EINVAL;
	dev_index = strtol(s, &endptr, 10);
	if (*endptr)
		return -EINVAL;

	s = strtok(NULL, ":");
	if (!s)
		return -EINVAL;
	node_index = strtol(s, &endptr, 10);
	if (*endptr)
		return -EINVAL;

	s = strtok(NULL, ":");
	if (!s)
		return -EINVAL;
	value = strtol(s, &endptr, 10);
	if (*endptr)
		return -EINVAL;
	if (value > INT_MAX || value < INT_MIN)
		return -EOVERFLOW;

	*id_out = cras_make_node_id(dev_index, node_index);
	*value_out = value;
	return 0;
}

/* Run from callback thread. */
static int got_samples(struct cras_client *client, cras_stream_id_t stream_id,
		       uint8_t *captured_samples, uint8_t *playback_samples,
		       unsigned int frames,
		       const struct timespec *captured_time,
		       const struct timespec *playback_time, void *user_arg)
{
	int *fd = (int *)user_arg;
	int ret;
	int write_size;
	int frame_bytes;

	while (pause_client)
		usleep(10000);

	cras_client_calc_capture_latency(captured_time, &last_latency);

	frame_bytes = cras_client_format_bytes_per_frame(aud_format);
	write_size = frames * frame_bytes;

	/* Update RMS values with all available frames. */
	if (keep_looping) {
		update_rms(captured_samples,
			   MIN(write_size, duration_frames * frame_bytes));
	}

	check_stream_terminate(frames);

	ret = write(*fd, captured_samples, write_size);
	if (ret != write_size)
		printf("Error writing file\n");
	return frames;
}

/* Run from callback thread. */
static int put_samples(struct cras_client *client, cras_stream_id_t stream_id,
		       uint8_t *captured_samples, uint8_t *playback_samples,
		       unsigned int frames,
		       const struct timespec *captured_time,
		       const struct timespec *playback_time, void *user_arg)
{
	uint32_t frame_bytes = cras_client_format_bytes_per_frame(aud_format);
	int fd = *(int *)user_arg;
	uint8_t buff[BUF_SIZE];
	int nread;

	while (pause_client)
		usleep(10000);

	if (pause_a_reply) {
		usleep(pause_in_playback_reply);
		pause_a_reply = 0;
	}

	check_stream_terminate(frames);

	cras_client_calc_playback_latency(playback_time, &last_latency);

	if (play_short_sound) {
		if (play_short_sound_periods_left)
			/* Play a period from file. */
			play_short_sound_periods_left--;
		else {
			/* Fill zeros to play silence. */
			memset(playback_samples, 0,
			       MIN(frames * frame_bytes, BUF_SIZE));
			return frames;
		}
	}

	nread = read(fd, buff, MIN(frames * frame_bytes, BUF_SIZE));
	if (nread <= 0) {
		if (exit_after_done_playing)
			terminate_stream_loop();
		return nread;
	}

	memcpy(playback_samples, buff, nread);
	return nread / frame_bytes;
}

/* Run from callback thread. */
static int
put_stdin_samples(struct cras_client *client, cras_stream_id_t stream_id,
		  uint8_t *captured_samples, uint8_t *playback_samples,
		  unsigned int frames, const struct timespec *captured_time,
		  const struct timespec *playback_time, void *user_arg)
{
	int rc = 0;
	uint32_t frame_bytes = cras_client_format_bytes_per_frame(aud_format);

	rc = read(0, playback_samples, frames * frame_bytes);
	if (rc <= 0) {
		terminate_stream_loop();
		return -1;
	}

	return rc / frame_bytes;
}

static int stream_error(struct cras_client *client, cras_stream_id_t stream_id,
			int err, void *arg)
{
	printf("Stream error %d\n", err);
	terminate_stream_loop();
	return 0;
}

static void print_last_latency()
{
	if (last_latency.tv_sec > 0 || last_latency.tv_nsec > 0)
		printf("%u.%09u\n", (unsigned)last_latency.tv_sec,
		       (unsigned)last_latency.tv_nsec);
	else {
		printf("-%lld.%09lld\n", (long long)-last_latency.tv_sec,
		       (long long)-last_latency.tv_nsec);
	}
}

static void print_last_rms()
{
	if (last_rms_size != 0)
		printf("%.9f\n", sqrt(last_rms_sqr_sum / last_rms_size));
}

static void print_total_rms()
{
	if (total_rms_size != 0)
		printf("%.9f\n", sqrt(total_rms_sqr_sum / total_rms_size));
}

static void print_dev_info(const struct cras_iodev_info *devs, int num_devs)
{
	unsigned i;

	printf("\tID\tName\n");
	for (i = 0; i < num_devs; i++)
		printf("\t%u\t%s\n", devs[i].idx, devs[i].name);
}

static void print_node_info(const struct cras_ionode_info *nodes, int num_nodes,
			    int is_input)
{
	unsigned i;

	printf("\tStable Id\t ID\t%4s   Plugged\tL/R swapped\t      "
	       "Time Hotword\tType\t\t Name\n",
	       is_input ? "Gain" : " Vol");
	for (i = 0; i < num_nodes; i++)
		printf("\t(%08x)\t%u:%u\t%5g  %7s\t%14s\t%10ld %-7s\t%-16s%c%s\n",
		       nodes[i].stable_id, nodes[i].iodev_idx,
		       nodes[i].ionode_idx,
		       is_input ? nodes[i].capture_gain / 100.0 :
				  (double)nodes[i].volume,
		       nodes[i].plugged ? "yes" : "no",
		       nodes[i].left_right_swapped ? "yes" : "no",
		       (long)nodes[i].plugged_time.tv_sec,
		       nodes[i].active_hotword_model, nodes[i].type,
		       nodes[i].active ? '*' : ' ', nodes[i].name);
}

static void print_device_lists(struct cras_client *client)
{
	struct cras_iodev_info devs[MAX_IODEVS];
	struct cras_ionode_info nodes[MAX_IONODES];
	size_t num_devs, num_nodes;
	int rc;

	num_devs = MAX_IODEVS;
	num_nodes = MAX_IONODES;
	rc = cras_client_get_output_devices(client, devs, nodes, &num_devs,
					    &num_nodes);
	if (rc < 0)
		return;
	printf("Output Devices:\n");
	print_dev_info(devs, num_devs);
	printf("Output Nodes:\n");
	print_node_info(nodes, num_nodes, 0);

	num_devs = MAX_IODEVS;
	num_nodes = MAX_IONODES;
	rc = cras_client_get_input_devices(client, devs, nodes, &num_devs,
					   &num_nodes);
	printf("Input Devices:\n");
	print_dev_info(devs, num_devs);
	printf("Input Nodes:\n");
	print_node_info(nodes, num_nodes, 1);
}

static void print_attached_client_list(struct cras_client *client)
{
	struct cras_attached_client_info clients[MAX_ATTACHED_CLIENTS];
	size_t i;
	int num_clients;

	num_clients = cras_client_get_attached_clients(client, clients,
						       MAX_ATTACHED_CLIENTS);
	if (num_clients < 0)
		return;
	num_clients = MIN(num_clients, MAX_ATTACHED_CLIENTS);
	printf("Attached clients:\n");
	printf("\tID\tpid\tuid\n");
	for (i = 0; i < num_clients; i++)
		printf("\t%u\t%d\t%d\n", clients[i].id, clients[i].pid,
		       clients[i].gid);
}

static void print_active_stream_info(struct cras_client *client)
{
	struct timespec ts;
	unsigned num_streams;

	num_streams = cras_client_get_num_active_streams(client, &ts);
	printf("Num active streams: %u\n", num_streams);
	printf("Last audio active time: %llu, %llu\n", (long long)ts.tv_sec,
	       (long long)ts.tv_nsec);
}

static void print_system_volumes(struct cras_client *client)
{
	printf("System Volume (0-100): %zu %s\n"
	       "Capture Gain (%.2f - %.2f): %.2fdB %s\n",
	       cras_client_get_system_volume(client),
	       cras_client_get_system_muted(client) ? "(Muted)" : "",
	       cras_client_get_system_min_capture_gain(client) / 100.0,
	       cras_client_get_system_max_capture_gain(client) / 100.0,
	       cras_client_get_system_capture_gain(client) / 100.0,
	       cras_client_get_system_capture_muted(client) ? "(Muted)" : "");
}

static void print_user_muted(struct cras_client *client)
{
	printf("User muted: %s\n",
	       cras_client_get_user_muted(client) ? "Muted" : "Not muted");
}

/*
 * Convert time value from one clock to the other using given offset
 * in sec and nsec.
 */
static void convert_time(unsigned int *sec, unsigned int *nsec,
			 time_t sec_offset, int32_t nsec_offset)
{
	sec_offset += *sec;
	nsec_offset += *nsec;
	if (nsec_offset >= 1000000000L) {
		sec_offset++;
		nsec_offset -= 1000000000L;
	} else if (nsec_offset < 0) {
		sec_offset--;
		nsec_offset += 1000000000L;
	}
	*sec = sec_offset;
	*nsec = nsec_offset;
}

static void show_alog_tag(const struct audio_thread_event_log *log,
			  unsigned int tag_idx, int32_t sec_offset,
			  int32_t nsec_offset)
{
	unsigned int tag = (log->log[tag_idx].tag_sec >> 24) & 0xff;
	unsigned int sec = log->log[tag_idx].tag_sec & 0x00ffffff;
	unsigned int nsec = log->log[tag_idx].nsec;
	unsigned int data1 = log->log[tag_idx].data1;
	unsigned int data2 = log->log[tag_idx].data2;
	unsigned int data3 = log->log[tag_idx].data3;
	time_t lt;
	struct tm *t;

	/* Skip unused log entries. */
	if (log->log[tag_idx].tag_sec == 0 && log->log[tag_idx].nsec == 0)
		return;

	/* Convert from monotomic raw clock to realtime clock. */
	convert_time(&sec, &nsec, sec_offset, nsec_offset);
	lt = sec;
	t = localtime(&lt);
	strftime(time_str, 128, "%Y-%m-%dT%H:%M:%S", t);

	printf("%s.%09u cras atlog  ", time_str, nsec);

	/* Prepare realtime string for arguments. */
	switch (tag) {
	case AUDIO_THREAD_READ_AUDIO_TSTAMP:
	case AUDIO_THREAD_FILL_AUDIO_TSTAMP:
	case AUDIO_THREAD_STREAM_RESCHEDULE:
	case AUDIO_THREAD_STREAM_SLEEP_TIME:
	case AUDIO_THREAD_STREAM_SLEEP_ADJUST:
	case AUDIO_THREAD_DEV_SLEEP_TIME:
		sec = data2;
		nsec = data3;
		break;
	}
	convert_time(&sec, &nsec, sec_offset, nsec_offset);
	lt = sec;
	t = localtime(&lt);
	strftime(time_str, 128, " %H:%M:%S", t);

	switch (tag) {
	case AUDIO_THREAD_WAKE:
		printf("%-30s num_fds:%d\n", "WAKE", (int)data1);
		break;
	case AUDIO_THREAD_SLEEP:
		printf("%-30s sleep:%09d.%09d\n", "SLEEP", (int)data1,
		       (int)data2);
		break;
	case AUDIO_THREAD_READ_AUDIO:
		printf("%-30s dev:%u hw_level:%u read:%u\n", "READ_AUDIO",
		       data1, data2, data3);
		break;
	case AUDIO_THREAD_READ_AUDIO_TSTAMP:
		printf("%-30s dev:%u tstamp:%s.%09u\n", "READ_AUDIO_TSTAMP",
		       data1, time_str, nsec);
		break;
	case AUDIO_THREAD_READ_AUDIO_DONE:
		printf("%-30s read_remainder:%u\n", "READ_AUDIO_DONE", data1);
		break;
	case AUDIO_THREAD_READ_OVERRUN:
		printf("%-30s dev:%u stream:%x num_overruns:%u\n",
		       "READ_AUDIO_OVERRUN", data1, data2, data3);
		break;
	case AUDIO_THREAD_FILL_AUDIO:
		printf("%-30s dev:%u hw_level:%u\n", "FILL_AUDIO", data1,
		       data2);
		break;
	case AUDIO_THREAD_FILL_AUDIO_TSTAMP:
		printf("%-30s dev:%u tstamp:%s.%09u\n", "FILL_AUDIO_TSTAMP",
		       data1, time_str, nsec);
		break;
	case AUDIO_THREAD_FILL_AUDIO_DONE:
		printf("%-30s hw_level:%u total_written:%u min_cb_level:%u\n",
		       "FILL_AUDIO_DONE", data1, data2, data3);
		break;
	case AUDIO_THREAD_WRITE_STREAMS_WAIT:
		printf("%-30s stream:%x\n", "WRITE_STREAMS_WAIT", data1);
		break;
	case AUDIO_THREAD_WRITE_STREAMS_WAIT_TO:
		printf("%-30s\n", "WRITE_STREAMS_WAIT_TO");
		break;
	case AUDIO_THREAD_WRITE_STREAMS_MIX:
		printf("%-30s write_limit:%u max_offset:%u\n",
		       "WRITE_STREAMS_MIX", data1, data2);
		break;
	case AUDIO_THREAD_WRITE_STREAMS_MIXED:
		printf("%-30s write_limit:%u\n", "WRITE_STREAMS_MIXED", data1);
		break;
	case AUDIO_THREAD_WRITE_STREAMS_STREAM:
		printf("%-30s id:%x shm_frames:%u cb_pending:%u\n",
		       "WRITE_STREAMS_STREAM", data1, data2, data3);
		break;
	case AUDIO_THREAD_FETCH_STREAM:
		printf("%-30s id:%x cbth:%u delay:%u\n",
		       "WRITE_STREAMS_FETCH_STREAM", data1, data2, data3);
		break;
	case AUDIO_THREAD_STREAM_ADDED:
		printf("%-30s id:%x dev:%u\n", "STREAM_ADDED", data1, data2);
		break;
	case AUDIO_THREAD_STREAM_REMOVED:
		printf("%-30s id:%x\n", "STREAM_REMOVED", data1);
		break;
	case AUDIO_THREAD_A2DP_ENCODE:
		printf("%-30s proc:%d queued:%u readable:%u\n", "A2DP_ENCODE",
		       data1, data2, data3);
		break;
	case AUDIO_THREAD_A2DP_WRITE:
		printf("%-30s written:%d queued:%u\n", "A2DP_WRITE", data1,
		       data2);
		break;
	case AUDIO_THREAD_DEV_STREAM_MIX:
		printf("%-30s written:%u read:%u\n", "DEV_STREAM_MIX", data1,
		       data2);
		break;
	case AUDIO_THREAD_CAPTURE_POST:
		printf("%-30s stream:%x thresh:%u rd_buf:%u\n", "CAPTURE_POST",
		       data1, data2, data3);
		break;
	case AUDIO_THREAD_CAPTURE_WRITE:
		printf("%-30s stream:%x write:%u shm_fr:%u\n", "CAPTURE_WRITE",
		       data1, data2, data3);
		break;
	case AUDIO_THREAD_CONV_COPY:
		printf("%-30s wr_buf:%u shm_writable:%u offset:%u\n",
		       "CONV_COPY", data1, data2, data3);
		break;
	case AUDIO_THREAD_STREAM_FETCH_PENDING:
		printf("%-30s id:%x\n", "STREAM_FETCH_PENGING", data1);
		break;
	case AUDIO_THREAD_STREAM_RESCHEDULE:
		printf("%-30s id:%x next_cb_ts:%s.%09u\n", "STREAM_RESCHEDULE",
		       data1, time_str, nsec);
		break;
	case AUDIO_THREAD_STREAM_SLEEP_TIME:
		printf("%-30s id:%x wake:%s.%09u\n", "STREAM_SLEEP_TIME", data1,
		       time_str, nsec);
		break;
	case AUDIO_THREAD_STREAM_SLEEP_ADJUST:
		printf("%-30s id:%x from:%s.%09u\n", "STREAM_SLEEP_ADJUST",
		       data1, time_str, nsec);
		break;
	case AUDIO_THREAD_STREAM_SKIP_CB:
		printf("%-30s id:%x write_offset_0:%u write_offset_1:%u\n",
		       "STREAM_SKIP_CB", data1, data2, data3);
		break;
	case AUDIO_THREAD_DEV_SLEEP_TIME:
		printf("%-30s dev:%u wake:%s.%09u\n", "DEV_SLEEP_TIME", data1,
		       time_str, nsec);
		break;
	case AUDIO_THREAD_SET_DEV_WAKE:
		printf("%-30s dev:%u hw_level:%u sleep:%u\n", "SET_DEV_WAKE",
		       data1, data2, data3);
		break;
	case AUDIO_THREAD_DEV_ADDED:
		printf("%-30s dev:%u\n", "DEV_ADDED", data1);
		break;
	case AUDIO_THREAD_DEV_REMOVED:
		printf("%-30s dev:%u\n", "DEV_REMOVED", data1);
		break;
	case AUDIO_THREAD_IODEV_CB:
		printf("%-30s is_write:%u\n", "IODEV_CB", data1);
		break;
	case AUDIO_THREAD_PB_MSG:
		printf("%-30s msg_id:%u\n", "PB_MSG", data1);
		break;
	case AUDIO_THREAD_ODEV_NO_STREAMS:
		printf("%-30s dev:%u\n", "ODEV_NO_STREAMS", data1);
		break;
	case AUDIO_THREAD_ODEV_LEAVE_NO_STREAMS:
		printf("%-30s dev:%u\n", "ODEV_LEAVE_NO_STREAMS", data1);
		break;
	case AUDIO_THREAD_ODEV_START:
		printf("%-30s dev:%u min_cb_level:%u\n", "ODEV_START", data1,
		       data2);
		break;
	case AUDIO_THREAD_FILL_ODEV_ZEROS:
		printf("%-30s dev:%u write:%u\n", "FILL_ODEV_ZEROS", data1,
		       data2);
		break;
	case AUDIO_THREAD_ODEV_DEFAULT_NO_STREAMS:
		printf("%-30s dev:%u hw_level:%u target:%u\n",
		       "DEFAULT_NO_STREAMS", data1, data2, data3);
		break;
	case AUDIO_THREAD_UNDERRUN:
		printf("%-30s dev:%u hw_level:%u total_written:%u\n",
		       "UNDERRUN", data1, data2, data3);
		break;
	case AUDIO_THREAD_SEVERE_UNDERRUN:
		printf("%-30s dev:%u\n", "SEVERE_UNDERRUN", data1);
		break;
	case AUDIO_THREAD_CAPTURE_DROP_TIME:
		printf("%-30s time:%09u.%09d\n", "CAPTURE_DROP_TIME", data1,
		       data2);
		break;
	case AUDIO_THREAD_DEV_DROP_FRAMES:
		printf("%-30s dev:%u frames:%u\n", "DEV_DROP_FRAMES", data1,
		       data2);
		break;
	default:
		printf("%-30s tag:%u\n", "UNKNOWN", tag);
		break;
	}
}

static void print_audio_debug_info(const struct audio_debug_info *info)
{
	time_t sec_offset;
	int32_t nsec_offset;
	int i, j;

	printf("Audio Debug Stats:\n");
	printf("-------------devices------------\n");
	if (info->num_devs > MAX_DEBUG_DEVS)
		return;

	for (i = 0; i < info->num_devs; i++) {
		printf("%s dev: %s\n",
		       (info->devs[i].direction == CRAS_STREAM_INPUT) ?
			       "Input" :
			       "Output",
		       info->devs[i].dev_name);
		printf("buffer_size: %u\n"
		       "min_buffer_level: %u\n"
		       "min_cb_level: %u\n"
		       "max_cb_level: %u\n"
		       "frame_rate: %u\n"
		       "num_channels: %u\n"
		       "est_rate_ratio: %lf\n"
		       "num_underruns: %u\n"
		       "num_severe_underruns: %u\n"
		       "highest_hw_level: %u\n"
		       "runtime: %u.%09u\n"
		       "longest_wake: %u.%09u\n"
		       "software_gain_scaler: %lf\n",
		       (unsigned int)info->devs[i].buffer_size,
		       (unsigned int)info->devs[i].min_buffer_level,
		       (unsigned int)info->devs[i].min_cb_level,
		       (unsigned int)info->devs[i].max_cb_level,
		       (unsigned int)info->devs[i].frame_rate,
		       (unsigned int)info->devs[i].num_channels,
		       info->devs[i].est_rate_ratio,
		       (unsigned int)info->devs[i].num_underruns,
		       (unsigned int)info->devs[i].num_severe_underruns,
		       (unsigned int)info->devs[i].highest_hw_level,
		       (unsigned int)info->devs[i].runtime_sec,
		       (unsigned int)info->devs[i].runtime_nsec,
		       (unsigned int)info->devs[i].longest_wake_sec,
		       (unsigned int)info->devs[i].longest_wake_nsec,
		       info->devs[i].software_gain_scaler);
		printf("\n");
	}

	printf("-------------stream_dump------------\n");
	if (info->num_streams > MAX_DEBUG_STREAMS)
		return;

	for (i = 0; i < info->num_streams; i++) {
		int channel;
		printf("stream: %llu dev: %u\n",
		       (unsigned long long)info->streams[i].stream_id,
		       (unsigned int)info->streams[i].dev_idx);
		printf("direction: %s\n",
		       (info->streams[i].direction == CRAS_STREAM_INPUT) ?
			       "Input" :
			       "Output");
		printf("stream_type: %s\n",
		       cras_stream_type_str(info->streams[i].stream_type));
		printf("client_type: %s\n",
		       cras_client_type_str(info->streams[i].client_type));
		printf("buffer_frames: %u\n"
		       "cb_threshold: %u\n"
		       "effects: 0x%.4x\n"
		       "frame_rate: %u\n"
		       "num_channels: %u\n"
		       "longest_fetch_sec: %u.%09u\n"
		       "num_overruns: %u\n"
		       "is_pinned: %x\n"
		       "pinned_dev_idx: %x\n"
		       "num_missed_cb: %u\n"
		       "%s: %lf\n"
		       "runtime: %u.%09u\n",
		       (unsigned int)info->streams[i].buffer_frames,
		       (unsigned int)info->streams[i].cb_threshold,
		       (unsigned int)info->streams[i].effects,
		       (unsigned int)info->streams[i].frame_rate,
		       (unsigned int)info->streams[i].num_channels,
		       (unsigned int)info->streams[i].longest_fetch_sec,
		       (unsigned int)info->streams[i].longest_fetch_nsec,
		       (unsigned int)info->streams[i].num_overruns,
		       (unsigned int)info->streams[i].is_pinned,
		       (unsigned int)info->streams[i].pinned_dev_idx,
		       (unsigned int)info->streams[i].num_missed_cb,
		       (info->streams[i].direction == CRAS_STREAM_INPUT) ?
			       "gain" :
			       "volume",
		       info->streams[i].stream_volume,
		       (unsigned int)info->streams[i].runtime_sec,
		       (unsigned int)info->streams[i].runtime_nsec);
		printf("channel map:");
		for (channel = 0; channel < CRAS_CH_MAX; channel++)
			printf("%d ", info->streams[i].channel_layout[channel]);
		printf("\n\n");
	}

	printf("Audio Thread Event Log:\n");

	fill_time_offset(&sec_offset, &nsec_offset);
	j = info->log.write_pos % info->log.len;
	i = 0;
	printf("start at %d\n", j);
	for (; i < info->log.len; i++) {
		show_alog_tag(&info->log, j, sec_offset, nsec_offset);
		j++;
		j %= info->log.len;
	}
}

static void audio_debug_info(struct cras_client *client)
{
	const struct audio_debug_info *info;
	info = cras_client_get_audio_debug_info(client);
	if (!info)
		return;
	print_audio_debug_info(info);

	/* Signal main thread we are done after the last chunk. */
	pthread_mutex_lock(&done_mutex);
	pthread_cond_signal(&done_cond);
	pthread_mutex_unlock(&done_mutex);
}

static void show_btlog_tag(const struct cras_bt_event_log *log,
			   unsigned int tag_idx, int32_t sec_offset,
			   int32_t nsec_offset)
{
	unsigned int tag = (log->log[tag_idx].tag_sec >> 24) & 0xff;
	unsigned int sec = log->log[tag_idx].tag_sec & 0x00ffffff;
	unsigned int nsec = log->log[tag_idx].nsec;
	unsigned int data1 = log->log[tag_idx].data1;
	unsigned int data2 = log->log[tag_idx].data2;
	time_t lt;
	struct tm *t;

	/* Skip unused log entries. */
	if (log->log[tag_idx].tag_sec == 0 && log->log[tag_idx].nsec == 0)
		return;

	/* Convert from monotomic raw clock to realtime clock. */
	convert_time(&sec, &nsec, sec_offset, nsec_offset);
	lt = sec;
	t = localtime(&lt);
	strftime(time_str, 128, "%Y-%m-%dT%H:%M:%S", t);

	printf("%s.%09u cras btlog  ", time_str, nsec);

	switch (tag) {
	case BT_ADAPTER_ADDED:
		printf("%-30s\n", "ADAPTER_ADDED");
		break;
	case BT_ADAPTER_REMOVED:
		printf("%-30s\n", "ADAPTER_REMOVED");
		break;
	case BT_A2DP_CONFIGURED:
		printf("%-30s connected profiles %u\n", "A2DP_CONFIGURED",
		       data1);
		break;
	case BT_A2DP_START:
		printf("%-30s\n", "A2DP_START");
		break;
	case BT_A2DP_SUSPENDED:
		printf("%-30s\n", "A2DP_SUSPENDED");
		break;
	case BT_AUDIO_GATEWAY_INIT:
		printf("%-30s supported profiles %u\n", "AUDIO_GATEWAY_INIT",
		       data1);
		break;
	case BT_AUDIO_GATEWAY_START:
		printf("%-30s \n", "AUDIO_GATEWAY_START");
		break;
	case BT_AVAILABLE_CODECS:
		printf("%-30s codec #%u id %u\n", "AVAILABLE_CODECS", data1,
		       data2);
		break;
	case BT_CODEC_SELECTION:
		printf("%-30s dir %u codec id %u\n", "CODEC_SELECTION", data1,
		       data2);
		break;
	case BT_DEV_CONNECTED_CHANGE:
		printf("%-30s profiles %u now %u\n", "DEV_CONENCTED_CHANGE",
		       data1, data2);
		break;
	case BT_DEV_CONN_WATCH_CB:
		printf("%-30s %u retries left, supported profiles %u\n",
		       "DEV_CONN_WATCH_CB", data1, data2);
		break;
	case BT_DEV_SUSPEND_CB:
		printf("%-30s profiles supported %u, connected %u\n",
		       "DEV_SUSPEND_CB", data1, data2);
		break;
	case BT_HFP_NEW_CONNECTION:
		printf("%-30s\n", "HFP_NEW_CONNECTION");
		break;
	case BT_HFP_REQUEST_DISCONNECT:
		printf("%-30s\n", "HFP_REQUEST_DISCONNECT");
		break;
	case BT_HFP_SUPPORTED_FEATURES:
		printf("%-30s role %s features %u\n", "HFP_SUPPORTED_FEATURES",
		       data1 ? "AG" : "HF", data2);
		break;
	case BT_HSP_NEW_CONNECTION:
		printf("%-30s\n", "HSP_NEW_CONNECTION");
		break;
	case BT_HSP_REQUEST_DISCONNECT:
		printf("%-30s\n", "HSP_REQUEST_DISCONNECT");
		break;
	case BT_NEW_AUDIO_PROFILE_AFTER_CONNECT:
		printf("%-30s old %u, new %u\n",
		       "NEW_AUDIO_PROFILE_AFTER_CONNECT", data1, data2);
		break;
	case BT_RESET:
		printf("%-30s\n", "RESET");
		break;
	case BT_SCO_CONNECT:
		printf("%-30s %s sk %d\n", "SCO_CONNECT",
		       data1 ? "success" : "failed", (int)data2);
		break;
	case BT_TRANSPORT_ACQUIRE:
		printf("%-30s %s fd %d\n", "TRANSPORT_ACQUIRE",
		       data1 ? "success" : "failed", (int)data2);
		break;
	case BT_TRANSPORT_RELEASE:
		printf("%-30s\n", "TRANSPORT_RELEASE");
		break;
	default:
		printf("%-30s\n", "UNKNOWN");
		break;
	}
}

static void cras_bt_debug_info(struct cras_client *client)
{
	const struct cras_bt_debug_info *info;
	time_t sec_offset;
	int32_t nsec_offset;
	int i, j;

	info = cras_client_get_bt_debug_info(client);
	fill_time_offset(&sec_offset, &nsec_offset);
	j = info->bt_log.write_pos;
	i = 0;
	printf("BT debug log:\n");
	for (; i < info->bt_log.len; i++) {
		show_btlog_tag(&info->bt_log, j, sec_offset, nsec_offset);
		j++;
		j %= info->bt_log.len;
	}

	/* Signal main thread we are done after the last chunk. */
	pthread_mutex_lock(&done_mutex);
	pthread_cond_signal(&done_cond);
	pthread_mutex_unlock(&done_mutex);
}

static void print_cras_audio_thread_snapshot(
	const struct cras_audio_thread_snapshot *snapshot)
{
	printf("-------------snapshot------------\n");
	printf("Event time: %" PRId64 ".%ld\n",
	       (int64_t)snapshot->timestamp.tv_sec,
	       snapshot->timestamp.tv_nsec);

	printf("Event type: ");
	switch (snapshot->event_type) {
	case AUDIO_THREAD_EVENT_BUSYLOOP:
		printf("busyloop\n");
		break;
	case AUDIO_THREAD_EVENT_UNDERRUN:
		printf("underrun\n");
		break;
	case AUDIO_THREAD_EVENT_SEVERE_UNDERRUN:
		printf("severe underrun\n");
		break;
	case AUDIO_THREAD_EVENT_DROP_SAMPLES:
		printf("drop samples\n");
		break;
	case AUDIO_THREAD_EVENT_DEBUG:
		printf("debug\n");
		break;
	default:
		printf("no such type\n");
	}
	print_audio_debug_info(&snapshot->audio_debug_info);
}

static void audio_thread_snapshots(struct cras_client *client)
{
	const struct cras_audio_thread_snapshot_buffer *snapshot_buffer;
	uint32_t i;
	int j;
	int count = 0;

	snapshot_buffer = cras_client_get_audio_thread_snapshot_buffer(client);
	i = snapshot_buffer->pos;
	for (j = 0; j < CRAS_MAX_AUDIO_THREAD_SNAPSHOTS; j++) {
		if (snapshot_buffer->snapshots[i].timestamp.tv_sec ||
		    snapshot_buffer->snapshots[i].timestamp.tv_nsec) {
			print_cras_audio_thread_snapshot(
				&snapshot_buffer->snapshots[i]);
			count++;
		}
		i++;
		i %= CRAS_MAX_AUDIO_THREAD_SNAPSHOTS;
	}
	printf("There are %d, snapshots.\n", count);

	/* Signal main thread we are done after the last chunk. */
	pthread_mutex_lock(&done_mutex);
	pthread_cond_signal(&done_cond);
	pthread_mutex_unlock(&done_mutex);
}

static int start_stream(struct cras_client *client, cras_stream_id_t *stream_id,
			struct cras_stream_params *params, float stream_volume)
{
	int rc;

	if (pin_device_id)
		rc = cras_client_add_pinned_stream(client, pin_device_id,
						   stream_id, params);
	else
		rc = cras_client_add_stream(client, stream_id, params);
	if (rc < 0) {
		fprintf(stderr, "adding a stream %d\n", rc);
		return rc;
	}
	return cras_client_set_stream_volume(client, *stream_id, stream_volume);
}

static int parse_channel_layout(char *channel_layout_str,
				int8_t channel_layout[CRAS_CH_MAX])
{
	int i = 0;
	char *chp;

	chp = strtok(channel_layout_str, ",");
	while (chp && i < CRAS_CH_MAX) {
		channel_layout[i++] = atoi(chp);
		chp = strtok(NULL, ",");
	}

	return 0;
}

static void run_aecdump(struct cras_client *client, uint64_t stream_id,
			int start)
{
	int aecdump_fd;
	if (start) {
		aecdump_fd =
			open(aecdump_file, O_CREAT | O_RDWR | O_TRUNC, 0666);
		if (aecdump_fd == -1) {
			printf("Fail to open file %s", aecdump_file);
			return;
		}

		printf("Dumping AEC info to %s, stream %" PRId64 ", fd %d\n",
		       aecdump_file, stream_id, aecdump_fd);
		cras_client_set_aec_dump(client, stream_id, 1, aecdump_fd);
	} else {
		cras_client_set_aec_dump(client, stream_id, 0, -1);
		printf("Close AEC dump file %s\n", aecdump_file);
	}
}

static int run_file_io_stream(struct cras_client *client, int fd,
			      enum CRAS_STREAM_DIRECTION direction,
			      size_t block_size,
			      enum CRAS_STREAM_TYPE stream_type, size_t rate,
			      snd_pcm_format_t format, size_t num_channels,
			      uint32_t flags, int is_loopback, int is_post_dsp)
{
	int rc, tty;
	struct cras_stream_params *params;
	cras_unified_cb_t aud_cb;
	cras_stream_id_t stream_id = 0;
	int stream_playing = 0;
	int *pfd = malloc(sizeof(*pfd));
	*pfd = fd;
	fd_set poll_set;
	struct timespec sleep_ts;
	float volume_scaler = 1.0;
	size_t sys_volume = 100;
	long cap_gain = 0;
	int mute = 0;
	int8_t layout[CRAS_CH_MAX];

	/* Set the sleep interval between latency/RMS prints. */
	sleep_ts.tv_sec = 1;
	sleep_ts.tv_nsec = 0;

	/* Open the pipe file descriptor. */
	rc = pipe(pipefd);
	if (rc == -1) {
		perror("failed to open pipe");
		return -errno;
	}

	/* Reset the total RMS value. */
	total_rms_sqr_sum = 0;
	total_rms_size = 0;

	if (direction == CRAS_STREAM_INPUT)
		aud_cb = got_samples;
	else
		aud_cb = put_samples;

	if (fd == 0) {
		if (direction != CRAS_STREAM_OUTPUT)
			return -EINVAL;
		aud_cb = put_stdin_samples;
	}

	aud_format = cras_audio_format_create(format, rate, num_channels);
	if (aud_format == NULL)
		return -ENOMEM;

	if (channel_layout) {
		/* Set channel layout to format */
		parse_channel_layout(channel_layout, layout);
		cras_audio_format_set_channel_layout(aud_format, layout);
	}

	params = cras_client_unified_params_create(direction, block_size,
						   stream_type, flags, pfd,
						   aud_cb, stream_error,
						   aud_format);
	if (params == NULL)
		return -ENOMEM;

	cras_client_stream_params_set_client_type(params,
						  CRAS_CLIENT_TYPE_TEST);

	if (effect_aec)
		cras_client_stream_params_enable_aec(params);
	if (effect_ns)
		cras_client_stream_params_enable_ns(params);
	if (effect_agc)
		cras_client_stream_params_enable_agc(params);
	if (effect_vad)
		cras_client_stream_params_enable_vad(params);

	cras_client_run_thread(client);
	if (is_loopback) {
		enum CRAS_NODE_TYPE type =
			(is_post_dsp ? CRAS_NODE_TYPE_POST_DSP :
				       CRAS_NODE_TYPE_POST_MIX_PRE_DSP);

		cras_client_connected_wait(client);
		pin_device_id = cras_client_get_first_dev_type_idx(
			client, type, CRAS_STREAM_INPUT);
	}

	stream_playing =
		start_stream(client, &stream_id, params, volume_scaler) == 0;

	tty = open("/dev/tty", O_RDONLY);

	// There could be no terminal available when run in autotest.
	if (tty == -1)
		perror("warning: failed to open /dev/tty");

	while (keep_looping) {
		char input;
		int nread;

		FD_ZERO(&poll_set);
		if (tty >= 0)
			FD_SET(tty, &poll_set);
		FD_SET(pipefd[0], &poll_set);
		pselect(MAX(tty, pipefd[0]) + 1, &poll_set, NULL, NULL,
			show_latency || show_rms ? &sleep_ts : NULL, NULL);

		if (stream_playing && show_latency)
			print_last_latency();

		if (stream_playing && show_rms)
			print_last_rms();

		if (tty < 0 || !FD_ISSET(tty, &poll_set))
			continue;

		nread = read(tty, &input, 1);
		if (nread < 1) {
			fprintf(stderr, "Error reading stdin\n");
			return nread;
		}
		switch (input) {
		case 'p':
			pause_client = !pause_client;
			break;
		case 'i':
			pause_a_reply = 1;
			break;
		case 'q':
			terminate_stream_loop();
			break;
		case 's':
			if (stream_playing)
				break;

			/* If started by hand keep running after it finishes. */
			exit_after_done_playing = 0;

			stream_playing =
				start_stream(client, &stream_id, params,
					     volume_scaler) == 0;
			break;
		case 'r':
			if (!stream_playing)
				break;
			cras_client_rm_stream(client, stream_id);
			stream_playing = 0;
			break;
		case 'u':
			volume_scaler = MIN(volume_scaler + 0.1, 1.0);
			cras_client_set_stream_volume(client, stream_id,
						      volume_scaler);
			break;
		case 'd':
			volume_scaler = MAX(volume_scaler - 0.1, 0.0);
			cras_client_set_stream_volume(client, stream_id,
						      volume_scaler);
			break;
		case 'k':
			sys_volume = MIN(sys_volume + 1, 100);
			cras_client_set_system_volume(client, sys_volume);
			break;
		case 'j':
			sys_volume = sys_volume == 0 ? 0 : sys_volume - 1;
			cras_client_set_system_volume(client, sys_volume);
			break;
		case 'K':
			cap_gain = MIN(cap_gain + 100, 5000);
			cras_client_set_system_capture_gain(client, cap_gain);
			break;
		case 'J':
			cap_gain = cap_gain == -5000 ? -5000 : cap_gain - 100;
			cras_client_set_system_capture_gain(client, cap_gain);
			break;
		case 'm':
			mute = !mute;
			cras_client_set_system_mute(client, mute);
			break;
		case '@':
			print_device_lists(client);
			break;
		case '#':
			print_attached_client_list(client);
			break;
		case 'v':
			printf("Volume: %zu%s Min dB: %ld Max dB: %ld\n"
			       "Capture: %ld%s Min dB: %ld Max dB: %ld\n",
			       cras_client_get_system_volume(client),
			       cras_client_get_system_muted(client) ?
				       "(Muted)" :
				       "",
			       cras_client_get_system_min_volume(client),
			       cras_client_get_system_max_volume(client),
			       cras_client_get_system_capture_gain(client),
			       cras_client_get_system_capture_muted(client) ?
				       "(Muted)" :
				       "",
			       cras_client_get_system_min_capture_gain(client),
			       cras_client_get_system_max_capture_gain(client));
			break;
		case '\'':
			play_short_sound_periods_left =
				play_short_sound_periods;
			break;
		case '\n':
			break;
		default:
			printf("Invalid key\n");
			break;
		}
	}

	if (show_total_rms)
		print_total_rms();

	cras_client_stop(client);

	cras_audio_format_destroy(aud_format);
	cras_client_stream_params_destroy(params);
	free(pfd);

	close(pipefd[0]);
	close(pipefd[1]);

	return 0;
}

static int run_capture(struct cras_client *client, const char *file,
		       size_t block_size, enum CRAS_STREAM_TYPE stream_type,
		       size_t rate, snd_pcm_format_t format,
		       size_t num_channels, uint32_t flags, int is_loopback,
		       int is_post_dsp)
{
	int fd = open(file, O_CREAT | O_RDWR | O_TRUNC, 0666);
	if (fd == -1) {
		perror("failed to open file");
		return -errno;
	}

	run_file_io_stream(client, fd, CRAS_STREAM_INPUT, block_size,
			   stream_type, rate, format, num_channels, flags,
			   is_loopback, is_post_dsp);

	close(fd);
	return 0;
}

static int run_playback(struct cras_client *client, const char *file,
			size_t block_size, enum CRAS_STREAM_TYPE stream_type,
			size_t rate, snd_pcm_format_t format,
			size_t num_channels)
{
	int fd;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		perror("failed to open file");
		return -errno;
	}

	run_file_io_stream(client, fd, CRAS_STREAM_OUTPUT, block_size,
			   stream_type, rate, format, num_channels, 0, 0, 0);

	close(fd);
	return 0;
}

static void print_server_info(struct cras_client *client)
{
	cras_client_run_thread(client);
	cras_client_connected_wait(client); /* To synchronize data. */
	print_system_volumes(client);
	print_user_muted(client);
	print_device_lists(client);
	print_attached_client_list(client);
	print_active_stream_info(client);
}

static void show_audio_thread_snapshots(struct cras_client *client)
{
	struct timespec wait_time;

	cras_client_run_thread(client);
	cras_client_connected_wait(client); /* To synchronize data. */
	cras_client_update_audio_thread_snapshots(client,
						  audio_thread_snapshots);

	clock_gettime(CLOCK_REALTIME, &wait_time);
	wait_time.tv_sec += 2;

	pthread_mutex_lock(&done_mutex);
	pthread_cond_timedwait(&done_cond, &done_mutex, &wait_time);
	pthread_mutex_unlock(&done_mutex);
}

static void show_audio_debug_info(struct cras_client *client)
{
	struct timespec wait_time;

	cras_client_run_thread(client);
	cras_client_connected_wait(client); /* To synchronize data. */
	cras_client_update_audio_debug_info(client, audio_debug_info);

	clock_gettime(CLOCK_REALTIME, &wait_time);
	wait_time.tv_sec += 2;

	pthread_mutex_lock(&done_mutex);
	pthread_cond_timedwait(&done_cond, &done_mutex, &wait_time);
	pthread_mutex_unlock(&done_mutex);
}

static void show_cras_bt_debug_info(struct cras_client *client)
{
	struct timespec wait_time;

	cras_client_run_thread(client);
	cras_client_connected_wait(client); /* To synchronize data. */
	cras_client_update_bt_debug_info(client, cras_bt_debug_info);

	clock_gettime(CLOCK_REALTIME, &wait_time);
	wait_time.tv_sec += 2;

	pthread_mutex_lock(&done_mutex);
	pthread_cond_timedwait(&done_cond, &done_mutex, &wait_time);
	pthread_mutex_unlock(&done_mutex);
}

static void hotword_models_cb(struct cras_client *client,
			      const char *hotword_models)
{
	printf("Hotword models: %s\n", hotword_models);
}

static void print_hotword_models(struct cras_client *client, cras_node_id_t id)
{
	struct timespec wait_time;

	cras_client_run_thread(client);
	cras_client_connected_wait(client);
	cras_client_get_hotword_models(client, id, hotword_models_cb);

	clock_gettime(CLOCK_REALTIME, &wait_time);
	wait_time.tv_sec += 2;

	pthread_mutex_lock(&done_mutex);
	pthread_cond_timedwait(&done_cond, &done_mutex, &wait_time);
	pthread_mutex_unlock(&done_mutex);
}

static void check_output_plugged(struct cras_client *client, const char *name)
{
	cras_client_run_thread(client);
	cras_client_connected_wait(client); /* To synchronize data. */
	printf("%s\n",
	       cras_client_output_dev_plugged(client, name) ? "Yes" : "No");
}

/* Repeatedly mute and unmute the output until there is an error. */
static void mute_loop_test(struct cras_client *client, int auto_reconnect)
{
	int mute = 0;
	int rc;

	if (auto_reconnect)
		cras_client_run_thread(client);
	while (1) {
		rc = cras_client_set_user_mute(client, mute);
		printf("cras_client_set_user_mute(%d): %d\n", mute, rc);
		if (rc != 0 && !auto_reconnect)
			return;
		mute = !mute;
		sleep(2);
	}
}

static void show_atlog(time_t sec_offset, int32_t nsec_offset,
		       struct audio_thread_event_log *log, int len,
		       uint64_t missing)
{
	int i;
	printf("Audio Thread Event Log:\n");

	if (missing)
		printf("%" PRIu64 " logs are missing.\n", missing);

	for (i = 0; i < len; ++i) {
		show_alog_tag(log, i, sec_offset, nsec_offset);
	}
}

static void unlock_main_thread(struct cras_client *client)
{
	pthread_mutex_lock(&done_mutex);
	pthread_cond_signal(&done_cond);
	pthread_mutex_unlock(&done_mutex);
}

static void cras_show_continuous_atlog(struct cras_client *client)
{
	struct audio_thread_event_log log;
	struct timespec wait_time;
	static time_t sec_offset;
	static int32_t nsec_offset;
	static uint64_t atlog_read_idx = 0, missing;
	int len, rc;

	cras_client_run_thread(client);
	cras_client_connected_wait(client); /* To synchronize data. */
	cras_client_get_atlog_access(client, unlock_main_thread);

	clock_gettime(CLOCK_REALTIME, &wait_time);
	wait_time.tv_sec += 2;

	pthread_mutex_lock(&done_mutex);
	rc = pthread_cond_timedwait(&done_cond, &done_mutex, &wait_time);
	pthread_mutex_unlock(&done_mutex);

	if (rc)
		goto fail;

	fill_time_offset(&sec_offset, &nsec_offset);

	while (1) {
		len = cras_client_read_atlog(client, &atlog_read_idx, &missing,
					     &log);

		if (len < 0)
			break;
		if (len > 0)
			show_atlog(sec_offset, nsec_offset, &log, len, missing);
		nanosleep(&follow_atlog_sleep_ts, NULL);
	}
fail:
	printf("Failed to get audio thread log.\n");
}

// clang-format off
static struct option long_options[] = {
	{"show_latency",        no_argument,            &show_latency, 1},
	{"show_rms",            no_argument,            &show_rms, 1},
	{"show_total_rms",      no_argument,            &show_total_rms, 1},
	{"select_input",        required_argument,      0, 'a'},
	{"block_size",          required_argument,      0, 'b'},
	{"num_channels",        required_argument,      0, 'c'},
	{"duration_seconds",    required_argument,      0, 'd'},
	{"dump_events",         no_argument,            0, 'e'},
	{"format",              required_argument,      0, 'f'},
	{"capture_gain",        required_argument,      0, 'g'},
	{"help",                no_argument,            0, 'h'},
	{"dump_server_info",    no_argument,            0, 'i'},
	{"check_output_plugged",required_argument,      0, 'j'},
	{"add_active_input",    required_argument,      0, 'k'},
	{"dump_dsp",            no_argument,            0, 'l'},
	{"dump_audio_thread",   no_argument,            0, 'm'},
	{"syslog_mask",         required_argument,      0, 'n'},
	{"channel_layout",      required_argument,      0, 'o'},
	{"get_aec_group_id",    no_argument,            0, 'p'},
	{"user_mute",           required_argument,      0, 'q'},
	{"rate",                required_argument,      0, 'r'},
	{"reload_dsp",          no_argument,            0, 's'},
	{"add_active_output",   required_argument,      0, 't'},
	{"mute",                required_argument,      0, 'u'},
	{"volume",              required_argument,      0, 'v'},
	{"set_node_volume",     required_argument,      0, 'w'},
	{"plug",                required_argument,      0, 'x'},
	{"select_output",       required_argument,      0, 'y'},
	{"playback_delay_us",   required_argument,      0, 'z'},
	{"capture_mute",        required_argument,      0, '0'},
	{"rm_active_input",     required_argument,      0, '1'},
	{"rm_active_output",    required_argument,      0, '2'},
	{"swap_left_right",     required_argument,      0, '3'},
	{"version",             no_argument,            0, '4'},
	{"add_test_dev",        required_argument,      0, '5'},
	{"test_hotword_file",   required_argument,      0, '6'},
	{"listen_for_hotword",  required_argument,      0, '7'},
	{"pin_device",          required_argument,      0, '8'},
	{"suspend",             required_argument,      0, '9'},
	{"set_node_gain",       required_argument,      0, ':'},
	{"play_short_sound",    required_argument,      0, '!'},
	{"config_global_remix", required_argument,      0, ';'},
	{"set_hotword_model",   required_argument,      0, '<'},
	{"get_hotword_models",  required_argument,      0, '>'},
	{"post_dsp",            required_argument,      0, 'A'},
	{"stream_id",           required_argument,      0, 'B'},
	{"capture_file",        required_argument,      0, 'C'},
	{"reload_aec_config",   no_argument,            0, 'D'},
	{"effects",             required_argument,      0, 'E'},
	{"get_aec_supported",   no_argument,            0, 'F'},
	{"aecdump",             required_argument,      0, 'G'},
	{"dump_bt",             no_argument,            0, 'H'},
	{"set_wbs_enabled",     required_argument,      0, 'I'},
	{"follow_atlog",	no_argument,		0, 'J'},
	{"connection_type",     required_argument,      0, 'K'},
	{"loopback_file",       required_argument,      0, 'L'},
	{"mute_loop_test",      required_argument,      0, 'M'},
	{"playback_file",       required_argument,      0, 'P'},
	{"stream_type",         required_argument,      0, 'T'},
	{0, 0, 0, 0}
};
// clang-format on

static void show_usage()
{
	int i;

	printf("--add_active_input <N>:<M> - "
	       "Add the ionode with the given id to active input device "
	       "list\n");
	printf("--add_active_output <N>:<M> - "
	       "Add the ionode with the given id to active output device "
	       "list\n");
	printf("--add_test_dev <type> - "
	       "Add a test iodev.\n");
	printf("--block_size <N> - "
	       "The number for frames per callback(dictates latency).\n");
	printf("--capture_file <name> - "
	       "Name of file to record to.\n");
	printf("--capture_gain <dB> - "
	       "Set system caputre gain in dB*100 (100 = 1dB).\n");
	printf("--capture_mute <0|1> - "
	       "Set capture mute state.\n");
	printf("--channel_layout <layout_str> - "
	       "Set multiple channel layout.\n");
	printf("--check_output_plugged <output name> - "
	       "Check if the output is plugged in\n");
	printf("--connection_type <connection_type> - "
	       "Set cras_client connection_type (default to 0).\n"
	       "                                      "
	       "Argument: 0 - For control client.\n"
	       "                                      "
	       "          1 - For playback client.\n"
	       "                                      "
	       "          2 - For capture client.\n");
	printf("--dump_audio_thread - "
	       "Dumps audio thread info.\n");
	printf("--dump_bt - "
	       "Dumps debug info for bt audio\n");
	printf("--dump_dsp - "
	       "Print status of dsp to syslog.\n");
	printf("--dump_server_info - "
	       "Print status of the server.\n");
	printf("--duration_seconds <N> - "
	       "Seconds to record or playback.\n");
	printf("--follow_atlog - "
	       "Continuously dumps audio thread event log.\n");
	printf("--format <name> - "
	       "The sample format. Either ");
	for (i = 0; supported_formats[i].name; ++i)
		printf("%s ", supported_formats[i].name);
	printf("(default to S16_LE).\n");
	printf("--get_hotword_models <N>:<M> - "
	       "Get the supported hotword models of node\n");
	printf("--help - "
	       "Print this message.\n");
	printf("--listen_for_hotword <name> - "
	       "Listen and capture hotword stream if supported\n");
	printf("--loopback_file <name> - "
	       "Name of file to record from loopback device.\n");
	printf("--mute <0|1> - "
	       "Set system mute state.\n");
	printf("--mute_loop_test <0|1> - "
	       "Continuously loop mute/umute.\n"
	       "                         "
	       "Argument: 0 - stop on error.\n"
	       "                         "
	       "          1 - automatically reconnect to CRAS.\n");
	printf("--num_channels <N> - "
	       "Two for stereo.\n");
	printf("--pin_device <N> - "
	       "Playback/Capture only on the given device.\n");
	printf("--playback_file <name> - "
	       "Name of file to play, "
	       "\"-\" to playback raw audio from stdin.\n");
	printf("--play_short_sound <N> - "
	       "Plays the content in the file for N periods when ' "
	       "is pressed.\n");
	printf("--plug <N>:<M>:<0|1> - "
	       "Set the plug state (0 or 1) for the ionode with the given "
	       "index M on the device with index N\n");
	printf("--rate <N> - "
	       "Specifies the sample rate in Hz.\n");
	printf("--reload_dsp - "
	       "Reload dsp configuration from the ini file\n");
	printf("--rm_active_input <N>:<M> - "
	       "Removes the ionode with the given id from active input device "
	       "list\n");
	printf("--rm_active_output <N>:<M> - "
	       "Removes the ionode with the given id from active output device "
	       "list\n");
	printf("--select_input <N>:<M> - "
	       "Select the ionode with the given id as preferred input\n");
	printf("--select_output <N>:<M> - "
	       "Select the ionode with the given id as preferred output\n");
	printf("--set_hotword_model <N>:<M>:<model> - "
	       "Set the model to node\n");
	printf("--playback_delay_us <N> - "
	       "Set the time in us to delay a reply for playback when i is "
	       "pressed\n");
	printf("--post_dsp <0|1> - "
	       "Use this flag with --loopback_file. The default value is 0.\n"
	       "                   "
	       "Argument: 0 - Record from post-mix, pre-DSP loopback device.\n"
	       "                   "
	       "          1 - Record from post-DSP loopback device.\n");
	printf("--set_node_volume <N>:<M>:<0-100> - "
	       "Set the volume of the ionode with the given id\n");
	printf("--show_latency - "
	       "Display latency while playing or recording.\n");
	printf("--show_rms - "
	       "Display RMS value of loopback stream.\n");
	printf("--show_total_rms - "
	       "Display total RMS value of loopback stream at the end.\n");
	printf("--suspend <0|1> - "
	       "Set audio suspend state.\n");
	printf("--swap_left_right <N>:<M>:<0|1> - "
	       "Swap or unswap (1 or 0) the left and right channel for the "
	       "ionode with the given index M on the device with index N\n");
	printf("--stream_type <N> - "
	       "Specify the type of the stream.\n");
	printf("--syslog_mask <n> - "
	       "Set the syslog mask to the given log level.\n");
	printf("--test_hotword_file <N>:<filename> - "
	       "Use filename as a hotword buffer for device N\n");
	printf("--user_mute <0|1> - "
	       "Set user mute state.\n");
	printf("--version - "
	       "Print the git commit ID that was used to build the client.\n");
	printf("--volume <0-100> - "
	       "Set system output volume.\n");
}

static int cras_client_create_and_connect(struct cras_client **client,
					  enum CRAS_CONNECTION_TYPE conn_type)
{
	int rc;

	rc = cras_client_create_with_type(client, conn_type);
	if (rc < 0) {
		fprintf(stderr, "Couldn't create client.\n");
		return rc;
	}

	rc = cras_client_connect_timeout(*client, 1000);
	if (rc) {
		fprintf(stderr, "Couldn't connect to server.\n");
		cras_client_destroy(*client);
		return rc;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct cras_client *client;
	int c, option_index;
	size_t block_size = NOT_ASSIGNED;
	size_t rate = 48000;
	size_t num_channels = 2;
	float duration_seconds = 0;
	const char *capture_file = NULL;
	const char *playback_file = NULL;
	const char *loopback_file = NULL;
	int post_dsp = 0;
	enum CRAS_STREAM_TYPE stream_type = CRAS_STREAM_TYPE_DEFAULT;
	int rc = 0;
	uint32_t stream_flags = 0;
	cras_stream_id_t stream_id = 0;
	snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
	enum CRAS_CONNECTION_TYPE conn_type = CRAS_CONTROL;
	enum CRAS_CONNECTION_TYPE new_conn_type;

	option_index = 0;
	openlog("cras_test_client", LOG_PERROR, LOG_USER);
	setlogmask(LOG_UPTO(LOG_INFO));

	rc = cras_client_create_and_connect(&client, conn_type);
	if (rc) {
		return rc;
	}

	if (argc == 1) {
		/* Nothing specified, default to dump_server_info. */
		print_server_info(client);
		goto destroy_exit;
	}

	while (1) {
		c = getopt_long(argc, argv, "o:s:P:C:r:c:f:h", long_options,
				&option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'y':
		case 'a': {
			cras_node_id_t id;
			rc = parse_node_id(optarg, &id);
			if (rc) {
				show_usage();
				return rc;
			}

			enum CRAS_STREAM_DIRECTION direction =
				(c == 'y') ? CRAS_STREAM_OUTPUT :
					     CRAS_STREAM_INPUT;
			cras_client_select_node(client, direction, id);
			break;
		}
		case 'b':
			block_size = atoi(optarg);
			break;
		case 'c':
			num_channels = atoi(optarg);
			break;
		case 'd':
			duration_seconds = atof(optarg);
			break;
		case 'e':
			show_audio_thread_snapshots(client);
			break;
		case 'f': {
			int i;

			for (i = 0; supported_formats[i].name; ++i) {
				if (strcasecmp(optarg,
					       supported_formats[i].name) ==
				    0) {
					format = supported_formats[i].format;
					break;
				}
			}

			if (!supported_formats[i].name) {
				printf("Unsupported format: %s\n", optarg);
				return -EINVAL;
			}
			break;
		}
		case 'g': {
			long gain = atol(optarg);
			rc = cras_client_set_system_capture_gain(client, gain);
			if (rc < 0) {
				fprintf(stderr, "problem setting capture\n");
				goto destroy_exit;
			}
			break;
		}
		case 'h':
			show_usage();
			break;
		case 'i':
			print_server_info(client);
			break;
		case 'j':
			check_output_plugged(client, optarg);
			break;
		case 'k':
		case 't':
		case '1':
		case '2': {
			cras_node_id_t id;
			rc = parse_node_id(optarg, &id);
			if (rc) {
				show_usage();
				return rc;
			}

			enum CRAS_STREAM_DIRECTION dir;
			if (c == 't' || c == '2')
				dir = CRAS_STREAM_OUTPUT;
			else
				dir = CRAS_STREAM_INPUT;

			if (c == 'k' || c == 't')
				cras_client_add_active_node(client, dir, id);
			else
				cras_client_rm_active_node(client, dir, id);
			break;
		}
		case 'l':
			cras_client_dump_dsp_info(client);
			break;
		case 'm':
			show_audio_debug_info(client);
			break;
		case 'n': {
			int log_level = atoi(optarg);

			setlogmask(LOG_UPTO(log_level));
			break;
		}
		case 'o':
			channel_layout = optarg;
			break;
		case 'p':
			printf("AEC group ID %d\n",
			       cras_client_get_aec_group_id(client));
			break;
		case 'q': {
			int mute = atoi(optarg);
			rc = cras_client_set_user_mute(client, mute);
			if (rc < 0) {
				fprintf(stderr, "problem setting mute\n");
				goto destroy_exit;
			}
			break;
		}
		case 'r':
			rate = atoi(optarg);
			break;
		case 's':
			cras_client_reload_dsp(client);
			break;
		case 'u': {
			int mute = atoi(optarg);
			rc = cras_client_set_system_mute(client, mute);
			if (rc < 0) {
				fprintf(stderr, "problem setting mute\n");
				goto destroy_exit;
			}
			break;
		}
		case 'v': {
			int volume = atoi(optarg);
			volume = MIN(100, MAX(0, volume));
			rc = cras_client_set_system_volume(client, volume);
			if (rc < 0) {
				fprintf(stderr, "problem setting volume\n");
				goto destroy_exit;
			}
			break;
		}
		case ':':
		case 'w': {
			cras_node_id_t id;
			int value;
			rc = parse_node_id_with_value(optarg, &id, &value);
			if (rc) {
				show_usage();
				return rc;
			}

			if (c == 'w')
				cras_client_set_node_volume(client, id, value);
			else
				cras_client_set_node_capture_gain(client, id,
								  value);
			break;
		}
		case 'x': {
			cras_node_id_t id;
			int value;
			rc = parse_node_id_with_value(optarg, &id, &value);
			if (rc) {
				show_usage();
				return rc;
			}

			enum ionode_attr attr = IONODE_ATTR_PLUGGED;
			cras_client_set_node_attr(client, id, attr, value);
			break;
		}
		case 'z':
			pause_in_playback_reply = atoi(optarg);
			break;

		case '0': {
			int mute = atoi(optarg);
			rc = cras_client_set_system_capture_mute(client, mute);
			if (rc < 0) {
				fprintf(stderr, "problem setting mute\n");
				goto destroy_exit;
			}
			break;
		}
		case '3': {
			cras_node_id_t id;
			int value;
			rc = parse_node_id_with_value(optarg, &id, &value);
			if (rc) {
				show_usage();
				return rc;
			}

			cras_client_swap_node_left_right(client, id, value);
			break;
		}
		case '4':
			printf("%s\n", VCSID);
			break;
		case '5': {
			cras_client_add_test_iodev(client, atoi(optarg));
			break;
		}
		case '6': {
			const char *s;
			int dev_index;

			s = strtok(optarg, ":");
			if (!s) {
				show_usage();
				return -EINVAL;
			}
			dev_index = atoi(s);

			const char *file_name = strtok(NULL, ":");
			if (!file_name) {
				show_usage();
				return -EINVAL;
			}
			cras_client_test_iodev_command(
				client, dev_index,
				TEST_IODEV_CMD_HOTWORD_TRIGGER,
				strlen(file_name) + 1, (uint8_t *)file_name);
			break;
		}
		case '7': {
			stream_flags = HOTWORD_STREAM;
			capture_file = optarg;
			break;
		}
		case '8':
			pin_device_id = atoi(optarg);
			break;
		case '9': {
			int suspend = atoi(optarg);
			cras_client_set_suspend(client, suspend);
			break;
		}

		case '!': {
			play_short_sound = 1;
			play_short_sound_periods = atoi(optarg);
			break;
		}
		case ';': {
			char *s;
			int nch;
			int size = 0;
			float *coeff;

			s = strtok(optarg, ":");
			nch = atoi(s);
			coeff = (float *)calloc(nch * nch, sizeof(*coeff));
			for (size = 0; size < nch * nch; size++) {
				s = strtok(NULL, ",");
				if (NULL == s)
					break;
				coeff[size] = atof(s);
			}
			cras_client_config_global_remix(client, nch, coeff);
			free(coeff);
			break;
		}
		case '<':
		case '>': {
			char *s;
			int dev_index;
			int node_index;

			s = strtok(optarg, ":");
			if (!s) {
				show_usage();
				return -EINVAL;
			}
			dev_index = atoi(s);

			s = strtok(NULL, ":");
			if (!s) {
				show_usage();
				return -EINVAL;
			}
			node_index = atoi(s);

			s = strtok(NULL, ":");
			if (!s && c == ';') {
				//TODO: c never == ';'
				show_usage();
				return -EINVAL;
			}

			cras_node_id_t id =
				cras_make_node_id(dev_index, node_index);
			if (c == '<')
				cras_client_set_hotword_model(client, id, s);
			else
				print_hotword_models(client, id);
			break;
		}

		case 'A':
			post_dsp = atoi(optarg);
			break;
		case 'B':
			stream_id = atoi(optarg);
			break;
		case 'C':
			capture_file = optarg;
			break;
		case 'D':
			cras_client_reload_aec_config(client);
			break;
		case 'E': {
			char *s;

			s = strtok(optarg, ",");
			while (s) {
				if (strcmp("aec", s) == 0)
					effect_aec = 1;
				else if (strcmp("ns", s) == 0)
					effect_ns = 1;
				else if (strcmp("agc", s) == 0)
					effect_agc = 1;
				else if (strcmp("vad", s) == 0)
					effect_vad = 1;
				else
					printf("Unknown effect %s\n", s);
				s = strtok(NULL, ",");
			}
			break;
		}
		case 'F':
			printf("AEC supported %d\n",
			       !!cras_client_get_aec_supported(client));
			break;
		case 'G':
			aecdump_file = optarg;
			break;
		case 'H':
			show_cras_bt_debug_info(client);
			break;
		case 'I':
			cras_client_set_bt_wbs_enabled(client, atoi(optarg));
			break;
		case 'J':
			cras_show_continuous_atlog(client);
			break;
		case 'K':
			new_conn_type = atoi(optarg);
			if (cras_validate_connection_type(new_conn_type)) {
				if (new_conn_type != conn_type) {
					cras_client_destroy(client);
					client = NULL;
					rc = cras_client_create_and_connect(
						&client, new_conn_type);
					if (rc) {
						fprintf(stderr,
							"Couldn't connect to "
							"server.\n");
						return rc;
					}
					conn_type = new_conn_type;
				}
			} else {
				printf("Input connection type is not "
				       "supported.\n");
			}
			break;
		case 'L':
			loopback_file = optarg;
			break;
		case 'M':
			mute_loop_test(client, atoi(optarg));
			break;
		case 'P':
			playback_file = optarg;
			break;
		case 'T':
			stream_type = atoi(optarg);
			break;

		default:
			break;
		}
	}

	duration_frames = duration_seconds * rate;
	if (block_size == NOT_ASSIGNED)
		block_size = get_block_size(PLAYBACK_BUFFERED_TIME_IN_US, rate);

	if (capture_file != NULL) {
		if (strcmp(capture_file, "-") == 0)
			rc = run_file_io_stream(client, 1, CRAS_STREAM_INPUT,
						block_size, stream_type, rate,
						format, num_channels,
						stream_flags, 0, 0);
		else
			rc = run_capture(client, capture_file, block_size,
					 stream_type, rate, format,
					 num_channels, stream_flags, 0, 0);
	} else if (playback_file != NULL) {
		if (strcmp(playback_file, "-") == 0)
			rc = run_file_io_stream(client, 0, CRAS_STREAM_OUTPUT,
						block_size, stream_type, rate,
						format, num_channels,
						stream_flags, 0, 0);
		else
			rc = run_playback(client, playback_file, block_size,
					  stream_type, rate, format,
					  num_channels);
	} else if (loopback_file != NULL) {
		rc = run_capture(client, loopback_file, block_size, stream_type,
				 rate, format, num_channels, stream_flags, 1,
				 post_dsp);
	} else if (aecdump_file != NULL) {
		run_aecdump(client, stream_id, 1);
		sleep(duration_seconds);
		run_aecdump(client, stream_id, 0);
	}

destroy_exit:
	cras_client_destroy(client);
	return rc;
}
