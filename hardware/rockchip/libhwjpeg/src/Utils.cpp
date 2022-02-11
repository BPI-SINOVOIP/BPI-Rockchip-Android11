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

//#define LOG_NDEBUG 0
#define LOG_TAG "Utils"
#include <utils/Log.h>

#include <sys/system_properties.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <drmrga.h>
#include <RgaApi.h>
#include <fcntl.h>

#include "mpp_mem.h"
#include "Utils.h"

static int g_rga_init = 0;

void CommonUtil::dumpMppFrameToFile(MppFrame frame, FILE *file)
{
    int width    = 0;
    int height   = 0;
    int h_stride = 0;
    int v_stride = 0;
    MppFrameFormat fmt  = MPP_FMT_YUV420SP;
    MppBuffer buffer    = NULL;
    uint8_t *base = NULL;

    if (NULL == file || NULL == frame)
        return;

    width    = mpp_frame_get_width(frame);
    height   = mpp_frame_get_height(frame);
    h_stride = mpp_frame_get_hor_stride(frame);
    v_stride = mpp_frame_get_ver_stride(frame);
    fmt      = mpp_frame_get_fmt(frame);
    buffer   = mpp_frame_get_buffer(frame);

    if (NULL == buffer)
        return;

    base = (uint8_t *)mpp_buffer_get_ptr(buffer);

    switch (fmt) {
    case MPP_FMT_YUV422SP : {
        /* YUV422SP -> YUV422P for better display */
        int i, j;
        uint8_t *base_y = base;
        uint8_t *base_c = base + h_stride * v_stride;
        uint8_t *tmp = mpp_malloc(uint8_t, h_stride * height * 2);
        uint8_t *tmp_u = tmp;
        uint8_t *tmp_v = tmp + width * height / 2;

        for (i = 0; i < height; i++, base_y += h_stride)
            fwrite(base_y, 1, width, file);

        for (i = 0; i < height; i++, base_c += h_stride) {
            for (j = 0; j < width / 2; j++) {
                tmp_u[j] = base_c[2 * j + 0];
                tmp_v[j] = base_c[2 * j + 1];
            }
            tmp_u += width / 2;
            tmp_v += width / 2;
        }

        fwrite(tmp, 1, width * height, file);
        mpp_free(tmp);
    } break;
    case MPP_FMT_YUV420SP : {
        int i;
        uint8_t *base_y = base;
        uint8_t *base_c = base + h_stride * v_stride;

        for (i = 0; i < height; i++, base_y += h_stride) {
            fwrite(base_y, 1, width, file);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride) {
            fwrite(base_c, 1, width, file);
        }
    } break;
    case MPP_FMT_YUV420P : {
        int i;
        uint8_t *base_y = base;
        uint8_t *base_c = base + h_stride * v_stride;

        for (i = 0; i < height; i++, base_y += h_stride) {
            fwrite(base_y, 1, width, file);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride / 2) {
            fwrite(base_c, 1, width / 2, file);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride / 2) {
            fwrite(base_c, 1, width / 2, file);
        }
    } break;
    case MPP_FMT_YUV444SP : {
        /* YUV444SP -> YUV444P for better display */
        int i, j;
        uint8_t *base_y = base;
        uint8_t *base_c = base + h_stride * v_stride;
        uint8_t *tmp = mpp_malloc(uint8_t, h_stride * height * 2);
        uint8_t *tmp_u = tmp;
        uint8_t *tmp_v = tmp + width * height;

        for (i = 0; i < height; i++, base_y += h_stride)
            fwrite(base_y, 1, width, file);

        for (i = 0; i < height; i++, base_c += h_stride * 2) {
            for (j = 0; j < width; j++) {
                tmp_u[j] = base_c[2 * j + 0];
                tmp_v[j] = base_c[2 * j + 1];
            }
            tmp_u += width;
            tmp_v += width;
        }

        fwrite(tmp, 1, width * height * 2, file);
        mpp_free(tmp);
    } break;
    default : {
        ALOGE("not supported format %d", fmt);
    } break;
    }
}

