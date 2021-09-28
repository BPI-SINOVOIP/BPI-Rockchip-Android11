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

#include <errno.h>
#include <fcntl.h>
#include <gsl/gsl_fft_real.h>
#include <math.h>
#include <unistd.h>

#include "igt_audio.h"
#include "igt_core.h"

#define FREQS_MAX 64
#define CHANNELS_MAX 8
#define SYNTHESIZE_AMPLITUDE 0.9
#define SYNTHESIZE_ACCURACY 0.2
/** MIN_FREQ: minimum frequency that audio_signal can generate.
 *
 * To make sure the audio signal doesn't contain noise, #audio_signal_detect
 * checks that low frequencies have a power lower than #NOISE_THRESHOLD.
 * However if too-low frequencies are generated, noise detection can fail.
 *
 * This value should be at least 100Hz plus one bin. Best is not to change this
 * value.
 */
#define MIN_FREQ 200 /* Hz */
#define NOISE_THRESHOLD 0.0005

/**
 * SECTION:igt_audio
 * @short_description: Library for audio-related tests
 * @title: Audio
 * @include: igt_audio.h
 *
 * This library contains helpers for audio-related tests. More specifically,
 * it allows generating additions of sine signals as well as detecting them.
 */

struct audio_signal_freq {
	int freq;
	int channel;

	double *period;
	size_t period_len;
	int offset;
};

struct audio_signal {
	int channels;
	int sampling_rate;

	struct audio_signal_freq freqs[FREQS_MAX];
	size_t freqs_count;
};

/**
 * audio_signal_init:
 * @channels: The number of channels to use for the signal
 * @sampling_rate: The sampling rate to use for the signal
 *
 * Allocate and initialize an audio signal structure with the given parameters.
 *
 * Returns: A newly-allocated audio signal structure
 */
struct audio_signal *audio_signal_init(int channels, int sampling_rate)
{
	struct audio_signal *signal;

	igt_assert(channels > 0);
	igt_assert(channels <= CHANNELS_MAX);

	signal = calloc(1, sizeof(struct audio_signal));
	signal->sampling_rate = sampling_rate;
	signal->channels = channels;
	return signal;
}

/**
 * audio_signal_add_frequency:
 * @signal: The target signal structure
 * @frequency: The frequency to add to the signal
 * @channel: The channel to add this frequency to, or -1 to add it to all
 * channels
 *
 * Add a frequency to the signal.
 *
 * Returns: An integer equal to zero for success and negative for failure
 */
int audio_signal_add_frequency(struct audio_signal *signal, int frequency,
				int channel)
{
	size_t index = signal->freqs_count;
	struct audio_signal_freq *freq;

	igt_assert(index < FREQS_MAX);
	igt_assert(channel < signal->channels);
	igt_assert(frequency >= MIN_FREQ);

	/* Stay within the Nyquist–Shannon sampling theorem. */
	if (frequency > signal->sampling_rate / 2) {
		igt_debug("Skipping frequency %d: too high for a %d Hz "
			  "sampling rate\n", frequency, signal->sampling_rate);
		return -1;
	}

	/* Clip the frequency to an integer multiple of the sampling rate.
	 * This to be able to store a full period of it and use that for
	 * signal generation, instead of recurrent calls to sin().
	 */
	frequency = signal->sampling_rate / (signal->sampling_rate / frequency);

	igt_debug("Adding test frequency %d to channel %d\n",
		  frequency, channel);

	freq = &signal->freqs[index];
	memset(freq, 0, sizeof(*freq));
	freq->freq = frequency;
	freq->channel = channel;

	signal->freqs_count++;

	return 0;
}

/**
 * audio_signal_synthesize:
 * @signal: The target signal structure
 *
 * Synthesize the data tables for the audio signal, that can later be used
 * to fill audio buffers. The resources allocated by this function must be
 * freed with a call to audio_signal_clean when the signal is no longer used.
 */
void audio_signal_synthesize(struct audio_signal *signal)
{
	double *period;
	double value;
	size_t period_len;
	int freq;
	int i, j;

	for (i = 0; i < signal->freqs_count; i++) {
		freq = signal->freqs[i].freq;
		period_len = signal->sampling_rate / freq;

		period = calloc(period_len, sizeof(double));

		for (j = 0; j < period_len; j++) {
			value = 2.0 * M_PI * freq / signal->sampling_rate * j;
			value = sin(value) * SYNTHESIZE_AMPLITUDE;

			period[j] = value;
		}

		signal->freqs[i].period = period;
		signal->freqs[i].period_len = period_len;
	}
}

/**
 * audio_signal_fini:
 *
 * Release the signal.
 */
void audio_signal_fini(struct audio_signal *signal)
{
	audio_signal_reset(signal);
	free(signal);
}

