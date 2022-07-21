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
  int rv = resource_manager_->Init(this);
  if (rv) {
    ALOGE("Can't initialize the resource manager %d", rv);
    return HWC2::Error::NoResources;
  }

  HWC2::Error ret = HWC2::Error::None;
  for (auto &map_display : resource_manager_->getDisplays()) {
    ret = CreateDisplay(map_display.second, HWC2::DisplayType::Physical);
    if (ret != HWC2::Error::None) {
      ALOGE("Failed to create display %d with error %d", map_display.second, ret);
      return ret;
    }
  }

  auto &drmDevices = resource_manager_->GetDrmDevices();
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
      auto &drmDevices = resource_manager_->GetDrmDevices();
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

int DrmHwcTwo::HwcDisplay::ClearDisplay() {
  if(!init_success_){
    HWC2_ALOGE("init_success_=%d skip.",init_success_);
    return -1;
  }

  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);
  compositor_->ClearDisplay();
  return 0;
}

int DrmHwcTwo::HwcDisplay::ReleaseResource(){
  resource_manager_->removeActiveDisplayCnt(static_cast<int>(handle_));
  resource_manager_->assignPlaneGroup();
  return 0;
}

HWC2::Error DrmHwcTwo::HwcDisplay::Init() {

  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  int display = static_cast<int>(handle_);

  if(sync_timeline_.isValid()){
    HWC2_ALOGD_IF_INFO("sync_timeline_ fd = %d isValid",sync_timeline_.getFd());
  }

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

  compositor_ = resource_manager_->GetDrmDisplayCompositor(crtc_);
  ret = compositor_->Init(resource_manager_, display);
  if (ret) {
    ALOGE("Failed display compositor init for display %d (%d)", display, ret);
    return HWC2::Error::NoResources;
  }

  // CropSpilt must to
  if(connector_->isCropSpilt()){
    std::unique_ptr<DrmDisplayComposition> composition = compositor_->CreateComposition();
    composition->Init(drm_, crtc_, importer_.get(), planner_.get(), frame_no_, handle_);
    composition->SetDpmsMode(DRM_MODE_DPMS_ON);
    ret = compositor_->QueueComposition(std::move(composition));
    if (ret) {
      HWC2_ALOGE("Failed to apply the dpms composition ret=%d", ret);
    }
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
    for(auto &map_layer : layers_){
      map_layer.second.clear();
    }

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

  compositor_ = resource_manager_->GetDrmDisplayCompositor(crtc_);
  ret = compositor_->Init(resource_manager_, display);
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


    if(connector_->isHorizontalSpilt()){
      ctx_.framebuffer_width = best_mode.h_display() / 2;
      ctx_.framebuffer_height = best_mode.v_display();
    }else{
      ctx_.framebuffer_width = best_mode.h_display();
      ctx_.framebuffer_height = best_mode.v_display();
    }

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

    if(connector_->isHorizontalSpilt()){
      ctx_.rel_xres = best_mode.h_display() / DRM_CONNECTOR_SPILT_RATIO;
      ctx_.rel_yres = best_mode.v_display();
      ctx_.framebuffer_width = ctx_.framebuffer_width / DRM_CONNECTOR_SPILT_RATIO;
      ctx_.framebuffer_height = ctx_.framebuffer_height;
      if(handle_ >= DRM_CONNECTOR_SPILT_MODE_MASK){
        ctx_.rel_xoffset = best_mode.h_display() / DRM_CONNECTOR_SPILT_RATIO;
        ctx_.rel_yoffset = 0;//best_mode.v_display() / 2;
      }
    }else if(connector_->isCropSpilt()){
      int32_t fb_w = 0, fb_h = 0;
      connector_->getCropSpiltFb(&fb_w, &fb_h);
      ctx_.framebuffer_width = fb_w;
      ctx_.framebuffer_height = fb_h;
      ctx_.rel_xres = best_mode.h_display();
      ctx_.rel_yres = best_mode.v_display();
    }else{
      ctx_.rel_xres = best_mode.h_display();
      ctx_.rel_yres = best_mode.v_display();
    }

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

  uint32_t num_request = 0;
  if(hwc_get_int_property("ro.vendor.rk_sdk","0") == 0){
    HWC2_ALOGI("Maybe GSI SDK, to disable AFBC\n");
  if (!layers || !layer_requests){
    *num_elements = num_request;
    return HWC2::Error::None;
  }else{
    *display_requests = 0;
    return HWC2::Error::None;
  }
  }

  // TODO: I think virtual display should request
  //      HWC2_DISPLAY_REQUEST_WRITE_CLIENT_TARGET_TO_OUTPUT here
  uint32_t client_layer_id = false;
  for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &l : layers_) {
    if (l.second.validated_type() == HWC2::Composition::Client) {
        client_layer_id = l.first;
        break;
    }
  }

  if(client_layer_id > 0 && validate_success_ && !client_layer_.isAfbc()){
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
    layers[0] = client_layer_id;
    layer_requests[0] = 0;
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
  HWC2_ALOGD_IF_DEBUG("display-id=%" PRIu64,handle_);

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
    fences[num_layers - 1] = l.second.release_fence()->isValid() ? dup(l.second.release_fence()->getFd()) : -1;

    if(LogLevel(DBG_DEBUG))
      HWC2_ALOGD_IF_DEBUG("Check Layer %" PRIu64 " Release(%d) %s Info: size=%d act=%d signal=%d err=%d",
                          l.first,l.second.release_fence()->isValid(),l.second.release_fence()->getName().c_str(),
                          l.second.release_fence()->getSize(), l.second.release_fence()->getActiveCount(),
                          l.second.release_fence()->getSignaledCount(), l.second.release_fence()->getErrorCount());
    // HWC2_ALOGD_IF_DEBUG("GetReleaseFences [%" PRIu64 "][%d]",layers[num_layers - 1],fences[num_layers - 1]);
    // the new fence semantics for a frame n by returning the fence from frame n-1. For frame 0,
    // the adapter returns NO_FENCE.
  }
  *num_elements = num_layers;
  return HWC2::Error::None;
}