void CommonUtil::dumpMppPacketToFile(MppPacket packet, FILE *file)
{
    uint8_t *data;
    int len;

    if (NULL == file || NULL == packet)
        return;

    data = (uint8_t*)mpp_packet_get_pos(packet);
    len = mpp_packet_get_length(packet);

    fwrite(data, 1, len, file);
    fflush(file);
}

void CommonUtil::dumpDataToFile(char *data, size_t size, FILE *file)
{
    if (NULL == file || NULL == data)
        return;

    fwrite(data, 1, size, file);
    fflush(file);
}

void CommonUtil::dumpDataToFile(char *data, size_t size, const char *fileName)
{
    FILE *file = NULL;

    file = fopen(fileName, "w+b");
    if (NULL == file) {
        ALOGE("failed to open file %s - %s", fileName, strerror(errno));
        return;
    }

    fwrite(data, 1, size, file);
    fflush(file);
    fclose(file);
}

void CommonUtil::dumpDmaFdToFile(int fd, size_t size, FILE *file)
{
    void *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr != NULL) {
        fwrite(ptr, 1, size, file);
        fflush(file);
    } else {
        ALOGE("failed to map fd value %d", fd);
    }
}

MPP_RET CommonUtil::storeFileData(const char *file_name, char **data, size_t *length)
{
    FILE   *file    = NULL;
    size_t  fileLen = 0;

    file = fopen(file_name, "rb");
    if (NULL == file) {
        ALOGE("failed to open file %s - %s", file_name, strerror(errno));
        return MPP_NOK;
    }

    fseek(file, 0L, SEEK_END);
    fileLen = ftell(file);
    rewind(file);

    *data = (char *)malloc(fileLen);
    if (NULL == *data) {
        ALOGE("failed to malloc buffer - file %s", file_name);
        fclose(file);
        return MPP_NOK;
    }

    fread(*data, 1, fileLen, file);
    *length = fileLen;

    fclose(file);

    return MPP_OK;
}

MPP_RET CommonUtil::cropImage(char *src, char *dst,
                              int src_width, int src_height,
                              int src_wstride, int src_hstride,
                              int dst_width, int dst_height)
{
    int ret = 0;
    void *rgaCtx = NULL;
    int srcFormat, dstFormat;
    rga_info_t rgasrc, rgadst;

    if (!g_rga_init) {
        RgaInit(&rgaCtx);
        g_rga_init = 1;
        ALOGD("init rga ctx done");
    }

    srcFormat = dstFormat = HAL_PIXEL_FORMAT_YCrCb_NV12;

    memset(&rgasrc, 0, sizeof(rga_info_t));
    rgasrc.fd = -1;
    rgasrc.mmuFlag = 1;
    rgasrc.virAddr = src;

    memset(&rgadst, 0, sizeof(rga_info_t));
    rgadst.fd = -1;
    rgadst.mmuFlag = 1;
    rgadst.virAddr = dst;

    rga_set_rect(&rgasrc.rect, 0, 0, src_width, src_height,
                 src_wstride, src_hstride, srcFormat);
    rga_set_rect(&rgadst.rect, 0, 0, dst_width, dst_height,
                 dst_width, dst_height, dstFormat);

    ret = RgaBlit(&rgasrc, &rgadst, NULL);
    if (ret) {
        ALOGE("failed to rga blit ret %d", ret);
        return MPP_NOK;
    }

    return MPP_OK;
}

