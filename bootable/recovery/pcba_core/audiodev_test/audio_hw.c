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
 * @file    audio_hw.c
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
 * @version 1.0.5
 * @date    2015-08-24
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "AudioHardwareTiny"

#include "alsa_audio.h"
#include "audio_hw.h"
#include <system/audio.h>
#include "codec_config/config.h"
//#include "audio_bitstream.h"
//#include "audio_setting.h"
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#define SNDRV_CARDS 8
#define SNDRV_DEVICES 8

#define DBG 1
#if DBG
#define LOGINFO(args...) printf(args)
#define LOGERR(args...) fprintf(stderr, args)
#else
#define LOGINFO(args...)
#define LOGERR(args...)
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SND_CARDS_NODE          "/proc/asound/cards"
struct dev_proc_info SPEAKER_OUT_NAME[] = /* add codes& dai name here*/
{
    {"realtekrt5616c", NULL,},
    {"realtekrt5651co", "rt5651-aif1",},
    {"realtekrt5670c", NULL,},
    {"realtekrt5672c", NULL,},
    {"realtekrt5678co", NULL,},
    {"rkhdmianalogsnd", NULL,},
    {"rockchipcx2072x", NULL,},
    {"rockchipes8316c", NULL,},
    {"rockchipes8323c", NULL,},
    {"rockchipes8388c", NULL,},
    {"rockchipes8396c", NULL,},
    {"rockchiprk", NULL, },
    {"rockchiprk809co", NULL,},
    {"rockchiprk817co", NULL,},
    {"rockchiprt5640c", "rt5640-aif1",},
    {"rockchiprt5670c", NULL,},
    {"rockchiprt5672c", NULL,},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};

struct dev_proc_info HDMI_OUT_NAME[] =
{
    {"realtekrt5651co", "i2s-hifi",},
    {"realtekrt5670co", "i2s-hifi",},
    {"rkhdmidpsound", NULL,},
    {"rockchiphdmi", NULL,},
    {"rockchiprt5640c", "i2s-hifi",},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};


struct dev_proc_info SPDIF_OUT_NAME[] =
{
    {"ROCKCHIPSPDIF", "dit-hifi",},
    {"rockchipcdndp", NULL,},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};

struct dev_proc_info BT_OUT_NAME[] =
{
    {"rockchipbt", NULL,},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};

struct dev_proc_info MIC_IN_NAME[] =
{
    {"realtekrt5616c", NULL,},
    {"realtekrt5651co", "rt5651-aif1",},
    {"realtekrt5670c", NULL,},
    {"realtekrt5672c", NULL,},
    {"realtekrt5678co", NULL,},
    {"rockchipes8316c", NULL,},
    {"rockchipes8323c", NULL,},
    {"rockchipes8396c", NULL,},
    {"rockchipes7210", NULL,},
    {"rockchipes7243", NULL,},
    {"rockchiprk", NULL, },
    {"rockchiprk809co", NULL,},
    {"rockchiprk817co", NULL,},
    {"rockchiprt5640c", NULL,},
    {"rockchiprt5670c", NULL,},
    {"rockchiprt5672c", NULL,},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};


struct dev_proc_info HDMI_IN_NAME[] =
{
    {"realtekrt5651co", "tc358749x-audio"},
    {"hdmiin", NULL},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};

struct dev_proc_info BT_IN_NAME[] =
{
    {"rockchipbt", NULL},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};

static bool is_specified_out_sound_card(char *id, struct dev_proc_info *match)
{
    int i = 0;

    if (!match)
	return true; /* match any */

    while (match[i].cid) {
	if (!strcmp(id, match[i].cid)) {
	    return true;
    }
	i++;
    }
    return false;
}

static bool dev_id_match(char *info, const char *did)
{
    const char *deli = "id:";
    char *id;
    int idx = 0;

    if (!did)
	return true;
    if (!info)
	return false;
    /* find str like-> id: ff880000.i2s-rt5651-aif1 rt5651-aif1-0 */
    id = strstr(info, deli);
    if (!id)
	return false;
    id += strlen(deli);
    while(id[idx] != '\0') {
	if (id[idx] == '\r' ||id[idx] == '\n') {
	    id[idx] = '\0';
	    break;
    	}
	idx ++;
    }
    if (strstr(id, did)) {
	fprintf(stderr, "match dai!!!: %s <-> %s", id, did);
	return true;
    }
    return false;
}

