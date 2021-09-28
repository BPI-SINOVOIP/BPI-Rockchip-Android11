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

#define LOG_TAG "hwc-drm-connector"

#include "drmconnector.h"
#include "drmresources.h"

#include <errno.h>
#include <stdint.h>

#ifdef ANDROID_P
#include <log/log.h>
#else
#include <cutils/log.h>
#endif

#include <xf86drmMode.h>

#if DRM_DRIVER_VERSION==2
#define HDR_METADATA_PROPERTY "HDR_OUTPUT_METADATA"
#else
#define HDR_METADATA_PROPERTY "HDR_SOURCE_METADATA"
#endif

namespace android {

DrmConnector::DrmConnector(DrmResources *drm, drmModeConnectorPtr c,
                           DrmEncoder *current_encoder,
                           std::vector<DrmEncoder *> &possible_encoders)
    : drm_(drm),
      id_(c->connector_id),
      type_id_(c->connector_type_id),
      encoder_(current_encoder),
      display_(-1),
      type_(c->connector_type),
      priority_(-1),
      state_(c->connection),
      force_disconnect_(false),
      mm_width_(c->mmWidth),
      mm_height_(c->mmHeight),
      possible_encoders_(possible_encoders),
      connector_(c) {
}

int DrmConnector::Init() {
  ALOGW("DrmConnector init id=%d,type=%s",id_,drm_->connector_type_str(type_));
  int ret = drm_->GetConnectorProperty(*this, "DPMS", &dpms_property_);
  if (ret) {
    ALOGE("Could not get DPMS property\n");
    return ret;
  }
  ret = drm_->GetConnectorProperty(*this, "CRTC_ID", &crtc_id_property_);
  if (ret) {
    ALOGE("Could not get CRTC_ID property\n");
    return ret;
  }

  ret = drm_->GetConnectorProperty(*this, "brightness", &brightness_id_property_);
  if (ret)
    ALOGW("Could not get brightness property\n");

  ret = drm_->GetConnectorProperty(*this, "contrast", &contrast_id_property_);
  if (ret)
    ALOGW("Could not get contrast property\n");

  ret = drm_->GetConnectorProperty(*this, "saturation", &saturation_id_property_);
  if (ret)
    ALOGW("Could not get saturation property\n");

  ret = drm_->GetConnectorProperty(*this, "hue", &hue_id_property_);
  if (ret)
    ALOGW("Could not get hue property\n");

  ret = drm_->GetConnectorProperty(*this, HDR_METADATA_PROPERTY, &hdr_metadata_property_);
  if (ret)
    ALOGW("Could not get hdr source metadata property\n");

  ret = drm_->GetConnectorProperty(*this, "HDR_PANEL_METADATA", &hdr_panel_property_);
  if (ret)
    ALOGW("Could not get hdr panel metadata property\n");

  ret = drm_->GetConnectorProperty(*this, "hdmi_output_colorimetry", &hdmi_output_colorimetry_);
  if (ret)
    ALOGW("Could not get hdmi_output_colorimetry property\n");

  ret = drm_->GetConnectorProperty(*this, "hdmi_output_format", &hdmi_output_format_);
  if (ret) {
    ALOGW("Could not get hdmi_output_format property\n");
  }

  ret = drm_->GetConnectorProperty(*this, "hdmi_output_depth", &hdmi_output_depth_);
  if (ret) {
   ALOGW("Could not get hdmi_output_depth property\n");
  }

  bSupportSt2084_ = drm_->is_hdr_panel_support_st2084(this);
  bSupportHLG_    = drm_->is_hdr_panel_support_HLG(this);

  return 0;
}

bool DrmConnector::is_hdmi_support_hdr() const
{
    return (hdr_metadata_property_.id() && bSupportSt2084_) || (hdr_metadata_property_.id() && bSupportHLG_);
}

uint32_t DrmConnector::id() const {
  return id_;
}

uint32_t DrmConnector::type_id() const {
  return type_id_;
}

int DrmConnector::priority() const{
  return priority_;
}
void DrmConnector::set_priority(int priority){
  priority_ = priority;
}

int DrmConnector::display() const {
  return display_;
}

void DrmConnector::set_display(int display) {
  display_ = display;
}

void DrmConnector::set_display_possible(int possible_displays) {
  possible_displays_ = possible_displays;
}

bool DrmConnector::built_in() const {
  return type_ == DRM_MODE_CONNECTOR_LVDS || type_ == DRM_MODE_CONNECTOR_eDP ||
         type_ == DRM_MODE_CONNECTOR_DSI ||
         type_ == DRM_MODE_CONNECTOR_VIRTUAL ||
         type_ == DRM_MODE_CONNECTOR_TV ||
	 type_ == DRM_MODE_CONNECTOR_DPI;
}

const DrmMode &DrmConnector::best_mode() const {
  return best_mode_;
}

/* update mode infomation */
int DrmConnector::UpdateModes() {
  int fd = drm_->fd();

  drmModeConnectorPtr c = drmModeGetConnector(fd, id_);
  if (!c) {
    ALOGE("Failed to get connector %d", id_);
    return -ENODEV;
  }

  //When Plug-in/Plug-out TV panel,some Property of the connector will need be updated.
  bSupportSt2084_ = drm_->is_hdr_panel_support_st2084(this);

  state_ = c->connection;
  if (!c->count_modes)
    state_ = DRM_MODE_DISCONNECTED;

  std::vector<DrmMode> new_modes;
  for (int i = 0; i < c->count_modes; ++i) {
    bool exists = false;
    for (const DrmMode &mode : modes_) {
      if (mode == c->modes[i]) {
        if(type_ == DRM_MODE_CONNECTOR_HDMIA || type_ == DRM_MODE_CONNECTOR_DisplayPort)
        {
            //filter mode by /system/usr/share/resolution_white.xml.
            if(drm_->mode_verify(mode))
            {
                new_modes.push_back(mode);
                exists = true;
                break;
            }
        }
        else
        {
            new_modes.push_back(mode);
            exists = true;
            break;
        }
      }
    }
    if (exists)
      continue;

    DrmMode m(&c->modes[i]);
    if ((type_ == DRM_MODE_CONNECTOR_HDMIA || type_ == DRM_MODE_CONNECTOR_DisplayPort) && !drm_->mode_verify(m))
      continue;

    m.set_id(drm_->next_mode_id());
    new_modes.push_back(m);
  }
  modes_.swap(new_modes);

  //Get original mode from connector
  std::vector<DrmMode> new_raw_modes;
  for (int i = 0; i < c->count_modes; ++i) {
    bool exists = false;
    for (const DrmMode &mode : modes_) {
      if (mode == c->modes[i]) {
        new_raw_modes.push_back(mode);
        exists = true;
        break;
      }
    }
    if (exists)
      continue;

    DrmMode m(&c->modes[i]);
    m.set_id(drm_->next_mode_id());
    new_raw_modes.push_back(m);
  }
  raw_modes_.swap(new_raw_modes);

  drmModeFreeConnector(c);

  return 0;
}

void DrmConnector::update_size(int w, int h) {
    mm_width_ = w;
    mm_height_ = h;
}

void DrmConnector::update_state(drmModeConnection state) {
    state_ = state;
}

const DrmMode &DrmConnector::active_mode() const {
    return active_mode_;
}

const DrmMode &DrmConnector::current_mode() const {
  return current_mode_;
}

void DrmConnector::SetDpmsMode(uint32_t dpms_mode) {
  int ret = drmModeConnectorSetProperty(drm_->fd(), id_, dpms_property_.id(), dpms_mode);
  if (ret) {
    ALOGE("Failed to set dpms mode %d %d", ret, dpms_mode);
    return;
  }
}

void DrmConnector::set_best_mode(const DrmMode &mode) {
  best_mode_ = mode;
}

void DrmConnector::set_active_mode(const DrmMode &mode) {
  active_mode_ = mode;
}

void DrmConnector::set_current_mode(const DrmMode &mode) {
  current_mode_ = mode;
}

const DrmProperty &DrmConnector::dpms_property() const {
  return dpms_property_;
}

const DrmProperty &DrmConnector::crtc_id_property() const {
  return crtc_id_property_;
}

const DrmProperty &DrmConnector::brightness_id_property() const {
  return brightness_id_property_;
}
const DrmProperty &DrmConnector::contrast_id_property() const {
  return contrast_id_property_;
}
const DrmProperty &DrmConnector::saturation_id_property() const {
  return saturation_id_property_;
}
const DrmProperty &DrmConnector::hue_id_property() const {
  return hue_id_property_;
}

const DrmProperty &DrmConnector::hdr_metadata_property() const {
  return hdr_metadata_property_;
}

const DrmProperty &DrmConnector::hdr_panel_property() const {
  return hdr_panel_property_;
}

const DrmProperty &DrmConnector::hdmi_output_colorimetry_property() const {
  return hdmi_output_colorimetry_;
}

const DrmProperty &DrmConnector::hdmi_output_format_property() const {
  return hdmi_output_format_;
}

const DrmProperty &DrmConnector::hdmi_output_depth_property() const {
  return hdmi_output_depth_;
}

DrmEncoder *DrmConnector::encoder() const {
  return encoder_;
}

void DrmConnector::set_encoder(DrmEncoder *encoder) {
  encoder_ = encoder;
}

void DrmConnector::force_disconnect(bool force) {
    force_disconnect_ = force;
}

drmModeConnection DrmConnector::state() const {
  if (force_disconnect_)
    return DRM_MODE_DISCONNECTED;
  return state_;
}

drmModeConnection DrmConnector::raw_state() const {
  return state_;
}

uint32_t DrmConnector::mm_width() const {
  return mm_width_;
}

uint32_t DrmConnector::mm_height() const {
  return mm_height_;
}

void DrmConnector::dump_connector(std::ostringstream *out) const {
    int j;

    *out << connector_->connector_id << "\t"
         << connector_->encoder_id << "\t"
         << drm_->connector_status_str(connector_->connection) << "\t"
         << drm_->connector_type_str(connector_->connector_type) << "\t"
         << connector_->mmWidth << "\t"
         << connector_->mmHeight << "\t"
         << connector_->count_modes << "\t";

		for (j = 0; j < connector_->count_encoders; j++) {
			if(j > 0) {
                *out << ", ";
			} else {
			    *out << "";
			}
			*out << connector_->encoders[j];
	    }
		*out << "\n";

		if (connector_->count_modes) {
			*out << "  modes:\n";
			*out << "\tname refresh (Hz) hdisp hss hse htot vdisp "
			       "vss vse vtot)\n";
			for (j = 0; j < connector_->count_modes; j++)
				drm_->dump_mode(&connector_->modes[j],out);
		}

		drm_->DumpConnectorProperty(*this,out);
}

}
