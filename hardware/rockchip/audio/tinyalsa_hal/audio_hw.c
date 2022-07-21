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

#include "audio_setting.h"
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#define SNDRV_CARDS 8
#define SNDRV_DEVICES 8

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SND_CARDS_NODE          "/proc/asound/cards"
#define SAMPLECOUNT 441*5*2*2

#define CHR_VALID (1 << 1)
#define CHL_VALID (1 << 0)
#define CH_CHECK (1 << 2)

#define HDMI_BITSTREAM_BYPASS "ELD Bypass"

struct SurroundFormat {
    audio_format_t format;
    const char *value;
};


const struct SurroundFormat sSurroundFormat[] = {
    {AUDIO_FORMAT_AC3,"AUDIO_FORMAT_AC3"},
    {AUDIO_FORMAT_E_AC3,"AUDIO_FORMAT_E_AC3"},
    {AUDIO_FORMAT_DTS,"AUDIO_FORMAT_DTS"},
    {AUDIO_FORMAT_DTS_HD,"AUDIO_FORMAT_DTS_HD"},
    {AUDIO_FORMAT_AAC_LC,"AUDIO_FORMAT_AAC_LC"},
    {AUDIO_FORMAT_DOLBY_TRUEHD,"AUDIO_FORMAT_DOLBY_TRUEHD"},
    {AUDIO_FORMAT_AC4,"AUDIO_FORMAT_E_AC3_JOC"}
};

enum SOUND_CARD_OWNER{
    SOUND_CARD_HDMI = 0,
    SOUND_CARD_SPDIF = 1,
};

/*
 * mute audio datas when screen off or standby
 * The MediaPlayer no stop/pause when screen off, they may be just play in background,
 * so they still send audio datas to audio hal.
 * HDMI may disconnet and enter stanby status, this means no voice output on HDMI
 * but speaker/av and spdif still work, and voice may output on them.
 * Some customer need to mute the audio datas in this condition.
 * If need mute datas when screen off, define this marco.
 */
//#define MUTE_WHEN_SCREEN_OFF

/*
 * if current audio stream bitstream over hdmi,
 * and hdmi is removed and reconnected later,
 * the driver of hdmi may config it with pcm mode automatically,
 * which is according the implement of hdmi driver.
 * If hdmi driver implement in this way, in order to output audio
 * bitstream stream after hdmi reconnected,
 * we must close sound card of hdmi and reopen/config
 * it in bitstream mode. If need this, define this macro.
 */
#define AUDIO_BITSTREAM_REOPEN_HDMI

//#define ALSA_DEBUG
#ifdef ALSA_IN_DEBUG
FILE *in_debug;
#endif

int in_dump(const struct audio_stream *stream, int fd);
int out_dump(const struct audio_stream *stream, int fd);

/**
 * @brief get_output_device_id
 *
 * @param device
 *
 * @returns
 */
int get_output_device_id(audio_devices_t device)
{
    if (device == AUDIO_DEVICE_NONE)
        return OUT_DEVICE_NONE;

    if (popcount(device) == 2) {
        if ((device == (AUDIO_DEVICE_OUT_SPEAKER |
                        AUDIO_DEVICE_OUT_WIRED_HEADSET)) ||
                (device == (AUDIO_DEVICE_OUT_SPEAKER |
                            AUDIO_DEVICE_OUT_WIRED_HEADPHONE)))
            return OUT_DEVICE_SPEAKER_AND_HEADSET;
        else
            return OUT_DEVICE_NONE;
    }

    if (popcount(device) != 1)
        return OUT_DEVICE_NONE;

    switch (device) {
    case AUDIO_DEVICE_OUT_SPEAKER:
        return OUT_DEVICE_SPEAKER;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        return OUT_DEVICE_HEADSET;
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        return OUT_DEVICE_HEADPHONES;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        return OUT_DEVICE_BT_SCO;
    default:
        return OUT_DEVICE_NONE;
    }
}

/**
 * @brief get_input_source_id
 *
 * @param source
 *
 * @returns
 */
int get_input_source_id(audio_source_t source)
{
    switch (source) {
    case AUDIO_SOURCE_DEFAULT:
        return IN_SOURCE_NONE;
    case AUDIO_SOURCE_MIC:
        return IN_SOURCE_MIC;
    case AUDIO_SOURCE_CAMCORDER:
        return IN_SOURCE_CAMCORDER;
    case AUDIO_SOURCE_VOICE_RECOGNITION:
        return IN_SOURCE_VOICE_RECOGNITION;
    case AUDIO_SOURCE_VOICE_COMMUNICATION:
        return IN_SOURCE_VOICE_COMMUNICATION;
    default:
        return IN_SOURCE_NONE;
    }
}

/**
 * @brief force_non_hdmi_out_standby
 * must be called with hw device outputs list, all out streams, and hw device mutexes locked
 *
 * @param adev
 */
static void force_non_hdmi_out_standby(struct audio_device *adev)
{
    enum output_type type;
    struct stream_out *out;
    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        out = adev->outputs[type];
        if (type == OUTPUT_HDMI_MULTI|| !out)
            continue;
        /* This will never recurse more than 2 levels deep. */
        do_out_standby(out);
    }
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
    ALOGE("not support now");
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
    ALOGE("%s:device:%x",__FUNCTION__,device);
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
    {"rockchipes8388", NULL,},
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
    {"hdmisound", NULL},
    {"rockchiphdmi", NULL,},
    {"rockchiphdmi1", NULL,},
    {"rockchiphdmi0", NULL,},
    {"rockchiprt5640c", "i2s-hifi",},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};


struct dev_proc_info SPDIF_OUT_NAME[] =
{
    {"ROCKCHIPSPDIF", "dit-hifi",},
    {"rockchipspdif", NULL,},
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
    {"rockchipes8388c", NULL,},
    {"rockchipes8388", NULL,},
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
    {"rockchiphdmirx", NULL},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};

struct dev_proc_info BT_IN_NAME[] =
{
    {"rockchipbt", NULL},
    {NULL, NULL}, /* Note! Must end with NULL, else will cause crash */
};

static int name_match(const char* dst, const char* src)
{
    int score = 0;
    // total equal
    if (!strcmp(dst, src)) {
        score = 100;
    } else  if (strstr(dst, src)) {
        // part equal
        score = 50;
    }

    return score;
}

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