static bool get_specified_out_dev(struct dev_info *devinfo,
				  int card,
				  const char *id,
				  struct dev_proc_info *match)
{
    int i = 0;
    int device;
    char str_device[32];
    char info[256];
    size_t len;
    FILE* file = NULL;

    /* parse card id */
    if (!match)
	return true; /* match any */
    while (match[i].cid) {
        if (!strcmp(id, match[i].cid)) {
            break;
        }
        i++;
    }
    if (!match[i].cid)
        return false;
    if (!match[i].did) { /* no exist dai info, exit */
        devinfo->card = card;
        devinfo->device = 0;
        devinfo->info = &match[i];
        fprintf(stderr, "%s card, got card=%d,device=%d", devinfo->id,
                devinfo->card, devinfo->device);
        return true;
    }

    /* parse device id */
    for (device = 0; device < SNDRV_DEVICES; device++) {
        sprintf(str_device, "proc/asound/card%d/pcm%dp/info", card, device);
        if (access(str_device, 0)) {
            fprintf(stderr, "No exist %s, break and finish parsing", str_device);
            break;
        }
        file = fopen(str_device, "r");
        if (!file) {
            fprintf(stderr, "Could reading %s property", str_device);
            continue;
        }
        len = fread(info, sizeof(char), sizeof(info)/sizeof(char), file);
        fclose(file);
        if (len == 0 || len > sizeof(info)/sizeof(char))
            continue;
        if (info[len - 1] == '\n') {
            len--;
            info[len] = '\0';
        }
        /* parse device dai */
        if (dev_id_match(info, match[i].did)) {
            devinfo->card = card;
            devinfo->device = device;
            devinfo->info = &match[i];
            fprintf(stderr, "%s card, got card=%d,device=%d", devinfo->id,
                devinfo->card, devinfo->device);
        return true;
        }
    }
    return false;
}

static bool get_specified_in_dev(struct dev_info *devinfo,
				 int card,
				 const char *id,
				 struct dev_proc_info *match)
{
    int i = 0;
    int device;
    char str_device[32];
    char info[256];
    size_t len;
    FILE* file = NULL;

    /* parse card id */
    if (!match)
	return true; /* match any */
    while (match[i].cid) {
	if (!strcmp(id, match[i].cid)) {
	    break;
	}
	i++;
    }
    if (!match[i].cid)
	return false;
    if (!match[i].did) { /* no exist dai info, exit */
	devinfo->card = card;
	devinfo->device = 0;
	fprintf(stderr, "%s card, got card=%d,device=%d", devinfo->id,
	      devinfo->card, devinfo->device);
	return true;
    }

    /* parse device id */
    for (device = 0; device < SNDRV_DEVICES; device++) {
        sprintf(str_device, "proc/asound/card%d/pcm%dc/info", card, device);
        if (access(str_device, 0)) {
            fprintf(stderr, "No exist %s, break and finish parsing", str_device);
            break;
        }
        file = fopen(str_device, "r");
        if (!file) {
            fprintf(stderr, "Could reading %s property", str_device);
            continue;
        }
        len = fread(info, sizeof(char), sizeof(info)/sizeof(char), file);
        fclose(file);
        if (len == 0 || len > sizeof(info)/sizeof(char))
            continue;
        if (info[len - 1] == '\n') {
            len--;
            info[len] = '\0';
        }
        /* parse device dai */
        if (dev_id_match(info, match[i].did)) {
            devinfo->card = card;
            devinfo->device = device;
            fprintf(stderr, "%s card, got card=%d,device=%d", devinfo->id,
                devinfo->card, devinfo->device);
            return true;
        }
    }
    return false;
}

static bool is_specified_in_sound_card(char *id, struct dev_proc_info *match)
{
    int i = 0;

    /*
     * mic: diffrent product may have diffrent card name,modify codes here
     * for example: 0 [rockchiprk3328 ]: rockchip-rk3328 - rockchip-rk3328
     */
    if (!match)
	return true;/* match any */
    while (match[i].cid) {
	if (!strcmp(id, match[i].cid)) {
	    return true;
  }
	i++;
    }
    return false;
}

static void set_default_dev_info( struct dev_info *info, int size, int rid)
{
    for(int i =0; i < size; i++) {
	if (rid) {
	    info[i].id = NULL;
	}
	info[i].card = (int)SND_OUT_SOUND_CARD_UNKNOWN;
    }
}

