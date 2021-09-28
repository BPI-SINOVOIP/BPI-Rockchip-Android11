/*
 * Copyright (C) 2017 The Android Open Source Project
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

/**
 * Derived from goldfish/audio/audio_hw.c
 * Changes made to adding support of AUDIO_DEVICE_OUT_BUS
 */

#define LOG_TAG "audio_hw_generic"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <log/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>

#include <hardware/hardware.h>
#include <system/audio.h>

#include "audio_hw.h"
#include "ext_pcm.h"

#define PCM_CARD 0
#define PCM_DEVICE 0

#define OUT_PERIOD_MS 15
#define OUT_PERIOD_COUNT 4

#define IN_PERIOD_MS 15
#define IN_PERIOD_COUNT 4

#define PI 3.14159265
#define TWO_PI  (2*PI)

// 150 Hz
#define DEFAULT_FREQUENCY 150
// Increase in changes to tone frequency
#define TONE_FREQUENCY_INCREASE 20
// Max tone frequency to auto assign, don't want to generate too high of a pitch
#define MAX_TONE_FREQUENCY 500

#define _bool_str(x) ((x)?"true":"false")

static const char * const PROP_KEY_SIMULATE_MULTI_ZONE_AUDIO = "ro.aae.simulateMultiZoneAudio";
static const char * const AAE_PARAMETER_KEY_FOR_SELECTED_ZONE = "com.android.car.emulator.selected_zone";
#define PRIMARY_ZONE_ID 0
#define INVALID_ZONE_ID -1
// Note the primary zone goes to left speaker so route other zone to right speaker
#define DEFAULT_ZONE_TO_LEFT_SPEAKER (PRIMARY_ZONE_ID + 1)

static const char * const TONE_ADDRESS_KEYWORD = "_tone_";
static const char * const AUDIO_ZONE_KEYWORD = "_audio_zone_";

#define SIZE_OF_PARSE_BUFFER 32

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state);

static struct pcm_config pcm_config_out = {
    .channels = 2,
    .rate = 0,
    .period_size = 0,
    .period_count = OUT_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
};

static int get_int_value(const struct str_parms *str_parms, const char *key, int *return_value) {
    char value[SIZE_OF_PARSE_BUFFER];
    int results = str_parms_get_str(str_parms, key, value, SIZE_OF_PARSE_BUFFER);
    if (results >= 0) {
        char *end = NULL;
        errno = 0;
        long val = strtol(value, &end, 10);
        if ((errno == 0) && (end != NULL) && (*end == '\0') && ((int) val == val)) {
            *return_value = val;
        } else {
            results = -EINVAL;
        }
    }
    return results;
}

static struct pcm_config pcm_config_in = {
    .channels = 2,
    .rate = 0,
    .period_size = 0,
    .period_count = IN_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .stop_threshold = INT_MAX,
};

static pthread_mutex_t adev_init_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int audio_device_ref_count = 0;

static bool is_zone_selected_to_play(struct audio_hw_device *dev, int zone_id) {
    // play if current zone is enable or zone equal to primary zone
    bool is_selected_zone = true;
    if (zone_id != PRIMARY_ZONE_ID) {
        struct generic_audio_device *adev = (struct generic_audio_device *)dev;
        pthread_mutex_lock(&adev->lock);
        is_selected_zone = adev->last_zone_selected_to_play == zone_id;
        pthread_mutex_unlock(&adev->lock);
    }
    return is_selected_zone;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    return out->req_config.sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate) {
    return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    int size = out->pcm_config.period_size *
                audio_stream_out_frame_size(&out->stream);

    return size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    return out->req_config.channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    return out->req_config.format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format) {
    return -ENOSYS;
}

static int out_dump(const struct audio_stream *stream, int fd) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    pthread_mutex_lock(&out->lock);
    dprintf(fd, "\tout_dump:\n"
                "\t\taddress: %s\n"
                "\t\tsample rate: %u\n"
                "\t\tbuffer size: %zu\n"
                "\t\tchannel mask: %08x\n"
                "\t\tformat: %d\n"
                "\t\tdevice: %08x\n"
                "\t\tamplitude ratio: %f\n"
                "\t\tenabled channels: %d\n"
                "\t\taudio dev: %p\n\n",
                out->bus_address,
                out_get_sample_rate(stream),
                out_get_buffer_size(stream),
                out_get_channels(stream),
                out_get_format(stream),
                out->device,
                out->amplitude_ratio,
                out->enabled_channels,
                out->dev);
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    struct str_parms *parms;
    int ret = 0;

    pthread_mutex_lock(&out->lock);
    if (!out->standby) {
        //Do not support changing params while stream running
        ret = -ENOSYS;
    } else {
        parms = str_parms_create_str(kvpairs);
        int val = 0;
        ret = get_int_value(parms, AUDIO_PARAMETER_STREAM_ROUTING, &val);
        if (ret >= 0) {
            out->device = (int)val;
            ret = 0;
        }
        str_parms_destroy(parms);
    }
    pthread_mutex_unlock(&out->lock);
    return ret;
}

static char *out_get_parameters(const struct audio_stream *stream, const char *keys) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    int ret;

    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        pthread_mutex_lock(&out->lock);
        str_parms_add_int(reply, AUDIO_PARAMETER_STREAM_ROUTING, out->device);
        pthread_mutex_unlock(&out->lock);
        str = strdup(str_parms_to_str(reply));
    } else {
        str = strdup(keys);
    }

    str_parms_destroy(query);
    str_parms_destroy(reply);
    return str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    return (out->pcm_config.period_size * 1000) / out->pcm_config.rate;
}

static int out_set_volume(struct audio_stream_out *stream,
        float left, float right) {
    return -ENOSYS;
}

