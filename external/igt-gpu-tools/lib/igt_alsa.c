/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *  Paul Kocialkowski <paul.kocialkowski@linux.intel.com>
 */

#include "config.h"

#include <limits.h>

#include "igt_alsa.h"
#include "igt_aux.h"
#include "igt_core.h"

#define HANDLES_MAX	8

/**
 * SECTION:igt_alsa
 * @short_description: Library with ALSA helpers
 * @title: ALSA
 * @include: igt_alsa.h
 *
 * This library contains helpers for ALSA playback and capture.
 */

struct alsa {
	snd_pcm_t *output_handles[HANDLES_MAX];
	int output_handles_count;
	snd_pcm_format_t output_format;
	int output_sampling_rate;
	int output_channels;

	int (*output_callback)(void *data, void *buffer, int samples);
	void *output_callback_data;
	int output_samples_trigger;
};

/**
 * alsa_has_exclusive_access:
 * Check whether ALSA has exclusive access to audio devices. Fails if
 * PulseAudio is running.
 */
bool alsa_has_exclusive_access(void)
{
	if (igt_is_process_running("pulseaudio")) {
		igt_warn("alsa doesn't have exclusive access to audio devices\n");
		igt_warn("It seems that PulseAudio is running. Audio tests "
			 "need direct access to audio devices, so PulseAudio "
			 "needs to be stopped. You can do so by running "
			 "`pulseaudio --kill`. Also make sure to add "
			 "autospawn=no to /etc/pulse/client.conf\n");
		return false;
	}

	return true;
}

static void alsa_error_handler(const char *file, int line, const char *function,
			       int err, const char *fmt, ...)
{
	if (err)
		igt_debug("[ALSA] %s: %s\n", function, snd_strerror(err));
}

/**
 * alsa_init:
 * Allocate and initialize an alsa structure and configure the error handler.
 *
 * Returns: A newly-allocated alsa structure
 */
struct alsa *alsa_init(void)
{
	struct alsa *alsa;

	if (!alsa_has_exclusive_access()) {
		return NULL;
	}

	alsa = malloc(sizeof(struct alsa));
	memset(alsa, 0, sizeof(struct alsa));

	/* Redirect errors to igt_debug instead of stderr. */
	snd_lib_error_set_handler(alsa_error_handler);

	return alsa;
}

static char *alsa_resolve_indentifier(const char *device_name, int skip)
{
	snd_ctl_card_info_t *card_info;
	snd_pcm_info_t *pcm_info;
	snd_ctl_t *handle = NULL;
	const char *pcm_name;
	char *identifier = NULL;
	char name[32];
	int card = -1;
	int dev;
	int ret;

	snd_ctl_card_info_alloca(&card_info);
	snd_pcm_info_alloca(&pcm_info);

	/* First try to open the device as-is. */
	if (!skip) {
		ret = snd_ctl_open(&handle, device_name, 0);
		if (!ret) {
			identifier = strdup(device_name);
			goto resolved;
		}
	}

	do {
		ret = snd_card_next(&card);
		if (ret < 0 || card < 0)
			break;

		snprintf(name, sizeof(name), "hw:%d", card);

		ret = snd_ctl_open(&handle, name, 0);
		if (ret < 0)
			continue;

		ret = snd_ctl_card_info(handle, card_info);
		if (ret < 0) {
			snd_ctl_close(handle);
			handle = NULL;
			continue;
		}

		dev = -1;

		do {
			ret = snd_ctl_pcm_next_device(handle, &dev);
			if (ret < 0 || dev < 0)
				break;

			snd_pcm_info_set_device(pcm_info, dev);
			snd_pcm_info_set_subdevice(pcm_info, 0);

			ret = snd_ctl_pcm_info(handle, pcm_info);
			if (ret < 0)
				continue;

			pcm_name = snd_pcm_info_get_name(pcm_info);
			if (!pcm_name)
				continue;

			ret = strncmp(device_name, pcm_name,
				      strlen(device_name));

			if (ret == 0) {
				if (skip > 0) {
					skip--;
					continue;
				}

				igt_debug("Matched device \"%s\"\n", pcm_name);

				snprintf(name, sizeof(name), "hw:%d,%d", card,
					 dev);

				identifier = strdup(name);
				goto resolved;
			}
		} while (dev >= 0);

		snd_ctl_close(handle);
		handle = NULL;
	} while (card >= 0);

resolved:
	if (handle)
		snd_ctl_close(handle);

	return identifier;
}