/**
 * audio_signal_reset:
 * @signal: The target signal structure
 *
 * Free the resources allocated by audio_signal_synthesize and remove
 * the previously-added frequencies.
 */
void audio_signal_reset(struct audio_signal *signal)
{
	size_t i;

	for (i = 0; i < signal->freqs_count; i++) {
		free(signal->freqs[i].period);
	}

	signal->freqs_count = 0;
}

static size_t audio_signal_count_freqs(struct audio_signal *signal, int channel)
{
	size_t n, i;
	struct audio_signal_freq *freq;

	n = 0;
	for (i = 0; i < signal->freqs_count; i++) {
		freq = &signal->freqs[i];
		if (freq->channel < 0 || freq->channel == channel)
			n++;
	}

	return n;
}

/** audio_sanity_check:
 *
 * Make sure our generated signal is not messed up. In particular, make sure
 * the maximum reaches a reasonable value but doesn't exceed our
 * SYNTHESIZE_AMPLITUDE limit. Same for the minimum.
 *
 * We want the signal to be powerful enough to be able to hear something. We
 * want the signal not to reach 1.0 so that we're sure it won't get capped by
 * the audio card or the receiver.
 */
static void audio_sanity_check(double *samples, size_t samples_len)
{
	size_t i;
	double min = 0, max = 0;

	for (i = 0; i < samples_len; i++) {
		if (samples[i] < min)
			min = samples[i];
		if (samples[i] > max)
			max = samples[i];
	}

	igt_assert(-SYNTHESIZE_AMPLITUDE <= min);
	igt_assert(min <= -SYNTHESIZE_AMPLITUDE + SYNTHESIZE_ACCURACY);
	igt_assert(SYNTHESIZE_AMPLITUDE - SYNTHESIZE_ACCURACY <= max);
	igt_assert(max <= SYNTHESIZE_AMPLITUDE);
}

/**
 * audio_signal_fill:
 * @signal: The target signal structure
 * @buffer: The target buffer to fill
 * @samples: The number of samples to fill
 *
 * Fill the requested number of samples to the target buffer with the audio
 * signal data (in interleaved double format), at the requested sampling rate
 * and number of channels.
 *
 * Each sample is normalized (ie. between 0 and 1).
 */
void audio_signal_fill(struct audio_signal *signal, double *buffer,
		       size_t samples)
{
	double *dst, *src;
	struct audio_signal_freq *freq;
	int total;
	int count;
	int i, j, k;
	size_t freqs_per_channel[CHANNELS_MAX];

	memset(buffer, 0, sizeof(double) * signal->channels * samples);

	for (i = 0; i < signal->channels; i++) {
		freqs_per_channel[i] = audio_signal_count_freqs(signal, i);
		igt_assert(freqs_per_channel[i] > 0);
	}

	for (i = 0; i < signal->freqs_count; i++) {
		freq = &signal->freqs[i];
		total = 0;

		igt_assert(freq->period);

		while (total < samples) {
			src = freq->period + freq->offset;
			dst = buffer + total * signal->channels;

			count = freq->period_len - freq->offset;
			if (count > samples - total)
				count = samples - total;

			freq->offset += count;
			freq->offset %= freq->period_len;

			for (j = 0; j < count; j++) {
				for (k = 0; k < signal->channels; k++) {
					if (freq->channel >= 0 &&
					    freq->channel != k)
						continue;
					dst[j * signal->channels + k] +=
						src[j] / freqs_per_channel[k];
				}
			}

			total += count;
		}
	}

	audio_sanity_check(buffer, signal->channels * samples);
}

/* See https://en.wikipedia.org/wiki/Window_function#Hann_and_Hamming_windows */
static double hann_window(double v, size_t i, size_t N)
{
	return v * 0.5 * (1 - cos(2.0 * M_PI * (double) i / (double) N));
}

/**
 * Checks that frequencies specified in signal, and only those, are included
 * in the input data.
 *
 * sampling_rate is given in Hz. samples_len is the number of elements in
 * samples.
 */
