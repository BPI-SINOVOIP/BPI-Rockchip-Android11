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

#ifndef _AUDIO_H_
#define _AUDIO_H_

#define PCM_OUT        0x00000000
#define PCM_IN         0x10000000

#define PCM_STEREO     0x00000000
#define PCM_MONO       0x01000000

#define PCM_44100HZ    0x00000000
#define PCM_48000HZ    0x00100000
#define PCM_8000HZ     0x00200000
#define PCM_RATE_MASK  0x00F00000

#define PCM_DEVICE0      0x00000000
#define PCM_DEVICE1      0x00000010
#define PCM_DEVICE2      0x00000020
#define PCM_DEVICE_MASK  0x000000F0
#define PCM_DEVICE_SHIFT 4

#define PCM_CARD0      0x00000000
#define PCM_CARD1      0x00000001
#define PCM_CARD2      0x00000002
#define PCM_CARD_MASK  0x0000000F
#define PCM_CARD_SHIFT 0

#define PCM_PERIOD_CNT_MIN 3
#define PCM_PERIOD_CNT_SHIFT 16
#define PCM_PERIOD_CNT_MASK (0xF << PCM_PERIOD_CNT_SHIFT)
#define PCM_PERIOD_SZ_MIN 64
#define PCM_PERIOD_SZ_SHIFT 12
#define PCM_PERIOD_SZ_MASK (0xF << PCM_PERIOD_SZ_SHIFT)

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

    USB_NORMAL_ROUTE, // 29
    USB_CAPTURE_ROUTE,

    MAX_ROUTE, // 31
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

/* Acquire/release a pcm channel.
 * Returns non-zero on error
 */
struct pcm *pcm_open(unsigned flags);
int pcm_close(struct pcm *pcm);
int pcm_ready(struct pcm *pcm);

/* Returns a human readable reason for the last error. */
const char *pcm_error(struct pcm *pcm);

/* Returns the buffer size (int bytes) that should be used for pcm_write.
 * This will be 1/2 of the actual fifo size.
 */
unsigned pcm_buffer_size(struct pcm *pcm);

/* Write data to the fifo.
 * Will start playback on the first write or on a write that
 * occurs after a fifo underrun.
 */
int pcm_write(struct pcm *pcm, void *data, unsigned count);
int pcm_read(struct pcm *pcm, void *data, unsigned count);

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

struct mixer *mixer_open(unsigned card);
void mixer_close(struct mixer *mixer);
void mixer_dump(struct mixer *mixer);

struct mixer_ctl *mixer_get_control(struct mixer *mixer,
                                    const char *name, unsigned index);
struct mixer_ctl *mixer_get_nth_control(struct mixer *mixer, unsigned n);

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

int route_init(void);
void route_uninit(void);
int route_set_input_source(const char *source);
int route_set_voice_volume(const char *ctlName, float volume);
int route_set_controls(unsigned route);
struct pcm *route_pcm_open(unsigned route, unsigned int flags);
int route_pcm_close(unsigned route);
#endif
