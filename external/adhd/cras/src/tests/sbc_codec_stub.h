/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SBC_CODEC_STUB_H_
#define SBC_CODEC_STUB_H_

#include <stdint.h>
#include <stdlib.h>

void sbc_codec_stub_reset();
void set_sbc_codec_create_fail(int fail);
int get_sbc_codec_create_called();
int get_msbc_codec_create_called();
uint8_t get_sbc_codec_create_freq_val();
uint8_t get_sbc_codec_create_mode_val();
uint8_t get_sbc_codec_create_subbands_val();
uint8_t get_sbc_codec_create_alloc_val();
uint8_t get_sbc_codec_create_blocks_val();
uint8_t get_sbc_codec_create_bitpool_val();
int get_sbc_codec_destroy_called();
void set_sbc_codec_decoded_out(size_t ret);
void set_sbc_codec_decoded_fail(int fail);
void set_sbc_codec_encoded_out(size_t ret);
void set_sbc_codec_encoded_fail(int fail);

struct cras_audio_codec* cras_sbc_codec_create(uint8_t freq,
                                               uint8_t mode,
                                               uint8_t subbands,
                                               uint8_t alloc,
                                               uint8_t blocks,
                                               uint8_t bitpool);
struct cras_audio_codec* cras_msbc_codec_create();
void cras_sbc_codec_destroy(struct cras_audio_codec* codec);
int cras_sbc_get_codesize(struct cras_audio_codec* codec);
int cras_sbc_get_frame_length(struct cras_audio_codec* codec);

#endif  // SBC_CODEC_STUB_H_
