/*
 * Copyright (C) 2020 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
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
#pragma once

#include "mali_gralloc_formats.h"

extern mali_gralloc_format_caps cpu_runtime_caps;
extern mali_gralloc_format_caps dpu_runtime_caps;
extern mali_gralloc_format_caps dpu_aeu_runtime_caps;
extern mali_gralloc_format_caps vpu_runtime_caps;
extern mali_gralloc_format_caps gpu_runtime_caps;
extern mali_gralloc_format_caps cam_runtime_caps;

/*
 * Obtains the capabilities of each media system IP that form the producers
 * and consumers. Default capabilities are assigned (within this function) for
 * each IP, based on CFLAGS which specify the version of each IP (or, for GPU,
 * explicit features):
 * - GPU: MALI_GPU_SUPPORT_*
 * - DPU: MALI_DISPLAY_VERSION
 * - VPU: MALI_VIDEO_VERSION
 *
 * See src/Android.mk for default values.
 *
 * These defaults can be overridden by runtime capabilities defined in the
 * userspace drivers (*.so) loaded for each IP. The drivers should define a
 * symbol named MALI_GRALLOC_FORMATCAPS_SYM_NAME, which contains all
 * capabilities from set MALI_GRALLOC_FORMAT_CAPABILITY_*
 *
 * @return none.
 *
 * NOTE: although no capabilities are returned, global variables global variables
 * named '*_runtime_caps' are updated.
 */
void get_ip_capabilities(void);

#ifdef __cplusplus
extern "C" {
#endif

void mali_gralloc_get_caps(struct mali_gralloc_format_caps *gpu_caps,
                           struct mali_gralloc_format_caps *vpu_caps,
                           struct mali_gralloc_format_caps *dpu_caps,
                           struct mali_gralloc_format_caps *dpu_aeu_caps,
                           struct mali_gralloc_format_caps *cam_caps);
#ifdef __cplusplus
}
#endif
