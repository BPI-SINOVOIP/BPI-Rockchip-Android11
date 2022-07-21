/*
 * Copyright 2020 Rockchip Electronics Co. LTD
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
 *
 * Author: hh@rock-chips.com
 * Date: 2021/11/27
 */
#include <utils/Log.h>
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include "audio_iec958.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "iec958"

/* see IEC 60958-3-2006.pdf
 *
 * Byte 2: Source and channel number
 * Bits 16~19  Source number, bit 16 = LSB, bit 19 = MSB
 * Bit     16, 17, 18, 19
 * State   0   0   0   0     Do not take into account
 *         1   0   0   0     1
 *         0   1   0   0     2
 *         1   1   0   0     3
 *         ......
 *         1   1   1   1     15
 *
 * Bits 20 to 23  Channel number(audio channel), bit 20 = LSB, bit 23 = MSB
 * Bit     20  21  22  23
 * State   0   0   0   0   Do not take into account
 *         1   0   0   0   (left channel for stereo channel format)
 *         0   1   0   0   (right channel for stereo channel format)
 *         1   1   0   0
 *         ......
 *         1   1   1   1
 *
 *
 * Byte 3 Sampling frequency and clock accuracy
 * Bit 24~27 for samplerate
 * Bit      24, 25, 26, 27
 * State:   0   0   1   0    22050 hz
 *          0   0   0   0    44100 hz
 *          0   0   0   1    88200 hz
 *          0   0   1   1    176400 hz
 *          0   1   1   0    24000 hz
 *          0   1   0   0    48000 hz
 *          0   1   0   1    96000 hz
 *          0   1   1   1    192000 hz
 *          1   1   0   0    32000 hz
 *          1   0   0   0    samplerate not indicated
 *          1   0   0   1    768000 hz
 *
 *
 * Bit 32 Word length
 * Bit      0  Maximum audio sample word length is 20bis
 *          1  Maximum audio sample word length is 24bis
 *
 *
 * Bit 33~35 for sample word length
 * Bit       33, 34, 35   Audio sample word length if
 *                        maxumum lenth is 24bits(see bit32=1)
 *                        indicated by bit 32
 * State:    0   0   0    Word length not indicated dicated
 *                        (default)
 *           1   0   0      20bits
 *           0   1   0      22bits
 *           0   0   1      23bits
 *           1   0   1      24bits
 *           0   1   1      21bits
 *
 * Bit       33, 34, 35   Audio sample word length if
 *                        maxumum lenth is 20bits(see bit32=0)
 *                        indicated by bit 32
 * State:    0   0   0    Word length not indicated dicated
 *                        (default)
 *           1   0   0      16bits
 *           0   1   0      18bits
 *           0   0   1      19bits
 *           1   0   1      20bits
 *           0   1   1      17bits
 *
 *
 * Bit 36~39 for Original sampling frequency
 * Bit       36 37 38 39
 * State:    1  1  1  1     44100 hz
 *           1  1  1  0     88200 hz
 *           1  1  0  1     22050 hz
 *           1  1  0  0     176400 hz
 *           1  0  1  1     48000 hz
 *           1  0  1  0     96000 hz
 *           1  0  0  1     24000 hz
 *           1  0  0  0     192000 hz
 *           0  1  1  1     reserved
 *           0  1  1  0     8000 hz
 *           0  1  0  1     11025 hz
 *           0  1  0  0     12000 hz
 *           0  0  1  1     32000 hz
 *           0  0  1  0     reserved
 *           0  0  0  1     16000 hz
 *           0  0  0  0     Original sampling frequency not indicated(default)
 */

unsigned int iec958_16to32(short *buffer) {
    unsigned int data = (*buffer) << 16;
    return data;
}

static unsigned int iec958_parity(unsigned int data) {
    unsigned int parity;
    int bit;

    /*
    * Compose 32bit IEC958 subframe, two sub frames
    * build one frame with two channels.
    *
    * This is IP of HDMI data map:
    * Audio Width  31 30 29 28 27 26 ......  12  11  10  9  8  7  6  5  4   3   2   1   0
    *     24       B  P  C  U  V  MSB                                       LSB
    *     20       B  P  C  U  V  MSB                          LSB
    *     16       B  P  C  U  V  MSB            LSB
    *
    * So, for 16bit(IEC61937) to IEC958 subframe,
    * bit 0-10  = padding
    *     11-26 = data
    *     27    = validity (0 for valid data, else 'in error')
    *     28    = user data (0)
    *     29    = channel status (24 bytes for 192 frames)
    *     30    = parity
    *     31    = block start
    */
    parity = 0;
    data >>= 11;
    for (bit = 11; bit <= 29; bit++) {
        if (data & 1)
            parity++;
        data >>= 1;
    }
    return (parity & 1);
}

static uint32_t iec958_subframe(rk_iec958 *iec, uint32_t data, int channel) {
    unsigned int byte = iec->counter >> 3;
    unsigned int mask = 1 << (iec->counter - (byte << 3));

    data = (data & 0xffff0000) >> 5;

    /* set IEC status bits (up to 192 bits) */
    if (iec->status[byte] & mask)
        data |= 0x20000000;

    if (iec958_parity(data))
        data |= 0x40000000;

    /* block start */
    if (!iec->counter)
        data |= 0x80000000;

    return data;
}