static int get_zone_id_from_address(const char *address) {
    int zone_id = INVALID_ZONE_ID;
    char *zone_start = strstr(address, AUDIO_ZONE_KEYWORD);
    if (zone_start) {
        char *end = NULL;
        zone_id = strtol(zone_start + strlen(AUDIO_ZONE_KEYWORD), &end, 10);
        if (end == NULL || zone_id < 0) {
            return INVALID_ZONE_ID;
        }
    }
    return zone_id;
}

static void *out_write_worker(void *args) {
    struct generic_stream_out *out = (struct generic_stream_out *)args;
    struct ext_pcm *ext_pcm = NULL;
    uint8_t *buffer = NULL;
    int buffer_frames;
    int buffer_size;
    bool restart = false;
    bool shutdown = false;
    int zone_id = PRIMARY_ZONE_ID;
    // If it is a audio zone keyword bus address then get zone id
    if (strstr(out->bus_address, AUDIO_ZONE_KEYWORD)) {
        zone_id = get_zone_id_from_address(out->bus_address);
        if (zone_id == INVALID_ZONE_ID) {
            ALOGE("%s Found invalid zone id, defaulting device %s to zone %d", __func__,
                out->bus_address, DEFAULT_ZONE_TO_LEFT_SPEAKER);
            zone_id = DEFAULT_ZONE_TO_LEFT_SPEAKER;
        }
    }
    ALOGD("Out worker:%s zone id %d", out->bus_address, zone_id);

    while (true) {
        pthread_mutex_lock(&out->lock);
        while (out->worker_standby || restart) {
            restart = false;
            if (ext_pcm) {
                ext_pcm_close(ext_pcm); // Frees pcm
                ext_pcm = NULL;
                free(buffer);
                buffer=NULL;
            }
            if (out->worker_exit) {
                break;
            }
            pthread_cond_wait(&out->worker_wake, &out->lock);
        }

        if (out->worker_exit) {
            if (!out->worker_standby) {
                ALOGE("Out worker:%s not in standby before exiting", out->bus_address);
            }
            shutdown = true;
        }

        while (!shutdown && audio_vbuffer_live(&out->buffer) == 0) {
            pthread_cond_wait(&out->worker_wake, &out->lock);
        }

        if (shutdown) {
            pthread_mutex_unlock(&out->lock);
            break;
        }

        if (!ext_pcm) {
            ext_pcm = ext_pcm_open(PCM_CARD, PCM_DEVICE,
                    PCM_OUT | PCM_MONOTONIC, &out->pcm_config);
            if (!ext_pcm_is_ready(ext_pcm)) {
                ALOGE("pcm_open(out) failed: %s: address %s channels %d format %d rate %d",
                        ext_pcm_get_error(ext_pcm),
                        out->bus_address,
                        out->pcm_config.channels,
                        out->pcm_config.format,
                        out->pcm_config.rate);
                pthread_mutex_unlock(&out->lock);
                break;
            }
            buffer_frames = out->pcm_config.period_size;
            buffer_size = ext_pcm_frames_to_bytes(ext_pcm, buffer_frames);
            buffer = malloc(buffer_size);
            if (!buffer) {
                ALOGE("could not allocate write buffer");
                pthread_mutex_unlock(&out->lock);
                break;
            }
        }
        int frames = audio_vbuffer_read(&out->buffer, buffer, buffer_frames);
        pthread_mutex_unlock(&out->lock);

        if (is_zone_selected_to_play(out->dev, zone_id)) {
            int write_error = ext_pcm_write(ext_pcm, out->bus_address,
                buffer, ext_pcm_frames_to_bytes(ext_pcm, frames));
            if (write_error) {
                ALOGE("pcm_write failed %s address %s",
                    ext_pcm_get_error(ext_pcm), out->bus_address);
                restart = true;
            } else {
                ALOGV("pcm_write succeed address %s", out->bus_address);
            }
        }
    }
    if (buffer) {
        free(buffer);
    }

    return NULL;
}

// Call with in->lock held
static void get_current_output_position(struct generic_stream_out *out,
        uint64_t *position, struct timespec * timestamp) {
    struct timespec curtime = { .tv_sec = 0, .tv_nsec = 0 };
    clock_gettime(CLOCK_MONOTONIC, &curtime);
    const int64_t now_us = (curtime.tv_sec * 1000000000LL + curtime.tv_nsec) / 1000;
    if (timestamp) {
        *timestamp = curtime;
    }
    int64_t position_since_underrun;
    if (out->standby) {
        position_since_underrun = 0;
    } else {
        const int64_t first_us = (out->underrun_time.tv_sec * 1000000000LL +
                                  out->underrun_time.tv_nsec) / 1000;
        position_since_underrun = (now_us - first_us) *
                out_get_sample_rate(&out->stream.common) /
                1000000;
        if (position_since_underrun < 0) {
            position_since_underrun = 0;
        }
    }
    *position = out->underrun_position + position_since_underrun;

    // The device will reuse the same output stream leading to periods of
    // underrun.
    if (*position > out->frames_written) {
        ALOGW("Not supplying enough data to HAL, expected position %" PRIu64 " , only wrote "
              "%" PRIu64,
              *position, out->frames_written);

        *position = out->frames_written;
        out->underrun_position = *position;
        out->underrun_time = curtime;
        out->frames_total_buffered = 0;
    }
}