static void dumpdev_info(const char *tag, struct dev_info  *devinfo, int count)
{
    LOGINFO("dump %s device info\n", tag);
    for(int i = 0; i < count; i++) {
	if (devinfo[i].id && devinfo[i].card != SND_OUT_SOUND_CARD_UNKNOWN)
	    LOGERR("dev_info %s  card=%d, device:%d\n", devinfo[i].id,
		  devinfo[i].card,
		  devinfo[i].device);
    }
}

/*
 * get sound card infor by parser node: /proc/asound/cards
 * the sound card number is not always the same value
 */
void read_out_sound_card(struct stream_out *out)
{

    struct audio_device *device = NULL;
    int card = 0;
    char str[32];
    char id[20];
    size_t len;
    FILE* file = NULL;

    if((out == NULL) || (out->dev == NULL)) {
	return ;
    }
    device = out->dev;
    set_default_dev_info(device->dev_out, SND_OUT_SOUND_CARD_UNKNOWN, 0);
    for (card = 0; card < SNDRV_CARDS; card++) {
	sprintf(str, "proc/asound/card%d/id", card);
	if (access(str, 0)) {
	    fprintf(stderr, "No exist %s, break and finish parsing\n", str);
	    break;
	}
	file = fopen(str, "r");
	if (!file) {
	    fprintf(stderr, "Could reading %s property\n", str);
	    continue;
	}
	len = fread(id, sizeof(char), sizeof(id)/sizeof(char), file);
	fclose(file);
	if (len == 0 || len > sizeof(id)/sizeof(char))
	    continue;
	if (id[len - 1] == '\n') {
	    len--;
	    id[len] = '\0';
	}
	fprintf(stderr, "card%d id:%s\n", card, id);
	get_specified_out_dev(&device->dev_out[SND_OUT_SOUND_CARD_SPEAKER], card, id, SPEAKER_OUT_NAME);
	get_specified_out_dev(&device->dev_out[SND_OUT_SOUND_CARD_HDMI], card, id, HDMI_OUT_NAME);
	get_specified_out_dev(&device->dev_out[SND_OUT_SOUND_CARD_SPDIF], card, id, SPDIF_OUT_NAME);
	get_specified_out_dev(&device->dev_out[SND_OUT_SOUND_CARD_BT], card, id, BT_OUT_NAME);
    }
    dumpdev_info("out", device->dev_out, SND_OUT_SOUND_CARD_MAX);
    return ;
}

/*
 * get sound card infor by parser node: /proc/asound/cards
 * the sound card number is not always the same value
 */
void read_in_sound_card(struct stream_in *in)
{
    struct audio_device *device = NULL;
    int card = 0;
    char str[32];
    char id[20];
    size_t len;
    FILE* file = NULL;

    if((in == NULL) || (in->dev == NULL)){
	return ;
    }
    device = in->dev;
    set_default_dev_info(device->dev_in, SND_IN_SOUND_CARD_UNKNOWN, 0);
    for (card = 0; card < SNDRV_CARDS; card++) {
	sprintf(str, "proc/asound/card%d/id", card);
	if(access(str, 0)) {
	    fprintf(stderr, "No exist %s, break and finish parsing\n", str);
		break;
	}
	file = fopen(str, "r");
	if (!file) {
	    fprintf(stderr, "Could reading %s property\n", str);
	    continue;
	}
	len = fread(id, sizeof(char), sizeof(id)/sizeof(char), file);
	fclose(file);
	if (len == 0 || len > sizeof(id)/sizeof(char))
	    continue;
	if (id[len - 1] == '\n') {
	    len--;
	   id[len] = '\0';
	}
	get_specified_in_dev(&device->dev_in[SND_IN_SOUND_CARD_MIC], card, id, MIC_IN_NAME);
	/* set HDMI audio input info if need hdmi audio input */
	get_specified_in_dev(&device->dev_in[SND_IN_SOUND_CARD_HDMI], card, id, HDMI_IN_NAME);
	get_specified_in_dev(&device->dev_in[SND_IN_SOUND_CARD_BT], card, id, BT_IN_NAME);
    }
    dumpdev_info("in", device->dev_in, SND_IN_SOUND_CARD_MAX);
    return ;
}



static void updateBit(int *maskAndState, int position, const char *state, const char *name) 
{
	char namestate1[64];
	char namestate0[64];

	sprintf(namestate1, "%s=1", name);
	sprintf(namestate0, "%s=0", name);
	if (strstr(state, namestate1)) {
		maskAndState[0] |= position;
		maskAndState[1] |= position;
	} else if (strstr(state, namestate0)) {
		maskAndState[0] |= position;
		maskAndState[1] &= ~position;
	}
}

