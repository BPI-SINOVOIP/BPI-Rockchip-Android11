/*
 * Copyright © 2019 Intel Corporation
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
 * Authors: Simon Ser <simon.ser@intel.com>
 */

#ifndef IGT_INFOFRAME_H
#define IGT_INFOFRAME_H

#include "config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum infoframe_avi_rgb_ycbcr {
	INFOFRAME_AVI_RGB = 0,
	INFOFRAME_AVI_YCBCR422 = 1,
	INFOFRAME_AVI_YCBCR444 = 2,
	INFOFRAME_AVI_YCBCR420 = 3,
	INFOFRAME_AVI_IDO_DEFINED = 7,
};

enum infoframe_avi_scan {
	INFOFRAME_AVI_SCAN_UNSPECIFIED = 0,
	INFOFRAME_AVI_OVERSCAN = 1,
	INFOFRAME_AVI_UNDERSCAN = 2,
};

enum infoframe_avi_colorimetry {
	INFOFRAME_AVI_COLORIMETRY_UNSPECIFIED = 0,
	INFOFRAME_AVI_SMPTE_170M = 1,
	INFOFRAME_AVI_ITUR_BT709 = 2,
	INFOFRAME_AVI_COLORIMETRY_EXTENDED = 3,
};

enum infoframe_avi_picture_aspect_ratio {
	INFOFRAME_AVI_PIC_AR_UNSPECIFIED = 0,
	INFOFRAME_AVI_PIC_AR_4_3 = 1,
	INFOFRAME_AVI_PIC_AR_16_9 = 2,
};

enum infoframe_avi_active_aspect_ratio {
	INFOFRAME_AVI_ACT_AR_PIC = 8, /* same as picture aspect ratio */
	INFOFRAME_AVI_ACT_AR_4_3 = 9,
	INFOFRAME_AVI_ACT_AR_16_9 = 10,
	INFOFRAME_AVI_ACT_AR_14_9 = 11,
};

#define INFOFRAME_AVI_VIC_UNSPECIFIED 0

struct infoframe_avi {
	enum infoframe_avi_rgb_ycbcr rgb_ycbcr;
	enum infoframe_avi_scan scan;
	enum infoframe_avi_colorimetry colorimetry;
	enum infoframe_avi_picture_aspect_ratio picture_aspect_ratio;
	enum infoframe_avi_active_aspect_ratio active_aspect_ratio;
	uint8_t vic; /* Video Identification Code */
	/* TODO: remaining fields */
};

enum infoframe_audio_coding_type {
	INFOFRAME_AUDIO_CT_UNSPECIFIED = 0, /* refer to stream header */
	INFOFRAME_AUDIO_CT_PCM = 1, /* IEC 60958 PCM */
	INFOFRAME_AUDIO_CT_AC3 = 2,
	INFOFRAME_AUDIO_CT_MPEG1 = 3,
	INFOFRAME_AUDIO_CT_MP3 = 4,
	INFOFRAME_AUDIO_CT_MPEG2 = 5,
	INFOFRAME_AUDIO_CT_AAC = 6,
	INFOFRAME_AUDIO_CT_DTS = 7,
	INFOFRAME_AUDIO_CT_ATRAC = 8,
	INFOFRAME_AUDIO_CT_ONE_BIT = 9,
	INFOFRAME_AUDIO_CT_DOLBY = 10, /* Dolby Digital + */
	INFOFRAME_AUDIO_CT_DTS_HD = 11,
	INFOFRAME_AUDIO_CT_MAT = 12,
	INFOFRAME_AUDIO_CT_DST = 13,
	INFOFRAME_AUDIO_CT_WMA_PRO = 14,
};

struct infoframe_audio {
	enum infoframe_audio_coding_type coding_type;
	int channel_count; /* -1 if unspecified */
	int sampling_freq; /* in Hz, -1 if unspecified */
	int sample_size; /* in bits, -1 if unspecified */
	/* TODO: speaker allocation */
};

bool infoframe_avi_parse(struct infoframe_avi *infoframe, int version,
			 const uint8_t *buf, size_t buf_size);
bool infoframe_audio_parse(struct infoframe_audio *infoframe, int version,
			   const uint8_t *buf, size_t buf_size);

#endif