// Applies gain naively, assumes AUDIO_FORMAT_PCM_16_BIT and stereo output
static void out_apply_gain(struct generic_stream_out *out, const void *buffer, size_t bytes) {
    int16_t *int16_buffer = (int16_t *)buffer;
    size_t int16_size = bytes / sizeof(int16_t);
    for (int i = 0; i < int16_size; i++) {
        if ((i % 2) && !(out->enabled_channels & RIGHT_CHANNEL)) {
            int16_buffer[i] = 0;
        } else if (!(i % 2) && !(out->enabled_channels & LEFT_CHANNEL)) {
            int16_buffer[i] = 0;
        } else {
            float multiplied = int16_buffer[i] * out->amplitude_ratio;
            if (multiplied > INT16_MAX) int16_buffer[i] = INT16_MAX;
            else if (multiplied < INT16_MIN) int16_buffer[i] = INT16_MIN;
            else int16_buffer[i] = (int16_t)multiplied;
        }
    }
}

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer, size_t bytes) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    ALOGV("%s: to device %s", __func__, out->bus_address);
    const size_t frames =  bytes / audio_stream_out_frame_size(stream);

    pthread_mutex_lock(&out->lock);

    if (out->worker_standby) {
        out->worker_standby = false;
    }

    uint64_t current_position;
    struct timespec current_time;

    get_current_output_position(out, &current_position, &current_time);
    const uint64_t now_us = (current_time.tv_sec * 1000000000LL +
                             current_time.tv_nsec) / 1000;
    if (out->standby) {
        out->standby = false;
        out->underrun_time = current_time;
        out->frames_rendered = 0;
        out->frames_total_buffered = 0;
    }

    size_t frames_written = frames;
    if (out->dev->master_mute) {
        ALOGV("%s: ignored due to master mute", __func__);
    } else {
        out_apply_gain(out, buffer, bytes);
        frames_written = audio_vbuffer_write(&out->buffer, buffer, frames);
        pthread_cond_signal(&out->worker_wake);
    }

    /* Implementation just consumes bytes if we start getting backed up */
    out->frames_written += frames;
    out->frames_rendered += frames;
    out->frames_total_buffered += frames;

    // We simulate the audio device blocking when it's write buffers become
    // full.

    // At the beginning or after an underrun, try to fill up the vbuffer.
    // This will be throttled by the PlaybackThread
    int frames_sleep = out->frames_total_buffered < out->buffer.frame_count ? 0 : frames;

    uint64_t sleep_time_us = frames_sleep * 1000000LL /
                            out_get_sample_rate(&stream->common);

    // If the write calls are delayed, subtract time off of the sleep to
    // compensate
    uint64_t time_since_last_write_us = now_us - out->last_write_time_us;
    if (time_since_last_write_us < sleep_time_us) {
        sleep_time_us -= time_since_last_write_us;
    } else {
        sleep_time_us = 0;
    }
    out->last_write_time_us = now_us + sleep_time_us;

    pthread_mutex_unlock(&out->lock);

    if (sleep_time_us > 0) {
        usleep(sleep_time_us);
    }

    if (frames_written < frames) {
        ALOGW("%s Hardware backing HAL too slow, could only write %zu of %zu frames",
            __func__, frames_written, frames);
    }

    /* Always consume all bytes */
    return bytes;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
        uint64_t *frames, struct timespec *timestamp) {
    int ret = -EINVAL;
    if (stream == NULL || frames == NULL || timestamp == NULL) {
        return -EINVAL;
    }
    struct generic_stream_out *out = (struct generic_stream_out *)stream;

    pthread_mutex_lock(&out->lock);
    get_current_output_position(out, frames, timestamp);
    pthread_mutex_unlock(&out->lock);

    return 0;
}

static int out_get_render_position(const struct audio_stream_out *stream, uint32_t *dsp_frames) {
    if (stream == NULL || dsp_frames == NULL) {
        return -EINVAL;
    }
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    pthread_mutex_lock(&out->lock);
    *dsp_frames = out->frames_rendered;
    pthread_mutex_unlock(&out->lock);
    return 0;
}

// Must be called with out->lock held
static void do_out_standby(struct generic_stream_out *out) {
    int frames_sleep = 0;
    uint64_t sleep_time_us = 0;
    if (out->standby) {
        return;
    }
    while (true) {
        get_current_output_position(out, &out->underrun_position, NULL);
        frames_sleep = out->frames_written - out->underrun_position;

        if (frames_sleep == 0) {
            break;
        }

        sleep_time_us = frames_sleep * 1000000LL /
                        out_get_sample_rate(&out->stream.common);

        pthread_mutex_unlock(&out->lock);
        usleep(sleep_time_us);
        pthread_mutex_lock(&out->lock);
    }
    out->worker_standby = true;
    out->standby = true;
}

static int out_standby(struct audio_stream *stream) {
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    pthread_mutex_lock(&out->lock);
    do_out_standby(out);
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect) {
    // out_add_audio_effect is a no op
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect) {
    // out_remove_audio_effect is a no op
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
        int64_t *timestamp) {
    return -ENOSYS;
}

static uint32_t in_get_sample_rate(const struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    return in->req_config.sample_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate) {
    return -ENOSYS;
}

static int refine_output_parameters(uint32_t *sample_rate, audio_format_t *format,
        audio_channel_mask_t *channel_mask) {
    static const uint32_t sample_rates [] = {
        8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000
    };
    static const int sample_rates_count = sizeof(sample_rates)/sizeof(uint32_t);
    bool inval = false;
    if (*format != AUDIO_FORMAT_PCM_16_BIT) {
        *format = AUDIO_FORMAT_PCM_16_BIT;
        inval = true;
    }

    int channel_count = popcount(*channel_mask);
    if (channel_count != 1 && channel_count != 2) {
        *channel_mask = AUDIO_CHANNEL_IN_STEREO;
        inval = true;
    }

    int i;
    for (i = 0; i < sample_rates_count; i++) {
        if (*sample_rate < sample_rates[i]) {
            *sample_rate = sample_rates[i];
            inval=true;
            break;
        }
        else if (*sample_rate == sample_rates[i]) {
            break;
        }
        else if (i == sample_rates_count-1) {
            // Cap it to the highest rate we support
            *sample_rate = sample_rates[i];
            inval=true;
        }
    }

    if (inval) {
        return -EINVAL;
    }
    return 0;
}

