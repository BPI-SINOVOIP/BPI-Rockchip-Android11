#ifndef ANDROID_AUDIO_USBAUDIO_HARDWARE_H
#define ANDROID_AUDIO_USBAUDIO_HARDWARE_H

#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <utils/Log.h>
#include <utils/String8.h>

#define UA_Path "/proc/asound/card2/stream0"
#define UA_Record_SampleRate 48000
#define UA_Playback_SampleRate 48000
#define RETRY_TIMES 10
#define RETRY_SLEEPTIME 300*1000
#define UA_Record_type "Capture"
#define UA_Playback_type "Playback"
#define UA_Format   "Format"
#define UA_Channels "Channels"
#define UA_SampleRates "Rates"

bool has_USBAudio_Speaker_MIC(const char *type)
{
    int fd;
    char buf[2048] = {0};
    char *str,tmp[6] = {0};

    for(int i = 0; i < RETRY_TIMES; i++) {
        fd = open(UA_Path,O_RDONLY);
        if(fd < 0) {
            ALOGV("Can not open /proc/asound/card2/stream0, try time = %d", i + 1);
            usleep(RETRY_SLEEPTIME);
            continue;
        }
        break;
    }

    if (fd < 0) {
        ALOGE("Can't open /proc/asound/card2/stream0, giveup");
        return false;
    }

    read(fd, buf, sizeof(buf));
    str = strstr(buf, type);
    close(fd);

    if (str != NULL) {
        return true;
    } else {
        return false;
    }
}

uint32_t get_USBAudio_sampleRate(const char *type, uint32_t req_rate)
{//support like this: Rates: 8000, 16000, 24000, 32000, 44100, 48000  or  Rates: 48000
    int fd;
    uint32_t sampleRate = 0, lastSampleRate = 0;
    char buf[2048]={0};
    char *str;
    ssize_t nbytes;

    ALOGD("get_USBAudio_sampleRate() %s : req_rate %d", type, req_rate);

    for(int i = 0; i < RETRY_TIMES; i++) {
        fd = open(UA_Path,O_RDONLY);
        if(fd < 0) {
            ALOGV("Can not open /proc/asound/card2/stream0, try time = %d", i + 1);
            usleep(RETRY_SLEEPTIME);
            continue;
        }
        break;
    }

    if (fd < 0) {
        ALOGE("Can't open /proc/asound/card2/stream0, giveup");
        return 0;
    }

    read(fd, buf, sizeof(buf) - 1);
    close(fd);

    str = strstr(buf, type);
    if(!str) return 0;

    str = strstr(str, UA_SampleRates);//point to the param line
    if(!str) return 0;

    str += sizeof(UA_SampleRates);

    nbytes = strlen(str);

    //ALOGD("get_USBAudio_sampleRate() nbytes = %d, str = %s", nbytes, str);

    while (nbytes > 0 && *str != '\n') {

        while (nbytes > 0 && (*str > '9' || *str < '0') && *str != '\n') {
            str++;
            nbytes--;
        }

        if (*str == '\n') break;

        sampleRate = atoi(str);

        if (sampleRate == 0) {
            sampleRate = lastSampleRate;
            break;
        }

        ALOGV("get_USBAudio_sampleRate() Get rate : %d", sampleRate);

        if (sampleRate >= req_rate)
            break;

        lastSampleRate = sampleRate;

        while (nbytes > 0 && *str <= '9' && *str >= '0') {
            str++;
            nbytes--;
        }
    }

    ALOGD("get_USBAudio_sampleRate() Get rate %d for %s", sampleRate, type);

    return sampleRate;
}

uint32_t get_USBAudio_Channels(const char *type, uint32_t req_channel)
{
    int fd;
    uint32_t channels = 0;
    char *str;
    char buf[2048]={0};

    ALOGV("get_USBAudio_Channels() %s : req_rate %d", type, req_channel);

    for (int i = 0; i < RETRY_TIMES; i++) {
        fd = open(UA_Path, O_RDONLY);
        if (fd < 0) {
            ALOGD("Can not open /proc/asound/card2/stream0,try time =%d", i + 1);
            usleep(RETRY_SLEEPTIME);
            continue;
        }
        break;
    }

    if (fd < 0) {
        ALOGE("Can't open /proc/asound/card2/stream0,giveup");
        return 0;
    }

    read(fd, buf, sizeof(buf)-1);
    close(fd);

    str = strstr(buf, type);
    if (!str) return 0;

    str = strstr(str, UA_Channels);//point to the param line
    str += sizeof(UA_Channels);
    channels = atoi(str);

    ALOGD("get_USBAudio_Channels() Get channels %d for %s",
        channels, type);

    return channels;
}

#endif
