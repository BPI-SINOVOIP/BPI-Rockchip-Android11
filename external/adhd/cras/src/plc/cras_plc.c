/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cras_plc.h"

#define MSBC_SAMPLE_SIZE 2 /* 2 bytes*/
#define MSBC_PKT_LEN 57 /* Packet length without the header */
#define MSBC_FS 120 /* Frame Size */
#define MSBC_CODE_SIZE 240 /* MSBC_SAMPLE_SIZE * MSBC_FS */

#define PLC_WL 256 /* 16ms - Window Length for pattern matching */
#define PLC_TL 64 /* 4ms - Template Length for matching */
#define PLC_HL (PLC_WL + MSBC_FS - 1) /* Length of History buffer required */
#define PLC_SBCRL 36 /* SBC Reconvergence sample Length */
#define PLC_OLAL 16 /* OverLap-Add Length */

/* The pre-computed zero input bit stream of mSBC codec, per HFP 1.7 spec.
 * This mSBC frame will be decoded into all-zero input PCM. */
static const uint8_t msbc_zero_frame[] = {
	0xad, 0x00, 0x00, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x77, 0x6d, 0xb6, 0xdd,
	0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d, 0xb6,
	0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d,
	0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77,
	0x6d, 0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6c
};

/* Raised Cosine table for OLA */
static const float rcos[PLC_OLAL] = { 0.99148655f, 0.96623611f, 0.92510857f,
				      0.86950446f, 0.80131732f, 0.72286918f,
				      0.63683150f, 0.54613418f, 0.45386582f,
				      0.36316850f, 0.27713082f, 0.19868268f,
				      0.13049554f, 0.07489143f, 0.03376389f,
				      0.00851345f };

/* The PLC is specifically designed for mSBC. The algorithm searches the
 * history of receiving samples to find the best match samples and constructs
 * substitutions for the lost samples. The selection is based on pattern
 * matching a template, composed of a length of samples preceding to the lost
 * samples. It then uses the following samples after the best match as the
 * replacement samples and applies Overlap-Add to reduce the audible
 * distortion.
 *
 * This structure holds related info needed to conduct the PLC algorithm.
 * Members:
 *    hist - The history buffer for receiving samples, we also use it to
 *           buffer the processed replacement samples.
 *    best_lag - The index of the best substitution samples in sample history.
 *    handled_bad_frames - Number of bad frames handled since the last good
 *                         frame.
 *    zero_frame - A buffer used for storing the samples from decoding the
 *                 mSBC zero frame packet.
 */
struct cras_msbc_plc {
	int16_t hist[PLC_HL + MSBC_FS + PLC_SBCRL + PLC_OLAL];
	unsigned int best_lag;
	int handled_bad_frames;
	int16_t zero_frame[MSBC_FS];
};

struct cras_msbc_plc *cras_msbc_plc_create()
{
	struct cras_msbc_plc *plc =
		(struct cras_msbc_plc *)calloc(1, sizeof(*plc));
	return plc;
}

void cras_msbc_plc_destroy(struct cras_msbc_plc *plc)
{
	free(plc);
}

static int16_t f_to_s16(float input)
{
	return input > INT16_MAX ?
		       INT16_MAX :
		       input < INT16_MIN ? INT16_MIN : (int16_t)input;
}

void overlap_add(int16_t *output, float scaler_d, const int16_t *desc,
		 float scaler_a, const int16_t *asc)
{
	for (int i = 0; i < PLC_OLAL; i++) {
		output[i] =
			f_to_s16(scaler_d * desc[i] * rcos[i] +
				 scaler_a * asc[i] * rcos[PLC_OLAL - 1 - i]);
	}
}

