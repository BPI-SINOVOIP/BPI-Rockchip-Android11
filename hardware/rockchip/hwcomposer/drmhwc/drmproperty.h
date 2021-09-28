/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ANDROID_DRM_PROPERTY_H_
#define ANDROID_DRM_PROPERTY_H_

#include <stdint.h>
#include <string>
#include <xf86drmMode.h>
#include <vector>

namespace android {

enum DrmPropertyType {
  DRM_PROPERTY_TYPE_INT,
  DRM_PROPERTY_TYPE_ENUM,
  DRM_PROPERTY_TYPE_OBJECT,
  DRM_PROPERTY_TYPE_BLOB,
  DRM_PROPERTY_TYPE_BITMASK,
  DRM_PROPERTY_TYPE_INVALID,
};

class DrmProperty {
 public:
  DrmProperty() = default;
  DrmProperty(drmModePropertyPtr p, uint64_t value);
  DrmProperty(const DrmProperty &) = delete;
  DrmProperty &operator=(const DrmProperty &) = delete;

  void Init(drmModePropertyPtr p, uint64_t value);

  uint32_t id() const;
  std::string name() const;

  int value(uint64_t *value) const;

  void set_feature(const char* pcFeature)const;

 private:
  class DrmPropertyEnum {
   public:
    DrmPropertyEnum(drm_mode_property_enum *e);
    ~DrmPropertyEnum();

    uint64_t value_;
    std::string name_;
  };

  uint32_t id_ = 0;

  DrmPropertyType type_ = DRM_PROPERTY_TYPE_INVALID;
  uint32_t flags_ = 0;
  std::string name_;
  mutable const char* feature_name_;
  uint64_t value_ = 0;

  std::vector<uint64_t> values_;
  std::vector<DrmPropertyEnum> enums_;
  std::vector<uint32_t> blob_ids_;
};
}

#endif  // ANDROID_DRM_PROPERTY_H_
