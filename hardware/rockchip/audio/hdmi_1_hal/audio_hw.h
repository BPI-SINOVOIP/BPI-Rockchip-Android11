/*
 * Copyright (C) 2012 The Android Open Source Project
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
 * @file audio_hw.h
 * @brief
 *                 ALSA Audio Git Log
 * - V0.1.0:add alsa audio hal,just support 312x now.
 * - V0.2.0:remove unused variable.
 * - V0.3.0:turn off device when do_standby.
 * - V0.4.0:turn off device before open pcm.
 * - V0.4.1:Need to re-open the control to fix no sound when suspend.
 * - V0.5.0:Merge the mixer operation from legacy_alsa.
 * - V0.6.0:Merge speex denoise from legacy_alsa.
 * - V0.7.0:add copyright.
 * - V0.7.1:add support for box audio
 * - V0.7.2:add support for dircet output
 * - V0.8.0:update the direct output for box, add the DVI mode
 * - V1.0.0:stable version
 *
 * @author  RkAudio
 * @version 1.0.8
 * @date 2015-08-24
 */

#ifndef AUIDO_HW_H
#define AUIDO_HW_H
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>

#include <hardware/audio.h>
#include <hardware/hardware.h>

#include <linux/videodev2.h>

#include <system/audio.h>

#include "asoundlib.h"

#include <audio_utils/resampler.h>
#include <audio_route/audio_route.h>

#include <poll.h>
#include <linux/fb.h>
#include <hardware_legacy/uevent.h>

#include "voice_preprocess.h"
#include "audio_hw_hdmi.h"
#include "denoise/rkdenoise.h"
#include "bitstream/audio_bitstream_manager.h"

#define AUDIO_HAL_VERSION "ALSA Audio Version: V1.1.0"

#define PCM_DEVICE 0
#define PCM_DEVICE_SCO 1
#define PCM_DEVICE_VOICE 2
#define PCM_DEVICE_HDMIIN 2
#define PCM_DEVICE_DEEP 3
/* for bt client call */
#define PCM_DEVICE_HFP 1

#define MIXER_CARD 0

/* duration in ms of volume ramp applied when starting capture to remove plop */
#define CAPTURE_START_RAMP_MS 100

/* default sampling for default output */
#define DEFAULT_PLAYBACK_SAMPLERATE 44100

#define DEFAULT_PLAYBACK_CHANNELS 2

/* default sampling for HDMI multichannel output */
#define HDMI_MULTI_DEFAULT_SAMPLING_RATE  44100
/* maximum number of channel mask configurations supported. Currently the primary
 * output only supports 1 (stereo) and the multi channel HDMI output 2 (5.1 and 7.1) */
#define MAX_SUPPORTED_CHANNEL_MASKS 2
#define MAX_SUPPORTED_SAMPLE_RATES 2

#ifndef BOX_HAL
#define RK_DENOISE_ENABLE
#endif

#define HW_PARAMS_FLAG_LPCM 0
#define HW_PARAMS_FLAG_NLPCM 1

