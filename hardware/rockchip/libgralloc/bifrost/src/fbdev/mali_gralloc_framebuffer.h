/*
 * Copyright (C) 2019-2020 ARM Limited. All rights reserved.
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

#ifndef MALI_GRALLOC_FRAMEBUFFER_H
#define MALI_GRALLOC_FRAMEBUFFER_H

#include <hardware/hardware.h>
#include <hardware/fb.h>

#include "core/mali_gralloc_bufferdescriptor.h"
#include "1.x/mali_gralloc_module.h"

int32_t mali_gralloc_fb_allocate(private_module_t * const module,
                                 const buffer_descriptor_t * const bufDescriptor,
                                 buffer_handle_t * const outBuffers);

int mali_gralloc_fb_module_init(private_module_t *module);
#endif
