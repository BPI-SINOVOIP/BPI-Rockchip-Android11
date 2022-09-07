/*
** open sound card and route, add by Jear.Chen.
*/

#define LOG_TAG "alsa_route"

#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <cutils/config_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

#include <linux/ioctl.h>
#include "alsa_audio.h"

#define __force
#define __bitwise
#define __user
#include "../asound.h"

#include "../codec_config/config_list.h"

#define PCM_DEVICE0_PLAYBACK 0
#define PCM_DEVICE0_CAPTURE 1
#define PCM_DEVICE1_PLAYBACK 2
#define PCM_DEVICE1_CAPTURE 3
#define PCM_DEVICE2_PLAYBACK 4
#define PCM_DEVICE2_CAPTURE 5

#define PCM_MAX PCM_DEVICE2_CAPTURE

const struct config_route_table *route_table;

struct pcm* mPcm[PCM_MAX + 1];
struct mixer* mMixerPlayback;
struct mixer* mMixerCapture;

int route_init(void)
{
    char soundCardID[20] = "";
    static FILE * fp;
    unsigned i, config_count = sizeof(sound_card_config_list) / sizeof(struct alsa_sound_card_config);
    size_t read_size;

    printf("route_init()\n");
    printf("################################!\n");
    fp = fopen("/proc/asound/card0/id", "rt");
    if (!fp) {
        printf("Open sound card0 id error!\n");
    } else {

	printf("################################!\n");
        read_size = fread(soundCardID, sizeof(char), sizeof(soundCardID), fp);
        fclose(fp);

        if (soundCardID[read_size - 1] == '\n') {
            read_size--;
            soundCardID[read_size] = '\0';
        }

        printf("Sound card0 is %s\n", soundCardID);

        for (i = 0; i < config_count; i++) {
            if (!(sound_card_config_list + i) || !sound_card_config_list[i].sound_card_name ||
                !sound_card_config_list[i].route_table)
                continue;

		printf("SOUND_CARD_NAME:%s\n",sound_card_config_list[i].sound_card_name);
            if (strncmp(sound_card_config_list[i].sound_card_name, soundCardID, 
                read_size) == 0) {
                route_table = sound_card_config_list[i].route_table;
                printf("Get route table for sound card0 %s\n", soundCardID);
            }
        }
    }

    if (!route_table) {
        route_table = &default_config_table;
        ALOGD("Can not get config table for sound card0 %s, so get default config table.", soundCardID);
    }

    for (i = PCM_DEVICE0_PLAYBACK; i < PCM_MAX; i++)
         mPcm[i] = NULL;

    return 0;
}

void route_uninit(void)
{
    ALOGV("route_uninit()");

    if (mPcm[PCM_DEVICE0_PLAYBACK]) {
        route_pcm_close(PLAYBACK_OFF_ROUTE);
    }

    if (mPcm[PCM_DEVICE0_CAPTURE]) {
        route_pcm_close(CAPTURE_OFF_ROUTE);
    }
}

int is_playback_route(unsigned route)
{
    switch (route) {
    case MAIN_MIC_CAPTURE_ROUTE:
    case HANDS_FREE_MIC_CAPTURE_ROUTE:
    case BLUETOOTH_SOC_MIC_CAPTURE_ROUTE:
    case CAPTURE_OFF_ROUTE:
    case USB_CAPTURE_ROUTE:
        return 0;
    case SPEAKER_NORMAL_ROUTE:
    case SPEAKER_INCALL_ROUTE:
    case SPEAKER_RINGTONE_ROUTE:
    case SPEAKER_VOIP_ROUTE:
    case EARPIECE_NORMAL_ROUTE:
    case EARPIECE_INCALL_ROUTE:
    case EARPIECE_RINGTONE_ROUTE:
    case EARPIECE_VOIP_ROUTE:
    case HEADPHONE_NORMAL_ROUTE:
    case HEADPHONE_INCALL_ROUTE:
    case HEADPHONE_RINGTONE_ROUTE:
    case SPEAKER_HEADPHONE_NORMAL_ROUTE:
    case SPEAKER_HEADPHONE_RINGTONE_ROUTE:
    case HEADPHONE_VOIP_ROUTE:
    case HEADSET_NORMAL_ROUTE:
    case HEADSET_INCALL_ROUTE:
    case HEADSET_RINGTONE_ROUTE:
    case HEADSET_VOIP_ROUTE:
    case BLUETOOTH_NORMAL_ROUTE:
    case BLUETOOTH_INCALL_ROUTE:
    case BLUETOOTH_VOIP_ROUTE:
    case PLAYBACK_OFF_ROUTE:
    case INCALL_OFF_ROUTE:
    case VOIP_OFF_ROUTE:
    case HDMI_NORMAL_ROUTE:
    case USB_NORMAL_ROUTE:
        return 1;
    default:
        ALOGE("is_playback_route() Error route %d", route);
        return -EINVAL;
    }
}

