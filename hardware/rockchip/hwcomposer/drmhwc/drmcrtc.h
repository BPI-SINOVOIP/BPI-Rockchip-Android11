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

#ifndef ANDROID_DRM_CRTC_H_
#define ANDROID_DRM_CRTC_H_

#include "drmmode.h"
#include "drmproperty.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace android {

class DrmResources;

class DrmCrtc {
 public:
  DrmCrtc(DrmResources *drm, drmModeCrtcPtr c, unsigned pipe);
  DrmCrtc(const DrmCrtc &) = delete;
  DrmCrtc &operator=(const DrmCrtc &) = delete;

  int Init();

  uint32_t id() const;
  unsigned pipe() const;

  bool can_bind(int display) const;
  bool can_overscan() const;

  const DrmProperty &active_property() const;
  const DrmProperty &mode_property() const;
  bool get_afbc() const;
  bool get_alpha_scale() const;
  const DrmProperty &left_margin_property() const;
  const DrmProperty &right_margin_property() const;
  const DrmProperty &top_margin_property() const;
  const DrmProperty &bottom_margin_property() const;
  const DrmProperty &alpha_scale_property() const;
  void dump_crtc(std::ostringstream *out) const;


  DrmResources *getDrmReoources()
  {
        return drm_;
  }

 private:
  DrmResources *drm_;

  uint32_t id_;
  unsigned pipe_;
  int display_;

  uint32_t x_;
  uint32_t y_;
  uint32_t width_;
  uint32_t height_;
  bool b_afbc_;

  DrmMode mode_;
  bool mode_valid_;
  bool can_overscan_;
  bool can_alpha_scale_;

  DrmProperty active_property_;
  DrmProperty mode_property_;
  DrmProperty feature_property_;
  DrmProperty left_margin_property_;
  DrmProperty top_margin_property_;
  DrmProperty right_margin_property_;
  DrmProperty bottom_margin_property_;
  DrmProperty alpha_scale_property_;
  drmModeCrtcPtr crtc_;
};
}

#endif  // ANDROID_DRM_CRTC_H_
