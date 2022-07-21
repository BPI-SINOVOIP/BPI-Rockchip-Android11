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

#ifndef ANDROID_DRM_H_
#define ANDROID_DRM_H_

#include "drmconnector.h"
#include "drmcrtc.h"
#include "drmencoder.h"
#include "drmeventlistener.h"
#include "drmplane.h"
#include "platform.h"
#include "rockchip/drmbaseparameter.h"
#include "rockchip/drmxml.h"

#include <stdint.h>
#include <tuple>

namespace android {

//you can define it in external/libdrm/include/drm/drm.h
#define DRM_CLIENT_CAP_SHARE_PLANES     6
#define DRM_CLIENT_CAP_ASPECT_RATIO     4

#define type_name_define(res) const char * res##_str(int type);

#define DRM_ATOMIC_ADD_PROP(object_id, prop_id, value) \
  if (prop_id) { \
    ret = drmModeAtomicAddProperty(pset, object_id, prop_id, value); \
    if (ret < 0) { \
      ALOGE("Failed to add prop[%d] to [%d]", prop_id, object_id); \
    } \
  }

class DrmDevice {
 public:
  DrmDevice();
  ~DrmDevice();

  std::tuple<int, int> Init(const char *path, int num_displays);
  void InitResevedPlane();
  int fd() const {
    return fd_.get();
  }

  const std::vector<std::unique_ptr<DrmConnector>> &connectors() const {
    return connectors_;
  }
  const std::vector<std::unique_ptr<DrmCrtc>> &crtc() const{
    return crtcs_;
  }
  const std::vector<std::unique_ptr<DrmPlane>> &planes() const {
    return planes_;
  }

  const std::vector<DrmPlane*> &sort_planes() const {
    return sort_planes_;
  }

  std::pair<uint32_t, uint32_t> min_resolution() const {
    return min_resolution_;
  }

  std::pair<uint32_t, uint32_t> max_resolution() const {
    return max_resolution_;
  }

  DrmConnector *GetConnectorForDisplay(int display) const;
  int GetTypeForConnector(DrmConnector *conn) const;
  DrmConnector *GetWritebackConnectorForDisplay(int display) const;
  DrmConnector *AvailableWritebackConnector(int display) const;
  DrmCrtc *GetCrtcForDisplay(int display) const;
  DrmPlane *GetPlane(uint32_t id) const;
  DrmEventListener *event_listener();

  int GetPlaneProperty(const DrmPlane &plane, const char *prop_name,
                       DrmProperty *property);
  int GetCrtcProperty(const DrmCrtc &crtc, const char *prop_name,
                      DrmProperty *property);
  int GetConnectorProperty(const DrmConnector &connector, const char *prop_name,
                           DrmProperty *property);

  const std::vector<std::unique_ptr<DrmCrtc>> &crtcs() const;
  uint32_t next_mode_id();

  int CreatePropertyBlob(void *data, size_t length, uint32_t *blob_id);
  int DestroyPropertyBlob(uint32_t blob_id);
  bool HandlesDisplay(int display) const;
  void RegisterHotplugHandler(DrmEventHandler *handler) {
    event_listener_.RegisterHotplugHandler(handler);
  }

  // RK support
  type_name_define(encoder_type);
  type_name_define(connector_status);
  type_name_define(connector_type);
  void SetCommitMirrorDisplayId(int display);
  int  GetCommitMirrorDisplayId() const;
  int UpdateDisplay3DLut(int display_id);
  int UpdateDisplayGamma(int display_id);
  int UpdateDisplayMode(int display_id);
  int BindDpyRes(int display_id);
  int ReleaseDpyRes(int display_id);
  void ClearDisplay(void);
  void ClearDisplay(int display);
  void ClearAllDisplay(void);
  int timeline(void);

  std::vector<PlaneGroup*> &GetPlaneGroups(){
    return plane_groups_;
  }

  int DumpPlaneProperty(const DrmPlane &plane, std::ostringstream *out);
  int DumpCrtcProperty(const DrmCrtc &crtc, std::ostringstream *out);
  int DumpConnectorProperty(const DrmConnector &connector, std::ostringstream *out);
  void DumpMode(drmModeModeInfo *mode,std::ostringstream *out);
  void DumpBlob(uint32_t blob_id, std::ostringstream *out);
  void DumpProp(drmModePropertyPtr prop,
                    uint32_t prop_id, uint64_t value, std::ostringstream *out);
  int DumpProperty(uint32_t obj_id, uint32_t obj_type, std::ostringstream *out);
  bool GetHdrPanelMetadata(DrmConnector *conn, struct hdr_static_metadata* blob_data);
  bool is_hdr_panel_support_st2084(DrmConnector *conn) const;
  bool is_hdr_panel_support_HLG(DrmConnector *conn) const;
  bool is_plane_support_hdr2sdr(DrmCrtc *conn) const;
  bool mode_verify(const DrmMode &mode);
  int getSocId(){ return soc_id_; };
  int getDrmVersion(){ return drm_version_;}
  int UpdateConnectorBaseInfo(unsigned int connector_type,unsigned int connector_id,struct disp_info *info);
  int DumpConnectorBaseInfo(unsigned int connector_type,unsigned int connector_id,struct disp_info *info);
  int SetScreenInfo(unsigned int connector_type,unsigned int connector_id, int index, struct screen_info *info);

  std::map<int, int> GetDisplays() { return displays_;}

 private:
  void init_white_modes(void);
  int InitEnvFromXml();
  int UpdateInfoFromXml();
  void ConfigurePossibleDisplays();
  int TryEncoderForDisplay(int display, DrmEncoder *enc);
  int GetProperty(uint32_t obj_id, uint32_t obj_type, const char *prop_name,
                  DrmProperty *property);

  int CreateDisplayPipe(DrmConnector *connector);
  int AttachWriteback(DrmConnector *display_conn);

  UniqueFd fd_;
  int soc_id_;
  // Kernel 4.19 = 2.0.0
  // Kernel 5.10 = 3.0.0
  int drm_version_;
  uint32_t mode_id_ = 0;
  bool enable_changed_;
  int hotplug_timeline;
  int prop_timeline_;
  int commit_mirror_display_id_=-1;

  std::vector<std::unique_ptr<DrmConnector>> connectors_;
  std::vector<std::unique_ptr<DrmConnector>> writeback_connectors_;
  std::vector<std::unique_ptr<DrmEncoder>> encoders_;
  std::vector<std::unique_ptr<DrmCrtc>> crtcs_;
  std::vector<std::unique_ptr<DrmPlane>> planes_;
  std::vector<DrmPlane*> sort_planes_;
  std::vector<PlaneGroup*> plane_groups_;
  DrmEventListener event_listener_;
  DrmBaseparameter baseparameter_;

  std::pair<uint32_t, uint32_t> min_resolution_;
  std::pair<uint32_t, uint32_t> max_resolution_;
  std::map<int, int> displays_;
  std::vector<DrmMode> white_modes_;
  struct DisplayModeXml DmXml_;
};
}  // namespace android

#endif  // ANDROID_DRM_H_
