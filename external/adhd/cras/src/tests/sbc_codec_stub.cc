/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stdlib.h>

extern "C" {
#include "cras_audio_codec.h"
#include "sbc_codec_stub.h"
}

namespace {

static int create_fail;
static int create_called;
static int msbc_create_called;
static int destroy_called;
static uint8_t freq_val;
static uint8_t mode_val;
static uint8_t subbands_val;
static uint8_t alloc_val;
static uint8_t blocks_val;
static uint8_t bitpool_val;
static struct cras_audio_codec* sbc_codec;
static size_t decode_out_decoded_return_val;
static int decode_fail;
static size_t encode_out_encoded_return_val;
static int encode_fail;
static int cras_sbc_get_frame_length_val;
static int cras_sbc_get_codesize_val;

};  // namespace

void sbc_codec_stub_reset() {
  create_fail = 0;
  create_called = 0;
  msbc_create_called = 0;
  destroy_called = 0;
  freq_val = 0;
  mode_val = 0;
  subbands_val = 0;
  alloc_val = 0;
  blocks_val = 0;
  bitpool_val = 0;

  sbc_codec = NULL;
  decode_out_decoded_return_val = 0;
  encode_out_encoded_return_val = 0;
  decode_fail = 0;
  encode_fail = 0;

  cras_sbc_get_frame_length_val = 5;
  cras_sbc_get_codesize_val = 5;
}

void set_sbc_codec_create_fail(int fail) {
  create_fail = fail;
}

int get_sbc_codec_create_called() {
  return create_called;
}

int get_msbc_codec_create_called() {
  return msbc_create_called;
}

uint8_t get_sbc_codec_create_freq_val() {
  return freq_val;
}

uint8_t get_sbc_codec_create_mode_val() {
  return mode_val;
}

uint8_t get_sbc_codec_create_subbands_val() {
  return subbands_val;
}

uint8_t get_sbc_codec_create_alloc_val() {
  return alloc_val;
}

uint8_t get_sbc_codec_create_blocks_val() {
  return blocks_val;
}

uint8_t get_sbc_codec_create_bitpool_val() {
  return bitpool_val;
}

int get_sbc_codec_destroy_called() {
  return destroy_called;
}

void set_sbc_codec_decoded_out(size_t ret) {
  decode_out_decoded_return_val = ret;
}

void set_sbc_codec_decoded_fail(int fail) {
  decode_fail = fail;
}

void set_sbc_codec_encoded_out(size_t ret) {
  encode_out_encoded_return_val = ret;
}

void set_sbc_codec_encoded_fail(int fail) {
  encode_fail = fail;
}

int decode(struct cras_audio_codec* codec,
           const void* input,
           size_t input_len,
           void* output,
           size_t output_len,
           size_t* count) {
  *count = decode_out_decoded_return_val;
  return decode_fail ? -1 : input_len;
}

int encode(struct cras_audio_codec* codec,
           const void* input,
           size_t input_len,
           void* output,
           size_t output_len,
           size_t* count) {
  // Written half the output buffer.
  *count = encode_out_encoded_return_val;
  return encode_fail ? -1 : input_len;
}

struct cras_audio_codec* cras_sbc_codec_create(uint8_t freq,
                                               uint8_t mode,
                                               uint8_t subbands,
                                               uint8_t alloc,
                                               uint8_t blocks,
                                               uint8_t bitpool) {
  if (!create_fail) {
    sbc_codec = (struct cras_audio_codec*)calloc(1, sizeof(*sbc_codec));
    sbc_codec->decode = decode;
    sbc_codec->encode = encode;
  }

  create_called++;
  freq_val = freq;
  mode_val = mode;
  subbands_val = subbands;
  alloc_val = alloc;
  blocks_val = blocks;
  bitpool_val = bitpool;
  return sbc_codec;
}

struct cras_audio_codec* cras_msbc_codec_create() {
  msbc_create_called++;
  sbc_codec = (struct cras_audio_codec*)calloc(1, sizeof(*sbc_codec));
  sbc_codec->decode = decode;
  sbc_codec->encode = encode;
  return sbc_codec;
}

void cras_sbc_codec_destroy(struct cras_audio_codec* codec) {
  destroy_called++;
  free(codec);
}

int cras_sbc_get_codesize(struct cras_audio_codec* codec) {
  return cras_sbc_get_codesize_val;
}

int cras_sbc_get_frame_length(struct cras_audio_codec* codec) {
  return cras_sbc_get_frame_length_val;
}