MPP_RET CommonUtil::readImage(char *src, char *dst,
                              int width, int height,
                              int wstride, int hstride,
                              MppFrameFormat fmt)
{
    MPP_RET ret = MPP_OK;
    int row = 0;
    char *buf_y = dst;
    char *buf_u = buf_y + wstride * hstride;
    char *buf_v = buf_u + wstride * hstride / 4;

    switch (fmt) {
    case MPP_FMT_YUV420SP : {
        for (row = 0; row < height; row++) {
            memcpy(buf_y + row * wstride, src, width);
            src += width;
        }

        for (row = 0; row < height / 2; row++) {
            memcpy(buf_u + row * wstride, src, width);
            src += width;
        }
    } break;
    case MPP_FMT_YUV420P : {
        for (row = 0; row < height; row++) {
            memcpy(buf_y + row * wstride, src, width);
            src += width;
        }

        for (row = 0; row < height / 2; row++) {
            memcpy(buf_u + row * wstride / 2, src, width / 2);
            src += width / 2;
        }

        for (row = 0; row < height / 2; row++) {
            memcpy(buf_v + row * wstride / 2, src, width / 2);
            src += width / 2;
        }
    } break;
    case MPP_FMT_RGBA8888 :
    case MPP_FMT_ABGR8888 :
    case MPP_FMT_ARGB8888 : {
        for (row = 0; row < height; row++) {
            memcpy(buf_y + row * wstride * 4, src, width * 4);
            src += width * 4;
        }
    } break;
    case MPP_FMT_YUV422_YUYV :
    case MPP_FMT_YUV422_UYVY : {
        for (row = 0; row < height; row++) {
            memcpy(buf_y + row * wstride * 2, src, width * 2);
            src += width * 2;
        }
    } break;
    case MPP_FMT_RGB888 :
    case MPP_FMT_BGR888 : {
    for (row = 0; row < height; row++) {
        memcpy(buf_y + row * wstride * 3, src, width * 3);
        src += width * 3;
    }
    } break;
    default : {
        ALOGE("read image do not support fmt %d", fmt);
        ret = MPP_ERR_VALUE;
    } break;
    }

    return ret;
}

int CommonUtil::envGetU32(const char *name, uint32_t *value, uint32_t default_value)
{
   char prop[PROP_VALUE_MAX + 1];
   int len = __system_property_get(name, prop);
   if (len > 0) {
       char *endptr;
       int base = (prop[0] == '0' && prop[1] == 'x') ? (16) : (10);
       errno = 0;
       *value = strtoul(prop, &endptr, base);
       if (errno || (prop == endptr)) {
           errno = 0;
           *value = default_value;
       }
   } else {
       *value = default_value;
   }

   return 0;
}

int CommonUtil::envGetStr(const char *name, const char **value, const char *default_value)
{
   static unsigned char env_str[2][PROP_VALUE_MAX + 1];
   static int32_t env_idx = 0;
   char *prop = reinterpret_cast<char *>(env_str[env_idx]);
   int len = __system_property_get(name, prop);
   if (len > 0) {
       *value  = prop;
       env_idx = !env_idx;
   } else {
       *value = default_value;
   }

   return 0;
}

int CommonUtil::envSetU32(const char *name, uint32_t value)
{
   char buf[PROP_VALUE_MAX + 1 + 2];
   snprintf(buf, sizeof(buf), "0x%x", value);
   int len = __system_property_set(name, buf);
   return (len) ? (0) : (-1);
}

int CommonUtil::envSetStr(const char *name, char *value)
{
   int len = __system_property_set(name, value);
   return (len) ? (0) : (-1);
}

bool CommonUtil::isValidDmaFd(int fd)
{
    /* detect input file handle */
    int fs_flag = fcntl(fd, F_GETFL, NULL);
    int fd_flag = fcntl(fd, F_GETFD, NULL);
    if (fs_flag == -1 || fd_flag == -1) {
        return false;;
    }

    return true;
}

void CommonUtil::setPerformanceMode(int on)
{
    int fd = -1;

    fd = open("/sys/class/devfreq/dmc/system_status", O_WRONLY);
    if (fd  == -1) {
        ALOGD("failed to open /sys/class/devfreq/dmc/system_status");
    }

    if (fd != -1) {
        ALOGD("%s performance mode", (on == 1) ? "config" : "clear");
        write(fd, (on == 1) ? "p" : "n", 1);
        close(fd);
    } else {
        ALOGD("failed to open /sys/class/devfreq/dmc/system_status");
    }
}
