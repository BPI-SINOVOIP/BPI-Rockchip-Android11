// Copyright (C) 2020 The Android Open Source Project
// Copyright (C) 2020 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#ifndef HOST_BUILD
#include "drm.h"
#endif

#define DRM_VIRTGPU_RESOURCE_CREATE_BLOB 0x0a

#define VIRTGPU_PARAM_RESOURCE_BLOB 3 /* DRM_VIRTGPU_RESOURCE_CREATE_BLOB */
#define VIRTGPU_PARAM_HOST_VISIBLE 4

struct drm_virtgpu_resource_create_blob {
#define VIRTGPU_BLOB_MEM_GUEST              0x0001
#define VIRTGPU_BLOB_MEM_HOST               0x0002
#define VIRTGPU_BLOB_MEM_HOST_GUEST         0x0003

#define VIRTGPU_BLOB_FLAG_MAPPABLE          0x0001
#define VIRTGPU_BLOB_FLAG_SHAREABLE         0x0002
#define VIRTGPU_BLOB_FLAG_CROSS_DEVICE      0x0004
	/* zero is invalid blob_mem */
    uint32_t blob_mem;
    uint32_t blob_flags;
    uint32_t bo_handle;
    uint32_t res_handle;
    uint64_t size;

	/*
	 * for 3D contexts with VIRTGPU_BLOB_MEM_HOSTGUEST and
	 * VIRTGPU_BLOB_MEM_HOST otherwise, must be zero.
	 */
	uint32_t pad;
    uint32_t cmd_size;
    uint64_t cmd;
    uint64_t blob_id;
};


#define DRM_IOCTL_VIRTGPU_RESOURCE_CREATE_BLOB              \
        DRM_IOWR(DRM_COMMAND_BASE + DRM_VIRTGPU_RESOURCE_CREATE_BLOB,   \
                        struct drm_virtgpu_resource_create_blob)
