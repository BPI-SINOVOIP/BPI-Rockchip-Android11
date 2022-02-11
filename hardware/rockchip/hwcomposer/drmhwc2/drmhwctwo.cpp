/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwc-drm-two"

#include "drmhwctwo.h"
#include "drmdisplaycomposition.h"
#include "drmlayer.h"
#include "platform.h"
#include "vsyncworker.h"
#include "rockchip/utils/drmdebug.h"
#include "rockchip/drmgralloc.h"

#include <inttypes.h>
#include <string>

#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <log/log.h>
#include <utils/Trace.h>

#include <linux/fb.h>

namespace android {


#define fourcc_code(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
             ((__u32)(c) << 16) | ((__u32)(d) << 24))

#define DRM_FORMAT_ABGR8888	fourcc_code('A', 'B', '2', '4')

#define ALOGD_HWC2_DRM_LAYER_INFO(log_level, drmHwcLayers) \
    if(LogLevel(log_level)){ \
      String8 output; \
      for(auto &drmHwcLayer : drmHwcLayers) {\
        drmHwcLayer.DumpInfo(output); \
        ALOGD_IF(LogLevel(log_level),"%s",output.string()); \
        output.clear(); \
      }\
    }

class DrmVsyncCallback : public VsyncCallback {
 public:
  DrmVsyncCallback(hwc2_callback_data_t data, hwc2_function_pointer_t hook)
      : data_(data), hook_(hook) {
  }

  void Callback(int display, int64_t timestamp) {
    auto hook = reinterpret_cast<HWC2_PFN_VSYNC>(hook_);
    if(hook){
      hook(data_, display, timestamp);
    }
  }

 private:
  hwc2_callback_data_t data_;
  hwc2_function_pointer_t hook_;
};

class DrmInvalidateCallback : public InvalidateCallback {
 public:
  DrmInvalidateCallback(hwc2_callback_data_t data, hwc2_function_pointer_t hook)
      : data_(data), hook_(hook) {
  }

  void Callback(int display) {
    auto hook = reinterpret_cast<HWC2_PFN_REFRESH>(hook_);
    if(hook){
      hook(data_, display);
    }
  }

 private:
  hwc2_callback_data_t data_;
  hwc2_function_pointer_t hook_;
};


DrmHwcTwo::DrmHwcTwo()
  : resource_manager_(ResourceManager::getInstance()) {
  common.tag = HARDWARE_DEVICE_TAG;
  common.version = HWC_DEVICE_API_VERSION_2_0;
  common.close = HookDevClose;
  getCapabilities = HookDevGetCapabilities;
  getFunction = HookDevGetFunction;
}

HWC2::Error DrmHwcTwo::CreateDisplay(hwc2_display_t displ,
                                     HWC2::DisplayType type) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,displ);

  DrmDevice *drm = resource_manager_->GetDrmDevice(displ);
  std::shared_ptr<Importer> importer = resource_manager_->GetImporter(displ);
  if (!drm || !importer) {
    ALOGE("Failed to get a valid drmresource and importer");
    return HWC2::Error::NoResources;
  }
  displays_.emplace(std::piecewise_construct, std::forward_as_tuple(displ),
                    std::forward_as_tuple(resource_manager_, drm, importer,
                                          displ, type));
  displays_.at(displ).Init();
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::Init() {
  HWC2_ALOGD_IF_VERBOSE();
  int rv = resource_manager_->Init();
  if (rv) {
    ALOGE("Can't initialize the resource manager %d", rv);
    return HWC2::Error::NoResources;
  }

  HWC2::Error ret = HWC2::Error::None;
  for (int i = 0; i < resource_manager_->getDisplayCount(); i++) {
    ret = CreateDisplay(i, HWC2::DisplayType::Physical);
    if (ret != HWC2::Error::None) {
      ALOGE("Failed to create display %d with error %d", i, ret);
      return ret;
    }
  }

  auto &drmDevices = resource_manager_->getDrmDevices();
  for (auto &device : drmDevices) {
    device->RegisterHotplugHandler(new DrmHotplugHandler(this, device.get()));
  }
  return ret;
}

template <typename... Args>
static inline HWC2::Error unsupported(char const *func, Args... /*args*/) {
  ALOGV("Unsupported function: %s", func);
  return HWC2::Error::Unsupported;
}

static inline void supported(char const *func) {
  ALOGV("Supported function: %s", func);
}

HWC2::Error DrmHwcTwo::CreateVirtualDisplay(uint32_t width, uint32_t height,
                                            int32_t *format,
                                            hwc2_display_t *display) {
  HWC2_ALOGD_IF_VERBOSE("w=%u,h=%u",width,height);
  // TODO: Implement virtual display
  return unsupported(__func__, width, height, format, display);
}

HWC2::Error DrmHwcTwo::DestroyVirtualDisplay(hwc2_display_t display) {

  HWC2_ALOGD_IF_VERBOSE();
  // TODO: Implement virtual display
  return unsupported(__func__, display);
}

void DrmHwcTwo::Dump(uint32_t *size, char *buffer) {
  if (buffer != nullptr) {
      auto copiedBytes = mDumpString.copy(buffer, *size);
      *size = static_cast<uint32_t>(copiedBytes);
      return;
  }
  String8 output;

  char acVersion[50] = {0};
  strcpy(acVersion,GHWC_VERSION);

  output.appendFormat("-- HWC2 Version %s by bin.li@rock-chips.com --\n",acVersion);
  for(auto &map_disp: displays_){
    output.append("\n");
    if((map_disp.second.DumpDisplayInfo(output)) < 0)
      continue;
  }
  mDumpString = output.string();
  *size = static_cast<uint32_t>(mDumpString.size());
  return;
}

uint32_t DrmHwcTwo::GetMaxVirtualDisplayCount() {
  HWC2_ALOGD_IF_VERBOSE();
  // TODO: Implement virtual display
  unsupported(__func__);
  return 0;
}

static bool isValid(HWC2::Callback descriptor) {
    switch (descriptor) {
        case HWC2::Callback::Hotplug: // Fall-through
        case HWC2::Callback::Refresh: // Fall-through
        case HWC2::Callback::Vsync: return true;
        default: return false;
    }
}

HWC2::Error DrmHwcTwo::RegisterCallback(int32_t descriptor,
                                        hwc2_callback_data_t data,
                                        hwc2_function_pointer_t function) {
  HWC2_ALOGD_IF_VERBOSE();

  auto callback = static_cast<HWC2::Callback>(descriptor);

  if (!isValid(callback)) {
      return HWC2::Error::BadParameter;
  }

  if (!function) {
    callbacks_.erase(callback);
    switch (callback) {
      case HWC2::Callback::Vsync: {
        for (std::pair<const hwc2_display_t, DrmHwcTwo::HwcDisplay> &d :
             displays_)
          d.second.UnregisterVsyncCallback();
        break;
      }
      case HWC2::Callback::Refresh: {
        for (std::pair<const hwc2_display_t, DrmHwcTwo::HwcDisplay> &d :
             displays_)
          d.second.UnregisterInvalidateCallback();
          break;
      }
      default:
        break;
    }
    return HWC2::Error::None;
  }

  callbacks_.emplace(callback, HwcCallback(data, function));

  switch (callback) {
    case HWC2::Callback::Hotplug: {
      auto hotplug = reinterpret_cast<HWC2_PFN_HOTPLUG>(function);
      hotplug(data, HWC_DISPLAY_PRIMARY,
              static_cast<int32_t>(HWC2::Connection::Connected));
      auto &drmDevices = resource_manager_->getDrmDevices();
      for (auto &device : drmDevices)
        HandleInitialHotplugState(device.get());
      break;
    }
    case HWC2::Callback::Vsync: {
      for (std::pair<const hwc2_display_t, DrmHwcTwo::HwcDisplay> &d :
           displays_)
        d.second.RegisterVsyncCallback(data, function);
      break;
    }
    case HWC2::Callback::Refresh: {
      for (std::pair<const hwc2_display_t, DrmHwcTwo::HwcDisplay> &d :
           displays_)
        d.second.RegisterInvalidateCallback(data, function);
        break;
    }
    default:
      break;
  }
  return HWC2::Error::None;
}

DrmHwcTwo::HwcDisplay::HwcDisplay(ResourceManager *resource_manager,
                                  DrmDevice *drm,
                                  std::shared_ptr<Importer> importer,
                                  hwc2_display_t handle, HWC2::DisplayType type)
    : resource_manager_(resource_manager),
      drm_(drm),
      importer_(importer),
      handle_(handle),
      type_(type),
      client_layer_(UINT32_MAX,drm),
      init_success_(false){
}

void DrmHwcTwo::HwcDisplay::ClearDisplay() {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  compositor_.ClearDisplay();

  DrmCrtc *crtc = crtc_;
  if(init_success_ && crtc != NULL){
    uint32_t crtc_mask = 1 << crtc->pipe();
    std::vector<PlaneGroup*> plane_groups = drm_->GetPlaneGroups();
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
      //loop plane
      if((*iter)->is_release(crtc_mask) && (*iter)->release(crtc_mask)){
          for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
                !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane) {
                if ((*iter_plane)->GetCrtcSupported(*crtc_)) {
                    ALOGD_IF(LogLevel(DBG_DEBUG),"ClearDisplay %s %s",
                              (*iter_plane)->name(),"release plane");
                   break;
                }
          }
      }
    }
  }
}

void DrmHwcTwo::HwcDisplay::ReleaseResource(){
  resource_manager_->removeActiveDisplayCnt(static_cast<int>(handle_));
  resource_manager_->assignPlaneGroup();
}

HWC2::Error DrmHwcTwo::HwcDisplay::Init() {

  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  int display = static_cast<int>(handle_);

  connector_ = drm_->GetConnectorForDisplay(display);
  if (!connector_) {
    ALOGE("Failed to get connector for display %d", display);
    return HWC2::Error::BadDisplay;
  }

  int ret = vsync_worker_.Init(drm_, display);
  if (ret) {
    ALOGE("Failed to create event worker for d=%d %d\n", display, ret);
    return HWC2::Error::BadDisplay;
  }

  ret = invalidate_worker_.Init(display);
  if (ret) {
    ALOGE("Failed to create invalidate worker for d=%d %d\n", display, ret);
    return HWC2::Error::BadDisplay;
  }

  if(connector_->state() != DRM_MODE_CONNECTED){
    ALOGI("Connector %u type=%s, type_id=%d, state is DRM_MODE_DISCONNECTED, skip init.\n",
          connector_->id(),drm_->connector_type_str(connector_->type()),connector_->type_id());
    return HWC2::Error::NoResources;
  }

  UpdateDisplayMode();
  ret = drm_->BindDpyRes(handle_);
  if (ret) {
    HWC2_ALOGE("Failed to BindDpyRes for display=%d %d\n", display, ret);
    return HWC2::Error::NoResources;
  }

  ret = drm_->UpdateDisplayGamma(handle_);
  if (ret) {
    HWC2_ALOGE("Failed to UpdateDisplayGamma for display=%d %d\n", display, ret);
  }

  ret = drm_->UpdateDisplay3DLut(handle_);
  if (ret) {
    HWC2_ALOGE("Failed to UpdateDisplay3DLut for display=%d %d\n", display, ret);
  }

  crtc_ = drm_->GetCrtcForDisplay(display);
  if (!crtc_) {
    ALOGE("Failed to get crtc for display %d", display);
    return HWC2::Error::BadDisplay;
  }

  planner_ = Planner::CreateInstance(drm_);
  if (!planner_) {
    ALOGE("Failed to create planner instance for composition");
    return HWC2::Error::NoResources;
  }

  ret = compositor_.Init(resource_manager_, display);
  if (ret) {
    ALOGE("Failed display compositor init for display %d (%d)", display, ret);
    return HWC2::Error::NoResources;
  }

  resource_manager_->creatActiveDisplayCnt(display);
  resource_manager_->assignPlaneGroup();

  // soc_id
  ctx_.soc_id = resource_manager_->getSocId();
  // vop aclk
  ctx_.aclk = crtc_->get_aclk();
  // Baseparameter Info
  ctx_.baseparameter_info = connector_->baseparameter_info();
  // Standard Switch Resolution Mode
  ctx_.bStandardSwitchResolution = hwc_get_bool_property("vendor.hwc.enable_display_configs","false");

  HWC2::Error error = ChosePreferredConfig();
  if(error != HWC2::Error::None){
    ALOGE("Failed to chose prefererd config for display %d (%d)", display, error);
    return error;
  }

  init_success_ = true;

  return HWC2::Error::None;
}


