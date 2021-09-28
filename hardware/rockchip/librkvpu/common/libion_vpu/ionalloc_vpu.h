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

#ifndef ANDROID_IONALLOC_INTERFACE_VPU_C_H
#define ANDROID_IONALLOC_INTERFACE_VPU_C_H

enum {
        ION_MODULE_PERFORM_QUERY_BUFCOUNT = 0x03140001,
        ION_MODULE_PERFORM_QUERY_CLIENT_ALLOCATED,
        ION_MODULE_PERFORM_QUERY_HEAP_SIZE,  
        ION_MODULE_PERFORM_QUERY_HEAP_ALLOCATED,
};
enum ion_module_id {
        ION_MODULE_VPU = 0,
        ION_MODULE_CAM,
        ION_MODULE_UI,
        ION_NUM_MODULES,
};
enum _ion_heap_type {
        _ION_HEAP_RESERVE = 0,
        _ION_HEAP_VMALLOC,
        _ION_HEAP_KZALLOC,
        _ION_HEAP_IOMMU,
        _ION_NUM_HEAPS,
};

enum cache_op_type {
        ION_CLEAN_CACHE = 0,
        ION_INVALID_CACHE,
        ION_FLUSH_CACHE,
};
typedef struct ion_buffer_t {
	void *virt;
	unsigned long phys;
	unsigned long size;
        void *reserved;
}ion_buffer_t;

typedef struct ion_device_t {
        int (* alloc)(struct ion_device_t *ion, unsigned long size, enum _ion_heap_type type, ion_buffer_t **data);
        int (* free)(struct ion_device_t *ion, ion_buffer_t *data);

        int (* share)(struct ion_device_t *ion, ion_buffer_t *data, int *share_fd);

        int (* map)(struct ion_device_t *ion, int share_fd, ion_buffer_t **data);
        int (* unmap)(struct ion_device_t *ion, ion_buffer_t *data);

        int (* cache_op)(struct ion_device_t *ion, ion_buffer_t *data, enum cache_op_type type);

        int (* perform)(struct ion_device_t *ion, int operation, ... );
        void *reserved;

}ion_device_t;

unsigned long ion_get_flags(enum ion_module_id id, enum _ion_heap_type type);
int ion_open_vpu(unsigned long align, enum ion_module_id id, ion_device_t **ion);
int ion_close_vpu(ion_device_t *ion);

#endif /* ANDROID_IONALLOC_INTERFACE_C_H */