static bool dev_id_match(const char *info, const char *did)
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
        ALOGE("match dai!!!: %s %s", id, did);
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
    int score  = 0;
    int better = devinfo->score;
    int index = -1;

    /* parse card id */
    if (!match)
        return true; /* match any */
    while (match[i].cid) {
        score = name_match(id, match[i].cid);
        if (score > better) {
            better = score;
            index = i;
        }
        i++;
    }

    if (index < 0)
        return false;

    if (!match[index].cid)
        return false;

    if (!match[index].did) { /* no exist dai info, exit */
        devinfo->card = card;
        devinfo->device = 0;
        devinfo->score  = better;
        ALOGD("%s card, got card=%d,device=%d", devinfo->id,
              devinfo->card, devinfo->device);
        return true;
    }

    /* parse device id */
    for (device = 0; device < SNDRV_DEVICES; device++) {
        sprintf(str_device, "proc/asound/card%d/pcm%dp/info", card, device);
        if (access(str_device, 0)) {
            ALOGD("No exist %s, break and finish parsing", str_device);
            break;
        }
        file = fopen(str_device, "r");
        if (!file) {
            ALOGD("Could reading %s property", str_device);
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
        if (dev_id_match(info, match[index].did)) {
            devinfo->card = card;
            devinfo->device = device;
            devinfo->score  = better;
            ALOGD("%s card, got card=%d,device=%d", devinfo->id,
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
    int score  = 0;
    int better = devinfo->score;
    int index = -1;

    /* parse card id */
    if (!match)
        return true; /* match any */

    while (match[i].cid) {
        score = name_match(id, match[i].cid);
        if (score > better) {
            better = score;
            index = i;
        }
        i++;
    }

    if (index < 0)
        return false;

    if (!match[index].cid)
        return false;

    if (!match[index].did) { /* no exist dai info, exit */
        devinfo->card = card;
        devinfo->device = 0;
        devinfo->score = better;
        ALOGD("%s card, got card=%d,device=%d", devinfo->id,
              devinfo->card, devinfo->device);
        return true;
    }

    /* parse device id */
    for (device = 0; device < SNDRV_DEVICES; device++) {
        sprintf(str_device, "proc/asound/card%d/pcm%dc/info", card, device);
        if (access(str_device, 0)) {
            ALOGD("No exist %s, break and finish parsing", str_device);
            break;
        }
        file = fopen(str_device, "r");
        if (!file) {
            ALOGD("Could reading %s property", str_device);
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
            devinfo->score = better;
            ALOGD("%s card, got card=%d,device=%d", devinfo->id,
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
        info[i].score = 0;
    }
}

static void dumpdev_info(const char *tag, struct dev_info  *devinfo, int count)
{
    ALOGD("dump %s device info", tag);
    for(int i = 0; i < count; i++) {
        if (devinfo[i].id && devinfo[i].card != SND_OUT_SOUND_CARD_UNKNOWN)
            ALOGD("dev_info %s  card=%d, device:%d", devinfo[i].id,
                  devinfo[i].card,
                  devinfo[i].device);
    }
}

/*
 * get sound card infor by parser node: /proc/asound/cards
 * the sound card number is not always the same value
 */
static void read_out_sound_card(struct stream_out *out)
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
            ALOGD("No exist %s, break and finish parsing", str);
            break;
        }
        file = fopen(str, "r");
        if (!file) {
            ALOGD("Could reading %s property", str);
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
        ALOGD("card%d id:%s", card, id);
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
static void read_in_sound_card(struct stream_in *in)
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
            ALOGD("No exist %s, break and finish parsing", str);
                break;
        }
        file = fopen(str, "r");
        if (!file) {
            ALOGD("Could reading %s property", str);
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

static uint32_t channel_check(void *data, unsigned int len)
{
    short *pcmLeftChannel = (short*)data;
    short *pcmRightChannel = pcmLeftChannel + 1;
    unsigned int index = 0;
    int leftValid = 0x0;
    int rightValid = 0x0;
    short valuel = 0;
    short valuer = 0;
    uint32_t validflag = 0;

    valuel = *pcmLeftChannel;
    valuer = *pcmRightChannel;
    for (index = 0; index < len; index += 2) {
        if ((pcmLeftChannel[index] >= valuel + 50) ||
            (pcmLeftChannel[index] <= valuel - 50))
            leftValid++;
        if ((pcmRightChannel[index] >= valuer + 50) ||
            (pcmRightChannel[index] <= valuer - 50))
            rightValid++;
    }
    if (leftValid > 20)
        validflag |= CHL_VALID;
    if (rightValid > 20)
        validflag |= CHR_VALID;
    return validflag;
}

static void channel_fixed(void *data, unsigned len, uint32_t chFlag)
{
    short *ch0 ,*ch1, *pcmValid, *pcmInvalid;

    if ((chFlag&(CHL_VALID | CHR_VALID)) == 0 ||
        (chFlag&(CHL_VALID | CHR_VALID)) == (CHL_VALID | CHR_VALID))
        return;
    ch0 = (short*)data;
    ch1 = ch0 + 1;
    pcmValid = ch0;
    pcmInvalid = ch0;
    if (chFlag & CHL_VALID)
        pcmInvalid  = ch1;
    else if (chFlag & CHR_VALID)
        pcmValid = ch1;
    for (unsigned index = 0; index < len; index += 2) {
        pcmInvalid[index] = pcmValid[index];
    }
    return;
}

static void channel_check_start(struct stream_in *in)
{
    in->channel_flag = CH_CHECK;
    in->start_checkcount = 0;
}

static bool is_bitstream(struct stream_out *out)
{
    if (out == NULL) {
        return false;
    }

    if (out->config.format == PCM_FORMAT_IEC958_SUBFRAME_LE)
        return true;

    bool bitstream = false;
    if (out->output_direct) {
        switch(out->output_direct_mode) {
            case HBR:
            case NLPCM:
                bitstream = true;
                break;
            case LPCM:
            default:
                bitstream = false;
                break;
        }
    } else {
        if(out->output_direct_mode != LPCM) {
            ALOGD("%s: %d: error output_direct = false, but output_direct_mode != LPCM, this is error config",__FUNCTION__,__LINE__);
        }
    }

    return bitstream;
}

static bool is_multi_pcm(struct stream_out *out)
{
    if (out == NULL) {
        return false;
    }

    bool multi = false;
    if (out->output_direct && (out->output_direct_mode == LPCM) && (out->config.channels > 2)) {
        multi = true;
    }

    return multi;
}

/**
 * @brief mixer_hdmi_set_force_bypass
 * force hdmi to bypass even if hdmi not support bypass
 */
static int mixer_hdmi_set_force_bypass(struct stream_out *out)
{
    int ret = 0;
    struct mixer *pMixer = NULL;
    struct mixer_ctl *pctl;
    struct audio_device *adev = out->dev;

    if (out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
        pMixer = mixer_open_legacy(adev->dev_out[SND_OUT_SOUND_CARD_HDMI].card);
        if (!pMixer) {
            return ret;
        }

        pctl = mixer_get_control(pMixer, HDMI_BITSTREAM_BYPASS, 0);
        if (pctl != NULL) {
            // do not care edid
            if (is_bitstream(out)) {
                ret = mixer_ctl_set_val(pctl, 1);
            } else {
                ret = mixer_ctl_set_val(pctl, 0);
            }
        }
        mixer_close_legacy(pMixer);
    }

    return ret;
}


/**
 * @brief mixer_mode_set
 * for rk3399 audio output mixer mode set
 * @param out
 *
 * @return
 */
static int mixer_mode_set(struct stream_out *out)
{
    int ret = 0;
    struct mixer *pMixer = NULL;
    struct mixer_ctl *pctl;
    struct audio_device *adev = out->dev;

    /*
     * set audio mode for hdmi
     * The driver of hdmi read the audio mode to identify
     * the type of audio stream according to audio mode.
     * 1) LPCM: the stream is pcm format
     * 2) NLPCM: the stream is bitstream format, AC3/EAC3/DTS use this format
     * 3) HDR: the stream is bitstream format, TrueHD/Atoms/DTS-HD/DTS-X use this format.
     */
    if (out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
        pMixer = mixer_open_legacy(adev->dev_out[SND_OUT_SOUND_CARD_HDMI].card);
        if (!pMixer) {
            ALOGE("mMixer is a null point %s %d,CARD = %d",__func__, __LINE__,adev->dev_out[SND_OUT_SOUND_CARD_HDMI].card);
            return ret;
        }
        pctl = mixer_get_control(pMixer,"AUDIO MODE",0 );
        if (pctl != NULL) {
            ALOGD("Now set mixer audio_mode is %d for drm",out->output_direct_mode);
            switch (out->output_direct_mode) {
            case HBR:
                ret = mixer_ctl_set_val(pctl , out->output_direct_mode);
                break;
            case NLPCM:
                ret = mixer_ctl_set_val(pctl , out->output_direct_mode);
                break;
            default:
                ret = mixer_ctl_set_val(pctl , out->output_direct_mode);
                break;
            }

            if (ret != 0) {
                ALOGE("set_controls() can not set ctl!");
                mixer_close_legacy(pMixer);
                return -EINVAL;
            }
        }
        mixer_close_legacy(pMixer);
    }

    return ret;
}


static void open_sound_card_policy(struct stream_out *out)
{
    if (out == NULL) {
        return ;
    }

    if (is_bitstream(out) || (is_multi_pcm(out))) {
        return ;
    }

    /*
     * In Box Product, ouput 2 channles pcm datas over hdmi,speaker and spdif simultaneous.
     * speaker can only support 44.1k or 48k
     */
    bool support = ((out->config.rate == 44100) || (out->config.rate == 48000));
    struct audio_device *adev = out->dev;
    if (support) {
        if(adev->dev_out[SND_OUT_SOUND_CARD_SPEAKER].card != SND_OUT_SOUND_CARD_UNKNOWN) {
            out->device |= AUDIO_DEVICE_OUT_SPEAKER;
        }

        if(adev->dev_out[SND_OUT_SOUND_CARD_HDMI].card != SND_OUT_SOUND_CARD_UNKNOWN) {
            /*
             * hdmi is taken by direct/mulit pcm output
             */
            if(adev->outputs[OUTPUT_HDMI_MULTI] != NULL) {
                out->device &= ~AUDIO_DEVICE_OUT_AUX_DIGITAL;
            } else {
                out->device |= AUDIO_DEVICE_OUT_AUX_DIGITAL;
            }
        }

        if(adev->dev_out[SND_OUT_SOUND_CARD_SPDIF].card != SND_OUT_SOUND_CARD_UNKNOWN){
           out->device |= AUDIO_DEVICE_OUT_SPDIF;
        }
    }

    // some specail config for chips
#ifdef RK3288
    /*3288's hdmi & codec use the same i2s,so only config the codec card*/
    audio_devices_t devices = (AUDIO_DEVICE_OUT_AUX_DIGITAL|AUDIO_DEVICE_OUT_SPEAKER);
    if ((out->device & devices) == devices) {
        out->device &= ~AUDIO_DEVICE_OUT_AUX_DIGITAL;
    }
#endif
}

/**
 * @brief start_output_stream
 * must be called with hw device outputs list, output stream, and hw device mutexes locked
 *
 * @param out
 *
 * @returns
 */
static int start_output_stream(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    int ret = 0;
    int card = (int)SND_OUT_SOUND_CARD_UNKNOWN;
    int device = 0;
    // set defualt value to true for compatible with mid project


    ALOGD("%s:%d out = %p,device = 0x%x,outputs[OUTPUT_HDMI_MULTI] = %p",__FUNCTION__,__LINE__,out,out->device,adev->outputs[OUTPUT_HDMI_MULTI]);
    if (out == adev->outputs[OUTPUT_HDMI_MULTI]) {
        force_non_hdmi_out_standby(adev);
    } else if (adev->outputs[OUTPUT_HDMI_MULTI] &&
            !adev->outputs[OUTPUT_HDMI_MULTI]->standby) {
        out->disabled = true;
        return 0;
    }

    out->disabled = false;
    read_out_sound_card(out);

#ifdef BOX_HAL
    open_sound_card_policy(out);
#endif

    out_dump(out, 0);

#if SUPPORT_MULTIAUDIO
    out->device &= ~(AUDIO_DEVICE_OUT_AUX_DIGITAL | AUDIO_DEVICE_OUT_SPDIF);
#endif

    if (out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
        audio_devices_t route_device = out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL;
        route_pcm_card_open(adev->dev_out[SND_OUT_SOUND_CARD_HDMI].card, getRouteFromDevice(route_device));

        if (adev->owner[SOUND_CARD_HDMI] == NULL) {
            card = adev->dev_out[SND_OUT_SOUND_CARD_HDMI].card;
            device =adev->dev_out[SND_OUT_SOUND_CARD_HDMI].device;
            if (card != (int)SND_OUT_SOUND_CARD_UNKNOWN) {

#ifndef IEC958_FORAMT
#ifdef  USE_DRM
                // set audio mode
                ret = mixer_mode_set(out);
                if (ret != 0) {
                    ALOGE("mixer mode set error,ret=%d!",ret);
                }
#endif
#endif
                mixer_hdmi_set_force_bypass(out);
                out->pcm[SND_OUT_SOUND_CARD_HDMI] = pcm_open(card, device,
                                                    PCM_OUT | PCM_MONOTONIC, &out->config);
                if (out->pcm[SND_OUT_SOUND_CARD_HDMI] &&
                        !pcm_is_ready(out->pcm[SND_OUT_SOUND_CARD_HDMI])) {
                    ALOGE("pcm_open(PCM_CARD_HDMI) failed: %s, card number = %d",
                          pcm_get_error(out->pcm[SND_OUT_SOUND_CARD_HDMI]),card);
                    pcm_close(out->pcm[SND_OUT_SOUND_CARD_HDMI]);
                    return -ENOMEM;
                }

                if (is_bitstream(out) && ((out->config.format == PCM_FORMAT_S24_LE)
                    || (out->config.format == PCM_FORMAT_IEC958_SUBFRAME_LE))) {
                    out->bistream = bitstream_init(out->config.format,
                        out->config.rate, out->config.channels);
                }

                if (is_multi_pcm(out) || is_bitstream(out)){
                    adev->owner[SOUND_CARD_HDMI] = (int*)out;
                }
            }
        } else {
            ALOGD("The current HDMI is DVI mode");
            out->device |= AUDIO_DEVICE_OUT_SPEAKER;
        }
    }

    if (out->device & (AUDIO_DEVICE_OUT_SPEAKER |
                       AUDIO_DEVICE_OUT_WIRED_HEADSET |
                       AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                       AUDIO_DEVICE_OUT_ALL_SCO)) {
        audio_devices_t route_device = out->device & (AUDIO_DEVICE_OUT_SPEAKER |
                                                      AUDIO_DEVICE_OUT_WIRED_HEADSET |
                                                      AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                                                      AUDIO_DEVICE_OUT_ALL_SCO);
        route_pcm_card_open(adev->dev_out[SND_OUT_SOUND_CARD_SPEAKER].card, getRouteFromDevice(route_device));
        card = adev->dev_out[SND_OUT_SOUND_CARD_SPEAKER].card;
        device = adev->dev_out[SND_OUT_SOUND_CARD_SPEAKER].device;
        if(card != (int)SND_OUT_SOUND_CARD_UNKNOWN) {
            out->pcm[SND_OUT_SOUND_CARD_SPEAKER] = pcm_open(card, device,
                                          PCM_OUT | PCM_MONOTONIC, &out->config);
            if (out->pcm[SND_OUT_SOUND_CARD_SPEAKER] && !pcm_is_ready(out->pcm[SND_OUT_SOUND_CARD_SPEAKER])) {
                ALOGE("pcm_open(PCM_CARD) failed: %s,card number = %d",
                      pcm_get_error(out->pcm[SND_OUT_SOUND_CARD_SPEAKER]),card);
                pcm_close(out->pcm[SND_OUT_SOUND_CARD_SPEAKER]);
                return -ENOMEM;
            }
        }

    }

    if (out->device & AUDIO_DEVICE_OUT_SPDIF) {
        if (adev->owner[SOUND_CARD_SPDIF] == NULL){
            card = adev->dev_out[SND_OUT_SOUND_CARD_SPDIF].card;
            device = adev->dev_out[SND_OUT_SOUND_CARD_SPDIF].device;
            if(card != (int)SND_OUT_SOUND_CARD_UNKNOWN) {
                out->pcm[SND_OUT_SOUND_CARD_SPDIF] = pcm_open(card, device,
                                                    PCM_OUT | PCM_MONOTONIC, &out->config);

                if (out->pcm[SND_OUT_SOUND_CARD_SPDIF] &&
                        !pcm_is_ready(out->pcm[SND_OUT_SOUND_CARD_SPDIF])) {
                    ALOGE("pcm_open(PCM_CARD_SPDIF) failed: %s,card number = %d",
                          pcm_get_error(out->pcm[SND_OUT_SOUND_CARD_SPDIF]),card);
                    pcm_close(out->pcm[SND_OUT_SOUND_CARD_SPDIF]);
                    return -ENOMEM;
                }

                if (is_multi_pcm(out) || is_bitstream(out)){
                    adev->owner[SOUND_CARD_SPDIF] = (int*)out;
                }
            }
        }
    }

    adev->out_device |= out->device;
    ALOGD("%s:%d, out = %p",__FUNCTION__,__LINE__,out);
    return 0;
}

/**
 * @brief get_next_buffer
 *
 * @param buffer_provider
 * @param buffer
 *
 * @returns
 */
static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer* buffer)
{
    struct stream_in *in;
    size_t i,size;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    in = (struct stream_in *)((char *)buffer_provider -
                              offsetof(struct stream_in, buf_provider));

    if (in->pcm == NULL) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

    if (in->frames_in == 0) {
        size = pcm_frames_to_bytes(in->pcm, in->config->period_size);
        in->read_status = pcm_read(in->pcm,
                                   (void*)in->buffer, size);
        if (in->read_status != 0) {
            ALOGE("get_next_buffer() pcm_read error %d", in->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }

        if (in->config->channels == 2) {
            if (in->channel_flag & CH_CHECK) {
                if (in->start_checkcount < SAMPLECOUNT) {
                    in->start_checkcount += size;
                } else {
                    in->channel_flag = channel_check((void*)in->buffer, size / 2);
                    in->channel_flag &= ~CH_CHECK;
                }
            }
            channel_fixed((void*)in->buffer, size / 2, in->channel_flag & ~CH_CHECK);
        }

#ifdef RK_DENOISE_ENABLE
        if (!(in->device & AUDIO_DEVICE_IN_HDMI)) {
            rkdenoise_process(in->mDenioseState, (void*)in->buffer, size, (void*)in->buffer);
        }
#endif
        //fwrite(in->buffer,pcm_frames_to_bytes(in->pcm,pcm_get_buffer_size(in->pcm)),1,in_debug);
        in->frames_in = in->config->period_size;

        /* Do stereo to mono conversion in place by discarding right channel */
        if ((in->channel_mask == AUDIO_CHANNEL_IN_MONO)
                &&(in->config->channels == 2)) {
            //ALOGE("channel_mask = AUDIO_CHANNEL_IN_MONO");
            for (i = 0; i < in->frames_in; i++)
                in->buffer[i] = in->buffer[i * 2];
        }
    }

    //ALOGV("pcm_frames_to_bytes(in->pcm,pcm_get_buffer_size(in->pcm)):%d",size);
    buffer->frame_count = (buffer->frame_count > in->frames_in) ?
                          in->frames_in : buffer->frame_count;
    buffer->i16 = in->buffer +
                  (in->config->period_size - in->frames_in) *
                  audio_channel_count_from_in_mask(in->channel_mask);

    return in->read_status;

}

/**
 * @brief release_buffer
 *
 * @param buffer_provider
 * @param buffer
 */
static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                           struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    in = (struct stream_in *)((char *)buffer_provider -
                              offsetof(struct stream_in, buf_provider));

    in->frames_in -= buffer->frame_count;
}

static bool get_hdmiin_audio_info(struct audio_device *adev, char *prop, int *value)
{
    char strfile[128];
    FILE* file = NULL;
    char info[20] = {0};

    if (!value)
        return false;
    sprintf(strfile, "/sys/class/hdmirx/%s/%s", "hdmirx", prop);
    if (access(strfile, 0)) {
        ALOGD("No exist %s", strfile);
        return false;
    }
    file = fopen(strfile, "r");
    if (!file) {
        ALOGD("Could reading %s property", strfile);
        return false;
    }
    fread(info, sizeof(char), sizeof(info)/sizeof(char) - 1, file);
    fclose(file);
    *value = atoi(info);
    return true;
}

#define STR_32KHZ "32KHZ"
#define STR_44_1KHZ "44.1KHZ"
#define STR_48KHZ "48KHZ"
/**
 * @brief get_hdmiin_audio_rate
 * @param
 * @return hdmiin audio rate
 */
static int get_hdmiin_audio_rate(struct audio_device *adev)
{
    int rate;
    char value[PROPERTY_VALUE_MAX] = "";

    if (get_hdmiin_audio_info(adev, "audio_rate", &rate)) {
        return rate;
    }
    property_get("vendor.hdmiin.audiorate", value, STR_44_1KHZ);
    if ( 0 == strncmp(value, STR_32KHZ, strlen(STR_32KHZ)) ){
        rate = 32000;
    } else if ( 0 == strncmp(value, STR_44_1KHZ, strlen(STR_44_1KHZ)) ){
        rate = 44100;
    } else if ( 0 == strncmp(value, STR_48KHZ, strlen(STR_48KHZ)) ){
        rate = 48000;
    } else {
        rate = atoi(value);
        if (rate <= 0)
            rate = 44100;
    }

    // if hdmiin connect to codec, use 44100 sample rate
    if (adev->dev_out[SND_IN_SOUND_CARD_HDMI].card
            == adev->dev_out[SND_OUT_SOUND_CARD_SPEAKER].card)
        rate = 44100;

    return rate;
}

int create_resampler_helper(struct stream_in *in, uint32_t in_rate)
{
    int ret = 0;
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }

    in->buf_provider.get_next_buffer = get_next_buffer;
    in->buf_provider.release_buffer = release_buffer;
    ALOGD("create resampler, channel %d, rate %d => %d",
                    audio_channel_count_from_in_mask(in->channel_mask),
                    in_rate, in->requested_rate);
    ret = create_resampler(in_rate,
                    in->requested_rate,
                    audio_channel_count_from_in_mask(in->channel_mask),
                    RESAMPLER_QUALITY_DEFAULT,
                    &in->buf_provider,
                    &in->resampler);
    if (ret != 0) {
        ret = -EINVAL;
    }

    return ret;
}

/**
 * @brief start_input_stream
 * must be called with input stream and hw device mutexes locked
 *
 * @param in
 *
 * @returns
 */
static int start_input_stream(struct stream_in *in)
{
    struct audio_device *adev = in->dev;
    int  ret = 0;
    int card = 0;
    int device = 0;
    int hdmiin_present = 0;

    channel_check_start(in);
    in_dump(in, 0);
    read_in_sound_card(in);
    route_pcm_card_open(adev->dev_in[SND_IN_SOUND_CARD_MIC].card,
                        getRouteFromDevice(in->device | AUDIO_DEVICE_BIT_IN));
#ifdef RK3399_LAPTOP //HARD CODE FIXME
    if ((in->device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) &&
            (adev->mode == AUDIO_MODE_IN_COMMUNICATION)) {
        in->config = &pcm_config_in_bt;
        card = adev->dev_in[SND_IN_SOUND_CARD_BT].card;
        device =  adev->dev_in[SND_IN_SOUND_CARD_BT].device;
        if(card != SND_IN_SOUND_CARD_UNKNOWN){
            in->pcm = pcm_open(card, device, PCM_IN, in->config);
            if (in->resampler) {
                release_resampler(in->resampler);

                in->buf_provider.get_next_buffer = get_next_buffer;
                in->buf_provider.release_buffer = release_buffer;

                ret = create_resampler(8000,
                                       in->requested_rate,
                                       audio_channel_count_from_in_mask(in->channel_mask),
                                       RESAMPLER_QUALITY_DEFAULT,
                                       &in->buf_provider,
                                       &in->resampler);
                if (ret != 0) {
                    ret = -EINVAL;
                }
            }
        } else {
            ALOGE("%s: %d,the card number of bt is = %d",__FUNCTION__,__LINE__,card);
            return -EINVAL;
        }
    } else {
        in->config = &pcm_config_in;
        card = adev->dev_in[SND_IN_SOUND_CARD_MIC].card;
        device =  adev->dev_in[SND_IN_SOUND_CARD_MIC].device;
        if (card != SND_IN_SOUND_CARD_UNKNOWN) {
            in->pcm = pcm_open(card, device, PCM_IN, in->config);

            if (in->resampler) {
                release_resampler(in->resampler);

                in->buf_provider.get_next_buffer = get_next_buffer;
                in->buf_provider.release_buffer = release_buffer;

                ret = create_resampler(48000,
                                       in->requested_rate,
                                       audio_channel_count_from_in_mask(in->channel_mask),
                                       RESAMPLER_QUALITY_DEFAULT,
                                       &in->buf_provider,
                                       &in->resampler);
                if (ret != 0) {
                    ret = -EINVAL;
                }
            }
        } else {
            ALOGE("%s: %d,the card number of mic is %d",__FUNCTION__,__LINE__,card);
            return -EINVAL;
        }
    }
#else
    card = (int)adev->dev_in[SND_IN_SOUND_CARD_HDMI].card;
    if (in->device & AUDIO_DEVICE_IN_HDMI && (card != (int)SND_OUT_SOUND_CARD_UNKNOWN)) {
        if (get_hdmiin_audio_info(adev, "audio_present", &hdmiin_present)) {
            if (!hdmiin_present) {
                ALOGD("hdmiin audio is no present, don't open hdmiin sound");
                return -EEXIST;
            }
        }
        in->config->rate = get_hdmiin_audio_rate(adev);
        in->pcm = pcm_open(card, PCM_DEVICE, PCM_IN, in->config);
        ALOGD("open HDMIIN %d", card);
        if (in->resampler) {
            release_resampler(in->resampler);
            in->resampler = NULL;
        }

        // if hdmiin connect to codec, don't resample
        if (in->config->rate != in->requested_rate) {
            ret = create_resampler_helper(in, in->config->rate);
        }
    } else if (in->device & AUDIO_DEVICE_IN_BUILTIN_MIC ||
               in->device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
        card = adev->dev_in[SND_IN_SOUND_CARD_MIC].card;
        device =  adev->dev_in[SND_IN_SOUND_CARD_MIC].device;
        in->pcm = pcm_open(card, device, PCM_IN, in->config);
#ifdef RK_DENOISE_ENABLE
        {
            int ch = in->config->channels;
            int period = in->config->period_size;
            int rate = in->config->rate;
            int type = 0;
            {
                char value[PROPERTY_VALUE_MAX];
                property_get("vendor.audio.anr.speex", value, "0");
                type = atoi(value);
            }
            if (in->mDenioseState)
                rkdenoise_destroy(in->mDenioseState);
            in->mDenioseState = rkdenoise_create(rate, ch, period, type ? ALG_SPX : ALG_SKV);
            if (in->mDenioseState == NULL) {
                ALOGW("crate rkdenoise failed!!!");
            }
        }
#endif

    } else {
        card = adev->dev_in[SND_IN_SOUND_CARD_BT].card;
        device = adev->dev_in[SND_IN_SOUND_CARD_BT].device;
        in->pcm = pcm_open(card, device, PCM_IN, in->config);
    }
#endif
    if (in->pcm && !pcm_is_ready(in->pcm)) {
        ALOGE("pcm_open() failed: %s", pcm_get_error(in->pcm));
        pcm_close(in->pcm);
        return -ENOMEM;
    }

    /* if no supported sample rate is available, use the resampler */
    if (in->resampler)
        in->resampler->reset(in->resampler);

    in->frames_in = 0;
    adev->input_source = in->input_source;
    adev->in_device = in->device;
    adev->in_channel_mask = in->channel_mask;


    /* initialize volume ramp */
    in->ramp_frames = (CAPTURE_START_RAMP_MS * in->requested_rate) / 1000;
    in->ramp_step = (uint16_t)(USHRT_MAX / in->ramp_frames);
    in->ramp_vol = 0;;


    return 0;
}

/**
 * @brief get_input_buffer_size
 *
 * @param sample_rate
 * @param format
 * @param channel_count
 * @param is_low_latency
 *
 * @returns
 */
static size_t get_input_buffer_size(unsigned int sample_rate,
                                    audio_format_t format,
                                    unsigned int channel_count,
                                    bool is_low_latency)
{
    const struct pcm_config *config = is_low_latency ?
                                              &pcm_config_in_low_latency : &pcm_config_in;
    size_t size;

    /*
     * take resampling into account and return the closest majoring
     * multiple of 16 frames, as audioflinger expects audio buffers to
     * be a multiple of 16 frames
     */
    size = (config->period_size * sample_rate) / config->rate;
    size = ((size + 15) / 16) * 16;

    return size * channel_count * audio_bytes_per_sample(format);
}


/**
 * @brief read_frames
 * read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified
 *
 * @param in
 * @param buffer
 * @param frames
 *
 * @returns
 */
static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;
    size_t frame_size = audio_stream_in_frame_size(&in->stream);

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        if (in->resampler != NULL) {
            in->resampler->resample_from_provider(in->resampler,
                                                  (int16_t *)((char *)buffer +
                                                          frames_wr * frame_size),
                                                  &frames_rd);
        } else {
            struct resampler_buffer buf = {
                { raw : NULL, },
frame_count :
                frames_rd,
            };
            if (get_next_buffer(&in->buf_provider, &buf))
                break;
            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                       frames_wr * frame_size,
                       buf.raw,
                       buf.frame_count * frame_size);
                frames_rd = buf.frame_count;
            }
            release_buffer(&in->buf_provider, &buf);
        }
        /* in->read_status is updated by getNextBuffer() also called by
         * in->resampler->resample_from_provider() */
        if (in->read_status != 0)
            return in->read_status;

        frames_wr += frames_rd;
    }
    return frames_wr;
}

