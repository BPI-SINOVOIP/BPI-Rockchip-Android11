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

#define LOG_TAG "audio_hw_generic"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <log/log.h>

#include "audio_vbuffer.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

int audio_vbuffer_init(audio_vbuffer_t *audio_vbuffer, size_t frame_count,
                       size_t frame_size) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }
  audio_vbuffer->frame_size = frame_size;
  audio_vbuffer->frame_count = frame_count;
  size_t bytes = frame_count * frame_size;
  audio_vbuffer->data = calloc(bytes, 1);
  if (!audio_vbuffer->data) {
    return -ENOMEM;
  }
  audio_vbuffer->head = 0;
  audio_vbuffer->tail = 0;
  audio_vbuffer->live = 0;
  pthread_mutex_init(&audio_vbuffer->lock, (const pthread_mutexattr_t *)NULL);
  return 0;
}

int audio_vbuffer_destroy(audio_vbuffer_t *audio_vbuffer) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }
  free(audio_vbuffer->data);
  pthread_mutex_destroy(&audio_vbuffer->lock);
  return 0;
}

int audio_vbuffer_live(audio_vbuffer_t *audio_vbuffer) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }
  pthread_mutex_lock(&audio_vbuffer->lock);
  int live = audio_vbuffer->live;
  pthread_mutex_unlock(&audio_vbuffer->lock);
  return live;
}

int audio_vbuffer_dead(audio_vbuffer_t *audio_vbuffer) {
  if (!audio_vbuffer) {
    return -EINVAL;
  }
  pthread_mutex_lock(&audio_vbuffer->lock);
  int dead = audio_vbuffer->frame_count - audio_vbuffer->live;
  pthread_mutex_unlock(&audio_vbuffer->lock);
  return dead;
}

size_t audio_vbuffer_write(audio_vbuffer_t *audio_vbuffer, const void *buffer,
                           size_t frame_count) {
  size_t frames_written = 0;
  pthread_mutex_lock(&audio_vbuffer->lock);

  while (frame_count != 0) {
    int frames = 0;
    if (audio_vbuffer->live == 0 || audio_vbuffer->head > audio_vbuffer->tail) {
      frames =
          MIN(frame_count, audio_vbuffer->frame_count - audio_vbuffer->head);
    } else if (audio_vbuffer->head < audio_vbuffer->tail) {
      frames = MIN(frame_count, audio_vbuffer->tail - (audio_vbuffer->head));
    } else {
      ALOGD("%s audio_vbuffer is full", __func__);
      break;
    }
    memcpy(
        &audio_vbuffer->data[audio_vbuffer->head * audio_vbuffer->frame_size],
        &((uint8_t *)buffer)[frames_written * audio_vbuffer->frame_size],
        frames * audio_vbuffer->frame_size);
    audio_vbuffer->live += frames;
    frames_written += frames;
    frame_count -= frames;
    audio_vbuffer->head =
        (audio_vbuffer->head + frames) % audio_vbuffer->frame_count;
  }

  pthread_mutex_unlock(&audio_vbuffer->lock);
  return frames_written;
}

size_t audio_vbuffer_read(audio_vbuffer_t *audio_vbuffer, void *buffer,
                          size_t frame_count) {
  size_t frames_read = 0;
  pthread_mutex_lock(&audio_vbuffer->lock);

  while (frame_count != 0) {
    int frames = 0;
    if (audio_vbuffer->live == audio_vbuffer->frame_count ||
        audio_vbuffer->tail > audio_vbuffer->head) {
      frames =
          MIN(frame_count, audio_vbuffer->frame_count - audio_vbuffer->tail);
    } else if (audio_vbuffer->tail < audio_vbuffer->head) {
      frames = MIN(frame_count, audio_vbuffer->head - audio_vbuffer->tail);
    } else {
      break;
    }
    memcpy(
        &((uint8_t *)buffer)[frames_read * audio_vbuffer->frame_size],
        &audio_vbuffer->data[audio_vbuffer->tail * audio_vbuffer->frame_size],
        frames * audio_vbuffer->frame_size);
    audio_vbuffer->live -= frames;
    frames_read += frames;
    frame_count -= frames;
    audio_vbuffer->tail =
        (audio_vbuffer->tail + frames) % audio_vbuffer->frame_count;
  }

  pthread_mutex_unlock(&audio_vbuffer->lock);
  return frames_read;
}
