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
 * parse HDMI's EDID for get the information of support's format,samplerate,channel and channeMask
 * the driver of hdmi not give the interface how to get audio information of audio(hdmi),so we
 * parse the edid myself to get the inforamtions.
 * this parse codes is following CEA-861
 */

#ifndef AUIDO_HDMI_AUIOD_INFORMATION_PARSER_H
#define AUIDO_HDMI_AUIOD_INFORMATION_PARSER_H


#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <system/audio.h>
#include <pthread.h>


/* HDMI Audio Sample Rate */
enum hdmi_audio_samplerate {
    HDMI_AUDIO_FS_32000  = (1<<0),
    HDMI_AUDIO_FS_44100  = (1<<1),
    HDMI_AUDIO_FS_48000  = (1<<2),
    HDMI_AUDIO_FS_88200  = (1<<3),
    HDMI_AUDIO_FS_96000  = (1<<4),
    HDMI_AUDIO_FS_176400 = (1<<5),
    HDMI_AUDIO_FS_192000 = (1<<6)
};

/* HDMI Audio Word Length */
enum hdmi_audio_word_length {
    HDMI_AUDIO_WORD_LENGTH_16bit = 0x1,
    HDMI_AUDIO_WORD_LENGTH_20bit = 0x2,
    HDMI_AUDIO_WORD_LENGTH_24bit = 0x4
};

enum hdmi_audio_type {
    HDMI_AUDIO_NLPCM = 0,
    HDMI_AUDIO_LPCM = 1,    /* PCM */
    HDMI_AUDIO_AC3,
    HDMI_AUDIO_MPEG1,
    HDMI_AUDIO_MP3,
    HDMI_AUDIO_MPEG2,
    HDMI_AUDIO_AAC_LC,      /*AAC */
    HDMI_AUDIO_DTS,
    HDMI_AUDIO_ATARC,
    HDMI_AUDIO_DSD,
    HDMI_AUDIO_E_AC3 = 10,
    HDMI_AUDIO_DTS_HD,
    HDMI_AUDIO_MLP,        /*Dolby TrueHD and Dolby MAT */
    HDMI_AUDIO_DST,
    HDMI_AUDIO_WMA_PRO,

    HDMI_AUDIO_FORMAT_INVALID = 0xff
};

struct hdmi_audio_information {
    unsigned char type;
    unsigned char channel;
    unsigned char sample;            // index of sample, every bit mean a samplerate
    unsigned char value;             // Audio coding type dependent value, some audio codec type using the same type(Dolby TrueHD and Dolby MAT, AC4 ande EAC3),
                                     // so need this value judge which type supported?
    unsigned char word_length;       // support pcm word length, only valid when type = HDMI_AUDIO_LPCM
    unsigned int  max_bitrate;       // support max bitrates, only valid when type belong to [HDMI_AUDIO_AC3,HDMI_AUDIO_ATARC]
};

struct hdmi_audio_infors {
    pthread_mutex_t lock;
    int number;
    int channel_layout; // the layout of speaker, only valid when type = HDMI_AUDIO_LPCM
    struct hdmi_audio_information* audio;
};

extern void init_hdmi_audio(struct hdmi_audio_infors *infor);
extern int parse_hdmi_audio(struct hdmi_audio_infors *audios);
extern int get_hdmi_audio_speaker_allocation(struct hdmi_audio_infors *infor);
extern bool is_support_format(struct hdmi_audio_infors *infor,audio_format_t format);
extern void destory_hdmi_audio(struct hdmi_audio_infors *infor);
extern void dump(struct hdmi_audio_infors *infor);
#endif