/**
 * @brief out_get_sample_rate
 *
 * @param stream
 *
 * @returns
 */
static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.vts_test", value, NULL);
    if (strcmp(value, "true") == 0) {
        if (out->use_default_config) {
            return 48000;
        } else {
            return out->aud_config.sample_rate;
        }
    } else {
        return out->config.rate;
    }
}

/**
 * @brief out_set_sample_rate
 *
 * @param stream
 * @param rate
 *
 * @returns
 */
static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return -ENOSYS;
}

/**
 * @brief out_get_buffer_size
 *
 * @param stream
 *
 * @returns
 */
static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->config.period_size *
           audio_stream_out_frame_size((const struct audio_stream_out *)stream);
}

/**
 * @brief out_get_channels
 *
 * @param stream
 *
 * @returns
 */
static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.vts_test", value, NULL);
    if (out->use_default_config) {
        return AUDIO_CHANNEL_OUT_MONO;
    } else {
        return out->aud_config.channel_mask;
    }
}

/**
 * @brief out_get_format
 *
 * @param stream
 *
 * @returns
 */
static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.vts_test", value, NULL);
    if (out->use_default_config) {
        return AUDIO_FORMAT_PCM_16_BIT;
    } else {
        return out->aud_config.format;
    }
}

/**
 * @brief out_set_format
 *
 * @param stream
 * @param format
 *
 * @returns
 */
static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -ENOSYS;
}

/**
 * @brief output_devices
 * Return the set of output devices associated with active streams
 * other than out.  Assumes out is non-NULL and out->dev is locked.
 *
 * @param out
 *
 * @returns
 */
static audio_devices_t output_devices(struct stream_out *out)
{
    struct audio_device *dev = out->dev;
    enum output_type type;
    audio_devices_t devices = AUDIO_DEVICE_NONE;

    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        struct stream_out *other = dev->outputs[type];
        if (other && (other != out) && !other->standby) {
            // TODO no longer accurate
            /* safe to access other stream without a mutex,
             * because we hold the dev lock,
             * which prevents the other stream from being closed
             */
            devices |= other->device;
        }
    }

    return devices;
}

