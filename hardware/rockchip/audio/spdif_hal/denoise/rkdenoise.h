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
#ifndef RK_DENOISE_H
#define RK_DENOISE_H
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <system/audio.h>

#define ALG_AUTO (1<<31)
#define ALG_SPX (1<<0)
#define ALG_SKV (1<<1)

typedef void *hrkdeniose;
hrkdeniose rkdenoise_create(int rate, int ch, int period, uint32_t flag);
int rkdenoise_process(hrkdeniose context, void *bufferin, int bytes, void *bufferout);
void rkdenoise_destroy(hrkdeniose context);

#endif
