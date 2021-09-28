/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ANDROID_PLATFORM_HISI_H_
#define ANDROID_PLATFORM_HISI_H_

#include "drmdevice.h"
#include "platform.h"
#include "platformdrmgeneric.h"

#include <stdatomic.h>

#include <hardware/gralloc.h>

namespace android {

class MesonImporter : public DrmGenericImporter {
 public:
  using DrmGenericImporter::DrmGenericImporter;

  int ConvertBoInfo(buffer_handle_t handle, hwc_drm_bo_t *bo) override;

 private:
  uint64_t ConvertGrallocFormatToDrmModifiers(uint64_t flags);
};
}  // namespace android

#endif
