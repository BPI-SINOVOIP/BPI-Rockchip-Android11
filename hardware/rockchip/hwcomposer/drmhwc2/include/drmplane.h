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

#ifndef ANDROID_DRM_PLANE_H_
#define ANDROID_DRM_PLANE_H_

#include "drmcrtc.h"
#include "drmproperty.h"

#include <stdint.h>
#include <xf86drmMode.h>
#include <vector>
#include <set>

namespace android {

// Rk3588
enum DrmPlaneTypeRK3588{
      // Cluster 0
      PLANE_RK3588_CLUSTER0_WIN0 = 1 << 0,
      PLANE_RK3588_CLUSTER0_WIN1 = 1 << 1,
      // Cluster 1
      PLANE_RK3588_CLUSTER1_WIN0 = 1 << 2,
      PLANE_RK3588_CLUSTER1_WIN1 = 1 << 3,
      // Cluster 2
      PLANE_RK3588_CLUSTER2_WIN0 = 1 << 4,
      PLANE_RK3588_CLUSTER2_WIN1 = 1 << 5,
      // Cluster 3
      PLANE_RK3588_CLUSTER3_WIN0 = 1 << 6,
      PLANE_RK3588_CLUSTER3_WIN1 = 1 << 7,
      // Esmart 0
      PLANE_RK3588_ESMART0_WIN0 = 1 << 8,
      PLANE_RK3588_ESMART0_WIN1 = 1 << 9,
      PLANE_RK3588_ESMART0_WIN2 = 1 << 10,
      PLANE_RK3588_ESMART0_WIN3 = 1 << 11,
      // Esmart 1
      PLANE_RK3588_ESMART1_WIN0 = 1 << 12,
      PLANE_RK3588_ESMART1_WIN1 = 1 << 13,
      PLANE_RK3588_ESMART1_WIN2 = 1 << 14,
      PLANE_RK3588_ESMART1_WIN3 = 1 << 15,
      // Esmart 2
      PLANE_RK3588_ESMART2_WIN0 = 1 << 16,
      PLANE_RK3588_ESMART2_WIN1 = 1 << 17,
      PLANE_RK3588_ESMART2_WIN2 = 1 << 18,
      PLANE_RK3588_ESMART2_WIN3 = 1 << 19,
      // Esmart 3
      PLANE_RK3588_ESMART3_WIN0 = 1 << 20,
      PLANE_RK3588_ESMART3_WIN1 = 1 << 21,
      PLANE_RK3588_ESMART3_WIN2 = 1 << 22,
      PLANE_RK3588_ESMART3_WIN3 = 1 << 23,
      // Cluster mask
      PLANE_RK3588_ALL_CLUSTER0_MASK= 0x3,
      PLANE_RK3588_ALL_CLUSTER1_MASK= 0xc,
      PLANE_RK3588_ALL_CLUSTER2_MASK= 0x30,
      PLANE_RK3588_ALL_CLUSTER3_MASK= 0xc0,

      PLANE_RK3588_ALL_CLUSTER_MASK = 0xff,
      // Esmart mask
      PLANE_RK3588_ALL_ESMART0_MASK = 0xf00,
      PLANE_RK3588_ALL_ESMART1_MASK = 0xf000,
      PLANE_RK3588_ALL_ESMART2_MASK = 0xf0000,
      PLANE_RK3588_ALL_ESMART3_MASK = 0xf00000,
      PLANE_RK3588_ALL_ESMART_MASK = 0xffff00,
      PLANE_RK3588_Unknown      = 0xffffffff,
};

// Rk356x
enum DrmPlaneTypeRK356x{
      DRM_PLANE_TYPE_CLUSTER0_WIN0 = 1 << 0,
      DRM_PLANE_TYPE_CLUSTER0_WIN1 = 1 << 1,

      DRM_PLANE_TYPE_CLUSTER1_WIN0 = 1 << 2,
      DRM_PLANE_TYPE_CLUSTER1_WIN1 = 1 << 3,

      DRM_PLANE_TYPE_ESMART0_WIN0 = 1 << 4,
      DRM_PLANE_TYPE_ESMART0_WIN1 = 1 << 5,
      DRM_PLANE_TYPE_ESMART0_WIN2 = 1 << 6,
      DRM_PLANE_TYPE_ESMART0_WIN3 = 1 << 7,

      DRM_PLANE_TYPE_ESMART1_WIN0 = 1 << 8,
      DRM_PLANE_TYPE_ESMART1_WIN1 = 1 << 9,
      DRM_PLANE_TYPE_ESMART1_WIN2 = 1 << 10,
      DRM_PLANE_TYPE_ESMART1_WIN3 = 1 << 11,

