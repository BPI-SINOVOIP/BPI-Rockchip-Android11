/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unordered_map>

extern "C" {
#include "cras_rstream.h"
}

namespace {

struct cb_data {
  std::unordered_map<unsigned int, unsigned int> dev_offset;
  int pending_reply;
};
std::unordered_map<const cras_rstream*, cb_data> data_map;

};  // namespace

void rstream_stub_reset() {
  data_map.clear();
}

void rstream_stub_dev_offset(const cras_rstream* rstream,
                             unsigned int dev_id,
                             unsigned int offset) {
  auto data = data_map.find(rstream);
  if (data == data_map.end()) {
    cb_data new_data;
    new_data.dev_offset[dev_id] = offset;
    data_map.insert({rstream, new_data});
  } else {
    data->second.dev_offset[dev_id] = offset;
  }
}

void rstream_stub_pending_reply(const cras_rstream* rstream, int ret_value) {
  auto data = data_map.find(rstream);
  if (data == data_map.end()) {
    cb_data new_data;
    new_data.pending_reply = ret_value;
    data_map.insert({rstream, new_data});
  } else {
    data->second.pending_reply = ret_value;
  }
}

extern "C" {

void cras_rstream_record_fetch_interval(struct cras_rstream* rstream,
                                        const struct timespec* now) {}

void cras_rstream_dev_attach(struct cras_rstream* rstream,
                             unsigned int dev_id,
                             void* dev_ptr) {}

void cras_rstream_dev_detach(struct cras_rstream* rstream,
                             unsigned int dev_id) {}

unsigned int cras_rstream_dev_offset(const struct cras_rstream* rstream,
                                     unsigned int dev_id) {
  auto elem = data_map.find(rstream);
  if (elem != data_map.end())
    return elem->second.dev_offset[dev_id];
  return 0;
}

void cras_rstream_dev_offset_update(struct cras_rstream* rstream,
                                    unsigned int frames,
                                    unsigned int dev_id) {}

unsigned int cras_rstream_playable_frames(struct cras_rstream* rstream,
                                          unsigned int dev_id) {
  return 0;
}

float cras_rstream_get_volume_scaler(struct cras_rstream* rstream) {
  return 1.0;
}

int cras_rstream_get_mute(const struct cras_rstream* rstream) {
  return 0;
}

uint8_t* cras_rstream_get_readable_frames(struct cras_rstream* rstream,
                                          unsigned int offset,
                                          size_t* frames) {
  return NULL;
}

void cras_rstream_update_input_write_pointer(struct cras_rstream* rstream) {}

void cras_rstream_update_output_read_pointer(struct cras_rstream* rstream) {}

int cras_rstream_audio_ready(struct cras_rstream* stream, size_t count) {
  cras_shm_buffer_write_complete(stream->shm);
  return 0;
}

int cras_rstream_request_audio(struct cras_rstream* stream,
                               const struct timespec* now) {
  return 0;
}

void cras_rstream_update_queued_frames(struct cras_rstream* rstream) {}

int cras_rstream_is_pending_reply(const struct cras_rstream* rstream) {
  auto elem = data_map.find(rstream);
  if (elem != data_map.end())
    return elem->second.pending_reply;
  return 0;
}

int cras_rstream_flush_old_audio_messages(struct cras_rstream* rstream) {
  return 0;
}

}  // extern "C"