bool audio_signal_detect(struct audio_signal *signal, int sampling_rate,
			 int channel, const double *samples, size_t samples_len)
{
	double *data;
	size_t data_len = samples_len;
	size_t bin_power_len = data_len / 2 + 1;
	double bin_power[bin_power_len];
	bool detected[FREQS_MAX];
	int ret, freq_accuracy, freq, local_max_freq;
	double max, local_max, threshold;
	size_t i, j;
	bool above, success;

	/* gsl will mutate the array in-place, so make a copy */
	data = malloc(samples_len * sizeof(double));
	memcpy(data, samples, samples_len * sizeof(double));

	/* Apply a Hann window to the input signal, to reduce frequency leaks
	 * due to the endpoints of the signal being discontinuous.
	 *
	 * For more info:
	 * - https://download.ni.com/evaluation/pxi/Understanding%20FFTs%20and%20Windowing.pdf
	 * - https://en.wikipedia.org/wiki/Window_function
	 */
	for (i = 0; i < data_len; i++)
		data[i] = hann_window(data[i], i, data_len);

	/* Allowed error in Hz due to FFT step */
	freq_accuracy = sampling_rate / data_len;
	igt_debug("Allowed freq. error: %d Hz\n", freq_accuracy);

	ret = gsl_fft_real_radix2_transform(data, 1, data_len);
	if (ret != 0) {
		free(data);
		igt_assert(0);
	}

	/* Compute the power received by every bin of the FFT.
	 *
	 * For i < data_len / 2, the real part of the i-th term is stored at
	 * data[i] and its imaginary part is stored at data[data_len - i].
	 * i = 0 and i = data_len / 2 are special cases, they are purely real
	 * so their imaginary part isn't stored.
	 *
	 * The power is encoded as the magnitude of the complex number and the
	 * phase is encoded as its angle.
	 */
	bin_power[0] = data[0];
	for (i = 1; i < bin_power_len - 1; i++) {
		bin_power[i] = hypot(data[i], data[data_len - i]);
	}
	bin_power[bin_power_len - 1] = data[data_len / 2];

	/* Normalize the power */
	for (i = 0; i < bin_power_len; i++)
		bin_power[i] = 2 * bin_power[i] / data_len;

	/* Detect noise with a threshold on the power of low frequencies */
	for (i = 0; i < bin_power_len; i++) {
		freq = sampling_rate * i / data_len;
		if (freq > MIN_FREQ - 100)
			break;
		if (bin_power[i] > NOISE_THRESHOLD) {
			igt_debug("Noise level too high: freq=%d power=%f\n",
				  freq, bin_power[i]);
			return false;
		}
	}

	/* Record the maximum power received as a way to normalize all the
	 * others. */
	max = NAN;
	for (i = 0; i < bin_power_len; i++) {
		if (isnan(max) || bin_power[i] > max)
			max = bin_power[i];
	}

	for (i = 0; i < signal->freqs_count; i++)
		detected[i] = false;

	/* Do a linear search through the FFT bins' power to find the the local
	 * maximums that exceed half of the absolute maximum that we previously
	 * calculated.
	 *
	 * Since the frequencies might not be perfectly aligned with the bins of
	 * the FFT, we need to find the local maximum across some consecutive
	 * bins. Once the power returns under the power threshold, we compare
	 * the frequency of the bin that received the maximum power to the
	 * expected frequencies. If found, we mark this frequency as such,
	 * otherwise we warn that an unexpected frequency was found.
	 */
	threshold = max / 2;
	success = true;
	above = false;
	local_max = 0;
	local_max_freq = -1;
	for (i = 0; i < bin_power_len; i++) {
		freq = sampling_rate * i / data_len;

		if (bin_power[i] > threshold)
			above = true;

		if (!above) {
			continue;
		}

		/* If we were above the threshold and we're not anymore, it's
		 * time to decide whether the peak frequency is correct or
		 * invalid. */
		if (bin_power[i] < threshold) {
			for (j = 0; j < signal->freqs_count; j++) {
				if (signal->freqs[j].channel >= 0 &&
				    signal->freqs[j].channel != channel)
					continue;

				if (signal->freqs[j].freq >
				    local_max_freq - freq_accuracy &&
				    signal->freqs[j].freq <
				    local_max_freq + freq_accuracy) {
					detected[j] = true;
					igt_debug("Frequency %d detected\n",
						  local_max_freq);
					break;
				}
			}

			/* We haven't generated this frequency, but we detected
			 * it. */
			if (j == signal->freqs_count) {
				igt_debug("Detected additional frequency: %d\n",
					  local_max_freq);
				success = false;
			}

			above = false;
			local_max = 0;
			local_max_freq = -1;
		}

		if (bin_power[i] > local_max) {
			local_max = bin_power[i];
			local_max_freq = freq;
		}
	}

	/* Check that all frequencies we generated have been detected. */
	for (i = 0; i < signal->freqs_count; i++) {
		if (signal->freqs[i].channel >= 0 &&
		    signal->freqs[i].channel != channel)
			continue;

		if (!detected[i]) {
			igt_debug("Missing frequency: %d\n",
				  signal->freqs[i].freq);
			success = false;
		}
	}

	free(data);

	return success;
}

/**
 * audio_extract_channel_s32_le: extracts a single channel from a multi-channel
 * S32_LE input buffer.
 *
 * If dst_cap is zero, no copy is performed. This can be used to compute the
 * minimum required capacity.
 *
 * Returns: the number of samples extracted.
 */
