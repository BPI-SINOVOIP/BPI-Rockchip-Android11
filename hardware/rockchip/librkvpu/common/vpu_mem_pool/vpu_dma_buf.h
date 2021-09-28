/*
 *
 * Copyright 2014 Rockchip Electronics S.LSI Co. LTD
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

#ifndef _VPU_DMA_BUF_H_
#define _VPU_DMA_BUF_H_

#include "../include/vpu_mem.h"
#include "classmagic.h"

#define vpu_dmabuf_dev_FIELDS \
    int (*alloc)(struct vpu_dmabuf_dev *dev, size_t size, VPUMemLinear_t **data); \
    int (*free)(struct vpu_dmabuf_dev *dev, VPUMemLinear_t *idata); \
    int (*map)(struct vpu_dmabuf_dev *dev, int origin_fd, size_t size, VPUMemLinear_t **data); \
    int (*unmap)(struct vpu_dmabuf_dev *dev, VPUMemLinear_t *idata); \
    int (*share)(struct vpu_dmabuf_dev *dev, VPUMemLinear_t *idata, VPUMemLinear_t **out_data); \
    int (*reserve)(VPUMemLinear_t *idata, int origin_fd, void *priv); \
    int (*get_origin_fd)(VPUMemLinear_t *idata); \
    int (*get_fd)(VPUMemLinear_t *idata); \
    int (*get_ref)(VPUMemLinear_t *idata); \
    void* (*get_priv)(VPUMemLinear_t *idata);\
    int (*get_phyaddr)(struct vpu_dmabuf_dev *dev,int share_fd,uint32_t *phy_addr);

typedef struct vpu_dmabuf_dev {
    vpu_dmabuf_dev_FIELDS
} vpu_dmabuf_dev;

int vpu_mem_judge_used_heaps_type();

int vpu_dmabuf_open(unsigned long align, struct vpu_dmabuf_dev **dev, char *title);
int vpu_dmabuf_close(struct vpu_dmabuf_dev *dev);

#endif