#ifdef BOX_HAL
struct pcm_config pcm_config = {
    .channels = 2,
    .rate = 44100,
    .period_size = 512,
    .period_count = 3,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_in = {
    .channels = 2,
    .rate = 44100,
    .period_size = 1024,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};
#elif defined RK3399_LAPTOP
struct pcm_config pcm_config = {
    .channels = 2,
    .rate = 48000,
    .period_size = 480,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_in = {
    .channels = 2,
    .rate = 48000,
    .period_size = 120,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};
#else
struct pcm_config pcm_config = {
    .channels = 2,
    .rate = 44100,
    .period_size = 512,
    .period_count = 6,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_in = {
    .channels = 2,
    .rate = 44100,
#ifdef RK_DENOISE_ENABLE
    .period_size = 441,
#else
    .period_size = 256,
#endif
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};
#endif

struct pcm_config pcm_config_in_low_latency = {
    .channels = 2,
    .rate = 44100,
    .period_size = 256,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_sco = {
    .channels = 1,
    .rate = 8000,
    .period_size = 128,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};

/* for bt client call*/
struct pcm_config pcm_config_hfp = {
    .channels = 2,
    .rate = 44100,
    .period_size = 256,
    .period_count = 4,
};
#ifdef BT_AP_SCO
struct pcm_config pcm_config_ap_sco = {
    .channels = 2,
    .rate = 8000,
    .period_size = 80,
    .period_count = 4,
};

struct pcm_config pcm_config_in_bt = {
    .channels = 2,
    .rate = 8000,
    .period_size = 120,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};
#endif
struct pcm_config pcm_config_deep = {
    .channels = 2,
    .rate = 44100,
    /* FIXME This is an arbitrary number, may change.
     * Dynamic configuration based on screen on/off is not implemented;
     * let's see what power consumption is first to see if necessary.
     */
    .period_size = 8192,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_hdmi_multi = {
    .channels = 6, /* changed when the stream is opened */
    .rate = HDMI_MULTI_DEFAULT_SAMPLING_RATE,
    .period_size = 1024,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_direct = {
    .channels = 2,
    .rate = 48000,
    .period_size = 1024,
    .period_count = 4,

#ifdef IEC958_FORAMT
    .format = PCM_FORMAT_IEC958_SUBFRAME_LE,
#else
    .format = PCM_FORMAT_S24_LE,
#endif
};

enum output_type {
    OUTPUT_DEEP_BUF,      // deep PCM buffers output stream
    OUTPUT_LOW_LATENCY,   // low latency output stream
    OUTPUT_HDMI_MULTI,          // HDMI multi channel
    OUTPUT_DIRECT,
    OUTPUT_TOTAL
};

struct direct_mode_t {
    int output_mode;
    char* hbr_Buf;
};

enum snd_out_sound_cards {
    SND_OUT_SOUND_CARD_UNKNOWN = -1,
    SND_OUT_SOUND_CARD_SPEAKER = 0,
    SND_OUT_SOUND_CARD_HDMI,
    SND_OUT_SOUND_CARD_SPDIF,
    SND_OUT_SOUND_CARD_BT,
    SND_OUT_SOUND_CARD_MAX,
};

enum snd_in_sound_cards {
    SND_IN_SOUND_CARD_UNKNOWN = -1,
    SND_IN_SOUND_CARD_MIC = 0,
    SND_IN_SOUND_CARD_BT,
    SND_IN_SOUND_CARD_HDMI,
    SND_IN_SOUND_CARD_MAX,
};

struct dev_proc_info
{
    const char *cid; /* cardX/id match */
    const char *did; /* dai id match */
};

struct dev_info
{
    const char *id;
    int card;
    int device;
    int score;
};

struct audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    audio_devices_t out_device; /* "or" of stream_out.device for all active output streams */
    audio_devices_t in_device;
    bool mic_mute;
    struct audio_route *ar;
    audio_source_t input_source;
    audio_channel_mask_t in_channel_mask;

    struct stream_out *outputs[OUTPUT_TOTAL];
    pthread_mutex_t lock_outputs; /* see note below on mutex acquisition order */
    unsigned int mode;
    bool   screenOff;
#ifdef AUDIO_3A
    rk_process_api* voice_api;
#endif

    /*
     * hh@rock-chips.com
     * this is for HDMI/SPDIF bitstream
     * when HDMI/SPDIF bistream AC3/EAC3/DTS/TRUEHD/DTS-HD, some key tone or other pcm
     * datas may come(play a Ac3 audio and seek the file to play). It is not allow to open sound card
     * as pcm format and not allow to write pcm datas to HDMI/SPDIF sound cards when open it
     * with config.flag = 1.
     */
    int*  owner;

    struct dev_info dev_out;
    struct dev_info dev_in;
};

struct stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct pcm *pcm;
    struct pcm_config config;
    struct audio_config aud_config;
    unsigned int pcm_device;
    bool standby; /* true if all PCMs are inactive */
    audio_devices_t device;
    /* FIXME: when HDMI multichannel output is active, other outputs must be disabled as
     * HDMI and WM1811 share the same I2S. This means that notifications and other sounds are
     * silent when watching a 5.1 movie. */
    bool disabled;
    audio_channel_mask_t channel_mask;
    /* Array of supported channel mask configurations. +1 so that the last entry is always 0 */
    audio_channel_mask_t supported_channel_masks[MAX_SUPPORTED_CHANNEL_MASKS + 1];
    uint32_t supported_sample_rates[MAX_SUPPORTED_SAMPLE_RATES + 1];
    bool muted;
    uint64_t written; /* total frames written, not cleared when entering standby */
    uint64_t nframes;