int parseState(struct StatePair *pPair, const char *status)
{
    int maskAndState[2] = {0};
    if (!pPair) {
        LOGINFO("pPair cannot be NULL\n");
        return -1;
    }
    LOGINFO("parseState %s\n", status);
    // extcon event state changes from kernel4.9
    // new state will be like STATE=MICROPHONE=1\nHEADPHONE=0
    updateBit(maskAndState, BIT_HEADSET_NO_MIC, status, "HEADPHONE") ;
    updateBit(maskAndState, BIT_HEADSET, status,"MICROPHONE") ;
    updateBit(maskAndState, BIT_HDMI_AUDIO, status,"HDMI") ;
    updateBit(maskAndState, BIT_LINEOUT, status,"LINE-OUT") ;
    LOGINFO("mask %08X state %08X\n", maskAndState[0], maskAndState[1]);
    pPair->mask = maskAndState[0];
    pPair->state = maskAndState[1];
    return 0;
}

void updateLocked(struct audio_device *adev, const char *newName, int newState)
{
    // Retain only relevant bits
    int headsetState = newState & SUPPORTED_HEADSETS;
    int usb_headset_anlg = headsetState & BIT_USB_HEADSET_ANLG;
    int usb_headset_dgtl = headsetState & BIT_USB_HEADSET_DGTL;
    int h2w_headset = headsetState & (BIT_HEADSET | BIT_HEADSET_NO_MIC | BIT_LINEOUT);
    bool h2wStateChange = true;
    bool usbStateChange = true;
    int mHeadsetState;

    if (NULL == adev) {
        LOGERR("adev can not be null\n");
    }
    mHeadsetState = adev->mHeadsetState;
    LOGINFO("newName=%s newState=%d headsetState=%d prev headsetState=%d\n",
            newName, newState, headsetState, mHeadsetState);
    if (mHeadsetState == headsetState) {
        LOGINFO("No state change.\n");
        return;
    }
    // reject all suspect transitions: only accept state changes from:
    // - a: 0 headset to 1 headset
    // - b: 1 headset to 0 headset
    if (h2w_headset == (BIT_HEADSET | BIT_HEADSET_NO_MIC | BIT_LINEOUT)) {
        LOGINFO("Invalid combination, unsetting h2w flag\n");
        h2wStateChange = false;
    }
    // - c: 0 usb headset to 1 usb headset
    // - d: 1 usb headset to 0 usb headset
    if (usb_headset_anlg == BIT_USB_HEADSET_ANLG && usb_headset_dgtl == BIT_USB_HEADSET_DGTL) {
        LOGINFO("Invalid combination, unsetting usb flag\n");
        usbStateChange = false;
    }
    if (!h2wStateChange && !usbStateChange) {
        LOGINFO("invalid transition, returning ...\n");
        return;
    }
    //mHeadsetState = headsetState;
    adev->mHeadsetState = headsetState;
}



void adev_open_init(struct audio_device *adev)
{
    struct stream_out tout;
    struct stream_in tin;

    printf("adev_open_init in\n");
    if (NULL == adev) {
        printf("adev_open_init:adev can not be NULL\n");
        return;
    }
    set_default_dev_info(adev->dev_out, SND_OUT_SOUND_CARD_UNKNOWN, 1);
    set_default_dev_info(adev->dev_in, SND_IN_SOUND_CARD_UNKNOWN, 1);
    adev->dev_out[SND_OUT_SOUND_CARD_SPEAKER].id = "SPEAKER";
    adev->dev_out[SND_OUT_SOUND_CARD_HDMI].id = "HDMI";
    adev->dev_out[SND_OUT_SOUND_CARD_SPDIF].id = "SPDIF";
    adev->dev_out[SND_OUT_SOUND_CARD_BT].id = "BT";
    adev->dev_in[SND_IN_SOUND_CARD_MIC].id = "MIC";
    adev->dev_in[SND_IN_SOUND_CARD_BT].id = "BT";
    tout.dev = adev;
    tin.dev = adev;
    read_out_sound_card(&tout);
    read_in_sound_card(&tin);
    printf("adev_open_init out\n");
}

