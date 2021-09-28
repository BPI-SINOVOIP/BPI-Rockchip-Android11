/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EXT_PCM_H
#define EXT_PCM_H

#include <pthread.h>

#include <cutils/hashmap.h>
#include <tinyalsa/asoundlib.h>

// Holds up to 4KB buffer for each mixer pipeline, this value is arbitrary chosen
#define MIXER_BUFFER_SIZE (1024 * 4)

struct ext_mixer_pipeline {
  int16_t buffer[MIXER_BUFFER_SIZE];
  unsigned int position;
};

struct ext_pcm {
  struct pcm *pcm;
  pthread_mutex_t lock;
  unsigned int ref_count;
  pthread_mutex_t mixer_lock;
  struct ext_mixer_pipeline mixer_pipeline;
  pthread_t mixer_thread;
  Hashmap *mixer_pipeline_map;
};

struct ext_pcm *ext_pcm_open(unsigned int card, unsigned int device,
                             unsigned int flags, struct pcm_config *config);
int ext_pcm_close(struct ext_pcm *ext_pcm);
int ext_pcm_is_ready(struct ext_pcm *ext_pcm);
int ext_pcm_write(struct ext_pcm *ext_pcm, const char *bus_address,
                  const void *data, unsigned int count);
const char *ext_pcm_get_error(struct ext_pcm *ext_pcm);
unsigned int ext_pcm_frames_to_bytes(struct ext_pcm *ext_pcm,
                                     unsigned int frames);

#endif  // EXT_PCM_H