HWC2::Error DrmHwcTwo::HwcDisplay::CheckStateAndReinit() {

  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  int display = static_cast<int>(handle_);

  connector_ = drm_->GetConnectorForDisplay(display);
  if (!connector_) {
    ALOGE("Failed to get connector for display %d", display);
    return HWC2::Error::BadDisplay;
  }

  if(connector_->state() != DRM_MODE_CONNECTED){
    ALOGI("Connector %u type=%s, type_id=%d, state is DRM_MODE_DISCONNECTED, skip init.\n",
          connector_->id(),drm_->connector_type_str(connector_->type()),connector_->type_id());
    return HWC2::Error::NoResources;
  }

  UpdateDisplayMode();
  int ret = drm_->BindDpyRes(handle_);
  if (ret) {
    HWC2_ALOGE("Failed to BindDpyRes for display=%d %d\n", display, ret);
    return HWC2::Error::NoResources;
  }

  crtc_ = drm_->GetCrtcForDisplay(display);
  if (!crtc_) {
    ALOGE("Failed to get crtc for display %d", display);
    return HWC2::Error::BadDisplay;
  }

  ret = drm_->UpdateDisplayGamma(handle_);
  if (ret) {
    HWC2_ALOGE("Failed to UpdateDisplayGamma for display=%d %d\n", display, ret);
  }

  ret = drm_->UpdateDisplay3DLut(handle_);
  if (ret) {
    HWC2_ALOGE("Failed to UpdateDisplay3DLut for display=%d %d\n", display, ret);
  }

  resource_manager_->creatActiveDisplayCnt(display);
  resource_manager_->assignPlaneGroup();

  // Reset HwcLayer resource
  if(handle_ != HWC_DISPLAY_PRIMARY){
    // Clear Layers
    layers_.clear();
    // Clear Client Target Layer
    client_layer_.clear();
  }

  if(init_success_){
    return HWC2::Error::None;
  }

  planner_ = Planner::CreateInstance(drm_);
  if (!planner_) {
    ALOGE("Failed to create planner instance for composition");
    return HWC2::Error::NoResources;
  }

  ret = compositor_.Init(resource_manager_, display);
  if (ret) {
    ALOGE("Failed display compositor init for display %d (%d)", display, ret);
    return HWC2::Error::NoResources;
  }

  // soc_id
  ctx_.soc_id = resource_manager_->getSocId();
  // vop aclk
  ctx_.aclk = crtc_->get_aclk();
  // Baseparameter Info
  ctx_.baseparameter_info = connector_->baseparameter_info();
  // Standard Switch Resolution Mode
  ctx_.bStandardSwitchResolution = hwc_get_bool_property("vendor.hwc.enable_display_configs","false");

  HWC2::Error error = ChosePreferredConfig();
  if(error != HWC2::Error::None){
    ALOGE("Failed to chose prefererd config for display %d (%d)", display, error);
    return error;
  }

  init_success_ = true;

  return HWC2::Error::None;
}


HWC2::Error DrmHwcTwo::HwcDisplay::CheckDisplayState(){
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  int display = static_cast<int>(handle_);

  if(!init_success_){
    ALOGE_IF(LogLevel(DBG_ERROR),"Display %d not init success! %s,line=%d", display,
          __FUNCTION__, __LINE__);
    return HWC2::Error::BadDisplay;
  }

  connector_ = drm_->GetConnectorForDisplay(display);
  if (!connector_) {
    ALOGE_IF(LogLevel(DBG_ERROR),"Failed to get connector for display %d, %s,line=%d",
          display, __FUNCTION__, __LINE__);
    return HWC2::Error::BadDisplay;
  }

  if(connector_->state() != DRM_MODE_CONNECTED){
    ALOGE_IF(LogLevel(DBG_ERROR),"Connector %u type=%s, type_id=%d, state is DRM_MODE_DISCONNECTED, skip init, %s,line=%d\n",
          connector_->id(),drm_->connector_type_str(connector_->type()),connector_->type_id(),
          __FUNCTION__, __LINE__);
    return HWC2::Error::NoResources;
  }

  crtc_ = drm_->GetCrtcForDisplay(display);
  if (!crtc_) {
    ALOGE_IF(LogLevel(DBG_ERROR),"Failed to get crtc for display %d, %s,line=%d", display,
          __FUNCTION__, __LINE__);
    return HWC2::Error::BadDisplay;
  }

  if(!layers_.size()){
    ALOGE_IF(LogLevel(DBG_ERROR),"display %d layer size is %zu, %s,line=%d", display, layers_.size(),
          __FUNCTION__, __LINE__);
    return HWC2::Error::BadLayer;
  }

  return HWC2::Error::None;
}


HWC2::Error DrmHwcTwo::HwcDisplay::ChosePreferredConfig() {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  // Fetch the number of modes from the display
  uint32_t num_configs;
  HWC2::Error err = GetDisplayConfigs(&num_configs, NULL);
  if (err != HWC2::Error::None || !num_configs)
    return err;

  err = SetActiveConfig(connector_->best_mode().id());
  return err;
}

HWC2::Error DrmHwcTwo::HwcDisplay::RegisterVsyncCallback(
    hwc2_callback_data_t data, hwc2_function_pointer_t func) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  auto callback = std::make_shared<DrmVsyncCallback>(data, func);
  vsync_worker_.RegisterCallback(std::move(callback));
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::RegisterInvalidateCallback(
    hwc2_callback_data_t data, hwc2_function_pointer_t func) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  auto callback = std::make_shared<DrmInvalidateCallback>(data, func);
  invalidate_worker_.RegisterCallback(std::move(callback));
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::UnregisterVsyncCallback() {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);;
  vsync_worker_.RegisterCallback(NULL);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::UnregisterInvalidateCallback() {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  invalidate_worker_.RegisterCallback(NULL);
  return HWC2::Error::None;
}