int route_set_input_source(const char *source)
{
    struct mixer* mMixer = mMixerCapture;

    if (mMixer == NULL || source[0] == '\0') return 0;

    struct mixer_ctl *ctl= mixer_get_control(mMixer, "Input Source", 0);

    if (ctl == NULL)
        return 0;

    ALOGV("mixer_ctl_select, Input Source, (%s)", source);
    return mixer_ctl_select(ctl, source);
}

int route_set_voice_volume(const char *ctlName, float volume)
{
    struct mixer* mMixer = mMixerPlayback;

    if (mMixer == NULL || ctlName[0] == '\0')
        return 0;

    struct mixer_ctl *ctl = mixer_get_control(mMixer, ctlName, 0);
    if (ctl == NULL)
        return 0;

    long long vol, vol_min, vol_max;
    unsigned int Nmax = 6, N = volume * 5 + 1;
    float e = 2.71828, dB_min, dB_max, dB_vol, dB_step, volFloat;

    ALOGD("route_set_voice_volume() set incall voice volume %f to control %s", volume, ctlName);

    if (mixer_get_ctl_minmax(ctl, &vol_min, &vol_max) < 0) {
        ALOGE("mixer_get_dB_range() get control min max value fail");
        return 0;
    }

    mixer_get_dB_range(ctl, (long)vol_min, (long)vol_max, &dB_min, &dB_max, &dB_step);

    dB_vol = 20 * log((Nmax * pow(e, dB_min / 20) + N * (pow(e, dB_max / 20) - pow(e, dB_min / 20))) / Nmax);

    volFloat = vol_min + (dB_vol - dB_min) / dB_step;
    vol = (long long)volFloat;

    if (((unsigned)(volFloat * 10) % 10) >= 5)
        vol++;

    ALOGV("dB_min = %f, dB_step = %f, dB_max = %f, dB_vol = %f",
        dB_min,
        dB_step,
        dB_max,
        dB_vol);

    ALOGV("N = %u, volFloat = %f, vol = %lld", N, volFloat, vol);

    return mixer_ctl_set_int(ctl, vol);
}