      DRM_PLANE_TYPE_SMART0_WIN0 = 1 << 12,
      DRM_PLANE_TYPE_SMART0_WIN1 = 1 << 13,
      DRM_PLANE_TYPE_SMART0_WIN2 = 1 << 14,
      DRM_PLANE_TYPE_SMART0_WIN3 = 1 << 15,

      DRM_PLANE_TYPE_SMART1_WIN0 = 1 << 16,
      DRM_PLANE_TYPE_SMART1_WIN1 = 1 << 17,
      DRM_PLANE_TYPE_SMART1_WIN2 = 1 << 18,
      DRM_PLANE_TYPE_SMART1_WIN3 = 1 << 19,

      DRM_PLANE_TYPE_CLUSTER0_MASK= 0x3,
      DRM_PLANE_TYPE_CLUSTER1_MASK= 0xc,
      DRM_PLANE_TYPE_CLUSTER_MASK = 0xf,
      DRM_PLANE_TYPE_ESMART0_MASK = 0xf0,
      DRM_PLANE_TYPE_ESMART1_MASK = 0xf00,
      DRM_PLANE_TYPE_SMART0_MASK  = 0xf000,
      DRM_PLANE_TYPE_SMART1_MASK  = 0xf0000,
      DRM_PLANE_TYPE_VOP2_Unknown      = 0xffffffff,
};

// RK3399/Rk3288/RK3328/RK3128
enum DrmPlaneTypeRK3399{
      DRM_PLANE_TYPE_VOP0_WIN0   = 1 << 0,
      DRM_PLANE_TYPE_VOP0_WIN1   = 1 << 1,

      DRM_PLANE_TYPE_VOP0_WIN2_0 = 1 << 2,
      DRM_PLANE_TYPE_VOP0_WIN2_1 = 1 << 3,
      DRM_PLANE_TYPE_VOP0_WIN2_2 = 1 << 4,
      DRM_PLANE_TYPE_VOP0_WIN2_3 = 1 << 5,

      DRM_PLANE_TYPE_VOP0_WIN3_0 = 1 << 6,
      DRM_PLANE_TYPE_VOP0_WIN3_1 = 1 << 7,
      DRM_PLANE_TYPE_VOP0_WIN3_2 = 1 << 8,
      DRM_PLANE_TYPE_VOP0_WIN3_3 = 1 << 9,

      DRM_PLANE_TYPE_VOP1_WIN0   = 1 << 10,

      DRM_PLANE_TYPE_VOP1_WIN2_0 = 1 << 11,
      DRM_PLANE_TYPE_VOP1_WIN2_1 = 1 << 12,
      DRM_PLANE_TYPE_VOP1_WIN2_2 = 1 << 13,
      DRM_PLANE_TYPE_VOP1_WIN2_3 = 1 << 14,

      DRM_PLANE_TYPE_VOP0_MASK   = 0x3ff,
      DRM_PLANE_TYPE_VOP1_MASK   = 0x7c,
      DRM_PLANE_TYPE_VOP1_Unknown      = 0xffffffff,
};

enum DrmPlaneRotationType{
      DRM_PLANE_ROTATION_0 = 1 << 0,
      DRM_PLANE_ROTATION_90 = 1 << 1,
      DRM_PLANE_ROTATION_270 = 1 << 2,
      DRM_PLANE_ROTATION_X_MIRROR = 1 << 3,
      DRM_PLANE_ROTATION_Y_MIRROR = 1 << 4,
      DRM_PLANE_ROTATION_Unknown = 0xff,
};

struct plane_rotation_type_name {
  DrmPlaneRotationType type;
  const char *name;
};


enum DrmPlaneFeatureType{
      DRM_PLANE_FEARURE_SCALE   = 0,
      DRM_PLANE_FEARURE_ALPHA   = 1,
      DRM_PLANE_FEARURE_HDR2SDR = 2,
      DRM_PLANE_FEARURE_SDR2HDR = 3,
      DRM_PLANE_FEARURE_AFBDC   = 4,
};

enum DrmPlaneFeatureTypeBit{
      DRM_PLANE_FEARURE_BIT_SCALE   = 1 << DRM_PLANE_FEARURE_SCALE,
      DRM_PLANE_FEARURE_BIT_ALPHA   = 1 << DRM_PLANE_FEARURE_ALPHA,
      DRM_PLANE_FEARURE_BIT_HDR2SDR = 1 << DRM_PLANE_FEARURE_HDR2SDR,
      DRM_PLANE_FEARURE_BIT_SDR2HDR = 1 << DRM_PLANE_FEARURE_SDR2HDR,
      DRM_PLANE_FEARURE_BIT_AFBDC   = 1 << DRM_PLANE_FEARURE_AFBDC,
};

class DrmDevice;

class DrmPlane {
 public:
  DrmPlane(DrmDevice *drm, drmModePlanePtr p, int soc_id);
  DrmPlane(const DrmPlane &) = delete;
  DrmPlane &operator=(const DrmPlane &) = delete;