void DrmHwcTwo::HwcDisplay::AddFenceToRetireFence(int fd) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  char acBuf[32];
  int retire_fence_fd = -1;
  if (fd < 0){
    // Collet all layer releaseFence
    const sp<ReleaseFence> client_rf = client_layer_.back_release_fence();
    if(client_rf->isValid()){
      retire_fence_fd = dup(client_rf->getFd());
      sprintf(acBuf,"RTD%" PRIu64 "-FN%d-%d", handle_, frame_no_, 0);
    }
    for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &hwc2layer : layers_) {
    if(hwc2layer.second.validated_type() != HWC2::Composition::Device)
        continue;
      // the new fence semantics for a frame n by returning the fence from frame n-1. For frame 0,
      // the adapter returns NO_FENCE.
      const sp<ReleaseFence> rf = hwc2layer.second.back_release_fence();
      if (rf->isValid()){
        // cur_retire_fence is null
        if(retire_fence_fd > 0){
          sprintf(acBuf,"RTD%" PRIu64 "-FN%d-%" PRIu64,handle_, frame_no_, hwc2layer.first);
          int retire_fence_merge = rf->merge(retire_fence_fd, acBuf);
          if(retire_fence_merge > 0){
            close(retire_fence_fd);
            retire_fence_fd = retire_fence_merge;
            HWC2_ALOGD_IF_DEBUG("RetireFence(%d) %s frame = %d merge %s sucess!", retire_fence_fd, acBuf, frame_no_, rf->getName().c_str());
          }else{
            HWC2_ALOGE("RetireFence(%d) %s frame = %d merge %s faile!", retire_fence_fd, acBuf,frame_no_, rf->getName().c_str());
          }
        }else{
          retire_fence_fd = dup(rf->getFd());
          continue;
        }
      }
    }
  }else{
    retire_fence_fd = fd;
  }
  d_retire_fence_.add(retire_fence_fd, acBuf);
  return;
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

#ifdef USE_LIBSVEP
  if(handle_ == 0){
    int ret = client_layer_.DoSvep(true, &client_target_layer);
    if(ret){
      HWC2_ALOGE("ClientLayer DoSvep fail, ret = %d", ret);
    }
  }
#endif

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

  std::vector<PlaneGroup *> plane_groups;
  DrmDevice *drm = crtc_->getDrmDevice();
  plane_groups.clear();
  std::vector<PlaneGroup *> all_plane_groups = drm->GetPlaneGroups();
  for(auto &plane_group : all_plane_groups){
    if(plane_group->acquire(1 << crtc_->pipe(), handle_)){
      plane_groups.push_back(plane_group);
    }
  }

  std::tie(ret,
           composition_planes_) = planner_->TryHwcPolicy(layers, plane_groups, crtc_,
                                                         static_screen_opt_ ||
                                                         force_gles_ ||
                                                         connector_->isCropSpilt());
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
      if(drm_hwc_layer.bUseSvep_){
        ALOGD_IF(LogLevel(DBG_INFO),"[%.4" PRIu32 "]=Device-Svep : %s",drm_hwc_layer.uId_,drm_hwc_layer.sLayerName_.c_str());
      }else{
        ALOGD_IF(LogLevel(DBG_INFO),"[%.4" PRIu32 "]=Device : %s",drm_hwc_layer.uId_,drm_hwc_layer.sLayerName_.c_str());
      }
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
#ifdef USE_LIBSVEP
      if(handle_ == 0){
        int ret = client_layer_.DoSvep(false, &drm_hwc_layer);
        if(ret){
          HWC2_ALOGE("ClientLayer DoSvep fail, ret = %d", ret);
        }
      }