HWC2::Error DrmHwcTwo::HwcDisplay::AcceptDisplayChanges() {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &l : layers_)
    l.second.accept_type_change();
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::CreateLayer(hwc2_layer_t *layer) {
  layers_.emplace(static_cast<hwc2_layer_t>(layer_idx_), HwcLayer(layer_idx_, drm_));
  *layer = static_cast<hwc2_layer_t>(layer_idx_);
  ++layer_idx_;
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", layer-id=%" PRIu64,handle_,*layer);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::DestroyLayer(hwc2_layer_t layer) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", layer-id=%" PRIu64,handle_,layer);

  auto map_layer = layers_.find(layer);
  if (map_layer != layers_.end()){
    map_layer->second.clear();
    layers_.erase(layer);
    return HWC2::Error::None;
  }else{
    return HWC2::Error::BadLayer;
  }
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetActiveConfig(hwc2_config_t *config) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ,handle_);

  if(ctx_.bStandardSwitchResolution){
    DrmMode const &mode = connector_->active_mode();
    if (mode.id() == 0)
      return HWC2::Error::BadConfig;

    DrmMode const &best_mode = connector_->best_mode();

    ctx_.framebuffer_width = best_mode.h_display();
    ctx_.framebuffer_height = best_mode.v_display();

    *config = mode.id();
  }else{
    *config = 0;
  }
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 "config-id=%d" ,handle_,*config);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetChangedCompositionTypes(
    uint32_t *num_elements, hwc2_layer_t *layers, int32_t *types) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  uint32_t num_changes = 0;
  for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &l : layers_) {
    if (l.second.type_changed()) {
      if (layers && num_changes < *num_elements)
        layers[num_changes] = l.first;
      if (types && num_changes < *num_elements)
        types[num_changes] = static_cast<int32_t>(l.second.validated_type());
      ++num_changes;
    }
  }
  if (!layers && !types)
    *num_elements = num_changes;

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetClientTargetSupport(uint32_t width,
                                                          uint32_t height,
                                                          int32_t /*format*/,
                                                          int32_t dataspace) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  std::pair<uint32_t, uint32_t> min = drm_->min_resolution();
  std::pair<uint32_t, uint32_t> max = drm_->max_resolution();

  if (width < min.first || height < min.second)
    return HWC2::Error::Unsupported;

  if (width > max.first || height > max.second)
    return HWC2::Error::Unsupported;

  if (dataspace != HAL_DATASPACE_UNKNOWN &&
      dataspace != HAL_DATASPACE_STANDARD_UNSPECIFIED)
    return HWC2::Error::Unsupported;

  // TODO: Validate format can be handled by either GL or planes
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetColorModes(uint32_t *num_modes,
                                                 int32_t *modes) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  if (!modes)
    *num_modes = 1;

  if (modes)
    *modes = HAL_COLOR_MODE_NATIVE;

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayAttribute(hwc2_config_t config,
                                                       int32_t attribute_in,
                                                       int32_t *value) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  if(ctx_.bStandardSwitchResolution){
    auto mode = std::find_if(connector_->modes().begin(),
                             connector_->modes().end(),
                             [config](DrmMode const &m) {
                               return m.id() == config;
                             });
    if (mode == connector_->modes().end()) {
      ALOGE("Could not find active mode for %d", config);
      return HWC2::Error::BadConfig;
    }

    static const int32_t kUmPerInch = 25400;
    uint32_t mm_width = connector_->mm_width();
    uint32_t mm_height = connector_->mm_height();
    auto attribute = static_cast<HWC2::Attribute>(attribute_in);
    switch (attribute) {
      case HWC2::Attribute::Width:
        *value = mode->h_display();
        break;
      case HWC2::Attribute::Height:
        *value = mode->v_display();
        break;
      case HWC2::Attribute::VsyncPeriod:
        // in nanoseconds
        *value = 1000 * 1000 * 1000 / mode->v_refresh();
        break;
      case HWC2::Attribute::DpiX:
        // Dots per 1000 inches
        *value = mm_width ? (mode->h_display() * kUmPerInch) / mm_width : -1;
        break;
      case HWC2::Attribute::DpiY:
        // Dots per 1000 inches
        *value = mm_height ? (mode->v_display() * kUmPerInch) / mm_height : -1;
        break;
      default:
        *value = -1;
        return HWC2::Error::BadConfig;
    }
  }else{

    static const int32_t kUmPerInch = 25400;
    uint32_t mm_width = connector_->mm_width();
    uint32_t mm_height = connector_->mm_height();
    int w = ctx_.framebuffer_width;
    int h = ctx_.framebuffer_height;
    int vrefresh = ctx_.vrefresh;
    auto attribute = static_cast<HWC2::Attribute>(attribute_in);
    switch (attribute) {
      case HWC2::Attribute::Width:
        *value = w;
        break;
      case HWC2::Attribute::Height:
        *value = h;
        break;
      case HWC2::Attribute::VsyncPeriod:
        // in nanoseconds
        *value = 1000 * 1000 * 1000 / vrefresh;
        break;
      case HWC2::Attribute::DpiX:
        // Dots per 1000 inches
        *value = mm_width ? (w * kUmPerInch) / mm_width : -1;
        break;
      case HWC2::Attribute::DpiY:
        // Dots per 1000 inches
        *value = mm_height ? (h * kUmPerInch) / mm_height : -1;
        break;
      default:
        *value = -1;
        return HWC2::Error::BadConfig;
    }
  }
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                                     hwc2_config_t *configs) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  // Since this callback is normally invoked twice (once to get the count, and
  // once to populate configs), we don't really want to read the edid
  // redundantly. Instead, only update the modes on the first invocation. While
  // it's possible this will result in stale modes, it'll all come out in the
  // wash when we try to set the active config later.
  if (!configs) {
    if (!connector_->ModesReady()) {
      int ret = connector_->UpdateModes();
      if(ret){
        ALOGE("Failed to update display modes %d", ret);
        return HWC2::Error::BadDisplay;
      }
    }
  }
  if(ctx_.bStandardSwitchResolution){
    // Since the upper layers only look at vactive/hactive/refresh, height and
    // width, it doesn't differentiate interlaced from progressive and other
    // similar modes. Depending on the order of modes we return to SF, it could
    // end up choosing a suboptimal configuration and dropping the preferred
    // mode. To workaround this, don't offer interlaced modes to SF if there is
    // at least one non-interlaced alternative and only offer a single WxH@R
    // mode with at least the prefered mode from in DrmConnector::UpdateModes()

    // TODO: Remove the following block of code until AOSP handles all modes
    std::vector<DrmMode> sel_modes;

    // Add the preferred mode first to be sure it's not dropped
    auto mode = std::find_if(connector_->modes().begin(),
                             connector_->modes().end(), [&](DrmMode const &m) {
                               return m.id() ==
                                      connector_->get_preferred_mode_id();
                             });
    if (mode != connector_->modes().end())
      sel_modes.push_back(*mode);

    // Add the active mode if different from preferred mode
    if (connector_->active_mode().id() != connector_->get_preferred_mode_id())
      sel_modes.push_back(connector_->active_mode());

    // Cycle over the modes and filter out "similar" modes, keeping only the
    // first ones in the order given by DRM (from CEA ids and timings order)
    for (const DrmMode &mode : connector_->modes()) {
      // TODO: Remove this when 3D Attributes are in AOSP
      if (mode.flags() & DRM_MODE_FLAG_3D_MASK)
        continue;

      // TODO: Remove this when the Interlaced attribute is in AOSP
      if (mode.flags() & DRM_MODE_FLAG_INTERLACE) {
        auto m = std::find_if(connector_->modes().begin(),
                              connector_->modes().end(),
                              [&mode](DrmMode const &m) {
                                return !(m.flags() & DRM_MODE_FLAG_INTERLACE) &&
                                       m.h_display() == mode.h_display() &&
                                       m.v_display() == mode.v_display();
                              });
        if (m == connector_->modes().end())
          sel_modes.push_back(mode);

        continue;
      }

      // Search for a similar WxH@R mode in the filtered list and drop it if
      // another mode with the same WxH@R has already been selected
      // TODO: Remove this when AOSP handles duplicates modes
      auto m = std::find_if(sel_modes.begin(), sel_modes.end(),
                            [&mode](DrmMode const &m) {
                              return m.h_display() == mode.h_display() &&
                                     m.v_display() == mode.v_display() &&
                                     m.v_refresh() == mode.v_refresh();
                            });
      if (m == sel_modes.end())
        sel_modes.push_back(mode);
    }

    auto num_modes = static_cast<uint32_t>(sel_modes.size());
    if (!configs) {
      *num_configs = num_modes;
      return HWC2::Error::None;
    }

    uint32_t idx = 0;
    for (const DrmMode &mode : sel_modes) {
      if (idx >= *num_configs)
        break;
      configs[idx++] = mode.id();
    }

    sf_modes_.swap(sel_modes);

    *num_configs = idx;
  }else{

    UpdateDisplayMode();
    const DrmMode best_mode = connector_->best_mode();

    char framebuffer_size[PROPERTY_VALUE_MAX];
    uint32_t width = 0, height = 0 , vrefresh = 0;

    connector_->GetFramebufferInfo(handle_, &width, &height, &vrefresh);

    if (width && height) {
      ctx_.framebuffer_width = width;
      ctx_.framebuffer_height = height;
      ctx_.vrefresh = vrefresh ? vrefresh : 60;
    } else if (best_mode.h_display() && best_mode.v_display() && best_mode.v_refresh()) {
      ctx_.framebuffer_width = best_mode.h_display();
      ctx_.framebuffer_height = best_mode.v_display();
      ctx_.vrefresh = best_mode.v_refresh();
      /*
       * RK3588：Limit to 4096x2160 if large than 2160p
       * Other:  Limit to 1920x1080 if large than 2160p
       */
      if(isRK3588(resource_manager_->getSocId())){
        if (ctx_.framebuffer_height >= 2160 && ctx_.framebuffer_width >= ctx_.framebuffer_height) {
          ctx_.framebuffer_width = ctx_.framebuffer_width * (2160.0 / ctx_.framebuffer_height);
          ctx_.framebuffer_height = 2160;
        }
      }else{
        if (ctx_.framebuffer_height >= 2160 && ctx_.framebuffer_width >= ctx_.framebuffer_height) {
          ctx_.framebuffer_width = ctx_.framebuffer_width * (1080.0 / ctx_.framebuffer_height);
          ctx_.framebuffer_height = 1080;
        }
      }
    } else {
      ctx_.framebuffer_width = 1920;
      ctx_.framebuffer_height = 1080;
      ctx_.vrefresh = 60;
      ALOGE("Failed to find available display mode for display %" PRIu64 "\n", handle_);
    }

    ctx_.rel_xres = best_mode.h_display();
    ctx_.rel_yres = best_mode.v_display();

    // AFBC limit
    bool disable_afbdc = false;
    if(handle_ == HWC_DISPLAY_PRIMARY){
      if(isRK356x(resource_manager_->getSocId())){
        if(ctx_.framebuffer_width % 4 != 0){
          disable_afbdc = true;
          HWC2_ALOGI("RK356x primary framebuffer size %dx%d not support AFBC, to disable AFBC\n",
                      ctx_.framebuffer_width,ctx_.framebuffer_height);
        }
      }
      if(hwc_get_int_property("ro.vendor.rk_sdk","0") == 0){
          disable_afbdc = true;
          HWC2_ALOGI("Maybe GSI SDK, to disable AFBC\n");
      }
      if(disable_afbdc){
        property_set( "vendor.gralloc.no_afbc_for_fb_target_layer", "1");
      }
    }
    if (!configs) {
      *num_configs = 1;
      return HWC2::Error::None;
    }
    *num_configs = 1;
    configs[0] = 0;
  }

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayName(uint32_t *size, char *name) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  std::ostringstream stream;
  stream << "display-" << connector_->id();
  std::string string = stream.str();
  size_t length = string.length();
  if (!name) {
    *size = length;
    return HWC2::Error::None;
  }

  *size = std::min<uint32_t>(static_cast<uint32_t>(length - 1), *size);
  strncpy(name, string.c_str(), *size);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayRequests(int32_t *display_requests,
                                                      uint32_t *num_elements,
                                                      hwc2_layer_t *layers,
                                                      int32_t *layer_requests) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  // TODO: I think virtual display should request
  //      HWC2_DISPLAY_REQUEST_WRITE_CLIENT_TARGET_TO_OUTPUT here
  uint32_t num_request = 0;
  if(!client_layer_.isAfbc()){
    num_request++;
    if(display_requests){
      // RK: Reuse HWC2_DISPLAY_REQUEST_FLIP_CLIENT_TARGET definition to
      //     implement ClientTarget feature.
      *display_requests = HWC2_DISPLAY_REQUEST_FLIP_CLIENT_TARGET;
    }
  }else{
      *display_requests = 0;
  }

  if (!layers || !layer_requests)
    *num_elements = num_request;
  else{
    for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &l : layers_) {
      if (l.second.validated_type() == HWC2::Composition::Client) {
          layers[0] = l.first;
          layer_requests[0] = 0;
          break;
      }
    }
  }

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDisplayType(int32_t *type) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  *type = static_cast<int32_t>(type_);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetDozeSupport(int32_t *support) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  *support = 0;
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetHdrCapabilities(
    uint32_t *num_types, int32_t *types, float * max_luminance,
    float *max_average_luminance, float * min_luminance) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  int display = static_cast<int>(handle_);
  int HdrIndex = 0;

    if (!connector_) {
    ALOGE("%s:Failed to get connector for display %d line=%d", __FUNCTION__,display,__LINE__);
    return HWC2::Error::None;
  }
  if(!connector_->ModesReady()){
    int ret = connector_->UpdateModes();
    if (ret) {
      ALOGE("Failed to update display modes %d", ret);
      return HWC2::Error::None;
    }
  }
  const std::vector<DrmHdr> hdr_support_list = connector_->get_hdr_support_list();

  if(types == NULL){
      *num_types = hdr_support_list.size();
      return HWC2::Error::None;
  }

  for(const DrmHdr &hdr_mode : hdr_support_list){
      types[HdrIndex] = hdr_mode.drmHdrType;
      *max_luminance = hdr_mode.outMaxLuminance;
      *max_average_luminance = hdr_mode.outMaxAverageLuminance;
      *min_luminance = hdr_mode.outMinLuminance;
      HdrIndex++;
  }
  *num_types = hdr_support_list.size();

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::GetReleaseFences(uint32_t *num_elements,
                                                    hwc2_layer_t *layers,
                                                    int32_t *fences) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  uint32_t num_layers = 0;

  for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &l : layers_) {
    ++num_layers;
    if (layers == NULL || fences == NULL) {
      continue;
    } else if (num_layers > *num_elements) {
      ALOGW("Overflow num_elements %d/%d", num_layers, *num_elements);
      return HWC2::Error::None;
    }

    layers[num_layers - 1] = l.first;
    fences[num_layers - 1] = l.second.take_release_fence();
    ALOGV("rk-debug GetReleaseFences [%" PRIu64 "][%d]",layers[num_layers - 1],fences[num_layers - 1]);
    // the new fence semantics for a frame n by returning the fence from frame n-1. For frame 0,
    // the adapter returns NO_FENCE.
    l.second.manage_release_fence();
  }
  *num_elements = num_layers;
  return HWC2::Error::None;
}

void DrmHwcTwo::HwcDisplay::AddFenceToRetireFence(int fd) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  if (fd < 0){
    for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &hwc2layer : layers_) {

      // the new fence semantics for a frame n by returning the fence from frame n-1. For frame 0,
      // the adapter returns NO_FENCE.
      hwc2layer.second.manage_next_release_fence();

      int next_releaseFenceFd = hwc2layer.second.next_release_fence();
      if (next_releaseFenceFd < 0)
        continue;

      if (next_retire_fence_.get() >= 0) {
        int old_retire_fence = next_retire_fence_.get();
        next_retire_fence_.Set(sync_merge("dc_retire", old_retire_fence, next_releaseFenceFd));
      } else {
        next_retire_fence_.Set(dup(next_releaseFenceFd));
      }
    }
    client_layer_.manage_next_release_fence();
    int next_releaseFenceFd = client_layer_.next_release_fence();
    if(next_releaseFenceFd > 0){
      if (next_retire_fence_.get() >= 0) {
        int old_retire_fence = next_retire_fence_.get();
        next_retire_fence_.Set(sync_merge("dc_retire", old_retire_fence, next_releaseFenceFd));
      } else {
        next_retire_fence_.Set(dup(next_releaseFenceFd));
      }
    }
    return;
  }else{
    if (next_retire_fence_.get() >= 0) {
      int old_fence = next_retire_fence_.get();
      next_retire_fence_.Set(sync_merge("dc_retire", old_fence, fd));
    } else {
      next_retire_fence_.Set(dup(fd));
    }
  }
}