static int refine_input_parameters(uint32_t *sample_rate, audio_format_t *format,
        audio_channel_mask_t *channel_mask) {
    static const uint32_t sample_rates [] = {
        8000, 11025, 16000, 22050, 44100, 48000
    };
    static const int sample_rates_count = sizeof(sample_rates)/sizeof(uint32_t);
    bool inval = false;
    // Only PCM_16_bit is supported. If this is changed, stereo to mono drop
    // must be fixed in in_read
    if (*format != AUDIO_FORMAT_PCM_16_BIT) {
        *format = AUDIO_FORMAT_PCM_16_BIT;
        inval = true;
    }

    int channel_count = popcount(*channel_mask);
    if (channel_count != 1 && channel_count != 2) {
        *channel_mask = AUDIO_CHANNEL_IN_STEREO;
        inval = true;
    }

    int i;
    for (i = 0; i < sample_rates_count; i++) {
        if (*sample_rate < sample_rates[i]) {
            *sample_rate = sample_rates[i];
            inval=true;
            break;
        }
        else if (*sample_rate == sample_rates[i]) {
            break;
        }
        else if (i == sample_rates_count-1) {
            // Cap it to the highest rate we support
            *sample_rate = sample_rates[i];
            inval=true;
        }
    }

    if (inval) {
        return -EINVAL;
    }
    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate, audio_format_t format,
        audio_channel_mask_t channel_mask) {
    size_t size;
    size_t device_rate;
    int channel_count = popcount(channel_mask);
    if (refine_input_parameters(&sample_rate, &format, &channel_mask) != 0)
        return 0;

    size = sample_rate*IN_PERIOD_MS/1000;
    // Audioflinger expects audio buffers to be multiple of 16 frames
    size = ((size + 15) / 16) * 16;
    size *= sizeof(short) * channel_count;

    return size;
}

static size_t in_get_buffer_size(const struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    int size = get_input_buffer_size(in->req_config.sample_rate,
                                 in->req_config.format,
                                 in->req_config.channel_mask);

    return size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    return in->req_config.channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    return in->req_config.format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format) {
    return -ENOSYS;
}

static int in_dump(const struct audio_stream *stream, int fd) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;

    pthread_mutex_lock(&in->lock);
    dprintf(fd, "\tin_dump:\n"
                "\t\tsample rate: %u\n"
                "\t\tbuffer size: %zu\n"
                "\t\tchannel mask: %08x\n"
                "\t\tformat: %d\n"
                "\t\tdevice: %08x\n"
                "\t\taudio dev: %p\n\n",
                in_get_sample_rate(stream),
                in_get_buffer_size(stream),
                in_get_channels(stream),
                in_get_format(stream),
                in->device,
                in->dev);
    pthread_mutex_unlock(&in->lock);
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    struct str_parms *parms;
    int ret = 0;

    pthread_mutex_lock(&in->lock);
    if (!in->standby) {
        ret = -ENOSYS;
    } else {
        parms = str_parms_create_str(kvpairs);
        int val = 0;
        ret = get_int_value(parms, AUDIO_PARAMETER_STREAM_ROUTING, &val);
        if (ret >= 0) {
            in->device = (int)val;
            ret = 0;
        }

        str_parms_destroy(parms);
    }
    pthread_mutex_unlock(&in->lock);
    return ret;
}

static char *in_get_parameters(const struct audio_stream *stream, const char *keys) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    int ret;

    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        str_parms_add_int(reply, AUDIO_PARAMETER_STREAM_ROUTING, in->device);
        str = strdup(str_parms_to_str(reply));
    } else {
        str = strdup(keys);
    }

    str_parms_destroy(query);
    str_parms_destroy(reply);
    return str;
}

static int in_set_gain(struct audio_stream_in *stream, float gain) {
    // TODO(hwwang): support adjusting input gain
    return 0;
}

// Call with in->lock held
static void get_current_input_position(struct generic_stream_in *in,
        int64_t * position, struct timespec * timestamp) {
    struct timespec t = { .tv_sec = 0, .tv_nsec = 0 };
    clock_gettime(CLOCK_MONOTONIC, &t);
    const int64_t now_us = (t.tv_sec * 1000000000LL + t.tv_nsec) / 1000;
    if (timestamp) {
        *timestamp = t;
    }
    int64_t position_since_standby;
    if (in->standby) {
        position_since_standby = 0;
    } else {
        const int64_t first_us = (in->standby_exit_time.tv_sec * 1000000000LL +
                                  in->standby_exit_time.tv_nsec) / 1000;
        position_since_standby = (now_us - first_us) *
                in_get_sample_rate(&in->stream.common) /
                1000000;
        if (position_since_standby < 0) {
            position_since_standby = 0;
        }
    }
    *position = in->standby_position + position_since_standby;
}

// Must be called with in->lock held
static void do_in_standby(struct generic_stream_in *in) {
    if (in->standby) {
        return;
    }
    in->worker_standby = true;
    get_current_input_position(in, &in->standby_position, NULL);
    in->standby = true;
}

static int in_standby(struct audio_stream *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    pthread_mutex_lock(&in->lock);
    do_in_standby(in);
    pthread_mutex_unlock(&in->lock);
    return 0;
}

// Generates pure tone for FM_TUNER and bus_device
static int pseudo_pcm_read(void *data, unsigned int count, struct oscillator *oscillator) {
    unsigned int length = count / sizeof(int16_t);
    int16_t *sdata = (int16_t *)data;
    for (int index = 0; index < length; index++) {
        sdata[index] = (int16_t)(sin(oscillator->phase) * 4096);
        oscillator->phase += oscillator->phase_increment;
        oscillator->phase = oscillator->phase > TWO_PI ?
            oscillator->phase - TWO_PI : oscillator->phase;
    }

    return count;
}