    /*
     * true: current stream take sound card in exclusive Mode, when this stream using this sound card,
     *       other stream can't using this sound card. This happen when current stream
     *       is multi pcm stream or bitstream. Multi channels pcm datas or bitstream datas can't be mixed.
     * false: current stream is 2 channels pcm stream
     */
    bool output_direct;

    /*
     * LPCM:  pcm datas(include multi channels pcm)
     * others: bitstream
     */
    int output_direct_mode;
    struct audio_device *dev;
    struct resampler_itfe *resampler;

    // for hdmi bitstream
    rk_bistream *bistream;

    struct hdmi_audio_infors hdmi_audio;

    bool  snd_reopen;
    bool  use_default_config;
    float volume[2];
};

struct stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */
    struct pcm *pcm;
    bool standby;

    unsigned int requested_rate;
    struct resampler_itfe *resampler;
    struct resampler_buffer_provider buf_provider;
    int16_t *buffer;
    size_t frames_in;
    int read_status;
    audio_source_t input_source;
    audio_io_handle_t io_handle;
    audio_devices_t device;
    uint16_t ramp_vol;
    uint16_t ramp_step;
    size_t  ramp_frames;
    audio_channel_mask_t channel_mask;
    audio_input_flags_t flags;
    struct pcm_config *config;