  int Init();

  uint32_t id() const;

  bool GetCrtcSupported(const DrmCrtc &crtc) const;

  uint32_t type() const;

  uint64_t win_type() const;
  const char* name() const;
  void mark_type_by_name();
  const DrmProperty &crtc_property() const;
  const DrmProperty &fb_property() const;
  const DrmProperty &crtc_x_property() const;
  const DrmProperty &crtc_y_property() const;
  const DrmProperty &crtc_w_property() const;
  const DrmProperty &crtc_h_property() const;
  const DrmProperty &src_x_property() const;
  const DrmProperty &src_y_property() const;
  const DrmProperty &src_w_property() const;
  const DrmProperty &src_h_property() const;
  const DrmProperty &zpos_property() const;
  const DrmProperty &rotation_property() const;
  const DrmProperty &alpha_property() const;
  const DrmProperty &eotf_property() const;
  const DrmProperty &blend_property() const;
  const DrmProperty &colorspace_property() const;
  const DrmProperty &area_id_property() const;
  const DrmProperty &share_id_property() const;
  const DrmProperty &feature_property() const;
  const DrmProperty &name_property() const;
  const DrmProperty &input_w_property() const;
  const DrmProperty &input_h_property() const;
  const DrmProperty &output_w_property() const;
  const DrmProperty &output_h_property() const;
  const DrmProperty &scale_rate_property() const;
  bool is_use();
  void set_use(bool b_use);
  bool get_scale();
  bool get_rotate();
  bool get_hdr2sdr();
  bool get_sdr2hdr();
  bool get_afbc();
  bool get_yuv();
  int get_input_w_max();
  int get_input_h_max();
  int get_output_w_max();
  int get_output_h_max();
  int get_transform();
  void set_yuv(bool b_yuv);
  bool is_reserved();
  void set_reserved(bool bReserved);
  bool is_support_scale(float scale_rate);
  bool is_support_input(int input_w, int input_h);
  bool is_support_output(int output_w, int output_h);
  bool is_support_format(uint32_t format, bool afbcd);
  bool is_support_transform(int transform);
  inline uint32_t get_possible_crtc_mask() const{ return possible_crtc_mask_; }
  inline void set_current_crtc_bit(uint32_t current_crtc) { current_crtc_ = current_crtc;}
  inline uint32_t get_current_crtc_bit() const{ return current_crtc_; }

  // 8K
  int get_input_w_max_8k();
  int get_input_h_max_8k();
  int get_output_w_max_8k();
  int get_output_h_max_8k();
  bool is_support_input_8k(int input_w, int input_h);
  bool is_support_output_8k(int output_w, int output_h);
  int  get_transform_8k();
  bool is_support_transform_8k(int transform);
  bool is_support_scale_8k(float scale_rate);


 private:
  DrmDevice *drm_;
  uint32_t id_;

  uint32_t possible_crtc_mask_;
  uint32_t current_crtc_;

  uint32_t type_;

  DrmProperty crtc_property_;
  DrmProperty fb_property_;
  DrmProperty crtc_x_property_;
  DrmProperty crtc_y_property_;
  DrmProperty crtc_w_property_;
  DrmProperty crtc_h_property_;
  DrmProperty src_x_property_;
  DrmProperty src_y_property_;
  DrmProperty src_w_property_;
  DrmProperty src_h_property_;
  DrmProperty rotation_property_;
  DrmProperty alpha_property_;
  DrmProperty blend_mode_property_;
  DrmProperty zpos_property_;
  //RK support
  DrmProperty eotf_property_;
  DrmProperty colorspace_property_;
  DrmProperty area_id_property_;
  DrmProperty share_id_property_;
  DrmProperty feature_property_;
  DrmProperty name_property_;
  DrmProperty input_w_property_;
  DrmProperty input_h_property_;
  DrmProperty output_w_property_;
  DrmProperty output_h_property_;
  DrmProperty scale_rate_property_;

  bool bReserved_;
  bool b_use_;
  bool b_yuv_;
  bool b_scale_;
  bool b_alpha_;
  bool b_hdr2sdr_;
  bool b_sdr2hdr_;
  bool b_afbdc_;
  bool b_afbc_prop_;
  uint64_t win_type_;
  const char *name_;
  uint32_t rotate_=0;
  int input_w_max_;
  int input_h_max_;
  int output_w_max_;
  int output_h_max_;
  float scale_min_=0.0;
  float scale_max_=0.0;

  std::set<uint32_t> support_format_list;
  drmModePlanePtr plane_;
  int soc_id_;
};
}  // namespace android

#endif  // ANDROID_DRM_PLANE_H_