size_t audio_extract_channel_s32_le(double *dst, size_t dst_cap,
				    int32_t *src, size_t src_len,
				    int n_channels, int channel)
{
	size_t dst_len, i;

	igt_assert(channel < n_channels);
	igt_assert(src_len % n_channels == 0);
	dst_len = src_len / n_channels;
	if (dst_cap == 0)
		return dst_len;

	igt_assert(dst_len <= dst_cap);
	for (i = 0; i < dst_len; i++)
		dst[i] = (double) src[i * n_channels + channel] / INT32_MAX;

	return dst_len;
}

static void audio_convert_to_s16_le(int16_t *dst, double *src, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i)
		dst[i] = INT16_MAX * src[i];
}

static void audio_convert_to_s24_le(int32_t *dst, double *src, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i)
		dst[i] = 0x7FFFFF * src[i];
}

static void audio_convert_to_s32_le(int32_t *dst, double *src, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i)
		dst[i] = INT32_MAX * src[i];
}

void audio_convert_to(void *dst, double *src, size_t len,
		      snd_pcm_format_t format)
{
	switch (format) {
	case SND_PCM_FORMAT_S16_LE:
		audio_convert_to_s16_le(dst, src, len);
		break;
	case SND_PCM_FORMAT_S24_LE:
		audio_convert_to_s24_le(dst, src, len);
		break;
	case SND_PCM_FORMAT_S32_LE:
		audio_convert_to_s32_le(dst, src, len);
		break;
	default:
		assert(false); /* unreachable */
	}
}

#define RIFF_TAG "RIFF"
#define WAVE_TAG "WAVE"
#define FMT_TAG "fmt "
#define DATA_TAG "data"

static void
append_to_buffer(char *dst, size_t *i, const void *src, size_t src_size)
{
	memcpy(&dst[*i], src, src_size);
	*i += src_size;
}

/**
 * audio_create_wav_file_s32_le:
 * @qualifier: the basename of the file (the test name will be prepended, and
 * the file extension will be appended)
 * @sample_rate: the sample rate in Hz
 * @channels: the number of channels
 * @path: if non-NULL, will be set to a pointer to the new file path (the
 * caller is responsible for free-ing it)
 *
 * Creates a new WAV file.
 *
 * After calling this function, the caller is expected to write S32_LE PCM data
 * to the returned file descriptor.
 *
 * See http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html for
 * a WAV file format specification.
 *
 * Returns: a file descriptor to the newly created file, or -1 on error.
 */
int audio_create_wav_file_s32_le(const char *qualifier, uint32_t sample_rate,
				 uint16_t channels, char **path)
{
	char _path[PATH_MAX];
	const char *test_name, *subtest_name;
	int fd;
	char header[44];
	size_t i = 0;
	uint32_t file_size, chunk_size, byte_rate;
	uint16_t format, block_align, bits_per_sample;

	test_name = igt_test_name();
	subtest_name = igt_subtest_name();

	igt_assert(igt_frame_dump_path);
	snprintf(_path, sizeof(_path), "%s/audio-%s-%s-%s.wav",
		 igt_frame_dump_path, test_name, subtest_name, qualifier);

	if (path)
		*path = strdup(_path);

	igt_debug("Dumping %s audio to %s\n", qualifier, _path);
	fd = open(_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		igt_warn("open failed: %s\n", strerror(errno));
		return -1;
	}

	/* File header */
	file_size = UINT32_MAX; /* unknown file size */
	append_to_buffer(header, &i, RIFF_TAG, strlen(RIFF_TAG));
	append_to_buffer(header, &i, &file_size, sizeof(file_size));
	append_to_buffer(header, &i, WAVE_TAG, strlen(WAVE_TAG));

	/* Format chunk */
	chunk_size = 16;
	format = 1; /* PCM */
	bits_per_sample = 32; /* S32_LE */
	byte_rate = sample_rate * channels * bits_per_sample / 8;
	block_align = channels * bits_per_sample / 8;
	append_to_buffer(header, &i, FMT_TAG, strlen(FMT_TAG));
	append_to_buffer(header, &i, &chunk_size, sizeof(chunk_size));
	append_to_buffer(header, &i, &format, sizeof(format));
	append_to_buffer(header, &i, &channels, sizeof(channels));
	append_to_buffer(header, &i, &sample_rate, sizeof(sample_rate));
	append_to_buffer(header, &i, &byte_rate, sizeof(byte_rate));
	append_to_buffer(header, &i, &block_align, sizeof(block_align));
	append_to_buffer(header, &i, &bits_per_sample, sizeof(bits_per_sample));

	/* Data chunk */
	chunk_size = UINT32_MAX; /* unknown chunk size */
	append_to_buffer(header, &i, DATA_TAG, strlen(DATA_TAG));
	append_to_buffer(header, &i, &chunk_size, sizeof(chunk_size));

	igt_assert(i == sizeof(header));

	if (write(fd, header, sizeof(header)) != sizeof(header)) {
		igt_warn("write failed: %s'n", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}