/**
 * alsa_open_output:
 * @alsa: The target alsa structure
 * @device_name: The name prefix of the output device(s) to open
 *
 * Open ALSA output devices whose name prefixes match the provided name prefix.
 *
 * Returns: An integer equal to zero for success and negative for failure
 */
int alsa_open_output(struct alsa *alsa, const char *device_name)
{
	snd_pcm_t *handle;
	char *identifier;
	int skip;
	int index;
	int ret;

	skip = alsa->output_handles_count;
	index = alsa->output_handles_count;

	while (index < HANDLES_MAX) {
		identifier = alsa_resolve_indentifier(device_name, skip++);
		if (!identifier)
			break;

		ret = snd_pcm_open(&handle, identifier, SND_PCM_STREAM_PLAYBACK,
				   SND_PCM_NONBLOCK);
		if (ret < 0) {
			free(identifier);
			continue;
		}

		igt_debug("Opened output %s\n", identifier);

		alsa->output_handles[index++] = handle;
		free(identifier);
	}

	if (index == 0)
		return -1;

	alsa->output_handles_count = index;

	return 0;
}

/**
 * alsa_close_output:
 * @alsa: The target alsa structure
 *
 * Close all the open ALSA outputs.
 */
void alsa_close_output(struct alsa *alsa)
{
	snd_pcm_t *handle;
	int i;

	for (i = 0; i < alsa->output_handles_count; i++) {
		handle = alsa->output_handles[i];
		if (!handle)
			continue;

		snd_pcm_close(handle);
		alsa->output_handles[i] = NULL;
	}

	alsa->output_handles_count = 0;

	alsa->output_callback = NULL;
}

static bool alsa_test_configuration(snd_pcm_t *handle, snd_pcm_format_t fmt,
				    int channels, int sampling_rate)
{
	snd_pcm_hw_params_t *params;
	int ret;
	unsigned int min_channels, max_channels;
	unsigned int min_rate, max_rate;
	int min_rate_dir, max_rate_dir;

	snd_pcm_hw_params_alloca(&params);

	ret = snd_pcm_hw_params_any(handle, params);
	if (ret < 0)
		return false;

	ret = snd_pcm_hw_params_test_format(handle, params, fmt);
	if (ret < 0) {
		igt_debug("Output device doesn't support the format %s\n",
			  snd_pcm_format_name(fmt));
		return false;
	}

	ret = snd_pcm_hw_params_test_rate(handle, params, sampling_rate, 0);
	if (ret < 0) {
		snd_pcm_hw_params_get_rate_min(params, &min_rate, &min_rate_dir);
		snd_pcm_hw_params_get_rate_max(params, &max_rate, &max_rate_dir);
		igt_debug("Output device supports rates between %u and %u, "
			  "requested %d\n",
			  min_rate, max_rate, sampling_rate);
		return false;
	}

	ret = snd_pcm_hw_params_test_channels(handle, params, channels);
	if (ret < 0) {
		snd_pcm_hw_params_get_channels_min(params, &min_channels);
		snd_pcm_hw_params_get_channels_max(params, &max_channels);
		igt_debug("Output device supports between %u and "
			  "%u channels, requested %d\n",
			  min_channels, max_channels, channels);
		return false;
	}

	return true;
}

/**
 * alsa_test_output_configuration:
 * @alsa: The target alsa structure
 * @fmt: The format to test
 * @channels: The number of channels to test
 * @sampling_rate: The sampling rate to test
 *
 * Test the output configuration specified by @channels and @sampling_rate
 * for the output devices.
 *
 * Returns: A boolean indicating whether the test succeeded
 */
bool alsa_test_output_configuration(struct alsa *alsa, snd_pcm_format_t fmt,
				    int channels, int sampling_rate)
{
	snd_pcm_t *handle;
	bool ret;
	int i;

	for (i = 0; i < alsa->output_handles_count; i++) {
		handle = alsa->output_handles[i];

		ret = alsa_test_configuration(handle, fmt, channels, sampling_rate);
		if (!ret)
			return false;
	}

	return true;
}

/**
 * alsa_configure_output:
 * @alsa: The target alsa structure
 * @channels: The number of channels to test
 * @sampling_rate: The sampling rate to test
 *
 * Configure the output devices with the configuration specified by @channels
 * and @sampling_rate.
 */
