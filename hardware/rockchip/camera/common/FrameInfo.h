/*
 * Copyright (C) 2015-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef _FRAMEINFO_H_
#define _FRAMEINFO_H_

/* ********************************************************************
 * Common HAL data structures and methods
 */

/**
 *  \struct FrameInfo
 *  Structure to pass resolution, size and and stride info between objects.
 */
NAMESPACE_DECLARATION {

struct FrameInfo {
    FrameInfo(): format(0), width(0), height(0), stride(0),
    size(0), field(0), maxWidth(0), maxHeight(0), bufsNum(0) {}

    int format;
    int width;
    int height;

    int stride;     // stride in pixels(can be bigger than width)
    int size;
    int field;

    int maxWidth;
    int maxHeight;

    int bufsNum;
};
} NAMESPACE_DECLARATION_END

#endif // _FRAMEINFO_H_