bool SortByZpos(const DrmHwcLayer &drmHwcLayer1, const DrmHwcLayer &drmHwcLayer2){
    return drmHwcLayer1.iZpos_ < drmHwcLayer2.iZpos_;
}
HWC2::Error DrmHwcTwo::HwcDisplay::InitDrmHwcLayer() {
  drm_hwc_layers_.clear();

  // now that they're ordered by z, add them to the composition
  for (auto &hwc2layer : layers_) {
    DrmHwcLayer drmHwclayer;
    hwc2layer.second.PopulateDrmLayer(hwc2layer.first, &drmHwclayer, &ctx_, frame_no_);
    drm_hwc_layers_.emplace_back(std::move(drmHwclayer));
  }

  std::sort(drm_hwc_layers_.begin(),drm_hwc_layers_.end(),SortByZpos);

  uint32_t client_id = 0;
  DrmHwcLayer client_target_layer;
  client_layer_.PopulateFB(client_id, &client_target_layer, &ctx_, frame_no_, true);
  drm_hwc_layers_.emplace_back(std::move(client_target_layer));

  ALOGD_HWC2_DRM_LAYER_INFO((DBG_INFO),drm_hwc_layers_);

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::ValidatePlanes() {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  int ret;

  InitDrmHwcLayer();

  std::vector<DrmHwcLayer *> layers;
  layers.reserve(drm_hwc_layers_.size());
  for(size_t i = 0; i < drm_hwc_layers_.size(); ++i){
      layers.push_back(&drm_hwc_layers_[i]);
  }

  std::tie(ret,
           composition_planes_) = planner_->TryHwcPolicy(layers, crtc_, static_screen_opt_ || force_gles_);
  if (ret){
    ALOGE("First, GLES policy fail ret=%d", ret);
    return HWC2::Error::BadConfig;
  }

  for (auto &drm_hwc_layer : drm_hwc_layers_) {
    if(drm_hwc_layer.bFbTarget_){
      if(drm_hwc_layer.bAfbcd_)
        client_layer_.EnableAfbc();
      else
        client_layer_.DisableAfbc();
      continue;
    }
    if(drm_hwc_layer.bMatch_){
      auto map_hwc2layer = layers_.find(drm_hwc_layer.uId_);
      map_hwc2layer->second.set_validated_type(HWC2::Composition::Device);
      ALOGD_IF(LogLevel(DBG_INFO),"[%.4" PRIu32 "]=Device : %s",drm_hwc_layer.uId_,drm_hwc_layer.sLayerName_.c_str());
    }else{
      auto map_hwc2layer = layers_.find(drm_hwc_layer.uId_);
      map_hwc2layer->second.set_validated_type(HWC2::Composition::Client);
      ALOGD_IF(LogLevel(DBG_INFO),"[%.4" PRIu32 "]=Client : %s",drm_hwc_layer.uId_,drm_hwc_layer.sLayerName_.c_str());
    }
  }

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::CreateComposition() {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  int ret;
  std::vector<DrmCompositionDisplayLayersMap> layers_map;
  layers_map.emplace_back();
  DrmCompositionDisplayLayersMap &map = layers_map.back();

  map.display = static_cast<int>(handle_);
  map.geometry_changed = true;  // TODO: Fix this

  bool use_client_layer = false;
  for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &l : layers_)
    if(l.second.sf_type() == HWC2::Composition::Client)
      use_client_layer = true;

  for (auto &drm_hwc_layer : drm_hwc_layers_) {
    if(!use_client_layer && drm_hwc_layer.bFbTarget_)
      continue;
    if(!drm_hwc_layer.bMatch_)
      continue;
    if(drm_hwc_layer.bFbTarget_){
      uint32_t client_id = 0;
      client_layer_.PopulateFB(client_id, &drm_hwc_layer, &ctx_, frame_no_, false);
    }
    ret = drm_hwc_layer.ImportBuffer(importer_.get());
    if (ret) {
      ALOGE("Failed to import layer, ret=%d", ret);
      return HWC2::Error::NoResources;
    }
    map.layers.emplace_back(std::move(drm_hwc_layer));
  }

  std::unique_ptr<DrmDisplayComposition> composition = compositor_
                                                         .CreateComposition();
  composition->Init(drm_, crtc_, importer_.get(), planner_.get(), frame_no_);

  // TODO: Don't always assume geometry changed
  ret = composition->SetLayers(map.layers.data(), map.layers.size(), true);
  if (ret) {
    ALOGE("Failed to set layers in the composition ret=%d", ret);
    return HWC2::Error::BadLayer;
  }
  for(auto &composition_plane :composition_planes_)
    ret = composition->AddPlaneComposition(std::move(composition_plane));

  ret = composition->DisableUnusedPlanes();
  if (ret) {
    ALOGE("Failed to plan the composition ret=%d", ret);
    return HWC2::Error::BadConfig;
  }

  // 利用 vendor.hwc.disable_releaseFence 属性强制关闭ReleaseFence，主要用于调试
  char value[PROPERTY_VALUE_MAX];
  property_get("vendor.hwc.disable_releaseFence", value, "0");
  if(atoi(value) == 0){
    ret = composition->CreateAndAssignReleaseFences();
    AddFenceToRetireFence(composition->take_out_fence());
  }

  ret = compositor_.QueueComposition(std::move(composition));

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::PresentDisplay(int32_t *retire_fence) {
  ATRACE_CALL();

  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  DumpAllLayerData();

  HWC2::Error ret;
  ret = CheckDisplayState();
  if(ret != HWC2::Error::None || !validate_success_){
    ALOGE_IF(LogLevel(DBG_ERROR),"Check display %" PRIu64 " state fail %s, %s,line=%d", handle_,
          validate_success_? "" : "or validate fail.",__FUNCTION__, __LINE__);
    ClearDisplay();
    *retire_fence = -1;
    return HWC2::Error::None;
  }else{
    ret = CreateComposition();
    if (ret == HWC2::Error::BadLayer) {
      // Can we really have no client or device layers?
      *retire_fence = -1;
      return HWC2::Error::None;
    }
    if (ret != HWC2::Error::None)
      return ret;
  }

  // The retire fence returned here is for the last frame, so return it and
  // promote the next retire fence
  *retire_fence = retire_fence_.Release();
  retire_fence_ = std::move(next_retire_fence_);

  ++frame_no_;

  UpdateTimerState(!static_screen_opt_);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetActiveConfig(hwc2_config_t config) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 " config=%d",handle_,config);
  if(ctx_.bStandardSwitchResolution){
    auto mode = std::find_if(connector_->modes().begin(),
                             connector_->modes().end(),
                             [config](DrmMode const &m) {
                               return m.id() == config;
                             });
    if (mode == connector_->modes().end()) {
      ALOGE("Could not find active mode for %d", config);
      return HWC2::Error::BadConfig;
    }

  //  std::unique_ptr<DrmDisplayComposition> composition = compositor_
  //                                                           .CreateComposition();
  //  composition->Init(drm_, crtc_, importer_.get(), planner_.get(), frame_no_);
  //  int ret = composition->SetDisplayMode(*mode);
  //  ret = compositor_.QueueComposition(std::move(composition));
  //  if (ret) {
  //    ALOGE("Failed to queue dpms composition on %d", ret);
  //    return HWC2::Error::BadConfig;
  //  }
    connector_->set_best_mode(*mode);

    connector_->set_current_mode(*mode);
    ctx_.rel_xres = (*mode).h_display();
    ctx_.rel_yres = (*mode).v_display();

    // Setup the client layer's dimensions
    hwc_rect_t display_frame = {.left = 0,
                                .top = 0,
                                .right = static_cast<int>(mode->h_display()),
                                .bottom = static_cast<int>(mode->v_display())};
    client_layer_.SetLayerDisplayFrame(display_frame);
    hwc_frect_t source_crop = {.left = 0.0f,
                               .top = 0.0f,
                               .right = mode->h_display() + 0.0f,
                               .bottom = mode->v_display() + 0.0f};
    client_layer_.SetLayerSourceCrop(source_crop);

    drm_->UpdateDisplayMode(handle_);
    // SetDisplayModeInfo cost 2.5ms - 5ms, a A few cases cost 10ms - 20ms
    connector_->SetDisplayModeInfo(handle_);
  }else{

    // Setup the client layer's dimensions
    hwc_rect_t display_frame = {.left = 0,
                                .top = 0,
                                .right = static_cast<int>(ctx_.framebuffer_width),
                                .bottom = static_cast<int>(ctx_.framebuffer_height)};
    client_layer_.SetLayerDisplayFrame(display_frame);
    hwc_frect_t source_crop = {.left = 0.0f,
                               .top = 0.0f,
                               .right = ctx_.framebuffer_width + 0.0f,
                               .bottom = ctx_.framebuffer_height + 0.0f};
    client_layer_.SetLayerSourceCrop(source_crop);

  }

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetClientTarget(buffer_handle_t target,
                                                   int32_t acquire_fence,
                                                   int32_t dataspace,
                                                   hwc_region_t /*damage*/) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", Buffer=%p, acq_fence=%d, dataspace=%x",
                         handle_,target,acquire_fence,dataspace);
  UniqueFd uf(acquire_fence);

  client_layer_.set_buffer(target);
  client_layer_.set_acquire_fence(uf.get());
  client_layer_.SetLayerDataspace(dataspace);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetColorMode(int32_t mode) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", mode=%x",handle_,mode);

  if (mode != HAL_COLOR_MODE_NATIVE)
    return HWC2::Error::BadParameter;

  color_mode_ = mode;
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetColorTransform(const float *matrix,
                                                     int32_t hint) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", hint=%x",handle_,hint);
  // TODO: Force client composition if we get this
  // hint definition from android_color_transform_t in system/core/libsystem/include/system/graphics-base-v1.0.h
  force_gles_ = (hint > 0);
  unsupported(__func__, matrix, hint);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetOutputBuffer(buffer_handle_t buffer,
                                                   int32_t release_fence) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", buffer=%p, rel_fence=%d",handle_,buffer,release_fence);
  // TODO: Need virtual display support
  return unsupported(__func__, buffer, release_fence);
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetPowerMode(int32_t mode_in) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", mode_in=%d",handle_,mode_in);

  uint64_t dpms_value = 0;
  auto mode = static_cast<HWC2::PowerMode>(mode_in);
  switch (mode) {
    case HWC2::PowerMode::Off:
      dpms_value = DRM_MODE_DPMS_OFF;
      break;
    case HWC2::PowerMode::On:
      dpms_value = DRM_MODE_DPMS_ON;
      break;
    case HWC2::PowerMode::Doze:
    case HWC2::PowerMode::DozeSuspend:
      ALOGI("Power mode %d is unsupported\n", mode);
      return HWC2::Error::Unsupported;
    default:
      ALOGI("Power mode %d is BadParameter\n", mode);
      return HWC2::Error::BadParameter;
  };

  std::unique_ptr<DrmDisplayComposition> composition = compositor_
                                                           .CreateComposition();
  composition->Init(drm_, crtc_, importer_.get(), planner_.get(), frame_no_);
  composition->SetDpmsMode(dpms_value);
  int ret = compositor_.QueueComposition(std::move(composition));
  if (ret) {
    ALOGE("Failed to apply the dpms composition ret=%d", ret);
    return HWC2::Error::BadParameter;
  }

  int fb0_fd = resource_manager_->getFb0Fd();
  if(fb0_fd<=0)
    ALOGE_IF(LogLevel(DBG_ERROR),"%s,line=%d fb0_fd = %d can't operation /dev/graphics/fb0 node.",
              __FUNCTION__,__LINE__,fb0_fd);
  int fb_blank = 0;
  if(dpms_value == DRM_MODE_DPMS_OFF)
    fb_blank = FB_BLANK_POWERDOWN;
  else if(dpms_value == DRM_MODE_DPMS_ON)
    fb_blank = FB_BLANK_UNBLANK;
  else
    ALOGE("dpmsValue is invalid value= %" PRIu64 "",dpms_value);
  if(fb_blank != fb_blanked && fb0_fd > 0){
    int err = ioctl(fb0_fd, FBIOBLANK, fb_blank);
    ALOGD_IF(LogLevel(DBG_DEBUG),"%s Notice fb_blank to fb=%d", __FUNCTION__, fb_blank);
    if (err < 0) {
      ALOGE("fb_blank ioctl failed(%d) display=%" PRIu64 ",fb_blank=%d,dpmsValue=%" PRIu64 "",
          errno,handle_,fb_blank,dpms_value);
    }
  }

  fb_blanked = fb_blank;

  if(dpms_value == DRM_MODE_DPMS_OFF){
    ClearDisplay();
    int ret = drm_->ReleaseDpyRes(handle_);
    if (ret) {
      HWC2_ALOGE("Failed to ReleaseDpyRes for display=%" PRIu64 " %d\n", handle_, ret);
    }
    if(isRK3566(resource_manager_->getSocId())){
      int display_id = drm_->GetCommitMirrorDisplayId();
      DrmConnector *extend = drm_->GetConnectorForDisplay(display_id);
      if(extend != NULL){
        int extend_display_id = extend->display();
        auto &display = g_ctx->displays_.at(extend_display_id);
        display.ClearDisplay();
        ret = drm_->ReleaseDpyRes(extend_display_id);
        if (ret) {
          HWC2_ALOGE("Failed to ReleaseDpyRes for display=%d %d\n", extend_display_id, ret);
        }
      }
    }
  }else{
    int ret = drm_->BindDpyRes(handle_);
    if (ret) {
      HWC2_ALOGE("Failed to BindDpyRes for display=%" PRIu64 " ret=%d\n", handle_, ret);
    }
    if(isRK3566(resource_manager_->getSocId())){
      ALOGD_IF(LogLevel(DBG_DEBUG),"SetPowerMode display-id=%" PRIu64 ",soc is rk3566" ,handle_);
      int display_id = drm_->GetCommitMirrorDisplayId();
      DrmConnector *extend = drm_->GetConnectorForDisplay(display_id);
      if(extend != NULL){
        int extend_display_id = extend->display();
        ret = drm_->BindDpyRes(extend_display_id);
        if (ret) {
          HWC2_ALOGE("Failed to BindDpyRes for display=%d ret=%d\n", extend_display_id, ret);
        }
      }
    }
  }
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetVsyncEnabled(int32_t enabled) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", enable=%d",handle_,enabled);
  vsync_worker_.VSyncControl(HWC2_VSYNC_ENABLE == enabled);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::ValidateDisplay(uint32_t *num_types,
                                                   uint32_t *num_requests) {
  ATRACE_CALL();
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ,handle_);
  // Enable/disable debug log
  UpdateLogLevel();
  UpdateBCSH();
  UpdateHdmiOutputFormat();
  UpdateOverscan();
  if(!ctx_.bStandardSwitchResolution){
    UpdateDisplayMode();
    drm_->UpdateDisplayMode(handle_);
    if(isRK3566(resource_manager_->getSocId())){
      int display_id = drm_->GetCommitMirrorDisplayId();
      drm_->UpdateDisplayMode(display_id);
    }
  }

  *num_types = 0;
  *num_requests = 0;

  HWC2::Error ret;

  if(LogLevel(DBG_INFO)){
    DumpDisplayLayersInfo();
  }

  for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &l : layers_)
    l.second.set_validated_type(HWC2::Composition::Invalid);

  ret = CheckDisplayState();
  if(ret != HWC2::Error::None){
    ALOGE_IF(LogLevel(DBG_ERROR),"Check display %" PRIu64 " state fail, %s,line=%d", handle_,
          __FUNCTION__, __LINE__);
    ClearDisplay();
    composition_planes_.clear();
    validate_success_ = false;
    return HWC2::Error::None;
  }

  ret = ValidatePlanes();
  if (ret != HWC2::Error::None){
    ALOGE("%s fail , ret = %d,line = %d",__FUNCTION__,ret,__LINE__);
    return HWC2::Error::BadConfig;
  }

  SwitchHdrMode();
  // Static screen opt
  UpdateTimerEnable();

  for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &l : layers_) {
    DrmHwcTwo::HwcLayer &layer = l.second;
    // We can only handle layers of Device type, send everything else to SF
    if (layer.validated_type() != HWC2::Composition::Device) {
      layer.set_validated_type(HWC2::Composition::Client);
      ++*num_types;
    }
  }

  if(!client_layer_.isAfbc()){
    ++(*num_requests);
  }
  validate_success_ = true;
  return *num_types ? HWC2::Error::HasChanges : HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetCursorPosition(int32_t x, int32_t y) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d"", x=%d, y=%d" ,id_,x,y);
  cursor_x_ = x;
  cursor_y_ = y;
  return HWC2::Error::None;
}

