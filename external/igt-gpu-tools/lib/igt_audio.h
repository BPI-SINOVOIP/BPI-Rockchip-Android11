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

#ifndef IGT_AUDIO_H
#define IGT_AUDIO_H

#include "config.h"

#include <stdbool.h>
#include <stdint.h>

#include <alsa/asoundlib.h>

struct audio_signal;

struct audio_signal *audio_signal_init(int channels, int sampling_rate);
void audio_signal_fini(struct audio_signal *signal);
int audio_signal_add_frequency(struct audio_signal *signal, int frequency,
			       int channel);
void audio_signal_synthesize(struct audio_signal *signal);
void audio_signal_reset(struct audio_signal *signal);
void audio_signal_fill(struct audio_signal *signal, double *buffer,
		       size_t samples);
bool audio_signal_detect(struct audio_signal *signal, int sampling_rate,
			 int channel, const double *samples, size_t samples_len);
size_t audio_extract_channel_s32_le(double *dst, size_t dst_cap,
				    int32_t *src, size_t src_len,
				    int n_channels, int channel);
void audio_convert_to(void *dst, double *src, size_t len,
		      snd_pcm_format_t format);
int audio_create_wav_file_s32_le(const char *qualifier, uint32_t sample_rate,
				 uint16_t channels, char **path);

#endif
