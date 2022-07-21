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
 * @file audio_hw_hdmi.c
 * @brief
 * @author  RkAudio
 * @version 1.0.8
 * @date 2015-08-24
 */

#define LOG_TAG "audio_hdmi_parser"

#include "audio_hw_hdmi.h"
#include <unistd.h>
#include <errno.h>
#include <cutils/log.h>

#ifdef USE_DRM
#define HDMI_EDID_NODE      "/sys/class/drm/card0-HDMI-A-1/edid"
#else
#define HDMI_EDID_NODE      "/sys/class/display/HDMI/edid"
#endif

//#define DEBUG
#ifdef DEBUG
#define ALOGVV ALOGD
#else
#define ALOGVV
#endif

#define HDMI_EDID_BLOCK_SIZE    128
#define HDMI_MAX_EDID_BLOCK     8
#define HDMI_AUDIO_LPCM         1

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct HDMI_Audio_Sample {
    int index;
    int sample;
};

const struct HDMI_Audio_Sample HDMI_SAMPLE_TABLE[] =
{
    {HDMI_AUDIO_FS_32000,32000},
    {HDMI_AUDIO_FS_44100,44100},
    {HDMI_AUDIO_FS_48000,48000},
    {HDMI_AUDIO_FS_88200,88200},
    {HDMI_AUDIO_FS_96000,96000},
    {HDMI_AUDIO_FS_176400,176400},
    {HDMI_AUDIO_FS_192000,192000},
};

struct HDMI_Audio_Speaker_Allocation {
    int index;
    int location;
    char* name;
};

/*
 * the allocation is defined in CEA-861
 */
const struct HDMI_Audio_Speaker_Allocation HDMI_SPEAKER_ALLOCATION_TABLE[] =
{
    {(1<<0), AUDIO_CHANNEL_OUT_STEREO ,"FL/FR"},
    {(1<<1), AUDIO_CHANNEL_OUT_LOW_FREQUENCY ,"LFE"},
    {(1<<2), AUDIO_CHANNEL_OUT_FRONT_CENTER ,"FC"},
    {(1<<3), (AUDIO_CHANNEL_OUT_SIDE_LEFT|AUDIO_CHANNEL_OUT_SIDE_RIGHT), "SL/SR"},
    {(1<<4), AUDIO_CHANNEL_OUT_BACK_CENTER ,"RC"},
    {(1<<5), (AUDIO_CHANNEL_OUT_FRONT_LEFT_OF_CENTER|AUDIO_CHANNEL_OUT_FRONT_RIGHT_OF_CENTER), "FLC/FRC"},
    {(1<<6), (AUDIO_CHANNEL_OUT_BACK_LEFT|AUDIO_CHANNEL_OUT_BACK_RIGHT), "RLC/RRC"},
    {(1<<7), AUDIO_CHANNEL_NONE, "FLW/FRW"}, // no FLW/FRW defined in AUDIO_CHANNEL_OUT_xx
    {(1<<8), (AUDIO_CHANNEL_OUT_TOP_FRONT_LEFT|AUDIO_CHANNEL_OUT_TOP_FRONT_RIGHT), "FLH/FRH"},
    {(1<<9), AUDIO_CHANNEL_OUT_TOP_CENTER, "TC"},
    {(1<<10), AUDIO_CHANNEL_OUT_TOP_FRONT_CENTER, "FCH"},
};

int hdmi_edid_checksum(unsigned char *buf)
{
    int i;
    int checksum = 0;
    if(buf == NULL) {
        return -1;
    }

    for(i = 0; i < HDMI_EDID_BLOCK_SIZE; i++)
        checksum += buf[i];

    checksum &= 0xff;
    ALOGVV("%s checksum is %x\n", __FUNCTION__, checksum);
    int result = 0;
    if(checksum == 0) {
        result = 0;
    } else {
        result = -1;
    }

    return result;
}

bool translate_sample(unsigned char index,unsigned int* sample,int size)
{
    bool result = false;
    int i = 0, j = 0;
    int sample_array_length = ARRAY_SIZE(HDMI_SAMPLE_TABLE);
    for(i = 0; i < sample_array_length; i++) {
        // every bit mean a samplerate
        if(index & HDMI_SAMPLE_TABLE[i].index) {
            sample[j] = HDMI_SAMPLE_TABLE[i].sample;
            if(j < size){
                j++;
            }

            result = true;
        }
    }

    return result;
}


