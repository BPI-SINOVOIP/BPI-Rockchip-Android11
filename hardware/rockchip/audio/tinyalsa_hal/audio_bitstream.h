#ifndef _RK_AUDIO_HDMI_BITSTREAM_
#define _RK_AUDIO_HDMI_BITSTREAM_

#include <stdbool.h>

#define CHASTA_NUM     192
#define CHASTA_SUB_NUM (CHASTA_NUM*2)

/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co., Ltd
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

extern bool isValidSamplerate(int samplerate);
extern void initchnsta(char* buffer);
extern void setChanSta(char* buffer,int samplerate, int channel);
extern void fill_hdmi_bitstream_buf(void * in, void* out,void* chan, int length);
#endif