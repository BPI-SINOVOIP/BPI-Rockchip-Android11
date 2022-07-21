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
#ifndef AUIDO_HW_BITSTREAM_INTERFACE_
#define AUIDO_HW_BITSTREAM_INTERFACE_

#include "audio_iec958.h"
#include "audio_bitstream.h"
#include "../asoundlib.h"

typedef struct _rk_bistream {
    char *buffer;             // output buffer
    int  capaticy;            // output buffer size
    enum pcm_format format;   // output format
    char *chnStatus;          // for kernel 4.19 and before version
    rk_iec958 iec958;         // for kernel 5.10 and later version
} rk_bistream;

rk_bistream* bitstream_init(enum pcm_format format, int samplerate, int channel);
int bitstream_encode(rk_bistream *bs, char *inBuffer, int inSize,
    char **outBuffer, int *outSize);
void bitstream_destory(rk_bistream **bs);

#endif  // AUIDO_HW_BITSTREAM_INTERFACE_
