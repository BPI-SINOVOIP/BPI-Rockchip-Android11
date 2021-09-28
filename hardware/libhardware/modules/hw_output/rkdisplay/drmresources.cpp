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

#define LOG_TAG "rkdisplay-resources"

#include "drmconnector.h"
#include "drmcrtc.h"
#include "drmencoder.h"
#include "drmresources.h"

#include <cinttypes>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <log/log.h>
#include <cutils/properties.h>
#include <drm_fourcc.h>

//you can define it in external/libdrm/include/drm/drm.h
#define DRM_CLIENT_CAP_SHARE_PLANES     4
#define DRM_CLIENT_CAP_ASPECT_RADIO     4

namespace android {

DrmResources::DrmResources()  {
  extend_ = NULL;
  primary_ = NULL;
}

DrmResources::~DrmResources() {
}

void DrmResources::ConfigurePossibleDisplays()
{
  char primary_name[PROPERTY_VALUE_MAX];
  char extend_name[PROPERTY_VALUE_MAX];
  int primary_length, extend_length;
  int default_display_possible = 0;
  std::string conn_name;

  primary_length = property_get("vendor.hwc.device.primary", primary_name, NULL);
  extend_length = property_get("vendor.hwc.device.extend", extend_name, NULL);

  if (!primary_length)
    default_display_possible |= HWC_DISPLAY_PRIMARY_BIT;
  if (!extend_length)
    default_display_possible |= HWC_DISPLAY_EXTERNAL_BIT;

  for (auto &conn : connectors_) {
    /*
     * build_in connector default only support on primary display
     */
    if (conn->built_in())
      conn->set_display_possible(default_display_possible & HWC_DISPLAY_PRIMARY_BIT);
    else
      conn->set_display_possible(default_display_possible & HWC_DISPLAY_EXTERNAL_BIT);
  }

  if (primary_length) {
    std::stringstream ss(primary_name);

    while(getline(ss, conn_name, ',')) {
      for (auto &conn : connectors_) {
        if (!strcmp(connector_type_str(conn->get_type()), conn_name.c_str()))
          conn->set_display_possible(HWC_DISPLAY_PRIMARY_BIT);
      }
    }
  }

  if (extend_length) {
    std::stringstream ss(extend_name);

    while(getline(ss, conn_name, ',')) {
      for (auto &conn : connectors_) {
        if (!strcmp(connector_type_str(conn->get_type()), conn_name.c_str()))
          conn->set_display_possible(conn->possible_displays() | HWC_DISPLAY_EXTERNAL_BIT);
      }
    }
  }
}
int DrmResources::Init() {
  char path[PROPERTY_VALUE_MAX];
  property_get("vendor.hwc.drm.device", path, "/dev/dri/card0");

  /* TODO: Use drmOpenControl here instead */
  fd_.Set(open(path, O_RDWR));
  if (fd() < 0) {
    ALOGE("Failed to open dri- %s", strerror(-errno));
    return -ENODEV;
  }

  int ret = drmSetClientCap(fd(), DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
  if (ret) {
    ALOGE("Failed to set universal plane cap %d", ret);
    return ret;
  }

  ret = drmSetClientCap(fd(), DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret) {
    ALOGE("Failed to set atomic cap %d", ret);
    return ret;
  }

  //Enable 3d function.
  ret = drmSetClientCap(fd(), DRM_CLIENT_CAP_STEREO_3D, 1);
  if (ret) {
    ALOGE("Failed to set stereo 3d cap %d", ret);
    return ret;
  }

#if USE_MULTI_AREAS
  //Open Multi-area support.
  ret = drmSetClientCap(fd(), DRM_CLIENT_CAP_SHARE_PLANES, 1);
  if (ret) {
    ALOGE("Failed to set share planes %d", ret);
    return ret;
  }
#endif
  ret = drmSetClientCap(fd(), DRM_CLIENT_CAP_ASPECT_RADIO, 0);
  if (ret) {
    ALOGE("Failed to disable aspect radio %d", ret);
    return ret;
  }

  drmModeResPtr res = drmModeGetResources(fd());
  if (!res) {
    ALOGE("Failed to get DrmResources resources");
    return -ENODEV;
  }

  bool found_primary = false;
  int display_num = 1;

    std::ostringstream out;
    out << "Frame buffers:\n";
    out << "id\tsize\tpitch\n";
    for (int i = 0; !ret && i < res->count_fbs; ++i) {
        drmModeFBPtr fb = drmModeGetFB(fd(), res->fbs[i]);
        if (!fb) {
          ALOGE("Failed to get FB %d", res->fbs[i]);
          ret = -ENODEV;
          break;
        }

        out << fb->fb_id << "\t("
            << fb->width << "x"
            << fb->height << ")\t"
            << fb->pitch << "\n";

        drmModeFreeFB(fb);
    }

  ALOGD_IF(1,"%s",out.str().c_str());
  out.str("");

  out << "CRTCs:\n";
  out << "id\tfb\tpos\tsize\n";

  for (int i = 0; !ret && i < res->count_crtcs; ++i) {
    drmModeCrtcPtr c = drmModeGetCrtc(fd(), res->crtcs[i]);
    if (!c) {
      ALOGE("Failed to get crtc %d", res->crtcs[i]);
      ret = -ENODEV;
      break;
    }

    std::unique_ptr<DrmCrtc> crtc(new DrmCrtc(this, c, i));

    crtc->dump_crtc(&out);
    out << "\n";

    drmModeFreeCrtc(c);

    ret = crtc->Init();
    if (ret) {
      ALOGE("Failed to initialize crtc %d", res->crtcs[i]);
      break;
    }
    crtcs_.emplace_back(std::move(crtc));
  }

  ALOGD_IF(1,"%s",out.str().c_str());
  out.str("");

  out << "Encoders:\n";
  out << "id\tcrtc\ttype\tpossible crtcs\tpossible clones\t\n";

  for (int i = 0; !ret && i < res->count_encoders; ++i) {
    drmModeEncoderPtr e = drmModeGetEncoder(fd(), res->encoders[i]);
    if (!e) {
      ALOGE("Failed to get encoder %d", res->encoders[i]);
      ret = -ENODEV;
      break;
    }

    std::vector<DrmCrtc *> possible_crtcs;
    DrmCrtc *current_crtc = NULL;
    for (auto &crtc : crtcs_) {
      if ((1 << crtc->pipe()) & e->possible_crtcs)
        possible_crtcs.push_back(crtc.get());

      if (crtc->id() == e->crtc_id)
        current_crtc = crtc.get();
    }

    std::unique_ptr<DrmEncoder> enc(
        new DrmEncoder(this, e, current_crtc, possible_crtcs));
    ALOGD("current_crtc id : %d  crtid : %d", enc->id(), e->crtc_id);

    enc->dump_encoder(&out);
    out << "\n";

    drmModeFreeEncoder(e);

    encoders_.emplace_back(std::move(enc));
  }
  ALOGD_IF(1,"%s",out.str().c_str());
  out.str("");


  out << "Connectors:\n";
  out << "id\tencoder\tstatus\t\ttype\tsize (mm)\tmodes\tencoders\n";
  for (int i = 0; !ret && i < res->count_connectors; ++i) {
    drmModeConnectorPtr c = drmModeGetConnector(fd(), res->connectors[i]);
    if (!c) {
      ALOGE("Failed to get connector %d", res->connectors[i]);
      ret = -ENODEV;
      break;
    }

    std::vector<DrmEncoder *> possible_encoders;
    DrmEncoder *current_encoder = NULL;
    for (int j = 0; j < c->count_encoders; ++j) {
      for (auto &encoder : encoders_) {
        if (encoder->id() == c->encoders[j])
          possible_encoders.push_back(encoder.get());
        if (encoder->id() == c->encoder_id)
          current_encoder = encoder.get();
      }
    }

    std::unique_ptr<DrmConnector> conn(
        new DrmConnector(this, c, current_encoder, possible_encoders));

    conn->dump_connector(&out);
    out << "\n";

    drmModeFreeConnector(c);

    ret = conn->Init();
    if (ret) {
      ALOGE("Init connector %d failed", res->connectors[i]);
      break;
    }
    conn->UpdateModes();

    conn->set_display(display_num);
    display_num++;

    connectors_.emplace_back(std::move(conn));
  }
  
  ConfigurePossibleDisplays();
  for (auto &conn : connectors_) {
    if (!(conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT))
      continue;
    if (conn->built_in())
      continue;
    if (conn->state() != DRM_MODE_CONNECTED)
      continue;
    found_primary = true;
    SetPrimaryDisplay(conn.get());
  }
  
  if (!found_primary) {
    for (auto &conn : connectors_) {
      if (!(conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT))
        continue;
      if (conn->state() != DRM_MODE_CONNECTED)
        continue;
      found_primary = true;
      SetPrimaryDisplay(conn.get());
    }
  }

  if (!found_primary) {
    for (auto &conn : connectors_) {
      if (!(conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT))
        continue;
      found_primary = true;
      SetPrimaryDisplay(conn.get());
    }
  }

  if (!found_primary) {
    ALOGE("failed to find primary display\n");
    return -ENODEV;
  }

  for (auto &conn : connectors_) {
      if (!(conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT))
        continue;
      if (conn->state() != DRM_MODE_CONNECTED)
        continue;
      SetExtendDisplay(conn.get());
  }

  ALOGD_IF(1,"%s",out.str().c_str());
  out.str("");

  if (res)
    drmModeFreeResources(res);

  // Catch-all for the above loops
  if (ret)
    return ret;


  return 0;
}

void DrmResources::DisplayChanged(void) {
    enable_changed_ = true;
};

void DrmResources::SetPrimaryDisplay(DrmConnector *c) {
  if (primary_ != c) {
    primary_ = c;
  }
    enable_changed_ = true;
};

void DrmResources::SetExtendDisplay(DrmConnector *c) {
  if (extend_ != c) {
    if (extend_)
      extend_->force_disconnect(false);
    extend_ = c;
    enable_changed_ = true;
  }
};

DrmConnector *DrmResources::GetConnectorFromType(int display_type) const {
  if (display_type == HWC_DISPLAY_PRIMARY)
    return primary_;
  else if (display_type == HWC_DISPLAY_EXTERNAL)
    return extend_;
  return NULL;
}

DrmCrtc *DrmResources::GetCrtcFromConnector(DrmConnector *conn) const {
   if (conn->encoder())
     return conn->encoder()->crtc();
   return NULL;
}


uint32_t DrmResources::next_mode_id() {
  return ++mode_id_;
}

void DrmResources::ClearDisplay(void)
{
    for (int i = 0; i < HWC_NUM_PHYSICAL_DISPLAY_TYPES; i++) {
      DrmConnector *conn = GetConnectorFromType(i);
      if (conn && conn->state() == DRM_MODE_CONNECTED &&
          conn->current_mode().id() && conn->encoder() &&
          conn->encoder()->crtc())
        continue;
    }
}

int DrmResources::UpdateDisplayRoute(void)
{
  bool mode_changed = false;
  int i;

  for (i = 0; i < HWC_NUM_PHYSICAL_DISPLAY_TYPES; i++) {
    DrmConnector *conn = GetConnectorFromType(i);

    if (!conn || conn->state() != DRM_MODE_CONNECTED || !conn->current_mode().id())
      continue;
    if (conn->current_mode() == conn->active_mode())
      continue;
    mode_changed = true;
  }

  if (!enable_changed_ && !mode_changed)
    return 0;

  DrmConnector *primary = GetConnectorFromType(HWC_DISPLAY_PRIMARY);
  if (!primary) {
    ALOGE("Failed to find primary display\n");
    return -EINVAL;
  }
  DrmConnector *extend = GetConnectorFromType(HWC_DISPLAY_EXTERNAL);
  if (enable_changed_) {
    primary->set_encoder(NULL);
    if (extend)
      extend->set_encoder(NULL);
    if (primary->state() == DRM_MODE_CONNECTED) {
      for (DrmEncoder *enc : primary->possible_encoders()) {
        for (DrmCrtc *crtc : enc->possible_crtcs()) {
          if (crtc->get_afbc()) {
            enc->set_crtc(crtc);
            primary->set_encoder(enc);
            ALOGD_IF(log_level(DBG_VERBOSE), "set primary with conn[%d] crtc=%d\n",primary->id(), crtc->id());
          }
        }
      }
      /*
       * not limit
       */
      if (!primary->encoder() || !primary->encoder()->crtc()) {
        for (DrmEncoder *enc : primary->possible_encoders()) {
          for (DrmCrtc *crtc : enc->possible_crtcs()) {
              enc->set_crtc(crtc);
              primary->set_encoder(enc);
              ALOGD_IF(log_level(DBG_VERBOSE), "set primary with conn[%d] crtc=%d\n",primary->id(), crtc->id());
          }
        }
      }
    }
    if (extend && extend->state() == DRM_MODE_CONNECTED) {
      for (DrmEncoder *enc : extend->possible_encoders()) {
        for (DrmCrtc *crtc : enc->possible_crtcs()) {
          if (primary && primary->encoder() && primary->encoder()->crtc()) {
            if (crtc == primary->encoder()->crtc())
              continue;
          }
          ALOGD_IF(log_level(DBG_VERBOSE), "set extend[%d] with crtc=%d\n", extend->id(), crtc->id());
          enc->set_crtc(crtc);
          extend->set_encoder(enc);
        }
      }
      if (!extend->encoder() || !extend->encoder()->crtc()) {
        for (DrmEncoder *enc : extend->possible_encoders()) {
          for (DrmCrtc *crtc : enc->possible_crtcs()) {
            enc->set_crtc(crtc);
            extend->set_encoder(enc);
            ALOGD_IF(log_level(DBG_VERBOSE), "set extend[%d] with crtc=%d\n", extend->id(), crtc->id());
            if (primary && primary->encoder() && primary->encoder()->crtc()) {
              if (crtc == primary->encoder()->crtc()) {
                primary->encoder()->set_crtc(NULL);
                primary->set_encoder(NULL);
                for (DrmEncoder *primary_enc : primary->possible_encoders()) {
                  for (DrmCrtc *primary_crtc : primary_enc->possible_crtcs()) {
                    if (extend && extend->encoder() && extend->encoder()->crtc()) {
                      if (primary_crtc == extend->encoder()->crtc())
                        continue;
                    }

                    primary_enc->set_crtc(primary_crtc);
                    primary->set_encoder(primary_enc);
                    ALOGD_IF(log_level(DBG_VERBOSE), "set primary with conn[%d] crtc=%d\n",primary->id(), primary_crtc->id());
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  enable_changed_ = false;

  return 0;
}

int DrmResources::CreatePropertyBlob(void *data, size_t length,
                                     uint32_t *blob_id) {
  struct drm_mode_create_blob create_blob;
  memset(&create_blob, 0, sizeof(create_blob));
  create_blob.length = length;
  create_blob.data = (__u64)data;

  int ret = drmIoctl(fd(), DRM_IOCTL_MODE_CREATEPROPBLOB, &create_blob);
  if (ret) {
    ALOGE("Failed to create mode property blob %d", ret);
    return ret;
  }
  *blob_id = create_blob.blob_id;
  return 0;
}

int DrmResources::DestroyPropertyBlob(uint32_t blob_id) {
  if (!blob_id)
    return 0;

  struct drm_mode_destroy_blob destroy_blob;
  memset(&destroy_blob, 0, sizeof(destroy_blob));
  destroy_blob.blob_id = (__u32)blob_id;
  int ret = drmIoctl(fd(), DRM_IOCTL_MODE_DESTROYPROPBLOB, &destroy_blob);
  if (ret) {
    ALOGE("Failed to destroy mode property blob %" PRIu32 "/%d", blob_id, ret);
    return ret;
  }
  return 0;
}


int DrmResources::GetProperty(uint32_t obj_id, uint32_t obj_type,
                              const char *prop_name, DrmProperty *property) {
  drmModeObjectPropertiesPtr props;

  props = drmModeObjectGetProperties(fd(), obj_id, obj_type);
  if (!props) {
    ALOGE("Failed to get properties for %d/%x", obj_id, obj_type);
    return -ENODEV;
  }

  bool found = false;
  for (int i = 0; !found && (size_t)i < props->count_props; ++i) {
    drmModePropertyPtr p = drmModeGetProperty(fd(), props->props[i]);
    if (!strcmp(p->name, prop_name)) {
      property->Init(p, props->prop_values[i]);
      found = true;
    }
    drmModeFreeProperty(p);
  }

  drmModeFreeObjectProperties(props);
  return found ? 0 : -ENOENT;
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
static inline int64_t U642I64(uint64_t val)
{
	return (int64_t)*((int64_t *)&val);
}

struct type_name {
	int type;
	const char *name;
};

#define type_name_fn(res) \
const char * DrmResources::res##_str(int type) {			\
	unsigned int i;					\
	for (i = 0; i < ARRAY_SIZE(res##_names); i++) { \
		if (res##_names[i].type == type)	\
			return res##_names[i].name;	\
	}						\
	return "(invalid)";				\
}

struct type_name encoder_type_names[] = {
	{ DRM_MODE_ENCODER_NONE, "none" },
	{ DRM_MODE_ENCODER_DAC, "DAC" },
	{ DRM_MODE_ENCODER_TMDS, "TMDS" },
	{ DRM_MODE_ENCODER_LVDS, "LVDS" },
	{ DRM_MODE_ENCODER_TVDAC, "TVDAC" },
};

type_name_fn(encoder_type)

struct type_name connector_status_names[] = {
	{ DRM_MODE_CONNECTED, "connected" },
	{ DRM_MODE_DISCONNECTED, "disconnected" },
	{ DRM_MODE_UNKNOWNCONNECTION, "unknown" },
};

type_name_fn(connector_status)

struct type_name connector_type_names[] = {
	{ DRM_MODE_CONNECTOR_Unknown, "unknown" },
	{ DRM_MODE_CONNECTOR_VGA, "VGA" },
	{ DRM_MODE_CONNECTOR_DVII, "DVI-I" },
	{ DRM_MODE_CONNECTOR_DVID, "DVI-D" },
	{ DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
	{ DRM_MODE_CONNECTOR_Composite, "composite" },
	{ DRM_MODE_CONNECTOR_SVIDEO, "s-video" },
	{ DRM_MODE_CONNECTOR_LVDS, "LVDS" },
	{ DRM_MODE_CONNECTOR_Component, "component" },
	{ DRM_MODE_CONNECTOR_9PinDIN, "9-pin DIN" },
	{ DRM_MODE_CONNECTOR_DisplayPort, "DP" },
	{ DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
	{ DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
	{ DRM_MODE_CONNECTOR_TV, "TV" },
	{ DRM_MODE_CONNECTOR_eDP, "eDP" },
	{ DRM_MODE_CONNECTOR_VIRTUAL, "Virtual" },
	{ DRM_MODE_CONNECTOR_DSI, "DSI" },
};

type_name_fn(connector_type)

#define bit_name_fn(res)					\
const char * res##_str(int type, std::ostringstream *out) {				\
	unsigned int i;						\
	const char *sep = "";					\
	for (i = 0; i < ARRAY_SIZE(res##_names); i++) {		\
		if (type & (1 << i)) {				\
			*out << sep << res##_names[i];	\
			sep = ", ";				\
		}						\
	}							\
	return NULL;						\
}

static const char *mode_type_names[] = {
	"builtin",
	"clock_c",
	"crtc_c",
	"preferred",
	"default",
	"userdef",
	"driver",
};

static bit_name_fn(mode_type)

static const char *mode_flag_names[] = {
	"phsync",
	"nhsync",
	"pvsync",
	"nvsync",
	"interlace",
	"dblscan",
	"csync",
	"pcsync",
	"ncsync",
	"hskew",
	"bcast",
	"pixmux",
	"dblclk",
	"clkdiv2"
};
static bit_name_fn(mode_flag)

void DrmResources::dump_mode(drmModeModeInfo *mode, std::ostringstream *out) {
	*out << mode->name << " " << mode->vrefresh << " "
	     << mode->hdisplay << " " << mode->hsync_start << " "
	     << mode->hsync_end << " " << mode->htotal << " "
	     << mode->vdisplay << " " << mode->vsync_start << " "
	     << mode->vsync_end << " " << mode->vtotal;

	*out << " flags: ";
	mode_flag_str(mode->flags, out);
	*out << " types: " << mode->type << "\n";
    mode_type_str(mode->type, out);
}

void DrmResources::dump_blob(uint32_t blob_id, std::ostringstream *out) {
	uint32_t i;
	unsigned char *blob_data;
	drmModePropertyBlobPtr blob;

	blob = drmModeGetPropertyBlob(fd(), blob_id);
	if (!blob) {
		*out << "\n";
		return;
	}

	blob_data = (unsigned char*)blob->data;

	for (i = 0; i < blob->length; i++) {
		if (i % 16 == 0)
			*out << "\n\t\t\t";
		*out << std::hex << blob_data[i];
	}
	*out << "\n";

	drmModeFreePropertyBlob(blob);
}

void DrmResources::dump_prop(drmModePropertyPtr prop,
		      uint32_t prop_id, uint64_t value, std::ostringstream *out) {
	int i;

	*out << "\t" << prop_id;
	if (!prop) {
		*out << "\n";
		return;
	}
ALOGD_IF(log_level(DBG_VERBOSE),"%s",out->str().c_str());
out->str("");
	*out << " " << prop->name << ":\n";

	*out << "\t\tflags:";
	if (prop->flags & DRM_MODE_PROP_PENDING)
		*out << " pending";
	if (prop->flags & DRM_MODE_PROP_IMMUTABLE)
		*out << " immutable";
	if (drm_property_type_is(prop, DRM_MODE_PROP_SIGNED_RANGE))
		*out << " signed range";
	if (drm_property_type_is(prop, DRM_MODE_PROP_RANGE))
		*out << " range";
	if (drm_property_type_is(prop, DRM_MODE_PROP_ENUM))
		*out << " enum";
	if (drm_property_type_is(prop, DRM_MODE_PROP_BITMASK))
		*out << " bitmask";
	if (drm_property_type_is(prop, DRM_MODE_PROP_BLOB))
		*out << " blob";
	if (drm_property_type_is(prop, DRM_MODE_PROP_OBJECT))
		*out << " object";
	*out << "\n";

	if (drm_property_type_is(prop, DRM_MODE_PROP_SIGNED_RANGE)) {
		*out << "\t\tvalues:";
		for (i = 0; i < prop->count_values; i++)
			*out << U642I64(prop->values[i]);
		*out << "\n";
	}

	if (drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
		*out << "\t\tvalues:";
		for (i = 0; i < prop->count_values; i++)
			*out << prop->values[i];
		*out << "\n";
	}

	if (drm_property_type_is(prop, DRM_MODE_PROP_ENUM)) {
		*out << "\t\tenums:";
		for (i = 0; i < prop->count_enums; i++)
			*out << prop->enums[i].name << "=" << prop->enums[i].value;
		*out << "\n";
	} else if (drm_property_type_is(prop, DRM_MODE_PROP_BITMASK)) {
		*out << "\t\tvalues:";
		for (i = 0; i < prop->count_enums; i++)
			*out << prop->enums[i].name << "=" << std::hex << (1LL << prop->enums[i].value);
		*out << "\n";
	} else {
		//assert(prop->count_enums == 0);
	}

	if (drm_property_type_is(prop, DRM_MODE_PROP_BLOB)) {
		*out << "\t\tblobs:\n";
		for (i = 0; i < prop->count_blobs; i++)
			dump_blob(prop->blob_ids[i], out);
		*out << "\n";
	} else {
		//assert(prop->count_blobs == 0);
	}

	*out << "\t\tvalue:";
	if (drm_property_type_is(prop, DRM_MODE_PROP_BLOB))
		dump_blob(value, out);
	else
		*out << value;

    *out << "\n";
}

int DrmResources::DumpProperty(uint32_t obj_id, uint32_t obj_type, std::ostringstream *out) {
  drmModePropertyPtr* prop_info;
  drmModeObjectPropertiesPtr props;

  props = drmModeObjectGetProperties(fd(), obj_id, obj_type);
  if (!props) {
    ALOGE("Failed to get properties for %d/%x", obj_id, obj_type);
    return -ENODEV;
  }
  prop_info = (drmModePropertyPtr*)malloc(props->count_props * sizeof *prop_info);
  if (!prop_info) {
    ALOGE("Malloc drmModePropertyPtr array failed");
    return -ENOMEM;
  }

  *out << "  props:\n";
  for (int i = 0;(size_t)i < props->count_props; ++i) {
    prop_info[i] = drmModeGetProperty(fd(), props->props[i]);

    dump_prop(prop_info[i],props->props[i],props->prop_values[i],out);

    drmModeFreeProperty(prop_info[i]);
  }

  drmModeFreeObjectProperties(props);
  free(prop_info);
  return 0;
}

int DrmResources::DumpCrtcProperty(const DrmCrtc &crtc, std::ostringstream *out) {
  return DumpProperty(crtc.id(), DRM_MODE_OBJECT_CRTC, out);
}

int DrmResources::DumpConnectorProperty(const DrmConnector &connector, std::ostringstream *out) {
   return DumpProperty(connector.id(), DRM_MODE_OBJECT_CONNECTOR, out);
}


int DrmResources::GetCrtcProperty(const DrmCrtc &crtc, const char *prop_name,
                                  DrmProperty *property) {
  return GetProperty(crtc.id(), DRM_MODE_OBJECT_CRTC, prop_name, property);
}

int DrmResources::GetConnectorProperty(const DrmConnector &connector,
                                       const char *prop_name,
                                       DrmProperty *property) {
  return GetProperty(connector.id(), DRM_MODE_OBJECT_CONNECTOR, prop_name,
                     property);
}


}