    struct audio_device *dev;
    audio_channel_mask_t supported_channel_masks[MAX_SUPPORTED_CHANNEL_MASKS + 1];
    uint32_t supported_sample_rates[MAX_SUPPORTED_SAMPLE_RATES + 1];
#ifdef RK_DENOISE_ENABLE
    hrkdeniose mDenioseState;
#endif
    uint32_t channel_flag;
    int start_checkcount;
    uint64_t frames_read;
};

#define STRING_TO_ENUM(string) { #string, string }

struct string_to_enum {
    const char *name;
    uint32_t value;
};

static const struct string_to_enum channels_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_MONO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_5POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_7POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_IN_MONO),
    STRING_TO_ENUM(AUDIO_CHANNEL_IN_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_IN_FRONT_BACK),
};

enum {
    OUT_DEVICE_SPEAKER,
    OUT_DEVICE_HEADSET,
    OUT_DEVICE_HEADPHONES,
    OUT_DEVICE_BT_SCO,
    OUT_DEVICE_SPEAKER_AND_HEADSET,
    OUT_DEVICE_OFF,
    OUT_DEVICE_TAB_SIZE,           /* number of rows in route_configs[][] */
    OUT_DEVICE_NONE,
    OUT_DEVICE_CNT
};

enum {
    IN_SOURCE_MIC,
    IN_SOURCE_CAMCORDER,
    IN_SOURCE_VOICE_RECOGNITION,
    IN_SOURCE_VOICE_COMMUNICATION,
    IN_SOURCE_OFF,
    IN_SOURCE_TAB_SIZE,            /* number of lines in route_configs[][] */
    IN_SOURCE_NONE,
    IN_SOURCE_CNT
};

enum {
    LPCM = 0,
    NLPCM,
    HBR,
};

struct route_config {
    const char * const output_route;
    const char * const input_route;
    const char * const output_off;
    const char * const input_off;
};

const struct route_config media_speaker = {
    "media-speaker",
    "media-main-mic",
    "playback-off",
    "capture-off",
};

const struct route_config media_headphones = {
    "media-headphones",
    "media-main-mic",
    "playback-off",
    "capture-off",
};

const struct route_config media_headset = {
    "media-headphones",
    "media-headset-mic",
    "playback-off",
    "capture-off",
};

const struct route_config camcorder_speaker = {
    "media-speaker",
    "media-second-mic",
    "playback-off",
    "capture-off",
};

const struct route_config camcorder_headphones = {
    "media-headphones",
    "media-second-mic",
    "playback-off",
    "capture-off",
};

const struct route_config voice_rec_speaker = {
    "voice-rec-speaker",
    "voice-rec-main-mic",
    "incall-off",
    "incall-off",
};

const struct route_config voice_rec_headphones = {
    "voice-rec-headphones",
    "voice-rec-main-mic",
    "incall-off",
    "incall-off",
};

const struct route_config voice_rec_headset = {
    "voice-rec-headphones",
    "voice-rec-headset-mic",
    "incall-off",
    "incall-off",
};

const struct route_config communication_speaker = {
    "communication-speaker",
    "communication-main-mic",
    "voip-off",
    "voip-off",
};

const struct route_config communication_headphones = {
    "communication-headphones",
    "communication-main-mic",
    "voip-off",
    "voip-off",
};

const struct route_config communication_headset = {
    "communication-headphones",
    "communication-headset-mic",
    "voip-off",
    "voip-off",
};

const struct route_config speaker_and_headphones = {
    "speaker-and-headphones",
    "main-mic",
    "playback-off",
    "capture-off",
};

const struct route_config bluetooth_sco = {
    "bt-sco-headset",
    "bt-sco-mic",
    "playback-off",
    "capture-off",
};

const struct route_config * const route_configs[IN_SOURCE_TAB_SIZE]
        [OUT_DEVICE_TAB_SIZE] = {
    {   /* IN_SOURCE_MIC */
        &media_speaker,             /* OUT_DEVICE_SPEAKER */
        &media_headset,             /* OUT_DEVICE_HEADSET */
        &media_headphones,          /* OUT_DEVICE_HEADPHONES */
        &bluetooth_sco,             /* OUT_DEVICE_BT_SCO */
        &speaker_and_headphones     /* OUT_DEVICE_SPEAKER_AND_HEADSET */
    },
    {   /* IN_SOURCE_CAMCORDER */
        &camcorder_speaker,         /* OUT_DEVICE_SPEAKER */
        &camcorder_headphones,      /* OUT_DEVICE_HEADSET */
        &camcorder_headphones,      /* OUT_DEVICE_HEADPHONES */
        &bluetooth_sco,             /* OUT_DEVICE_BT_SCO */
        &speaker_and_headphones     /* OUT_DEVICE_SPEAKER_AND_HEADSET */
    },
    {   /* IN_SOURCE_VOICE_RECOGNITION */
        &voice_rec_speaker,         /* OUT_DEVICE_SPEAKER */
        &voice_rec_headset,         /* OUT_DEVICE_HEADSET */
        &voice_rec_headphones,      /* OUT_DEVICE_HEADPHONES */
        &bluetooth_sco,             /* OUT_DEVICE_BT_SCO */
        &speaker_and_headphones     /* OUT_DEVICE_SPEAKER_AND_HEADSET */
    },
    {   /* IN_SOURCE_VOICE_COMMUNICATION */
        &communication_speaker,     /* OUT_DEVICE_SPEAKER */
        &communication_headset,     /* OUT_DEVICE_HEADSET */
        &communication_headphones,  /* OUT_DEVICE_HEADPHONES */
        &bluetooth_sco,             /* OUT_DEVICE_BT_SCO */
        &speaker_and_headphones     /* OUT_DEVICE_SPEAKER_AND_HEADSET */
    }
};

static void do_out_standby(struct stream_out *out);
#endif

