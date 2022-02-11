/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 * author: kevin.chen@rock-chips.com
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>

#include "mpp_err.h"
#include "mpp_frame.h"
#include "mpp_packet.h"

#define ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))

class CommonUtil {
public:
    /* global dump and store methods */
    static void dumpMppFrameToFile(MppFrame frame, FILE *file);
    static void dumpMppPacketToFile(MppPacket packet, FILE *file);
    static void dumpDataToFile(char *data, size_t size, FILE *file);
    static void dumpDataToFile(char *data, size_t size, const char *fileName);
    static void dumpDmaFdToFile(int fd, size_t size, FILE *file);
    // allocate buffer memory inside, don't forget to free it.
    static MPP_RET storeFileData(const char *file_name, char **data, size_t *length);

    /* yuv image related operations */
    static MPP_RET cropImage(char *src, char *dst,
                             int src_width, int src_height,
                             int src_wstride, int src_hstride,
                             int dst_width, int dst_height);
    static MPP_RET readImage(char *src, char *dst,
                             int width, int height,
                             int wstride, int hstride,
                             MppFrameFormat fmt);

    /*  set\get __system_property_ */
    static int envGetU32(const char *name, uint32_t *value, uint32_t default_value);
    static int envGetStr(const char *name, const char **value, const char *default_value);
    static int envSetU32(const char *name, uint32_t value);
    static int envSetStr(const char *name, char *value);

    /* other util methods */
    static bool isValidDmaFd(int fd);
    static void setPerformanceMode(int on);
};

#endif //__UTILS_H__
