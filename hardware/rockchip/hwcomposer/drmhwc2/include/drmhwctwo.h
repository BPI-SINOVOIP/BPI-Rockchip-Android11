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

#include "drmdisplaycompositor.h"
#include "drmlayer.h"
#include "resourcemanager.h"
#include "vsyncworker.h"
#include "platform.h"
#include "rockchip/drmgralloc.h"
#include "rockchip/invalidateworker.h"

#include <hardware/hwcomposer2.h>

#include <map>

namespace android {

class DrmGralloc;
class DrmHwcTwo;
class GemHandle;

DrmHwcTwo *g_ctx = NULL;

#define MAX_NUM_BUFFER_SLOTS 32

class DrmHwcTwo : public hwc2_device_t {
 public:
  static int HookDevOpen(const struct hw_module_t *module, const char *name,
                         struct hw_device_t **dev);

  DrmHwcTwo();

  HWC2::Error Init();

 private:

  class HwcLayer {
   public:
    HwcLayer(uint32_t layer_id, DrmDevice *drm){
      id_ = layer_id;
      drmGralloc_ = DrmGralloc::getInstance();
      drm_ = drm;
      bufferInfoMap_.clear();
    };

    void clear(){
      buffer_ = NULL;
      acquire_fence_.Close();
      release_fence_.Close();
      next_release_fence_.Close();

    }

    HWC2::Composition sf_type() const {
      return sf_type_;
    }
    HWC2::Composition validated_type() const {
      return validated_type_;
    }
    void accept_type_change() {
      sf_type_ = validated_type_;
    }
    void set_validated_type(HWC2::Composition type) {
      validated_type_ = type;
    }
    bool type_changed() const {
      return sf_type_ != validated_type_;
    }

    uint32_t z_order() const {
      return z_order_;
    }

    class GemHandle {
      public:
        GemHandle(){};
        ~GemHandle(){
          if(uBufferId_ == 0 || uGemHandle_ == 0)
            return;
          int ret = drmGralloc_->hwc_free_gemhandle(uBufferId_);
          if(ret){
            HWC2_ALOGE("%s hwc_free_gemhandle fail, buffer_id =%" PRIx64, name_, uBufferId_);
          }

        };

        int InitGemHandle(DrmDevice *drm,
                          DrmGralloc *drm_gralloc,
                          const char *name,
                          uint64_t buffer_fd,
                          uint64_t buffer_id){
            drm_ = drm;
            drmGralloc_ = drm_gralloc;
            name_ = name;
            uBufferId_ = buffer_id;
            int ret = drmGralloc_->hwc_get_gemhandle_from_fd(drm_->fd(), buffer_fd, uBufferId_, &uGemHandle_);
            if(ret){
              HWC2_ALOGE("%s hwc_get_gemhandle_from_fd fail, buffer_id =%" PRIx64, name_, uBufferId_);
            }
            return ret;
        };
        uint32_t GetGemHandle(){ return uGemHandle_;};
      private:
        DrmDevice *drm_;
        DrmGralloc *drmGralloc_;
        uint64_t uBufferId_=0;
        uint32_t uGemHandle_=0;
        const char *name_;
    };
    typedef struct bufferInfo{
      bufferInfo(){};
      int iFd_=0;
      int iFormat_=0;
      int iWidth_=0;
      int iHeight_=0;
      int iStride_=0;
      int iByteStride_=0;
      int iUsage_=0;
      uint32_t uFourccFormat_=0;
      uint32_t uGemHandle_=0;
      uint64_t uModifier_=0;
      uint64_t uBufferId_;
      GemHandle gemHandle_;
      std::string sLayerName_;
    }bufferInfo_t;