void adev_wired_init(struct audio_device *adev)
{
    DIR *dir;
    struct dirent *de;
    const char *extcon = "/sys/class/extcon";
    const char *matchs = "extcon";
    char file_name[128];
    char buf[128];
    FILE *file;
    int read_size;
    struct StatePair Pair;
    int mask;
    int state;

    LOGINFO("adev_wired_init\n");
    if (NULL == adev) {
        LOGERR("adev_wired_init:adev can not be null\n");
    }
    dir = opendir(extcon);
    if (NULL == dir) {
        LOGERR("can not open %s\n", extcon);
    }
    while ((de = readdir(dir))) {
        if(strstr(de->d_name, matchs)){
            LOGINFO("name: [%s]\n", de->d_name);
            snprintf(file_name, 128, "%s/%s/state", extcon, de->d_name);
            LOGINFO("open: [%s]\n", file_name);
            file = fopen(file_name, "rt");
            if (file == NULL) {
                LOGERR("open fail\n");
                continue;
            }
            memset(buf, 0, sizeof(buf));
            read_size = fread(buf, sizeof(char), sizeof(buf)/sizeof(buf[0]), file);
            if (read_size > 0 ) {
                parseState(&Pair, buf);
                mask = Pair.mask;
                state = Pair.state;
                updateLocked(adev, "h2w", (adev->mHeadsetState & ~(mask & ~state)) | (mask & state));
            }
            fclose(file);
        }
    }
    LOGINFO("adev_wired_init: 0x%08X\n", adev->mHeadsetState);
}


/**
 * @brief getOutputRouteFromDevice
 *
 * @param device
 *
 * @returns
 */
unsigned getOutputRouteFromDevice(uint32_t device)
{
    /*if (mMode != AudioSystem::MODE_RINGTONE && mMode != AudioSystem::MODE_NORMAL)
        return PLAYBACK_OFF_ROUTE;
    */
    switch (device) {
    case AUDIO_DEVICE_OUT_SPEAKER:
        return SPEAKER_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        return HEADSET_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        return HEADPHONE_NORMAL_ROUTE;
    case (AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADPHONE):
    case (AUDIO_DEVICE_OUT_SPEAKER|AUDIO_DEVICE_OUT_WIRED_HEADSET):
        return SPEAKER_HEADPHONE_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        return BLUETOOTH_NORMAL_ROUTE;
    case AUDIO_DEVICE_OUT_AUX_DIGITAL:
	return HDMI_NORMAL_ROUTE;
        //case AudioSystem::DEVICE_OUT_EARPIECE:
        //	return EARPIECE_NORMAL_ROUTE;
        //case AudioSystem::DEVICE_OUT_ANLG_DOCK_HEADSET:
        //case AudioSystem::DEVICE_OUT_DGTL_DOCK_HEADSET:
        //	return USB_NORMAL_ROUTE;
    default:
        return PLAYBACK_OFF_ROUTE;
    }
}

/**
 * @brief getVoiceRouteFromDevice
 *
 * @param device
 *
 * @returns
 */
uint32_t getVoiceRouteFromDevice(uint32_t device)
{
    LOGINFO("not support now\n");

    device = 0;
    return 0;
}

/**
 * @brief getInputRouteFromDevice
 *
 * @param device
 *
 * @returns
 */
uint32_t getInputRouteFromDevice(uint32_t device)
{
    /*if (mMicMute) {
        return CAPTURE_OFF_ROUTE;
    }*/
    LOGINFO("%s:device:%x\n", __FUNCTION__, device);
    switch (device) {
    case AUDIO_DEVICE_IN_BUILTIN_MIC:
        return MAIN_MIC_CAPTURE_ROUTE;
    case AUDIO_DEVICE_IN_WIRED_HEADSET:
        return HANDS_FREE_MIC_CAPTURE_ROUTE;
    case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
        return BLUETOOTH_SOC_MIC_CAPTURE_ROUTE;
    case AUDIO_DEVICE_IN_ANLG_DOCK_HEADSET:
        return USB_CAPTURE_ROUTE;
    case AUDIO_DEVICE_IN_HDMI:
        return HDMI_IN_CAPTURE_ROUTE;
    default:
        return CAPTURE_OFF_ROUTE;
    }
}

/**
 * @brief getRouteFromDevice
 *
 * @param device
 *
 * @returns
 */
uint32_t getRouteFromDevice(uint32_t device)
{
    if (device & AUDIO_DEVICE_BIT_IN)
        return getInputRouteFromDevice(device);
    else
        return getOutputRouteFromDevice(device);
}