const struct config_route *get_route_config(unsigned route)
{
    ALOGV("get_route_config() route %d", route);

    if (!route_table) {
        ALOGE("get_route_config() route_table is NULL!");
        return NULL;
    }
    switch (route) {
    case SPEAKER_NORMAL_ROUTE:
        return &(route_table->speaker_normal);
    case SPEAKER_INCALL_ROUTE:
        return &(route_table->speaker_incall);
    case SPEAKER_RINGTONE_ROUTE:
        return &(route_table->speaker_ringtone);
    case SPEAKER_VOIP_ROUTE:
        return &(route_table->speaker_voip);
    case EARPIECE_NORMAL_ROUTE:
        return &(route_table->earpiece_normal);
    case EARPIECE_INCALL_ROUTE:
        return &(route_table->earpiece_incall);
    case EARPIECE_RINGTONE_ROUTE:
        return &(route_table->earpiece_ringtone);
    case EARPIECE_VOIP_ROUTE:
        return &(route_table->earpiece_voip);
    case HEADPHONE_NORMAL_ROUTE:
        return &(route_table->headphone_normal);
    case HEADPHONE_INCALL_ROUTE:
        return &(route_table->headphone_incall);
    case HEADPHONE_RINGTONE_ROUTE:
        return &(route_table->headphone_ringtone);
    case SPEAKER_HEADPHONE_NORMAL_ROUTE:
        return &(route_table->speaker_headphone_normal);
    case SPEAKER_HEADPHONE_RINGTONE_ROUTE:
        return &(route_table->speaker_headphone_ringtone);
    case HEADPHONE_VOIP_ROUTE:
        return &(route_table->headphone_voip);
    case HEADSET_NORMAL_ROUTE:
        return &(route_table->headset_normal);
    case HEADSET_INCALL_ROUTE:
        return &(route_table->headset_incall);
    case HEADSET_RINGTONE_ROUTE:
        return &(route_table->headset_ringtone);
    case HEADSET_VOIP_ROUTE:
        return &(route_table->headset_voip);
    case BLUETOOTH_NORMAL_ROUTE:
        return &(route_table->bluetooth_normal);
    case BLUETOOTH_INCALL_ROUTE:
        return &(route_table->bluetooth_incall);
    case BLUETOOTH_VOIP_ROUTE:
        return &(route_table->bluetooth_voip);
    case MAIN_MIC_CAPTURE_ROUTE:
        return &(route_table->main_mic_capture);
    case HANDS_FREE_MIC_CAPTURE_ROUTE:
        return &(route_table->hands_free_mic_capture);
    case BLUETOOTH_SOC_MIC_CAPTURE_ROUTE:
        return &(route_table->bluetooth_sco_mic_capture);
    case PLAYBACK_OFF_ROUTE:
        return &(route_table->playback_off);
    case CAPTURE_OFF_ROUTE:
        return &(route_table->capture_off);
    case INCALL_OFF_ROUTE:
        return &(route_table->incall_off);
    case VOIP_OFF_ROUTE:
        return &(route_table->voip_off);
    case HDMI_NORMAL_ROUTE:
        return &(route_table->hdmi_normal);
    case USB_NORMAL_ROUTE:
        return &(route_table->usb_normal);
    case USB_CAPTURE_ROUTE:
        return &(route_table->usb_capture);
    default:
        ALOGE("get_route_config() Error route %d", route);
        return NULL;
    }
}

int set_controls(struct mixer *mixer, const struct config_control *ctls, const unsigned ctls_count)
{
    struct mixer_ctl *ctl;
    unsigned i;

    ALOGV("set_controls() ctls_count %d", ctls_count);

    if (!ctls || ctls_count <= 0) {
        ALOGV("set_controls() ctls is NULL");
        return 0;
    }

    for (i = 0; i < ctls_count; i++) {
        ctl = mixer_get_control(mixer, ctls[i].ctl_name, 0);
        ALOGE("========ctls[%d].ctl_name:%s=========",i,ctls[i].ctl_name);
        if (!ctl) {
            ALOGE_IF(route_table != &default_config_table, "set_controls() Can not get ctl : %s", ctls[i].ctl_name);
            ALOGV_IF(route_table == &default_config_table, "set_controls() Can not get ctl : %s", ctls[i].ctl_name);
            return -EINVAL;
        }

        if (ctl->info->type != SNDRV_CTL_ELEM_TYPE_BOOLEAN &&
            ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER &&
            ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER64 &&
            ctl->info->type != SNDRV_CTL_ELEM_TYPE_ENUMERATED) {
            ALOGE("set_controls() ctl %s is not a type of INT or ENUMERATED", ctls[i].ctl_name);
            return -EINVAL;
        }

        if (ctls[i].str_val) {
            if (ctl->info->type != SNDRV_CTL_ELEM_TYPE_ENUMERATED) {
                ALOGE("set_controls() ctl %s is not a type of ENUMERATED", ctls[i].ctl_name);
                return -EINVAL;
            }
            if (mixer_ctl_select(ctl, ctls[i].str_val) != 0) {
                ALOGE("set_controls() Can not set ctl %s to %s", ctls[i].ctl_name, ctls[i].str_val);
                return -EINVAL;
            }
            ALOGE("set_controls() set ctl %s to %s", ctls[i].ctl_name, ctls[i].str_val);
        } else {
            if (mixer_ctl_set_int_double(ctl, ctls[i].int_val[0], ctls[i].int_val[1]) != 0) {
                ALOGE("set_controls() can not set ctl %s to %d", ctls[i].ctl_name, ctls[i].int_val[0]);
                return -EINVAL;
            }
            ALOGE("set_controls() set ctl %s to %d", ctls[i].ctl_name, ctls[i].int_val[0]);
        }
    }

    return 0;
}

