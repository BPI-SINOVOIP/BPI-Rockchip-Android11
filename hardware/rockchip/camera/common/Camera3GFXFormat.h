/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#ifndef _CAMERA3_GFX_FORMAT_H_
#define _CAMERA3_GFX_FORMAT_H_

#include <system/window.h>
#include "Camera3V4l2Format.h"

NAMESPACE_DECLARATION {

/* ********************************************************************
 * Common GFX data structures and methods
 */

/**
 * Calculates the frame bytes-per-line following the limitations imposed by display subsystem
 * This is used to model the HACK in a atomsisp that forces allocation
 * to be aligned to the bpl that SGX, GEN or other gfx requires.
 *
 * \param format [in] V4L2 pixel format of the image
 * \param width [in] width in pixels
 *
 * \return bpl following the Display subsystem requirement
 **/
int widthToStride(int fourcc, int width);

int v4L2Fmt2GFXFmt(int v4l2Fmt);

} NAMESPACE_DECLARATION_END

#endif // _CAMERA3_GFX_FORMAT_H_
