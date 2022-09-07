/*
** Copyright 2010, The Android Open-Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/**
 * @file alsa_audio.h
 * @brief
 * @author  RkAudio
 * @version 1.0.8
 * @date 2015-08-24
 */

#ifndef _AUDIO_H_
#define _AUDIO_H_
#ifdef __cplusplus
extern "C"{
#endif
typedef enum _AudioRoute {
    SPEAKER_NORMAL_ROUTE = 0,
    SPEAKER_INCALL_ROUTE, // 1
    SPEAKER_RINGTONE_ROUTE,
    SPEAKER_VOIP_ROUTE,

    EARPIECE_NORMAL_ROUTE, // 4
    EARPIECE_INCALL_ROUTE,
    EARPIECE_RINGTONE_ROUTE,
    EARPIECE_VOIP_ROUTE,

    HEADPHONE_NORMAL_ROUTE, // 8
    HEADPHONE_INCALL_ROUTE,
    HEADPHONE_RINGTONE_ROUTE,
    SPEAKER_HEADPHONE_NORMAL_ROUTE,
    SPEAKER_HEADPHONE_RINGTONE_ROUTE,
    HEADPHONE_VOIP_ROUTE,

    HEADSET_NORMAL_ROUTE, // 14
    HEADSET_INCALL_ROUTE,
    HEADSET_RINGTONE_ROUTE,
    HEADSET_VOIP_ROUTE,

    BLUETOOTH_NORMAL_ROUTE, // 18
    BLUETOOTH_INCALL_ROUTE,
    BLUETOOTH_VOIP_ROUTE,

    MAIN_MIC_CAPTURE_ROUTE, // 21
    HANDS_FREE_MIC_CAPTURE_ROUTE,
    BLUETOOTH_SOC_MIC_CAPTURE_ROUTE,

    PLAYBACK_OFF_ROUTE, // 24
    CAPTURE_OFF_ROUTE,
    INCALL_OFF_ROUTE,
    VOIP_OFF_ROUTE,

    HDMI_NORMAL_ROUTE, // 28

    SPDIF_NORMAL_ROUTE,

    USB_NORMAL_ROUTE, // 30
    USB_CAPTURE_ROUTE,

    HDMI_IN_NORMAL_ROUTE,
    HDMI_IN_OFF_ROUTE,
    HDMI_IN_CAPTURE_ROUTE,
    HDMI_IN_CAPTURE_OFF_ROUTE,

    MAX_ROUTE, //36
} AudioRoute;

#define PCM_ERROR_MAX 128

struct pcm {
    int fd;
    unsigned flags;
    int running:1;
    int underruns;
    unsigned buffer_size;
    char error[PCM_ERROR_MAX];
};

struct mixer_ctl {
    struct mixer *mixer;
    struct snd_ctl_elem_info *info;
    struct snd_ctl_tlv *tlv;
    char **ename;
};

struct mixer {
    int fd;
    struct snd_ctl_elem_info *info;
    struct mixer_ctl *ctl;
    unsigned count;
};

struct mixer *mixer_open_legacy(unsigned card);
void mixer_close_legacy(struct mixer *mixer);
void mixer_dump(struct mixer *mixer);

struct mixer_ctl *mixer_get_control(struct mixer *mixer,
                                    const char *name, unsigned index);
struct mixer_ctl *mixer_get_nth_control(struct mixer *mixer, unsigned n);

int mixer_ctl_set_val(struct mixer_ctl *ctl,int value);
int mixer_ctl_set(struct mixer_ctl *ctl, unsigned percent);
int mixer_ctl_select(struct mixer_ctl *ctl, const char *value);
void mixer_ctl_print(struct mixer_ctl *ctl);
int mixer_ctl_set_int_double(struct mixer_ctl *ctl, long long left, long long right);
int mixer_ctl_set_int(struct mixer_ctl *ctl, long long value);
int mixer_tlv_get_dB_range(unsigned int *tlv, long rangemin, long rangemax,
                           long *min, long *max);
int mixer_get_ctl_minmax(struct mixer_ctl *ctl, long long *min, long long *max);
int mixer_get_dB_range(struct mixer_ctl *ctl, long rangemin, long rangemax,
                       float *dB_min, float *dB_max, float *dB_step);

int route_card_init(void **pproute_data, int card);
int route_set_controls(void *proute_data, unsigned route);
void route_uninit(void *proute_data);
int is_playback_route(unsigned route);
void route_pcm_card_open(void **pproute_data, int card, unsigned route);
int route_pcm_close(void *proute_data, unsigned route);
#ifdef __cplusplus
}
#endif
#endif