/**
 * @brief do_out_standby
 * must be called with hw device outputs list, all out streams, and hw device mutex locked
 *
 * @param out
 */
static void do_out_standby(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    int i;
    ALOGD("%s,out = %p,device = 0x%x",__FUNCTION__,out,out->device);
    if (!out->standby) {
        for (i = 0; i < SND_OUT_SOUND_CARD_MAX; i++) {
            if (out->pcm[i]) {
                pcm_close(out->pcm[i]);
                out->pcm[i] = NULL;
            }
        }
        out->standby = true;
        out->nframes = 0;
        if (out == adev->outputs[OUTPUT_HDMI_MULTI]) {
            /* force standby on low latency output stream so that it can reuse HDMI driver if
             * necessary when restarted */
            force_non_hdmi_out_standby(adev);
        }
#ifdef USE_DRM
        mixer_mode_set(out);
#endif
        /* re-calculate the set of active devices from other streams */
        adev->out_device = output_devices(out);

#ifdef AUDIO_3A
        if (adev->voice_api != NULL) {
            adev->voice_api->flush();
        }
#endif
        route_pcm_close(PLAYBACK_OFF_ROUTE);
        ALOGD("close device");

        /* Skip resetting the mixer if no output device is active */
        if (adev->out_device) {
            route_pcm_open(getRouteFromDevice(adev->out_device));
            ALOGD("change device");
        }
        if(adev->owner[SOUND_CARD_HDMI] == (int*)out){
            adev->owner[SOUND_CARD_HDMI] = NULL;
        }

        if(adev->owner[SOUND_CARD_SPDIF] == (int*)out){
            adev->owner[SOUND_CARD_SPDIF] = NULL;
        }

        bitstream_destory(&out->bistream);
    }
}

/**
 * @brief lock_all_outputs
 * lock outputs list, all output streams, and device
 *
 * @param adev
 */
static void lock_all_outputs(struct audio_device *adev)
{
    enum output_type type;
    pthread_mutex_lock(&adev->lock_outputs);
    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        struct stream_out *out = adev->outputs[type];
        if (out)
            pthread_mutex_lock(&out->lock);
    }
    pthread_mutex_lock(&adev->lock);
}

/**
 * @brief unlock_all_outputs
 * unlock device, all output streams (except specified stream), and outputs list
 *
 * @param adev
 * @param except
 */
static void unlock_all_outputs(struct audio_device *adev, struct stream_out *except)
{
    /* unlock order is irrelevant, but for cleanliness we unlock in reverse order */
    pthread_mutex_unlock(&adev->lock);
    enum output_type type = OUTPUT_TOTAL;
    do {
        struct stream_out *out = adev->outputs[--type];
        if (out && out != except)
            pthread_mutex_unlock(&out->lock);
    } while (type != (enum output_type) 0);
    pthread_mutex_unlock(&adev->lock_outputs);
}

/**
 * @brief out_standby
 *
 * @param stream
 *
 * @returns
 */
static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;

    lock_all_outputs(adev);

    do_out_standby(out);

    unlock_all_outputs(adev, NULL);

    return 0;
}

/**
 * @brief out_dump
 *
 * @param stream
 * @param fd
 *
 * @returns
 */
int out_dump(const struct audio_stream *stream, int fd)
{
    struct stream_out *out = (struct stream_out *)stream;

    ALOGD("out->Device     : 0x%x", out->device);
    ALOGD("out->SampleRate : %d", out->config.rate);
    ALOGD("out->Channels   : %d", out->config.channels);
    ALOGD("out->Format     : %d", out->config.format);
    ALOGD("out->PreiodSize : %d", out->config.period_size);
    return 0;
}
/**
 * @brief out_set_parameters
 *
 * @param stream
 * @param kvpairs
 *
 * @returns
 */
static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    int status = 0;
    unsigned int val;

    ALOGD("%s: kvpairs = %s", __func__, kvpairs);

    parms = str_parms_create_str(kvpairs);

    //set channel_mask
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_CHANNELS,
                            value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        out->aud_config.channel_mask = val;
    }
    // set sample rate
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_SAMPLING_RATE,
                            value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        out->aud_config.sample_rate = val;
    }
    // set format
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_FORMAT,
                            value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        out->aud_config.format = val;
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    lock_all_outputs(adev);
    if (ret >= 0) {
        val = atoi(value);
        /* Don't switch HDMI audio in box products */
        if ((val != 0) && ((out->device & val) != val) ||
            (val != 0) && !(out->device & AUDIO_DEVICE_OUT_HDMI)) {
            /* Force standby if moving to/from SPDIF or if the output
             * device changes when in SPDIF mode */
            if (((val & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) ^
                    (adev->out_device & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET)) ||
                    (adev->out_device & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET)) {
                do_out_standby(out);
            }

            /* force output standby to start or stop SCO pcm stream if needed */
            if ((val & AUDIO_DEVICE_OUT_ALL_SCO) ^
                    (out->device & AUDIO_DEVICE_OUT_ALL_SCO)) {
                do_out_standby(out);
            }

            if (!out->standby && (out == adev->outputs[OUTPUT_HDMI_MULTI] ||
                                  !adev->outputs[OUTPUT_HDMI_MULTI] ||
                                  adev->outputs[OUTPUT_HDMI_MULTI]->standby)) {
                adev->out_device = output_devices(out) | val;
#ifndef RK3228
                do_out_standby(out);
#endif
            }
            out->device = val;
        }
    }
    out->use_default_config = false;
    unlock_all_outputs(adev, NULL);

    str_parms_destroy(parms);

    ALOGV("%s: exit: status(%d)", __func__, status);
    return status;

}

/*
 * function: get support formats
 * Query supported formats. The response is a '|' separated list of strings from audio_format_t enum
 *  e.g: "sup_formats=AUDIO_FORMAT_PCM_16_BIT"
 */

static int stream_get_parameter_formats(const struct audio_stream *stream,
                                    struct str_parms *query,
                                    struct str_parms *reply)
{
    struct stream_out *out = (struct stream_out *)stream;
    int avail = 1024;
    char value[avail];
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        memset(value,0,avail);
        // set support pcm 16 bit default
        strcat(value, "AUDIO_FORMAT_PCM_16_BIT");
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_FORMATS, value);
        return 0;
    }

    return -1;
}


/*
 * function: get support channels
 * Query supported channel masks. The response is a '|' separated list of strings from
 * audio_channel_mask_t enum
 * e.g: "sup_channels=AUDIO_CHANNEL_OUT_STEREO|AUDIO_CHANNEL_OUT_MONO"
 */

static int stream_get_parameter_channels(struct str_parms *query,
                                    struct str_parms *reply,
                                    audio_channel_mask_t *supported_channel_masks)
{
    char value[1024];
    size_t i, j;
    bool first = true;

    if(str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)){
        value[0] = '\0';
        i = 0;
        /* the last entry in supported_channel_masks[] is always 0 */
        while (supported_channel_masks[i] != 0) {
            for (j = 0; j < ARRAY_SIZE(channels_name_to_enum_table); j++) {
                if (channels_name_to_enum_table[j].value == supported_channel_masks[i]) {
                    if (!first) {
                        strcat(value, "|");
                    }

                    strcat(value, channels_name_to_enum_table[j].name);
                    first = false;
                    break;
                }
            }
            i++;
        }
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value);
        return 0;
    }

    return -1;
}

/*
 * function: get support sample_rates
 * Query supported sampling rates. The response is a '|' separated list of integer values
 * e.g: ""sup_sampling_rates=44100|48000"
 */

static int stream_get_parameter_rates(struct str_parms *query,
                                struct str_parms *reply,
                                uint32_t *supported_sample_rates)
{
    char value[256];
    int ret = -1;

    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        value[0] = '\0';
        int cursor = 0;
        int i = 0;
        while(supported_sample_rates[i]){
            int avail = sizeof(value) - cursor;
            ret = snprintf(value + cursor, avail, "%s%d",
                           cursor > 0 ? "|" : "",
                           supported_sample_rates[i]);

            if (ret < 0 || ret > avail){
                value[cursor] = '\0';
                break;
            }

            cursor += ret;
            ++i;
        }
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES, value);
        return 0;
    }
    return -1;
}

/**
 * @brief out_get_parameters
 *
 * @param stream
 * @param keys
 *
 * @returns
 */
static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    ALOGD("%s: keys = %s", __func__, keys);

    struct stream_out *out = (struct stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str = NULL;
    struct str_parms *reply = str_parms_create();
    out->use_default_config = true;

    if (stream_get_parameter_formats(stream,query,reply) == 0) {
        str = str_parms_to_str(reply);
    } else if (stream_get_parameter_channels(query, reply, &out->supported_channel_masks[0]) == 0) {
        str = str_parms_to_str(reply);
    } else if (stream_get_parameter_rates(query, reply, &out->supported_sample_rates[0]) == 0) {
        str = str_parms_to_str(reply);
    } else {
        ALOGD("%s,str_parms_get_str failed !",__func__);
        str = strdup("");
    }
    str_parms_destroy(query);
    str_parms_destroy(reply);

    ALOGV("%s,exit -- str = %s",__func__,str);
    return str;
}

/**
 * @brief out_get_latency
 *
 * @param stream
 *
 * @returns
 */
static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return (out->config.period_size * out->config.period_count * 1000) /
           out->config.rate;
}

/**
 * @brief out_set_volume
 *
 * @param stream
 * @param left
 * @param right
 *
 * @returns
 */
static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    out->volume[0] = left;
    out->volume[1] = right;

    /* The mutex lock is not needed, because the client
     * is not allowed to close the stream concurrently with this API
     *  pthread_mutex_lock(&adev->lock_outputs);
     */
    bool is_HDMI = out == adev->outputs[OUTPUT_HDMI_MULTI];
    /*  pthread_mutex_unlock(&adev->lock_outputs); */
    if (is_HDMI) {
        /* only take left channel into account: the API is for stereo anyway */
        out->muted = (left == 0.0f);
        return 0;
    }
    return -ENOSYS;
}
/**
 * @brief dump_out_data
 *
 * @param buffer bytes
 */
static void dump_out_data(const void* buffer,size_t bytes)
{
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.audio.record", value, "0");
    int size = atoi(value);
    if (size <= 0)
        return ;

    ALOGD("dump pcm file.");
    static FILE* fd = NULL;
    static int offset = 0;
    if (fd == NULL) {
        fd=fopen("/data/misc/audioserver/debug.pcm","wb+");
        if(fd == NULL) {
            ALOGD("DEBUG open /data/debug.pcm ,errno = %s",strerror(errno));
            offset = 0;
        }
    }

    if (fd != NULL) {
        fwrite(buffer,bytes,1,fd);
        offset += bytes;
        fflush(fd);
        if(offset >= size*1024*1024) {
            fclose(fd);
            fd = NULL;
            offset = 0;
            property_set("vendor.audio.record", "0");
            ALOGD("TEST playback pcmfile end");
        }
    }
}

static void dump_in_data(const void* buffer, size_t bytes)
{
    static int offset = 0;
    static FILE* fd = NULL;
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.audio.record.in", value, "0");
    int size = atoi(value);
    if (size > 0) {
        if(fd == NULL) {
            fd=fopen("/data/misc/audioserver/debug_in.pcm","wb+");
            if(fd == NULL) {
                ALOGD("DEBUG open /data/misc/audioserver/debug_in.pcm ,errno = %s",strerror(errno));
            } else {
                ALOGD("dump pcm to file /data/misc/audioserver/debug_in.pcm");
            }
            offset = 0;
        }
    }

    if (fd != NULL) {
        ALOGD("dump in pcm %zu bytes", bytes);
        fwrite(buffer,bytes,1,fd);
        offset += bytes;
        fflush(fd);
        if (offset >= size*1024*1024) {
            fclose(fd);
            fd = NULL;
            offset = 0;
            property_set("vendor.audio.record.in", "0");
            ALOGD("TEST record pcmfile end");
        }
    }
}


