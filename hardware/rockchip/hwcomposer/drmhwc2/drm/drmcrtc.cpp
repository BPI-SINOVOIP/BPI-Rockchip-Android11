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
#include "drmdevice.h"
#include "rockchip/utils/drmdebug.h"

#include <stdint.h>
#include <xf86drmMode.h>

#include <log/log.h>

namespace android {

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct plane_mask_name {
  uint64_t mask;
  const char *name;
};

// RK3588
struct plane_mask_name plane_mask_names_rk3588[] = {
  { PLANE_RK3588_ALL_CLUSTER0_MASK, "Cluster0" },
  { PLANE_RK3588_ALL_CLUSTER1_MASK, "Cluster1" },
  { PLANE_RK3588_ALL_CLUSTER2_MASK, "Cluster2" },
  { PLANE_RK3588_ALL_CLUSTER3_MASK, "Cluster3" },
  { PLANE_RK3588_ALL_ESMART0_MASK, "Esmart0" },
  { PLANE_RK3588_ALL_ESMART1_MASK, "Esmart1" },
  { PLANE_RK3588_ALL_ESMART2_MASK, "Esmart2" },
  { PLANE_RK3588_ALL_ESMART3_MASK, "Esmart3" },
  { PLANE_RK3588_Unknown, "unknown" },
};

// RK356x
struct plane_mask_name plane_mask_names_rk356x[] = {
  { DRM_PLANE_TYPE_CLUSTER0_MASK, "Cluster0" },
  { DRM_PLANE_TYPE_CLUSTER1_MASK, "Cluster1" },
  { DRM_PLANE_TYPE_ESMART0_MASK, "Esmart0" },
  { DRM_PLANE_TYPE_ESMART1_MASK, "Esmart1" },
  { DRM_PLANE_TYPE_SMART0_MASK, "Smart0" },
  { DRM_PLANE_TYPE_SMART1_MASK, "Smart1" },
  { DRM_PLANE_TYPE_VOP2_Unknown, "unknown" },
};

// RK3399
struct plane_mask_name plane_mask_names_rk3399[] = {
  { DRM_PLANE_TYPE_VOP0_MASK, "VOP0-win0" },
  { DRM_PLANE_TYPE_VOP0_MASK, "VOP0-win1" },
  { DRM_PLANE_TYPE_VOP0_MASK, "VOP0-win2" },
  { DRM_PLANE_TYPE_VOP0_MASK, "VOP0-win3" },
  { DRM_PLANE_TYPE_VOP1_MASK, "VOP1-win0" },
  { DRM_PLANE_TYPE_VOP1_MASK, "VOP1-win1" },
  { DRM_PLANE_TYPE_VOP1_Unknown, "unknown" },
};

DrmCrtc::DrmCrtc(DrmDevice *drm, drmModeCrtcPtr c, unsigned pipe)
    : drm_(drm),
      drm_version_(drm->getDrmVersion()),
      id_(c->crtc_id),
      pipe_(pipe),
      display_(-1),
      mode_(&c->mode) {
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

  b_can_overscan_ = true;
  ret = drm_->GetCrtcProperty(*this, "left margin", &left_margin_property_);
  if (ret) {
    ALOGE("Failed to get left margin property");
    b_can_overscan_ = false;
  }
  ret = drm_->GetCrtcProperty(*this, "right margin", &right_margin_property_);
  if (ret) {
    ALOGE("Failed to get right margin property");
    b_can_overscan_ = false;
  }
  ret = drm_->GetCrtcProperty(*this, "top margin", &top_margin_property_);
  if (ret) {
    ALOGE("Failed to get top margin property");
    b_can_overscan_ = false;
  }
  ret = drm_->GetCrtcProperty(*this, "bottom margin", &bottom_margin_property_);
  if (ret) {
    ALOGE("Failed to get bottom margin property");
    b_can_overscan_ = false;
  }

  ret = drm_->GetCrtcProperty(*this, "OUT_FENCE_PTR", &out_fence_ptr_property_);
  if (ret) {
    ALOGE("Failed to get OUT_FENCE_PTR property");
  }

  ret = drm_->GetCrtcProperty(*this, "SOC_ID", &soc_type_property_);
  if (ret) {
    ALOGE("Failed to get SOC_ID property");
  }
  std::tie(ret, soc_id_) = soc_type_property_.value();
  if(ret)
    ALOGE("Failed to get SOC_ID value");

  ret = drm_->GetCrtcProperty(*this, "PORT_ID", &port_id_property_);
  if (ret) {
    ALOGE("Failed to get PORT_ID property");
  }
  std::tie(ret, port_id_) = port_id_property_.value();
  if(ret)
    ALOGE("Failed to get PORT_ID value");

  ret = drm_->GetCrtcProperty(*this, "ACLK", &aclk_property_);
  if (ret) {
    ALOGE("Failed to get ACLK property");
    aclk_=0;
  }else{
    std::tie(ret, aclk_) = aclk_property_.value();
    if(ret){
      ALOGE("Failed to get ACLK value");
    }
  }

  // Plane mask
  plane_mask_=0;
  ret = drm_->GetCrtcProperty(*this, "PLANE_MASK", &plane_mask_property_);
  if (ret) {
    ALOGE("Failed to get plane_mask property");
  }else{

    if(isRK3588(soc_id_)){
      for(int i = 0; i < ARRAY_SIZE(plane_mask_names_rk3588); i++){
        bool have_mask = false;
        std::tie(ret,have_mask) = plane_mask_property_.value_bitmask(plane_mask_names_rk3588[i].name);
        if(have_mask){
          plane_mask_ |= plane_mask_names_rk3588[i].mask;
        }
      }
    }else if(isRK356x(soc_id_)){
      for(int i = 0; i < ARRAY_SIZE(plane_mask_names_rk356x); i++){
        bool have_mask = false;
        std::tie(ret,have_mask) = plane_mask_property_.value_bitmask(plane_mask_names_rk356x[i].name);
        if(have_mask){
          plane_mask_ |= plane_mask_names_rk356x[i].mask;
        }
      }
    }else if(isRK3399(soc_id_)){
      for(int i = 0; i < ARRAY_SIZE(plane_mask_names_rk3399); i++){
        bool have_mask = false;
        std::tie(ret,have_mask) = plane_mask_property_.value_bitmask(plane_mask_names_rk3399[i].name);
        if(have_mask){
          plane_mask_ |= plane_mask_names_rk3399[i].mask;
        }
      }
    }
  }

  // GAMMA LUT
  ret = drm_->GetCrtcProperty(*this, "GAMMA_LUT", &gamma_lut_property_);
  if (ret) {
    ALOGE("Failed to get GAMMA_LUT property");
  }

  ret = drm_->GetCrtcProperty(*this, "GAMMA_LUT_SIZE", &gamma_lut_size_property_);
  if (ret) {
    ALOGE("Failed to get GAMMA_LUT_SIZE property");
  }

  // CUBIC LUT
  ret = drm_->GetCrtcProperty(*this, "CUBIC_LUT", &cubic_lut_property_);
  if (ret) {
    ALOGE("Failed to get CUBIC_LUT property");
  }

  ret = drm_->GetCrtcProperty(*this, "CUBIC_LUT_SIZE", &cubic_lut_size_property_);
  if (ret) {
    ALOGE("Failed to get CUBIC_LUT_SIZE property");
  }

  if(isDrmVerison44(drm_version_)){
    // Nothing.
  }else if(isDrmVerison419(drm_version_)){
    // ALPHA_SCALE
    uint64_t alpha_scale = 0;
    b_can_alpha_scale_ = true;
    ret = drm_->GetCrtcProperty(*this, "ALPHA_SCALE", &alpha_scale_property_);
    if (ret) {
      ALOGE("Failed to get alpha_scale_property property");
    }
    std::tie(alpha_scale, ret) = alpha_scale_property_.value();
    if(alpha_scale == 0)
      b_can_alpha_scale_ = false;

    // FEATURE: afbc
    ret = drm_->GetCrtcProperty(*this, "FEATURE", &feature_property_);
    if (ret)
      ALOGE("Could not get FEATURE property");

    uint64_t feature=0;
    b_can_afbc_ = false;
    feature_property_.set_feature("afbdc");
    std::tie(ret,feature) = feature_property_.value();
    b_can_afbc_ = (feature ==1)?true:false;

    // Workround: rk356x
    if(isRK356x(soc_id_)){
      b_can_alpha_scale_ = true;
      b_can_hdr10_ = false;
      b_can_next_hdr_ = false;
      if(port_id_ == 0){
        b_can_hdr10_ = true;
      }
    }
  }else if(isDrmVerison510(drm_version_)){

    // FEATURE: alpha_scale / HDR10 / Next_HDR
    ret = drm_->GetCrtcProperty(*this, "FEATURE", &feature_property_);
    if (ret)
      ALOGE("Could not get FEATURE property");

    b_can_alpha_scale_ = false;
    b_can_hdr10_ = false;
    b_can_next_hdr_ = false;
    std::tie(ret,b_can_alpha_scale_) = feature_property_.value_bitmask("ALPHA_SCALE");
    std::tie(ret,b_can_hdr10_) = feature_property_.value_bitmask("HDR10");
    std::tie(ret,b_can_next_hdr_) = feature_property_.value_bitmask("NEXT_HDR");
  }

  HWC2_ALOGD_IF_DEBUG("crtc-id=%d b_can_alpha_scale_=%d b_can_hdr10_=%d b_can_next_hdr_=%d",
    id_, b_can_alpha_scale_, b_can_hdr10_, b_can_next_hdr_);
  return 0;
}

uint32_t DrmCrtc::id() const {
  return id_;
}

unsigned DrmCrtc::pipe() const {
  return pipe_;
}

int DrmCrtc::display() const {
  return display_;
}

void DrmCrtc::set_display(int display) {
  display_ = display;
}

bool DrmCrtc::can_bind(int display) const {
  return display_ == -1 || display_ == display;
}

const DrmProperty &DrmCrtc::active_property() const {
  return active_property_;
}

const DrmProperty &DrmCrtc::mode_property() const {
  return mode_property_;
}
const DrmProperty &DrmCrtc::out_fence_ptr_property() const {
  return out_fence_ptr_property_;
}

bool DrmCrtc::can_overscan() const {
  return b_can_overscan_;
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

const DrmProperty &DrmCrtc::alpha_scale_property() const {
  return alpha_scale_property_;
}

const DrmProperty &DrmCrtc::gamma_lut_property() const{
  return gamma_lut_property_;
}

const DrmProperty &DrmCrtc::gamma_lut_size_property() const{
  return gamma_lut_size_property_;
}

const DrmProperty &DrmCrtc::cubic_lut_property() const{
  return cubic_lut_property_;
}

const DrmProperty &DrmCrtc::cubic_lut_size_property() const{
  return cubic_lut_size_property_;
}
bool DrmCrtc::get_afbc() const {
    return b_can_afbc_;
}

bool DrmCrtc::get_alpha_scale() const {
    return b_can_alpha_scale_;
}

bool DrmCrtc::get_hdr() const {
    return b_can_hdr10_;
}

bool DrmCrtc::get_next_hdr() const {
    return b_can_next_hdr_;
}

const DrmMode &DrmCrtc::kernel_mode() const {
  mode_.dump();
  return mode_;
}


}  // namespace android
