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
#include "autofd.h"

#include <sstream>
#include <stdint.h>
#if RK_RGA
#include <RockchipRga.h>
#endif

namespace android {
  enum LOG_LEVEL
  {
      //Log level flag
      /*1*/
      DBG_VERBOSE = 1 << 0,
      /*2*/
      DBG_DEBUG = 1 << 1,
      /*4*/
      DBG_INFO = 1 << 2,
      /*8*/
      DBG_WARN = 1 << 3,
      /*16*/
      DBG_ERROR = 1 << 4,
      /*32*/
      DBG_FETAL = 1 << 5,
      /*64*/
      DBG_SILENT = 1 << 6,
  };

  enum {
      HWC_DISPLAY_PRIMARY     = 0,
      HWC_DISPLAY_EXTERNAL    = 1,    // HDMI, DP, etc.
      HWC_DISPLAY_VIRTUAL     = 2,
  
      HWC_NUM_PHYSICAL_DISPLAY_TYPES = 2,
      HWC_NUM_DISPLAY_TYPES          = 3,
  };

  enum {
      HWC_DISPLAY_PRIMARY_BIT     = 1 << HWC_DISPLAY_PRIMARY,
      HWC_DISPLAY_EXTERNAL_BIT    = 1 << HWC_DISPLAY_EXTERNAL,
      HWC_DISPLAY_VIRTUAL_BIT     = 1 << HWC_DISPLAY_VIRTUAL,
  };



#define type_name_define(res) \
const char * res##_str(int type);

class DrmResources {
 public:
  DrmResources();
  ~DrmResources();

  int Init();

  int fd() const {
    return fd_.get();
  }

  const std::vector<std::unique_ptr<DrmConnector>> &connectors() const {
    return connectors_;
  }


  bool mode_verify(const DrmMode &mode);
  void DisplayChanged(void);
  void SetPrimaryDisplay(DrmConnector *c);
  void SetExtendDisplay(DrmConnector *c);

  DrmCrtc *GetCrtcFromConnector(DrmConnector *conn) const;
  DrmConnector *GetConnectorFromType(int display_type) const;
  int GetCrtcProperty(const DrmCrtc &crtc, const char *prop_name,
                      DrmProperty *property);
  int GetConnectorProperty(const DrmConnector &connector, const char *prop_name,
                           DrmProperty *property);

  uint32_t next_mode_id();
  int SetDisplayActiveMode(int display, const DrmMode &mode);
  int SetDpmsMode(int display, uint64_t mode);
  int UpdateDisplayRoute(void);
  void ClearDisplay(void);

  int CreatePropertyBlob(void *data, size_t length, uint32_t *blob_id);
  int DestroyPropertyBlob(uint32_t blob_id);
  type_name_define(encoder_type);
  type_name_define(connector_status);
  type_name_define(connector_type);
  
  int DumpCrtcProperty(const DrmCrtc &crtc, std::ostringstream *out);
  int DumpConnectorProperty(const DrmConnector &connector, std::ostringstream *out);
  void dump_mode(drmModeModeInfo *mode,std::ostringstream *out);
  bool log_level(LOG_LEVEL log_level)
  {
      return g_log_level & log_level;
  }

#if RK_RGA
  bool isSupportRkRga() {
	RockchipRga& rkRga(RockchipRga::get());
	return rkRga.RkRgaIsReady();
  }
#endif

 private:
  void ConfigurePossibleDisplays();
  int TryEncoderForDisplay(int display, DrmEncoder *enc);
  int GetProperty(uint32_t obj_id, uint32_t obj_type, const char *prop_name,
                  DrmProperty *property);

  void dump_blob(uint32_t blob_id, std::ostringstream *out);
  void dump_prop(drmModePropertyPtr prop,
                     uint32_t prop_id, uint64_t value, std::ostringstream *out);
  int DumpProperty(uint32_t obj_id, uint32_t obj_type, std::ostringstream *out);

  UniqueFd fd_;
  uint32_t mode_id_ = 0;
  bool enable_changed_;
  DrmConnector *primary_;
  DrmConnector *extend_;

  std::vector<std::unique_ptr<DrmConnector>> connectors_;
  std::vector<std::unique_ptr<DrmEncoder>> encoders_;
  std::vector<std::unique_ptr<DrmCrtc>> crtcs_;

  unsigned int g_log_level;
  
  enum {
      HWC_DISPLAY_PRIMARY     = 0,
      HWC_DISPLAY_EXTERNAL    = 1,    // HDMI, DP, etc.
      HWC_DISPLAY_VIRTUAL     = 2,
  
      HWC_NUM_PHYSICAL_DISPLAY_TYPES = 2,
      HWC_NUM_DISPLAY_TYPES          = 3,
  };

};
}

#endif  // ANDROID_DRM_H_