static void check_hdmi_reconnect(struct stream_out *out)
{
    if (out == NULL) {
        return ;
    }

    struct audio_device *adev = out->dev;
    lock_all_outputs(adev);
    /*
     * if snd_reopen is set to true, this means we need to reopen sound card.
     * There are a situation, we need to do this:
     *   current stream is bistream over hdmi, and hdmi is unpluged and plug later,
     *   the driver of hdmi may init the hdmi in pcm mode automatically, according the
     *   implement of driver of hdmi. If we contiune send bitstream to hdmi open in pcm mode,
     *   hdmi may make noies or mute.
     */
    if (out->snd_reopen && !out->standby)
    {
        /*
         * standby sound cards
         * the driver of hdmi will auto init with last configurations,
         * so, we don't need close and reopen sound card of hdmi here.
         * If driver of hdmi not config the hdmi with last output configurations,
         * please open this codes to close and reopen sound card of hdmi.
         */
  //      do_out_standby(out);
  //      reset_bitstream_buf(out);
    }
    unlock_all_outputs(adev,NULL);
    /*
     * audio hal recived the msg of hdmi plugin, and other part of sdk will reviced it too.
     * Other part(maybe hwc) will config hdmi after it reviced the msg.
     * Audio must wait other part(maybe hwc) codes config hdmi finish, before send bitstream datas to hdmi
     */
    if (out->snd_reopen && is_bitstream(out) && (out->device == AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
#ifdef USE_DRM
        const char* PATH = "/sys/class/drm/card0-HDMI-A-1/enabled";
#else
        const char* PATH = "/sys/class/display/HDMI/enabled";
#endif
        if (access(PATH, R_OK) != 0) {
            /*
             * in most test, the time is 700~800ms between received msg of hdmi plug in
             * and hdmi init finish, so we sleep 1 sec here if no way to get the status of hdmi.
             */
            usleep(1000000);
        } else {
            /*
             * read this node to judge the status of hdmi is config finish?
             */
            char buffer[1024];
            int counter  = 200;
            FILE* file = NULL;
            while (counter >= 0 && ((file = fopen(PATH,"r")) != NULL)) {
                int size = fread(buffer,1,sizeof(buffer),file);
                if(size >= 0) {
                    if(strstr(buffer,"enabled")) {
                        fclose(file);
                        usleep(10000);
                        break;
                    }
                }
                usleep(10000);
                counter --;
                fclose(file);
            }
        }
        ALOGD("%s: out = %p",__FUNCTION__,out);
        out->snd_reopen = false;
    }
}

static void out_mute_data(struct stream_out *out,void* buffer,size_t bytes)
{
    struct audio_device *adev = out->dev;
    bool mute = false;

#ifdef MUTE_WHEN_SCREEN_OFF
    mute = adev->screenOff;
#endif
    // for some special customer
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.audio.mute", value, "false");
    if (!strcasecmp(value,"true")) {
        mute = true;
    }

    if (out->muted || mute){
        memset((void *)buffer, 0, bytes);
    }
}

static int bitstream_write_data(struct stream_out *out, void* buffer, size_t bytes)
{
    if ((out == NULL) || (buffer == NULL) || (bytes <= 0)) {
        ALOGD("%s: %d, input parameter is invalid",__FUNCTION__,__LINE__);
        return -1;
    }

    struct audio_device *adev = out->dev;
    int ret = 0;
    if ((out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL) && (is_multi_pcm(out) || is_bitstream(out))) {
        int card = adev->dev_out[SND_OUT_SOUND_CARD_HDMI].card;
        if ((card != SND_OUT_SOUND_CARD_UNKNOWN) && (out->pcm[SND_OUT_SOUND_CARD_HDMI] != NULL)) {
            if(out->config.format == PCM_FORMAT_S16_LE){
                out_mute_data(out,buffer,bytes);
                dump_out_data(buffer, bytes);
                ret = pcm_write(out->pcm[SND_OUT_SOUND_CARD_HDMI], (void *)buffer, bytes);
            } else if(out->config.format == PCM_FORMAT_S24_LE ||
                     out->config.format == PCM_FORMAT_IEC958_SUBFRAME_LE){
                char *outBuffer = NULL;
                int   outSize = 0;
                ret = bitstream_encode(out->bistream, (char*)buffer, (int)bytes, &outBuffer, &outSize);
                if (ret == 0 && outSize > 0) {
                    out_mute_data(out,(void*)outBuffer, outSize);
                    dump_out_data((void*)outBuffer, outSize);
                    ret = pcm_write(out->pcm[SND_OUT_SOUND_CARD_HDMI], (void *)outBuffer, outSize);
                }
            }
        } else {
            ALOGD("%s: %d: HDMI sound card not open",__FUNCTION__,__LINE__);
            ret = -1;
        }
    }

    return ret;
}

/*
 * process volume of one multi pcm frame
 * The multi pcm output no using mixer,so the can't control by volume setting,
 * so here we process multi pcm datas with volume value.
 *
 */
static void out_multi_pcm_volume_process(struct stream_out *out, void *buffer)
{
    if ((out == NULL) || (buffer == NULL)){
        return ;
    }

    int format = out->config.format;
    int channel = out->config.channels;
    if((format == PCM_FORMAT_S16_LE)) {
        float left = out->volume[0];
        short *pcm = (short*)buffer;
        float temp = 0;
        for (int ch = 0;  ch < channel; ch++) {
            temp = (float)pcm[ch];
            pcm[ch] = (short)(temp*left);
        }
    }
}

/*
 * switch LFE and FC of one multi pcm frame
 * swtich Front Center's datas and Low Frequency datas
 * 5.1            FL+FR+FC+LFE+BL+BR
 * 5.1(side)      FL+FR+FC+LFE+SL+SR
 * 7.1            FL+FR+FC+LFE+SL+SR+BL+BR
 * the datas needed in HDMI is:
 *                FL+FR+LFE+FC+SL+SR+BL+BR
 */
static void out_multi_pcm_switch_fc_lfe(struct stream_out *out, void *buffer)
{
    if ((out == NULL) || (buffer == NULL)) {
        return ;
    }

    const int CENTER = 2;
    const int LFE    = 3;
    int channel = out->config.channels;
    int format = out->config.format;
    audio_channel_mask_t channel_mask = out->channel_mask;
    bool hasLFE = ((channel_mask & AUDIO_CHANNEL_OUT_LOW_FREQUENCY) != 0);

    if (format == PCM_FORMAT_S16_LE) {
        short *pcm = (short*)buffer;
        short temp = 0;
        if (hasLFE && ((channel == 6) || (channel == 8))) {
            // Front Center's datas
            temp = pcm[CENTER];
            // swap FC and Low Frequency Effect's datas
            pcm[CENTER] = pcm[LFE];
            pcm[LFE] = temp;
        }
    }
}

static void out_multi_pcm_process(struct stream_out *out, void * buffer, size_t len) {
    if((out == NULL) || (buffer == NULL) || (len <= 0)){
        return ;
    }

    int format = out->config.format;
    // only process PCM16
    if (format == PCM_FORMAT_S16_LE) {
        short *pcm = (short*)buffer;
        int channel = out->config.channels;
        int frames = len/audio_stream_out_frame_size(out);
        for (int frame = 0; frame < frames; frame ++){
            out_multi_pcm_volume_process(out, pcm);
            out_multi_pcm_switch_fc_lfe(out, pcm);
            pcm += channel;
        }
    }
}

/**
 * @brief out_write
 *
 * @param stream
 * @param buffer
 * @param bytes
 *
 * @returns
 */

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    int ret = 0;
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    size_t newbytes = bytes * 2;
    int i,card;
    /* FIXME This comment is no longer correct
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the output stream mutex - e.g.
     * executing out_set_parameters() while holding the hw device
     * mutex
     */
    check_hdmi_reconnect(out);
    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        pthread_mutex_unlock(&out->lock);
        lock_all_outputs(adev);
        if (!out->standby) {
            unlock_all_outputs(adev, out);
            goto false_alarm;
        }
        ret = start_output_stream(out);
        if (ret < 0) {
            unlock_all_outputs(adev, NULL);
            goto final_exit;
        }
        out->standby = false;
        unlock_all_outputs(adev, out);
    }
false_alarm:

    if (out->disabled) {
        ret = -EPIPE;
        ALOGD("%s: %d: error out = %p",__FUNCTION__,__LINE__,out);
        goto exit;
    }


#ifdef AUDIO_3A
    if (adev->voice_api != NULL) {
        int ret = 0;
        adev->voice_api->queuePlaybackBuffer(buffer, bytes);
        ret = adev->voice_api->getPlaybackBuffer(buffer, bytes);
        if (ret < 0) {
            memset((char *)buffer, 0x00, bytes);
        }
    }
#endif

    /* Write to all active PCMs */
    if ((out->device & AUDIO_DEVICE_OUT_AUX_DIGITAL) && is_bitstream(out)) {
        ret = bitstream_write_data(out, (void*)buffer, bytes);
        if(ret < 0) {
            goto exit;
        }
    } else {
        if(is_multi_pcm(out)) {
            if(out->device == AUDIO_DEVICE_OUT_AUX_DIGITAL) {
                out_multi_pcm_process(out, buffer, bytes);
            }
        }

        out_mute_data(out,(void*)buffer,bytes);
        dump_out_data(buffer, bytes);
        ret = -1;
        for (i = 0; i < SND_OUT_SOUND_CARD_MAX; i++)
            if (out->pcm[i]) {
                if (i == SND_OUT_SOUND_CARD_BT) {
                    // HARD CODE FIXME 48000 stereo -> 8000 stereo
                    size_t inFrameCount = bytes/2/2;
                    size_t outFrameCount = inFrameCount/6;
                    int16_t out_buffer[outFrameCount*2];
                    memset(out_buffer, 0x00, outFrameCount*2);

                    out->resampler->resample_from_input(out->resampler,
                                                        (const int16_t *)buffer,
                                                        &inFrameCount,
                                                        out_buffer,
                                                        &outFrameCount);

                    ret = pcm_write(out->pcm[i], (void *)out_buffer, outFrameCount*2*2);
                    if (ret != 0)
                        break;
                } else {
                    /*
                     * do not write hdmi/spdif snd sound if they are taken by other bitstream/multi channle pcm stream
                     */
                    if(((i == SND_OUT_SOUND_CARD_HDMI) && (adev->owner[SOUND_CARD_HDMI] != (int*)out) && (adev->owner[SOUND_CARD_HDMI] != NULL)) ||
                        ((i == SND_OUT_SOUND_CARD_SPDIF) && (adev->owner[SOUND_CARD_SPDIF] != (int*)out) && (adev->owner[SOUND_CARD_SPDIF] != NULL))){
                        continue;
                    }
                    ret = pcm_write(out->pcm[i], (void *)buffer, bytes);
                    if (ret != 0)
                        break;
                }
            }
    }
exit:
    pthread_mutex_unlock(&out->lock);
final_exit:
    {
        // For PCM we always consume the buffer and return #bytes regardless of ret.
        out->written += bytes / (out->config.channels * sizeof(short));
        out->nframes = out->written;
    }
    if (ret != 0) {
        ALOGV("AudioData write  error , keep slience! ret = %d", ret);
        usleep((int64_t)bytes * 1000000ll / audio_stream_out_frame_size(stream) /
               out_get_sample_rate(&stream->common));
    }

    return bytes;
}

/**
 * @brief out_get_render_position
 *
 * @param stream
 * @param dsp_frames
 *
 * @returns
 */
static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    struct stream_out *out = (struct stream_out *)stream;

    *dsp_frames = out->nframes;
    return 0;
}

/**
 * @brief out_add_audio_effect
 *
 * @param stream
 * @param effect
 *
 * @returns
 */
static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

/**
 * @brief out_remove_audio_effect
 *
 * @param stream
 * @param effect
 *
 * @returns
 */
static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

/**
 * @brief out_get_next_write_timestamp
 *
 * @param stream
 * @param timestamp
 *
 * @returns
 */
static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    ALOGV("%s: %d Entered", __FUNCTION__, __LINE__);
    return -ENOSYS;
}

/**
 * @brief out_get_presentation_position
 *
 * @param stream
 * @param frames
 * @param timestamp
 *
 * @returns
 */
static int out_get_presentation_position(const struct audio_stream_out *stream,
        uint64_t *frames, struct timespec *timestamp)
{
    struct stream_out *out = (struct stream_out *)stream;
    int ret = -1;

    pthread_mutex_lock(&out->lock);

    int i;
    // There is a question how to implement this correctly when there is more than one PCM stream.
    // We are just interested in the frames pending for playback in the kernel buffer here,
    // not the total played since start.  The current behavior should be safe because the
    // cases where both cards are active are marginal.
    for (i = 0; i < SND_OUT_SOUND_CARD_MAX; i++)
        if (out->pcm[i]) {
            size_t avail;
            //ALOGD("===============%s,%d==============",__FUNCTION__,__LINE__);
            if (pcm_get_htimestamp(out->pcm[i], &avail, timestamp) == 0) {
                size_t kernel_buffer_size = out->config.period_size * out->config.period_count;
                //ALOGD("===============%s,%d==============",__FUNCTION__,__LINE__);
                // FIXME This calculation is incorrect if there is buffering after app processor
                int64_t signed_frames = out->written - kernel_buffer_size + avail;
                //signed_frames -= 17;
                //ALOGV("============singed_frames:%lld=======",signed_frames);
                //ALOGV("============timestamp:%lld==========",timestamp);
                // It would be unusual for this value to be negative, but check just in case ...
                if (signed_frames >= 0) {
                    *frames = signed_frames;
                    ret = 0;
                }
                break;
            }
        }
    pthread_mutex_unlock(&out->lock);

    return ret;
}

