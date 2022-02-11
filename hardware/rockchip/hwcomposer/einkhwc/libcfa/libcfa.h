/*
 * Copyright (C) 2017 - 2019 E Ink Holdings Inc. 
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

#ifndef LIBCFA_H
#define LIBCFA_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <inttypes.h>
namespace android {
/**
 *
 * Description : for short stride
 *  WIDTH : image width 
 *  HEIGHT : image height 
 *	color_buffer : input image RGB8888 raw data
 *  gray_buffer : output data
 *
 */
 //7�����WIDTH = 1680, HEIGHT = 1264, color_buffer��С��(WIDTH*HEIGHT*4) bytes, gray_buffer��С��(WIDTH*HEIGHT) bytes.
void image_to_cfa_grayscale_gen2_ARGBB8888(int WIDTH, int HEIGHT,unsigned char *color_buffer, unsigned char *gray_buffer);
}
#if defined(__cplusplus)
}
#endif

#endif /* LIBCFA_H */
