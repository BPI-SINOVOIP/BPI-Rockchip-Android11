/*
 * Copyright (C) 2020 ARM Limited. All rights reserved.
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

#if HIDL_MAPPER_VERSION_SCALED == 200
#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#elif HIDL_MAPPER_VERSION_SCALED == 210
#include <android/hardware/graphics/mapper/2.1/IMapper.h>
#endif

using android::hardware::graphics::mapper::V2_0::Error;
using android::hardware::graphics::mapper::V2_0::YCbCrLayout;
using android::hardware::graphics::mapper::V2_0::BufferDescriptor;
using android::hardware::graphics::common::V1_1::PixelFormat;
using android::hardware::graphics::common::V1_1::BufferUsage;
using android::hardware::graphics::mapper::V2_1::IMapper;

#endif