int hdmi_edid_parse_cea_audio(unsigned char *buf, struct hdmi_audio_infors *infor)
{
    int i, count;
    struct hdmi_audio_information* audio = NULL;
    if((buf == NULL) || (infor == NULL)) {
        ALOGD("%s: error, input parameter is NULL",__FUNCTION__);
        return -1;
    }

    count = buf[0] & 0x1F;
    if(count > 0) {
        audio = malloc(count*sizeof(struct hdmi_audio_information));
        if (audio == NULL) {
            ALOGD("%s: malloc hdmi_audio_information fail",__FUNCTION__);
            return -1;
        }

        memset(audio,0,count*sizeof(struct hdmi_audio_information));
        int audio_num = count / 3;
        unsigned char sample = 0;
        for (i = 0; i < count; i++) {
            audio[i].type = (buf[i * 3 + 1] >> 3) & 0x0F;
            audio[i].channel = (buf[i * 3 + 1] & 0x07) + 1;
            audio[i].sample = buf[i * 3 + 2];
            audio[i].value = buf[i * 3 + 3];
            //   translate_sample(sample,audio[i].sample,8);

            if (audio[i].type == HDMI_AUDIO_LPCM) {
                audio[i].word_length = buf[i * 3 + 3];
            } else if(audio[i].type >= HDMI_AUDIO_AC3 && audio[i].type <= HDMI_AUDIO_ATARC) {
                audio[i].max_bitrate = 8000*buf[i * 3 + 3];
            }

            ALOGVV("%s: i = %d, type = %d,channel = %d, sample = %d,value = 0x%x",
                __FUNCTION__,i,audio[i].type,audio[i].channel,audio[i].sample,audio[i].value);
        }

        infor->number = count;
        infor->audio = audio;
    }

    return 0;
}

/*
 * see document <cea-861-e.pdf> Part: Speaker Allocation Data Block(Page 67)
 * multi LPCM
 */
int hdmi_edid_parse_speaker_allocation(unsigned char *buf, struct hdmi_audio_infors *infor)
{
    if((buf == NULL) || (infor == NULL)) {
        ALOGD("%s: error, input parameter is NULL",__FUNCTION__);
        return -1;
    }

    /*
     * bytes in buf[1] ~ buf[3] is valid, see CEA-861 part:Speaker Allocation Data Block
     */
  //  int tag = (buf[0]>>5);
  //  int length = buf[0] & 0x1f;
    int hightlayout = (buf[2] & 0x07); // hight 5 bit is reseved
    infor->channel_layout = ((hightlayout<<8) | buf[1]);
    ALOGVV("%s : %d,buf[1]= 0x%x,buf[2]= 0x%x, layout = 0x%x",__FUNCTION__,__LINE__,buf[1],buf[2],infor->channel_layout);
    return 0;
}

int hdmi_edid_parse_extensions_cea(unsigned char *buf, struct hdmi_audio_infors *infor)
{
    unsigned int ddc_offset, native_dtd_num, cur_offset = 4;
    unsigned int tag, IEEEOUI = 0, count, i;
    struct fb_videomode *vmode;

    if((buf == NULL) || (infor == NULL)) {
        ALOGD("%s: error, input parameter is NULL",__FUNCTION__);
        return -1;
    }

    /* Check ces extension version */
    if (buf[1] != 3) {
        ALOGD("%s: %d [CEA] error version",__FUNCTION__,__LINE__);
        return -1;
    }

    ddc_offset = buf[2];
        /* Parse data block */
    while (cur_offset < ddc_offset) {
        tag = buf[cur_offset] >> 5;
        count = buf[cur_offset] & 0x1F;
        switch (tag) {
            case 0x02:  /* Video Data Block */
                ALOGVV("%s: [CEA] Video Data Block",__FUNCTION__);
                break;
            case 0x01:  /* Audio Data Block */
                ALOGVV("%s [CEA] Audio Data Block",__FUNCTION__);
                hdmi_edid_parse_cea_audio(buf + cur_offset,infor);
                break;
            case 0x04:  /* Speaker Allocation Data Block */
                ALOGVV("%s: [CEA] Speaker Allocatio Data Block",__FUNCTION__);
                hdmi_edid_parse_speaker_allocation(buf + cur_offset,infor);
                break;
            case 0x03:  /* Vendor Specific Data Block */
                ALOGVV("%s: [CEA] Vendor Specific Data Block",__FUNCTION__);
                break;
            case 0x05:  /* VESA DTC Data Block */
                ALOGVV("%s: [CEA] VESA DTC Data Block",__FUNCTION__);
                break;
            case 0x07:  /* Use Extended Tag */
                ALOGVV("%s: [CEA] Use Extended Tag Data Block %02x",__FUNCTION__,buf[cur_offset + 1]);
                switch (buf[cur_offset + 1]) {
                case 0x00:
                    ALOGVV("%s: [CEA] Video Capability Data Block",__FUNCTION__);
                    break;
                case 0x05:
                    ALOGVV("%s: [CEA] Colorimetry Data Block",__FUNCTION__);
                    break;
                case 0x06:
                    ALOGVV("%s: [CEA] HDR Static Metedata data Block",__FUNCTION__);
                    break;
                case 0x0e:
                    ALOGVV("%s: [CEA] YCBCR 4:2:0 Video Data Block",__FUNCTION__);
                    break;
                case 0x0f:
                    ALOGVV("%s: [CEA] YCBCR 4:2:0 Capability Map Data",__FUNCTION__);
                    break;
            }
            break;
            default:
                ALOGVV("%s: [CEA] unkowned data block tag",__FUNCTION__);
                break;
        }
        cur_offset += (buf[cur_offset] & 0x1F) + 1;
    }

    return 0;
}