int route_set_controls(unsigned route)
{
    struct mixer* mMixer;

    if (route >= MAX_ROUTE) {
        ALOGE("route_set_controls() route %d error!", route);
        return -EINVAL;
    }

#ifdef SUPPORT_USB //usb input maybe used for primary
    if (route != USB_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != MAIN_MIC_CAPTURE_ROUTE &&
        route != HANDS_FREE_MIC_CAPTURE_ROUTE &&
        route != BLUETOOTH_SOC_MIC_CAPTURE_ROUTE) {
        ALOGV("route %d error for usb sound card!", route);
        return -EINVAL;
    }
#else //primary input maybe used for usb
    if (route > HDMI_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE) {
        ALOGV("route %d error for codec or hdmi!", route);
        return -EINVAL;
    }
#endif

    ALOGD("route_set_controls() set route %d", route);

    mMixer = is_playback_route(route) ? mMixerPlayback : mMixerCapture;

    if (!mMixer) {
        ALOGE("route_set_controls() mMixer is NULL!");
        return -EINVAL;
    }

    const struct config_route *route_info = get_route_config(route);
    if (!route_info) {
        ALOGE("route_set_controls() Can not get config of route");
        return -EINVAL;
    }

    if (route_info->controls_count > 0)
        set_controls(mMixer, route_info->controls, route_info->controls_count);

    return 0;
}

struct pcm *route_pcm_open(unsigned route, unsigned int flags)
{
    int is_playback;

    if (route >= MAX_ROUTE) {
        ALOGE("route_pcm_open() route %d error!", route);
        return NULL;
    }

#ifdef SUPPORT_USB //usb input maybe used for primary
    if (route != USB_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != MAIN_MIC_CAPTURE_ROUTE &&
        route != HANDS_FREE_MIC_CAPTURE_ROUTE &&
        route != BLUETOOTH_SOC_MIC_CAPTURE_ROUTE) {
        ALOGV("route %d error for usb sound card!", route);
        return NULL;
    }
#else //primary input maybe used for usb
    if (route > BLUETOOTH_SOC_MIC_CAPTURE_ROUTE &&
        route != HDMI_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE) {
        ALOGV("route %d error for codec or hdmi!", route);
        return NULL;
    }
#endif

    ALOGV("route_pcm_open() route %d", route);

    is_playback = is_playback_route(route);

	if (route_table) {
		ALOGD("route_table:%s",route_table->speaker_normal.controls->ctl_name);
	}

    if (!route_table) {
        route_init();
    }

    const struct config_route *route_info = get_route_config(route);
    if (!route_info) {
        ALOGE("route_pcm_open() Can not get config of route");
        return NULL;
    }

    ALOGD("route_info->sound_card %d, route_info->devices 0 %s %s",
        route_info->sound_card,
        (route_info->devices == DEVICES_0_1 || route_info->devices == DEVICES_0_2 ||
        route_info->devices == DEVICES_0_1_2) ? (route_info->devices == DEVICES_0_2 ? "2" : "1") : "",
        route_info->devices == DEVICES_0_1_2 ? "2" : "");

    flags &= ~PCM_CARD_MASK;
    switch(route_info->sound_card) {
    case 1:
        flags |= PCM_CARD1;
        break;
    case 2:
        flags |= PCM_CARD2;
        break;
    default:
        flags |= PCM_CARD0;
        break;
    }

    flags &= ~PCM_DEVICE_MASK;
    flags |= PCM_DEVICE0;

