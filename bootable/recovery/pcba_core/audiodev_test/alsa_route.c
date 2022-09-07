/*
 * Copyright (C) 2015 Rockchip Electronics Co., Ltd.
 */

/*
 * @file alsa_route.c
 * @brief 
 * @author  RkAudio
 * @version 1.0.8
 * @date 2015-08-24
 */

#define LOG_TAG "alsa_route"

//#define LOG_NDEBUG 0

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
#include "asound.h"

#include "codec_config/config_list.h"

#define DBG 1
#if DBG
#define LOGINFO(args...) printf(args)
#define LOGERR(args...) fprintf(stderr, args)
#else
#define LOGINFO(args...)
#define LOGERR(args...)
#endif
struct alsa_route
{
    const struct config_route_table *route_table;
    struct mixer* mMixerPlayback;
    struct mixer* mMixerCapture;
};

struct alsa_route *gpalsaroute;
/**
 * @brief route_init
 *
 * @returns
 */
int route_card_init(void **pproute_data, int card)
{
    char soundcard[32];
    char soundCardID[20] = "";
    static FILE *fp;
    unsigned i, config_count = sizeof(sound_card_config_list) / sizeof(struct alsa_sound_card_config);
    size_t read_size;
    struct alsa_route *palsa_route;
    const struct config_route_table *route_table = NULL;

    printf("route_card_init(card %d)", card);
    if (NULL == pproute_data) {
        LOGERR("route_card_init:pproute_data can not be null\n");
        return -1;
    }
    palsa_route = (struct alsa_route *)malloc(sizeof(*palsa_route));
    if (!palsa_route) {
        LOGERR("route_card_init:malloc palsa_route fail");
        return -1;
    }
    memset(palsa_route, 0, sizeof(*palsa_route));
    sprintf(soundcard, "/proc/asound/card%d/id", card);
    fp = fopen(soundcard, "rt");
    if (!fp) {
        LOGERR("Open %s error!\n", soundcard);
    } else {
        read_size = fread(soundCardID, sizeof(char), sizeof(soundCardID), fp);
        fclose(fp);
        if (soundCardID[read_size - 1] == '\n') {
            read_size--;
            soundCardID[read_size] = '\0';
        }
        LOGINFO("Sound card%d is %s\n", card, soundCardID);
        for (i = 0; i < config_count; i++) {
            if (!(sound_card_config_list + i) || !sound_card_config_list[i].sound_card_name ||
                !sound_card_config_list[i].route_table)
                continue;
            if (strncmp(sound_card_config_list[i].sound_card_name, soundCardID, read_size) == 0) {
                route_table = sound_card_config_list[i].route_table;
                LOGERR("Get route table for sound card0 %s\n", soundCardID);
            }
        }
    }
    if (!route_table) {
        route_table = (void *)&default_config_table;
        LOGERR("Can not get config table for sound card0 %s, so get default config table.\n", soundCardID);
    }
    palsa_route->route_table = route_table;
    *pproute_data = (void *)palsa_route;
    return 0;
}

/**
 * @brief get_route_config 
 *
 * @param route
 *
 * @returns 
 */
const struct config_route *get_route_config(void *proute_data, unsigned route)
{
    struct alsa_route *palsa_route = (struct alsa_route *)proute_data;
    const struct config_route_table *route_table;

    LOGINFO("get_route_config() route %d\n", route);
    if (NULL == palsa_route) {
        LOGERR("get_route_config:palsa_route can not be null\n");
        return NULL;
    }
    route_table = palsa_route->route_table;
    if (!route_table) {
        LOGERR("get_route_config() route_table is NULL!\n");
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
    case SPDIF_NORMAL_ROUTE:
        return &(route_table->spdif_normal);
    case HDMI_IN_NORMAL_ROUTE:
        return &(route_table->hdmiin_normal);
    case HDMI_IN_OFF_ROUTE:
        return &(route_table->hdmiin_off);
    case HDMI_IN_CAPTURE_ROUTE:
        return &(route_table->hdmiin_captrue);
    case HDMI_IN_CAPTURE_OFF_ROUTE:
        return &(route_table->hdmiin_captrue_off);
    default:
        LOGERR("get_route_config() Error route %d\n", route);
        return NULL;
    }
}

/**
 * @brief set_controls 
 *
 * @param mixer
 * @param ctls
 * @param ctls_count
 *
 * @returns 
 */
