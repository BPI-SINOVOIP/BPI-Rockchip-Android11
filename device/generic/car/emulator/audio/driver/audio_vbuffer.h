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

#ifndef AUDIO_VBUFFER_H
#define AUDIO_VBUFFER_H

#include <pthread.h>

typedef struct audio_vbuffer {
  pthread_mutex_t lock;
  uint8_t *data;
  size_t frame_size;
  size_t frame_count;
  size_t head;
  size_t tail;
  size_t live;
} audio_vbuffer_t;

int audio_vbuffer_init(audio_vbuffer_t *audio_vbuffer, size_t frame_count,
                       size_t frame_size);

int audio_vbuffer_destroy(audio_vbuffer_t *audio_vbuffer);

int audio_vbuffer_live(audio_vbuffer_t *audio_vbuffer);

int audio_vbuffer_dead(audio_vbuffer_t *audio_vbuffer);

size_t audio_vbuffer_write(audio_vbuffer_t *audio_vbuffer, const void *buffer,
                           size_t frame_count);

size_t audio_vbuffer_read(audio_vbuffer_t *audio_vbuffer, void *buffer,
                          size_t frame_count);

#endif  // AUDIO_VBUFFER_H