/**
 * @brief in_get_sample_rate
 * audio_stream_in implementation
 *
 * @param stream
 *
 * @returns
 */
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    //ALOGV("%s:get requested_rate : %d ",__FUNCTION__,in->requested_rate);
    return in->requested_rate;
}

/**
 * @brief in_set_sample_rate
 *
 * @param stream
 * @param rate
 *
 * @returns
 */
static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

/**
 * @brief in_get_channels
 *
 * @param stream
 *
 * @returns
 */
static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    //ALOGV("%s:get channel_mask : %d ",__FUNCTION__,in->channel_mask);
    return in->channel_mask;
}


/**
 * @brief in_get_buffer_size
 *
 * @param stream
 *
 * @returns
 */
static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return get_input_buffer_size(in->requested_rate,
                                 AUDIO_FORMAT_PCM_16_BIT,
                                 audio_channel_count_from_in_mask(in_get_channels(stream)),
                                 (in->flags & AUDIO_INPUT_FLAG_FAST) != 0);
}

/**
 * @brief in_get_format
 *
 * @param stream
 *
 * @returns
 */
static audio_format_t in_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

/**
 * @brief in_set_format
 *
 * @param stream
 * @param format
 *
 * @returns
 */
static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -ENOSYS;
}

/**
 * @brief do_in_standby
 * must be called with in stream and hw device mutex locked
 *
 * @param in
 */
static void do_in_standby(struct stream_in *in)
{
    struct audio_device *adev = in->dev;

    if (!in->standby) {
        pcm_close(in->pcm);
        in->pcm = NULL;

        if (in->device & AUDIO_DEVICE_IN_HDMI) {
            route_pcm_close(HDMI_IN_CAPTURE_OFF_ROUTE);
        }

        in->dev->input_source = AUDIO_SOURCE_DEFAULT;
        in->dev->in_device = AUDIO_DEVICE_NONE;
        in->dev->in_channel_mask = 0;
        in->standby = true;
        route_pcm_close(CAPTURE_OFF_ROUTE);
    }

}

/**
 * @brief in_standby
 *
 * @param stream
 *
 * @returns
 */
static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&in->dev->lock);

    do_in_standby(in);

    pthread_mutex_unlock(&in->dev->lock);
    pthread_mutex_unlock(&in->lock);

    return 0;
}

/**
 * @brief in_dump
 *
 * @param stream
 * @param fd
 *
 * @returns
 */
int in_dump(const struct audio_stream *stream, int fd)
{
    struct stream_in *in = (struct stream_in *)stream;

    ALOGD("in->Device     : 0x%x", in->device);
    ALOGD("in->SampleRate : %d", in->config->rate);
    ALOGD("in->Channels   : %d", in->config->channels);
    ALOGD("in->Formate    : %d", in->config->format);
    ALOGD("in->PreiodSize : %d", in->config->period_size);

    return 0;
}

/**
 * @brief in_set_parameters
 *
 * @param stream
 * @param kvpairs
 *
 * @returns
 */
static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    struct str_parms *parms;
    char value[32];
    int ret;
    int status = 0;
    unsigned int val;
    bool apply_now = false;

    ALOGV("%s: kvpairs = %s", __func__, kvpairs);
    parms = str_parms_create_str(kvpairs);

    //set channel_mask
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_CHANNELS,
                            value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        in->channel_mask = val;
    }
     // set sample rate
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_SAMPLING_RATE,
                            value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        in->requested_rate = val;
    }

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&adev->lock);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE,
                            value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        /* no audio source uses val == 0 */
        if ((in->input_source != val) && (val != 0)) {
            in->input_source = val;
            apply_now = !in->standby;
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING,
                            value, sizeof(value));
    if (ret >= 0) {
        /* strip AUDIO_DEVICE_BIT_IN to allow bitwise comparisons */
        val = atoi(value) & ~AUDIO_DEVICE_BIT_IN;
        /* no audio device uses val == 0 */
        if ((in->device != val) && (val != 0)) {
            channel_check_start(in);
            /* force output standby to start or stop SCO pcm stream if needed */
            if ((val & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) ^
                    (in->device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET)) {
                do_in_standby(in);
            }
            in->device = val;
            apply_now = !in->standby;
        }
    }

    if (apply_now) {
        adev->input_source = in->input_source;
        adev->in_device = in->device;
        route_pcm_open(getRouteFromDevice(in->device | AUDIO_DEVICE_BIT_IN));
    }

    pthread_mutex_unlock(&adev->lock);
    pthread_mutex_unlock(&in->lock);

    str_parms_destroy(parms);

    ALOGV("%s: exit: status(%d)", __func__, status);
    return status;

}

/**
 * @brief in_get_parameters
 *
 * @param stream
 * @param keys
 *
 * @returns
 */
static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    ALOGD("%s: keys = %s", __func__, keys);

    struct stream_in *in = (struct stream_in *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str = NULL;
    struct str_parms *reply = str_parms_create();

    if (stream_get_parameter_formats(stream,query,reply) == 0) {
        str = str_parms_to_str(reply);
    } else if (stream_get_parameter_channels(query, reply, &in->supported_channel_masks[0]) == 0) {
        str = str_parms_to_str(reply);
    } else if (stream_get_parameter_rates(query, reply, &in->supported_sample_rates[0]) == 0) {
        str = str_parms_to_str(reply);
    } else {
        ALOGD("%s,str_parms_get_str failed !",__func__);
        str = strdup("");
    }

    str_parms_destroy(query);
    str_parms_destroy(reply);

    ALOGV("%s,exit -- str = %s",__func__,str);
    return str;
}

/**
 * @brief in_set_gain
 *
 * @param stream
 * @param gain
 *
 * @returns
 */
static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

/**
 * @brief in_apply_ramp
 *
 * @param in
 * @param buffer
 * @param frames
 */
static void in_apply_ramp(struct stream_in *in, int16_t *buffer, size_t frames)
{
    size_t i;
    uint16_t vol = in->ramp_vol;
    uint16_t step = in->ramp_step;

    frames = (frames < in->ramp_frames) ? frames : in->ramp_frames;

    if (in->channel_mask == AUDIO_CHANNEL_IN_MONO)
        for (i = 0; i < frames; i++) {
            buffer[i] = (int16_t)((buffer[i] * vol) >> 16);
            vol += step;
        }
    else
        for (i = 0; i < frames; i++) {
            buffer[2*i] = (int16_t)((buffer[2*i] * vol) >> 16);
            buffer[2*i + 1] = (int16_t)((buffer[2*i + 1] * vol) >> 16);
            vol += step;
        }


    in->ramp_vol = vol;
    in->ramp_frames -= frames;
}

/**
 * @brief in_read
 *
 * @param stream
 * @param buffer
 * @param bytes
 *
 * @returns
 */
static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    int ret = 0;
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    size_t frames_rq = bytes / audio_stream_in_frame_size(stream);
    size_t frames_rd = 0;

    if (in->device & AUDIO_DEVICE_IN_HDMI) {
        unsigned int rate = get_hdmiin_audio_rate(adev);
        if(rate != in->config->rate){
            ALOGD("HDMI-In: rate is changed: %d -> %d, restart input stream",
                    in->config->rate, rate);
            do_in_standby(in);
        }
    }
    /*
     * acquiring hw device mutex systematically is useful if a low
     * priority thread is waiting on the input stream mutex - e.g.
     * executing in_set_parameters() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&in->lock);
    if (in->standby) {
        pthread_mutex_lock(&adev->lock);
        ret = start_input_stream(in);
        pthread_mutex_unlock(&adev->lock);
        if (ret < 0)
            goto exit;
        in->standby = false;
#ifdef AUDIO_3A
        if (adev->voice_api != NULL) {
            adev->voice_api->start();
        }
#endif
    }

    /*if (in->num_preprocessors != 0)
        ret = process_frames(in, buffer, frames_rq);
      else */
    //ALOGV("%s:frames_rq:%d",__FUNCTION__,frames_rq);
    frames_rd = read_frames(in, buffer, frames_rq);
    if (in->read_status) {
        ret = -EPIPE;
        goto exit;
    } else if (frames_rd > 0) {
        in->frames_read += frames_rd;
        bytes = frames_rd * audio_stream_in_frame_size(stream);
    }

    dump_in_data(buffer, bytes);

#ifdef AUDIO_3A
    do {
        if (adev->voice_api != NULL) {
            int ret  = 0;
            ret = adev->voice_api->quueCaputureBuffer(buffer, bytes);
            if (ret < 0) break;
            ret = adev->voice_api->getCapureBuffer(buffer, bytes);
            if (ret < 0) memset(buffer, 0x00, bytes);
        }
    } while (0);
#endif

    //if (in->ramp_frames > 0)
    //    in_apply_ramp(in, buffer, frames_rq);

    /*
     * Instead of writing zeroes here, we could trust the hardware
     * to always provide zeroes when muted.
     */
    //if (ret == 0 && adev->mic_mute)
    //    memset(buffer, 0, bytes);
#ifdef ALSA_IN_DEBUG
        fwrite(buffer, bytes, 1, in_debug);
#endif
exit:
    if (ret < 0) {
        memset(buffer, 0, bytes);
        usleep(bytes * 1000000 / audio_stream_in_frame_size(stream) /
               in_get_sample_rate(&stream->common));
        do_in_standby(in);
    }

    pthread_mutex_unlock(&in->lock);
    return bytes;
}

/**
 * @brief in_get_input_frames_lost
 *
 * @param stream
 *
 * @returns
 */
static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

/**
 * @brief in_add_audio_effect
 *
 * @param stream
 * @param effect
 *
 * @returns
 */
static int in_add_audio_effect(const struct audio_stream *stream,
                               effect_handle_t effect)
{
    struct stream_in *in = (struct stream_in *)stream;
    effect_descriptor_t descr;
    if ((*effect)->get_descriptor(effect, &descr) == 0) {

        pthread_mutex_lock(&in->lock);
        pthread_mutex_lock(&in->dev->lock);


        pthread_mutex_unlock(&in->dev->lock);
        pthread_mutex_unlock(&in->lock);
    }

    return 0;
}

/**
 * @brief in_remove_audio_effect
 *
 * @param stream
 * @param effect
 *
 * @returns
 */
static int in_remove_audio_effect(const struct audio_stream *stream,
                                  effect_handle_t effect)
{
    struct stream_in *in = (struct stream_in *)stream;
    effect_descriptor_t descr;
    if ((*effect)->get_descriptor(effect, &descr) == 0) {

        pthread_mutex_lock(&in->lock);
        pthread_mutex_lock(&in->dev->lock);


        pthread_mutex_unlock(&in->dev->lock);
        pthread_mutex_unlock(&in->lock);
    }

    return 0;
}

static int adev_get_microphones(const struct audio_hw_device *dev,
                         struct audio_microphone_characteristic_t *mic_array,
                         size_t *mic_count)
{
    struct audio_device *adev = (struct audio_device *)dev;
    size_t actual_mic_count = 0;

    int card_no = 0;

    char snd_card_node_id[100]={0};
    char snd_card_node_cap[100]={0};
    char address[32] = "bottom";

    do{
        sprintf(snd_card_node_id, "/proc/asound/card%d/id", card_no);
        if (access(snd_card_node_id,F_OK) == -1) break;

        sprintf(snd_card_node_cap, "/proc/asound/card%d/pcm0c/info", card_no);
        if (access(snd_card_node_cap,F_OK) == -1) continue;

        actual_mic_count++;
    }while(++card_no);

    mic_array->device = -2147483644;
    strcpy(mic_array->address,address);

    ALOGD("%s,get capture mic actual_mic_count =%d",__func__,actual_mic_count);
    *mic_count = actual_mic_count;
    return 0;
}

static int in_get_capture_position(const struct audio_stream_in *stream,
                        int64_t *frames, int64_t *time)
{
    if (stream == NULL || frames == NULL || time == NULL) {
        return -EINVAL;
    }
    struct stream_in *in = (struct stream_in *)stream;
    int ret = -ENOSYS;

    pthread_mutex_lock(&in->lock);
    // note: ST sessions do not close the alsa pcm driver synchronously
    // on standby. Therefore, we may return an error even though the
    // pcm stream is still opened.
    if (in->standby) {
        ALOGD("skip when standby is true.");
        goto exit;
    }
    if (in->pcm) {
        struct timespec timestamp;
        size_t avail;
        if (pcm_get_htimestamp(in->pcm, &avail, &timestamp) == 0) {
            *frames = in->frames_read + avail;
            *time = timestamp.tv_sec * 1000000000LL + timestamp.tv_nsec;
            ret = 0;
            ALOGD("Pos: %lld %lld", *time, *frames);
        }
    }
exit:
    pthread_mutex_unlock(&in->lock);

    return ret;
}