static void *in_read_worker(void *args) {
    struct generic_stream_in *in = (struct generic_stream_in *)args;
    struct pcm *pcm = NULL;
    uint8_t *buffer = NULL;
    size_t buffer_frames;
    int buffer_size;

    bool restart = false;
    bool shutdown = false;
    while (true) {
        pthread_mutex_lock(&in->lock);
        while (in->worker_standby || restart) {
            restart = false;
            if (pcm) {
                pcm_close(pcm); // Frees pcm
                pcm = NULL;
                free(buffer);
                buffer=NULL;
            }
            if (in->worker_exit) {
                break;
            }
            pthread_cond_wait(&in->worker_wake, &in->lock);
        }

        if (in->worker_exit) {
            if (!in->worker_standby) {
                ALOGE("In worker not in standby before exiting");
            }
            shutdown = true;
        }
        if (shutdown) {
            pthread_mutex_unlock(&in->lock);
            break;
        }
        if (!pcm) {
            pcm = pcm_open(PCM_CARD, PCM_DEVICE,
                    PCM_IN | PCM_MONOTONIC, &in->pcm_config);
            if (!pcm_is_ready(pcm)) {
                ALOGE("pcm_open(in) failed: %s: channels %d format %d rate %d",
                        pcm_get_error(pcm),
                        in->pcm_config.channels,
                        in->pcm_config.format,
                        in->pcm_config.rate);
                pthread_mutex_unlock(&in->lock);
                break;
            }
            buffer_frames = in->pcm_config.period_size;
            buffer_size = pcm_frames_to_bytes(pcm, buffer_frames);
            buffer = malloc(buffer_size);
            if (!buffer) {
                ALOGE("could not allocate worker read buffer");
                pthread_mutex_unlock(&in->lock);
                break;
            }
        }
        pthread_mutex_unlock(&in->lock);
        int ret = pcm_read(pcm, buffer, pcm_frames_to_bytes(pcm, buffer_frames));
        if (ret != 0) {
            ALOGW("pcm_read failed %s", pcm_get_error(pcm));
            restart = true;
        }

        pthread_mutex_lock(&in->lock);
        size_t frames_written = audio_vbuffer_write(&in->buffer, buffer, buffer_frames);
        pthread_mutex_unlock(&in->lock);

        if (frames_written != buffer_frames) {
            ALOGW("in_read_worker only could write %zu / %zu frames",
                    frames_written, buffer_frames);
        }
    }
    if (buffer) {
        free(buffer);
    }
    return NULL;
}

static bool address_has_tone_keyword(char * address) {
    return strstr(address, TONE_ADDRESS_KEYWORD) != NULL;
}

static bool is_tone_generator_device(struct generic_stream_in *in) {
    return in->device == AUDIO_DEVICE_IN_FM_TUNER || ((in->device == AUDIO_DEVICE_IN_BUS) &&
        address_has_tone_keyword(in->bus_address));
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer, size_t bytes) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    struct generic_audio_device *adev = in->dev;
    const size_t frames =  bytes / audio_stream_in_frame_size(stream);
    int ret = 0;
    bool mic_mute = false;
    size_t read_bytes = 0;

    adev_get_mic_mute(&adev->device, &mic_mute);
    pthread_mutex_lock(&in->lock);

    if (in->worker_standby) {
        in->worker_standby = false;
    }

    // Tone generators fill the buffer via pseudo_pcm_read directly
    if (!is_tone_generator_device(in)) {
        pthread_cond_signal(&in->worker_wake);
    }

    int64_t current_position;
    struct timespec current_time;

    get_current_input_position(in, &current_position, &current_time);
    if (in->standby) {
        in->standby = false;
        in->standby_exit_time = current_time;
        in->standby_frames_read = 0;
    }

    const int64_t frames_available =
        current_position - in->standby_position - in->standby_frames_read;
    assert(frames_available >= 0);

    const size_t frames_wait =
        ((uint64_t)frames_available > frames) ? 0 : frames - frames_available;

    int64_t sleep_time_us  = frames_wait * 1000000LL / in_get_sample_rate(&stream->common);

    pthread_mutex_unlock(&in->lock);

    if (sleep_time_us > 0) {
        usleep(sleep_time_us);
    }

    pthread_mutex_lock(&in->lock);
    int read_frames = 0;
    if (in->standby) {
        ALOGW("Input put to sleep while read in progress");
        goto exit;
    }
    in->standby_frames_read += frames;

    if (is_tone_generator_device(in)) {
        int read_bytes = pseudo_pcm_read(buffer, bytes, &in->oscillator);
        read_frames = read_bytes / audio_stream_in_frame_size(stream);
    } else if (popcount(in->req_config.channel_mask) == 1 &&
        in->pcm_config.channels == 2) {
        // Need to resample to mono
        if (in->stereo_to_mono_buf_size < bytes*2) {
            in->stereo_to_mono_buf = realloc(in->stereo_to_mono_buf, bytes*2);
            if (!in->stereo_to_mono_buf) {
                ALOGE("Failed to allocate stereo_to_mono_buff");
                goto exit;
            }
        }

        read_frames = audio_vbuffer_read(&in->buffer, in->stereo_to_mono_buf, frames);

        // Currently only pcm 16 is supported.
        uint16_t *src = (uint16_t *)in->stereo_to_mono_buf;
        uint16_t *dst = (uint16_t *)buffer;
        size_t i;
        // Resample stereo 16 to mono 16 by dropping one channel.
        // The stereo stream is interleaved L-R-L-R
        for (i = 0; i < frames; i++) {
            *dst = *src;
            src += 2;
            dst += 1;
        }
    } else {
        read_frames = audio_vbuffer_read(&in->buffer, buffer, frames);
    }

exit:
    read_bytes = read_frames*audio_stream_in_frame_size(stream);

    if (mic_mute) {
        read_bytes = 0;
    }

    if (read_bytes < bytes) {
        memset (&((uint8_t *)buffer)[read_bytes], 0, bytes-read_bytes);
    }

    pthread_mutex_unlock(&in->lock);

    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream) {
    return 0;
}

