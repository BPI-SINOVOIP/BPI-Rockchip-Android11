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

#define LOG_TAG "hwc-drm-crtc"

#include "drmcrtc.h"
#include "drmresources.h"

#include <stdint.h>
#include <xf86drmMode.h>

#include <log/log.h>

namespace android {

DrmCrtc::DrmCrtc(DrmResources *drm, drmModeCrtcPtr c, unsigned pipe)
    : drm_(drm),
      id_(c->crtc_id),
      pipe_(pipe),
      display_(-1),
      x_(c->x),
      y_(c->y),
      width_(c->width),
      height_(c->height),
      b_afbc_(false),
      mode_(&c->mode),
      mode_valid_(c->mode_valid),
      crtc_(c) {
}

int DrmCrtc::Init() {
  int ret = drm_->GetCrtcProperty(*this, "ACTIVE", &active_property_);
  if (ret) {
    ALOGE("Failed to get ACTIVE property");
    return ret;
  }

  ret = drm_->GetCrtcProperty(*this, "MODE_ID", &mode_property_);
  if (ret) {
    ALOGE("Failed to get MODE_ID property");
    return ret;
  }

  ret = drm_->GetCrtcProperty(*this, "FEATURE", &feature_property_);
  if (ret)
    ALOGE("Could not get FEATURE property");

    uint64_t feature=0;
    feature_property_.set_feature("afbdc");
    feature_property_.value(&feature);
    b_afbc_ = (feature ==1)?true:false;

  can_overscan_ = true;
  ret = drm_->GetCrtcProperty(*this, "left margin", &left_margin_property_);
  if (ret) {
    ALOGE("Failed to get left margin property");
    can_overscan_ = false;
  }
  ret = drm_->GetCrtcProperty(*this, "right margin", &right_margin_property_);
  if (ret) {
    ALOGE("Failed to get right margin property");
    can_overscan_ = false;
  }
  ret = drm_->GetCrtcProperty(*this, "top margin", &top_margin_property_);
  if (ret) {
    ALOGE("Failed to get top margin property");
    can_overscan_ = false;
  }
  ret = drm_->GetCrtcProperty(*this, "bottom margin", &bottom_margin_property_);
  if (ret) {
    ALOGE("Failed to get bottom margin property");
    can_overscan_ = false;
  }

  return 0;
}

bool DrmCrtc::get_afbc() const {
    return b_afbc_;
}

uint32_t DrmCrtc::id() const {
  return id_;
}

unsigned DrmCrtc::pipe() const {
  return pipe_;
}

bool DrmCrtc::can_overscan() const {
  return can_overscan_;
}

const DrmProperty &DrmCrtc::active_property() const {
  return active_property_;
}

const DrmProperty &DrmCrtc::mode_property() const {
  return mode_property_;
}

const DrmProperty &DrmCrtc::left_margin_property() const {
  return left_margin_property_;
}

const DrmProperty &DrmCrtc::right_margin_property() const {
  return right_margin_property_;
}

const DrmProperty &DrmCrtc::top_margin_property() const {
  return top_margin_property_;
}

const DrmProperty &DrmCrtc::bottom_margin_property() const {
  return bottom_margin_property_;
}

void DrmCrtc::dump_crtc(std::ostringstream *out) const
{

	*out << crtc_->crtc_id << "\t"
	     << crtc_->buffer_id << "\t"
	     << "(" << crtc_->x << "," << crtc_->y << ")\t("
	     << crtc_->width << "x" << crtc_->height << ")\n";

	drm_->dump_mode(&crtc_->mode, out);

	drm_->DumpCrtcProperty(*this,out);
}

}