static int in_get_active_microphones(const struct audio_stream_in *stream,
                         struct audio_microphone_characteristic_t *mic_array,
                         size_t *mic_count)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&adev->lock);

    size_t actual_mic_count = 0;
    int card_no = 0;

    char snd_card_node_id[100]={0};
    char snd_card_node_cap[100]={0};
    char snd_card_info[100]={0};
    char snd_card_state[255]={0};

    do{
        sprintf(snd_card_node_id, "/proc/asound/card%d/id", card_no);
        if (access(snd_card_node_id,F_OK) == -1) break;

        sprintf(snd_card_node_cap, "/proc/asound/card%d/pcm0c/info", card_no);
        if (access(snd_card_node_cap,F_OK) == -1) {
            continue;
        } else {
            sprintf(snd_card_info, "/proc/asound/card%d/pcm0c/sub0/status", card_no);
            int fd;
            fd = open(snd_card_info, O_RDONLY);
            if (fd < 0) {
                ALOGE("%s,failed to open node: %s", __func__, snd_card_info);
            } else {
                int length = read(fd, snd_card_state, sizeof(snd_card_state) -1);
                snd_card_state[length] = 0;
                if (strcmp(snd_card_state, "closed") != 0) actual_mic_count++;
            }
            close(fd);
        }
    } while(++card_no);

    pthread_mutex_unlock(&adev->lock);
    pthread_mutex_unlock(&in->lock);

    ALOGD("%s,get active mic actual_mic_count =%d", __func__, actual_mic_count);
    *mic_count = actual_mic_count;
    return 0;
}

/*
 * get support channels mask of hdmi from parsing edid of hdmi
 */
static void get_hdmi_support_channels_masks(struct stream_out *out)
{
    if(out == NULL)
        return ;

    int channels = get_hdmi_audio_speaker_allocation(&out->hdmi_audio);
    switch (channels) {
    case AUDIO_CHANNEL_OUT_5POINT1:
        ALOGD("%s: HDMI Support 5.1 channels pcm",__FUNCTION__);
        out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_5POINT1;
        out->supported_channel_masks[1] = AUDIO_CHANNEL_OUT_STEREO;
        break;
    case AUDIO_CHANNEL_OUT_7POINT1:
        ALOGD("%s: HDMI Support 7.1 channels pcm",__FUNCTION__);
        out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_5POINT1;
        out->supported_channel_masks[1] = AUDIO_CHANNEL_OUT_7POINT1;
        break;
    case AUDIO_CHANNEL_OUT_STEREO:
    default:
        ALOGD("%s: HDMI Support 2 channels pcm",__FUNCTION__);
        out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_STEREO;
        out->supported_channel_masks[1] = AUDIO_CHANNEL_OUT_MONO;
        break;
    }
}

/**
 * @brief adev_open_output_stream
 *
 * @param dev
 * @param handle
 * @param devices
 * @param flags
 * @param config
 * @param stream_out
 * @param __unused
 *
 * @returns
 */
static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address __unused)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_out *out;
    int ret;
    enum output_type type = OUTPUT_LOW_LATENCY;
    bool isPcm = audio_is_linear_pcm(config->format);

    ALOGD("audio hal adev_open_output_stream devices = 0x%x, flags = %d, samplerate = %d,format = 0x%x",
          devices, flags, config->sample_rate,config->format);
    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (!out)
        return -ENOMEM;

    /*get default supported channel_mask*/
    memset(out->supported_channel_masks, 0, sizeof(out->supported_channel_masks));
    out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_STEREO;
    out->supported_channel_masks[1] = AUDIO_CHANNEL_OUT_MONO;
    /*get default supported sample_rate*/
    memset(out->supported_sample_rates, 0, sizeof(out->supported_sample_rates));
    out->supported_sample_rates[0] = 44100;
    out->supported_sample_rates[1] = 48000;

    if(config != NULL)
        memcpy(&(out->aud_config),config,sizeof(struct audio_config));
    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    if (devices == AUDIO_DEVICE_NONE)
        devices = AUDIO_DEVICE_OUT_SPEAKER;
    out->device = devices;
    /*
     * set output_direct_mode to LPCM, means data is not multi pcm or bitstream datas.
     * set output_direct to false, means data is 2 channels pcm
     */
    out->output_direct_mode = LPCM;
    out->output_direct = false;
    out->snd_reopen = false;
    out->use_default_config = false;
    out->volume[0] = out->volume[1] = 1.0f;
    out->bistream = NULL;

    init_hdmi_audio(&out->hdmi_audio);
    if(devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
        parse_hdmi_audio(&out->hdmi_audio);
        get_hdmi_support_channels_masks(out);
    }

    if (flags & AUDIO_OUTPUT_FLAG_DIRECT) {
        if (devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
            if (config->format == AUDIO_FORMAT_IEC61937) {
                ALOGD("%s:out = %p HDMI Bitstream",__FUNCTION__,out);
                out->channel_mask = config->channel_mask;
                if (isValidSamplerate(config->sample_rate)) {
                    out->config = pcm_config_direct;
                    out->config.rate = config->sample_rate;
                    out->output_direct = true;
                    int channel = audio_channel_count_from_out_mask(config->channel_mask);

                    if (channel == 8 && config->sample_rate == 192000) {
                        out->output_direct_mode = HBR;
                    } else {
                        out->output_direct_mode = NLPCM;
                    }

                    if (out->config.format == PCM_FORMAT_S24_LE) {
                        if (config->sample_rate >= 176400) {
                            out->config.period_size = 1024 * 4;
                        } else {
                            out->config.period_size = 2048;
                        }
                    } else {
                        out->config.period_size = config->sample_rate/100;   // 10ms
                    }

                    #ifdef RK3128  // only 3128 using 16bit to bitstream
                    out->config.format = PCM_FORMAT_S16_LE;
                    #endif
                    type = OUTPUT_HDMI_MULTI;
                } else {
                    out->config = pcm_config;
                    out->config.rate = 44100;
                    ALOGE("hdmi bitstream samplerate %d unsupport", config->sample_rate);
                }
                out->config.channels = audio_channel_count_from_out_mask(config->channel_mask);
                if (out->config.channels < 2)
                    out->config.channels = 2;
                out->pcm_device = PCM_DEVICE;
                out->device = AUDIO_DEVICE_OUT_AUX_DIGITAL;
            } else if (isPcm){ // multi pcm
                if (config->sample_rate == 0)
                    config->sample_rate = HDMI_MULTI_DEFAULT_SAMPLING_RATE;
                if (config->channel_mask == 0)
                    config->channel_mask = AUDIO_CHANNEL_OUT_5POINT1;

                int layout = get_hdmi_audio_speaker_allocation(&out->hdmi_audio);
                unsigned int mask = (layout&config->channel_mask);
                ALOGD("%s:out = %p HDMI multi pcm: layout = 0x%x,mask = 0x%x",
                    __FUNCTION__,out,layout,mask);
                // current hdmi allocation(speaker) only support MONO or STEREO
                if(mask <= (int)AUDIO_CHANNEL_OUT_STEREO) {
                    ALOGD("%s:out = %p input stream is multi pcm,channle mask = 0x%x,but hdmi not support,mixer it to stereo output",
                        __FUNCTION__,out,config->channel_mask);
                    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
                    out->config = pcm_config;
                    out->pcm_device = PCM_DEVICE;
                    type = OUTPUT_LOW_LATENCY;
                    out->device = AUDIO_DEVICE_OUT_AUX_DIGITAL;
                    out->output_direct = false;
                } else {
                    /*
                     * maybe input audio stream is 7.1 channels,
                     * but hdmi only support 5.1, we also output 7.1 for default.
                     * Is better than output 2 channels after mixer?
                     * If customer like output 2 channles data after mixer,
                     * modify codes here
                     */
                    out->channel_mask = config->channel_mask;
                    out->config = pcm_config_hdmi_multi;
                    out->config.rate = config->sample_rate;
                    out->config.channels = audio_channel_count_from_out_mask(config->channel_mask);
                    out->pcm_device = PCM_DEVICE;
                    type = OUTPUT_HDMI_MULTI;
                    out->device = AUDIO_DEVICE_OUT_AUX_DIGITAL;
                    out->output_direct = true;
                }
            } else {
                ALOGD("Not any bitstream mode!");
            }
        } else if ((devices & AUDIO_DEVICE_OUT_SPDIF)
                      && (config->format == AUDIO_FORMAT_IEC61937)) {
            ALOGD("%s:out = %p Spdif Bitstream",__FUNCTION__,out);
            out->channel_mask = config->channel_mask;
            out->config = pcm_config_direct;
            if ((config->sample_rate == 48000) ||
                    (config->sample_rate == 32000) ||
                    (config->sample_rate == 44100)) {
                out->config.rate = config->sample_rate;
                out->config.format = PCM_FORMAT_S16_LE;
                out->config.period_size = config->sample_rate/100; // 10ms
            } else {
                out->config.rate = 44100;
                ALOGE("spdif passthrough samplerate %d is unsupport",config->sample_rate);
            }
            out->config.channels = audio_channel_count_from_out_mask(config->channel_mask);
            devices = AUDIO_DEVICE_OUT_SPDIF;
            out->pcm_device = PCM_DEVICE;
            out->output_direct = true;
            type = OUTPUT_HDMI_MULTI;
            out->device = AUDIO_DEVICE_OUT_SPDIF;
            out->output_direct_mode = NLPCM;
        } else {
            out->config = pcm_config;
            out->pcm_device = PCM_DEVICE;
            type = OUTPUT_LOW_LATENCY;
        }
    } else if (flags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER) {
        out->config = pcm_config_deep;
        out->pcm_device = PCM_DEVICE_DEEP;
        type = OUTPUT_DEEP_BUF;
    } else {
        out->config = pcm_config;
        out->pcm_device = PCM_DEVICE;
        type = OUTPUT_LOW_LATENCY;
    }

    ALOGD("out->config.rate = %d, out->config.channels = %d out->config.format = %d",
          out->config.rate, out->config.channels, out->config.format);

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
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->stream.get_presentation_position = out_get_presentation_position;

    out->dev = adev;

    out->standby = true;
    out->nframes = 0;

    pthread_mutex_lock(&adev->lock_outputs);
    if (adev->outputs[type]) {
        pthread_mutex_unlock(&adev->lock_outputs);
        ret = -EBUSY;
        goto err_open;
    }
    adev->outputs[type] = out;
    pthread_mutex_unlock(&adev->lock_outputs);

    *stream_out = &out->stream;

    return 0;

err_open:
    if (out != NULL) {
        destory_hdmi_audio(&out->hdmi_audio);
        free(out);
    }
    *stream_out = NULL;
    return ret;
}

/**
 * @brief adev_close_output_stream
 *
 * @param dev
 * @param stream
 */
static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct audio_device *adev;
    enum output_type type;

    ALOGD("adev_close_output_stream!");
    out_standby(&stream->common);
    adev = (struct audio_device *)dev;
    pthread_mutex_lock(&adev->lock_outputs);
    for (type = 0; type < OUTPUT_TOTAL; ++type) {
        if (adev->outputs[type] == (struct stream_out *) stream) {
            adev->outputs[type] = NULL;
            break;
        }
    }

    {
        struct stream_out *out = (struct stream_out *)stream;
        destory_hdmi_audio(&out->hdmi_audio);
    }

    pthread_mutex_unlock(&adev->lock_outputs);
    free(stream);
}

/**
 * @brief adev_set_parameters
 *
 * @param dev
 * @param kvpairs
 *
 * @returns
 */
static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms = NULL;
    char value[32] = "";
    /*
     * ret is the result of str_parms_get_str,
     * if no paramter which str_parms_get_str to get, it will return result < 0 always.
     * For example: kvpairs = connect=1024 is coming
     *              str_parms_get_str(parms, AUDIO_PARAMETER_KEY_SCREEN_STATE,value, sizeof(value))
     *              will return result < 0,this means no screen_state in parms
     */
    int ret = 0;

    /*
     * status is the result of one process,
     * For example: kvpairs = screen_state=on is coming,
     *              str_parms_get_str(parms, AUDIO_PARAMETER_KEY_SCREEN_STATE,value, sizeof(value))
     *              will return result >= 0,this means screen is on, we can do something,
     *              if the things we do is correct, we set status = 0, or status < 0 means fail.
     */
    int status = 0;

    ALOGD("%s: kvpairs = %s", __func__, kvpairs);
    parms = str_parms_create_str(kvpairs);
    pthread_mutex_lock(&adev->lock);

    // screen state off/on
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_SCREEN_STATE, // screen_state
                            value, sizeof(value));
    if (ret >= 0) {
        if(strcmp(value,"on") == 0){
            adev->screenOff = false;
        } else if(strcmp(value,"off") == 0){
            adev->screenOff = true;
        }
    }