int hdmi_edid_parse_extensions(unsigned char *buf, struct hdmi_audio_infors *infor)
{
    int rc;

    if((buf == NULL) || (infor == NULL)) {
        ALOGD("%s: error, input parameter is NULL",__FUNCTION__);
        return -1;
    }

    /* Checksum */
    rc = hdmi_edid_checksum(buf);
    if (rc != 0) {
        ALOGE("%s: [EDID] extensions block checksum error",__FUNCTION__);
        return -1;
    }
    switch (buf[0]) {
        case 0xF0:
            ALOGVV("%s: [EDID-EXTEND] Iextensions block map",__FUNCTION__);
            break;
        case 0x02:
            ALOGVV("%s: [EDID-EXTEND] CEA 861 Series Extension",__FUNCTION__);
            hdmi_edid_parse_extensions_cea(buf, infor);
            break;
        case 0x10:
            ALOGVV("%s: [EDID-EXTEND] Video Timing Block Extension",__FUNCTION__);
            break;
        case 0x40:
            ALOGVV("%s: [EDID-EXTEND] Display Information Extension",__FUNCTION__);
            break;
        case 0x50:
            ALOGVV("%s: [EDID-EXTEND] Localized String Extension",__FUNCTION__);
            break;
        case 0x60:
            ALOGVV("%s: [EDID-EXTEND] Digital Packet Video Link Extension",__FUNCTION__);
            break;
        default:
            ALOGVV("%s: [EDID-EXTEND] Unkowned Extension,tag = 0x%x",__FUNCTION__,buf[0]);
            return -1;
    }

    return 0;
}

int hdmi_parse_base_block(unsigned char *buf, int *extend_num)
{
    if(buf == NULL || extend_num == NULL)
        return -1;

    *extend_num = (int)buf[0x7e];
    // skip other informations
    return 0;
}

void init_hdmi_audio(struct hdmi_audio_infors *infor)
{
    if(infor != NULL) {
        pthread_mutex_init(&infor->lock, NULL);
        infor->number = 0;
        infor->channel_layout = -1;
        infor->audio = NULL;
    }
}

void destory_hdmi_audio(struct hdmi_audio_infors *infor)
{
    if(infor != NULL) {
        pthread_mutex_lock(&infor->lock);
        if(infor->audio != NULL) {
            free(infor->audio);
            infor->audio = NULL;
        }

        infor->number = 0;
        infor->channel_layout = -1;
        pthread_mutex_unlock(&infor->lock);
        pthread_mutex_destroy(&infor->lock);
    }
}

/*
 * get the speaker allocation
 * only valid when output stream is pcm
 */
int get_hdmi_audio_speaker_allocation(struct hdmi_audio_infors *infor)
{
    if((infor == NULL) || (infor->channel_layout == -1)) {
        return AUDIO_CHANNEL_OUT_STEREO;
    }

    pthread_mutex_lock(&infor->lock);
    int layout = AUDIO_CHANNEL_NONE;
    int length = ARRAY_SIZE(HDMI_SPEAKER_ALLOCATION_TABLE);
    for(int i = 0; i < length; i++) {
        if(infor->channel_layout & HDMI_SPEAKER_ALLOCATION_TABLE[i].index) {
            layout |= HDMI_SPEAKER_ALLOCATION_TABLE[i].location;
        }
    }
    pthread_mutex_unlock(&infor->lock);
    return layout;
}

