// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <algorithm>
#include <memory>

extern "C" {
#include "cras_iodev.h"
#include "cras_rstream.h"
#include "cras_shm.h"
#include "cras_types.h"
#include "dev_stream.h"
#include "utlist.h"
}

#include "dev_io_stubs.h"

ShmPtr create_shm(size_t cb_threshold) {
  uint32_t frame_bytes = 4;
  uint32_t used_size = cb_threshold * 2 * frame_bytes;

  ShmPtr shm(reinterpret_cast<struct cras_audio_shm*>(
                 calloc(1, sizeof(struct cras_audio_shm))),
             destroy_shm);

  shm->header = reinterpret_cast<struct cras_audio_shm_header*>(
      calloc(1, sizeof(struct cras_audio_shm_header)));
  shm->header->config.used_size = used_size;
  shm->header->config.frame_bytes = frame_bytes;
  shm->config = shm->header->config;

  uint32_t samples_size = cras_shm_calculate_samples_size(used_size);
  shm->samples = reinterpret_cast<uint8_t*>(calloc(1, samples_size));
  shm->samples_info.length = samples_size;
  return shm;
}

void destroy_shm(struct cras_audio_shm* shm) {
  free(shm->header);
  free(shm->samples);
  free(shm);
}

RstreamPtr create_rstream(cras_stream_id_t id,
                          CRAS_STREAM_DIRECTION direction,
                          size_t cb_threshold,
                          const cras_audio_format* format,
                          cras_audio_shm* shm) {
  RstreamPtr rstream(
      reinterpret_cast<cras_rstream*>(calloc(1, sizeof(cras_rstream))), free);
  rstream->stream_id = id;
  rstream->direction = direction;
  rstream->fd = RSTREAM_FAKE_POLL_FD;
  rstream->buffer_frames = cb_threshold * 2;
  rstream->cb_threshold = cb_threshold;
  rstream->shm = shm;
  rstream->format = *format;
  cras_frames_to_time(cb_threshold, rstream->format.frame_rate,
                      &rstream->sleep_interval_ts);
  return rstream;
}

DevStreamPtr create_dev_stream(unsigned int dev_id, cras_rstream* rstream) {
  DevStreamPtr dstream(
      reinterpret_cast<dev_stream*>(calloc(1, sizeof(dev_stream))), free);
  dstream->dev_id = dev_id;
  dstream->stream = rstream;
  dstream->dev_rate = rstream->format.frame_rate;
  dstream->is_running = true;
  return dstream;
}

StreamPtr create_stream(cras_stream_id_t id,
                        unsigned int dev_id,
                        CRAS_STREAM_DIRECTION direction,
                        size_t cb_threshold,
                        const cras_audio_format* format) {
  ShmPtr shm = create_shm(cb_threshold);
  RstreamPtr rstream =
      create_rstream(1, CRAS_STREAM_INPUT, cb_threshold, format, shm.get());
  DevStreamPtr dstream = create_dev_stream(1, rstream.get());
  StreamPtr s(
      new Stream(std::move(shm), std::move(rstream), std::move(dstream)));
  return s;
}

void AddFakeDataToStream(Stream* stream, unsigned int frames) {
  cras_shm_check_write_overrun(stream->rstream->shm);
  cras_shm_buffer_written(stream->rstream->shm, frames);
}

int delay_frames_stub(const struct cras_iodev* iodev) {
  return 0;
}

IonodePtr create_ionode(CRAS_NODE_TYPE type) {
  IonodePtr ionode(
      reinterpret_cast<cras_ionode*>(calloc(1, sizeof(cras_ionode))), free);
  ionode->type = type;
  return ionode;
}

int fake_flush_buffer(struct cras_iodev* iodev) {
  return 0;
}

IodevPtr create_open_iodev(CRAS_STREAM_DIRECTION direction,
                           size_t cb_threshold,
                           cras_audio_format* format,
                           cras_ionode* active_node) {
  IodevPtr iodev(reinterpret_cast<cras_iodev*>(calloc(1, sizeof(cras_iodev))),
                 free);
  iodev->is_enabled = 1;
  iodev->direction = direction;
  iodev->format = format;
  iodev->state = CRAS_IODEV_STATE_OPEN;
  iodev->delay_frames = delay_frames_stub;
  iodev->active_node = active_node;
  iodev->buffer_size = cb_threshold * 2;
  iodev->min_cb_level = UINT_MAX;
  iodev->max_cb_level = 0;
  iodev->largest_cb_level = 0;
  iodev->flush_buffer = &fake_flush_buffer;
  return iodev;
}

DevicePtr create_device(CRAS_STREAM_DIRECTION direction,
                        size_t cb_threshold,
                        cras_audio_format* format,
                        CRAS_NODE_TYPE active_node_type) {
  IonodePtr node = create_ionode(active_node_type);
  IodevPtr dev = create_open_iodev(direction, cb_threshold, format, node.get());
  OpendevPtr odev(reinterpret_cast<open_dev*>(calloc(1, sizeof(open_dev))),
                  free);
  odev->dev = dev.get();

  DevicePtr d(new Device(std::move(dev), std::move(node), std::move(odev)));
  return d;
}

void add_stream_to_dev(IodevPtr& dev, const StreamPtr& stream) {
  DL_APPEND(dev->streams, stream->dstream.get());
  dev->min_cb_level = std::min(stream->rstream->cb_threshold,
                               static_cast<size_t>(dev->min_cb_level));
  dev->max_cb_level = std::max(stream->rstream->cb_threshold,
                               static_cast<size_t>(dev->max_cb_level));
  dev->largest_cb_level = std::max(stream->rstream->cb_threshold,
                                   static_cast<size_t>(dev->max_cb_level));
}

void fill_audio_format(cras_audio_format* format, unsigned int rate) {
  format->format = SND_PCM_FORMAT_S16_LE;
  format->frame_rate = rate;
  format->num_channels = 2;
  format->channel_layout[0] = 0;
  format->channel_layout[1] = 1;
  for (int i = 2; i < CRAS_CH_MAX; i++)
    format->channel_layout[i] = -1;
}