static int in_get_capture_position(const struct audio_stream_in *stream,
        int64_t *frames, int64_t *time) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    pthread_mutex_lock(&in->lock);
    struct timespec current_time;
    get_current_input_position(in, frames, &current_time);
    *time = (current_time.tv_sec * 1000000000LL + current_time.tv_nsec);
    pthread_mutex_unlock(&in->lock);
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect) {
    // in_add_audio_effect is a no op
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect) {
    // in_add_audio_effect is a no op
    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
        audio_io_handle_t handle, audio_devices_t devices, audio_output_flags_t flags,
        struct audio_config *config, struct audio_stream_out **stream_out, const char *address) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    struct generic_stream_out *out;
    int ret = 0;

    if (refine_output_parameters(&config->sample_rate, &config->format, &config->channel_mask)) {
        ALOGE("Error opening output stream format %d, channel_mask %04x, sample_rate %u",
              config->format, config->channel_mask, config->sample_rate);
        ret = -EINVAL;
        goto error;
    }

    out = (struct generic_stream_out *)calloc(1, sizeof(struct generic_stream_out));

    if (!out)
        return -ENOMEM;

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_presentation_position = out_get_presentation_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;

    pthread_mutex_init(&out->lock, (const pthread_mutexattr_t *) NULL);
    out->dev = adev;
    out->device = devices;
    memcpy(&out->req_config, config, sizeof(struct audio_config));
    memcpy(&out->pcm_config, &pcm_config_out, sizeof(struct pcm_config));
    out->pcm_config.rate = config->sample_rate;
    out->pcm_config.period_size = out->pcm_config.rate*OUT_PERIOD_MS/1000;

    out->standby = true;
    out->underrun_position = 0;
    out->underrun_time.tv_sec = 0;
    out->underrun_time.tv_nsec = 0;
    out->last_write_time_us = 0;
    out->frames_total_buffered = 0;
    out->frames_written = 0;
    out->frames_rendered = 0;

    ret = audio_vbuffer_init(&out->buffer,
            out->pcm_config.period_size*out->pcm_config.period_count,
            out->pcm_config.channels *
            pcm_format_to_bits(out->pcm_config.format) >> 3);
    if (ret == 0) {
        pthread_cond_init(&out->worker_wake, NULL);
        out->worker_standby = true;
        out->worker_exit = false;
        pthread_create(&out->worker_thread, NULL, out_write_worker, out);
    }

    out->enabled_channels = BOTH_CHANNELS;
    if (address) {
        out->bus_address = calloc(strlen(address) + 1, sizeof(char));
        strncpy(out->bus_address, address, strlen(address));
        hashmapPut(adev->out_bus_stream_map, out->bus_address, out);
        /* TODO: read struct audio_gain from audio_policy_configuration */
        out->gain_stage = (struct audio_gain) {
            .min_value = -3200,
            .max_value = 600,
            .step_value = 100,
        };
        out->amplitude_ratio = 1.0;
        if (property_get_bool(PROP_KEY_SIMULATE_MULTI_ZONE_AUDIO, false)) {
            out->enabled_channels = strstr(out->bus_address, AUDIO_ZONE_KEYWORD)
                ? RIGHT_CHANNEL: LEFT_CHANNEL;
            ALOGD("%s Routing %s to %s channel", __func__,
             out->bus_address, out->enabled_channels == RIGHT_CHANNEL ? "Right" : "Left");
        }
    }
    *stream_out = &out->stream;
    ALOGD("%s bus: %s", __func__, out->bus_address);

error:
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
        struct audio_stream_out *stream) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    struct generic_stream_out *out = (struct generic_stream_out *)stream;
    ALOGD("%s bus:%s", __func__, out->bus_address);
    pthread_mutex_lock(&out->lock);
    do_out_standby(out);

    out->worker_exit = true;
    pthread_cond_signal(&out->worker_wake);
    pthread_mutex_unlock(&out->lock);

    pthread_join(out->worker_thread, NULL);
    pthread_mutex_destroy(&out->lock);
    audio_vbuffer_destroy(&out->buffer);

    if (out->bus_address) {
        hashmapRemove(adev->out_bus_stream_map, out->bus_address);
        free(out->bus_address);
    }
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    struct str_parms *parms = str_parms_create_str(kvpairs);
    int value = 0;
    int results = get_int_value(parms, AAE_PARAMETER_KEY_FOR_SELECTED_ZONE, &value);
    if (results >= 0) {
        adev->last_zone_selected_to_play = value;
        results = 0;
        ALOGD("%s Changed play zone id to %d", __func__, adev->last_zone_selected_to_play);
    }
    str_parms_destroy(parms);
    pthread_mutex_unlock(&adev->lock);
    return results;
}

static char *adev_get_parameters(const struct audio_hw_device * dev, const char *keys) {
    return NULL;
}

static int adev_init_check(const struct audio_hw_device *dev) {
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume) {
    // adev_set_voice_volume is a no op (simulates phones)
    return 0;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume) {
    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev, float *volume) {
    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted) {
    ALOGD("%s: %s", __func__, _bool_str(muted));
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    adev->master_mute = muted;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    *muted = adev->master_mute;
    pthread_mutex_unlock(&adev->lock);
    ALOGD("%s: %s", __func__, _bool_str(*muted));
    return 0;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode) {
    // adev_set_mode is a no op (simulates phones)
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    adev->mic_mute = state;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    *state = adev->mic_mute;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
        const struct audio_config *config) {
    return get_input_buffer_size(config->sample_rate, config->format, config->channel_mask);
}