int DrmHwcTwo::HwcDisplay::DumpDisplayInfo(String8 &output){

  output.appendFormat(" DisplayId=%" PRIu64 ", Connector %u, Type = %s-%u, Connector state = %s\n",handle_,
                        connector_->id(),drm_->connector_type_str(connector_->type()),connector_->type_id(),
                        connector_->state() == DRM_MODE_CONNECTED ? "DRM_MODE_CONNECTED" : "DRM_MODE_DISCONNECTED");

  if(connector_->state() != DRM_MODE_CONNECTED)
    return -1;

  DrmMode const &active_mode = connector_->active_mode();
  if (active_mode.id() == 0){
    return -1;
  }

  output.appendFormat("  NumHwLayers=%zu, activeModeId=%u, %s%c%.2f, colorMode = %d, bStandardSwitchResolution=%d\n",
                        get_layers().size(),
                        active_mode.id(), active_mode.name().c_str(),'p' ,active_mode.v_refresh(),
                        color_mode_,ctx_.bStandardSwitchResolution);
  uint32_t idx = 0;

  if(sf_modes_.size() > 0){
    for (const DrmMode &mode : sf_modes_) {
      if(active_mode.id() == mode.id())
        output.appendFormat("    Config[%2u] = %s%c%.2f mode-id=%d (active)\n",idx, mode.name().c_str(), 'p' , mode.v_refresh(),mode.id());
      else
        output.appendFormat("    Config[%2u] = %s%c%.2f mode-id=%d \n",idx, mode.name().c_str(), 'p' , mode.v_refresh(),mode.id());
      idx++;
    }
  }

  output.append(
              "------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------\n"
              "  id  |  z  |  sf-type  |  hwc-type |       handle       |  transform  |    blnd    |     source crop (l,t,r,b)      |          frame         | dataspace  | name\n"
              "------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------\n");
  for (uint32_t z_order = 0; z_order <= layers_.size(); z_order++) {
    for (auto &map_layer : layers_) {
      HwcLayer &layer = map_layer.second;
      if(layer.z_order() == z_order){
        layer.DumpLayerInfo(output);
        break;
      }
    }
  }

  output.append("------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------\n");
  output.append("DrmHwcLayer Dump:\n");

  for(auto &drmHwcLayer : drm_hwc_layers_)
      drmHwcLayer.DumpInfo(output);

  return 0;
}

int DrmHwcTwo::HwcDisplay::DumpDisplayLayersInfo(String8 &output){

  output.appendFormat(" DisplayId=%" PRIu64 ", Connector %u, Type = %s-%u, Connector state = %s , frame_no = %d\n",handle_,
                        connector_->id(),drm_->connector_type_str(connector_->type()),connector_->type_id(),
                        connector_->state() == DRM_MODE_CONNECTED ? "DRM_MODE_CONNECTED" : "DRM_MODE_DISCONNECTED",
                        frame_no_);

  output.append(
              "------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------\n"
              "  id  |  z  |  req-type | fina-type |       handle       |  transform  |    blnd    |     source crop (l,t,r,b)      |          frame         | dataspace  | name       \n"
              "------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------\n");
  for (uint32_t z_order = 0; z_order <= layers_.size(); z_order++) {
    for (auto &map_layer : layers_) {
      HwcLayer &layer = map_layer.second;
      if(layer.z_order() == z_order){
        layer.DumpLayerInfo(output);
        break;
      }
    }
  }
  output.append("------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------\n");
  return 0;
}

int DrmHwcTwo::HwcDisplay::DumpDisplayLayersInfo(){
  String8 output;
  output.appendFormat(" DisplayId=%" PRIu64 ", Connector %u, Type = %s-%u, Connector state = %s , frame_no = %d\n",handle_,
                        connector_->id(),drm_->connector_type_str(connector_->type()),connector_->type_id(),
                        connector_->state() == DRM_MODE_CONNECTED ? "DRM_MODE_CONNECTED" : "DRM_MODE_DISCONNECTED",
                        frame_no_);

  output.append(
              "------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------\n"
              "  id  |  z  |  sf-type  |  hwc-type |       handle       |  transform  |    blnd    |     source crop (l,t,r,b)      |          frame         | dataspace  | name       \n"
              "------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------\n");
  ALOGD("%s",output.string());
  for (uint32_t z_order = 0; z_order <= layers_.size(); z_order++) {
    for (auto &map_layer : layers_) {
      HwcLayer &layer = map_layer.second;
      if(layer.z_order() == z_order){
        output.clear();
        layer.DumpLayerInfo(output);
        ALOGD("%s",output.string());
        break;
      }
    }
  }
  output.clear();
  output.append("------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------\n");
  ALOGD("%s",output.string());
  return 0;
}
int DrmHwcTwo::HwcDisplay::DumpAllLayerData(){
  int ret = 0;
  char pro_value[PROPERTY_VALUE_MAX];
  property_get( PROPERTY_TYPE ".dump",pro_value,0);
  if(!strcmp(pro_value,"true")){
    for (auto &map_layer : layers_) {
      HwcLayer &layer = map_layer.second;
      layer.DumpData();
    }
    if(client_layer_.buffer() != NULL)
      client_layer_.DumpData();
  }
  return 0;
}

int DrmHwcTwo::HwcDisplay::HoplugEventTmeline(){
  ctx_.hotplug_timeline++;
  return 0;
}

int DrmHwcTwo::HwcDisplay::UpdateDisplayMode(){

  if(!ctx_.bStandardSwitchResolution){
    int timeline;
    int display_id = static_cast<int>(handle_);
    timeline = property_get_int32("vendor.display.timeline", -1);
    if(timeline && timeline == ctx_.display_timeline && ctx_.hotplug_timeline == drm_->timeline())
      return 0;
    ctx_.display_timeline = timeline;
    ctx_.hotplug_timeline = drm_->timeline();
    int ret = connector_->UpdateDisplayMode(display_id, timeline);
    if(!ret){
      const DrmMode best_mode = connector_->best_mode();
      connector_->set_current_mode(best_mode);
      ctx_.rel_xres = best_mode.h_display();
      ctx_.rel_yres = best_mode.v_display();
      ctx_.dclk = best_mode.clock();
    }

    if(isRK3566(resource_manager_->getSocId())){
      bool mirror_mode = true;
      int display_id = drm_->GetCommitMirrorDisplayId();
      DrmConnector *conn_mirror = drm_->GetConnectorForDisplay(display_id);
      if(!conn_mirror || conn_mirror->state() != DRM_MODE_CONNECTED){
        ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d disable bCommitMirrorMode",__FUNCTION__,__LINE__);
        mirror_mode = false;
      }

      if(mirror_mode){
        int ret = conn_mirror->UpdateDisplayMode(display_id, timeline);
        if(!ret){
          const DrmMode best_mode = conn_mirror->best_mode();
          conn_mirror->set_current_mode(best_mode);
        }
      }
    }
  }
  return 0;
}

int DrmHwcTwo::HwcDisplay::UpdateOverscan(){
  // RK3588 没有Overscan模块，所以需要用图层Scale实现Overscan效果
  if(isRK3588(resource_manager_->getSocId()))
    connector_->UpdateOverscan(handle_, ctx_.overscan_value);
  else
    ;// do notiong.
  return 0;
}

int DrmHwcTwo::HwcDisplay::UpdateHdmiOutputFormat(){
  int timeline = 0;
  int ret = 0;
  char prop_format[PROPERTY_VALUE_MAX];
  timeline = property_get_int32( "vendor.display.timeline", -1);
  /*
   * force update propetry when timeline is zero or not exist.
   */
  if (timeline && timeline == ctx_.display_timeline && ctx_.hotplug_timeline == drm_->timeline())
      return 0;

  connector_->UpdateOutputFormat(handle_, timeline);

  if(isRK3566(resource_manager_->getSocId())){
    bool mirror_mode = true;
    int display_id = drm_->GetCommitMirrorDisplayId();
    DrmConnector *conn_mirror = drm_->GetConnectorForDisplay(display_id);
    if(!conn_mirror || conn_mirror->state() != DRM_MODE_CONNECTED){
      ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d disable bCommitMirrorMode",__FUNCTION__,__LINE__);
      mirror_mode = false;
    }

    if(mirror_mode){
      conn_mirror->UpdateOutputFormat(display_id, timeline);
    }
  }

  return 0;
}

