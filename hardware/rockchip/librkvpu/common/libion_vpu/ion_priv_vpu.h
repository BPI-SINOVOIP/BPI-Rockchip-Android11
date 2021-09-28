/*
 * Copyright(C) 2010 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved
 * BY DOWNLOADING, INSTALLING, COPYING, SAVING OR OTHERWISE USING THIS
 * SOFTWARE, YOU ACKNOWLEDGE THAT YOU AGREE THE SOFTWARE RECEIVED FORM ROCKCHIP
 * IS PROVIDED TO YOU ON AN "AS IS" BASIS and ROCKCHIP DISCLAIMS ANY AND ALL
 * WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH FILE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED
 * WARRANTIES OF TITLE, NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY,
 * ACCURACY OR FITNESS FOR A PARTICULAR PURPOSE
 * Rockchip hereby grants to you a limited, non-exclusive, non-sublicensable and
 * non-transferable license
 *   (a) to install, save and use the Software;
 *   (b) to * copy and distribute the Software in binary code format only
 * Except as expressively authorized by Rockchip in writing, you may NOT:
 *   (a) distribute the Software in source code;
 *   (b) distribute on a standalone basis but you may distribute the Software in
 *   conjunction with platforms incorporating Rockchip integrated circuits;
 *   (c) modify the Software in whole or part;
 *   (d) decompile, reverse-engineer, dissemble, or attempt to derive any source
 *   code from the Software;
 *   (e) remove or obscure any copyright, patent, or trademark statement or
 *   notices contained in the Software
 */

#ifndef ANDROID_IONALLOC_PRIV_H
#define ANDROID_IONALLOC_PRIV_H

#include <stdlib.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <cutils/log.h>
#include <errno.h>
#include <linux/ioctl.h>
#include "ionalloc_vpu.h"

#include "linux/ion.h"

#define ION_DEVICE "/dev/ion"
enum {
    FD_INIT = -1,
};

typedef struct private_handle_t {
    struct ion_buffer_t data;
    int fd;
    int pid;
    ion_user_handle_t handle;
#define NUM_INTS    2
#define NUM_FDS     1
#define MAGIC       0x3141592
    int s_num_ints;
    int s_num_fds;
    int s_magic;
}private_handle_t;

typedef struct private_device_t {
    ion_device_t ion;
    int ionfd;
    unsigned long align;
    enum ion_module_id id;
}private_device_t;

#endif /* ANDROID_IONALLOC_PRIV_H */