#endif
    }
    ret = drm_hwc_layer.ImportBuffer(importer_.get());
    if (ret) {
      ALOGE("Failed to import layer, ret=%d", ret);
      return HWC2::Error::NoResources;
    }
    map.layers.emplace_back(std::move(drm_hwc_layer));
  }

  std::unique_ptr<DrmDisplayComposition> composition = compositor_->CreateComposition();
  composition->Init(drm_, crtc_, importer_.get(), planner_.get(), frame_no_, handle_);

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
    ret = composition->CreateAndAssignReleaseFences(sync_timeline_);
    for (std::pair<const hwc2_layer_t, DrmHwcTwo::HwcLayer> &l : layers_){
      if(l.second.sf_type() == HWC2::Composition::Device){
        sp<ReleaseFence> rf = composition->GetReleaseFence(l.first);
        l.second.set_release_fence(rf);
      }else{
        l.second.set_release_fence(ReleaseFence::NO_FENCE);
      }
    }
    sp<ReleaseFence> rf = composition->GetReleaseFence(0);
    client_layer_.set_release_fence(rf);
    AddFenceToRetireFence(composition->take_out_fence());
  }

  ret = compositor_->QueueComposition(std::move(composition));

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::PresentDisplay(int32_t *retire_fence) {
  ATRACE_CALL();

  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64,handle_);

  if(!init_success_){
    HWC2_ALOGE("init_success_=%d skip.",init_success_);
    return HWC2::Error::BadDisplay;
  }

  DumpAllLayerData();

  HWC2::Error ret;
  ret = CheckDisplayState();
  if(ret != HWC2::Error::None || !validate_success_){
    ALOGE_IF(LogLevel(DBG_ERROR),"Check display %" PRIu64 " state fail %s, %s,line=%d", handle_,
          validate_success_? "" : "or validate fail.",__FUNCTION__, __LINE__);
    ClearDisplay();
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

  int32_t merge_retire_fence = -1;
  DoMirrorDisplay(&merge_retire_fence);
  if(merge_retire_fence > 0){
    if(d_retire_fence_.get()->isValid()){
      char acBuf[32];
      sprintf(acBuf,"RTD%" PRIu64 "M-FN%d-%d", handle_, frame_no_, 0);
      sp<ReleaseFence> rt = sp<ReleaseFence>(new ReleaseFence(merge_retire_fence, acBuf));
      *retire_fence = rt->merge(d_retire_fence_.get()->getFd(), acBuf);
    }else{
      *retire_fence = merge_retire_fence;
    }
  }else{
    // The retire fence returned here is for the last frame, so return it and
    // promote the next retire fence
    *retire_fence = d_retire_fence_.get()->isValid() ? dup(d_retire_fence_.get()->getFd()) : -1;
    if(LogLevel(DBG_DEBUG))
      HWC2_ALOGD_IF_DEBUG("Return RetireFence(%d) %s frame = %d Info: size=%d act=%d signal=%d err=%d",
                      d_retire_fence_.get()->isValid(),
                      d_retire_fence_.get()->getName().c_str(), frame_no_,
                      d_retire_fence_.get()->getSize(),d_retire_fence_.get()->getActiveCount(),
                      d_retire_fence_.get()->getSignaledCount(),d_retire_fence_.get()->getErrorCount());
  }

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
    if(connector_->isCropSpilt()){
      int32_t srcX, srcY, srcW, srcH;
      connector_->getCropInfo(&srcX, &srcY, &srcW, &srcH);
      hwc_rect_t display_frame = {.left = 0,
                                  .top = 0,
                                  .right = static_cast<int>(ctx_.framebuffer_width),
                                  .bottom = static_cast<int>(ctx_.framebuffer_height)};
      client_layer_.SetLayerDisplayFrame(display_frame);
      hwc_frect_t source_crop = {.left = srcX + 0.0f,
                                 .top  = srcY + 0.0f,
                                 .right = srcX + srcW + 0.0f,
                                 .bottom = srcY + srcH + 0.0f};
      client_layer_.SetLayerSourceCrop(source_crop);

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
  }

  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetClientTarget(buffer_handle_t target,
                                                   int32_t acquire_fence,
                                                   int32_t dataspace,
                                                   hwc_region_t /*damage*/) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", Buffer=%p, acq_fence=%d, dataspace=%x",
                         handle_,target,acquire_fence,dataspace);

  client_layer_.set_buffer(target);
  client_layer_.set_acquire_fence(sp<AcquireFence>(new AcquireFence(acquire_fence)));
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

HWC2::Error DrmHwcTwo::HwcDisplay::SyncPowerMode() {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ,handle_);

  if(!init_success_){
    HWC2_ALOGE("init_success_=%d skip.",init_success_);
    return HWC2::Error::BadDisplay;
  }

  if(!bNeedSyncPMState_){
    HWC2_ALOGI("bNeedSyncPMState_=%d don't need to sync PowerMode state.",bNeedSyncPMState_);
    return HWC2::Error::None;
  }

  HWC2::Error error = SetPowerMode((int32_t)mPowerMode_);
  if(error != HWC2::Error::None){
    HWC2_ALOGE("SetPowerMode fail %d", error);
    return error;
  }

  bNeedSyncPMState_ = false;
  return HWC2::Error::None;
}