int iec958_frame_encode(rk_iec958 *iec, char *in, int inLength, char *out, int *outLength) {
    if (iec == NULL)
        return -1;

    if (in == NULL || inLength <= 0 || out == NULL || outLength == NULL)
        return -1;

    int size = 2;
    int counter  = iec->counter;
    int channels = iec->channels;
    int frames   = inLength/(size*channels);
    int frame1   = frames;

    short *input     = NULL;
    uint32_t *output = NULL;
    uint32_t data    = 0;
    for (int channel = 0; channel < channels; ++channel) {
        iec->counter = counter;
        frame1 = frames;
        input  = (short *)in+channel;
        output = (uint32_t *)out+channel;
        while (frame1 > 0) {
            data = iec958_16to32(input);
            *output = iec958_subframe(iec, data, channel);
            input += channels;
            output += channels;
            iec->counter++;
            iec->counter %= 192;
            frame1--;
        }
    }
    *outLength = 2*inLength;

    return 0;
}

void setResample(unsigned char *status, int sameplerate) {
    // set AES3(Byte3) bit24~27
    switch(sameplerate) {
        case 22050:
            *status = IEC958_AES3_CON_FS_22050;
            break;
        case 24000:
            *status = IEC958_AES3_CON_FS_24000;
            break;
        case 32000:
            *status = IEC958_AES3_CON_FS_32000;
            break;
        case 44100:
            *status = IEC958_AES3_CON_FS_44100;
            break;
        case 48000:
            *status = IEC958_AES3_CON_FS_48000;
            break;
        case 88200:
            *status = IEC958_AES3_CON_FS_88200;
            break;
        case 96000:
            *status = IEC958_AES3_CON_FS_96000;
            break;
        case 176400:
            *status = IEC958_AES3_CON_FS_176400;
            break;
        case 192000:
            *status = IEC958_AES3_CON_FS_192000;
            break;
        case 768000:
            *status = IEC958_AES3_CON_FS_768000;
            break;
        default:
            ALOGD("samplerate = %d not support", sameplerate);
            break;
    }
}

void setOriginalResample(unsigned char *status, int sameplerate) {
    // set AES4(Byte4) bit36~39
    switch(sameplerate) {
        case 22050:
            *status |= IEC958_AES4_CON_FS_22050;
            break;
        case 24000:
            *status |= IEC958_AES4_CON_FS_24000;
            break;
        case 32000:
            *status |= IEC958_AES4_CON_FS_32000;
            break;
        case 44100:
            *status |= IEC958_AES4_CON_FS_44100;
            break;
        case 48000:
            *status |= IEC958_AES4_CON_FS_48000;
            break;
        case 88200:
            *status |= IEC958_AES4_CON_FS_88200;
            break;
        case 96000:
            *status |= IEC958_AES4_CON_FS_96000;
            break;
        case 176400:
            *status |= IEC958_AES4_CON_FS_176400;
            break;
        case 192000:
            *status |= IEC958_AES4_CON_FS_192000;
            break;
        default:
            break;
    }
}


int iec958_init(rk_iec958 *iec, int samplerate, int channel, bool isPcm) {
    const unsigned char pcm_status_bits[] = {
        IEC958_AES0_CON_EMPHASIS_NONE,   // Byte0 consumer, not-copyright, emphasis-none, mode=0
        IEC958_AES1_CON_ORIGINAL | IEC958_AES1_CON_PCM_CODER,  // Byte1 original, PCM coder
        0,  // Byte2 source and channel
        IEC958_AES3_CON_FS_48000,  // Byte3 fs=48000Hz, clock accuracy=1000ppm
    };

    const unsigned char bistream_status_bits[] = {
        IEC958_AES0_NONAUDIO, // non pcm
        0,  // bit8~bit15
        0,  // Byte2 source and channel bit16~bit23
        IEC958_AES3_CON_FS_48000,  // bit24~bit31
        IEC958_AES4_CON_BITS24,
    };

    if (iec == NULL)
        return -1;

    iec->counter = 0;
    memset(iec->status, 0, sizeof(iec->status));
    if (isPcm) {
        memcpy(iec->status, pcm_status_bits, sizeof(pcm_status_bits));
    } else {
        memcpy(iec->status, bistream_status_bits, sizeof(bistream_status_bits));
        // HBR for example TRUEHD, DTS-HD using 768000 samplerate
        if (channel == 8) {
            samplerate = 768000;
        }
    }

    // always using channel = 2 to convert IEC61937/PCM frame to IEC958 frame
    channel = 2;
    setResample(&iec->status[3], samplerate);
    setOriginalResample(&iec->status[4], samplerate);

    iec->samplerate = samplerate;
    iec->channels   = channel;
    ALOGV("this = %p, status: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
        iec, iec->status[0], iec->status[1], iec->status[2], iec->status[3], iec->status[4]);
    ALOGV("this = %p, samplerate = %d, channel = %d", iec, samplerate, channel);

    return 0;
}

int iec958_deInit(rk_iec958 *iec) {
    (void)iec;
    return 0;
}