    if (is_playback) {
        //close all route and pcm
        if (mMixerPlayback) {
            route_set_controls(INCALL_OFF_ROUTE);
            route_set_controls(VOIP_OFF_ROUTE);
        }
        route_pcm_close(PLAYBACK_OFF_ROUTE);

        mPcm[PCM_DEVICE0_PLAYBACK] = pcm_open(flags);

        //Open playback and capture of device 1
        if (((flags & PCM_CARD_MASK) == PCM_CARD0) &&
            (route_info->devices == DEVICES_0_1 ||
            route_info->devices == DEVICES_0_1_2)) {
            unsigned int open_flags = flags;

            open_flags &= ~PCM_DEVICE_MASK;
            open_flags |= PCM_DEVICE1;

            if (mPcm[PCM_DEVICE1_PLAYBACK] == NULL)
                mPcm[PCM_DEVICE1_PLAYBACK] = pcm_open(open_flags);

            open_flags |= PCM_IN;

            if (mPcm[PCM_DEVICE1_CAPTURE] == NULL)
                mPcm[PCM_DEVICE1_CAPTURE] = pcm_open(open_flags);
        }

        //Open playback and capture of device 2
        if (((flags & PCM_CARD_MASK) == PCM_CARD0) &&
            (route_info->devices == DEVICES_0_2 ||
            route_info->devices == DEVICES_0_1_2)) {
            unsigned int open_flags = flags;

            open_flags &= ~PCM_DEVICE_MASK;
            open_flags |= PCM_DEVICE2;

            if (mPcm[PCM_DEVICE2_PLAYBACK] == NULL)
                mPcm[PCM_DEVICE2_PLAYBACK] = pcm_open(open_flags);

            open_flags |= PCM_IN;

            if (mPcm[PCM_DEVICE2_CAPTURE] == NULL)
                mPcm[PCM_DEVICE2_CAPTURE] = pcm_open(open_flags);
        }
    } else {
        route_pcm_close(CAPTURE_OFF_ROUTE);

        if (mPcm[PCM_DEVICE0_CAPTURE] == NULL)
            mPcm[PCM_DEVICE0_CAPTURE] = pcm_open(flags);
    }

    //update mMixer
    if (is_playback) {
        if (mMixerPlayback == NULL)
            mMixerPlayback = mixer_open(route_info->sound_card == 1 ? 0 : route_info->sound_card);
    } else {
        if (mMixerCapture == NULL)
            mMixerCapture = mixer_open(route_info->sound_card == 1 ? 0 : route_info->sound_card);
    }

    //set controls
    if (route_info->controls_count > 0)
        route_set_controls(route);

    return is_playback ? mPcm[PCM_DEVICE0_PLAYBACK] : mPcm[PCM_DEVICE0_CAPTURE];
}

int route_pcm_close(unsigned route)
{
    unsigned i;

    if (route != PLAYBACK_OFF_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != INCALL_OFF_ROUTE &&
        route != VOIP_OFF_ROUTE) {
        ALOGE("route_pcm_close() is not a off route");
        return 0;
    }

    ALOGV("route_pcm_close() route %d", route);

    //close pcm
    if (route == PLAYBACK_OFF_ROUTE) {
        if (mPcm[PCM_DEVICE0_PLAYBACK]) {
            pcm_close(mPcm[PCM_DEVICE0_PLAYBACK]);
            mPcm[PCM_DEVICE0_PLAYBACK] = NULL;
        }

        //close playback, we need to close device 1 and device 2
        for (i = PCM_DEVICE1_PLAYBACK; i < PCM_MAX; i++) {
            if (mPcm[i]) {
                pcm_close(mPcm[i]);
                mPcm[i] = NULL;
            }
        }
    } else if (route == CAPTURE_OFF_ROUTE) {
        if (mPcm[PCM_DEVICE0_CAPTURE]) {
            pcm_close(mPcm[PCM_DEVICE0_CAPTURE]);
            mPcm[PCM_DEVICE0_CAPTURE] = NULL;
        }
    }

    //set controls
    if (is_playback_route(route) ? mMixerPlayback : mMixerCapture)
        route_set_controls(route);

    //close mixer
    if (route == PLAYBACK_OFF_ROUTE) {
        if (mMixerPlayback) {
            mixer_close(mMixerPlayback);
            mMixerPlayback = NULL;
        }
    } else if (route == CAPTURE_OFF_ROUTE) {
        if (mMixerCapture) {
            mixer_close(mMixerCapture);
            mMixerCapture = NULL;
        }
    }

    return 0;
}