int DrmHwcTwo::HwcDisplay::UpdateBCSH(){

  int timeline = property_get_int32("vendor.display.timeline", -1);
  int ret;
  /*
   * force update propetry when timeline is zero or not exist.
   */
  if (timeline && timeline == ctx_.bcsh_timeline)
    return 0;
  connector_->UpdateBCSH(handle_,timeline);

  if(isRK3566(resource_manager_->getSocId())){
    bool mirror_mode = true;
    int display_id = drm_->GetCommitMirrorDisplayId();
    DrmConnector *conn_mirror = drm_->GetConnectorForDisplay(display_id);
    if(!conn_mirror || conn_mirror->state() != DRM_MODE_CONNECTED){
      ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d disable bCommitMirrorMode",__FUNCTION__,__LINE__);
      mirror_mode = false;
    }
    if(mirror_mode){
      conn_mirror->UpdateBCSH(display_id,timeline);
    }
  }

  ctx_.bcsh_timeline = timeline;

  return 0;
}

int DrmHwcTwo::HwcDisplay::SwitchHdrMode(){
  bool exist_hdr_layer = false;
  for(auto &drmHwcLayer : drm_hwc_layers_)
    if(drmHwcLayer.bHdr_){
      if(connector_->is_hdmi_support_hdr()){
        exist_hdr_layer = true;
        if(!ctx_.hdr_mode && !connector_->switch_hdmi_hdr_mode(drmHwcLayer.eDataSpace_)){
          ALOGD_IF(LogLevel(DBG_DEBUG),"Enable HDR mode success");
          ctx_.hdr_mode = true;
          property_set("vendor.hwc.hdr_state","HDR");
        }
      }
  }

  if(!exist_hdr_layer && ctx_.hdr_mode){
    if(!connector_->switch_hdmi_hdr_mode(HAL_DATASPACE_UNKNOWN)){
      ALOGD_IF(LogLevel(DBG_DEBUG),"Exit HDR mode success");
      ctx_.hdr_mode = false;
      property_set("vendor.hwc.hdr_state","NORMAL");
    }
  }

  return 0;
}

int DrmHwcTwo::HwcDisplay::UpdateTimerEnable(){
  bool enable_timer = true;
  for(auto &drmHwcLayer : drm_hwc_layers_){
    // Video
    if(drmHwcLayer.bYuv_){
      enable_timer = false;
      break;
    }

    // Surface w/h is larger than FB
    int crop_w = static_cast<int>(drmHwcLayer.source_crop.right - drmHwcLayer.source_crop.left);
    int crop_h = static_cast<int>(drmHwcLayer.source_crop.bottom - drmHwcLayer.source_crop.top);
    if(crop_w * crop_h > ctx_.framebuffer_width * ctx_.framebuffer_height){
      enable_timer = false;
      break;
    }
  }
  static_screen_timer_enable_ = enable_timer;
  ALOGD_IF(LogLevel(DBG_DEBUG),"%s timer!",static_screen_timer_enable_ ? "Enable" : "Disable");
  return 0;
}
int DrmHwcTwo::HwcDisplay::UpdateTimerState(bool gles_comp){
    struct itimerval tv = {{0,0},{0,0}};

    if (static_screen_timer_enable_ && gles_comp) {
        int interval_value = hwc_get_int_property( "vendor.hwc.static_screen_opt_time", "2500");
        interval_value = interval_value > 5000? 5000:interval_value;
        interval_value = interval_value < 250? 250:interval_value;
        tv.it_value.tv_sec = interval_value / 1000;
        tv.it_value.tv_usec=( interval_value % 1000) * 1000;
        ALOGD_IF(LogLevel(DBG_DEBUG),"reset timer! interval_value = %d",interval_value);
    } else {
        static_screen_opt_=false;
        tv.it_value.tv_usec = 0;
        ALOGD_IF(LogLevel(DBG_DEBUG),"close timer!");
    }
    setitimer(ITIMER_REAL, &tv, NULL);
    return 0;
}

int DrmHwcTwo::HwcDisplay::EntreStaticScreen(uint64_t refresh, int refresh_cnt){
    static_screen_opt_=true;
    invalidate_worker_.InvalidateControl(refresh, refresh_cnt);
    return 0;
}

int DrmHwcTwo::HwcDisplay::InvalidateControl(uint64_t refresh, int refresh_cnt){
    invalidate_worker_.InvalidateControl(refresh, refresh_cnt);
    return 0;
}


HWC2::Error DrmHwcTwo::HwcLayer::SetLayerBlendMode(int32_t mode) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d"", blend=%d" ,id_,mode);
  blending_ = static_cast<HWC2::BlendMode>(mode);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerBuffer(buffer_handle_t buffer,
                                                int32_t acquire_fence) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d"", buffer=%p, acq_fence=%d" ,id_,buffer,acquire_fence);
  UniqueFd uf(acquire_fence);

  //Deleting the following logic may cause the problem that the handle cannot be updated
  // The buffer and acquire_fence are handled elsewhere
//  if (sf_type_ == HWC2::Composition::Client ||
//      sf_type_ == HWC2::Composition::Sideband ||
//      sf_type_ == HWC2::Composition::SolidColor)
//    return HWC2::Error::None;

  set_buffer(buffer);
  set_acquire_fence(uf.get());
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerColor(hwc_color_t color) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d"", color [r,g,b,a]=[%d,%d,%d,%d]" ,id_,color.r,color.g,color.b,color.a);
  // TODO: Punt to client composition here?
  unsupported(__func__, color);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerCompositionType(int32_t type) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d"", type=0x%x" ,id_,type);
  sf_type_ = static_cast<HWC2::Composition>(type);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerDataspace(int32_t dataspace) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d"", dataspace=0x%x" ,id_,dataspace);
  dataspace_ = static_cast<android_dataspace_t>(dataspace);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerDisplayFrame(hwc_rect_t frame) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d"", frame=[%d,%d,%d,%d]" ,id_,frame.left,frame.top,frame.right,frame.bottom);
  display_frame_ = frame;
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerPlaneAlpha(float alpha) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d"", alpha=%f" ,id_,alpha);
  alpha_ = alpha;
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerSidebandStream(
    const native_handle_t *stream) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d",id_);
  // TODO: We don't support sideband
  return unsupported(__func__, stream);
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerSourceCrop(hwc_frect_t crop) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d"", frame=[%f,%f,%f,%f]" ,id_,crop.left,crop.top,crop.right,crop.bottom);
  source_crop_ = crop;
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerSurfaceDamage(hwc_region_t damage) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d",id_);
  // TODO: We don't use surface damage, marking as unsupported
  unsupported(__func__, damage);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerTransform(int32_t transform) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d" ", transform=%x",id_,transform);
  transform_ = static_cast<HWC2::Transform>(transform);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerVisibleRegion(hwc_region_t visible) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d",id_);
  // TODO: We don't use this information, marking as unsupported
  unsupported(__func__, visible);
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcLayer::SetLayerZOrder(uint32_t order) {
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d" ", z=%d",id_,order);
  z_order_ = order;
  return HWC2::Error::None;
}

void DrmHwcTwo::HwcLayer::PopulateDrmLayer(hwc2_layer_t layer_id, DrmHwcLayer *drmHwcLayer,
                                                 hwc2_drm_display_t* ctx, uint32_t frame_no) {
  drmHwcLayer->uId_        = layer_id;
  drmHwcLayer->iZpos_      = z_order_;
  drmHwcLayer->uFrameNo_   = frame_no;
  drmHwcLayer->bFbTarget_  = false;
  drmHwcLayer->bSkipLayer_ = false;
  drmHwcLayer->bUse_       = true;
  drmHwcLayer->eDataSpace_ = dataspace_;
  drmHwcLayer->alpha       = static_cast<uint16_t>(255.0f * alpha_ + 0.5f);
  drmHwcLayer->sf_composition = sf_type();

  OutputFd release_fence     = release_fence_output();
  drmHwcLayer->sf_handle     = buffer_;
  drmHwcLayer->acquire_fence = acquire_fence_.Release();
  drmHwcLayer->release_fence = std::move(release_fence);

  drmHwcLayer->iFbWidth_ = ctx->framebuffer_width;
  drmHwcLayer->iFbHeight_ = ctx->framebuffer_height;

  drmHwcLayer->uAclk_ = ctx->aclk;
  drmHwcLayer->uDclk_ = ctx->dclk;

  drmHwcLayer->SetBlend(blending_);
  drmHwcLayer->SetDisplayFrame(display_frame_, ctx);
  drmHwcLayer->SetSourceCrop(source_crop_);
  drmHwcLayer->SetTransform(transform_);

  // Commit mirror function
  drmHwcLayer->SetDisplayFrameMirror(display_frame_);

  if(buffer_){
    drmHwcLayer->iFd_     = pBufferInfo_->iFd_;
    drmHwcLayer->iWidth_  = pBufferInfo_->iWidth_;
    drmHwcLayer->iHeight_ = pBufferInfo_->iHeight_;
    drmHwcLayer->iStride_ = pBufferInfo_->iStride_;
    drmHwcLayer->iFormat_ = pBufferInfo_->iFormat_;
    drmHwcLayer->iUsage   = pBufferInfo_->iUsage_;
    drmHwcLayer->iByteStride_     = pBufferInfo_->iByteStride_;
    drmHwcLayer->uFourccFormat_   = pBufferInfo_->uFourccFormat_;
    drmHwcLayer->uModifier_       = pBufferInfo_->uModifier_;
    drmHwcLayer->sLayerName_      = pBufferInfo_->sLayerName_;
    drmHwcLayer->uGemHandle_      = pBufferInfo_->uGemHandle_;
  }else{
    drmHwcLayer->iFd_     = -1;
    drmHwcLayer->iWidth_  = -1;
    drmHwcLayer->iHeight_ = -1;
    drmHwcLayer->iStride_ = -1;
    drmHwcLayer->iFormat_ = -1;
    drmHwcLayer->iUsage   = -1;
    drmHwcLayer->uFourccFormat_   = 0x20202020; //0x20 is space
    drmHwcLayer->uModifier_ = 0;
    drmHwcLayer->uGemHandle_ = 0;
    drmHwcLayer->sLayerName_.clear();
  }

  drmHwcLayer->Init();

  return;
}