int set_controls(struct mixer *mixer, const struct config_control *ctls, const unsigned int ctls_count)
{
    struct mixer_ctl *ctl;
    unsigned i;

    LOGINFO("set_controls() ctls_count %d\n", ctls_count);
    if (!ctls || ctls_count <= 0) {
        LOGERR("set_controls() ctls is NULL\n");
        return 0;
    }
    for (i = 0; i < ctls_count; i++) {
        ctl = mixer_get_control(mixer, ctls[i].ctl_name, 0);
        if (!ctl) {
            LOGERR("set_controls() Can not get ctl : %s\n", ctls[i].ctl_name);
            return -EINVAL;
        }
        if (ctl->info->type != SNDRV_CTL_ELEM_TYPE_BOOLEAN &&
            ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER &&
            ctl->info->type != SNDRV_CTL_ELEM_TYPE_INTEGER64 &&
            ctl->info->type != SNDRV_CTL_ELEM_TYPE_ENUMERATED) {
            LOGERR("set_controls() ctl %s is not a type of INT or ENUMERATED\n", ctls[i].ctl_name);
            return -EINVAL;
        }
        if (ctls[i].str_val) {
            if (ctl->info->type != SNDRV_CTL_ELEM_TYPE_ENUMERATED) {
                LOGERR("set_controls() ctl %s is not a type of ENUMERATED\n", ctls[i].ctl_name);
                return -EINVAL;
            }
            if (mixer_ctl_select(ctl, ctls[i].str_val) != 0) {
                LOGERR("set_controls() Can not set ctl %s to %s\n", ctls[i].ctl_name, ctls[i].str_val);
                return -EINVAL;
            }
            LOGINFO("set_controls() set ctl %s to %s\n", ctls[i].ctl_name, ctls[i].str_val);
        } else {
            if (mixer_ctl_set_int_double(ctl, ctls[i].int_val[0], ctls[i].int_val[1]) != 0) {
                LOGERR("set_controls() can not set ctl %s to %d\n", ctls[i].ctl_name, ctls[i].int_val[0]);
                return -EINVAL;
            }
            LOGINFO("set_controls() set ctl %s to %d\n", ctls[i].ctl_name, ctls[i].int_val[0]);
        }
    }
    return 0;
}

/**
 * @brief route_set_controls 
 *
 * @param route
 *
 * @returns 
 */
int route_set_controls(void *proute_data, unsigned route)
{
    struct alsa_route *palsa_route = (struct alsa_route *)proute_data;
    const struct config_route *route_info = NULL;
    struct mixer *mMixer = NULL;

    if (NULL == palsa_route) {
        LOGERR("route_set_controls:palsa_route can not be null\n");
        return 0;
    }
    if (route >= MAX_ROUTE) {
        LOGERR("route_set_controls() route %d error!\n", route);
        return -EINVAL;
    }
#ifdef SUPPORT_USB //usb input maybe used for primary
    if (route != USB_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != MAIN_MIC_CAPTURE_ROUTE &&
        route != HANDS_FREE_MIC_CAPTURE_ROUTE &&
        route != BLUETOOTH_SOC_MIC_CAPTURE_ROUTE) {
        ALOGV("route %d error for usb sound card!\n", route);
        return -EINVAL;
    }
#else //primary input maybe used for usb
    if (route > SPDIF_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != HDMI_IN_NORMAL_ROUTE &&
        route != HDMI_IN_OFF_ROUTE &&
        route != HDMI_IN_CAPTURE_ROUTE &&
        route != HDMI_IN_CAPTURE_OFF_ROUTE) {
        LOGERR("route %d error for codec or hdmi!\n", route);
        return -EINVAL;
    }
#endif
    LOGINFO("route_set_controls() set route %d\n", route);
    mMixer = is_playback_route(route) ? palsa_route->mMixerPlayback : palsa_route->mMixerCapture;
    if (!mMixer) {
        LOGERR("route_set_controls() mMixer is NULL!\n");
        return -EINVAL;
    }
    route_info = get_route_config(proute_data, route);
    if (!route_info) {
        LOGERR("route_set_controls() Can not get config of route\n");
        return -EINVAL;
    }
    if (route_info->controls_count > 0)
        set_controls(mMixer, route_info->controls, route_info->controls_count);
    return 0;
}

/**
 * @brief route_uninit 
 */
void route_uninit(void *proute_data)
{
    LOGINFO("route_uninit()\n");
    route_pcm_close(proute_data, PLAYBACK_OFF_ROUTE);
    route_pcm_close(proute_data, CAPTURE_OFF_ROUTE);
}

/**
 * @brief is_playback_route 
 *
 * @param route
 *
 * @returns 
 */
int is_playback_route(unsigned route)
{
    switch (route) {
    case MAIN_MIC_CAPTURE_ROUTE:
    case HANDS_FREE_MIC_CAPTURE_ROUTE:
    case BLUETOOTH_SOC_MIC_CAPTURE_ROUTE:
    case CAPTURE_OFF_ROUTE:
    case USB_CAPTURE_ROUTE:
    case HDMI_IN_NORMAL_ROUTE:
    case HDMI_IN_OFF_ROUTE:
    case HDMI_IN_CAPTURE_ROUTE:
    case HDMI_IN_CAPTURE_OFF_ROUTE:
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
    case SPDIF_NORMAL_ROUTE:
        return 1;
    default:
        LOGERR("is_playback_route() Error route %d\n", route);
        return -EINVAL;
    }
}