int cras_msbc_plc_handle_good_frames(struct cras_msbc_plc *state,
				     const uint8_t *input, uint8_t *output)
{
	int16_t *frame_head, *input_samples, *output_samples;
	if (state->handled_bad_frames == 0) {
		/* If there was no packet loss before this good frame, there
		 * is nothing we need to do to the frame so we'll just pass
		 * the input to output.
		 */
		memmove(output, input, MSBC_FS * MSBC_SAMPLE_SIZE);
	} else {
		frame_head = &state->hist[PLC_HL];
		input_samples = (int16_t *)input;
		output_samples = (int16_t *)output;

		/* For the first good frame after packet loss, we need to
		 * conceal the received samples to have it reconverge with the
		 * true output.
		 */
		memcpy(output_samples, frame_head,
		       PLC_SBCRL * MSBC_SAMPLE_SIZE);
		overlap_add(&output_samples[PLC_SBCRL], 1.0,
			    &frame_head[PLC_SBCRL], 1.0,
			    &input_samples[PLC_SBCRL]);
		memmove(&output_samples[PLC_SBCRL + PLC_OLAL],
			&input_samples[PLC_SBCRL + PLC_OLAL],
			(MSBC_FS - PLC_SBCRL - PLC_OLAL) * MSBC_SAMPLE_SIZE);
		state->handled_bad_frames = 0;
	}

	/* Shift the history and update the good frame to the end of it. */
	memmove(state->hist, &state->hist[MSBC_FS],
		(PLC_HL - MSBC_FS) * MSBC_SAMPLE_SIZE);
	memcpy(&state->hist[PLC_HL - MSBC_FS], output,
	       MSBC_FS * MSBC_SAMPLE_SIZE);
	return MSBC_CODE_SIZE;
}

float cross_correlation(int16_t *x, int16_t *y)
{
	float sum = 0, x2 = 0, y2 = 0;

	for (int i = 0; i < PLC_TL; i++) {
		sum += ((float)x[i]) * y[i];
		x2 += ((float)x[i]) * x[i];
		y2 += ((float)y[i]) * y[i];
	}
	return sum / sqrt(x2 * y2);
}

int pattern_match(int16_t *hist)
{
	int best = 0;
	float cn, max_cn = FLT_MIN;

	for (int i = 0; i < PLC_WL; i++) {
		cn = cross_correlation(&hist[PLC_HL - PLC_TL], &hist[i]);
		if (cn > max_cn) {
			best = i;
			max_cn = cn;
		}
	}
	return best;
}

float amplitude_match(int16_t *x, int16_t *y)
{
	uint32_t sum_x = 0, sum_y = 0;
	float scaler;
	for (int i = 0; i < MSBC_FS; i++) {
		sum_x += abs(x[i]);
		sum_y += abs(y[i]);
	}

	if (sum_y == 0)
		return 1.2f;

	scaler = (float)sum_x / sum_y;
	return scaler > 1.2f ? 1.2f : scaler < 0.75f ? 0.75f : scaler;
}

int cras_msbc_plc_handle_bad_frames(struct cras_msbc_plc *state,
				    struct cras_audio_codec *codec,
				    uint8_t *output)
{
	float scaler;
	int16_t *best_match_hist;
	int16_t *frame_head = &state->hist[PLC_HL];
	size_t pcm_decoded = 0;

	codec->decode(codec, msbc_zero_frame, MSBC_PKT_LEN, state->zero_frame,
		      MSBC_FS, &pcm_decoded);

	if (state->handled_bad_frames == 0) {
		/* Finds the best matching samples and amplitude */
		state->best_lag = pattern_match(state->hist) + PLC_TL;
		best_match_hist = &state->hist[state->best_lag];
		scaler = amplitude_match(&state->hist[PLC_HL - MSBC_FS],
					 best_match_hist);

		/* Constructs the substitution samples */
		overlap_add(frame_head, 1.0, state->zero_frame, scaler,
			    best_match_hist);
		for (int i = PLC_OLAL; i < MSBC_FS; i++)
			state->hist[PLC_HL + i] =
				f_to_s16(scaler * best_match_hist[i]);
		overlap_add(&frame_head[MSBC_FS], scaler,
			    &best_match_hist[MSBC_FS], 1.0,
			    &best_match_hist[MSBC_FS]);

		memmove(&frame_head[MSBC_FS + PLC_OLAL],
			&best_match_hist[MSBC_FS + PLC_OLAL],
			PLC_SBCRL * MSBC_SAMPLE_SIZE);
	} else {
		memmove(frame_head, &state->hist[state->best_lag],
			(MSBC_FS + PLC_SBCRL + PLC_OLAL) * MSBC_SAMPLE_SIZE);
	}
	state->handled_bad_frames++;

	memcpy(output, frame_head, MSBC_CODE_SIZE);
	memmove(state->hist, &state->hist[MSBC_FS],
		(PLC_HL + PLC_SBCRL + PLC_OLAL) * MSBC_SAMPLE_SIZE);
	return MSBC_CODE_SIZE;
}
