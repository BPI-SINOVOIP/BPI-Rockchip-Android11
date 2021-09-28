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

#ifndef ANDROID_DRM_CONNECTOR_H_
#define ANDROID_DRM_CONNECTOR_H_

#include "drmencoder.h"
#include "drmmode.h"
#include "drmproperty.h"

#include <stdint.h>
#include <vector>
#include <xf86drmMode.h>

namespace android {

class DrmResources;

class DrmConnector {
 public:
  DrmConnector(DrmResources *drm, drmModeConnectorPtr c,
               DrmEncoder *current_encoder,
               std::vector<DrmEncoder *> &possible_encoders);
  DrmConnector(const DrmProperty &) = delete;
  DrmConnector &operator=(const DrmProperty &) = delete;

  int Init();

  uint32_t id() const;
  uint32_t type_id() const;

  int display() const;
  void set_display(int display);

  void set_display_possible(int display_bit);

  bool built_in() const;

  int UpdateModes();

  const std::vector<DrmMode> &modes() const {
    return modes_;
  }
  const std::vector<DrmMode> &raw_modes() const {
    return raw_modes_;
  }

  const DrmMode &best_mode() const;
  const DrmMode &active_mode() const;
  const DrmMode &current_mode() const;
  void set_best_mode(const DrmMode &mode);
  void set_active_mode(const DrmMode &mode);
  void set_current_mode(const DrmMode &mode);
  void SetDpmsMode(uint32_t dpms_mode);

  const DrmProperty &dpms_property() const;
  const DrmProperty &crtc_id_property() const;
  const DrmProperty &brightness_id_property() const;
  const DrmProperty &contrast_id_property() const;
  const DrmProperty &saturation_id_property() const;
  const DrmProperty &hue_id_property() const;
  const DrmProperty &hdr_metadata_property() const;
  const DrmProperty &hdr_panel_property() const;
  const DrmProperty &hdmi_output_colorimetry_property() const;
  const DrmProperty &hdmi_output_format_property() const;
  const DrmProperty &hdmi_output_depth_property() const;
  const std::vector<DrmEncoder *> &possible_encoders() const {
    return possible_encoders_;
  }
  DrmEncoder *encoder() const;
  void set_encoder(DrmEncoder *encoder);

  drmModeConnection state() const;
  drmModeConnection raw_state() const;
  void force_disconnect(bool force);
  int priority() const;
  void set_priority(int priority);

  uint32_t get_type() { return type_; }
  int possible_displays() { return possible_displays_; }

  bool isSupportSt2084() { return bSupportSt2084_; }
  bool isSupportHLG() { return bSupportHLG_; }
  bool is_hdmi_support_hdr() const;

  uint32_t mm_width() const;
  uint32_t mm_height() const;
  drmModeConnectorPtr get_connector() { return connector_; }
  void update_state(drmModeConnection state);
  void update_size(int w, int h);
  void dump_connector(std::ostringstream *out) const;

 private:
  DrmResources *drm_;

  uint32_t id_;
  uint32_t type_id_;
  DrmEncoder *encoder_;
  int display_;

  uint32_t type_;
  int priority_;
  drmModeConnection state_;
  bool force_disconnect_;

  uint32_t mm_width_;
  uint32_t mm_height_;

  DrmMode active_mode_;
  DrmMode current_mode_;
  DrmMode best_mode_;
  std::vector<DrmMode> modes_;
  std::vector<DrmMode> raw_modes_;

  DrmProperty dpms_property_;
  DrmProperty crtc_id_property_;
  DrmProperty brightness_id_property_;
  DrmProperty contrast_id_property_;
  DrmProperty saturation_id_property_;
  DrmProperty hue_id_property_;
  DrmProperty hdr_metadata_property_;
  DrmProperty hdr_panel_property_;
  DrmProperty hdmi_output_colorimetry_;
  DrmProperty hdmi_output_format_;
  DrmProperty hdmi_output_depth_;

  std::vector<DrmEncoder *> possible_encoders_;
  uint32_t possible_displays_;

  bool bSupportSt2084_;
  bool bSupportHLG_;

  drmModeConnectorPtr connector_;
};
}

#endif  // ANDROID_DRM_PLANE_H_