void alsa_configure_output(struct alsa *alsa, snd_pcm_format_t fmt,
			   int channels, int sampling_rate)
{
	snd_pcm_t *handle;
	int ret;
	int i;
	int soft_resample = 0; /* Don't allow ALSA to resample */
	unsigned int latency = 0;

	for (i = 0; i < alsa->output_handles_count; i++) {
		handle = alsa->output_handles[i];

		ret = snd_pcm_set_params(handle, fmt,
					 SND_PCM_ACCESS_RW_INTERLEAVED,
					 channels, sampling_rate,
					 soft_resample, latency);
		igt_assert(ret >= 0);
	}

	alsa->output_format = fmt;
	alsa->output_channels = channels;
	alsa->output_sampling_rate = sampling_rate;
}

/**
 * alsa_register_output_callback:
 * @alsa: The target alsa structure
 * @callback: The callback function to call to fill output data
 * @callback_data: The data pointer to pass to the callback function
 * @samples_trigger: The required number of samples to trigger the callback
 *
 * Register a callback function to be called to fill output data during a run.
 * The callback is called when @samples_trigger samples are required.
 *
 * The callback should return an integer equal to zero for success and negative
 * for failure.
 */
void alsa_register_output_callback(struct alsa *alsa,
				   int (*callback)(void *data, void *buffer, int samples),
				   void *callback_data, int samples_trigger)
{
	alsa->output_callback = callback;
	alsa->output_callback_data = callback_data;
	alsa->output_samples_trigger = samples_trigger;
}

/**
 * alsa_run:
 * @alsa: The target alsa structure
 * @duration_ms: The maximum duration of the run in milliseconds, or -1 for an
 * infinite duration.
 *
 * Run ALSA playback and capture on the input and output devices for at
 * most @duration_ms milliseconds, calling the registered callbacks when needed.
 *
 * Returns: An integer equal to zero for success, positive for a stop caused
 * by the input callback and negative for failure
 */
int alsa_run(struct alsa *alsa, int duration_ms)
{
	snd_pcm_t *handle;
	char *output_buffer = NULL;
	int output_limit;
	int output_total = 0;
	int output_counts[alsa->output_handles_count];
	bool output_ready = false;
	int output_channels;
	int bytes_per_sample;
	int output_trigger;
	bool reached;
	int index;
	int count;
	int avail;
	int i;
	int ret;

	output_limit = alsa->output_sampling_rate * duration_ms / 1000;
	output_channels = alsa->output_channels;
	bytes_per_sample = snd_pcm_format_physical_width(alsa->output_format) / 8;
	output_trigger = alsa->output_samples_trigger;
	output_buffer = malloc(output_channels * output_trigger *
			       bytes_per_sample);

	do {
		reached = true;

		if (output_limit < 0 || output_total < output_limit) {
			reached = false;

			if (!output_ready) {
				for (i = 0; i < alsa->output_handles_count; i++)
					output_counts[i] = 0;

				ret = alsa->output_callback(alsa->output_callback_data,
							    output_buffer,
							    output_trigger);
				if (ret < 0)
					goto complete;
			}

			for (i = 0; i < alsa->output_handles_count; i++) {
				handle = alsa->output_handles[i];

				ret = snd_pcm_avail(handle);
				if (output_counts[i] < output_trigger &&
				    ret > 0) {
					index = output_counts[i] *
						output_channels;
					count = output_trigger -
						output_counts[i];
					avail = snd_pcm_avail(handle);

					count = avail < count ? avail : count;

					ret = snd_pcm_writei(handle,
							     &output_buffer[index * bytes_per_sample],
							     count);
					if (ret < 0) {
						ret = snd_pcm_recover(handle,
								      ret, 0);
						if (ret < 0) {
							igt_debug("snd_pcm_recover after snd_pcm_writei failed");
							goto complete;
						}
					}

					output_counts[i] += ret;
				} else if (output_counts[i] < output_trigger &&
					   ret < 0) {
					ret = snd_pcm_recover(handle, ret, 0);
					if (ret < 0) {
						igt_debug("snd_pcm_recover failed");
						goto complete;
					}
				}
			}

			output_ready = false;

			for (i = 0; i < alsa->output_handles_count; i++)
				if (output_counts[i] < output_trigger)
					output_ready = true;

			if (!output_ready)
				output_total += output_trigger;

		}
	} while (!reached);

	ret = 0;

complete:
	free(output_buffer);

	return ret;
}