#ifdef AUDIO_BITSTREAM_REOPEN_HDMI
    // hdmi reconnect
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_DEVICE_CONNECT, // hdmi reconnect
                            value, sizeof(value));
    if (ret >= 0) {
        int device = atoi(value);
        if(device == (int)AUDIO_DEVICE_OUT_AUX_DIGITAL){
            struct stream_out *out = adev->outputs[OUTPUT_HDMI_MULTI];
            if((out != NULL) && is_bitstream(out) && (out->device == AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
                ALOGD("%s: hdmi connect when audio stream is output over hdmi, do something,out = %p",__FUNCTION__,out);
                out->snd_reopen = true;
            }

        }
    }
#endif

    pthread_mutex_unlock(&adev->lock);
    str_parms_destroy(parms);
    return status;
}

/*
 * get support formats for bitstream
 * There is no stand interface in andorid to get the formats can be bistream,
 * so we extend get parameter to report formats
 */
static int get_support_bitstream_formats(struct str_parms *query,
                                    struct str_parms *reply)
{
    int avail = 1024;
    char value[avail];

    struct hdmi_audio_infors hdmi_edid;
    init_hdmi_audio(&hdmi_edid);
    const char* AUDIO_PARAMETER_STREAM_SUP_BITSTREAM_FORMAT = "sup_bitstream_formats";
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_BITSTREAM_FORMAT)) {
        memset(value,0,avail);

        // get the format can be bistream?
        if(parse_hdmi_audio(&hdmi_edid) >= 0){
            int cursor = 0;
            for(int i = 0; i < ARRAY_SIZE(sSurroundFormat); i++){
                if(is_support_format(&hdmi_edid,sSurroundFormat[i].format)){
                    avail -= cursor;
                    int length = snprintf(value + cursor, avail, "%s%s",
                                   cursor > 0 ? "|" : "",
                                   sSurroundFormat[i].value);
                    if (length < 0 || length >= avail) {
                        break;
                    }
                    cursor += length;
                }
            }
        }

        destory_hdmi_audio(&hdmi_edid);
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_BITSTREAM_FORMAT, value);
        return 0;
    }

    return -1;
}

/**
 * @brief adev_get_parameters
 *
 * @param dev
 * @param keys
 *
 * @returns
 */
static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms = str_parms_create_str(keys);
    struct str_parms *reply = str_parms_create();
    char *str = NULL;
    ALOGD("%s: keys = %s",__FUNCTION__,keys);
    if (str_parms_has_key(parms, "ec_supported")) {
        str_parms_destroy(parms);
        parms = str_parms_create_str("ec_supported=yes");
        str = str_parms_to_str(parms);
    } else if (get_support_bitstream_formats(parms,reply) == 0) {
        str = str_parms_to_str(reply);
    } else {
        str = strdup("");
    }

    str_parms_destroy(parms);
    str_parms_destroy(reply);

    return str;
}

/**
 * @brief adev_init_check
 *
 * @param dev
 *
 * @returns
 */
static int adev_init_check(const struct audio_hw_device *dev)
{
    return 0;
}

/**
 * @brief adev_set_voice_volume
 *
 * @param dev
 * @param volume
 *
 * @returns
 */
static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    int ret = 0;
    struct audio_device *adev = (struct audio_device *)dev;
    if(adev->mode == AUDIO_MODE_IN_CALL) {
        if (volume < 0.0) {
            volume = 0.0;
        } else if (volume > 1.0) {
            volume = 1.0;
        }

        const char *mixer_ctl_name = "Speaker Playback Volume";
        ret = route_set_voice_volume(mixer_ctl_name,volume);
    }

    return ret;
}

/**
 * @brief adev_set_master_volume
 *
 * @param dev
 * @param volume
 *
 * @returns
 */
static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

/**
 * @brief adev_set_mode
 *
 * @param dev
 * @param mode
 *
 * @returns
 */
static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    struct audio_device *adev = (struct audio_device *)dev;

    ALOGD("%s: set_mode = %d", __func__, mode);
    adev->mode = mode;

    return 0;
}

/**
 * @brief adev_set_mic_mute
 *
 * @param dev
 * @param state
 *
 * @returns
 */
static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    adev->mic_mute = state;

    return 0;
}

/**
 * @brief adev_get_mic_mute
 *
 * @param dev
 * @param state
 *
 * @returns
 */
static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    *state = adev->mic_mute;

    return 0;
}

/**
 * @brief adev_get_input_buffer_size
 *
 * @param dev
 * @param config
 *
 * @returns
 */
static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
        const struct audio_config *config)
{

    return get_input_buffer_size(config->sample_rate, config->format,
                                 audio_channel_count_from_in_mask(config->channel_mask),
                                 false /* is_low_latency: since we don't know, be conservative */);
}

/**
 * @brief adev_open_input_stream
 *
 * @param dev
 * @param handle
 * @param devices
 * @param config
 * @param stream_in
 * @param flags
 * @param __unused
 * @param __unused
 *
 * @returns
 */
static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags,
                                  const char *address __unused,
                                  audio_source_t source __unused)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in;
    int ret;

    ALOGD("audio hal adev_open_input_stream devices = 0x%x, flags = %d, config->samplerate = %d,config->channel_mask = %x",
           devices, flags, config->sample_rate,config->channel_mask);

    *stream_in = NULL;
#ifdef ALSA_IN_DEBUG
    in_debug = fopen("/data/debug.pcm","wb");//please touch /data/debug.pcm first
#endif
    /* Respond with a request for mono if a different format is given. */
    //ALOGV("%s:config->channel_mask %d",__FUNCTION__,config->channel_mask);
    if (/*config->channel_mask != AUDIO_CHANNEL_IN_MONO &&
            config->channel_mask != AUDIO_CHANNEL_IN_FRONT_BACK*/
        config->channel_mask != AUDIO_CHANNEL_IN_STEREO) {
        config->channel_mask = AUDIO_CHANNEL_IN_STEREO;
        ALOGE("%s:channel is not support",__FUNCTION__);
        return -EINVAL;
    }
    if (config->sample_rate == 0 ) {
        config->sample_rate = 44100;
        ALOGW("%s: rate is not support",__FUNCTION__);
    }

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    if (!in)
        return -ENOMEM;

    /*get default supported channel_mask*/
    memset(in->supported_channel_masks, 0, sizeof(in->supported_channel_masks));
    in->supported_channel_masks[0] = AUDIO_CHANNEL_IN_STEREO;
    in->supported_channel_masks[1] = AUDIO_CHANNEL_IN_MONO;
    /*get default supported sample_rate*/
    memset(in->supported_sample_rates, 0, sizeof(in->supported_sample_rates));
    in->supported_sample_rates[0] = 44100;
    in->supported_sample_rates[1] = 48000;

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->stream.get_active_microphones = in_get_active_microphones;
    in->stream.get_capture_position = in_get_capture_position;

#ifdef RK_DENOISE_ENABLE
    in->mDenioseState = NULL;
#endif
    in->dev = adev;
    in->standby = true;
    in->requested_rate = config->sample_rate;
    in->input_source = AUDIO_SOURCE_DEFAULT;
    /* strip AUDIO_DEVICE_BIT_IN to allow bitwise comparisons */
    in->device = devices & ~AUDIO_DEVICE_BIT_IN;
    in->io_handle = handle;
    in->channel_mask = config->channel_mask;
    if (in->device & AUDIO_DEVICE_IN_HDMI) {
        ALOGD("HDMI-In: use low latency");
        flags |= AUDIO_INPUT_FLAG_FAST;
    }
    in->flags = flags;
    struct pcm_config *pcm_config = flags & AUDIO_INPUT_FLAG_FAST ?
                                            &pcm_config_in_low_latency : &pcm_config_in;
#ifdef BT_AP_SCO
    if (adev->mode == AUDIO_MODE_IN_COMMUNICATION && in->device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
        pcm_config = &pcm_config_in_bt;
    }
#endif

    in->config = pcm_config;

    in->buffer = malloc(pcm_config->period_size * pcm_config->channels
                        * audio_stream_in_frame_size(&in->stream));
    if (!in->buffer) {
        ret = -ENOMEM;
        goto err_malloc;
    }

    if ((in->requested_rate != 0) && (in->requested_rate != pcm_config->rate)) {
        in->buf_provider.get_next_buffer = get_next_buffer;
        in->buf_provider.release_buffer = release_buffer;

        ALOGD("pcm_config->rate:%d,in->requested_rate:%d,in->channel_mask:%d",
              pcm_config->rate,in->requested_rate,audio_channel_count_from_in_mask(in->channel_mask));
        ret = create_resampler(pcm_config->rate,
                               in->requested_rate,
                               audio_channel_count_from_in_mask(in->channel_mask),
                               RESAMPLER_QUALITY_DEFAULT,
                               &in->buf_provider,
                               &in->resampler);
        if (ret != 0) {
            ret = -EINVAL;
            goto err_resampler;
        }
    }

    if (in->device&AUDIO_DEVICE_IN_HDMI) {
        goto out;
    }

#ifdef AUDIO_3A
    ALOGD("voice process has opened, try to create voice process!");
    adev->voice_api = rk_voiceprocess_create(DEFAULT_PLAYBACK_SAMPLERATE,
                                             DEFAULT_PLAYBACK_CHANNELS,
                                             in->requested_rate,
                                             audio_channel_count_from_in_mask(in->channel_mask));
    if (adev->voice_api == NULL) {
        ALOGE("crate voice process failed!");
    }
#endif
out:
    *stream_in = &in->stream;
    return 0;
err_resampler:
    free(in->buffer);
err_malloc:
    free(in);
    return ret;
}

/**
 * @brief adev_close_input_stream
 *
 * @param dev
 * @param stream
 */
static void adev_close_input_stream(struct audio_hw_device *dev,
                                    struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = (struct audio_device *)dev;

    ALOGD("%s",__FUNCTION__);

    in_standby(&stream->common);
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }

#ifdef ALSA_IN_DEBUG
    fclose(in_debug);
#endif
#ifdef AUDIO_3A
    if (adev->voice_api != NULL) {
        rk_voiceprocess_destory();
        adev->voice_api = NULL;
    }
#endif

#ifdef RK_DENOISE_ENABLE
    if (in->mDenioseState)
        rkdenoise_destroy(in->mDenioseState);
    in->mDenioseState = NULL;
#endif
    free(in->buffer);
    free(stream);
}

/**
 * @brief adev_dump
 *
 * @param device
 * @param fd
 *
 * @returns
 */
static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

/**
 * @brief adev_close
 *
 * @param device
 *
 * @returns
 */
static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;

    //audio_route_free(adev->ar);


    route_uninit();

    free(device);
    return 0;
}

static void adev_open_init(struct audio_device *adev)
{
    ALOGD("%s",__func__);
    int i = 0;
    adev->mic_mute = false;
    adev->screenOff = false;

#ifdef AUDIO_3A
    adev->voice_api = NULL;
#endif

    adev->input_source = AUDIO_SOURCE_DEFAULT;

    for(i =0; i < OUTPUT_TOTAL; i++){
        adev->outputs[i] = NULL;
    }
    set_default_dev_info(adev->dev_out, SND_OUT_SOUND_CARD_MAX, 1);
    set_default_dev_info(adev->dev_in, SND_IN_SOUND_CARD_MAX, 1);
    adev->dev_out[SND_OUT_SOUND_CARD_SPEAKER].id = "SPEAKER";
    adev->dev_out[SND_OUT_SOUND_CARD_HDMI].id = "HDMI";
    adev->dev_out[SND_OUT_SOUND_CARD_SPDIF].id = "SPDIF";
    adev->dev_out[SND_OUT_SOUND_CARD_BT].id = "BT";
    adev->dev_in[SND_IN_SOUND_CARD_MIC].id = "MIC";
    adev->dev_in[SND_IN_SOUND_CARD_BT].id = "BT";
    adev->owner[0] = NULL;
    adev->owner[1] = NULL;

    char value[PROPERTY_VALUE_MAX];
    if (property_get("vendor.audio.period_size", value, NULL) > 0) {
        pcm_config.period_size = atoi(value);
        pcm_config_in.period_size = pcm_config.period_size;
    }
    if (property_get("vendor.audio.in_period_size", value, NULL) > 0)
        pcm_config_in.period_size = atoi(value);
}

/**
 * @brief adev_open
 *
 * @param module
 * @param name
 * @param device
 *
 * @returns
 */
static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct audio_device *adev;
    int ret;

    ALOGD(AUDIO_HAL_VERSION);

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct audio_device));
    if (!adev)
        return -ENOMEM;

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->hw_device.common.module = (struct hw_module_t *) module;
    adev->hw_device.common.close = adev_close;

    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream;
    adev->hw_device.close_output_stream = adev_close_output_stream;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.dump = adev_dump;
    adev->hw_device.get_microphones = adev_get_microphones;
    //adev->ar = audio_route_init(MIXER_CARD, NULL);
    //route_init();
    /* adev->cur_route_id initial value is 0 and such that first device
     * selection is always applied by select_devices() */
    *device = &adev->hw_device.common;

    adev_open_init(adev);
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
        .name = "Manta audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
