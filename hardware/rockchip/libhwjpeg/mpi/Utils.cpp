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

static int rga_init = 0;

void dump_mpp_frame_to_file(MppFrame frame, FILE *fp)
{
    int width    = 0;
    int height   = 0;
    int h_stride = 0;
    int v_stride = 0;
    MppFrameFormat fmt  = MPP_FMT_YUV420SP;
    MppBuffer buffer    = NULL;
    uint8_t *base = NULL;

    if (NULL == fp || NULL == frame)
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
            fwrite(base_y, 1, width, fp);

        for (i = 0; i < height; i++, base_c += h_stride) {
            for (j = 0; j < width / 2; j++) {
                tmp_u[j] = base_c[2 * j + 0];
                tmp_v[j] = base_c[2 * j + 1];
            }
            tmp_u += width / 2;
            tmp_v += width / 2;
        }

        fwrite(tmp, 1, width * height, fp);
        mpp_free(tmp);
    } break;
    case MPP_FMT_YUV420SP : {
        int i;
        uint8_t *base_y = base;
        uint8_t *base_c = base + h_stride * v_stride;

        for (i = 0; i < height; i++, base_y += h_stride) {
            fwrite(base_y, 1, width, fp);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride) {
            fwrite(base_c, 1, width, fp);
        }
    } break;
    case MPP_FMT_YUV420P : {
        int i;
        uint8_t *base_y = base;
        uint8_t *base_c = base + h_stride * v_stride;

        for (i = 0; i < height; i++, base_y += h_stride) {
            fwrite(base_y, 1, width, fp);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride / 2) {
            fwrite(base_c, 1, width / 2, fp);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride / 2) {
            fwrite(base_c, 1, width / 2, fp);
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
            fwrite(base_y, 1, width, fp);

        for (i = 0; i < height; i++, base_c += h_stride * 2) {
            for (j = 0; j < width; j++) {
                tmp_u[j] = base_c[2 * j + 0];
                tmp_v[j] = base_c[2 * j + 1];
            }
            tmp_u += width;
            tmp_v += width;
        }

        fwrite(tmp, 1, width * height * 2, fp);
        mpp_free(tmp);
    } break;
    default : {
        ALOGE("not supported format %d", fmt);
    } break;
    }
}

void dump_mpp_packet_to_file(MppPacket packet, FILE *fp)
{
    uint8_t *data;
    int len;

    if (NULL == fp || NULL == packet)
        return;

    data = (uint8_t*)mpp_packet_get_pos(packet);
    len = mpp_packet_get_length(packet);

    fwrite(data, 1, len, fp);
    fflush(fp);
}

void dump_data_to_file(uint8_t *data, int size, FILE *fp)
{
    if (NULL == fp || NULL == data)
        return;

    fwrite(data, 1, size, fp);
    fflush(fp);
}

MPP_RET dump_dma_fd_to_file(int fd, size_t size, FILE *fp)
{
    void *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr != NULL) {
        fwrite(ptr, 1, size, fp);
        fflush(fp);
        return MPP_OK;
    } else {
        ALOGE("failed to map fd value %d", fd);
        return MPP_NOK;
    }
}

MPP_RET get_file_ptr(const char *file_name, char **buf, size_t *size)
{
    FILE *fp = NULL;
    size_t file_size = 0;

    fp = fopen(file_name, "rb");
    if (NULL == fp) {
        ALOGE("failed to open file %s - %s", file_name, strerror(errno));
        return MPP_NOK;
    }

    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    *buf = (char*)malloc(file_size);
    if (NULL == *buf) {
        ALOGE("failed to malloc buffer - file %s", file_name);
        fclose(fp);
        return MPP_NOK;
    }

    fread(*buf, 1, file_size, fp);
    *size = file_size;
    fclose(fp);

    return MPP_OK;
}

MPP_RET dump_ptr_to_file(char *buf, size_t size, const char *output_file)
{
    FILE *fp = NULL;

    fp = fopen(output_file, "w+b");
    if (NULL == fp) {
        ALOGE("failed to open file %s - %s", output_file, strerror(errno));
        return MPP_NOK;
    }

    fwrite(buf, 1, size, fp);
    fflush(fp);
    fclose(fp);

    return MPP_OK;
}

MPP_RET crop_yuv_image(uint8_t *src, uint8_t *dst, int src_width, int src_height,
                       int src_wstride, int src_hstride,
                       int dst_width, int dst_height)
{
    int ret = 0;
    void *rga_ctx = NULL;
    int srcFormat, dstFormat;
    rga_info_t rgasrc, rgadst;

    if (!rga_init) {
        RgaInit(&rga_ctx);
        rga_init = 1;
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
                 dst_width, dst_height, srcFormat);

    ret = RgaBlit(&rgasrc, &rgadst, NULL);
    if (ret) {
        ALOGE("failed to rga blit ret %d", ret);
        return MPP_NOK;
    }

    return MPP_OK;
}