int parse_hdmi_audio(struct hdmi_audio_infors *infor)
{
    if(infor == NULL) {
        ALOGD("%s: error, input parameter is NULL",__FUNCTION__);
        return -1;
    }

    FILE* file = fopen(HDMI_EDID_NODE,"rb");
    if(file == NULL) {
        ALOGVV("%s: open %s fail,reason = %s",__FUNCTION__,HDMI_EDID_NODE,strerror(errno));
        return -1;
    }
    pthread_mutex_lock(&infor->lock);
    // there is 128 bytes in every block
    unsigned char buffer[HDMI_EDID_BLOCK_SIZE];
    memset(buffer, 0, HDMI_EDID_BLOCK_SIZE);
    int size = 0;
    int retry = 20;
    do{
        /*
         * using hdmi drm function to get this information is a better way,
         * but using this functions need System permissions.
         * May be this is no information in Node HDMI_EDID_NODE,
         * so if read fail ,we retry to read.
         */
        size = fread(buffer, 1, HDMI_EDID_BLOCK_SIZE, file);
        if(size == 0){
            usleep(20000);
            retry --;
        }
    }while((size==0) && (retry>0));
    ALOGVV("%s: %d: size = %d",__FUNCTION__,__LINE__,size);
    int extendblock = 0;
    int i = 0;

    // parse base block, we just need the information of the numbers of extend block
    hdmi_parse_base_block(buffer, &extendblock);
    ALOGVV("%s: %d: extendblock = %d",__FUNCTION__,__LINE__,extendblock);
    // paser extend block
    if(extendblock > 0) {
        for (i = 1; (i < extendblock + 1) && (i < HDMI_MAX_EDID_BLOCK); i++) {
            memset(buffer, 0, HDMI_EDID_BLOCK_SIZE);
            size = fread(buffer, 1, HDMI_EDID_BLOCK_SIZE, file);
            if(size > 0) {
                hdmi_edid_parse_extensions(buffer,infor);
            } else {
                break;
            }
        }
    }
    pthread_mutex_unlock(&infor->lock);

    dump(infor);
EXIT:
    if(file != NULL) {
        fclose(file);
        file = NULL;
    }

    return 0;
}

int translate_format(audio_format_t format)
{
    int type = (int)HDMI_AUDIO_FORMAT_INVALID;
    switch(format)
    {
        case AUDIO_FORMAT_AC3:
            type = (int)HDMI_AUDIO_AC3;
            break;
        case AUDIO_FORMAT_E_AC3:
            type = (int)HDMI_AUDIO_E_AC3;
            break;
        case AUDIO_FORMAT_DTS:
            type = (int)HDMI_AUDIO_DTS;
            break;
        case AUDIO_FORMAT_DTS_HD:
            type = (int)HDMI_AUDIO_DTS_HD;
            break;
        case AUDIO_FORMAT_AAC_LC:
            type = (int)HDMI_AUDIO_AAC_LC;
            break;
        case AUDIO_FORMAT_DOLBY_TRUEHD:
            type = (int)HDMI_AUDIO_MLP;
            break;
        case AUDIO_FORMAT_AC4:
            type = (int)HDMI_AUDIO_E_AC3;
            break;
        default:
            break;
    }

    return type;
}

bool is_support_ac4(int type,int support)
{
    /* Bits of byte 3
     * bit0 = 1 Decoding of joint object coding content is supported
     * bit1 = 1 Decoding of joint object coding content with ACMOD 28 is supported
     */
    if((type == (int)HDMI_AUDIO_E_AC3) && (support & 0x1)) {
        return true;
    }

    return false;
}
bool is_support_format(struct hdmi_audio_infors *infor,audio_format_t format)
{
    if((infor == NULL) || (infor->number <= 0) || (infor->audio == NULL)) {
        return false;
    }

    pthread_mutex_lock(&infor->lock);
    bool support = false;
    int type = translate_format(format);
    for(int i = 0; i < infor->number; i++) {
        // AC4 have the same value with EAC3, must judge the byte3 if cea_audio
        if(format == AUDIO_FORMAT_AC4){
            support = is_support_ac4((int)infor->audio[i].type,(int)infor->audio[i].value);
            if(support){
                break;
            }
        }else if(type == (int)infor->audio[i].type) {
            support = true;
            break;
        }
    }
    pthread_mutex_unlock(&infor->lock);
    return support;
}