HWC2::Error DrmHwcTwo::HwcDisplay::SetPowerMode(int32_t mode_in) {
  HWC2_ALOGD_IF_VERBOSE("display-id=%" PRIu64 ", mode_in=%d",handle_,mode_in);

  uint64_t dpms_value = 0;
  mPowerMode_ = static_cast<HWC2::PowerMode>(mode_in);
  switch (mPowerMode_) {
    case HWC2::PowerMode::Off:
      dpms_value = DRM_MODE_DPMS_OFF;
      break;
    case HWC2::PowerMode::On:
      dpms_value = DRM_MODE_DPMS_ON;
      break;
    case HWC2::PowerMode::Doze:
    case HWC2::PowerMode::DozeSuspend:
      ALOGI("Power mode %d is unsupported\n", mPowerMode_);
      return HWC2::Error::Unsupported;
    default:
      ALOGI("Power mode %d is BadParameter\n", mPowerMode_);
      return HWC2::Error::BadParameter;
  };

  if(!init_success_){
    bNeedSyncPMState_ = true;
    HWC2_ALOGE("init_success_=%d skip.",init_success_);
    return HWC2::Error::BadDisplay;
  }

  std::unique_ptr<DrmDisplayComposition> composition = compositor_->CreateComposition();
  composition->Init(drm_, crtc_, importer_.get(), planner_.get(), frame_no_, handle_);
  composition->SetDpmsMode(dpms_value);
  int ret = compositor_->QueueComposition(std::move(composition));
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
        auto &display = resource_manager_->GetHwc2()->displays_.at(extend_display_id);
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

  if(!init_success_){
    HWC2_ALOGE("init_success_=%d skip.",init_success_);
    return HWC2::Error::BadDisplay;
  }
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
    l.second.set_validated_type(HWC2::Composition::Client);

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
  // Enable Self-refresh mode.
  SelfRefreshEnable();
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

    for(auto &drm_layer : drm_hwc_layers_){
      if(drm_layer.bUseSvep_ && drm_layer.pSvepBuffer_){
        drm_layer.pSvepBuffer_->DumpData();
      }
    }
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
      // will change display resolution, to clear all display.
      if(!(connector_->current_mode() == connector_->active_mode()))
        ClearDisplay();
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
  int  hdr_area_ratio = 0;

  for(auto &drmHwcLayer : drm_hwc_layers_){
    if(drmHwcLayer.bHdr_){
      exist_hdr_layer = true;
      int dis_w = drmHwcLayer.display_frame.right - drmHwcLayer.display_frame.left;
      int dis_h = drmHwcLayer.display_frame.bottom - drmHwcLayer.display_frame.top;
      int dis_area_size = dis_w * dis_h;
      int screen_size = ctx_.rel_xres * ctx_.rel_yres;
      hdr_area_ratio = dis_area_size * 10 / screen_size;
    }
  }
  if(exist_hdr_layer){
    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.hwc.hdr_force_disable", value, "0");
    if(atoi(value) > 0){
      if(ctx_.hdr_mode && !connector_->switch_hdmi_hdr_mode(HAL_DATASPACE_UNKNOWN)){
        ALOGD_IF(LogLevel(DBG_DEBUG),"Exit HDR mode success");
        ctx_.hdr_mode = false;
        property_set("vendor.hwc.hdr_state","FORCE-NORMAL");
      }
      ALOGD_IF(LogLevel(DBG_DEBUG),"Fource Disable HDR mode.");
      return 0;
    }

    property_get("vendor.hwc.hdr_video_area", value, "10");
    if(atoi(value) > hdr_area_ratio){
      if(ctx_.hdr_mode && !connector_->switch_hdmi_hdr_mode(HAL_DATASPACE_UNKNOWN)){
        ALOGD_IF(LogLevel(DBG_DEBUG),"Exit HDR mode success");
        ctx_.hdr_mode = false;
        property_set("vendor.hwc.hdr_state","FORCE-NORMAL");
      }
      ALOGD_IF(LogLevel(DBG_DEBUG),"Force Disable HDR mode.");
      return 0;
    }
  }

  for(auto &drmHwcLayer : drm_hwc_layers_)
    if(drmHwcLayer.bHdr_){
      if(connector_->is_hdmi_support_hdr()){
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
      ALOGD_IF(LogLevel(DBG_DEBUG),"Yuv %s timer!",static_screen_timer_enable_ ? "Enable" : "Disable");
      enable_timer = false;
      break;
    }

#ifdef USE_LIBSVEP
    // Svep
    if(drmHwcLayer.bUseSvep_){
      ALOGD_IF(LogLevel(DBG_DEBUG),"Svep %s timer!",static_screen_timer_enable_ ? "Enable" : "Disable");
      enable_timer = false;
      break;
    }
#endif

    // Sideband
    if(drmHwcLayer.bSidebandStreamLayer_){
      enable_timer = false;
      break;
    }

    // Surface w/h is larger than FB
    int crop_w = static_cast<int>(drmHwcLayer.source_crop.right - drmHwcLayer.source_crop.left);
    int crop_h = static_cast<int>(drmHwcLayer.source_crop.bottom - drmHwcLayer.source_crop.top);
    if(crop_w * crop_h > ctx_.framebuffer_width * ctx_.framebuffer_height){
      ALOGD_IF(LogLevel(DBG_DEBUG),"LargeSurface %s timer!",static_screen_timer_enable_ ? "Enable" : "Disable");
      enable_timer = false;
      break;
    }
  }
  static_screen_timer_enable_ = enable_timer;
  return 0;
}
int DrmHwcTwo::HwcDisplay::SelfRefreshEnable(){
  bool enable_self_refresh = false;
  for(auto &drmHwcLayer : drm_hwc_layers_){

#ifdef USE_LIBSVEP
    // Svep
    if(drmHwcLayer.bUseSvep_){
      HWC2_ALOGD_IF_DEBUG("Svep Enable SelfRefresh!");
      enable_self_refresh = true;
      break;
    }
#endif
  }
  if(enable_self_refresh){
    InvalidateControl(10,-1);
  }else{
    InvalidateControl(0,0);
  }
  return 0 ;
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

int DrmHwcTwo::HwcDisplay::DoMirrorDisplay(int32_t *retire_fence){
  if(handle_ != 0)
    return 0;

  if(!connector_->isCropSpilt()){
    return 0;
  }

  int32_t merge_rt_fence;
  int32_t display_cnt = 1;
  for (auto &conn : drm_->connectors()) {
    if(!connector_->isCropSpilt()){
      continue;
    }
    int display_id = conn->display();
    if(display_id != 0){
      auto &display = resource_manager_->GetHwc2()->displays_.at(display_id);
      if (conn->state() == DRM_MODE_CONNECTED) {
        static hwc2_layer_t layer_id = 0;
        if(display.has_layer(layer_id)){
        }else{
          display.CreateLayer(&layer_id);
        }
        HwcLayer &layer = display.get_layer(layer_id);
        hwc_rect_t frame = {0,0,1920,1080};
        layer.SetLayerDisplayFrame(frame);
        hwc_frect_t crop = {0.0, 0.0, 1920.0, 1080.0};
        layer.SetLayerSourceCrop(crop);
        layer.SetLayerZOrder(0);
        layer.SetLayerBlendMode(HWC2_BLEND_MODE_NONE);
        layer.SetLayerPlaneAlpha(1.0);
        layer.SetLayerCompositionType(HWC2_COMPOSITION_DEVICE);
        // layer.SetLayerBuffer(NULL,-1);
        layer.SetLayerTransform(0);
        uint32_t num_types;
        uint32_t num_requests;
        display.ValidateDisplay(&num_types,&num_requests);
        // display.GetChangedCompositionTypes();
        // display.GetDisplayRequests();
        display.AcceptDisplayChanges();
        hwc_region_t damage;
        display.SetClientTarget(client_layer_.buffer(),
                                dup(client_layer_.acquire_fence()->getFd()),
                                0,
                                damage);
        int32_t rt_fence;
        display.PresentDisplay(&rt_fence);
        if(merge_rt_fence > 0){
            char acBuf[32];
            sprintf(acBuf,"RTD%" PRIu64 "M-FN%d-%d", handle_, frame_no_, display_cnt++);
            sp<ReleaseFence> rt = sp<ReleaseFence>(new ReleaseFence(rt_fence, acBuf));
            if(rt->isValid()){
              sprintf(acBuf,"RTD%" PRIu64 "M-FN%d-%d",handle_, frame_no_, display_cnt++);
              int32_t merge_rt_fence_temp = merge_rt_fence;
              merge_rt_fence = rt->merge(merge_rt_fence, acBuf);
              close(merge_rt_fence_temp);
            }else{
              HWC2_ALOGE("connector %u type=%s, type_id=%d is MirrorDisplay get retireFence fail.\n",
                          conn->id(),
                          drm_->connector_type_str(conn->type()),
                          conn->type_id());
            }
        }else{
          merge_rt_fence = rt_fence;
        }
      }
    }
  }
  *retire_fence = merge_rt_fence;
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
  //Deleting the following logic may cause the problem that the handle cannot be updated
  // The buffer and acquire_fence are handled elsewhere
  //  if (sf_type_ == HWC2::Composition::Client ||
  //      sf_type_ == HWC2::Composition::Sideband ||
  //      sf_type_ == HWC2::Composition::SolidColor)
  //    return HWC2::Error::None;
  if (sf_type_ == HWC2::Composition::Sideband){
    return HWC2::Error::None;
  }

  set_buffer(buffer);
  acquire_fence_ = sp<AcquireFence>(new AcquireFence(acquire_fence));
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
  HWC2_ALOGD_IF_VERBOSE("layer-id=%d stream=%p",id_,stream);

  setSidebandStream(stream);
  return HWC2::Error::None;
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
  drmHwcLayer->iBestPlaneType = 0;
  drmHwcLayer->bSidebandStreamLayer_ = false;

  drmHwcLayer->acquire_fence = acquire_fence_;

  drmHwcLayer->iFbWidth_ = ctx->framebuffer_width;
  drmHwcLayer->iFbHeight_ = ctx->framebuffer_height;

  drmHwcLayer->uAclk_ = ctx->aclk;
  drmHwcLayer->uDclk_ = ctx->dclk;
  drmHwcLayer->SetBlend(blending_);

  //
  bool sidebandStream = false;
  if(sf_type() == HWC2::Composition::Sideband){
    sidebandStream = true;
    drmHwcLayer->bSidebandStreamLayer_ = true;
    drmHwcLayer->sf_handle     = sidebandStreamHandle_;
    drmHwcLayer->SetDisplayFrame(display_frame_, ctx);

    hwc_frect source_crop;
    source_crop.top = 0;
    source_crop.left = 0;
    source_crop.right = pBufferInfo_->iWidth_;
    source_crop.bottom = pBufferInfo_->iHeight_;
    drmHwcLayer->SetSourceCrop(source_crop);

    drmHwcLayer->SetTransform(transform_);
    // Commit mirror function
    drmHwcLayer->SetDisplayFrameMirror(display_frame_);

    if(sidebandStreamHandle_){
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
  }else{
    drmHwcLayer->sf_handle = buffer_;
    drmHwcLayer->SetDisplayFrame(display_frame_, ctx);
    drmHwcLayer->SetSourceCrop(source_crop_);
    drmHwcLayer->SetTransform(transform_);
    // Commit mirror function
    drmHwcLayer->SetDisplayFrameMirror(display_frame_);

    if(buffer_){
      drmHwcLayer->uBufferId_ = pBufferInfo_->uBufferId_;
      drmHwcLayer->iFd_     = pBufferInfo_->iFd_;
      drmHwcLayer->iWidth_  = pBufferInfo_->iWidth_;
      drmHwcLayer->iHeight_ = pBufferInfo_->iHeight_;
      drmHwcLayer->iStride_ = pBufferInfo_->iStride_;
      drmHwcLayer->iSize_   = pBufferInfo_->iSize_;
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
      drmHwcLayer->iSize_   = -1;
      drmHwcLayer->iFormat_ = -1;
      drmHwcLayer->iUsage   = -1;
      drmHwcLayer->uFourccFormat_   = 0x20202020; //0x20 is space
      drmHwcLayer->uModifier_ = 0;
      drmHwcLayer->uGemHandle_ = 0;
      drmHwcLayer->sLayerName_.clear();
    }
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
  drmHwcLayer->iBestPlaneType = 0;

  if(!validate){
    drmHwcLayer->sf_handle     = buffer_;
    drmHwcLayer->acquire_fence = acquire_fence_;
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
    drmHwcLayer->iSize_   = pBufferInfo_->iSize_;
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
    drmHwcLayer->iSize_   = -1;
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

#ifdef USE_LIBSVEP
int DrmHwcTwo::HwcLayer::DoSvep(bool validate, DrmHwcLayer *drmHwcLayer){
  char value[PROPERTY_VALUE_MAX];
  property_get(SVEP_MODE_NAME, value, "0");
  int new_value = atoi(value);
  static bool use_svep_fb = false;
  if(new_value == 2){
    if(validate){
      if(bufferQueue_ == NULL){
        bufferQueue_ = std::make_shared<DrmBufferQueue>();
      }
      if(svep_ == NULL){
        property_set("vendor.gralloc.no_afbc_for_fb_target_layer", "1");
        svep_ = Svep::Get(true);
        if(svep_ != NULL){
          bSvepReady_ = true;
          HWC2_ALOGI("Svep module ready. to enable SvepMode.");
        }
      }
      if(bSvepReady_){
        // 1. Init Ctx
        int ret = svep_->InitCtx(svepCtx_);
        if(ret){
          HWC2_ALOGE("Svep ctx init fail");
          return ret;
        }
        // 2. Set buffer Info
        SvepImageInfo src;
        src.mBufferInfo_.iFd_     = 1;
        src.mBufferInfo_.iWidth_  = drmHwcLayer->iFbWidth_;
        src.mBufferInfo_.iHeight_ = drmHwcLayer->iFbHeight_;
        src.mBufferInfo_.iFormat_ = HAL_PIXEL_FORMAT_RGBA_8888;
        src.mBufferInfo_.iSize_   = drmHwcLayer->iFbWidth_ * drmHwcLayer->iFbHeight_ * 4;
        src.mBufferInfo_.iStride_ = drmHwcLayer->iFbWidth_;
        src.mBufferInfo_.uBufferId_ = 0x1;

        src.mCrop_.iLeft_  = (int)drmHwcLayer->source_crop.left;
        src.mCrop_.iTop_   = (int)drmHwcLayer->source_crop.top;
        src.mCrop_.iRight_ = (int)drmHwcLayer->source_crop.right;
        src.mCrop_.iBottom_= (int)drmHwcLayer->source_crop.bottom;

        ret = svep_->SetSrcImage(svepCtx_, src);
        if(ret){
          printf("Svep SetSrcImage fail\n");
          return ret;
        }

        // 3. Get dst info
        SvepImageInfo require;
        ret = svep_->GetDstRequireInfo(svepCtx_, require);
        if(ret){
          printf("Svep GetDstRequireInfo fail\n");
          return ret;
        }

        hwc_frect_t source_crop;
        source_crop.left   = require.mCrop_.iLeft_;
        source_crop.top    = require.mCrop_.iTop_;
        source_crop.right  = require.mCrop_.iRight_;
        source_crop.bottom = require.mCrop_.iBottom_;

        drmHwcLayer->iWidth_  = require.mBufferInfo_.iWidth_;
        drmHwcLayer->iHeight_ = require.mBufferInfo_.iHeight_;
        drmHwcLayer->iStride_ = require.mBufferInfo_.iStride_;
        drmHwcLayer->iFormat_ = require.mBufferInfo_.iFormat_;
        drmHwcLayer->SetSourceCrop(source_crop);
        use_svep_fb = true;
      }
    }else if(use_svep_fb){
      use_svep_fb = false;
      if(bufferQueue_ == NULL){
        bufferQueue_ = std::make_shared<DrmBufferQueue>();
      }
      if(svep_ == NULL){
        svep_ = Svep::Get();
        if(svep_ != NULL){
          bSvepReady_ = true;
          HWC2_ALOGI("Svep module ready. to enable SvepMode.");
        }
      }
      if(bSvepReady_){
        // 1. Init Ctx
        int ret = svep_->InitCtx(svepCtx_);
        if(ret){
          HWC2_ALOGE("Svep ctx init fail");
          return ret;
        }
        // 2. Set buffer Info
        SvepImageInfo src;
        src.mBufferInfo_.iFd_     = drmHwcLayer->iFd_;
        src.mBufferInfo_.iWidth_  = drmHwcLayer->iWidth_;
        src.mBufferInfo_.iHeight_ = drmHwcLayer->iHeight_;
        src.mBufferInfo_.iFormat_ = drmHwcLayer->iFormat_;
        src.mBufferInfo_.iStride_ = drmHwcLayer->iStride_;
        src.mBufferInfo_.iSize_   = drmHwcLayer->iSize_;
        src.mBufferInfo_.uBufferId_ = drmHwcLayer->uBufferId_;
        src.mBufferInfo_.uDataSpace_ = (uint64_t)drmHwcLayer->eDataSpace_;

        src.mCrop_.iLeft_  = (int)drmHwcLayer->source_crop.left;
        src.mCrop_.iTop_   = (int)drmHwcLayer->source_crop.top;
        src.mCrop_.iRight_ = (int)drmHwcLayer->source_crop.right;
        src.mCrop_.iBottom_= (int)drmHwcLayer->source_crop.bottom;

        ret = svep_->SetSrcImage(svepCtx_, src);
        if(ret){
          printf("Svep SetSrcImage fail\n");
          return ret;
        }

        // 3. Get dst info
        SvepImageInfo require;
        ret = svep_->GetDstRequireInfo(svepCtx_, require);
        if(ret){
          printf("Svep GetDstRequireInfo fail\n");
          return ret;
        }

        std::shared_ptr<DrmBuffer> dst_buffer;
        dst_buffer = bufferQueue_->DequeueDrmBuffer(require.mBufferInfo_.iWidth_,
                                                    require.mBufferInfo_.iHeight_,
                                                    require.mBufferInfo_.iFormat_,
                                                    "FB-target-transform");

        if(dst_buffer == NULL){
          HWC2_ALOGD_IF_DEBUG("DequeueDrmBuffer fail!, skip this policy.");
          return ret;
        }

        // 5. Set buffer Info
        SvepImageInfo dst;
        dst.mBufferInfo_.iFd_     = dst_buffer->GetFd();
        dst.mBufferInfo_.iWidth_  = dst_buffer->GetWidth();
        dst.mBufferInfo_.iHeight_ = dst_buffer->GetHeight();
        dst.mBufferInfo_.iFormat_ = dst_buffer->GetFormat();
        dst.mBufferInfo_.iStride_ = dst_buffer->GetStride();
        dst.mBufferInfo_.iSize_   = dst_buffer->GetSize();
        dst.mBufferInfo_.uBufferId_ = dst_buffer->GetBufferId();

        dst.mCrop_.iLeft_  = require.mCrop_.iLeft_;
        dst.mCrop_.iTop_   = require.mCrop_.iTop_;
        dst.mCrop_.iRight_ = require.mCrop_.iRight_;
        dst.mCrop_.iBottom_= require.mCrop_.iBottom_;

        ret = svep_->SetDstImage(svepCtx_, dst);
        if(ret){
          printf("Svep SetSrcImage fail\n");
          bufferQueue_->QueueBuffer(dst_buffer);
          return ret;
        }

        char value[PROPERTY_VALUE_MAX];
        property_get(SVEP_ENHANCEMENT_RATE_NAME, value, "5");
        int enhancement_rate = atoi(value);
        ret = svep_->SetEnhancementRate(svepCtx_, enhancement_rate);
        if(ret){
          printf("Svep SetSrcImage fail\n");
          return ret;
        }

        ret = svep_->SetOsdMode(svepCtx_, SVEP_OSD_ENABLE_GLOBAL, SVEP_OSD_GLOBAL_STR);
        if(ret){
          printf("Svep SetOsdMode fail\n");
          return ret;
        }

        hwc_frect_t source_crop;
        source_crop.left   = require.mCrop_.iLeft_;
        source_crop.top    = require.mCrop_.iTop_;
        source_crop.right  = require.mCrop_.iRight_;
        source_crop.bottom = require.mCrop_.iBottom_;
        drmHwcLayer->UpdateAndStoreInfoFromDrmBuffer(dst_buffer->GetHandle(),
                                                  dst_buffer->GetFd(),
                                                  dst_buffer->GetFormat(),
                                                  dst_buffer->GetWidth(),
                                                  dst_buffer->GetHeight(),
                                                  dst_buffer->GetStride(),
                                                  dst_buffer->GetByteStride(),
                                                  dst_buffer->GetSize(),
                                                  dst_buffer->GetUsage(),
                                                  dst_buffer->GetFourccFormat(),
                                                  dst_buffer->GetModifier(),
                                                  dst_buffer->GetName(),
                                                  source_crop,
                                                  dst_buffer->GetBufferId(),
                                                  dst_buffer->GetGemHandle());
        if(drmHwcLayer->acquire_fence->isValid()){
          ret = drmHwcLayer->acquire_fence->wait(1500);
          if(ret){
            HWC2_ALOGE("wait Fb-Target 1500ms timeout, ret=%d",ret);
            drmHwcLayer->bUseSvep_ = false;
            bufferQueue_->QueueBuffer(dst_buffer);
            return ret;
          }
        }
        int output_fence = 0;
        ret = svep_->RunAsync(svepCtx_, &output_fence);
        if(ret){
          HWC2_ALOGD_IF_DEBUG("RunAsync fail!");
          drmHwcLayer->bUseSvep_ = false;
          bufferQueue_->QueueBuffer(dst_buffer);
          return ret;
        }
        dst_buffer->SetFinishFence(dup(output_fence));
        drmHwcLayer->acquire_fence = sp<AcquireFence>(new AcquireFence(output_fence));

        property_get("vendor.dump", value, "false");
        if(!strcmp(value, "true")){
          drmHwcLayer->acquire_fence->wait();
          dst_buffer->DumpData();
        }
        bufferQueue_->QueueBuffer(dst_buffer);
      }
    }
  }

  drmHwcLayer->Init();

  return 0;
}
#endif

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
        auto &drmDevices = resource_manager_->GetDrmDevices();
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
        if(conn->display() == HWC_DISPLAY_PRIMARY){
          // SpiltDisplay Hotplug
          if(conn->isHorizontalSpilt()){
            HandleDisplayHotplug((conn->GetSpiltModeId()), conn->state());
            ALOGI("HWC2 Init: SF register connector %u type=%s, type_id=%d SpiltDisplay=%d\n",
              conn->id(),drmDevice->connector_type_str(conn->type()),conn->type_id(),conn->GetSpiltModeId());
          }
          continue;
        }
        // CropSpilt only register primary display.
        if(conn->display() != HWC_DISPLAY_PRIMARY){
          // SpiltDisplay Hotplug
          if(conn->isCropSpilt()){
            HWC2_ALOGI("HWC2 Init: not to register connector %u type=%s, type_id=%d isCropSpilt=%d\n",
                      conn->id(),drmDevice->connector_type_str(conn->type()),
                      conn->type_id(),
                      conn->isCropSpilt());
            continue;
          }
        }
        ALOGI("HWC2 Init: SF register connector %u type=%s, type_id=%d \n",
          conn->id(),drmDevice->connector_type_str(conn->type()),conn->type_id());
        HandleDisplayHotplug(conn->display(), conn->state());
        // SpiltDisplay Hotplug
        if(conn->isHorizontalSpilt()){
          HandleDisplayHotplug((conn->GetSpiltModeId()), conn->state());
          ALOGI("HWC2 Init: SF register connector %u type=%s, type_id=%d SpiltDisplay=%d\n",
            conn->id(),drmDevice->connector_type_str(conn->type()),conn->type_id(),conn->GetSpiltModeId());
        }
      }
    }
}

void DrmHwcTwo::DrmHotplugHandler::HandleEvent(uint64_t timestamp_us) {
  int32_t ret = 0;
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
      ret |= (int32_t)display.HoplugEventTmeline();
      ret |= (int32_t)display.UpdateDisplayMode();
      ret |= (int32_t)display.ChosePreferredConfig();
      ret |= (int32_t)display.CheckStateAndReinit();
      if(ret != 0){
        HWC2_ALOGE("hwc_hotplug: %s connector %u type=%s type_id=%d state is error, skip hotplug.",
                   cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                   conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
      }else if(conn->isCropSpilt()){
        HWC2_ALOGI("hwc_hotplug: %s connector %u type=%s type_id=%d isCropSpilt skip hotplug.",
                   cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                   conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
        display.SetPowerMode(HWC2_POWER_MODE_ON);
      }else{
        HWC2_ALOGI("hwc_hotplug: %s connector %u type=%s type_id=%d send hotplug event to SF.",
                   cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                   conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
        hwc2_->HandleDisplayHotplug(display_id, cur_state);
        display.SyncPowerMode();
      }
    }else{
      ret |= (int32_t)display.ClearDisplay();
      ret |= (int32_t)drm_->ReleaseDpyRes(display_id);
      ret |= (int32_t)display.ReleaseResource();
      if(ret != 0){
        HWC2_ALOGE("hwc_hotplug: %s connector %u type=%s type_id=%d state is error, skip hotplug.",
                   cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                   conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
      }else if(conn->isCropSpilt()){
        HWC2_ALOGI("hwc_hotplug: %s connector %u type=%s type_id=%d isCropSpilt skip hotplug.",
                   cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                   conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
        // display.SetPowerMode(HWC2_POWER_MODE_OFF);
      }else{
        HWC2_ALOGI("hwc_hotplug: %s connector %u type=%s type_id=%d send hotplug event to SF.",
                   cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                   conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
        hwc2_->HandleDisplayHotplug(display_id, cur_state);
      }
    }

    // SpiltDisplay Hoplug.
    if(conn->isHorizontalSpilt()){
      int display_id = conn->GetSpiltModeId();
      auto &display = hwc2_->displays_.at(display_id);
      if (cur_state == DRM_MODE_CONNECTED) {
        ret |= (int32_t)display.HoplugEventTmeline();
        ret |= (int32_t)display.UpdateDisplayMode();
        ret |= (int32_t)display.ChosePreferredConfig();
        ret |= (int32_t)display.CheckStateAndReinit();
        if(ret != 0){
          HWC2_ALOGE("hwc_hotplug: %s connector %u type=%s type_id=%d state is error, skip hotplug.",
                    cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                    conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
        }else{
          HWC2_ALOGI("hwc_hotplug: %s connector %u type=%s type_id=%d send hotplug event to SF.",
                    cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                    conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
          hwc2_->HandleDisplayHotplug(display_id, cur_state);
          display.SyncPowerMode();
        }
      }else{
      ret |= (int32_t)display.ClearDisplay();
      ret |= (int32_t)drm_->ReleaseDpyRes(display_id);
      ret |= (int32_t)display.ReleaseResource();
      if(ret != 0){
        HWC2_ALOGE("hwc_hotplug: %s connector %u type=%s type_id=%d state is error, skip hotplug.",
                  cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                  conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
      }else{
        HWC2_ALOGI("hwc_hotplug: %s connector %u type=%s type_id=%d send hotplug event to SF.",
                  cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug",
                  conn->id(),drm_->connector_type_str(conn->type()),conn->type_id());
        hwc2_->HandleDisplayHotplug(display_id, cur_state);
      }
    }
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
                                       int32_t * out_capabilities) {

  if(out_capabilities == NULL){
    *out_count = 1;
    return;
  }

  out_capabilities[0] = static_cast<int32_t>(HWC2::Capability::SidebandStream);
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
