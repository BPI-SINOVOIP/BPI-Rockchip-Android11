/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_FMT_CONV_OPS_H_
#define CRAS_FMT_CONV_OPS_H_

#include <sys/types.h>
#include "cras_audio_format.h"

/*
 * Format converter.
 */
void convert_u8_to_s16le(const uint8_t *in, size_t in_samples, uint8_t *out);
void convert_s243le_to_s16le(const uint8_t *in, size_t in_samples,
			     uint8_t *out);
void convert_s24le_to_s16le(const uint8_t *in, size_t in_samples, uint8_t *out);
void convert_s32le_to_s16le(const uint8_t *in, size_t in_samples, uint8_t *out);
void convert_s16le_to_u8(const uint8_t *in, size_t in_samples, uint8_t *out);
void convert_s16le_to_s243le(const uint8_t *in, size_t in_samples,
			     uint8_t *out);
void convert_s16le_to_s24le(const uint8_t *in, size_t in_samples, uint8_t *out);
void convert_s16le_to_s32le(const uint8_t *in, size_t in_samples, uint8_t *out);

/*
 * Channel converter: mono to stereo.
 */
size_t s16_mono_to_stereo(const uint8_t *in, size_t in_frames, uint8_t *out);

/*
 * Channel converter: stereo to mono.
 */
size_t s16_stereo_to_mono(const uint8_t *in, size_t in_frames, uint8_t *out);

/*
 * Channel converter: mono to 5.1 surround.
 */
size_t s16_mono_to_51(size_t left, size_t right, size_t center,
		      const uint8_t *in, size_t in_frames, uint8_t *out);

/*
 * Channel converter: stereo to 5.1 surround.
 */
size_t s16_stereo_to_51(size_t left, size_t right, size_t center,
			const uint8_t *in, size_t in_frames, uint8_t *out);

/*
 * Channel converter: 5.1 surround to stereo.
 */
size_t s16_51_to_stereo(const uint8_t *in, size_t in_frames, uint8_t *out);

/*
 * Channel converter: stereo to quad (front L/R, rear L/R).
 */
size_t s16_stereo_to_quad(size_t front_left, size_t front_right,
			  size_t rear_left, size_t rear_right,
			  const uint8_t *in, size_t in_frames, uint8_t *out);

/*
 * Channel converter: quad (front L/R, rear L/R) to stereo.
 */
size_t s16_quad_to_stereo(size_t front_left, size_t front_right,
			  size_t rear_left, size_t rear_right,
			  const uint8_t *in, size_t in_frames, uint8_t *out);

/*
 * Channel converter: N channels to M channels.
 */
size_t s16_default_all_to_all(struct cras_audio_format *out_fmt,
			      size_t num_in_ch, size_t num_out_ch,
			      const uint8_t *in, size_t in_frames,
			      uint8_t *out);

/*
 * Multiplies buffer vector with coefficient vector.
 */
int16_t s16_multiply_buf_with_coef(float *coef, const int16_t *buf,
				   size_t size);

/*
 * Channel layout converter.
 */
size_t s16_convert_channels(float **ch_conv_mtx, size_t num_in_ch,
			    size_t num_out_ch, const uint8_t *in,
			    size_t in_frames, uint8_t *out);

#endif /* CRAS_FMT_CONV_OPS_H_ */