void DrmHwcTwo::HwcLayer::PopulateFB(hwc2_layer_t layer_id, DrmHwcLayer *drmHwcLayer,
                                         hwc2_drm_display_t* ctx, uint32_t frame_no, bool validate) {
  drmHwcLayer->uId_        = layer_id;
  drmHwcLayer->uFrameNo_   = frame_no;
  drmHwcLayer->bFbTarget_  = true;
  drmHwcLayer->bUse_       = true;
  drmHwcLayer->bSkipLayer_ = false;
  drmHwcLayer->blending    = DrmHwcBlending::kPreMult;
  drmHwcLayer->iZpos_      = z_order_;
  drmHwcLayer->alpha       = static_cast<uint16_t>(255.0f * alpha_ + 0.5f);

  if(!validate){
    OutputFd release_fence     = release_fence_output();
    drmHwcLayer->sf_handle     = buffer_;
    drmHwcLayer->acquire_fence = acquire_fence_.Release();
    drmHwcLayer->release_fence = std::move(release_fence);
  }else{
    // Commit mirror function
    drmHwcLayer->SetDisplayFrameMirror(display_frame_);
  }

  drmHwcLayer->iFbWidth_ = ctx->framebuffer_width;
  drmHwcLayer->iFbHeight_ = ctx->framebuffer_height;

  drmHwcLayer->uAclk_ = ctx->aclk;
  drmHwcLayer->uDclk_ = ctx->dclk;

  drmHwcLayer->SetDisplayFrame(display_frame_, ctx);
  drmHwcLayer->SetSourceCrop(source_crop_);
  drmHwcLayer->SetTransform(transform_);

  if(buffer_ && !validate){
    drmHwcLayer->iFd_     = pBufferInfo_->iFd_;
    drmHwcLayer->iWidth_  = pBufferInfo_->iWidth_;
    drmHwcLayer->iHeight_ = pBufferInfo_->iHeight_;
    drmHwcLayer->iStride_ = pBufferInfo_->iStride_;
    drmHwcLayer->iFormat_ = pBufferInfo_->iFormat_;
    drmHwcLayer->iUsage   = pBufferInfo_->iUsage_;
    drmHwcLayer->iByteStride_     = pBufferInfo_->iByteStride_;
    drmHwcLayer->uFourccFormat_   = pBufferInfo_->uFourccFormat_;
    drmHwcLayer->uModifier_       = pBufferInfo_->uModifier_;
    drmHwcLayer->sLayerName_      = pBufferInfo_->sLayerName_;
    drmHwcLayer->uGemHandle_      = pBufferInfo_->uGemHandle_;

  }else{
    drmHwcLayer->iFd_     = -1;
    drmHwcLayer->iWidth_  = -1;
    drmHwcLayer->iHeight_ = -1;
    drmHwcLayer->iStride_ = -1;
    drmHwcLayer->iFormat_ = -1;
    drmHwcLayer->iUsage   = -1;
    drmHwcLayer->uFourccFormat_   = DRM_FORMAT_ABGR8888; // fb target default DRM_FORMAT_ABGR8888
    drmHwcLayer->uModifier_ = 0;
    drmHwcLayer->uGemHandle_      = 0;
    drmHwcLayer->sLayerName_.clear();
  }

  drmHwcLayer->Init();

  return;
}

void DrmHwcTwo::HwcLayer::DumpLayerInfo(String8 &output) {

  output.appendFormat( " %04" PRIu32 " | %03" PRIu32 " | %9s | %9s | %-18.18" PRIxPTR " | %-11.11s | %-10.10s |%7.1f,%7.1f,%7.1f,%7.1f |%5d,%5d,%5d,%5d | %10x | %s\n",
                    id_,z_order_,to_string(sf_type_).c_str(),to_string(validated_type_).c_str(),
                    intptr_t(buffer_), to_string(transform_).c_str(), to_string(blending_).c_str(),
                    source_crop_.left, source_crop_.top, source_crop_.right, source_crop_.bottom,
                    display_frame_.left, display_frame_.top, display_frame_.right, display_frame_.bottom,
                    dataspace_,layer_name_.c_str());
  return;
}
int DrmHwcTwo::HwcLayer::DumpData() {
  if(!buffer_)
    ALOGI_IF(LogLevel(DBG_INFO),"%s,line=%d LayerId=%u Buffer is null.",__FUNCTION__,__LINE__,id_);

  void* cpu_addr = NULL;
  static int frame_cnt =0;
  int width,height,stride,byte_stride,format,size;
  int ret = 0;
  width  = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_WIDTH);
  height = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_HEIGHT);
  stride = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_STRIDE);
  format = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_FORMAT);
  size   = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_SIZE);
  byte_stride = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_BYTE_STRIDE);

  cpu_addr = drmGralloc_->hwc_get_handle_lock(buffer_,width,height);
  if(ret){
    ALOGE("%s,line=%d, LayerId=%u, lock fail ret = %d ",__FUNCTION__,__LINE__,id_,ret);
    return ret;
  }
  FILE * pfile = NULL;
  char data_name[100] ;
  system("mkdir /data/dump/ && chmod /data/dump/ 777 ");
  sprintf(data_name,"/data/dump/%d_%5.5s_id-%d_%dx%d_z-%d.bin",
          frame_cnt++,layer_name_.size() < 5 ? "unset" : layer_name_.c_str(),
          id_,stride,height,z_order_);

  pfile = fopen(data_name,"wb");
  if(pfile)
  {
      fwrite((const void *)cpu_addr,(size_t)(size),1,pfile);
      fflush(pfile);
      fclose(pfile);
      ALOGD(" dump surface layer_id=%d ,data_name %s,w:%d,h:%d,stride :%d,size=%d,cpu_addr=%p",
          id_,data_name,width,height,byte_stride,size,cpu_addr);
  }
  else
  {
      ALOGE("Open %s fail", data_name);
      ALOGD(" dump surface layer_id=%d ,data_name %s,w:%d,h:%d,stride :%d,size=%d,cpu_addr=%p",
          id_,data_name,width,height,byte_stride,size,cpu_addr);
  }


  ret = drmGralloc_->hwc_get_handle_unlock(buffer_);
  if(ret){
    ALOGE("%s,line=%d, LayerId=%u, unlock fail ret = %d ",__FUNCTION__,__LINE__,id_,ret);
    return ret;
  }

  return ret;

}

void DrmHwcTwo::HandleDisplayHotplug(hwc2_display_t displayid, int state) {
  auto cb = callbacks_.find(HWC2::Callback::Hotplug);
  if (cb == callbacks_.end())
    return;

  if(isRK3566(resource_manager_->getSocId())){
      ALOGD_IF(LogLevel(DBG_DEBUG),"HandleDisplayHotplug skip display-id=%" PRIu64 " state=%d",displayid,state);
      if(displayid != HWC_DISPLAY_PRIMARY){
        auto &drmDevices = resource_manager_->getDrmDevices();
        for (auto &device : drmDevices) {
          if(state==DRM_MODE_CONNECTED)
            device->SetCommitMirrorDisplayId(displayid);
          else
            device->SetCommitMirrorDisplayId(-1);
        }
      }
      return;
  }

  if(displayid == HWC_DISPLAY_PRIMARY)
    return;

  auto hotplug = reinterpret_cast<HWC2_PFN_HOTPLUG>(cb->second.func);
  hotplug(cb->second.data, displayid,
          (state == DRM_MODE_CONNECTED ? HWC2_CONNECTION_CONNECTED
                                       : HWC2_CONNECTION_DISCONNECTED));
}

void DrmHwcTwo::HandleInitialHotplugState(DrmDevice *drmDevice) {
    for (auto &conn : drmDevice->connectors()) {
      if (conn->state() != DRM_MODE_CONNECTED)
        continue;
      for (auto &crtc : drmDevice->crtc()) {
        if(conn->display() != crtc->display())
          continue;
        // HWC_DISPLAY_PRIMARY display have been hotplug
        if(conn->display() == HWC_DISPLAY_PRIMARY)
          continue;
        ALOGI("HWC2 Init: SF register connector %u type=%s, type_id=%d \n",
          conn->id(),drmDevice->connector_type_str(conn->type()),conn->type_id());
        HandleDisplayHotplug(conn->display(), conn->state());
      }
    }
}


void DrmHwcTwo::DrmHotplugHandler::HandleEvent(uint64_t timestamp_us) {
  for (auto &conn : drm_->connectors()) {
    drmModeConnection old_state = conn->state();
    conn->ResetModesReady();
    drmModeConnection cur_state = conn->UpdateModes()
                                      ? DRM_MODE_UNKNOWNCONNECTION
                                      : conn->state();

    if(!conn->ModesReady())
      continue;
    if (cur_state == old_state)
      continue;
    ALOGI("hwc_hotplug: %s event @%" PRIu64 " for connector %u type=%s, type_id=%d\n",
          cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug", timestamp_us,
          conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());

    int display_id = conn->display();
    auto &display = hwc2_->displays_.at(display_id);
    if (cur_state == DRM_MODE_CONNECTED) {
      display.HoplugEventTmeline();
      display.UpdateDisplayMode();
      display.ChosePreferredConfig();
      display.CheckStateAndReinit();
      hwc2_->HandleDisplayHotplug(display_id, DRM_MODE_CONNECTED);
    }else{
      display.ClearDisplay();
      drm_->ReleaseDpyRes(display_id);
      display.ReleaseResource();
      hwc2_->HandleDisplayHotplug(display_id, DRM_MODE_DISCONNECTED);
    }
  }

  auto &display = hwc2_->displays_.at(0);
  display.InvalidateControl(5,20);

  return;
}

// static
int DrmHwcTwo::HookDevClose(hw_device_t * /*dev*/) {
  unsupported(__func__);
  return 0;
}

// static
void DrmHwcTwo::HookDevGetCapabilities(hwc2_device_t * /*dev*/,
                                       uint32_t *out_count,
                                       int32_t * /*out_capabilities*/) {
  supported(__func__);
  *out_count = 0;
}