    buffer_handle_t buffer() {
      return buffer_;
    }
    void set_buffer(buffer_handle_t buffer) {
      buffer_ = buffer;

      // Bufferinfo Cache
      uint64_t buffer_id;
      drmGralloc_->hwc_get_handle_buffer_id(buffer_, &buffer_id);

      // Get Buffer info
      const auto mapBuffer = bufferInfoMap_.find(buffer_id);
      if(mapBuffer == bufferInfoMap_.end()){
        // If bHasCache_ is true, the new buffer_id need to reset mapBuffer
        if(bHasCache_){
          HWC2_ALOGD_IF_VERBOSE("bHasCache=%d to reset bufferInfoMap_ BufferId=%" PRIx64 " Name=%s",
                               bHasCache_,buffer_id,pBufferInfo_->sLayerName_.c_str());
          bufferInfoMap_.clear();
          bHasCache_  = false;
        }

        // If size is too big, the new buffer_id need to reset mapBuffer
        if(bufferInfoMap_.size() > MAX_NUM_BUFFER_SLOTS){
          HWC2_ALOGD_IF_VERBOSE("MapSize=%zu too large to reset bufferInfoMap_ BufferId=%" PRIx64 " Name=%s",
                               bufferInfoMap_.size(),buffer_id,pBufferInfo_->sLayerName_.c_str());
          bufferInfoMap_.clear();
        }

        auto ret = bufferInfoMap_.emplace(std::make_pair(buffer_id, std::make_shared<bufferInfo_t>(bufferInfo())));
        if(ret.second == false){
          HWC2_ALOGD_IF_VERBOSE("bufferInfoMap_ emplace fail! BufferHandle=%p",buffer);
        }else{
          pBufferInfo_ = ret.first->second;
          pBufferInfo_->iFd_     = drmGralloc_->hwc_get_handle_primefd(buffer_);
          pBufferInfo_->iWidth_  = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_WIDTH);
          pBufferInfo_->iHeight_ = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_HEIGHT);
          pBufferInfo_->iStride_ = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_STRIDE);
          pBufferInfo_->iByteStride_ = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_BYTE_STRIDE_WORKROUND);
          pBufferInfo_->iFormat_ = drmGralloc_->hwc_get_handle_attibute(buffer_,ATT_FORMAT);
          pBufferInfo_->iUsage_   = drmGralloc_->hwc_get_handle_usage(buffer_);
          pBufferInfo_->uFourccFormat_ = drmGralloc_->hwc_get_handle_fourcc_format(buffer_);
          pBufferInfo_->uModifier_ = drmGralloc_->hwc_get_handle_format_modifier(buffer_);
          drmGralloc_->hwc_get_handle_name(buffer_,pBufferInfo_->sLayerName_);
          layer_name_ = pBufferInfo_->sLayerName_;
          pBufferInfo_->gemHandle_.InitGemHandle(drm_, drmGralloc_, pBufferInfo_->sLayerName_.c_str(), pBufferInfo_->iFd_, buffer_id);
          pBufferInfo_->uGemHandle_ = pBufferInfo_->gemHandle_.GetGemHandle();
          HWC2_ALOGD_IF_VERBOSE("bufferInfoMap_ size = %zu insert success! BufferId=%" PRIx64 " Name=%s",
                               bufferInfoMap_.size(),buffer_id,pBufferInfo_->sLayerName_.c_str());
        }
      }else{
        bHasCache_ = true;
        pBufferInfo_ = mapBuffer->second;
        HWC2_ALOGD_IF_VERBOSE("bufferInfoMap_ size = %zu has cache! BufferId=%" PRIx64 " Name=%s",
                             bufferInfoMap_.size(),buffer_id,pBufferInfo_->sLayerName_.c_str());
      }
    }

    int take_acquire_fence() {
      return acquire_fence_.Release();
    }
    void set_acquire_fence(int acquire_fence) {
      acquire_fence_.Set(dup(acquire_fence));
    }

    int release_fence() {
      return release_fence_.get();
    }
    int next_release_fence() {
      return next_release_fence_.get();
    }
    int take_release_fence() {
      return release_fence_.Release();
    }
    void manage_release_fence() {
      release_fence_ = std::move(next_release_fence_);
      next_release_fence_ = -1;
    }
    void manage_next_release_fence() {
      next_release_fence_.Set(release_fence_raw_);
      release_fence_raw_ = -1;
    }
    OutputFd release_fence_output() {
      return OutputFd(&release_fence_raw_);
    }

    uint32_t id(){ return id_; }

    void PopulateDrmLayer(hwc2_layer_t layer_id,DrmHwcLayer *layer, hwc2_drm_display_t* ctx,
                                 uint32_t frame_no);
    void PopulateFB(hwc2_layer_t layer_id, DrmHwcLayer *drmHwcLayer,
                        hwc2_drm_display_t* ctx, uint32_t frame_no, bool validate);
    void DumpLayerInfo(String8 &output);

    int DumpData();

    void EnableAfbc() { is_afbc_ = true;};
    void DisableAfbc() { is_afbc_ = false;};
    bool isAfbc() { return is_afbc_;};

    // Layer hooks
    HWC2::Error SetCursorPosition(int32_t x, int32_t y);
    HWC2::Error SetLayerBlendMode(int32_t mode);
    HWC2::Error SetLayerBuffer(buffer_handle_t buffer, int32_t acquire_fence);
    HWC2::Error SetLayerColor(hwc_color_t color);
    HWC2::Error SetLayerCompositionType(int32_t type);
    HWC2::Error SetLayerDataspace(int32_t dataspace);
    HWC2::Error SetLayerDisplayFrame(hwc_rect_t frame);
    HWC2::Error SetLayerPlaneAlpha(float alpha);
    HWC2::Error SetLayerSidebandStream(const native_handle_t *stream);
    HWC2::Error SetLayerSourceCrop(hwc_frect_t crop);
    HWC2::Error SetLayerSurfaceDamage(hwc_region_t damage);
    HWC2::Error SetLayerTransform(int32_t transform);
    HWC2::Error SetLayerVisibleRegion(hwc_region_t visible);
    HWC2::Error SetLayerZOrder(uint32_t z);

   private:
    // sf_type_ stores the initial type given to us by surfaceflinger,
    // validated_type_ stores the type after running ValidateDisplay
    HWC2::Composition sf_type_ = HWC2::Composition::Invalid;
    HWC2::Composition validated_type_ = HWC2::Composition::Invalid;

    HWC2::BlendMode blending_ = HWC2::BlendMode::None;
    buffer_handle_t buffer_ = NULL;
    UniqueFd acquire_fence_;
    int release_fence_raw_ = -1;
    UniqueFd release_fence_;
    UniqueFd next_release_fence_;
    hwc_rect_t display_frame_;
    float alpha_ = 1.0f;
    hwc_frect_t source_crop_;
    int32_t cursor_x_;
    int32_t cursor_y_;
    HWC2::Transform transform_ = HWC2::Transform::None;
    uint32_t z_order_ = 0;
    android_dataspace_t dataspace_ = HAL_DATASPACE_UNKNOWN;
    std::string layer_name_;
    bool is_afbc_;

    uint32_t id_;
    DrmGralloc *drmGralloc_;
    DrmDevice *drm_;

    // Buffer info map
    bool bHasCache_ = false;
    std::map<uint64_t, std::shared_ptr<bufferInfo_t>> bufferInfoMap_;

    // Buffer info point
    std::shared_ptr<bufferInfo_t> pBufferInfo_;
  };

  struct HwcCallback {
    HwcCallback(hwc2_callback_data_t d, hwc2_function_pointer_t f)
        : data(d), func(f) {
    }
    hwc2_callback_data_t data;
    hwc2_function_pointer_t func;
  };

  class HwcDisplay {
   public:
    HwcDisplay(ResourceManager *resource_manager, DrmDevice *drm,
               std::shared_ptr<Importer> importer, hwc2_display_t handle,
               HWC2::DisplayType type);
    HwcDisplay(const HwcDisplay &) = delete;
    HWC2::Error Init();
    HWC2::Error CheckStateAndReinit();

    HWC2::Error RegisterVsyncCallback(hwc2_callback_data_t data,
                                      hwc2_function_pointer_t func);
    HWC2::Error UnregisterVsyncCallback();

    HWC2::Error RegisterInvalidateCallback(hwc2_callback_data_t data,
                                      hwc2_function_pointer_t func);

    HWC2::Error UnregisterInvalidateCallback();

    void ClearDisplay();
    void ReleaseResource();

    HWC2::Error CheckDisplayState();

    // HWC Hooks
    HWC2::Error AcceptDisplayChanges();
    HWC2::Error CreateLayer(hwc2_layer_t *layer);
    HWC2::Error DestroyLayer(hwc2_layer_t layer);
    HWC2::Error GetActiveConfig(hwc2_config_t *config);
    HWC2::Error GetChangedCompositionTypes(uint32_t *num_elements,
                                           hwc2_layer_t *layers,
                                           int32_t *types);
    HWC2::Error GetClientTargetSupport(uint32_t width, uint32_t height,
                                       int32_t format, int32_t dataspace);
    HWC2::Error GetColorModes(uint32_t *num_modes, int32_t *modes);
    HWC2::Error GetDisplayAttribute(hwc2_config_t config, int32_t attribute,
                                    int32_t *value);
    HWC2::Error GetDisplayConfigs(uint32_t *num_configs,
                                  hwc2_config_t *configs);
    HWC2::Error GetDisplayName(uint32_t *size, char *name);
    HWC2::Error GetDisplayRequests(int32_t *display_requests,
                                   uint32_t *num_elements, hwc2_layer_t *layers,
                                   int32_t *layer_requests);
    HWC2::Error GetDisplayType(int32_t *type);
    HWC2::Error GetDozeSupport(int32_t *support);
    HWC2::Error GetHdrCapabilities(uint32_t *num_types, int32_t *types,
                                   float *max_luminance,
                                   float *max_average_luminance,
                                   float *min_luminance);
    HWC2::Error GetReleaseFences(uint32_t *num_elements, hwc2_layer_t *layers,
                                 int32_t *fences);
    HWC2::Error PresentDisplay(int32_t *retire_fence);
    HWC2::Error SetActiveConfig(hwc2_config_t config);
    HWC2::Error ChosePreferredConfig();
    HWC2::Error SetClientTarget(buffer_handle_t target, int32_t acquire_fence,
                                int32_t dataspace, hwc_region_t damage);
    HWC2::Error SetColorMode(int32_t mode);
    HWC2::Error SetColorTransform(const float *matrix, int32_t hint);
    HWC2::Error SetOutputBuffer(buffer_handle_t buffer, int32_t release_fence);
    HWC2::Error SetPowerMode(int32_t mode);
    HWC2::Error SetVsyncEnabled(int32_t enabled);
    HWC2::Error ValidateDisplay(uint32_t *num_types, uint32_t *num_requests);
    std::map<hwc2_layer_t, HwcLayer> &get_layers(){
        return layers_;
    }
    bool has_layer(hwc2_layer_t layer) {
      return layers_.count(layer) > 0;
    }
    HwcLayer &get_layer(hwc2_layer_t layer) {
      return layers_.at(layer);
    }

   int DumpDisplayInfo(String8 &output);
   int DumpDisplayLayersInfo(String8 &output);
   int DumpDisplayLayersInfo();
   int DumpAllLayerData();
   bool PresentFinish(void) { return present_finish_; };
   int HoplugEventTmeline();
   int UpdateDisplayMode();
   int UpdateHdmiOutputFormat();
   int UpdateBCSH();
   int UpdateOverscan();
   int SwitchHdrMode();

   // Static Screen opt function
   int UpdateTimerEnable();
   int UpdateTimerState(bool gles_comp);
   int EntreStaticScreen(uint64_t refresh, int refresh_cnt);
   int InvalidateControl(uint64_t refresh, int refresh_cnt);

   private:
    HWC2::Error ValidatePlanes();
    HWC2::Error InitDrmHwcLayer();
    HWC2::Error CreateComposition();
    void AddFenceToRetireFence(int fd);

    ResourceManager *resource_manager_;
    DrmDevice *drm_;
    DrmDisplayCompositor compositor_;
    std::shared_ptr<Importer> importer_;
    std::unique_ptr<Planner> planner_;

    std::vector<DrmHwcLayer> drm_hwc_layers_;
    std::vector<DrmCompositionPlane> composition_planes_;

    std::vector<PlaneGroup*> plane_group;

    VSyncWorker vsync_worker_;
    InvalidateWorker invalidate_worker_;
    DrmConnector *connector_ = NULL;
    DrmCrtc *crtc_ = NULL;
    std::vector<DrmMode> sf_modes_;
    hwc2_display_t handle_;
    HWC2::DisplayType type_;
    uint32_t layer_idx_ = 1;
    std::map<hwc2_layer_t, HwcLayer> layers_;
    HwcLayer client_layer_;
    UniqueFd retire_fence_;
    UniqueFd next_retire_fence_;
    int32_t color_mode_;
    bool init_success_;
    bool validate_success_;
    bool present_finish_;
    hwc2_drm_display_t ctx_;
    bool static_screen_timer_enable_;
    bool static_screen_opt_;
    bool force_gles_;
    int fb_blanked;

    uint32_t frame_no_ = 0;
  };

  class DrmHotplugHandler : public DrmEventHandler {
   public:
    DrmHotplugHandler(DrmHwcTwo *hwc2, DrmDevice *drm)
        : hwc2_(hwc2), drm_(drm) {
    }
    void HandleEvent(uint64_t timestamp_us);

   private:
    DrmHwcTwo *hwc2_;
    DrmDevice *drm_;
  };

  static DrmHwcTwo *toDrmHwcTwo(hwc2_device_t *dev) {
    return static_cast<DrmHwcTwo *>(dev);
  }

  template <typename PFN, typename T>
  static hwc2_function_pointer_t ToHook(T function) {
    static_assert(std::is_same<PFN, T>::value, "Incompatible fn pointer");
    return reinterpret_cast<hwc2_function_pointer_t>(function);
  }

  template <typename T, typename HookType, HookType func, typename... Args>
  static T DeviceHook(hwc2_device_t *dev, Args... args) {
    DrmHwcTwo *hwc = toDrmHwcTwo(dev);
    return static_cast<T>(((*hwc).*func)(std::forward<Args>(args)...));
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t DisplayHook(hwc2_device_t *dev, hwc2_display_t display_handle,
                             Args... args) {
    DrmHwcTwo *hwc = toDrmHwcTwo(dev);
    if(hwc->displays_.count(display_handle)){
      HwcDisplay &display = hwc->displays_.at(display_handle);
      return static_cast<int32_t>((display.*func)(std::forward<Args>(args)...));
    }else{
      return static_cast<int32_t>(HWC2::Error::BadDisplay);
    }
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t LayerHook(hwc2_device_t *dev, hwc2_display_t display_handle,
                           hwc2_layer_t layer_handle, Args... args) {
    DrmHwcTwo *hwc = toDrmHwcTwo(dev);
    if(hwc->displays_.count(display_handle)){
      HwcDisplay &display = hwc->displays_.at(display_handle);
      if(display.has_layer(layer_handle)){
        HwcLayer &layer = display.get_layer(layer_handle);
        return static_cast<int32_t>((layer.*func)(std::forward<Args>(args)...));
      }else{
        return static_cast<int32_t>(HWC2::Error::BadLayer);
      }
    }else{
      return static_cast<int32_t>(HWC2::Error::BadDisplay);
    }
  }

  // hwc2_device_t hooks
  static int HookDevClose(hw_device_t *dev);
  static void HookDevGetCapabilities(hwc2_device_t *dev, uint32_t *out_count,
                                     int32_t *out_capabilities);
  static hwc2_function_pointer_t HookDevGetFunction(struct hwc2_device *device,
                                                    int32_t descriptor);

  // Device functions
  HWC2::Error CreateVirtualDisplay(uint32_t width, uint32_t height,
                                   int32_t *format, hwc2_display_t *display);
  HWC2::Error DestroyVirtualDisplay(hwc2_display_t display);
  void Dump(uint32_t *size, char *buffer);
  uint32_t GetMaxVirtualDisplayCount();
  HWC2::Error RegisterCallback(int32_t descriptor, hwc2_callback_data_t data,
                               hwc2_function_pointer_t function);
  HWC2::Error CreateDisplay(hwc2_display_t displ, HWC2::DisplayType type);
  void HandleDisplayHotplug(hwc2_display_t displayid, int state);
  void HandleInitialHotplugState(DrmDevice *drmDevice);

  static void StaticScreenOptHandler(int sig){
    if (sig == SIGALRM)
      if(g_ctx!=NULL){
        HwcDisplay &display = g_ctx->displays_.at(0);
        display.EntreStaticScreen(60,1);
    }
    return;
};

  ResourceManager *resource_manager_;
  std::map<hwc2_display_t, HwcDisplay> displays_;
  std::map<HWC2::Callback, HwcCallback> callbacks_;
  std::string mDumpString;
};
}  // namespace android
