/*
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

#include <log/log.h>
#include <xf86drmMode.h>

namespace android {

DrmConnector::DrmConnector(DrmResources *drm, drmModeConnectorPtr c,
                           DrmEncoder *current_encoder,
                           std::vector<DrmEncoder *> &possible_encoders)
    : drm_(drm),
      id_(c->connector_id),
      encoder_(current_encoder),
      display_(-1),
      type_(c->connector_type),
      state_(c->connection),
      force_disconnect_(false),
      mm_width_(c->mmWidth),
      mm_height_(c->mmHeight),
      possible_encoders_(possible_encoders),
      possible_displays_(0),
      connector_(c) {
}

int DrmConnector::Init() {
  int ret = drm_->GetConnectorProperty(*this, "DPMS", &dpms_property_);
  if (ret) {
    ALOGE("Could not get DPMS property\n");
    return ret;
  }
  ret = drm_->GetConnectorProperty(*this, "CRTC_ID", &crtc_id_property_);
  ALOGD("crtc_id_property_: name %s ", crtc_id_property_.name().c_str());
  if (ret) {
    ALOGE("Could not get CRTC_ID property\n");
    return ret;
  }

  ret = drm_->GetConnectorProperty(*this, "hdmi_output_mode_capacity", &hdmi_output_mode_capacity_);
  if (ret)
    ALOGW("Could not get hdmi_output_colorimetry property\n");

  ret = drm_->GetConnectorProperty(*this, "hdmi_color_depth_capacity", &hdmi_color_depth_capacity_);
  if (ret) {
    ALOGW("Could not get hdmi_output_format property\n");
  }

  ret = drm_->GetConnectorProperty(*this, "CONNECTOR_ID", &connector_id_);
  if (ret) {
    ALOGW("Could not get CONNECTOR_ID property\n");
  }

  return 0;
}

uint32_t DrmConnector::id() const {
  return id_;
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
         type_ == DRM_MODE_CONNECTOR_DSI || type_ == DRM_MODE_CONNECTOR_VIRTUAL ||
         type_ == DRM_MODE_CONNECTOR_TV;
}


const DrmMode &DrmConnector::best_mode() const {
  return best_mode_;
}

int DrmConnector::UpdateModes() {
  int fd = drm_->fd();
  int ret;

  drmModeConnectorPtr c = drmModeGetConnector(fd, id_);
  if (!c) {
    ALOGE("Failed to get connector %d", id_);
    return -ENODEV;
  }

  state_ = c->connection;
  if (!c->count_modes)
    state_ = DRM_MODE_DISCONNECTED;

  std::vector<DrmMode> new_modes;
  for (int i = 0; i < c->count_modes; ++i) {
    bool exists = false;
    for (const DrmMode &mode : modes_) {
      if (mode == c->modes[i]) {
        new_modes.push_back(mode);
        exists = true;
        break;
      }
    }
    if (exists)
      continue;

    DrmMode m(&c->modes[i]);

    m.set_id(drm_->next_mode_id());
    new_modes.push_back(m);
  }
  modes_.swap(new_modes);

  ret = drm_->GetConnectorProperty(*this, "hdmi_output_mode_capacity", &hdmi_output_mode_capacity_);
  if (ret)
    ALOGW("UpdateModes Could not get hdmi_output_colorimetry property\n");

  ret = drm_->GetConnectorProperty(*this, "hdmi_color_depth_capacity", &hdmi_color_depth_capacity_);
  if (ret) {
    ALOGW("UpdateModes Could not get hdmi_output_format property\n");
  }
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

uint32_t DrmConnector::connector_id() {
  uint64_t id=0;
  connector_id_.value(&id);
  return (uint32_t)id;
}

const DrmProperty &DrmConnector::hdmi_output_mode_capacity_property() const {
  return hdmi_output_mode_capacity_;
}

const DrmProperty &DrmConnector::hdmi_output_depth_capacity_property() const {
  return hdmi_color_depth_capacity_;
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