// static
hwc2_function_pointer_t DrmHwcTwo::HookDevGetFunction(
    struct hwc2_device * /*dev*/, int32_t descriptor) {
  supported(__func__);
  auto func = static_cast<HWC2::FunctionDescriptor>(descriptor);
  switch (func) {
    // Device functions
    case HWC2::FunctionDescriptor::CreateVirtualDisplay:
      return ToHook<HWC2_PFN_CREATE_VIRTUAL_DISPLAY>(
          DeviceHook<int32_t, decltype(&DrmHwcTwo::CreateVirtualDisplay),
                     &DrmHwcTwo::CreateVirtualDisplay, uint32_t, uint32_t,
                     int32_t *, hwc2_display_t *>);
    case HWC2::FunctionDescriptor::DestroyVirtualDisplay:
      return ToHook<HWC2_PFN_DESTROY_VIRTUAL_DISPLAY>(
          DeviceHook<int32_t, decltype(&DrmHwcTwo::DestroyVirtualDisplay),
                     &DrmHwcTwo::DestroyVirtualDisplay, hwc2_display_t>);
    case HWC2::FunctionDescriptor::Dump:
      return ToHook<HWC2_PFN_DUMP>(
          DeviceHook<void, decltype(&DrmHwcTwo::Dump), &DrmHwcTwo::Dump,
                     uint32_t *, char *>);
    case HWC2::FunctionDescriptor::GetMaxVirtualDisplayCount:
      return ToHook<HWC2_PFN_GET_MAX_VIRTUAL_DISPLAY_COUNT>(
          DeviceHook<uint32_t, decltype(&DrmHwcTwo::GetMaxVirtualDisplayCount),
                     &DrmHwcTwo::GetMaxVirtualDisplayCount>);
    case HWC2::FunctionDescriptor::RegisterCallback:
      return ToHook<HWC2_PFN_REGISTER_CALLBACK>(
          DeviceHook<int32_t, decltype(&DrmHwcTwo::RegisterCallback),
                     &DrmHwcTwo::RegisterCallback, int32_t,
                     hwc2_callback_data_t, hwc2_function_pointer_t>);

    // Display functions
    case HWC2::FunctionDescriptor::AcceptDisplayChanges:
      return ToHook<HWC2_PFN_ACCEPT_DISPLAY_CHANGES>(
          DisplayHook<decltype(&HwcDisplay::AcceptDisplayChanges),
                      &HwcDisplay::AcceptDisplayChanges>);
    case HWC2::FunctionDescriptor::CreateLayer:
      return ToHook<HWC2_PFN_CREATE_LAYER>(
          DisplayHook<decltype(&HwcDisplay::CreateLayer),
                      &HwcDisplay::CreateLayer, hwc2_layer_t *>);
    case HWC2::FunctionDescriptor::DestroyLayer:
      return ToHook<HWC2_PFN_DESTROY_LAYER>(
          DisplayHook<decltype(&HwcDisplay::DestroyLayer),
                      &HwcDisplay::DestroyLayer, hwc2_layer_t>);
    case HWC2::FunctionDescriptor::GetActiveConfig:
      return ToHook<HWC2_PFN_GET_ACTIVE_CONFIG>(
          DisplayHook<decltype(&HwcDisplay::GetActiveConfig),
                      &HwcDisplay::GetActiveConfig, hwc2_config_t *>);
    case HWC2::FunctionDescriptor::GetChangedCompositionTypes:
      return ToHook<HWC2_PFN_GET_CHANGED_COMPOSITION_TYPES>(
          DisplayHook<decltype(&HwcDisplay::GetChangedCompositionTypes),
                      &HwcDisplay::GetChangedCompositionTypes, uint32_t *,
                      hwc2_layer_t *, int32_t *>);
    case HWC2::FunctionDescriptor::GetClientTargetSupport:
      return ToHook<HWC2_PFN_GET_CLIENT_TARGET_SUPPORT>(
          DisplayHook<decltype(&HwcDisplay::GetClientTargetSupport),
                      &HwcDisplay::GetClientTargetSupport, uint32_t, uint32_t,
                      int32_t, int32_t>);
    case HWC2::FunctionDescriptor::GetColorModes:
      return ToHook<HWC2_PFN_GET_COLOR_MODES>(
          DisplayHook<decltype(&HwcDisplay::GetColorModes),
                      &HwcDisplay::GetColorModes, uint32_t *, int32_t *>);
    case HWC2::FunctionDescriptor::GetDisplayAttribute:
      return ToHook<HWC2_PFN_GET_DISPLAY_ATTRIBUTE>(
          DisplayHook<decltype(&HwcDisplay::GetDisplayAttribute),
                      &HwcDisplay::GetDisplayAttribute, hwc2_config_t, int32_t,
                      int32_t *>);
    case HWC2::FunctionDescriptor::GetDisplayConfigs:
      return ToHook<HWC2_PFN_GET_DISPLAY_CONFIGS>(
          DisplayHook<decltype(&HwcDisplay::GetDisplayConfigs),
                      &HwcDisplay::GetDisplayConfigs, uint32_t *,
                      hwc2_config_t *>);
    case HWC2::FunctionDescriptor::GetDisplayName:
      return ToHook<HWC2_PFN_GET_DISPLAY_NAME>(
          DisplayHook<decltype(&HwcDisplay::GetDisplayName),
                      &HwcDisplay::GetDisplayName, uint32_t *, char *>);
    case HWC2::FunctionDescriptor::GetDisplayRequests:
      return ToHook<HWC2_PFN_GET_DISPLAY_REQUESTS>(
          DisplayHook<decltype(&HwcDisplay::GetDisplayRequests),
                      &HwcDisplay::GetDisplayRequests, int32_t *, uint32_t *,
                      hwc2_layer_t *, int32_t *>);
    case HWC2::FunctionDescriptor::GetDisplayType:
      return ToHook<HWC2_PFN_GET_DISPLAY_TYPE>(
          DisplayHook<decltype(&HwcDisplay::GetDisplayType),
                      &HwcDisplay::GetDisplayType, int32_t *>);
    case HWC2::FunctionDescriptor::GetDozeSupport:
      return ToHook<HWC2_PFN_GET_DOZE_SUPPORT>(
          DisplayHook<decltype(&HwcDisplay::GetDozeSupport),
                      &HwcDisplay::GetDozeSupport, int32_t *>);
    case HWC2::FunctionDescriptor::GetHdrCapabilities:
      return ToHook<HWC2_PFN_GET_HDR_CAPABILITIES>(
          DisplayHook<decltype(&HwcDisplay::GetHdrCapabilities),
                      &HwcDisplay::GetHdrCapabilities, uint32_t *, int32_t *,
                      float *, float *, float *>);
    case HWC2::FunctionDescriptor::GetReleaseFences:
      return ToHook<HWC2_PFN_GET_RELEASE_FENCES>(
          DisplayHook<decltype(&HwcDisplay::GetReleaseFences),
                      &HwcDisplay::GetReleaseFences, uint32_t *, hwc2_layer_t *,
                      int32_t *>);
    case HWC2::FunctionDescriptor::PresentDisplay:
      return ToHook<HWC2_PFN_PRESENT_DISPLAY>(
          DisplayHook<decltype(&HwcDisplay::PresentDisplay),
                      &HwcDisplay::PresentDisplay, int32_t *>);
    case HWC2::FunctionDescriptor::SetActiveConfig:
      return ToHook<HWC2_PFN_SET_ACTIVE_CONFIG>(
          DisplayHook<decltype(&HwcDisplay::SetActiveConfig),
                      &HwcDisplay::SetActiveConfig, hwc2_config_t>);
    case HWC2::FunctionDescriptor::SetClientTarget:
      return ToHook<HWC2_PFN_SET_CLIENT_TARGET>(
          DisplayHook<decltype(&HwcDisplay::SetClientTarget),
                      &HwcDisplay::SetClientTarget, buffer_handle_t, int32_t,
                      int32_t, hwc_region_t>);
    case HWC2::FunctionDescriptor::SetColorMode:
      return ToHook<HWC2_PFN_SET_COLOR_MODE>(
          DisplayHook<decltype(&HwcDisplay::SetColorMode),
                      &HwcDisplay::SetColorMode, int32_t>);
    case HWC2::FunctionDescriptor::SetColorTransform:
      return ToHook<HWC2_PFN_SET_COLOR_TRANSFORM>(
          DisplayHook<decltype(&HwcDisplay::SetColorTransform),
                      &HwcDisplay::SetColorTransform, const float *, int32_t>);
    case HWC2::FunctionDescriptor::SetOutputBuffer:
      return ToHook<HWC2_PFN_SET_OUTPUT_BUFFER>(
          DisplayHook<decltype(&HwcDisplay::SetOutputBuffer),
                      &HwcDisplay::SetOutputBuffer, buffer_handle_t, int32_t>);
    case HWC2::FunctionDescriptor::SetPowerMode:
      return ToHook<HWC2_PFN_SET_POWER_MODE>(
          DisplayHook<decltype(&HwcDisplay::SetPowerMode),
                      &HwcDisplay::SetPowerMode, int32_t>);
    case HWC2::FunctionDescriptor::SetVsyncEnabled:
      return ToHook<HWC2_PFN_SET_VSYNC_ENABLED>(
          DisplayHook<decltype(&HwcDisplay::SetVsyncEnabled),
                      &HwcDisplay::SetVsyncEnabled, int32_t>);
    case HWC2::FunctionDescriptor::ValidateDisplay:
      return ToHook<HWC2_PFN_VALIDATE_DISPLAY>(
          DisplayHook<decltype(&HwcDisplay::ValidateDisplay),
                      &HwcDisplay::ValidateDisplay, uint32_t *, uint32_t *>);

    // Layer functions
    case HWC2::FunctionDescriptor::SetCursorPosition:
      return ToHook<HWC2_PFN_SET_CURSOR_POSITION>(
          LayerHook<decltype(&HwcLayer::SetCursorPosition),
                    &HwcLayer::SetCursorPosition, int32_t, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerBlendMode:
      return ToHook<HWC2_PFN_SET_LAYER_BLEND_MODE>(
          LayerHook<decltype(&HwcLayer::SetLayerBlendMode),
                    &HwcLayer::SetLayerBlendMode, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerBuffer:
      return ToHook<HWC2_PFN_SET_LAYER_BUFFER>(
          LayerHook<decltype(&HwcLayer::SetLayerBuffer),
                    &HwcLayer::SetLayerBuffer, buffer_handle_t, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerColor:
      return ToHook<HWC2_PFN_SET_LAYER_COLOR>(
          LayerHook<decltype(&HwcLayer::SetLayerColor),
                    &HwcLayer::SetLayerColor, hwc_color_t>);
    case HWC2::FunctionDescriptor::SetLayerCompositionType:
      return ToHook<HWC2_PFN_SET_LAYER_COMPOSITION_TYPE>(
          LayerHook<decltype(&HwcLayer::SetLayerCompositionType),
                    &HwcLayer::SetLayerCompositionType, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerDataspace:
      return ToHook<HWC2_PFN_SET_LAYER_DATASPACE>(
          LayerHook<decltype(&HwcLayer::SetLayerDataspace),
                    &HwcLayer::SetLayerDataspace, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerDisplayFrame:
      return ToHook<HWC2_PFN_SET_LAYER_DISPLAY_FRAME>(
          LayerHook<decltype(&HwcLayer::SetLayerDisplayFrame),
                    &HwcLayer::SetLayerDisplayFrame, hwc_rect_t>);
    case HWC2::FunctionDescriptor::SetLayerPlaneAlpha:
      return ToHook<HWC2_PFN_SET_LAYER_PLANE_ALPHA>(
          LayerHook<decltype(&HwcLayer::SetLayerPlaneAlpha),
                    &HwcLayer::SetLayerPlaneAlpha, float>);
    case HWC2::FunctionDescriptor::SetLayerSidebandStream:
      return ToHook<HWC2_PFN_SET_LAYER_SIDEBAND_STREAM>(
          LayerHook<decltype(&HwcLayer::SetLayerSidebandStream),
                    &HwcLayer::SetLayerSidebandStream,
                    const native_handle_t *>);
    case HWC2::FunctionDescriptor::SetLayerSourceCrop:
      return ToHook<HWC2_PFN_SET_LAYER_SOURCE_CROP>(
          LayerHook<decltype(&HwcLayer::SetLayerSourceCrop),
                    &HwcLayer::SetLayerSourceCrop, hwc_frect_t>);
    case HWC2::FunctionDescriptor::SetLayerSurfaceDamage:
      return ToHook<HWC2_PFN_SET_LAYER_SURFACE_DAMAGE>(
          LayerHook<decltype(&HwcLayer::SetLayerSurfaceDamage),
                    &HwcLayer::SetLayerSurfaceDamage, hwc_region_t>);
    case HWC2::FunctionDescriptor::SetLayerTransform:
      return ToHook<HWC2_PFN_SET_LAYER_TRANSFORM>(
          LayerHook<decltype(&HwcLayer::SetLayerTransform),
                    &HwcLayer::SetLayerTransform, int32_t>);
    case HWC2::FunctionDescriptor::SetLayerVisibleRegion:
      return ToHook<HWC2_PFN_SET_LAYER_VISIBLE_REGION>(
          LayerHook<decltype(&HwcLayer::SetLayerVisibleRegion),
                    &HwcLayer::SetLayerVisibleRegion, hwc_region_t>);
    case HWC2::FunctionDescriptor::SetLayerZOrder:
      return ToHook<HWC2_PFN_SET_LAYER_Z_ORDER>(
          LayerHook<decltype(&HwcLayer::SetLayerZOrder),
                    &HwcLayer::SetLayerZOrder, uint32_t>);
    case HWC2::FunctionDescriptor::Invalid:
    default:
      return NULL;
  }
}

// static
int DrmHwcTwo::HookDevOpen(const struct hw_module_t *module, const char *name,
                           struct hw_device_t **dev) {
  if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
    ALOGE("Invalid module name- %s", name);
    return -EINVAL;
  }
  InitDebugModule();

  std::unique_ptr<DrmHwcTwo> ctx(new DrmHwcTwo());
  if (!ctx) {
    ALOGE("Failed to allocate DrmHwcTwo");
    return -ENOMEM;
  }

  HWC2::Error err = ctx->Init();
  if (err != HWC2::Error::None) {
    ALOGE("Failed to initialize DrmHwcTwo err=%d\n", err);
    return -EINVAL;
  }
  g_ctx = ctx.get();

  signal(SIGALRM, StaticScreenOptHandler);

  property_set("vendor.hwc.hdr_state","NORMAL");

  ctx->common.module = const_cast<hw_module_t *>(module);
  *dev = &ctx->common;
  ctx.release();

  return 0;
}
}  // namespace android

static struct hw_module_methods_t hwc2_module_methods = {
    .open = android::DrmHwcTwo::HookDevOpen,
};

hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = HARDWARE_MODULE_API_VERSION(2, 0),
    .id = HWC_HARDWARE_MODULE_ID,
    .name = "DrmHwcTwo module",
    .author = "The Android Open Source Project",
    .methods = &hwc2_module_methods,
    .dso = NULL,
    .reserved = {0},
};
