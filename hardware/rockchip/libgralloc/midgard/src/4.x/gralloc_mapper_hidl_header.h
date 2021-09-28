/*
 * Copyright (C) 2020 Arm Limited. All rights reserved.
 *
 * Copyright 2016 The Android Open Source Project
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

#ifndef GRALLOC_MAPPER_HIDL_HEADER_H
#define GRALLOC_MAPPER_HIDL_HEADER_H

#include <android/hardware/graphics/mapper/4.0/IMapper.h>
#include "mali_fourcc.h"
#include <gralloctypes/Gralloc4.h>

using android::hardware::graphics::mapper::V4_0::Error;
using android::hardware::graphics::mapper::V4_0::BufferDescriptor;
using android::hardware::graphics::common::V1_2::PixelFormat;
using android::hardware::graphics::common::V1_2::BufferUsage;
using android::hardware::graphics::mapper::V4_0::IMapper;

#endif