static void adev_close_input_stream(struct audio_hw_device *dev,
        struct audio_stream_in *stream) {
    struct generic_stream_in *in = (struct generic_stream_in *)stream;
    pthread_mutex_lock(&in->lock);
    do_in_standby(in);

    in->worker_exit = true;
    pthread_cond_signal(&in->worker_wake);
    pthread_mutex_unlock(&in->lock);
    pthread_join(in->worker_thread, NULL);

    if (in->stereo_to_mono_buf != NULL) {
        free(in->stereo_to_mono_buf);
        in->stereo_to_mono_buf_size = 0;
    }

    if (in->bus_address) {
        free(in->bus_address);
    }

    pthread_mutex_destroy(&in->lock);
    audio_vbuffer_destroy(&in->buffer);
    free(stream);
}

static void increase_next_tone_frequency(struct generic_audio_device *adev) {
    adev->next_tone_frequency_to_assign += TONE_FREQUENCY_INCREASE;
    if (adev->next_tone_frequency_to_assign > MAX_TONE_FREQUENCY) {
        adev->next_tone_frequency_to_assign = DEFAULT_FREQUENCY;
    }
}

static int create_or_fetch_tone_frequency(struct generic_audio_device *adev,
        char *address, int update_frequency) {
    int *frequency = hashmapGet(adev->in_bus_tone_frequency_map, address);
    if (frequency == NULL) {
        frequency = calloc(1, sizeof(int));
        *frequency = update_frequency;
        hashmapPut(adev->in_bus_tone_frequency_map, strdup(address), frequency);
        ALOGD("%s assigned frequency %d to %s", __func__, *frequency, address);
    }
    return *frequency;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
        audio_io_handle_t handle, audio_devices_t devices, struct audio_config *config,
        struct audio_stream_in **stream_in, audio_input_flags_t flags __unused, const char *address,
        audio_source_t source) {
    ALOGV("%s: audio_source_t: %d", __func__, source);
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    struct generic_stream_in *in;
    int ret = 0;
    if (refine_input_parameters(&config->sample_rate, &config->format, &config->channel_mask)) {
        ALOGE("Error opening input stream format %d, channel_mask %04x, sample_rate %u",
              config->format, config->channel_mask, config->sample_rate);
        ret = -EINVAL;
        goto error;
    }

    in = (struct generic_stream_in *)calloc(1, sizeof(struct generic_stream_in));
    if (!in) {
        ret = -ENOMEM;
        goto error;
    }

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;         // no op
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;                   // no op
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;       // no op
    in->stream.common.remove_audio_effect = in_remove_audio_effect; // no op
    in->stream.set_gain = in_set_gain;                              // no op
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;    // no op
    in->stream.get_capture_position = in_get_capture_position;

    pthread_mutex_init(&in->lock, (const pthread_mutexattr_t *) NULL);
    in->dev = adev;
    in->device = devices;
    memcpy(&in->req_config, config, sizeof(struct audio_config));
    memcpy(&in->pcm_config, &pcm_config_in, sizeof(struct pcm_config));
    in->pcm_config.rate = config->sample_rate;
    in->pcm_config.period_size = in->pcm_config.rate*IN_PERIOD_MS/1000;

    in->stereo_to_mono_buf = NULL;
    in->stereo_to_mono_buf_size = 0;

    in->standby = true;
    in->standby_position = 0;
    in->standby_exit_time.tv_sec = 0;
    in->standby_exit_time.tv_nsec = 0;
    in->standby_frames_read = 0;

    ret = audio_vbuffer_init(&in->buffer,
            in->pcm_config.period_size*in->pcm_config.period_count,
            in->pcm_config.channels *
            pcm_format_to_bits(in->pcm_config.format) >> 3);
    if (ret == 0) {
        pthread_cond_init(&in->worker_wake, NULL);
        in->worker_standby = true;
        in->worker_exit = false;
        pthread_create(&in->worker_thread, NULL, in_read_worker, in);
    }

    if (address) {
        in->bus_address = strdup(address);
        if (is_tone_generator_device(in)) {
            int update_frequency = adev->next_tone_frequency_to_assign;
            int frequency = create_or_fetch_tone_frequency(adev, address, update_frequency);
            if (update_frequency == frequency) {
                increase_next_tone_frequency(adev);
            }
            in->oscillator.phase = 0.0f;
            in->oscillator.phase_increment = (TWO_PI*(frequency))
                / ((float) in_get_sample_rate(&in->stream.common));
        }
    }

    *stream_in = &in->stream;

error:
    return ret;
}

static int adev_dump(const audio_hw_device_t *dev, int fd) {
    return 0;
}

static int adev_set_audio_port_config(struct audio_hw_device *dev,
        const struct audio_port_config *config) {
    int ret = 0;
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    const char *bus_address = config->ext.device.address;
    struct generic_stream_out *out = hashmapGet(adev->out_bus_stream_map, bus_address);
    if (out) {
        pthread_mutex_lock(&out->lock);
        int gainIndex = (config->gain.values[0] - out->gain_stage.min_value) /
            out->gain_stage.step_value;
        int totalSteps = (out->gain_stage.max_value - out->gain_stage.min_value) /
            out->gain_stage.step_value;
        int minDb = out->gain_stage.min_value / 100;
        int maxDb = out->gain_stage.max_value / 100;
        // curve: 10^((minDb + (maxDb - minDb) * gainIndex / totalSteps) / 20)
        out->amplitude_ratio = pow(10,
                (minDb + (maxDb - minDb) * (gainIndex / (float)totalSteps)) / 20);
        pthread_mutex_unlock(&out->lock);
        ALOGD("%s: set audio gain: %f on %s",
                __func__, out->amplitude_ratio, bus_address);
    } else {
        ALOGE("%s: can not find output stream by bus_address:%s", __func__, bus_address);
        ret = -EINVAL;
    }
    return ret;
}