/**
 * @brief route_pcm_open
 *
 * @param route
 */
void route_pcm_card_open(void **pproute_data, int card, unsigned route)
{
    struct alsa_route *palsa_route;
    const struct config_route *route_info;
    int is_playback;

    LOGINFO("route_pcm_card_open(card %d, route %d)\n", card, route);
    if (NULL == pproute_data) {
        LOGERR("route_pcm_card_open:pproute_data can not be null\n");
        goto __exit;
    }
    if (route >= MAX_ROUTE) {
        LOGERR("route_pcm_card_open() route %d error!\n", route);
        goto __exit;
    }
    if (card < 0) {
        LOGERR("route_pcm_card_open() card %d error!\n", card);
        goto __exit;
    }
#ifdef SUPPORT_USB //usb input maybe used for primary

	if (route != USB_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != MAIN_MIC_CAPTURE_ROUTE &&
        route != HANDS_FREE_MIC_CAPTURE_ROUTE &&
        route != BLUETOOTH_SOC_MIC_CAPTURE_ROUTE) {
        ALOGV("route %d error for usb sound card!", route);
        goto __exit;
    }
#else //primary input maybe used for usb
    if (route > BLUETOOTH_SOC_MIC_CAPTURE_ROUTE &&
        route != HDMI_NORMAL_ROUTE &&
        route != SPDIF_NORMAL_ROUTE &&
        route != USB_CAPTURE_ROUTE &&
        route != HDMI_IN_NORMAL_ROUTE &&
        route != HDMI_IN_OFF_ROUTE &&
        route != PLAYBACK_OFF_ROUTE) {
        LOGERR("route %d error for codec or hdmi!\n", route);
        goto __exit;
    }
#endif



    is_playback = is_playback_route(route);

    if (!pproute_data) {
        route_card_init(pproute_data, card);
    }
    palsa_route = *((struct alsa_route **)pproute_data);
    if (NULL == palsa_route) {
        LOGERR("route_pcm_close:palsa_route can not be null\n");
        goto __exit;
    }
    route_info = get_route_config(palsa_route, route);
    if (!route_info) {
        LOGERR("route_pcm_open() Can not get config of route\n");
        goto __exit;
    }
    if (is_playback) {
        //close all route and pcm
        if (palsa_route->mMixerPlayback) {
            route_set_controls(palsa_route, INCALL_OFF_ROUTE);
            route_set_controls(palsa_route, VOIP_OFF_ROUTE);
        }
        route_pcm_close(palsa_route, PLAYBACK_OFF_ROUTE);
    } else {
        route_pcm_close(palsa_route, CAPTURE_OFF_ROUTE);
    }
    if (is_playback) {
        if (palsa_route->mMixerPlayback == NULL)
            palsa_route->mMixerPlayback = mixer_open_legacy(card);
    } else {
        if (palsa_route->mMixerCapture == NULL)
            palsa_route->mMixerCapture = mixer_open_legacy(card);
    }
    if (route_info->controls_count > 0)
        route_set_controls(palsa_route, route);
__exit:
	LOGINFO("route_pcm_open exit\n");

}

/**
 * @brief route_pcm_close 
 *
 * @param route
 *
 * @returns 
 */
int route_pcm_close(void *proute_data, unsigned route)
{
    struct alsa_route *palsa_route = (struct alsa_route *)proute_data;

    if (NULL == palsa_route) {
        LOGERR("route_pcm_close:palsa_route can not be null\n");
        return 0;
    }
    if (route != PLAYBACK_OFF_ROUTE &&
        route != CAPTURE_OFF_ROUTE &&
        route != INCALL_OFF_ROUTE &&
        route != VOIP_OFF_ROUTE &&
        route != HDMI_IN_CAPTURE_OFF_ROUTE) {
        LOGERR("route_pcm_close() is not a off route\n");
        return 0;
    }
    LOGINFO("route_pcm_close() route %d\n", route);
    route_set_controls(proute_data, route);
    if (route == PLAYBACK_OFF_ROUTE) {
        if (palsa_route->mMixerPlayback) {
            mixer_close_legacy(palsa_route->mMixerPlayback);
            palsa_route->mMixerPlayback = NULL;
        }
    } else if (route == CAPTURE_OFF_ROUTE) {
        if (palsa_route->mMixerCapture) {
            mixer_close_legacy(palsa_route->mMixerCapture);
            palsa_route->mMixerCapture = NULL;
        }
    }
    return 0;
}