MPP_RET read_yuv_image(uint8_t *dst, uint8_t *src, int width, int height,
                       int hor_stride, int ver_stride, MppFrameFormat fmt)
{
    MPP_RET ret = MPP_OK;
    int row = 0;
    uint8_t *buf_y = dst;
    uint8_t *buf_u = buf_y + hor_stride * ver_stride; // NOTE: diff from gen_yuv_image
    uint8_t *buf_v = buf_u + hor_stride * ver_stride / 4; // NOTE: diff from gen_yuv_image

    switch (fmt) {
    case MPP_FMT_YUV420SP : {
        for (row = 0; row < height; row++) {
            memcpy(buf_y + row * hor_stride, src, width);
            src += width;
        }

        for (row = 0; row < height / 2; row++) {
            memcpy(buf_u + row * hor_stride, src, width);
            src += width;
        }
    } break;
    case MPP_FMT_YUV420P : {
        for (row = 0; row < height; row++) {
            memcpy(buf_y + row * hor_stride, src, width);
            src += width;
        }

        for (row = 0; row < height / 2; row++) {
            memcpy(buf_u + row * hor_stride / 2, src, width / 2);
            src += width / 2;
        }

        for (row = 0; row < height / 2; row++) {
            memcpy(buf_v + row * hor_stride / 2, src, width / 2);
            src += width / 2;
        }
    } break;
    case MPP_FMT_RGBA8888 :
    case MPP_FMT_ABGR8888 :
    case MPP_FMT_ARGB8888 : {
        for (row = 0; row < height; row++) {
            memcpy(buf_y + row * hor_stride * 4, src, width * 4);
            src += width * 4;
        }
    } break;
    case MPP_FMT_YUV422_YUYV :
    case MPP_FMT_YUV422_UYVY : {
        for (row = 0; row < height; row++) {
            memcpy(buf_y + row * hor_stride * 2, src, width * 2);
            src += width * 2;
        }
    } break;
    case MPP_FMT_RGB888 :
    case MPP_FMT_BGR888 : {
    for (row = 0; row < height; row++) {
        memcpy(buf_y + row * hor_stride * 3, src, width * 3);
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

MPP_RET fill_yuv_image(uint8_t *buf, int width, int height,
                       int hor_stride, int ver_stride, MppFrameFormat fmt,
                       int frame_count)
{
    MPP_RET ret = MPP_OK;
    uint8_t *buf_y = buf;
    uint8_t *buf_c = buf + hor_stride * ver_stride;
    int x, y;

    switch (fmt) {
    case MPP_FMT_YUV420SP : {
        uint8_t *p = buf_y;

        for (y = 0; y < height; y++, p += hor_stride) {
            for (x = 0; x < width; x++) {
                p[x] = x + y + frame_count * 3;
            }
        }

        p = buf_c;
        for (y = 0; y < height / 2; y++, p += hor_stride) {
            for (x = 0; x < width / 2; x++) {
                p[x * 2 + 0] = 128 + y + frame_count * 2;
                p[x * 2 + 1] = 64  + x + frame_count * 5;
            }
        }
    } break;
    case MPP_FMT_YUV420P : {
        uint8_t *p = buf_y;

        for (y = 0; y < height; y++, p += hor_stride) {
            for (x = 0; x < width; x++) {
                p[x] = x + y + frame_count * 3;
            }
        }

        p = buf_c;
        for (y = 0; y < height / 2; y++, p += hor_stride / 2) {
            for (x = 0; x < width / 2; x++) {
                p[x] = 128 + y + frame_count * 2;
            }
        }

        p = buf_c + hor_stride * ver_stride / 4;
        for (y = 0; y < height / 2; y++, p += hor_stride / 2) {
            for (x = 0; x < width / 2; x++) {
                p[x] = 64 + x + frame_count * 5;
            }
        }
    } break;
    case MPP_FMT_YUV422_UYVY : {
        uint8_t *p = buf_y;

        for (y = 0; y < height; y++, p += hor_stride) {
            for (x = 0; x < width / 2; x++) {
                p[x * 4 + 1] = x * 2 + 0 + y + frame_count * 3;
                p[x * 4 + 3] = x * 2 + 1 + y + frame_count * 3;
                p[x * 4 + 0] = 128 + y + frame_count * 2;
                p[x * 4 + 2] = 64  + x + frame_count * 5;
            }
        }
    } break;
    default : {
        ALOGE("filling function do not support type %d", fmt);
        ret = MPP_NOK;
    } break;
    }
    return ret;
}

/*
 * NOTE: __system_property_set only available after android-21
 * So the library should compiled on latest ndk
 */
int32_t env_get_u32(const char *name, uint32_t *value, uint32_t default_value) {
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

int32_t env_get_str(const char *name, const char **value, const char *default_value) {
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

int32_t env_set_u32(const char *name, uint32_t value) {
   char buf[PROP_VALUE_MAX + 1 + 2];
   snprintf(buf, sizeof(buf), "0x%x", value);
   int len = __system_property_set(name, buf);
   return (len) ? (0) : (-1);
}

int32_t env_set_str(const char *name, char *value) {
   int len = __system_property_set(name, value);
   return (len) ? (0) : (-1);
}


bool is_valid_dma_fd(int fd)
{
    /* detect input file handle */
    int fs_flag = fcntl(fd, F_GETFL, NULL);
    int fd_flag = fcntl(fd, F_GETFD, NULL);
    if (fs_flag == -1 || fd_flag == -1) {
        return false;;
    }

    return true;
}

void set_performance_mode(int on) {
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