void dump_hdmi_audio_sample(int index,char*name,int size)
{
    int i = 0;
    bool first = true;
    int offset = 0;

    if((name == NULL) || (size < 0)){
        return ;
    }

    int sample_array_length = ARRAY_SIZE(HDMI_SAMPLE_TABLE);
    for(i = 0; i < sample_array_length; i++) {
        // every bit mean a samplerate
        if(index & HDMI_SAMPLE_TABLE[i].index) {
            if(name != NULL) {
                int length = snprintf(name + offset, size-offset, "%s%d",
                                               first ? "" : ",",
                                               HDMI_SAMPLE_TABLE[i].sample);
                first = false;
                offset += length;
            }
        }
    }
}

void dump_hdmi_audio_format(int format,int support,char* buffer,int size)
{
    if((buffer == NULL) || (size <= 0)){
        return ;
    }

    char* name = NULL;
    switch(format)
    {
        case HDMI_AUDIO_LPCM:
            name = "Pcm";
            break;
        case HDMI_AUDIO_AC3:
            name = "AC3";
            break;
        case HDMI_AUDIO_MPEG1:
            name = "MPEG1";
            break;
        case HDMI_AUDIO_MP3:
            name = "MP3";
            break;
        case HDMI_AUDIO_MPEG2:
            name = "MP2";
            break;
        case HDMI_AUDIO_AAC_LC:
            name = "AAC_LC";
            break;
        case HDMI_AUDIO_DTS:
            name = "DTS";
            break;
        case HDMI_AUDIO_ATARC:
            name = "ATARC";
            break;
        case HDMI_AUDIO_DSD:
            name = "DSD";
            break;
        case HDMI_AUDIO_E_AC3:
            name = "EAC3";
            if(is_support_ac4(format,support)){
                name = "EAC3/AC4";
            }
            break;
        case HDMI_AUDIO_DTS_HD:
            name = "DTS-HD";
            break;
        case HDMI_AUDIO_MLP:
            name = "MLP";
            break;
        case HDMI_AUDIO_DST:
            name = "DST";
            break;
        case HDMI_AUDIO_WMA_PRO:
            name = "WMA-PRO";
            break;
        default:
            name = "Unknow";
            break;
    }

    if(name != NULL){
        snprintf(buffer, size, "%s", name);
    }
}

void dump_hdmi_audio_speaker_layout(int layout)
{
    int i = 0;
    bool first = true;
    int offset = 0;
    char buffer[512];
    int size = 512;

    memset(buffer,0,size);
    int length = ARRAY_SIZE(HDMI_SPEAKER_ALLOCATION_TABLE);
    for(i = 0; i < length; i++) {
        // every bit mean a samplerate
        if(layout & HDMI_SPEAKER_ALLOCATION_TABLE[i].index) {
            int length = snprintf(buffer + offset, size-offset, "%s%s",
                                           first ? "" : ",",
                                           HDMI_SPEAKER_ALLOCATION_TABLE[i].name);
            first = false;
            if(offset+length < size){
                offset += length;
            } else {
                ALOGVV("%s: buffer is not enought",__FUNCTION__);
                break;
            }
        }
    }

    ALOGVV("%s: speaker allocation = %s",__FUNCTION__,buffer);
}

void dump(struct hdmi_audio_infors *infor)
{
    if((infor == NULL) || (infor->number <= 0) || (infor->audio == NULL)) {
        return ;
    }

    char format[20];
    char channel[50];
    char sample[1024];
    char* name = NULL;

    pthread_mutex_lock(&infor->lock);
    for(int i = 0; i < infor->number; i++) {
        if(infor->audio[i].type == 0){
            continue;
        }

        dump_hdmi_audio_format(infor->audio[i].type,infor->audio[i].value,format,20);
        dump_hdmi_audio_sample(infor->audio[i].sample,sample,1024);
        ALOGVV("%s: type = %s,channel = %d,sample = %s",__FUNCTION__,format,infor->audio[i].channel,sample);
    }

    if(infor->channel_layout != -1){
        dump_hdmi_audio_speaker_layout(infor->channel_layout);
    }
    pthread_mutex_unlock(&infor->lock);
}