static int adev_create_audio_patch(struct audio_hw_device *dev,
        unsigned int num_sources,
        const struct audio_port_config *sources,
        unsigned int num_sinks,
        const struct audio_port_config *sinks,
        audio_patch_handle_t *handle) {
    struct generic_audio_device *audio_dev = (struct generic_audio_device *)dev;
    for (int i = 0; i < num_sources; i++) {
        ALOGD("%s: source[%d] type=%d address=%s", __func__, i, sources[i].type,
                sources[i].type == AUDIO_PORT_TYPE_DEVICE
                ? sources[i].ext.device.address
                : "");
    }
    for (int i = 0; i < num_sinks; i++) {
        ALOGD("%s: sink[%d] type=%d address=%s", __func__, i, sinks[i].type,
                sinks[i].type == AUDIO_PORT_TYPE_DEVICE ? sinks[i].ext.device.address
                : "N/A");
    }
    if (num_sources == 1 && num_sinks == 1 &&
            sources[0].type == AUDIO_PORT_TYPE_DEVICE &&
            sinks[0].type == AUDIO_PORT_TYPE_DEVICE) {
        pthread_mutex_lock(&audio_dev->lock);
        audio_dev->last_patch_id += 1;
        pthread_mutex_unlock(&audio_dev->lock);
        *handle = audio_dev->last_patch_id;
        ALOGD("%s: handle: %d", __func__, *handle);
    }
    return 0;
}

static int adev_release_audio_patch(struct audio_hw_device *dev,
        audio_patch_handle_t handle) {
    ALOGD("%s: handle: %d", __func__, handle);
    return 0;
}

static int adev_close(hw_device_t *dev) {
    struct generic_audio_device *adev = (struct generic_audio_device *)dev;
    int ret = 0;
    if (!adev)
        return 0;

    pthread_mutex_lock(&adev_init_lock);

    if (audio_device_ref_count == 0) {
        ALOGE("adev_close called when ref_count 0");
        ret = -EINVAL;
        goto error;
    }

    if ((--audio_device_ref_count) == 0) {
        if (adev->mixer) {
            mixer_close(adev->mixer);
        }
        if (adev->out_bus_stream_map) {
            hashmapFree(adev->out_bus_stream_map);
        }
        if (adev->in_bus_tone_frequency_map) {
            hashmapFree(adev->in_bus_tone_frequency_map);
        }
        free(adev);
    }

error:
    pthread_mutex_unlock(&adev_init_lock);
    return ret;
}

/* copied from libcutils/str_parms.c */
static bool str_eq(void *key_a, void *key_b) {
    return !strcmp((const char *)key_a, (const char *)key_b);
}

/**
 * use djb hash unless we find it inadequate.
 * copied from libcutils/str_parms.c
 */
#ifdef __clang__
__attribute__((no_sanitize("integer")))
#endif
static int str_hash_fn(void *str) {
    uint32_t hash = 5381;
    char *p;
    for (p = str; p && *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }
    return (int)hash;
}

static int adev_open(const hw_module_t *module,
        const char *name, hw_device_t **device) {
    static struct generic_audio_device *adev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    pthread_mutex_lock(&adev_init_lock);
    if (audio_device_ref_count != 0) {
        *device = &adev->device.common;
        audio_device_ref_count++;
        ALOGV("%s: returning existing instance of adev", __func__);
        ALOGV("%s: exit", __func__);
        goto unlock;
    }
    adev = calloc(1, sizeof(struct generic_audio_device));

    pthread_mutex_init(&adev->lock, (const pthread_mutexattr_t *) NULL);

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_3_0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;               // no op
    adev->device.set_voice_volume = adev_set_voice_volume;   // no op
    adev->device.set_master_volume = adev_set_master_volume; // no op
    adev->device.get_master_volume = adev_get_master_volume; // no op
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;                   // no op
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;       // no op
    adev->device.get_parameters = adev_get_parameters;       // no op
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    // New in AUDIO_DEVICE_API_VERSION_3_0
    adev->device.set_audio_port_config = adev_set_audio_port_config;
    adev->device.create_audio_patch = adev_create_audio_patch;
    adev->device.release_audio_patch = adev_release_audio_patch;

    *device = &adev->device.common;

    adev->mixer = mixer_open(PCM_CARD);

    ALOGD("%s Mixer name %s", __func__, mixer_get_name(adev->mixer));
    struct mixer_ctl *ctl;

    // Set default mixer ctls
    // Enable channels and set volume
    for (int i = 0; i < (int)mixer_get_num_ctls(adev->mixer); i++) {
        ctl = mixer_get_ctl(adev->mixer, i);
        ALOGD("mixer %d name %s", i, mixer_ctl_get_name(ctl));
        if (!strcmp(mixer_ctl_get_name(ctl), "Master Playback Volume") ||
            !strcmp(mixer_ctl_get_name(ctl), "Capture Volume")) {
            for (int z = 0; z < (int)mixer_ctl_get_num_values(ctl); z++) {
                ALOGD("set ctl %d to %d", z, 100);
                mixer_ctl_set_percent(ctl, z, 100);
            }
            continue;
        }
        if (!strcmp(mixer_ctl_get_name(ctl), "Master Playback Switch") ||
            !strcmp(mixer_ctl_get_name(ctl), "Capture Switch")) {
            for (int z = 0; z < (int)mixer_ctl_get_num_values(ctl); z++) {
                ALOGD("set ctl %d to %d", z, 1);
                mixer_ctl_set_value(ctl, z, 1);
            }
            continue;
        }
    }

    // Initialize the bus address to output stream map
    adev->out_bus_stream_map = hashmapCreate(5, str_hash_fn, str_eq);

    // Initialize the bus address to input stream map
    adev->in_bus_tone_frequency_map = hashmapCreate(5, str_hash_fn, str_eq);

    adev->next_tone_frequency_to_assign = DEFAULT_FREQUENCY;

    adev->last_zone_selected_to_play = DEFAULT_ZONE_TO_LEFT_SPEAKER;

    audio_device_ref_count++;

unlock:
    pthread_mutex_unlock(&adev_init_lock);
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Generic car audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
