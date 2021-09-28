/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwcomposer-drm"

#include "drmhwcomposer.h"
#include "drmeventlistener.h"
#include "drmresources.h"
#include "platform.h"
#include "virtualcompositorworker.h"
#include "vsyncworker.h"
#include "autolock.h"

#include <stdlib.h>

#include <cinttypes>
#include <map>
#include <vector>
#include <sstream>
#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifdef ANDROID_P
#include <log/log.h>
#include <libsync/sw_sync.h>
#include <android/sync.h>
#else
#include <cutils/log.h>
#include <sw_sync.h>
#include <sync/sync.h>
#endif

#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <utils/Trace.h>
#include <drm_fourcc.h>
#if RK_DRM_GRALLOC
#include "gralloc_drm_handle.h"
#endif
#include <linux/fb.h>

#include "hwc_util.h"
#include "hwc_rockchip.h"
#include <android/configuration.h>
#define UM_PER_INCH 25400

#if USE_GRALLOC_4
#include "drmgralloc4.h"
#endif


namespace android {

static int hwc_set_active_config(struct hwc_composer_device_1 *dev, int display,
                                 int index);

static int update_display_bestmode(hwc_drm_display_t *hd, int display, DrmConnector *c);

#if SKIP_BOOT
static unsigned int g_boot_cnt = 0;
#endif
static unsigned int g_boot_gles_cnt = 0;
static unsigned int g_extern_gles_cnt = 0;
static bool g_bSkipExtern = false;

#ifdef USE_HWC2
static bool g_hasHotplug = false;
// Must to wait hwc-set to send Hotplug event
// If you don't do this, there will be problems with the hot-plugging device registration and destruction timing.
// In severe cases, the fence fd will leak or the system will not respond.
static bool g_waitHwcSetHotplug = false;
#endif

//#if RK_INVALID_REFRESH
hwc_context_t* g_ctx = NULL;
//#endif

class DummySwSyncTimeline {
 public:
  int Init() {
    int ret = timeline_fd_.Set(sw_sync_timeline_create());
    if (ret < 0)
      return ret;
    return 0;
  }

  UniqueFd CreateDummyFence() {
    int ret = sw_sync_fence_create(timeline_fd_.get(), "dummy fence",
                                   timeline_pt_ + 1);
    if (ret < 0) {
      ALOGE("Failed to create dummy fence %d", ret);
      return ret;
    }

    UniqueFd ret_fd(ret);

    ret = sw_sync_timeline_inc(timeline_fd_.get(), 1);
    if (ret) {
      ALOGE("Failed to increment dummy sync timeline %d", ret);
      return ret;
    }

    ++timeline_pt_;
    return ret_fd;
  }

 private:
  UniqueFd timeline_fd_;
  int timeline_pt_ = 0;
};

struct CheckedOutputFd {
  CheckedOutputFd(int *fd, const char *description,
                  DummySwSyncTimeline &timeline)
      : fd_(fd), description_(description), timeline_(timeline) {
  }
  CheckedOutputFd(CheckedOutputFd &&rhs)
      : description_(rhs.description_), timeline_(rhs.timeline_) {
    std::swap(fd_, rhs.fd_);
  }

  CheckedOutputFd &operator=(const CheckedOutputFd &rhs) = delete;

  ~CheckedOutputFd() {
    if (fd_ == NULL)
      return;

    if (*fd_ >= 0)
      return;

    *fd_ = timeline_.CreateDummyFence().Release();

    if (*fd_ < 0)
      ALOGE("Failed to fill %s (%p == %d) before destruction",
            description_.c_str(), fd_, *fd_);
  }

 private:
  int *fd_ = NULL;
  std::string description_;
  DummySwSyncTimeline &timeline_;
};

// map of display:hwc_drm_display_t
typedef std::map<int, hwc_drm_display_t> DisplayMap;
class DrmHotplugHandler : public DrmEventHandler {
 public:
  void Init(DisplayMap* displays, DrmResources *drm, const struct hwc_procs *procs) {
    displays_ = displays;
    drm_ = drm;
    procs_ = procs;
    int ret = pthread_mutex_init(&lock_, NULL);
    if (ret) {
      ALOGE("Failed to initialize drm compositor lock %d\n", ret);
      return;
    }
  }

  void HandleEvent(uint64_t timestamp_us) {
    AutoLock lock(&lock_, __func__);
    if (lock.Lock())
      return;

    DrmConnector *extend = NULL;
    DrmConnector *primary = NULL;

    for (auto &conn : drm_->connectors()) {
      //In sleep mode,we need get raw connector state,otherwise,we will miss the chance
      //to handle hotplug event.
      //Pg: sleep (force_disconnect will be true)==> plug out HDMI ==> plug in HDMI ==> wake up (force_disconnect still be ture)
      //Workround: use raw connector state.
      drmModeConnection old_state = conn->raw_state();

      conn->UpdateModes();

      drmModeConnection cur_state = conn->raw_state();

      if (cur_state == old_state)
        continue;
      ALOGI("hwc_hotplug: %s event @%" PRIu64 " for connector %u type=%s, type_id=%d\n",
            cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug", timestamp_us,
            conn->id(),drm_->connector_type_str(conn->get_type()),conn->type_id());
      if (cur_state == DRM_MODE_CONNECTED) {
        /*
         * if connector is only one , only to use primary. by libin
         */
        if (drm_->connectors().size() == 1){
            primary = conn.get();
            ALOGI("connectors_.size()=%u only primary\n",(uint32_t)drm_->connectors().size());
        }else{
            if (conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT){
              ALOGD("hwc_hotplug: find the first connect external type=%s(%d)",
                drm_->connector_type_str(conn->get_type()), conn->type_id());
              extend = conn.get();
            }
            else if (conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT){
              ALOGD("hwc_hotplug: find the first connect primary type=%s(%d)",
                drm_->connector_type_str(conn->get_type()), conn->type_id());
              primary = conn.get();
            }
        }
      }
    }

    /*
     * status changed?
     */
    drm_->DisplayChanged();

    DrmConnector *old_primary = drm_->GetConnectorFromType(HWC_DISPLAY_PRIMARY);
    primary = primary ? primary : old_primary;
    if (!primary || primary->raw_state() != DRM_MODE_CONNECTED) {
      primary = NULL;
      for (auto &conn : drm_->connectors()) {
        if (!(conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT))
          continue;
        if (conn->raw_state() == DRM_MODE_CONNECTED) {
          primary = conn.get();
          ALOGD("hwc_hotplug: find the second connect primary type=%s(%d)",
            drm_->connector_type_str(conn->get_type()), conn->type_id());
          break;
        }
      }
    }

    if (!primary) {
      for (auto &conn : drm_->connectors()) {
        if (!(conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT))
          continue;
        ALOGD("hwc_hotplug: find the third primary type=%s(%d)",
            drm_->connector_type_str(conn->get_type()), conn->type_id());
        primary = conn.get();
      }
    }

    if (!primary) {
      ALOGE("hwc_hotplug: %s %d Failed to find primary display\n", __FUNCTION__, __LINE__);
      return;
    }

    //ClearDisplay show be called before display update(SetPrimaryDisplay/SetExtendDisplay).
    //It will singal the original disconnect display.
    drm_->ClearDisplay();
    if (primary != old_primary) {
      hwc_drm_display_t *hd = &(*displays_)[primary->display()];
      hwc_drm_display_t *old_hd = &(*displays_)[old_primary->display()];
      update_display_bestmode(hd, HWC_DISPLAY_PRIMARY, primary);
      DrmMode mode = primary->best_mode();
      primary->set_current_mode(mode);

      hd->framebuffer_width = old_hd->framebuffer_width;
      hd->framebuffer_height = old_hd->framebuffer_height;
      hd->rel_xres = mode.h_display();
      hd->rel_yres = mode.v_display();
      hd->v_total = mode.v_total();
      //rk: Avoid fb handle is null which lead HDMI display nothing with GLES.
      usleep(HOTPLUG_MSLEEP*1000);
      procs_->invalidate(procs_);

      drm_->SetPrimaryDisplay(primary);
    }

    DrmConnector *old_extend = drm_->GetConnectorFromType(HWC_DISPLAY_EXTERNAL);
    extend = extend ? extend : old_extend;
    if (!extend || extend->raw_state() != DRM_MODE_CONNECTED) {
      extend = NULL;
      for (auto &conn : drm_->connectors()) {
        if (!(conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT))
          continue;
        if (conn->id() == primary->id())
          continue;
        if (conn->raw_state() == DRM_MODE_CONNECTED) {
          extend = conn.get();
          ALOGD("hwc_hotplug: find the second connect external type=%s(%d)",
            drm_->connector_type_str(conn->get_type()), conn->type_id());
          break;
        }
      }
    }
    drm_->SetExtendDisplay(extend);

    if (!extend) {
#ifdef USE_HWC2
      g_waitHwcSetHotplug = false;
      procs_->invalidate(procs_);
      while(!g_waitHwcSetHotplug && g_hasHotplug){
          usleep(2000);
      }
#endif
      procs_->hotplug(procs_, HWC_DISPLAY_EXTERNAL, 0);

      /**********************long-running operations should move back of hotplug**************************/
      /*
       * If Connector changed ,update baseparameter , resolution , color.
       */
      hwc_get_baseparameter_config(NULL,0,BP_UPDATE,0);

      //When wake up from TV mode, it will bind crtc to TV connector.
      //If we also plug in HDMI,and don't bind crtc to HDMI connector,
      //the crtc of connected HDMI will be NULL. Which lead HDMI show nothing.
      //Pg: Defect #149666
      /**************************************************************************/
      //2. We should get current mode before UpdateDisplayRoute.
      //otherwise, it will lead crtc is disabled when current mode is 0
      //when boot system sometimes.
      drm_->UpdateDisplayRoute();

      //Update LUT from baseparameter when Hot_plug devices conneted
      hwc_SetGamma(drm_);

      //rk: Avoid fb handle is null which lead HDMI display nothing with GLES.
      usleep(HOTPLUG_MSLEEP*1000);
      procs_->invalidate(procs_);
      return;
    }

    //if extend is connected at boot time, to upload this hotplug event.
#ifdef USE_HWC2
    if( extend != old_extend || (!g_hasHotplug && extend != NULL)){
#else
    if( extend != old_extend){
#endif
      hwc_drm_display_t *hd = &(*displays_)[extend->display()];
      update_display_bestmode(hd, HWC_DISPLAY_EXTERNAL, extend);
      DrmMode mode = extend->best_mode();
      extend->set_current_mode(mode);

      char framebuffer_size[PROPERTY_VALUE_MAX];
      uint32_t width = 0, height = 0 , vrefresh = 0 ;
      property_get("persist." PROPERTY_TYPE ".framebuffer.aux", framebuffer_size, "use_baseparameter");
      /*
       * if unset framebuffer_size, get it from baseparameter , by libin
       */
      if(hwc_have_baseparameter() && !strcmp(framebuffer_size,"use_baseparameter")){
        int res = 0;
        res = hwc_get_baseparameter_config(framebuffer_size,HWC_DISPLAY_EXTERNAL,BP_FB_SIZE,0);
        if(res)
            ALOGW("BP: hwc get baseparameter config err ,res = %d",res);
      }

      sscanf(framebuffer_size, "%dx%d@%d", &width, &height, &vrefresh);
      if (width && height) {
        hd->framebuffer_width = width;
        hd->framebuffer_height = height;
        hd->vrefresh = vrefresh ? vrefresh : 60;
      } else if (mode.h_display() && mode.v_display() && mode.v_refresh()) {
        hd->framebuffer_width = mode.h_display();
        hd->framebuffer_height = mode.v_display();
        hd->vrefresh = mode.v_refresh();
        /*
         * Limit to 1080p if large than 2160p
         */
        if (hd->framebuffer_height >= 2160 && hd->framebuffer_width >= hd->framebuffer_height) {
          hd->framebuffer_width = hd->framebuffer_width * (1080.0 / hd->framebuffer_height);
          hd->framebuffer_height = 1080;
        }
      } else {
        hd->framebuffer_width = 1920;
        hd->framebuffer_height = 1080;
        hd->vrefresh = 60;
        ALOGE("Failed to find available display mode for display %d\n", HWC_DISPLAY_EXTERNAL);
      }

      hd->rel_xres = mode.h_display();
      hd->rel_yres = mode.v_display();
      hd->v_total = mode.v_total();
      hd->active = false;

      g_bSkipExtern = true;
      g_extern_gles_cnt = 0;
#ifdef USE_HWC2
      g_waitHwcSetHotplug = false;
      procs_->invalidate(procs_);
      while(!g_waitHwcSetHotplug && g_hasHotplug){
          usleep(2000);
      }
#endif
      procs_->hotplug(procs_, HWC_DISPLAY_EXTERNAL, 0);
      usleep(64 * 1000);
      hd->active = true;
      procs_->hotplug(procs_, HWC_DISPLAY_EXTERNAL, 1);
    }

    /**********************long-running operations should move back of hotplug**************************/
    /*
     * If Connector changed ,update baseparameter , resolution , color.
     */
    hwc_get_baseparameter_config(NULL,0,BP_UPDATE,0);

    //1. When wake up from TV mode, it will bind crtc to TV connector.
    //If we also plug in HDMI,and don't bind crtc to HDMI connector,
    //the crtc of connected HDMI will be NULL. Which lead HDMI show nothing.
    //Pg: Defect #149666
    /**************************************************************************/
    //2. We should get current mode before UpdateDisplayRoute.
    //otherwise, it will lead crtc is disabled when current mode is 0
    //when boot system sometimes.
    drm_->UpdateDisplayRoute();

    //Update LUT from baseparameter when Hot_plug devices conneted
    hwc_SetGamma(drm_);

#ifdef USE_HWC2
    if(!g_hasHotplug)
        g_hasHotplug = true;
#endif

    //rk: Avoid fb handle is null which lead HDMI display nothing with GLES.
    usleep(HOTPLUG_MSLEEP*1000);
    procs_->invalidate(procs_);
  }

 private:
  DrmResources *drm_ = NULL;
  const struct hwc_procs *procs_ = NULL;
  DisplayMap* displays_ = NULL;
  pthread_mutex_t lock_;
};


struct hwc_context_t {
  // map of display:hwc_drm_display_t
  typedef std::map<int, hwc_drm_display_t> DisplayMap;

  ~hwc_context_t() {
    virtual_compositor_worker.Exit();
  }

  hwc_composer_device_1_t device;
  hwc_procs_t const *procs = NULL;

  DisplayMap displays;
  DrmResources drm;
  std::unique_ptr<Importer> importer;
  const gralloc_module_t *gralloc;
  DummySwSyncTimeline dummy_timeline;
  VirtualCompositorWorker virtual_compositor_worker;
  DrmHotplugHandler hotplug_handler;
  VSyncWorker primary_vsync_worker;
  VSyncWorker extend_vsync_worker;

  int fb_fd;
  int fb_blanked;
  int hdmi_status_fd;
  int dp_status_fd;
#if RK_CTS_WORKROUND
  FILE* regFile;
#endif
    bool                isGLESComp;
#if RK_INVALID_REFRESH
    bool                mOneWinOpt;
    threadPamaters      mRefresh;
#endif

#if RK_STEREO
    bool is_3d;
    //int fd_3d;
    //threadPamaters mControlStereo;
#endif
    bool hdr_video_compose_by_gles = false;

    std::vector<DrmCompositionDisplayPlane> comp_plane_group;
    std::vector<DrmHwcDisplayContents> layer_contents;
};

static void  *hotplug_event_thread(void *arg){
  hwc_context_t* ctx = (hwc_context_t*)arg;
  ctx->hotplug_handler.HandleEvent(0);
  pthread_exit(NULL);
  return NULL;
}

/**
 * sys.3d_resolution.main 1920x1080p60-114693:148500
 * width x height p|i refresh-flag:clock
 */
static int update_display_bestmode(hwc_drm_display_t *hd, int display, DrmConnector *c)
{
  char resolution[PROPERTY_VALUE_MAX];
  char resolution_3d[PROPERTY_VALUE_MAX];
  uint32_t width, height, flags;
  uint32_t hsync_start, hsync_end, htotal;
  uint32_t vsync_start, vsync_end, vtotal;
  uint32_t width_3d, height_3d, vrefresh_3d, flag_3d, clk_3d;
  bool interlaced, interlaced_3d;
  float vrefresh;
  char val,val_3d;
  int timeline;
  static uint32_t last_mainType,last_auxType;
  uint32_t MaxResolution = 0,temp;

  timeline = property_get_int32( PROPERTY_TYPE ".display.timeline", -1);
  /*
   * force update propetry when timeline is zero or not exist.
   */
  if (timeline && timeline == hd->display_timeline &&
      hd->hotplug_timeline == hd->ctx->drm.timeline())
    return 0;
  hd->display_timeline = timeline;
  hd->hotplug_timeline = hd->ctx->drm.timeline();

  if (display == HWC_DISPLAY_PRIMARY)
  {
    if(hwc_have_baseparameter() && c->get_type() != last_mainType)
    {
        property_set("persist." PROPERTY_TYPE ".resolution.main","use_baseparameter");
        ALOGD("BP:DisplayDevice change type[%d] => type[%d],to update main resolution",last_mainType,c->get_type());
        last_mainType = c->get_type();
    }
    /* if baseparameter exist to use it, if none set to "Auto" */
    if(hwc_have_baseparameter()){
      property_get("persist." PROPERTY_TYPE ".resolution.main", resolution, "use_baseparameter");
      property_get( PROPERTY_TYPE ".3d_resolution.main", resolution_3d, "0x0p0-0:0");
      if(!(strcmp(resolution,"use_baseparameter"))){
        int res = 0;
        res = hwc_get_baseparameter_config(resolution,display,BP_RESOLUTION,c->get_type());
        if(res){
            ALOGE("BP:Get main BP_RESOLUTION fail, res = %d line =%d",res,__LINE__);
        }
      }
    }else{
      property_get("persist." PROPERTY_TYPE ".resolution.main", resolution, "Auto");
      property_get( PROPERTY_TYPE ".3d_resolution.main", resolution_3d, "0x0p0-0:0");
    }
  }
  else
  {
    if(hwc_have_baseparameter() && c->get_type() != last_auxType)
    {
        property_set("persist." PROPERTY_TYPE ".resolution.aux","use_baseparameter");
        ALOGD("BP:DisplayDevice change type[%d] => type[%d],to update aux resolution",last_auxType,c->get_type());
        last_auxType = c->get_type();
    }
    /* if baseparameter exist to use it, if none set to "Auto" */
    if(hwc_have_baseparameter()){
      property_get("persist." PROPERTY_TYPE ".resolution.aux", resolution, "use_baseparameter");
      property_get( PROPERTY_TYPE ".3d_resolution.aux", resolution_3d, "0x0p0-0:0");
      if(!(strcmp(resolution,"use_baseparameter"))){
        int res = 0;
        res = hwc_get_baseparameter_config(resolution,display,BP_RESOLUTION,c->get_type());
        if(res){
            ALOGE("BP:Get aux BP_RESOLUTION fail, res = %d line =%d",res,__LINE__);
        }
      }
    }else{
      property_get("persist." PROPERTY_TYPE ".resolution.aux", resolution, "Auto");
      property_get( PROPERTY_TYPE ".3d_resolution.aux", resolution_3d, "0x0p0-0:0");
    }
  }
  hwc_set_baseparameter_config(&hd->ctx->drm);

  if(hd->is_3d && strcmp(resolution_3d,"0x0p0-0:0"))
  {
    ALOGD_IF(log_level(DBG_DEBUG), "Enter 3d resolution=%s",resolution_3d);
    sscanf(resolution_3d, "%dx%d%c%d-%d:%d", &width_3d, &height_3d, &val_3d,
          &vrefresh_3d, &flag_3d, &clk_3d);

    if (val_3d == 'i')
      interlaced_3d = true;
    else
      interlaced_3d = false;

    if (width_3d != 0 && height_3d != 0) {
      //use raw mode,otherwise it may filter by resolution_white.xml
      for (const DrmMode &conn_mode : c->raw_modes()) {
        if (conn_mode.equal(width_3d, height_3d, vrefresh_3d,  flag_3d, clk_3d, interlaced_3d)) {
          ALOGD_IF(log_level(DBG_DEBUG), "Match 3D parameters: w=%d,h=%d,val=%c,vrefresh_3d=%d,flag=%d,clk=%d",
                width_3d,height_3d,val_3d,vrefresh_3d,flag_3d,clk_3d);
          c->set_best_mode(conn_mode);
          return 0;
        }
      }
    }
  }
  else if (strcmp(resolution,"Auto") != 0)
  {
    int len = sscanf(resolution, "%dx%d@%f-%d-%d-%d-%d-%d-%d-%x",
                     &width, &height, &vrefresh, &hsync_start,
                     &hsync_end, &htotal, &vsync_start,&vsync_end,
                     &vtotal, &flags);
    if (len == 10 && width != 0 && height != 0) {
      for (const DrmMode &conn_mode : c->modes()) {
        if (conn_mode.equal(width, height, vrefresh, hsync_start, hsync_end,
                            htotal, vsync_start, vsync_end, vtotal, flags)) {
          c->set_best_mode(conn_mode);
          return 0;
        }
      }
    }

    uint32_t ivrefresh;
    len = sscanf(resolution, "%dx%d%c%d", &width, &height, &val, &ivrefresh);

    if (val == 'i')
      interlaced = true;
    else
      interlaced = false;
    if (len == 4 && width != 0 && height != 0) {
      for (const DrmMode &conn_mode : c->modes()) {
        if (conn_mode.equal(width, height, ivrefresh, interlaced)) {
          c->set_best_mode(conn_mode);
          return 0;
        }
      }
    }
  }

  for (const DrmMode &conn_mode : c->modes()) {
    if (conn_mode.type() & DRM_MODE_TYPE_PREFERRED) {
      c->set_best_mode(conn_mode);
      return 0;
    }
    else {
      temp = conn_mode.h_display()*conn_mode.v_display();
      if(MaxResolution <= temp)
        MaxResolution = temp;
    }
  }
  for (const DrmMode &conn_mode : c->modes()) {
    if(MaxResolution == conn_mode.h_display()*conn_mode.v_display()) {
      c->set_best_mode(conn_mode);
      return 0;
    }
  }

  //use raw modes to get mode.
  for (const DrmMode &conn_mode : c->raw_modes()) {
    if (conn_mode.type() & DRM_MODE_TYPE_PREFERRED) {
      c->set_best_mode(conn_mode);
      return 0;
    }
    else {
      temp = conn_mode.h_display()*conn_mode.v_display();
      if(MaxResolution <= temp)
        MaxResolution = temp;
    }
  }
  for (const DrmMode &conn_mode : c->raw_modes()) {
    if(MaxResolution == conn_mode.h_display()*conn_mode.v_display()) {
      c->set_best_mode(conn_mode);
      return 0;
    }
  }

  ALOGE("Error: Should not get here display=%d %s %d\n", display, __FUNCTION__, __LINE__);
  DrmMode mode;
  c->set_best_mode(mode);

  return -ENOENT;
}

static native_handle_t *dup_buffer_handle(buffer_handle_t handle) {
  native_handle_t *new_handle =
      native_handle_create(handle->numFds, handle->numInts);
  if (new_handle == NULL)
    return NULL;

  const int *old_data = handle->data;
  int *new_data = new_handle->data;
  for (int i = 0; i < handle->numFds; i++) {
    *new_data = dup(*old_data);
    old_data++;
    new_data++;
  }
  memcpy(new_data, old_data, sizeof(int) * handle->numInts);

  return new_handle;
}

static void free_buffer_handle(native_handle_t *handle) {
  int ret = native_handle_close(handle);
  if (ret)
    ALOGE("Failed to close native handle %d", ret);
  ret = native_handle_delete(handle);
  if (ret)
    ALOGE("Failed to delete native handle %d", ret);
}

const hwc_drm_bo *DrmHwcBuffer::operator->() const {
  if (importer_ == NULL) {
    ALOGE("Access of non-existent BO");
    exit(1);
    return NULL;
  }
  return &bo_;
}

void DrmHwcBuffer::Clear() {
  if (importer_ != NULL) {
    importer_->ReleaseBuffer(&bo_);
    importer_ = NULL;
  }
}

int DrmHwcBuffer::ImportBuffer(buffer_handle_t handle, Importer *importer
#if RK_VIDEO_SKIP_LINE
, uint32_t SkipLine
#endif
) {
  hwc_drm_bo tmp_bo;

  int ret = importer->ImportBuffer(handle, &tmp_bo
#if RK_VIDEO_SKIP_LINE
  , SkipLine
#endif
  );
  if (ret)
    return ret;

  if (importer_ != NULL) {
    importer_->ReleaseBuffer(&bo_);
  }

  importer_ = importer;

  bo_ = tmp_bo;

  return 0;
}

int DrmHwcNativeHandle::CopyBufferHandle(buffer_handle_t handle,
                                         const gralloc_module_t *gralloc) {
#if USE_GRALLOC_4
      /* import 'handle' 得到对应的 新的 imported 的 buffer_handle_t 实例. */
      buffer_handle_t handle_copy;
      status_t ret = gralloc4::importBuffer(handle, &handle_copy);
      if ( ret != NO_ERROR )
      {
          ALOGE("err. ret : %d", ret);
          return ret;
      }
#else   // USE_GRALLOC_4
  native_handle_t *handle_copy = dup_buffer_handle(handle);
  if (handle_copy == NULL) {
    ALOGE("Failed to duplicate handle");
    return -ENOMEM;
  }

  int ret = gralloc->registerBuffer(gralloc, handle_copy);
  if (ret) {
    ALOGE("Failed to register buffer handle %d", ret);
    free_buffer_handle(handle_copy);
    return ret;
  }
#endif  // USE_GRALLOC_4

  Clear();

  gralloc_ = gralloc;
  handle_ = const_cast<native_handle_t*>(handle_copy);

  return 0;
}

DrmHwcNativeHandle::~DrmHwcNativeHandle() {
  Clear();
}

void DrmHwcNativeHandle::Clear() {
#if USE_GRALLOC_4
      if ( handle_ != NULL )
      {
          gralloc4::freeBuffer(handle_);
          gralloc_ = NULL;
          handle_ = NULL;
      }
#else   // USE_GRALLOC_4

  if (gralloc_ != NULL && handle_ != NULL) {
    gralloc_->unregisterBuffer(gralloc_, handle_);
    free_buffer_handle(handle_);
    gralloc_ = NULL;
    handle_ = NULL;
  }
#endif  // USE_GRALLOC_4

}

static const char *DrmFormatToString(uint32_t drm_format) {
  switch (drm_format) {
    case DRM_FORMAT_BGR888:
      return "DRM_FORMAT_BGR888";
    case DRM_FORMAT_ARGB8888:
      return "DRM_FORMAT_ARGB8888";
    case DRM_FORMAT_XBGR8888:
      return "DRM_FORMAT_XBGR8888";
    case DRM_FORMAT_ABGR8888:
      return "DRM_FORMAT_ABGR8888";
    case DRM_FORMAT_BGR565:
      return "DRM_FORMAT_BGR565";
    case DRM_FORMAT_YVU420:
      return "DRM_FORMAT_YVU420";
    case DRM_FORMAT_NV12:
      return "DRM_FORMAT_NV12";
    default:
      return "<invalid>";
  }
}

static void DumpBuffer(const DrmHwcBuffer &buffer, std::ostringstream *out) {
  if (!buffer) {
    *out << "buffer=<invalid>";
    return;
  }

  *out << "buffer[w/h/format]=";
  *out << buffer->width << "/" << buffer->height << "/" << DrmFormatToString(buffer->format);
}

static const char *TransformToString(uint32_t transform) {
  switch (transform) {
    case DrmHwcTransform::kIdentity:
      return "IDENTITY";
    case DrmHwcTransform::kFlipH:
      return "FLIPH";
    case DrmHwcTransform::kFlipV:
      return "FLIPV";
    case DrmHwcTransform::kRotate90:
      return "ROTATE90";
    case DrmHwcTransform::kRotate180:
      return "ROTATE180";
    case DrmHwcTransform::kRotate270:
      return "ROTATE270";
    default:
      return "<invalid>";
  }
}

const char *BlendingToString(DrmHwcBlending blending) {
  switch (blending) {
    case DrmHwcBlending::kNone:
      return "NONE";
    case DrmHwcBlending::kPreMult:
      return "PREMULT";
    case DrmHwcBlending::kCoverage:
      return "COVERAGE";
    default:
      return "<invalid>";
  }
}

void DrmHwcLayer::dump_drm_layer(int index, std::ostringstream *out) const {
    *out << "DrmHwcLayer[" << index << "] ";
    DumpBuffer(buffer,out);

    *out << " transform=" << TransformToString(transform)
         << " blending[a=" << (int)alpha
         << "]=" << BlendingToString(blending) << " source_crop";
    source_crop.Dump(out);
    *out << " handle parameter";
    *out << "[w/h/s]=" << width << "/" << height << "/" << stride;
    *out << " display_frame";
    display_frame.Dump(out);

    *out << "\n";
}

int DrmHwcLayer::InitFromHwcLayer(struct hwc_context_t *ctx, int display, hwc_layer_1_t *sf_layer, Importer *importer,
                                  const gralloc_module_t *gralloc, bool bClone) {
    DrmConnector *c;
    DrmMode mode;
    unsigned int size;
    int ret = 0;
    int src_w, src_h, dst_w, dst_h;
    hwc_region_t * visible_region = &sf_layer->visibleRegionScreen;
    hwc_rect_t const * visible_rects = visible_region->rects;
    int left_min = 0, top_min = 0, right_max = 0, bottom_max=0;

    UN_USED(importer);

    bClone_ = bClone;
#if RK_3D_VIDEO
    int32_t alreadyStereo = 0;
#ifdef USE_HWC2
    if(sf_layer->handle)
    {
        alreadyStereo = hwc_get_handle_alreadyStereo(ctx->gralloc, sf_layer->handle);
        if(alreadyStereo < 0)
        {
            ALOGE("hwc_get_handle_alreadyStereo fail");
            alreadyStereo = 0;
        }
    }
#else
    alreadyStereo = sf_layer->alreadyStereo;
#endif
  stereo = alreadyStereo;
#endif

  if(sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET)
   bFbTarget_ = true;
  else
   bFbTarget_ = false;

  if(sf_layer->flags & HWC_SKIP_LAYER)
    bSkipLayer = true;
  else
    bSkipLayer = false;
#if RK_VIDEO_SKIP_LINE
  SkipLine = 0;
#endif
  bUse = true;
  sf_handle = sf_layer->handle;
  raw_sf_layer = sf_layer;
  mlayer = sf_layer;
  alpha = sf_layer->planeAlpha;
  frame_no = get_frame();


  DrmConnector *conn = ctx->drm.GetConnectorFromType(display);
  if (!conn) {
    ALOGE("%s:Failed to get connector for display %d line=%d", __FUNCTION__,display,__LINE__);
    return -ENODEV;
  }

  hwc_drm_display_t *hd = &ctx->displays[conn->display()];

#if DUAL_VIEW_MODE
  int dualModeEnable = 0,dualModeTB = 0,dualModeRatioPri = 0,dualModeRatioAux = 0;
  char value[PROPERTY_VALUE_MAX];
  property_get("persist." PROPERTY_TYPE ".dualModeEnable", value, "0");
  dualModeEnable = atoi(value);
  property_get("persist." PROPERTY_TYPE ".dualModeTB", value, "0");
  dualModeTB = atoi(value);
  property_get("persist." PROPERTY_TYPE ".dualModeRatioPri", value, "0");
  dualModeRatioPri = atoi(value);
  property_get("persist." PROPERTY_TYPE ".dualModeRatioAux", value, "0");
  dualModeRatioAux = atoi(value);

  //DUAL_VIEW_MODE only support 2 or 3 Ratio
  if(dualModeRatioPri == 0 || dualModeRatioAux == 0){
    ALOGE_IF(log_level(DBG_ERROR),"DUAL:Not support 0 Ration (%d:%d) , disable DUAL_VIEW_MODE",dualModeRatioPri,dualModeRatioAux);
    dualModeEnable = 0;
  }

  //DUAL_VIEW_MODE Primary framebuffer must equal to Extend
  char framebuffer_size_pri[PROPERTY_VALUE_MAX] = {0};
  char framebuffer_size_aux[PROPERTY_VALUE_MAX] = {0};
  property_get("persist." PROPERTY_TYPE ".framebuffer.main", framebuffer_size_pri, "main");
  property_get("persist." PROPERTY_TYPE ".framebuffer.aux", framebuffer_size_aux, "aux");
  if(strcmp(framebuffer_size_pri,framebuffer_size_aux)){
    ALOGE_IF(log_level(DBG_ERROR),"DUAL:Primary framebuffer is not  equal to Extend, disable DUAL_VIEW_MODE");
    dualModeEnable = 0;
  }

  if(dualModeEnable == 1){
    hd->bDualViewMode = true;
    property_set( PROPERTY_TYPE ".hwc.compose_policy","0");
    if(display == 0){
      if(dualModeTB == 1)
        source_crop = DrmHwcRect<float>(
          sf_layer->sourceCropf.left, sf_layer->sourceCropf.top,
          sf_layer->sourceCropf.right, sf_layer->sourceCropf.bottom / (dualModeRatioPri + dualModeRatioAux) * dualModeRatioPri);
      else
        source_crop = DrmHwcRect<float>(
          sf_layer->sourceCropf.left, sf_layer->sourceCropf.top,
          sf_layer->sourceCropf.right / (dualModeRatioPri + dualModeRatioAux) * dualModeRatioPri , sf_layer->sourceCropf.bottom);
    }else if(display == 1){
      if(dualModeTB == 1)
        source_crop = DrmHwcRect<float>(
          sf_layer->sourceCropf.left, sf_layer->sourceCropf.top + sf_layer->sourceCropf.bottom / (dualModeRatioPri + dualModeRatioAux) * dualModeRatioPri,
          sf_layer->sourceCropf.right, sf_layer->sourceCropf.bottom);
      else
        source_crop = DrmHwcRect<float>(
          sf_layer->sourceCropf.left + sf_layer->sourceCropf.right / (dualModeRatioPri + dualModeRatioAux) * dualModeRatioPri , sf_layer->sourceCropf.top,
          sf_layer->sourceCropf.right, sf_layer->sourceCropf.bottom);

    }
  }else
#endif

  {
    source_crop = DrmHwcRect<float>(
        sf_layer->sourceCropf.left, sf_layer->sourceCropf.top,
        sf_layer->sourceCropf.right, sf_layer->sourceCropf.bottom);

  }

  if(bClone)
  {
      //int panle_height = hd->rel_yres + hd->v_total;
      //int y_offset =  (panle_height - panle_height * 3 / 147) / 2 + panle_height * 3 / 147;
      int y_offset = hd->v_total;
      display_frame = DrmHwcRect<int>(
          hd->w_scale * sf_layer->displayFrame.left, hd->h_scale * sf_layer->displayFrame.top + y_offset,
          hd->w_scale * sf_layer->displayFrame.right, hd->h_scale * sf_layer->displayFrame.bottom + y_offset);
  }
  else
  {
      if(stereo == FPS_3D)
      {
        int y_offset = hd->v_total;
        display_frame = DrmHwcRect<int>(
          hd->w_scale * sf_layer->displayFrame.left, hd->h_scale * sf_layer->displayFrame.top,
          hd->w_scale * sf_layer->displayFrame.right, hd->h_scale * sf_layer->displayFrame.bottom + y_offset);
      }
      else
      {
        display_frame = DrmHwcRect<int>(
          hd->w_scale * sf_layer->displayFrame.left, hd->h_scale * sf_layer->displayFrame.top,
          hd->w_scale * sf_layer->displayFrame.right, hd->h_scale * sf_layer->displayFrame.bottom);
      }
  }

  src_w = (int)(source_crop.right - source_crop.left);
  src_h = (int)(source_crop.bottom - source_crop.top);
  dst_w = (int)(display_frame.right - display_frame.left);
  dst_h = (int)(display_frame.bottom - display_frame.top);

    if(hd->is_interlaced)
    {
        //use vop plane scale instead of vop post scale.
        char overscan[PROPERTY_VALUE_MAX];
        int left_margin, right_margin, top_margin, bottom_margin;
        float left_margin_f, right_margin_f, top_margin_f, bottom_margin_f;
        float lscale = 0, tscale = 0, rscale = 0, bscale = 0;
        int disp_old_l,disp_old_t,disp_old_r,disp_old_b;

        if(hd->stereo_mode != NON_3D)
        {
            left_margin = 100;
            top_margin = 100;
            right_margin = 100;
            bottom_margin = 100;
        }
        else
        {
          if (display == HWC_DISPLAY_PRIMARY){
            if(hwc_have_baseparameter()){
              property_get("persist." PROPERTY_TYPE ".overscan.main", overscan, "use_baseparameter");
              if(!strcmp(overscan,"use_baseparameter"))
                hwc_get_baseparameter_config(overscan,display,BP_OVERSCAN,0);
            }else{
              property_get("persist." PROPERTY_TYPE ".overscan.main", overscan, "overscan 100,100,100,100");
            }
            sscanf(overscan, "overscan %d,%d,%d,%d", &left_margin, &top_margin,
                              &right_margin, &bottom_margin);
          }else{
            if(hwc_have_baseparameter()){
              property_get("persist." PROPERTY_TYPE ".overscan.aux", overscan, "use_baseparameter");
              if(!strcmp(overscan,"use_baseparameter"))
                hwc_get_baseparameter_config(overscan,display,BP_OVERSCAN,0);
            }else{
              property_get("persist." PROPERTY_TYPE ".overscan.aux", overscan, "overscan 100,100,100,100");
            }
           sscanf(overscan, "overscan %d,%d,%d,%d", &left_margin, &top_margin,
                    &right_margin, &bottom_margin);
          }
        }

        //limit overscan to (OVERSCAN_MIN_VALUE,OVERSCAN_MAX_VALUE)
        if (left_margin   < OVERSCAN_MIN_VALUE) left_margin   = OVERSCAN_MIN_VALUE;
        if (top_margin    < OVERSCAN_MIN_VALUE) top_margin    = OVERSCAN_MIN_VALUE;
        if (right_margin  < OVERSCAN_MIN_VALUE) right_margin  = OVERSCAN_MIN_VALUE;
        if (bottom_margin < OVERSCAN_MIN_VALUE) bottom_margin = OVERSCAN_MIN_VALUE;

        if (left_margin   > OVERSCAN_MAX_VALUE) left_margin   = OVERSCAN_MAX_VALUE;
        if (top_margin    > OVERSCAN_MAX_VALUE) top_margin    = OVERSCAN_MAX_VALUE;
        if (right_margin  > OVERSCAN_MAX_VALUE) right_margin  = OVERSCAN_MAX_VALUE;
        if (bottom_margin > OVERSCAN_MAX_VALUE) bottom_margin = OVERSCAN_MAX_VALUE;

        left_margin_f   = (float)(100 - left_margin   ) / 2;
        top_margin_f    = (float)(100 - top_margin    ) / 2;
        right_margin_f  = (float)(100 - right_margin  ) / 2;
        bottom_margin_f = (float)(100 - bottom_margin) / 2;

        lscale = ((float)left_margin_f   / 100);
        tscale = ((float)top_margin_f    / 100);
        rscale = ((float)right_margin_f  / 100);
        bscale = ((float)bottom_margin_f / 100);

        disp_old_l = display_frame.left;
        disp_old_t = display_frame.top;
        disp_old_r = display_frame.right;
        disp_old_b = display_frame.bottom;

        display_frame.left = ((int)(display_frame.left  * (1.0 - lscale - rscale)) + (int)(hd->rel_xres * lscale));
        display_frame.top =  ((int)(display_frame.top   * (1.0 - tscale - bscale)) + (int)(hd->rel_yres * tscale));
        dst_w -= ((int)(dst_w * lscale) + (int)(dst_w * rscale));
        dst_h -= ((int)(dst_h * tscale) + (int)(dst_h * bscale));
        display_frame.right  = display_frame.left + dst_w;
        display_frame.bottom = display_frame.top  + dst_h;

        ALOGD_IF(log_level(DBG_VERBOSE),"vop plane scale overscan, display margin(%f,%f,%f,%f) scale_factor(%f,%f,%f,%f) disp_area(%d,%d,%d,%d) ==> (%d,%d,%d,%d)",
            left_margin_f,top_margin_f,right_margin_f,bottom_margin_f,
            lscale,tscale,rscale,bscale,
            disp_old_l,disp_old_t,disp_old_r,disp_old_b,
            display_frame.left,display_frame.top,display_frame.right,display_frame.bottom);
    }

    c = ctx->drm.GetConnectorFromType(HWC_DISPLAY_PRIMARY);
    if (!c) {
        ALOGE("Failed to get DrmConnector for display %d", 0);
        return -ENODEV;
    }
    mode = c->active_mode();

    if(sf_handle)
    {
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
        width = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_WIDTH);
        height = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_HEIGHT);
        stride = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_STRIDE);
        format = hwc_get_handle_attibute(gralloc,sf_layer->handle,ATT_FORMAT);
#else
        width = hwc_get_handle_width(gralloc,sf_layer->handle);
        height = hwc_get_handle_height(gralloc,sf_layer->handle);
        stride = hwc_get_handle_stride(gralloc,sf_layer->handle);
        format = hwc_get_handle_format(gralloc,sf_layer->handle);
#endif
    }
    else
    {
        format = HAL_PIXEL_FORMAT_RGBA_8888;
    }

    if(format == HAL_PIXEL_FORMAT_YCrCb_NV12 || format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
        is_yuv = true;
    else
        is_yuv = false;

    rect_merge.left = display_frame.left;
    rect_merge.top = display_frame.top;
    rect_merge.right = display_frame.right;
    rect_merge.bottom = display_frame.bottom;

    if(visible_rects && format != HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO
            && format != HAL_PIXEL_FORMAT_YCrCb_NV12){
        left_min = visible_rects[0].left;
        top_min = visible_rects[0].top;
        right_max = visible_rects[0].right;
        bottom_max = visible_rects[0].bottom;

        for (int r = 0; r < (int) visible_region->numRects; r++) {
            int r_left;
            int r_top;
            int r_right;
            int r_bottom;

            r_left = hwcMAX(display_frame.left, visible_rects[r].left);
            left_min = hwcMIN(r_left, left_min);
            r_top = hwcMAX(display_frame.top, visible_rects[r].top);
            top_min = hwcMIN(r_top, top_min);
            r_right = hwcMIN(display_frame.right, visible_rects[r].right);
            right_max = hwcMAX(r_right, right_max);
            r_bottom = hwcMIN(display_frame.bottom, visible_rects[r].bottom);
            bottom_max  = hwcMAX(r_bottom, bottom_max);
        }

        rect_merge.left = hwcMAX(display_frame.left, left_min);
        rect_merge.top = hwcMAX(display_frame.top, top_min);
        rect_merge.right =  hwcMIN(display_frame.right, right_max);
        rect_merge.bottom = hwcMIN(display_frame.bottom, bottom_max);
    }

    if(hd->hasEotfPlane)
    {
        if(is_yuv)
        {
            uint32_t android_colorspace = hwc_get_layer_colorspace(sf_layer);
            colorspace = colorspace_convert_to_linux(android_colorspace);
            if(colorspace == 0)
            {
                colorspace = V4L2_COLORSPACE_DEFAULT;
            }
            if((android_colorspace & HAL_DATASPACE_TRANSFER_MASK) == HAL_DATASPACE_TRANSFER_ST2084)
            {
                ALOGD_IF(log_level(DBG_VERBOSE),"%s:line=%d has st2084",__FUNCTION__,__LINE__);
                eotf = SMPTE_ST2084;
            }
            else
            {
                //ALOGE("Unknow etof %d",eotf);
                eotf = TRADITIONAL_GAMMA_SDR;
            }
        }
        else
        {
            //If enter GLES in HDR video,fake the fb target layer to HDR.
            if(hd->isHdr && bFbTarget_)
            {
                colorspace = V4L2_COLORSPACE_BT2020;
                eotf = SMPTE_ST2084;
            }
            else
            {
                colorspace = V4L2_COLORSPACE_DEFAULT;
                eotf = TRADITIONAL_GAMMA_SDR;
            }
        }
    }
    else
    {
        if(is_yuv)
        {
            uint32_t android_colorspace = hwc_get_layer_colorspace(sf_layer);
            colorspace = colorspace_convert_to_linux(android_colorspace);
            if(colorspace == 0)
            {
                colorspace = V4L2_COLORSPACE_DEFAULT;
            }
            eotf = TRADITIONAL_GAMMA_SDR;
        }else{
            colorspace = V4L2_COLORSPACE_DEFAULT;
            eotf = TRADITIONAL_GAMMA_SDR;
        }
    }

#if RK_BOX
    if(is_yuv){
      char value_yuv[PROPERTY_VALUE_MAX];
      int scaleMode = 0;
      property_get("persist." PROPERTY_TYPE ".video.cvrs",value_yuv, "0");
      scaleMode = atoi(value_yuv);
      if(scaleMode > 0){
          ret = hwc_video_to_area(source_crop,display_frame,scaleMode);
          if(ret == false)
          ALOGE("hwc video to area fail !! reset to full screen");
      }
    }
#endif



    if((sf_layer->transform == HWC_TRANSFORM_ROT_90)
        ||(sf_layer->transform == HWC_TRANSFORM_ROT_270)){
        if(format == HAL_PIXEL_FORMAT_YCrCb_NV12 || format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
        {
            //rga need this alignment.
            src_h = ALIGN_DOWN(src_h, 8);
            src_w = ALIGN_DOWN(src_w, 2);
        }
         h_scale_mul = (float) (src_h)/(dst_w);
         v_scale_mul = (float) (src_w)/(dst_h);
     } else {
         h_scale_mul = (float) (src_w)/(dst_w);
         v_scale_mul = (float) (src_h)/(dst_h);
     }

#if RK_VIDEO_SKIP_LINE
    if(format == HAL_PIXEL_FORMAT_YCrCb_NV12 || format == HAL_PIXEL_FORMAT_YCrCb_NV12_10){
      if(width >= 3840){
        if(h_scale_mul > 1.0 || v_scale_mul > 1.0){
            SkipLine = 2;
        }
        if(format == HAL_PIXEL_FORMAT_YCrCb_NV12_10 && h_scale_mul >= (3840 / 1600)){
            SkipLine = 3;
        }
      }
      int video_skipline = property_get_int32("vendor.video.skipline", 0);
      if (video_skipline == 2){
        SkipLine = 2;
      }else if(video_skipline == 3){
        SkipLine = 3;
      }
    }
#endif

    is_scale = (h_scale_mul != 1.0) || (v_scale_mul != 1.0);
    is_match = false;
    is_take = false;
#if USE_AFBC_LAYER
    is_afbc = false;
#endif
#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
    is_rotate_by_rga = false;
#endif
    bMix = false;
    bpp = android::bytesPerPixel(format);
    size = (source_crop.right - source_crop.left) * (source_crop.bottom - source_crop.top) * bpp;
    is_large = (mode.h_display()*mode.v_display()*4*3/4 > size)? true:false;

#if RK_PRINT_LAYER_NAME
    char layername[100];
#ifdef USE_HWC2
    if(sf_handle)
    {
            HWC_GET_HANDLE_LAYERNAME(gralloc,sf_layer,sf_handle, layername, 100);
    }
#else
    strcpy(layername, sf_layer->LayerName);
#endif
    name = layername;
#endif


    ALOGV("\t sourceCropf(%f,%f,%f,%f)",source_crop.left,source_crop.top,source_crop.right,source_crop.bottom);
    ALOGV("h_scale_mul=%f,v_scale_mul=%f,is_scale=%d,is_large=%d",h_scale_mul,v_scale_mul,is_scale,is_large);

  transform = 0;

  // 270* and 180* cannot be combined with flips. More specifically, they
  // already contain both horizontal and vertical flips, so those fields are
  // redundant in this case. 90* rotation can be combined with either horizontal
  // flip or vertical flip, so treat it differently
  if (sf_layer->transform == HWC_TRANSFORM_ROT_270) {
    transform = DrmHwcTransform::kRotate270;
  } else if (sf_layer->transform == HWC_TRANSFORM_ROT_180) {
    transform = DrmHwcTransform::kRotate180;
  } else {
    if (sf_layer->transform & HWC_TRANSFORM_FLIP_H)
      transform |= DrmHwcTransform::kFlipH;
    if (sf_layer->transform & HWC_TRANSFORM_FLIP_V)
      transform |= DrmHwcTransform::kFlipV;
    if (sf_layer->transform & HWC_TRANSFORM_ROT_90)
      transform |= DrmHwcTransform::kRotate90;
    if(!sf_layer->transform)
      transform |= DrmHwcTransform::kRotate0;
  }

#if RK_PRINT_LAYER_NAME
#if RK_RGA_TEST
  if((format==HAL_PIXEL_FORMAT_RGB_565) && strstr(sf_layer->LayerName,"SurfaceView"))
    transform |= DrmHwcTransform::kRotate90;

#endif
#endif

  switch (sf_layer->blending) {
    case HWC_BLENDING_NONE:
      blending = DrmHwcBlending::kNone;
      break;
    case HWC_BLENDING_PREMULT:
      blending = DrmHwcBlending::kPreMult;
      break;
    case HWC_BLENDING_COVERAGE:
      blending = DrmHwcBlending::kCoverage;
      break;
    default:
      ALOGE("Invalid blending in hwc_layer_1_t %d", sf_layer->blending);
      return -EINVAL;
  }

#if 0
  ret = buffer.ImportBuffer(sf_layer->handle, importer
#if RK_VIDEO_SKIP_LINE
  , SkipLine
#endif
  );
  if (ret)
    return ret;
#endif


#if USE_AFBC_LAYER
    // if(sf_handle)
    if ( sf_handle && bFbTarget_ )
    {
        ALOGD_IF(log_level(DBG_VERBOSE),"we got buffer handle for fb_target_layer, to get internal_format.");
#if USE_GRALLOC_4
        internal_format = gralloc4::get_internal_format(sf_handle);
#else // #if USE_GRALLOC_4

#if RK_PER_MODE
        struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)sf_handle;
        internal_format = drm_hnd->internal_format;
#else
        ret = gralloc->perform(gralloc, GRALLOC_MODULE_PERFORM_GET_INTERNAL_FORMAT,
                             sf_handle, &internal_format);
        if (ret) {
            ALOGE("Failed to get internal_format for buffer %p (%d)", sf_handle, ret);
            return ret;
        }
#endif
#endif // #if USE_GRALLOC_4

#if USE_GRALLOC_4
        if ( gralloc4::does_use_afbc_format(sf_handle) )
#else
        if(isAfbcInternalFormat(internal_format))
#endif
        {
            ALOGD_IF(log_level(DBG_VERBOSE),"to set 'is_afbc'.");
            is_afbc = true;
        }
        else
        {
            ALOGD_IF(log_level(DBG_VERBOSE),"not a afbc_buffer.");
        }
    }

    if(bFbTarget_ && !sf_handle)
    {
        ALOGD_IF(log_level(DBG_VERBOSE),"we could not got buffer handle, and current buffer is for fb_target_layer, to check AFBC in a trick way.");

        static int iFbdcSupport = -1;
        ALOGD_IF(log_level(DBG_VERBOSE),"iFbdcSupport = %d",iFbdcSupport);

        // if(iFbdcSupport == -1)
        if(iFbdcSupport <= 0)
        {
            char fbdc_value[PROPERTY_VALUE_MAX];
            int ret = property_get( PROPERTY_TYPE ".gmali.fbdc_target", fbdc_value, "0");
            ALOGV("ret = %d",ret);

            iFbdcSupport = atoi(fbdc_value);
            if(iFbdcSupport > 0 && display == 0)
            {
                ALOGD_IF(log_level(DBG_VERBOSE),"to set 'is_afbc'.");
                is_afbc = true;
            }
        }
        else if(iFbdcSupport > 0 && display == 0)
        {
            ALOGD_IF(log_level(DBG_VERBOSE),"to set 'is_afbc'.");
            is_afbc = true;
        }
    }
#endif

  return 0;
}

int DrmHwcLayer::ImportBuffer(struct hwc_context_t *ctx, hwc_layer_1_t *sf_layer, Importer *importer)
{

#if TARGET_BOARD_PLATFORM_RK3326
   /*
    * RK3326 VOP not support alpha scale, need to convert layer format
    *   DRM_FORMAT_ARGB8888 -> DRM_FORMAT_XRGB8888
    *   DRM_FORMAT_ABGR8888 -> DRM_FORMAT_XBGR8888
    */
   if(is_scale)
     importer->SetFlag(DrmGenericImporterFlag::VOP_NOT_SUPPORT_ALPHA_SCALE);
   else
     importer->SetFlag(DrmGenericImporterFlag::NO_FLAG);
#endif

   int ret = buffer.ImportBuffer(sf_layer->handle, importer
#if RK_VIDEO_SKIP_LINE
  , SkipLine
#endif
  );

  ret = handle.CopyBufferHandle(sf_layer->handle, ctx->gralloc);
  if (ret)
    return ret;

  gralloc_buffer_usage = hwc_get_handle_usage(ctx->gralloc,sf_layer->handle);

  return ret;
}

static void hwc_dump(struct hwc_composer_device_1 *dev, char *buff,
                     int buff_len) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  std::ostringstream out;

  ctx->drm.compositor()->Dump(&out);
  std::string out_str = out.str();
  strncpy(buff, out_str.c_str(),
          std::min((size_t)buff_len, out_str.length() + 1));
  buff[buff_len - 1] = '\0';
}

static bool hwc_skip_layer(const std::pair<int, int> &indices, int i) {
  return indices.first >= 0 && i >= indices.first && i <= indices.second;
}

static bool is_use_gles_comp(struct hwc_context_t *ctx, DrmConnector *connector, hwc_display_contents_1_t *display_content, int display_id)
{
    int num_layers = display_content->numHwLayers;
    hwc_drm_display_t *hd = &ctx->displays[display_id];
    DrmCrtc *crtc = NULL;
    if (!connector) {
      ALOGE("%s: Failed to get connector for display %d line=%d", __FUNCTION__,display_id, __LINE__);
    }
    else
    {
        crtc = ctx->drm.GetCrtcFromConnector(connector);
        if (connector->state() != DRM_MODE_CONNECTED || !crtc) {
          ALOGE("Failed to get crtc for display %d line=%d", display_id, __LINE__);
        }
    }

    //force go into GPU
    /*
        <=0: DISPLAY_PRIMARY & DISPLAY_EXTERNAL both go into GPU.
        =1: DISPLAY_PRIMARY go into overlay,DISPLAY_EXTERNAL go into GPU.
        =2: DISPLAY_EXTERNAL go into overlay,DISPLAY_PRIMARY go into GPU.
        others: DISPLAY_PRIMARY & DISPLAY_EXTERNAL both go into overlay.
    */
    int iMode = hwc_get_int_property( PROPERTY_TYPE ".hwc.compose_policy","0");
    if( iMode <= 0 || (iMode == 1 && display_id == 2) || (iMode == 2 && display_id == 1) )
    {
        ALOGD_IF(log_level(DBG_DEBUG), PROPERTY_TYPE ".hwc.compose_policy=%d,go to GPU GLES at line=%d", iMode, __LINE__);
        return true;
    }

    iMode = hwc_get_int_property( PROPERTY_TYPE ".hwc","1");
    if( iMode <= 0 )
    {
        ALOGD_IF(log_level(DBG_DEBUG), PROPERTY_TYPE ".hwc=%d,go to GPU GLES at line=%d", iMode, __LINE__);
        return true;
    }

#if RK_CTS_WORKROUND
    int is_auto_fill = 0;
    bool isFind = FindAppHintInFile(ctx->regFile, AUTO_FILL_PROG_NAME, IS_AUTO_FILL, &is_auto_fill, IMG_INT_TYPE);
    if(is_auto_fill)
    {
        if(!hd->bPerfMode)
        {
            ALOGD_IF(log_level(DBG_DEBUG),"enter perf mode");
            ctl_gpu_performance(1);
            ctl_cpu_performance(1, 0);
            hd->bPerfMode = true;
        }
        ALOGD_IF(log_level(DBG_DEBUG),"is auto fill program,go to GPU GLES at line=%d",  __LINE__);
        return true;
    }
    else
    {
        if(hd->bPerfMode)
        {
            ALOGD_IF(log_level(DBG_DEBUG),"exit perf mode");
            ctl_gpu_performance(0);
            ctl_cpu_performance(0, 0);
            hd->bPerfMode = false;
        }
    }
#endif

    if(num_layers == 1)
    {
        ALOGD_IF(log_level(DBG_DEBUG),"No layer,go to GPU GLES at line=%d", __LINE__);
        return true;
    }

    if(g_boot_gles_cnt < BOOT_GLES_COUNT)
    {
        ALOGD_IF(log_level(DBG_DEBUG),"g_boot_gles_cnt=%d,go to GPU GLES at line=%d", g_boot_gles_cnt, __LINE__);
        g_boot_gles_cnt++;
        return true;
    }

    if(g_bSkipExtern && (g_extern_gles_cnt < BOOT_GLES_COUNT))
    {
       ALOGD_IF(log_level(DBG_DEBUG),"g_extern_gles_cnt=%d,go to GPU GLES at line=%d", g_extern_gles_cnt, __LINE__);
       g_extern_gles_cnt++;
       return true;
    }

#if RK_INVALID_REFRESH
    if(ctx->mOneWinOpt)
    {
        ALOGD_IF(log_level(DBG_DEBUG),"Enter static screen opt,go to GPU GLES at line=%d", __LINE__);
        return true;
    }
#endif

#if RK_STEREO
    if(ctx->is_3d)
    {
        ALOGD_IF(log_level(DBG_DEBUG),"Is 3d mode,go to GPU GLES at line=%d", __LINE__);
        return true;
    }
#endif

    //If the transform nv12 layers is bigger than one,then go into GPU GLES.
    //If the transform normal layers is bigger than zero,then go into GPU GLES.
    hd->transform_nv12 = 0;
    hd->transform_normal = 0;
    int ret = 0;
    int format = 0;
#if USE_AFBC_LAYER
    uint64_t internal_format = 0;
    int iFbdcCnt = 0;
#endif
    int video_4k_cnt = 0;
    int large_UI_cnt = 0;

    for (int j = 0; j < num_layers-1; j++) {
        hwc_layer_1_t *layer = &display_content->hwLayers[j];

        if(layer->handle)
        {
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
            format = hwc_get_handle_attibute(ctx->gralloc,layer->handle,ATT_FORMAT);
#else
            format = hwc_get_handle_format(ctx->gralloc,layer->handle);
#endif
        }
        int src_l=0,src_t=0,src_r=0,src_b=0,src_w=0,src_h=0;
        int dst_l=0,dst_t=0,dst_r=0,dst_b=0,dst_w=0,dst_h=0;
        hwc_rect_t  rect_merge;
        int left_min = 0, top_min = 0, right_max = 0, bottom_max=0;
        float rga_h_scale=1.0, rga_v_scale=1.0;

        src_l = (int)layer->sourceCropf.left;
        src_t = (int)layer->sourceCropf.top;
        src_r = (int)layer->sourceCropf.right;
        src_b = (int)layer->sourceCropf.bottom;
        src_w = (int)(layer->sourceCropf.right - layer->sourceCropf.left);
        src_h = (int)(layer->sourceCropf.bottom - layer->sourceCropf.top);

        dst_w = (int)(layer->displayFrame.right - layer->displayFrame.left);
        dst_h = (int)(layer->displayFrame.bottom - layer->displayFrame.top);


        src_l = ALIGN_DOWN(src_l, 2);
        dst_l = 0;
        dst_t = 0;

        if(format == HAL_PIXEL_FORMAT_YCrCb_NV12 || format == HAL_PIXEL_FORMAT_YCrCb_NV12_10){
            if(src_w >= 3840 && src_h >= 2160 && (src_w != dst_w ||src_h != dst_h))
                video_4k_cnt++;
        }else if(src_w * src_h >= 1920 * 1080){
            large_UI_cnt++;
        }

        if(hd->isVideo && (layer->transform != 0))
        {

#if !RK_RGA_SCALE_AND_ROTATE
            if((layer->transform == HWC_TRANSFORM_ROT_90) || (layer->transform == HWC_TRANSFORM_ROT_270))
            {
                dst_r = (int)(src_b - src_t);
                dst_b = (int)(src_r - src_l);
                src_h = ALIGN_DOWN(src_h, 8);
                src_w = ALIGN_DOWN(src_w, 2);
            }
            else
            {
                dst_r = (int)(src_r - src_l);
                dst_b = (int)(src_b - src_t);
                src_w = ALIGN_DOWN(src_w, 8);
                src_h = ALIGN_DOWN(src_h, 2);
            }
            dst_w = dst_r - dst_l;
            dst_h = dst_b - dst_t;
            int dst_raw_w = dst_w;
            int dst_raw_h = dst_h;
            dst_w = ALIGN_DOWN(dst_w, 8);
            dst_h = ALIGN_DOWN(dst_h, 2);
#else
            UN_USED(dst_r);
            UN_USED(dst_b);
            rect_merge.left = layer->displayFrame.left;
            rect_merge.top = layer->displayFrame.top;
            rect_merge.right = layer->displayFrame.right;
            rect_merge.bottom = layer->displayFrame.bottom;

#if 0
            int left_min = 0, top_min = 0, right_max = 0, bottom_max=0;
            hwc_region_t * visible_region = &layer->visibleRegionScreen;
            hwc_rect_t const * visible_rects = visible_region->rects;
            if(visible_rects){
                left_min = visible_rects[0].left;
                top_min = visible_rects[0].top;
                right_max = visible_rects[0].right;
                bottom_max = visible_rects[0].bottom;

                for (int r = 0; r < (int) visible_region->numRects; r++) {
                    int r_left;
                    int r_top;
                    int r_right;
                    int r_bottom;

                    r_left = hwcMAX(layer->displayFrame.left, visible_rects[r].left);
                    left_min = hwcMIN(r_left, left_min);
                    r_top = hwcMAX(layer->displayFrame.top, visible_rects[r].top);
                    top_min = hwcMIN(r_top, top_min);
                    r_right = hwcMIN(layer->displayFrame.right, visible_rects[r].right);
                    right_max = hwcMAX(r_right, right_max);
                    r_bottom = hwcMIN(layer->displayFrame.bottom, visible_rects[r].bottom);
                    bottom_max  = hwcMAX(r_bottom, bottom_max);
                }

               if(format == HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO
                    || format == HAL_PIXEL_FORMAT_YCrCb_NV12){
                rect_merge.left = layer->displayFrame.left;
                rect_merge.top = layer->displayFrame.top;
                rect_merge.right =  layer->displayFrame.right;
                rect_merge.bottom = layer->displayFrame.bottom;
               }
               else
               {
                rect_merge.left = hwcMAX(layer->displayFrame.left, left_min);
                rect_merge.top = hwcMAX(layer->displayFrame.top, top_min);
                rect_merge.right =  hwcMIN(layer->displayFrame.right, right_max);
                rect_merge.bottom = hwcMIN(layer->displayFrame.bottom, bottom_max);
               }
            }
#endif

            src_w = ALIGN_DOWN(src_w, 2);
            src_h = ALIGN_DOWN(src_h, 2);

            dst_w = rect_merge.right - rect_merge.left;
            dst_h = rect_merge.bottom - rect_merge.top;

            dst_w = ALIGN(dst_w, 8);
            dst_h = ALIGN(dst_h, 2);
#endif

            if(src_w <= 0 || src_h <= 0)
            {
                ALOGD_IF(log_level(DBG_DEBUG),"layer src sourceCropf(%f,%f,%f,%f) is invalid,go to GPU GLES at line=%d",
                        layer->sourceCropf.left,layer->sourceCropf.top,layer->sourceCropf.right,layer->sourceCropf.bottom, __LINE__);
                return true;
            }

            if((layer->transform == HWC_TRANSFORM_ROT_90) || (layer->transform == HWC_TRANSFORM_ROT_270))
            {
                rga_h_scale = (float)dst_h / src_w;
                rga_v_scale = (float)dst_w / src_h;
            }
            else
            {
                rga_h_scale = (float)dst_w / src_w;
                rga_v_scale = (float)dst_h / src_h;
            }

#if (RGA_VER == 0 || RGA_VER == 1)
            /* Arbitrary non-integer scaling ratio, from 1/2 to 8
               RGA1:
                RK3066
                RK3188
                Beetles
                Beetlesplus
               RGA1_plus:
                Audi -> 3128
                Granite -> soifa 3gr
             */
            if(rga_h_scale < 0.5 || rga_v_scale < 0.5 ||
                rga_h_scale > 8.0 || rga_v_scale > 8.0)
            {
                ALOGD_IF(log_level(DBG_DEBUG),"rga scale(%f,%f) out of range,go to GPU GLES at line=%d",
                        rga_h_scale,rga_v_scale,__LINE__);
                return true;
            }

            if(src_w >= 1920 || src_h >= 1080)
            {
                ALOGD_IF(log_level(DBG_DEBUG),"rga1/rga1_plus take more than 20ms when roate 1080p or bigger video(%d,%d),go to GPU GLES at line=%d",
                        src_w,src_h,__LINE__);
                return true;
            }

#elif (RGA_VER == 2)
            /* Arbitrary non-integer scaling ratio, from 1/8 to 8
               RGA2-Lite:
                Maybach -> 3368
                BMW -> 3366
                Benz   -> 3228
                infiniti ->3228H
                rk3328
                rk3326
            */
            if(rga_h_scale < 0.125 || rga_v_scale < 0.125 ||
                rga_h_scale > 8.0 || rga_v_scale > 8.0)
            {
                ALOGD_IF(log_level(DBG_DEBUG),"rga scale(%f,%f) out of range,go to GPU GLES at line=%d",
                        rga_h_scale,rga_v_scale,__LINE__);
                return true;
            }
#else
            /* Arbitrary non-integer scaling ratio, from 1/16 to 16
               RGA2:
                Lincoln -> 3288/3288w
                Capricorn -> 3190
               RGA2-Enhance
                mclaren -> 3399
                mercury -> 1108
             */
            if(rga_h_scale < 0.0625 || rga_v_scale < 0.0625 ||
                rga_h_scale > 16.0 || rga_v_scale > 16.0)
            {
                ALOGD_IF(log_level(DBG_DEBUG),"rga scale(%f,%f) out of range,go to GPU GLES at line=%d",
                        rga_h_scale,rga_v_scale,__LINE__);
                return true;
            }
#endif
            if(src_w > src_h && src_h >= 2160)
            {
                ALOGD_IF(log_level(DBG_DEBUG),"RGA take more than 30ms when roate 4K or bigger video(%d,%d),go to GPU GLES at line=%d",
                        src_w,src_h,__LINE__);
                return true;
            }
        }

#if 0
        if (layer->flags & HWC_SKIP_LAYER)
        {
            ALOGD_IF(log_level(DBG_DEBUG),"layer is skipped,go to GPU GLES at line=%d", __LINE__);
            return true;
        }
#endif
        if(
#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
            !ctx->drm.isSupportRkRga() && layer->transform
#else
            layer->transform
#endif
          )
        {
            ALOGD_IF(log_level(DBG_DEBUG),"layer's transform=0x%x,go to GPU GLES at line=%d", layer->transform, __LINE__);
            return true;
        }

        if(layer->transform != HWC_TRANSFORM_ROT_270 && layer->transform & HWC_TRANSFORM_ROT_90)
        {
            if((layer->transform & HWC_TRANSFORM_FLIP_H) || (layer->transform & HWC_TRANSFORM_FLIP_V) )
            {
                ALOGD_IF(log_level(DBG_DEBUG),"layer's transform=0x%x,go to GPU GLES at line=%d", layer->transform, __LINE__);
                return true;
            }
        }
#if 0
        if( (layer->blending == HWC_BLENDING_PREMULT)&& layer->planeAlpha!=0xFF )
        {
            ALOGD_IF(log_level(DBG_DEBUG),"layer's blending planeAlpha=0x%x,go to GPU GLES at line=%d", layer->planeAlpha, __LINE__);
            return true;
        }
#endif
        if(layer->handle)
        {
            char layername[100];
#if RK_PRINT_LAYER_NAME
#ifdef USE_HWC2
            HWC_GET_HANDLE_LAYERNAME(ctx->gralloc,layer,layer->handle, layername, 100);
#else
            strcpy(layername, layer->LayerName);
#endif
#endif
            //DumpLayer(layername,layer->handle);

            if(!vop_support_format(format))
            {
                ALOGD_IF(log_level(DBG_DEBUG),"layer's format=0x%x is not support,go to GPU GLES at line=%d", format, __LINE__);
                return true;
            }
#if 1 // vendor.hwc.hdr_video_compose_by_gles property to enable/disable hdr_video_compose_by_gles
#if  (defined TARGET_BOARD_PLATFORM_RK3399) || (defined TARGET_BOARD_PLATFORM_RK3288)
            if(hd->isHdr && ctx->hdr_video_compose_by_gles)
            {
                if(connector && !connector->is_hdmi_support_hdr()
                    && crtc && !ctx->drm.is_plane_support_hdr2sdr(crtc))
                {
                    ALOGD_IF(log_level(DBG_DEBUG), "layer is hdr video,go to GPU GLES at line=%d", __LINE__);
                    return true;
                }
            }
#endif
#endif
#if 1
            if(format == HAL_PIXEL_FORMAT_YCrCb_NV12 || format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
            {
                int src_xoffset = layer->sourceCropf.left * getPixelWidthByAndroidFormat(format);
                if(!IS_ALIGN(src_xoffset,16))
                {
                    ALOGD_IF(log_level(DBG_DEBUG),"layer's x offset = %d,vop nedd address should 16 bytes alignment,go to GPU GLES at line=%d", src_xoffset,__LINE__);
                    return true;
                }
            }
#endif
#if 1
            if(!vop_support_scale(layer,hd))
            {
                ALOGD_IF(log_level(DBG_DEBUG),"layer's scale is not support,go to GPU GLES at line=%d", __LINE__);
                return true;
            }
#endif
            if(layer->transform)
            {
#ifdef TARGET_BOARD_PLATFORM_RK3288
                if(format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
                {
                    ALOGD_IF(log_level(DBG_DEBUG),"rk3288'rga cann't support nv12_10,go to GPU GLES at line=%d", __LINE__);
                    return true;
                }
#endif
                if(format == HAL_PIXEL_FORMAT_YCrCb_NV12 || format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
                    hd->transform_nv12++;
                else if(layer->compositionType != HWC_NODRAW)
                    hd->transform_normal++;
            }

#if USE_AFBC_LAYER
#if USE_GRALLOC_4
                        internal_format = gralloc4::get_internal_format(layer->handle);
#else // #if USE_GRALLOC_4

#if RK_PER_MODE
            struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)layer->handle;
            internal_format = drm_hnd->internal_format;
            UN_USED(ret);
#else
            ret = ctx->gralloc->perform(ctx->gralloc, GRALLOC_MODULE_PERFORM_GET_INTERNAL_FORMAT,
                                 layer->handle, &internal_format);
            if (ret) {
                ALOGE("Failed to get internal_format for buffer %p (%d)", layer->handle, ret);
                return false;
            }
#endif
#endif // #if USE_GRALLOC_4

#if USE_GRALLOC_4
        if ( gralloc4::does_use_afbc_format(layer->handle) )
#else
            if(isAfbcInternalFormat(internal_format))
#endif
                iFbdcCnt++;
#else
                    UN_USED(ret);
#endif
        }
    }
    if(hd->transform_nv12 > 1 || hd->transform_normal > 0)
    {
        ALOGD_IF(log_level(DBG_DEBUG), "too many rotate layers,go to GPU GLES at line=%d", __LINE__);
        return true;
    }

    if(video_4k_cnt >= 1 && large_UI_cnt >= 2)
    {
        ALOGD_IF(log_level(DBG_DEBUG), "4k video(%d) and too much large UI(%d),go to GPU GLES at line=%d",video_4k_cnt,
                 large_UI_cnt, __LINE__);
        return true;
    }

#if USE_AFBC_LAYER
    if(iFbdcCnt > 1)
    {
        ALOGD_IF(log_level(DBG_DEBUG),"iFbdcCnt=%d,go to GPU GLES line=%d",iFbdcCnt, __LINE__);
        return true;
    }
#endif

    return false;
}

static HDMI_STAT DetectStatus(const char* property)
{
    char status[PROPERTY_VALUE_MAX];

    property_get(property, status, "on");
    ALOGD_IF(log_level(DBG_VERBOSE),"get %s is %s", property, status);
    if(!strcmp(status, "off"))
        return HDMI_OFF;
    else
        return HDMI_ON;
}

/*
 * Use property on/off to enable/disable display devices
 *   sys.hdmi_status.aux -> HDMI
 *   sys.dp_status.aux   -> DP
 */
static void DetectAuxStatus(const hwc_context_t *ctx)
{
  static HDMI_STAT last_hdmi_status = HDMI_ON;
  static HDMI_STAT last_dp_status = HDMI_ON;
  char acStatus[10];
  int ret = 0;
  HDMI_STAT hdmi_status = DetectStatus( PROPERTY_TYPE ".hdmi_status.aux");
  if(ctx->hdmi_status_fd > 0 && hdmi_status != last_hdmi_status)
  {
      if(hdmi_status == HDMI_ON)
          strcpy(acStatus,"detect");
      else
          strcpy(acStatus,"off");
      ret = write(ctx->hdmi_status_fd,acStatus,strlen(acStatus)+1);
      if(ret < 0)
      {
          ALOGE("set hdmi status to %s falied, ret = %d", acStatus, ret);
      }
      last_hdmi_status = hdmi_status;
      ALOGD_IF(log_level(DBG_VERBOSE),"set hdmi status to %s",acStatus);
  }

  HDMI_STAT dp_status = DetectStatus( PROPERTY_TYPE ".dp_status.aux");
  if(ctx->dp_status_fd > 0 && dp_status != last_dp_status)
  {
      if(dp_status == HDMI_ON)
          strcpy(acStatus,"detect");
      else
          strcpy(acStatus,"off");
      ret = write(ctx->dp_status_fd,acStatus,strlen(acStatus)+1);
      if(ret < 0)
      {
          ALOGE("set dp status to %s falied, ret = %d", acStatus, ret);
      }
      ALOGD_IF(log_level(DBG_VERBOSE),"set dp status to %s",acStatus);
      last_dp_status = dp_status;
  }
  return ;
}

static bool parse_hdmi_output_format_prop(char* strprop, drm_hdmi_output_type *format, dw_hdmi_rockchip_color_depth *depth) {
    if (!strcmp(strprop, "Auto")) {
        *format = DRM_HDMI_OUTPUT_YCBCR_HQ;
        *depth = ROCKCHIP_DEPTH_DEFAULT;
        return true;
    }

    if (!strcmp(strprop, "RGB-8bit")) {
        *format = DRM_HDMI_OUTPUT_DEFAULT_RGB;
        *depth = ROCKCHIP_HDMI_DEPTH_8;
        return true;
    }

    if (!strcmp(strprop, "RGB-10bit")) {
        *format = DRM_HDMI_OUTPUT_DEFAULT_RGB;
        *depth = ROCKCHIP_HDMI_DEPTH_10;
        return true;
    }

    if (!strcmp(strprop, "YCBCR444-8bit")) {
        *format = DRM_HDMI_OUTPUT_YCBCR444;
        *depth = ROCKCHIP_HDMI_DEPTH_8;
        return true;
    }

    if (!strcmp(strprop, "YCBCR444-10bit")) {
        *format = DRM_HDMI_OUTPUT_YCBCR444;
        *depth = ROCKCHIP_HDMI_DEPTH_10;
        return true;
    }

    if (!strcmp(strprop, "YCBCR422-8bit")) {
        *format = DRM_HDMI_OUTPUT_YCBCR422;
        *depth = ROCKCHIP_HDMI_DEPTH_8;
        return true;
    }

    if (!strcmp(strprop, "YCBCR422-10bit")) {
        *format = DRM_HDMI_OUTPUT_YCBCR422;
        *depth = ROCKCHIP_HDMI_DEPTH_10;
        return true;
    }

    if (!strcmp(strprop, "YCBCR420-8bit")) {
        *format = DRM_HDMI_OUTPUT_YCBCR420;
        *depth = ROCKCHIP_HDMI_DEPTH_8;
        return true;
    }

    if (!strcmp(strprop, "YCBCR420-10bit")) {
        *format = DRM_HDMI_OUTPUT_YCBCR420;
        *depth = ROCKCHIP_HDMI_DEPTH_10;
        return true;
    }
    ALOGE("hdmi output format is invalid. [%s]", strprop);
    return false;
}

static bool update_hdmi_output_format(struct hwc_context_t *ctx, DrmConnector *connector, int display,
                         hwc_drm_display_t *hd) {

    int timeline = 0;
    drm_hdmi_output_type    color_format = DRM_HDMI_OUTPUT_DEFAULT_RGB;
    dw_hdmi_rockchip_color_depth color_depth = ROCKCHIP_HDMI_DEPTH_8;
    int ret = 0;
    int need_change_format = 0;
    int need_change_depth = 0;
    char prop_format[PROPERTY_VALUE_MAX];
    static uint32_t last_mainType,last_auxType;
    timeline = property_get_int32( PROPERTY_TYPE ".display.timeline", -1);
    drmModeAtomicReqPtr pset = NULL;
    /*
    * force update propetry when timeline is zero or not exist.
    */
    if (timeline && timeline == hd->display_timeline &&
    hd->hotplug_timeline == hd->ctx->drm.timeline())
        return 0;
    //hd->display_timeline = timeline;//let update_display_bestmode function update the value.
    //hd->hotplug_timeline = hd->ctx->drm.timeline();//let update_display_bestmode function update the value.
    memset(prop_format, 0, sizeof(prop_format));
    if (display == HWC_DISPLAY_PRIMARY){
      if(hwc_have_baseparameter()){
        if(connector->get_type() != last_mainType){
          property_set("persist." PROPERTY_TYPE ".color.main","use_baseparameter");
          ALOGD("BP:DisplayDevice change type[%d] => type[%d],to update main color",last_mainType,connector->get_type());
          last_mainType = connector->get_type();
        }
        property_get("persist." PROPERTY_TYPE ".color.main", prop_format, "use_baseparameter");
        if(!strcmp(prop_format,"use_baseparameter")){
          hwc_get_baseparameter_config(prop_format,display,BP_COLOR,connector->get_type());
          ret = sscanf(prop_format,"%d-%d",&color_format,&color_depth);
          if(ret != 2){
            ALOGE("BP: get color fail! to use default ");
            color_format = DRM_HDMI_OUTPUT_DEFAULT_RGB;
            color_depth = ROCKCHIP_DEPTH_DEFAULT;
          }
        }else{
          /* Can get the persist." PROPERTY_TYPE ".color.main, to use it. */
          ret = parse_hdmi_output_format_prop(prop_format, &color_format, &color_depth);
          if (ret == false) {
            ALOGE("Get color fail! to use default ");
            color_format = DRM_HDMI_OUTPUT_DEFAULT_RGB;
            color_depth = ROCKCHIP_DEPTH_DEFAULT;
          }
        }
      }else{
        /* if resolution is null,set to "Auto" */
        property_get("persist." PROPERTY_TYPE ".color.main", prop_format, "Auto");
        ret = parse_hdmi_output_format_prop(prop_format, &color_format, &color_depth);
        if (ret == false) {
          ALOGE("Get color fail! to use default ");
          color_format = DRM_HDMI_OUTPUT_DEFAULT_RGB;
          color_depth = ROCKCHIP_DEPTH_DEFAULT;
        }
      }
    }else if(display == HWC_DISPLAY_EXTERNAL){
      if(hwc_have_baseparameter()){
        if(connector->get_type() != last_auxType){
          property_set("persist." PROPERTY_TYPE ".color.aux","use_baseparameter");
          ALOGD("BP:DisplayDevice change type[%d] => type[%d],to update aux color",last_auxType,connector->get_type());
          last_auxType = connector->get_type();
        }
        property_get("persist." PROPERTY_TYPE ".color.aux", prop_format, "use_baseparameter");
        if(!strcmp(prop_format,"use_baseparameter")){
          hwc_get_baseparameter_config(prop_format,display,BP_COLOR,connector->get_type());
          ret = sscanf(prop_format,"%d-%d",&color_format,&color_depth);
          if(ret != 2){
            ALOGE("BP: get color fail! to use default ");
            color_format = DRM_HDMI_OUTPUT_DEFAULT_RGB;
            color_depth = ROCKCHIP_DEPTH_DEFAULT;
          }
        }else{
          /* Can get the persist." PROPERTY_TYPE ".color.aux, to use it. */
          ret = parse_hdmi_output_format_prop(prop_format, &color_format, &color_depth);
          if (ret == false) {
            ALOGE("Get color fail! to use default ");
            color_format = DRM_HDMI_OUTPUT_DEFAULT_RGB;
            color_depth = ROCKCHIP_DEPTH_DEFAULT;
          }
        }
      }else{
        /* if resolution is null,set to "Auto" */
        property_get("persist." PROPERTY_TYPE ".color.aux", prop_format, "Auto");
        ret = parse_hdmi_output_format_prop(prop_format, &color_format, &color_depth);
        if (ret == false) {
          ALOGE("Get color fail! to use default ");
          color_format = DRM_HDMI_OUTPUT_DEFAULT_RGB;
          color_depth = ROCKCHIP_DEPTH_DEFAULT;
        }
      }
    }


    if(hd->color_format != color_format) {
        need_change_format = 1;
    }

    if(hd->color_depth != color_depth) {
        need_change_depth = 1;
    }
    if(connector->hdmi_output_format_property().id() > 0 && need_change_format > 0) {

        pset = drmModeAtomicAlloc();
        if (!pset) {
            ALOGE("%s:line=%d Failed to allocate property set", __FUNCTION__, __LINE__);
            return false;
        }
        ALOGD_IF(log_level(DBG_VERBOSE),"%s: change hdmi output format: %d", __FUNCTION__, color_format);
        ret = drmModeAtomicAddProperty(pset, connector->id(), connector->hdmi_output_format_property().id(), color_format);
        if (ret < 0) {
            ALOGE("%s:line=%d Failed to add prop[%d] to [%d]", __FUNCTION__, __LINE__, connector->hdmi_output_format_property().id(), connector->id());
        }

        if (ret < 0) {
            ALOGE("%s:line=%d Failed to commit pset ret=%d\n", __FUNCTION__, __LINE__, ret);
            drmModeAtomicFree(pset);
            return false;
        }
        else
        {
            hd->color_format = color_format;
        }
    }

    if(connector->hdmi_output_depth_property().id() > 0 && need_change_depth > 0) {

        if (!pset) {
            pset = drmModeAtomicAlloc();
        }
        if (!pset) {
            ALOGE("%s:line=%d Failed to allocate property set", __FUNCTION__, __LINE__);
            return false;
        }

        ALOGD_IF(log_level(DBG_VERBOSE),"%s: change hdmi output depth: %d", __FUNCTION__, color_depth);
        ret = drmModeAtomicAddProperty(pset, connector->id(), connector->hdmi_output_depth_property().id(), color_depth);
        if (ret < 0) {
            ALOGE("%s:line=%d Failed to add prop[%d] to [%d]", __FUNCTION__, __LINE__, connector->hdmi_output_depth_property().id(), connector->id());
        }

        if (ret < 0) {
            ALOGE("%s:line=%d Failed to commit pset ret=%d\n", __FUNCTION__, __LINE__, ret);
            drmModeAtomicFree(pset);
            return false;
        }
        else
        {
            hd->color_depth = color_depth;
        }
    }
    if (pset != NULL) {
        drmModeAtomicCommit(ctx->drm.fd(), pset, DRM_MODE_ATOMIC_ALLOW_MODESET, &ctx->drm);
        drmModeAtomicFree(pset);
        pset = NULL;
    }
    return true;
}

/**
 * @brief set hdr_metadata and colorimetry.
 *
 * @param hdr_metadata  [IN] hdr metadata
 * @param android_colorspace [IN] colorspace
 * @return
 *          true: set successfully.
 *          false: set fail.
 */
static bool set_hdmi_hdr_meta(struct hwc_context_t *ctx, DrmConnector *connector,
                                hdr_metadata_s* hdr_metadata, hwc_drm_display_t *hd,
                                uint32_t android_colorspace)
{
    uint32_t blob_id = 0;
    int ret = -1;
    int colorimetry = 0;
    if(!ctx || !connector || !hdr_metadata)
    {
        ALOGE("%s:line=%d parameter is null", __FUNCTION__, __LINE__);
        return false;
    }

    if(connector->hdr_metadata_property().id())
    {
        ALOGD_IF(log_level(DBG_VERBOSE),"%s: android_colorspace = 0x%x", __FUNCTION__, android_colorspace);
        drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
        if (!pset) {
            ALOGE("%s:line=%d Failed to allocate property set", __FUNCTION__, __LINE__);
            return false;
        }
        if(!memcmp(&hd->last_hdr_metadata, hdr_metadata, sizeof(hdr_metadata_s)))
        {
            ALOGD_IF(log_level(DBG_VERBOSE),"%s: no need to update metadata", __FUNCTION__);
        }
        else
        {
            ALOGD_IF(log_level(DBG_VERBOSE),"%s: hdr_metadata eotf=0x%x, hd->last_hdr_metadata=0x%x", __FUNCTION__,
                                            HDR_METADATA_EOTF_P(hdr_metadata), HDR_METADATA_EOTF_T(hd->last_hdr_metadata));
            ctx->drm.CreatePropertyBlob(hdr_metadata, sizeof(hdr_metadata_s), &blob_id);
            ret = drmModeAtomicAddProperty(pset, connector->id(), connector->hdr_metadata_property().id(), blob_id);
            if (ret < 0) {
              ALOGE("%s:line=%d Failed to add prop[%d] to [%d]", __FUNCTION__, __LINE__, connector->hdr_metadata_property().id(), connector->id());
            }
        }

        if(connector->hdmi_output_colorimetry_property().id())
        {
            if((android_colorspace & HAL_DATASPACE_STANDARD_BT2020) == HAL_DATASPACE_STANDARD_BT2020)
            {
                colorimetry = COLOR_METRY_ITU_2020;
            }

            if(hd->colorimetry != colorimetry)
            {
                ALOGD_IF(log_level(DBG_VERBOSE),"%s: change bt2020 %d", __FUNCTION__, colorimetry);
                ret = drmModeAtomicAddProperty(pset, connector->id(), connector->hdmi_output_colorimetry_property().id(), colorimetry);
                if (ret < 0) {
                  ALOGE("%s:line=%d Failed to add prop[%d] to [%d]", __FUNCTION__, __LINE__, connector->hdmi_output_colorimetry_property().id(), connector->id());
                }
            }
        }

        drmModeAtomicCommit(ctx->drm.fd(), pset, DRM_MODE_ATOMIC_ALLOW_MODESET, &ctx->drm);
        if (ret < 0) {
            ALOGE("%s:line=%d Failed to commit pset ret=%d\n", __FUNCTION__, __LINE__, ret);
            drmModeAtomicFree(pset);
            return false;
        }
        else
        {
            memcpy(&hd->last_hdr_metadata, hdr_metadata, sizeof(hdr_metadata_s));
            hd->colorimetry = colorimetry;
        }
        if (blob_id)
            ctx->drm.DestroyPropertyBlob(blob_id);

        drmModeAtomicFree(pset);
        return true;
    }
    else
    {
        ALOGD_IF(log_level(DBG_VERBOSE),"%s: hdmi don't support hdr metadata", __FUNCTION__);
        return false;
    }
}


#if RK_RGA_PREPARE_ASYNC
static int PrepareRgaBuffer(DrmRgaBuffer &rgaBuffer, DrmHwcLayer &layer) {
    int rga_transform = 0;
    int src_l=0,src_t=0,src_w=0,src_h=0;
    int dst_l=0,dst_t=0,dst_r=0,dst_b=0;
    int ret;
    int dst_w,dst_h,dst_stride;
    rga_info_t src, dst;
    int alloc_format = 0;

    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));
    src.fd = -1;
    dst.fd = -1;

#if 0
    ret = rgaBuffer.WaitReleased(-1);
    if (ret) {
        ALOGE("Failed to wait for rga buffer release %d", ret);
        return ret;
    }
    rgaBuffer.set_release_fence_fd(-1);
#endif
    src_l = (int)layer.source_crop.left;
    src_t = (int)layer.source_crop.top;
    src_w = (int)(layer.source_crop.right - layer.source_crop.left);
    src_h = (int)(layer.source_crop.bottom - layer.source_crop.top);
    src_l = ALIGN_DOWN(src_l, 2);
    src_t = ALIGN_DOWN(src_t, 2);
    dst_l = 0;
    dst_t = 0;

#if !RK_RGA_SCALE_AND_ROTATE
    if(layer.transform & DrmHwcTransform::kRotate90 || layer.transform & DrmHwcTransform::kRotate270)
    {
        dst_r = (int)(layer.source_crop.bottom - layer.source_crop.top);
        dst_b = (int)(layer.source_crop.right - layer.source_crop.left);
        src_h = ALIGN_DOWN(src_h, 8);
        src_w = ALIGN_DOWN(src_w, 2);
    }
    else
    {
        dst_r = (int)(layer.source_crop.right - layer.source_crop.left);
        dst_b = (int)(layer.source_crop.bottom - layer.source_crop.top);
        src_w = ALIGN_DOWN(src_w, 8);
        src_h = ALIGN_DOWN(src_h, 2);
    }
    dst_w = dst_r - dst_l;
    dst_h = dst_b - dst_t;
    int dst_raw_w = dst_w;
    int dst_raw_h = dst_h;
    dst_w = ALIGN_DOWN(dst_w, 8);
    dst_h = ALIGN_DOWN(dst_h, 2);
#else
    UN_USED(dst_r);
    UN_USED(dst_b);
    src_w = ALIGN_DOWN(src_w, 2);
    src_h = ALIGN_DOWN(src_h, 2);

    dst_w = layer.rect_merge.right - layer.rect_merge.left;
    dst_h = layer.rect_merge.bottom - layer.rect_merge.top;

    dst_w = ALIGN(dst_w, 8);
    dst_h = ALIGN(dst_h, 2);
#endif

    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    //If the layer's format is NV12_10,then use RGA to switch it to NV12.
    if(layer.format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
        alloc_format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    else
        alloc_format = layer.format;

    if (!rgaBuffer.Allocate(dst_w, dst_h, alloc_format)) {
        ALOGE("Failed to allocate rga buffer with size %dx%d", dst_w, dst_h);
        return -ENOMEM;
    }

    dst_stride = rgaBuffer.buffer()->getStride();

    //DumpLayer("rga", layer.sf_handle);

    if(layer.transform & DrmHwcTransform::kRotate90) {
        rga_transform = DRM_RGA_TRANSFORM_ROT_90;
    }
    else if(layer.transform & DrmHwcTransform::kRotate270) {
        rga_transform = DRM_RGA_TRANSFORM_ROT_270;
    }
    else if(layer.transform & DrmHwcTransform::kRotate180) {
        rga_transform = DRM_RGA_TRANSFORM_ROT_180;
    }
    else if(layer.transform & DrmHwcTransform::kRotate0) {
        rga_transform = DRM_RGA_TRANSFORM_ROT_0;
    }
    else if(layer.transform & DrmHwcTransform::kFlipH) {
        rga_transform = DRM_RGA_TRANSFORM_FLIP_H;
    }
    else if(layer.transform & DrmHwcTransform::kFlipV) {
        rga_transform = DRM_RGA_TRANSFORM_FLIP_V;
    }
    else {
        ALOGE("%s: line=%d, wrong transform=0x%x", __FUNCTION__, __LINE__, layer.transform);
        ret = -1;
        return ret;
    }

    if(rga_transform != DRM_RGA_TRANSFORM_FLIP_H && layer.transform & DrmHwcTransform::kFlipH)
        rga_transform |= DRM_RGA_TRANSFORM_FLIP_H;

    if (rga_transform != DRM_RGA_TRANSFORM_FLIP_V && layer.transform & DrmHwcTransform::kFlipV)
        rga_transform |= DRM_RGA_TRANSFORM_FLIP_V;

    //rga async mode,flush in Composite thread.
    src.sync_mode = RGA_BLIT_ASYNC;
    rga_set_rect(&src.rect,
                src_l, src_t, src_w, src_h,
                layer.stride, layer.height, layer.format);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w, dst_h, dst_stride, dst_h, alloc_format);
    ALOGD_IF(log_level(DBG_DEBUG),"RK_RGA_PREPARE_ASYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD_IF(log_level(DBG_DEBUG),"RK_RGA_PREPARE_ASYNC rgaRotateScale : src hnd=%p,dst hnd=%p, format=0x%x, transform=0x%x\n",
        (void*)layer.sf_handle, (void*)(rgaBuffer.buffer()->handle), layer.format, rga_transform);

    src.hnd = layer.sf_handle;
    dst.hnd = rgaBuffer.buffer()->handle;
    src.rotation = rga_transform;
    RockchipRga& rkRga(RockchipRga::get());
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst hnd=%p",
            strerror(errno), (void*)layer.sf_handle, (void*)(rgaBuffer.buffer()->handle));
    }

    DumpLayer("rga", dst.hnd);

    //instead of the original DrmHwcLayer
    layer.is_rotate_by_rga = true;
    layer.buffer.Clear();
    layer.source_crop = DrmHwcRect<float>(dst_l,dst_t,dst_w,dst_h);
    //The dst layer's format is NV12.
    if(layer.format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
        layer.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    layer.sf_handle = rgaBuffer.buffer()->handle;

#if RK_VIDEO_SKIP_LINE
    layer.SkipLine = 0;
#endif

    layer.rga_handle = rgaBuffer.buffer()->handle;

    return ret;
}


static int ApplyPreRotate(hwc_drm_display_t *hd, DrmHwcLayer &layer) {
  int ret = 0;

  ALOGD_IF(log_level(DBG_DEBUG), "%s:rgaBuffer_index=%d", __FUNCTION__, hd->rgaBuffer_index);

  DrmRgaBuffer &rga_buffer = hd->rgaBuffers[hd->rgaBuffer_index];
  ret = PrepareRgaBuffer(rga_buffer, layer);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }
#if 0
  ret = display_comp->CreateNextTimelineFence("ApplyPreRotate");
  if (ret <= 0) {
    ALOGE("Failed to create RGA rotate release fence %d", ret);
    return ret;
  }

  rga_buffer.set_release_fence_fd(ret);
#endif
  return 0;
}

static void freeRgaBuffers(hwc_drm_display_t *hd) {
    for(int i = 0; i < MaxRgaBuffers; i++) {
        hd->rgaBuffers[i].Clear();
    }
}
#endif

static int hwc_prepare(hwc_composer_device_1_t *dev, size_t num_displays,
                       hwc_display_contents_1_t **display_contents) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  int ret = -1;

#ifdef USE_PLANE_RESERVED
  int win1_reserved = hwc_get_int_property( PROPERTY_TYPE ".hwc.win1.reserved", "0");
#endif

#ifdef USE_HWC2
  DrmConnector *extend = ctx->drm.GetConnectorFromType(HWC_DISPLAY_EXTERNAL);

    //Fake handle event if the hotplug happen earlyer than hwc thread.
    if(get_frame() == 1 && !g_hasHotplug  && extend && (extend->raw_state() == DRM_MODE_CONNECTED))
    {
      pthread_t hotplug_event;
      if (pthread_create(&hotplug_event, NULL, hotplug_event_thread, ctx))
      {
          ALOGE("Create hotplug_event thread error .");
      }
    }
#endif
    //Update LUT from baseparameter at boot time
    if(get_frame() == 1){
         hwc_SetGamma(&ctx->drm);
    }
    init_log_level();
    hwc_dump_fps();
    ALOGD_IF(log_level(DBG_VERBOSE),"----------------------------frame=%d start ----------------------------",get_frame());
    ctx->layer_contents.clear();
    ctx->layer_contents.reserve(num_displays);
    ctx->comp_plane_group.clear();

    ctx->drm.UpdateDisplayRoute();

    DetectAuxStatus(ctx);

  for (int i = 0; i < (int)num_displays; ++i) {
    bool use_framebuffer_target = false;

    if (!display_contents[i])
      continue;

    ALOGD_IF(log_level(DBG_VERBOSE), "************** display=%d **************", i);
    int num_layers = display_contents[i]->numHwLayers;
    for (int j = 0; j < num_layers; j++) {
      hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];
      dump_layer(ctx->gralloc, false, layer, j);
    }

    if(i == HWC_DISPLAY_VIRTUAL)
    {
        for (int j = 0; j < num_layers; ++j) {
            hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];
            layer->compositionType = HWC_FRAMEBUFFER;
        }
        continue;
    }

    ctx->layer_contents.emplace_back();
    DrmHwcDisplayContents &layer_content = ctx->layer_contents.back();
    ctx->comp_plane_group.emplace_back();
    DrmCompositionDisplayPlane &comp_plane = ctx->comp_plane_group.back();
    comp_plane.display = i;

    if(ctx->fb_blanked == FB_BLANK_POWERDOWN){
      ALOGD_IF(log_level(DBG_DEBUG),"%s: display=%d fb_blanked = %s,line = %d",__FUNCTION__,i,
        ctx->fb_blanked == FB_BLANK_POWERDOWN ? "POWERDOWN" : "ACTIVE",__LINE__);
      hwc_list_nodraw(display_contents[i]);
      continue;
    }

    DrmConnector *connector = ctx->drm.GetConnectorFromType(i);
    if (!connector) {
      ALOGE("%s:Failed to get connector for display %d line=%d",__FUNCTION__, i,__LINE__);
      hwc_list_nodraw(display_contents[i]);
      continue;
    }
    hwc_drm_display_t *hd = &ctx->displays[connector->display()];
    DrmCrtc *crtc = ctx->drm.GetCrtcFromConnector(connector);
    if (connector->state() != DRM_MODE_CONNECTED || !crtc) {
      ALOGE("%s: display=%d, connector[%d] is disconnect type=%s",__FUNCTION__,i,
                connector->display(),ctx->drm.connector_type_str(connector->get_type()));
      hwc_list_nodraw(display_contents[i]);
      continue;
    }

#if RK_3D_VIDEO
    hd->stereo_mode = NON_3D;
    int bk_is_3d = hd->is_3d;
    hd->is_3d = detect_3d_mode(hd, display_contents[i], i);
    if(bk_is_3d != hd->is_3d)
    {
        int timeline = 0;
        char acTimelie[10];
        timeline = property_get_int32( PROPERTY_TYPE ".display.timeline", -1);
        timeline++;
        snprintf(acTimelie,10,"%d",timeline);
        property_set( PROPERTY_TYPE ".display.timeline", acTimelie);
    }
#endif

	  update_hdmi_output_format(ctx, connector, i, hd);
    update_display_bestmode(hd, i, connector);
    DrmMode mode = connector->best_mode();
    connector->set_current_mode(mode);
    hd->rel_xres = mode.h_display();
    hd->rel_yres = mode.v_display();
    hd->v_total = mode.v_total();
    hd->w_scale = (float)mode.h_display() / hd->framebuffer_width;
    hd->h_scale = (float)mode.v_display() / hd->framebuffer_height;
    int fbSize = hd->framebuffer_width * hd->framebuffer_height;
    //get plane size for display
    std::vector<PlaneGroup *>& plane_groups = ctx->drm.GetPlaneGroups();
    hd->iPlaneSize = 0;
    hd->hasEotfPlane = false;
    hd->is_interlaced = (mode.interlaced()>0) ? true:false;
    hd->bPreferMixDown = false;
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
        iter != plane_groups.end(); ++iter)
    {
#ifdef USE_PLANE_RESERVED
        if (win1_reserved > 0 && GetCrtcSupported(*crtc, (*iter)->possible_crtcs) &&
          ((*iter)->planes.at(0)->type() == DRM_PLANE_TYPE_OVERLAY) &&
          (*iter)->planes.at(0)->get_yuv())
        {
            (*iter)->b_reserved = true;
            for(std::vector<DrmPlane*> ::const_iterator iter_plane = (*iter)->planes.begin();
              iter_plane != (*iter)->planes.end(); ++iter_plane)
            {
                (*iter_plane)->set_reserved(true);
            }
            ALOGD_IF(log_level(DBG_DEBUG),"Enable USE_PLANE_RESERVED, plane share_id = %" PRIu64 "", (*iter)->share_id);
            continue;
        }
#endif
        if(hd->is_interlaced && (*iter)->planes.size() > 2)
        {
            (*iter)->b_reserved = true;
           // continue;
        }
        else if(GetCrtcSupported(*crtc, (*iter)->possible_crtcs))
        {
            (*iter)->b_reserved = false;
            hd->iPlaneSize++;

            if(!hd->hasEotfPlane)
            {
                for(std::vector<DrmPlane*> ::const_iterator iter_plane = (*iter)->planes.begin();
                    iter_plane != (*iter)->planes.end(); ++iter_plane)
                {
                    if((*iter_plane)->get_hdr2sdr())
                    {
                        hd->hasEotfPlane = true;
                        break;
                    }
                }
            }
        }
    }

#if SKIP_BOOT
    if(g_boot_cnt < BOOT_COUNT)
    {
        hwc_list_nodraw(display_contents[i]);
        ALOGD_IF(log_level(DBG_DEBUG),"prepare skip %d",g_boot_cnt);
        return 0;
    }
#endif

    for (int j = 0; j < num_layers-1; j++) {
        hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];

        if(layer->handle)
        {
            if(layer->compositionType == HWC_NODRAW)
                layer->compositionType = HWC_FRAMEBUFFER;
        }
    }

    int format = 0;
    int usage = 0;
    bool isHdr = false;
    hd->is10bitVideo = false;
    hd->isVideo = false;
    for (int j = 0; j < num_layers-1; j++) {
        hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];

        if(layer->handle)
        {
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
            format = hwc_get_handle_attibute(ctx->gralloc,layer->handle, ATT_FORMAT);
#else
            format = hwc_get_handle_format(ctx->gralloc,layer->handle);
#endif

           if(format == HAL_PIXEL_FORMAT_YCrCb_NV12)
           {
                hd->isVideo = true;
           }

            if(format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
            {
                hd->is10bitVideo = true;
                hd->isVideo = true;
                usage = hwc_get_handle_usage(ctx->gralloc,layer->handle);
                ALOGD_IF(log_level(DBG_VERBOSE),"%s:line=%d usage = %x",__FUNCTION__,__LINE__,
                             usage);
                ALOGD_IF(log_level(DBG_VERBOSE),"%s:line=%d isSupportSt2084 = %d, isSupportHLG = %d",__FUNCTION__,__LINE__,
                             connector->isSupportSt2084(),connector->isSupportHLG() );
                if((usage & 0x0F000000) == HDR_ST2084_USAGE || (usage & 0x0F000000) == HDR_HLG_USAGE)
                {
                    isHdr = true;
                    //vop limit: hdr video must in the bottom.
                    if(j != 0)
                    {
                        ALOGD_IF(log_level(DBG_DEBUG),"hdr video must in the bottom of layer list,go to GPU GLES at line=%d", __LINE__);
                        use_framebuffer_target = true;
                    }
                    if(hd->isHdr != isHdr && connector->is_hdmi_support_hdr())
                    {
                       ALOGD_IF(log_level(DBG_VERBOSE),"%s:line=%d isSupportSt2084 = %d, isSupportHLG = %d",__FUNCTION__,__LINE__,
                             connector->isSupportSt2084(),connector->isSupportHLG() );
                        uint32_t android_colorspace = hwc_get_layer_colorspace(layer);
                        hdr_metadata_s hdr_metadata;
                        memset(&hdr_metadata, 0, sizeof(hdr_metadata_s));
                        if((android_colorspace & HAL_DATASPACE_TRANSFER_MASK) == HAL_DATASPACE_TRANSFER_ST2084
                            && connector->isSupportSt2084())
                        {
                            ALOGD_IF(log_level(DBG_VERBOSE),"%s:line=%d has st2084",__FUNCTION__,__LINE__);
                            HDR_METADATA_EOTF_T(hdr_metadata) = SMPTE_ST2084;
                        }
                        else if((android_colorspace & HAL_DATASPACE_TRANSFER_MASK) == HAL_DATASPACE_TRANSFER_HLG
                            && connector->isSupportHLG())
                        {
                            ALOGD_IF(log_level(DBG_VERBOSE),"%s:line=%d has HLG",__FUNCTION__,__LINE__);
                            HDR_METADATA_EOTF_T(hdr_metadata) = HLG;
                        }
                        else
                        {
                            //ALOGE("Unknow etof %d",eotf);
                            HDR_METADATA_EOTF_T(hdr_metadata) = TRADITIONAL_GAMMA_SDR;
                        }

                        set_hdmi_hdr_meta(ctx, connector, &hdr_metadata, hd, android_colorspace);
                    }
                    break;
                }
            }
        }
    }

    bool force_not_invalid_refresh = false;
    for (int j = 0; j < num_layers-1; j++) {
        hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];

        if(layer->handle)
        {
#if RK_DRM_GRALLOC
            format = hwc_get_handle_attibute(ctx->gralloc,layer->handle, ATT_FORMAT);
#else
            format = hwc_get_handle_format(ctx->gralloc,layer->handle);
#endif

#if RK_PRINT_LAYER_NAME
            char layername[100];
#ifdef USE_HWC2
            HWC_GET_HANDLE_LAYERNAME(ctx->gralloc,layer,layer->handle, layername, 100);
#else
            strcpy(layername, layer->LayerName);
#endif
#endif
            int src_l,src_t,src_w,src_h;

            src_l = (int)layer->sourceCropf.left;
            src_t = (int)layer->sourceCropf.top;
            src_w = (int)(layer->sourceCropf.right - layer->sourceCropf.left);
            src_h = (int)(layer->sourceCropf.bottom - layer->sourceCropf.top);
            if(!force_not_invalid_refresh && src_w > src_h && src_w >= 3840
              && format != HAL_PIXEL_FORMAT_YCrCb_NV12 && format != HAL_PIXEL_FORMAT_YCrCb_NV12_10)
            {
                force_not_invalid_refresh = true;
            }

            /*
             *  VOP can't display layer size < 16 pixel , so set layer HWC_NODRAW in 1080P
             */
            if( hd->rel_xres * hd->rel_yres > 2073600 && (src_w * src_h < 16 ))
            {
               layer->compositionType = HWC_NODRAW;
               //layer->flags |= HWC_SKIP_LAYER;
               ALOGD_IF(log_level(DBG_DEBUG),
                       "%s:line=%d layer size[%d,%d] too small ,set HWC_NODRAW",
                       __FUNCTION__,__LINE__,src_w,src_h);
            }
            /*
             *  VOP can't scale layer width or height < 4 pixel , so set layer HWC_SKIP_LAYER to
             *  compose by GPU.
             */
            if( src_w < 4 || src_h < 4 )
            {
               layer->compositionType = HWC_FRAMEBUFFER;
               layer->flags |= HWC_SKIP_LAYER;
               ALOGD_IF(log_level(DBG_DEBUG),
                       "%s:line=%d layer size[%d,%d] too small ,set HWC_SKIP_LAYER",
                       __FUNCTION__,__LINE__,src_w,src_h);
            }


#if RK_PRINT_LAYER_NAME
            if(strstr(layername,"drawpath"))
            {
              hd->bPreferMixDown = true;
              ALOGD_IF(log_level(DBG_DEBUG),"%s:line=%d in drawpath mode prefer use mix down policy",
                     __FUNCTION__,__LINE__);
            }
#endif
         }
    }

#if RK_INVALID_REFRESH
    if(ctx->mOneWinOpt && force_not_invalid_refresh && hd->rel_xres >= 3840 && hd->rel_xres != hd->framebuffer_width)
    {
       ALOGD_IF(log_level(DBG_DEBUG),"disable static timer");
       ctx->mOneWinOpt = false;
    }
    if(hd->isHdr){
           ALOGD_IF(log_level(DBG_DEBUG),"HDR video mode,disable static timer");
           ctx->mOneWinOpt = false;
    }
#endif

    //Switch hdr mode
    if(hd->isHdr != isHdr)
    {
        hd->isHdr = isHdr;
#if RK_HDR_PERF_MODE
        if(hd->isHdr)
        {
            ALOGD_IF(log_level(DBG_DEBUG),"Enter hdr performance mode");
            ctl_little_cpu(0);
            ctl_cpu_performance(1, 1);
        }
        else
        {
            ALOGD_IF(log_level(DBG_DEBUG),"Exit hdr performance mode");
            ctl_cpu_performance(0, 1);
            ctl_little_cpu(1);
        }
#endif

        if(!hd->isHdr && connector->is_hdmi_support_hdr())
        {
            uint32_t android_colorspace = 0;
            hdr_metadata_s hdr_metadata;

            ALOGD_IF(log_level(DBG_VERBOSE),"disable hdmi hdr meta");
            memset(&hdr_metadata, 0, sizeof(hdr_metadata_s));
            set_hdmi_hdr_meta(ctx, connector, &hdr_metadata, hd, android_colorspace);
        }
    }

#if RK_3D_VIDEO
    int iLastFps = num_layers-1;
    if(hd->stereo_mode == FPS_3D)
    {
        for(int j=num_layers-1; j>=0; j--) {
            hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];
            int32_t alreadyStereo = 0;
#ifdef USE_HWC2
            if(layer->handle)
            {
                alreadyStereo = hwc_get_handle_alreadyStereo(ctx->gralloc, layer->handle);
                if(alreadyStereo < 0)
                {
                    ALOGE("hwc_get_handle_alreadyStereo fail");
                    alreadyStereo = 0;
                }
            }
#else
            alreadyStereo = layer->alreadyStereo;
#endif
            if(alreadyStereo == FPS_3D) {
                iLastFps = j;
                break;
            }
        }

        for (int j = 0; j < iLastFps; j++)
        {
            display_contents[i]->hwLayers[j].compositionType = HWC_NODRAW;
        }
    }
#endif

#if RK_VIDEO_UI_OPT
    video_ui_optimize(ctx->gralloc, display_contents[i], &ctx->displays[connector->display()]);
#endif

    if(!use_framebuffer_target)
        use_framebuffer_target = is_use_gles_comp(ctx, connector, display_contents[i], connector->display());

    bool bHasFPS_3D_UI = false;
    int index = 0;
    for (int j = 0; j < num_layers; j++) {
      hwc_layer_1_t *sf_layer = &display_contents[i]->hwLayers[j];
      if(!(sf_layer->flags & HWC_SKIP_LAYER) && sf_layer->compositionType != HWC_FRAMEBUFFER_TARGET && sf_layer->handle == NULL)
        continue;
      if(sf_layer->compositionType == HWC_NODRAW)
        continue;

#if RK_3D_VIDEO
        if(hd->stereo_mode == FPS_3D && iLastFps < num_layers-1)
        {
            int32_t alreadyStereo = 0, displayStereo = 0;
#ifdef USE_HWC2
            alreadyStereo = hwc_get_handle_alreadyStereo(ctx->gralloc, sf_layer->handle);
            if(alreadyStereo < 0)
            {
                ALOGE("hwc_get_handle_alreadyStereo fail");
                alreadyStereo = 0;
            }

            displayStereo = hwc_get_handle_displayStereo(ctx->gralloc, sf_layer->handle);
            if(displayStereo < 0)
            {
                ALOGE("hwc_get_handle_alreadyStereo fail");
                displayStereo = 0;
            }
#else
            alreadyStereo = sf_layer->alreadyStereo;
            displayStereo = sf_layer->displayStereo;
#endif
            if(j>iLastFps  && alreadyStereo!= FPS_3D && displayStereo)
            {
                bHasFPS_3D_UI = true;
            }
        }
#endif
      layer_content.layers.emplace_back();
      DrmHwcLayer &layer = layer_content.layers.back();
      ret = layer.InitFromHwcLayer(ctx, i, sf_layer, ctx->importer.get(), ctx->gralloc, false);
      if (ret) {
        ALOGE("Failed to init composition from layer %d", ret);
        return ret;
      }
      layer.index = j;
      index = j;

      std::ostringstream out;
      layer.dump_drm_layer(j,&out);
      ALOGD_IF(log_level(DBG_DEBUG),"%s",out.str().c_str());
    }

#if RK_3D_VIDEO
    if(bHasFPS_3D_UI)
    {
      hwc_layer_1_t *sf_layer = &display_contents[i]->hwLayers[num_layers-1];
      if(sf_layer->handle == NULL)
        continue;

      layer_content.layers.emplace_back();
      DrmHwcLayer &layer = layer_content.layers.back();
      ret = layer.InitFromHwcLayer(ctx, i, sf_layer, ctx->importer.get(), ctx->gralloc, true);
      if (ret) {
        ALOGE("Failed to init composition from layer %d", ret);
        return ret;
      }
      index++;
      layer.index = index;

      std::ostringstream out;
      layer.dump_drm_layer(index,&out);
      ALOGD_IF(log_level(DBG_DEBUG),"clone layer: %s",out.str().c_str());
    }
#endif

    //vop limit: If vop cann't support alpha scale,it should go into gles.
    if(!crtc->get_alpha_scale())
    {
        for (size_t j = 0; j < layer_content.layers.size(); j++) {
            DrmHwcLayer& layer = layer_content.layers[j];
            if(layer.format == HAL_PIXEL_FORMAT_RGBA_8888 || layer.format == HAL_PIXEL_FORMAT_BGRA_8888)
            {
                if(layer.h_scale_mul != 1.0 || layer.v_scale_mul != 1.0)
                {
                    use_framebuffer_target = true;
                    ALOGD_IF(log_level(DBG_DEBUG),"alpha scale is not support,format=0x%x,h_scale=%f,v_scale=%f,go to GPU GLES at line=%d",
                            layer.format, layer.h_scale_mul, layer.v_scale_mul, __LINE__);
                    break;
                }

                if(layer.alpha != 0xff)
                {
                    use_framebuffer_target = true;
                    ALOGD_IF(log_level(DBG_DEBUG),"per-pixel alpha with global alpha is not support,global alpha=0x%x,go to GPU GLES at line=%d",
                            layer.alpha, __LINE__);
                    break;
                }
            }
        }
    }

    if(!use_framebuffer_target)
    {
        int iRgaCnt = 0;
        for (size_t j = 0; j < layer_content.layers.size(); j++) {
            DrmHwcLayer& layer = layer_content.layers[j];

            if(layer.mlayer->compositionType == HWC_FRAMEBUFFER_TARGET)
                continue;

#if !RK_RGA_SCALE_AND_ROTATE
            if(layer.h_scale_mul > 1.0 &&  (int)(layer.display_frame.right - layer.display_frame.left) > 2560)
            {
                ALOGD_IF(log_level(DBG_DEBUG),"On rk3368 don't use rga for scale, go to GPU GLES at line=%d", __LINE__);
                use_framebuffer_target = true;
                break;
            }
#endif
            if(layer.transform!=DrmHwcTransform::kRotate0
#if  RK_RGA_SCALE_AND_ROTATE
                || (layer.h_scale_mul > 1.0 &&  (int)(layer.display_frame.right - layer.display_frame.left) > 2560)
#endif
                )
            {
                iRgaCnt++;
            }
        }
        if(iRgaCnt > 1)
        {
            ALOGD_IF(log_level(DBG_DEBUG),"rga cnt = %d, go to GPU GLES at line=%d", iRgaCnt, __LINE__);
            use_framebuffer_target = true;
        }
    }

    if(!use_framebuffer_target)
    {
        bool bAllMatch = false;

        hd->mixMode = HWC_DEFAULT;
        if(crtc && layer_content.layers.size()>0)
        {
            bAllMatch = mix_policy(&ctx->drm, crtc, &ctx->displays[connector->display()],layer_content.layers,
                                    hd->iPlaneSize, fbSize, comp_plane.composition_planes);
        }
        if(!bAllMatch)
        {
            ALOGD_IF(log_level(DBG_DEBUG),"mix_policy failed,go to GPU GLES at line=%d", __LINE__);
            use_framebuffer_target = true;
        }
    }

    for (int j = 0; j < num_layers; ++j) {
      hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];


      if (!use_framebuffer_target && layer->compositionType != HWC_MIX) {
        // If the layer is off the screen, don't earmark it for an overlay.
        // We'll leave it as-is, which effectively just drops it from the frame
        const hwc_rect_t *frame = &layer->displayFrame;
        if ((frame->right - frame->left) <= 0 ||
            (frame->bottom - frame->top) <= 0 ||
            frame->right <= 0 || frame->bottom <= 0 ||
            frame->left >= (int)hd->framebuffer_width ||
            frame->top >= (int)hd->framebuffer_height)
         {
            continue;
         }

        if (layer->compositionType == HWC_FRAMEBUFFER)
          layer->compositionType = HWC_OVERLAY;
      } else {
        switch (layer->compositionType) {
          case HWC_MIX:
          case HWC_OVERLAY:
          case HWC_BACKGROUND:
          case HWC_SIDEBAND:
          case HWC_CURSOR_OVERLAY:
            layer->compositionType = HWC_FRAMEBUFFER;
            break;
        }
      }
#if DUAL_VIEW_MODE
      if(hd->bDualViewMode && i == HWC_DISPLAY_EXTERNAL && layer->compositionType != HWC_FRAMEBUFFER_TARGET)
      {
        layer->compositionType = HWC_NODRAW;
      }
#endif
    }
#if RK_RGA_PREPARE_ASYNC
    if(!use_framebuffer_target && ctx->drm.isSupportRkRga())
    {
        bool bUseRga = false;

        for (size_t j = 0; j < layer_content.layers.size(); j++) {
            DrmHwcLayer& layer = layer_content.layers[j];

            if((layer.is_yuv && layer.transform!=DrmHwcTransform::kRotate0))
            {
                ret = ApplyPreRotate(hd,layer);
                if (ret)
                {
                    freeRgaBuffers(hd);
                    hd->mUseRga = hd->mUseRga ? false : hd->mUseRga;
                    return ret;
                }

                hd->rgaBuffer_index = (hd->rgaBuffer_index + 1) % MaxRgaBuffers;
                bUseRga = true;
                hd->mUseRga = hd->mUseRga ? hd->mUseRga : true;
            }
        }

        if(hd->mUseRga && !bUseRga)
        {
            freeRgaBuffers(hd);
            hd->mUseRga = false;
        }
    }
#endif

    if(use_framebuffer_target)
        ctx->isGLESComp = true;
    else
        ctx->isGLESComp = false;

    if(ctx->isGLESComp)
    {
#if RK_ROTATE_VIDEO_MODE
        if(hd->bRotateVideoMode)
        {
            ALOGD_IF(log_level(DBG_DEBUG), "Exit Rotate video Mode mode");
            set_cpu_min_freq(hd->original_min_freq);
            hd->bRotateVideoMode = false;
        }
#endif
        //remove all layers except fb layer
        for (auto k = layer_content.layers.begin(); k != layer_content.layers.end();)
        {
            //remove gles layers
            if((*k).mlayer->compositionType != HWC_FRAMEBUFFER_TARGET)
                k = layer_content.layers.erase(k);
            else
                k++;
        }

        //match plane for gles composer.
        bool bAllMatch = match_process(&ctx->drm, crtc, hd->is_interlaced ,layer_content.layers,
                                        hd->iPlaneSize, fbSize, comp_plane.composition_planes);
        if(!bAllMatch)
            ALOGE("Fetal error when match plane for fb layer");
    }
    else
    {
#if RK_ROTATE_VIDEO_MODE
        if(hd->transform_nv12==1 && !hd->bRotateVideoMode)
        {
            ALOGD_IF(log_level(DBG_DEBUG), "Enter Rotate video Mode mode");
            hd->original_min_freq = set_cpu_min_freq(408);
            hd->bRotateVideoMode = true;
        }
        else if(hd->transform_nv12!=1 && hd->bRotateVideoMode)
        {
            ALOGD_IF(log_level(DBG_DEBUG), "Exit Rotate video Mode mode");
            set_cpu_min_freq(hd->original_min_freq);
            hd->bRotateVideoMode = false;
        }
#endif
    }

    for (int j = 0; j < num_layers; ++j) {
        hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];
        char layername[100];

#if RK_PRINT_LAYER_NAME
#ifdef USE_HWC2
        if(layer->handle == NULL)
        {
            strcpy(layername,"");
        }
        else
        {
            HWC_GET_HANDLE_LAYERNAME(ctx->gralloc,layer,layer->handle, layername, 100);
        }
#else
        strcpy(layername, layer->LayerName);
#endif
#endif

        if(layer->compositionType==HWC_FRAMEBUFFER)
            ALOGD_IF(log_level(DBG_DEBUG),"%s: HWC_FRAMEBUFFER",layername);
        else if(layer->compositionType==HWC_OVERLAY)
            ALOGD_IF(log_level(DBG_DEBUG),"%s: HWC_OVERLAY",layername);
        else
            ALOGD_IF(log_level(DBG_DEBUG),"%s: HWC_OTHER",layername);
    }
  }

#if RK_INVALID_REFRESH
  if(ctx->mOneWinOpt)
    ctx->mOneWinOpt = false;
#endif

  return 0;
}

static void hwc_add_layer_to_retire_fence(
    hwc_layer_1_t *layer, hwc_display_contents_1_t *display_contents) {
  if (layer->releaseFenceFd < 0)
    return;

  if (display_contents->retireFenceFd >= 0) {
    int old_retire_fence = display_contents->retireFenceFd;
    display_contents->retireFenceFd =
        sync_merge("dc_retire", old_retire_fence, layer->releaseFenceFd);
    close(old_retire_fence);
  } else {
    display_contents->retireFenceFd = dup(layer->releaseFenceFd);
  }
}

/* rk:
 * acquireFenceFd may transfer from  hwc_layer_1_t to DrmHwcLayer.
 * So we signal acquire_fence of DrmHwcLayer at first.
 * Then we try to signal acquireFenceFd of hwc_layer_1_t.
 */
void signal_all_fence(DrmHwcDisplayContents &display_contents,hwc_display_contents_1_t  *dc)
{
  for (size_t j=0; j< display_contents.layers.size(); j++) {
      DrmHwcLayer &layer = display_contents.layers[j];
      int acquire_fence = layer.acquire_fence.get();

      if(acquire_fence >= 0)
      {
          int ret = sync_wait(acquire_fence, 1000);
          if (ret) {
            ALOGE("signal_all_fence Failed to wait for acquire %d/%d 1000ms", acquire_fence, ret);
            continue;
          }
          layer.acquire_fence.Close();
      }
  }
  hwc_sync_release(dc);
}

static int hwc_set(hwc_composer_device_1_t *dev, size_t num_displays,
                   hwc_display_contents_1_t **sf_display_contents) {
  ATRACE_CALL();
#ifdef USE_HWC2
  g_waitHwcSetHotplug = true;
#endif
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  int ret = 0;

 inc_frame();

  std::vector<CheckedOutputFd> checked_output_fences;
  std::vector<DrmHwcDisplayContents> displays_contents;
  std::vector<DrmCompositionDisplayLayersMap> layers_map;
  std::vector<std::vector<size_t>> layers_indices;
  std::vector<uint32_t> fail_displays;
  DrmComposition* composition = NULL;

  // layers_map.reserve(num_displays);
  layers_indices.reserve(num_displays);
  // Phase one does nothing that would cause errors. Only take ownership of FDs.
  for (size_t i = 0; i < num_displays; ++i) {
    hwc_display_contents_1_t *dc = sf_display_contents[i];
    DrmHwcDisplayContents &display_contents = ctx->layer_contents[i];
    displays_contents.emplace_back();
    DrmHwcDisplayContents &display_contents_tmp = displays_contents.back();
    layers_indices.emplace_back();

    if (!sf_display_contents[i])
      continue;
#if SKIP_BOOT
    if(g_boot_cnt < BOOT_COUNT) {
        hwc_sync_release(sf_display_contents[i]);
        if(0 == i)
            g_boot_cnt++;
        ALOGD_IF(log_level(DBG_DEBUG),"set skip %d",g_boot_cnt);
        return 0;
    }
#endif

    if (i == HWC_DISPLAY_VIRTUAL) {
      ctx->virtual_compositor_worker.QueueComposite(dc);
      continue;
    }

    if(ctx->fb_blanked == FB_BLANK_POWERDOWN){
      ALOGD_IF(log_level(DBG_DEBUG),"%s: display=%zu fb_blanked = %s,line = %d",__FUNCTION__,i,
        ctx->fb_blanked == FB_BLANK_POWERDOWN ? "POWERDOWN" : "ACTIVE",__LINE__);
      hwc_sync_release(sf_display_contents[i]);
      ctx->drm.ClearDisplay(i);
      continue;
    }

    DrmConnector *c = ctx->drm.GetConnectorFromType(i);
    size_t num_dc_layers = dc->numHwLayers;
    if (!c || c->state() != DRM_MODE_CONNECTED || num_dc_layers==1) {

      if(c)
        ALOGD_IF(log_level(DBG_DEBUG),"hwc_set connector is disconnect,type=%s",ctx->drm.connector_type_str(c->get_type()));
      if(num_dc_layers==1)
        ALOGD_IF(log_level(DBG_DEBUG), "%s display=%zu layer is null", __FUNCTION__, i);
      hwc_sync_release(sf_display_contents[i]);
      ctx->drm.ClearDisplay(i);
      continue;
    }

    DumpLayerList(dc,ctx->gralloc);

    std::ostringstream display_index_formatter;
    display_index_formatter << "retire fence for display " << i;
    std::string display_fence_description(display_index_formatter.str());
    checked_output_fences.emplace_back(&dc->retireFenceFd,
                                       display_fence_description.c_str(),
                                       ctx->dummy_timeline);
    display_contents.retire_fence = OutputFd(&dc->retireFenceFd);

    int framebuffer_target_index = -1;
    for (size_t j = 0; j < num_dc_layers; ++j) {
      hwc_layer_1_t *sf_layer = &dc->hwLayers[j];
      if (sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET) {
        framebuffer_target_index = j;
        break;
      }
    }

    for (size_t j = 0; j < num_dc_layers; ++j) {
      size_t k = 0;
      hwc_layer_1_t *sf_layer = &dc->hwLayers[j];

      // In prepare() we marked all layers FRAMEBUFFER between SKIP_LAYER's.
      // This means we should insert the FB_TARGET layer in the composition
      // stack at the location of the first skip layer, and ignore the rest.

    if (sf_layer->flags & HWC_SKIP_LAYER) {
        //rk: SurfaceFlinger will create acquireFenceFd for nodraw skip layer.
        //    Close it here to avoid anon_inode:sync_fence fd leak.
        if(sf_layer->compositionType == HWC_NODRAW)
        {
            if(sf_layer->acquireFenceFd >= 0)
            {
                close(sf_layer->acquireFenceFd);
                sf_layer->acquireFenceFd = -1;
            }
        }

        if (framebuffer_target_index < 0)
          continue;
        int idx = framebuffer_target_index;
        framebuffer_target_index = -1;
        hwc_layer_1_t *fbt_layer = &dc->hwLayers[idx];
        if (!fbt_layer->handle || (fbt_layer->flags & HWC_SKIP_LAYER)) {
          ALOGE("Invalid HWC_FRAMEBUFFER_TARGET with HWC_SKIP_LAYER present");
          continue;
        }
        continue;
      }

      char value[PROPERTY_VALUE_MAX];
      property_get( PROPERTY_TYPE ".hwc.force_wait_acquireFence", value, "0");
      if(atoi(value) != 0){
          // rk: wait acquireFenceFd at hwc_set.
          if(sf_layer->acquireFenceFd > 0)
          {
              sync_wait(sf_layer->acquireFenceFd, -1);
              close(sf_layer->acquireFenceFd);
              sf_layer->acquireFenceFd = -1;
          }
      }
      for (k = 0; k < display_contents.layers.size(); ++k)
      {
         DrmHwcLayer &layer = display_contents.layers[k];
         if(j == layer.index)
         {
            //  sf_layer = layer.raw_sf_layer;
              layer.acquire_fence.Set(sf_layer->acquireFenceFd);
              sf_layer->acquireFenceFd = -1;

              std::ostringstream layer_fence_formatter;
              layer_fence_formatter << "release fence for layer " << j << " of display "
                                    << i;
              std::string layer_fence_description(layer_fence_formatter.str());
              checked_output_fences.emplace_back(&sf_layer->releaseFenceFd,
                                                 layer_fence_description.c_str(),
                                                 ctx->dummy_timeline);
              layer.release_fence = OutputFd(&sf_layer->releaseFenceFd);
            break;
         }
      }

      if(k == display_contents.layers.size())
      {
          display_contents_tmp.layers.emplace_back();
          DrmHwcLayer &layer = display_contents_tmp.layers.back();

          layer.acquire_fence.Set(sf_layer->acquireFenceFd);
          sf_layer->acquireFenceFd = -1;

          std::ostringstream layer_fence_formatter;
          layer_fence_formatter << "release fence for layer " << j << " of display "
                                << i;
          std::string layer_fence_description(layer_fence_formatter.str());
          checked_output_fences.emplace_back(&sf_layer->releaseFenceFd,
                                             layer_fence_description.c_str(),
                                             ctx->dummy_timeline);
          layer.release_fence = OutputFd(&sf_layer->releaseFenceFd);
      }
    }

    if(display_contents.layers.size() == 0 && framebuffer_target_index >= 0)
    {
      hwc_layer_1_t *sf_layer = &dc->hwLayers[framebuffer_target_index];
      if (!sf_layer->handle || (sf_layer->flags & HWC_SKIP_LAYER)) {
        ALOGE(
            "Expected valid layer with HWC_FRAMEBUFFER_TARGET when all "
            "HWC_OVERLAY layers are skipped.");
        fail_displays.emplace_back(i);
        ret = -EINVAL;
      }
    }
  }

#if 0
  if (ret)
    return ret;
#endif
  int fail_displays_count = 0;
  for (size_t i = 0; i < num_displays; ++i) {
    hwc_display_contents_1_t *dc = sf_display_contents[i];
    DrmHwcDisplayContents &display_contents = ctx->layer_contents[i];
    bool bFindDisplay = false;
    if (!sf_display_contents[i] || i == HWC_DISPLAY_VIRTUAL)
      continue;

    for (auto &fail_display : fail_displays) {
        if( i == fail_display )
        {
            bFindDisplay = true;
	    fail_displays_count ++;
            ALOGD_IF(log_level(DBG_VERBOSE),"%s:line=%d,Find fail display %zu",__FUNCTION__,__LINE__,i);
            break;
        }
    }
    if(bFindDisplay)
        continue;

    size_t num_dc_layers = dc->numHwLayers;
    DrmConnector *c = ctx->drm.GetConnectorFromType(i);
    if (!c || c->state() != DRM_MODE_CONNECTED || num_dc_layers==1) {
        ALOGD_IF(log_level(DBG_VERBOSE),
        "%s,display %zu Connector is NULL or disconnect ,layer_list is NULL",__FUNCTION__,i);
        continue;
    }

    /*
     * DUAL_VIEW_MODE
     * Primary layers   ->    Primary Device
     *                  |
     *                  ->    Extend Device
     */
#if DUAL_VIEW_MODE
    static int primaryAcquirFenceDup = -1;
    hwc_drm_display_t *hd = &ctx->displays[c->display()];
    if(hd->bDualViewMode){
      DrmHwcDisplayContents &display_contents_pri = ctx->layer_contents[0];

      layers_map.emplace_back();
      DrmCompositionDisplayLayersMap &map = layers_map.back();
      map.display = i;
      map.geometry_changed =
          (dc->flags & HWC_GEOMETRY_CHANGED) == HWC_GEOMETRY_CHANGED;
      for (size_t j=0; j< display_contents.layers.size(); j++) {
        DrmHwcLayer &layer = display_contents.layers[j];
        DrmHwcLayer &layer_pri = display_contents_pri.layers[j];
        //Dup primary acquireFence to extend, should wait acquireFence before commmit.
        if( i == HWC_DISPLAY_PRIMARY ){
            if( primaryAcquirFenceDup > 0){
                close(primaryAcquirFenceDup);
                primaryAcquirFenceDup = -1;
            }
            if(layer_pri.acquire_fence.get() > 0){
                primaryAcquirFenceDup = dup(layer_pri.acquire_fence.get());
            }
        }else if( i == HWC_DISPLAY_EXTERNAL ){
            if(primaryAcquirFenceDup > 0){
                layer.acquire_fence.Set(primaryAcquirFenceDup);
                primaryAcquirFenceDup = -1;
            }
        }
        if(!layer_pri.sf_handle && layer_pri.raw_sf_layer->handle)
        {
          layer_pri.sf_handle = layer_pri.raw_sf_layer->handle;
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
          layer.width = hwc_get_handle_attibute(ctx->gralloc,layer_pri.sf_handle,ATT_WIDTH);
          layer.height = hwc_get_handle_attibute(ctx->gralloc,layer_pri.sf_handle,ATT_HEIGHT);
          layer.stride = hwc_get_handle_attibute(ctx->gralloc,layer_pri.sf_handle,ATT_STRIDE);
          layer.format = hwc_get_handle_attibute(ctx->gralloc,layer_pri.sf_handle,ATT_FORMAT);
#else
          layer.width = hwc_get_handle_width(ctx->gralloc,layer_pri.sf_handle);
          layer.height = hwc_get_handle_height(ctx->gralloc,layer_pri.sf_handle);
          layer.stride = hwc_get_handle_stride(ctx->gralloc,layer_pri.sf_handle);
          layer.format = hwc_get_handle_format(ctx->gralloc,layer_pri.sf_handle);
#endif
        }
        if(!layer_pri.sf_handle)
        {
          ALOGE("%s: disply=%zu sf_handle is null,maybe fb target is null",__FUNCTION__,i);
          signal_all_fence(display_contents, dc);
          ctx->drm.ClearDisplay(i);
          std::vector<DrmCompositionDisplayLayersMap>::iterator iter = layers_map.begin()+i - fail_displays_count;
          layers_map.erase(iter);
          break;
        }
        if(!layer_pri.bClone_)
        {
#if RK_RGA_PREPARE_ASYNC
          if(!layer_pri.is_rotate_by_rga)
#endif
              layer.ImportBuffer(ctx, layer_pri.raw_sf_layer, ctx->importer.get());
#if RK_RGA_PREPARE_ASYNC
          else
          {
              ret = layer.buffer.ImportBuffer(layer_pri.rga_handle,
                                                     ctx->importer.get()
#if RK_VIDEO_SKIP_LINE
                                                     , layer_pri.SkipLine
#endif
                                                     );
              if (ret) {
                  ALOGE("Failed to import rga buffer ret=%d", ret);
                  goto err;
              }

              ret = layer_pri.handle.CopyBufferHandle(layer_pri.rga_handle, ctx->gralloc);
              if (ret) {
                  ALOGE("Failed to copy rga handle ret=%d", ret);
                  goto err;
              }
          }
#endif
        }
        map.layers.emplace_back(std::move(layer));
      }
    }else
#endif //DUAL_VIEW_MODE
    {
      layers_map.emplace_back();
      DrmCompositionDisplayLayersMap &map = layers_map.back();
      map.display = i;
      map.geometry_changed =
          (dc->flags & HWC_GEOMETRY_CHANGED) == HWC_GEOMETRY_CHANGED;
      for (size_t j=0; j< display_contents.layers.size(); j++) {
        DrmHwcLayer &layer = display_contents.layers[j];
        if(!layer.sf_handle && layer.raw_sf_layer && layer.raw_sf_layer->handle)
        {
          layer.sf_handle = layer.raw_sf_layer->handle;
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
          layer.width = hwc_get_handle_attibute(ctx->gralloc,layer.sf_handle,ATT_WIDTH);
          layer.height = hwc_get_handle_attibute(ctx->gralloc,layer.sf_handle,ATT_HEIGHT);
          layer.stride = hwc_get_handle_attibute(ctx->gralloc,layer.sf_handle,ATT_STRIDE);
          layer.format = hwc_get_handle_attibute(ctx->gralloc,layer.sf_handle,ATT_FORMAT);
#else
          layer.width = hwc_get_handle_width(ctx->gralloc,layer.sf_handle);
          layer.height = hwc_get_handle_height(ctx->gralloc,layer.sf_handle);
          layer.stride = hwc_get_handle_stride(ctx->gralloc,layer.sf_handle);
          layer.format = hwc_get_handle_format(ctx->gralloc,layer.sf_handle);
#endif
        }
        if(!layer.sf_handle)
        {
          ALOGE("%s: disply=%zu sf_handle is null,maybe fb target is null",__FUNCTION__,i);
          signal_all_fence(display_contents, dc);
          ctx->drm.ClearDisplay(i);
          std::vector<DrmCompositionDisplayLayersMap>::iterator iter = layers_map.begin()+i - fail_displays_count;
          layers_map.erase(iter);
          break;
        }

        if(!layer.raw_sf_layer)
        {
          ALOGE("%s: disply=%zu raw_sf_handle is null, hw_layer init error",__FUNCTION__,i);
          signal_all_fence(display_contents, dc);
          ctx->drm.ClearDisplay(i);
          std::vector<DrmCompositionDisplayLayersMap>::iterator iter = layers_map.begin()+i - fail_displays_count;
          layers_map.erase(iter);
          break;
        }
        if(!layer.bClone_)
        {
#if RK_RGA_PREPARE_ASYNC
          if(!layer.is_rotate_by_rga)
#endif
              layer.ImportBuffer(ctx, layer.raw_sf_layer, ctx->importer.get());
#if RK_RGA_PREPARE_ASYNC
          else
          {
              ret = layer.buffer.ImportBuffer(layer.rga_handle,
                                                     ctx->importer.get()
#if RK_VIDEO_SKIP_LINE
                                                     , layer.SkipLine
#endif
                                                     );
              if (ret) {
                  ALOGE("Failed to import rga buffer ret=%d", ret);
                  goto err;
              }

              ret = layer.handle.CopyBufferHandle(layer.rga_handle, ctx->gralloc);
              if (ret) {
                  ALOGE("Failed to copy rga handle ret=%d", ret);
                  goto err;
              }
          }
#endif
        }
        map.layers.emplace_back(std::move(layer));
      }
    }

  }

  if(layers_map.size() == 0)
  {
    ALOGD("%s: layers_map size is 0",__FUNCTION__);
    goto err;
  }

  ctx->drm.UpdateDisplayRoute();
  ctx->drm.UpdatePropertys();
  ctx->drm.ClearDisplay();

  composition = ctx->drm.compositor()->CreateComposition(ctx->importer.get(), get_frame());
  if (!composition) {
    ALOGE("%s: Drm composition init failed",__FUNCTION__);
    goto err;
  }

  ret = composition->SetLayers(layers_map.size(), layers_map.data());
  if (ret) {
    ALOGD("%s: SetLayers fail",__FUNCTION__);
    goto err;
  }

  for (size_t i = 0; i < num_displays; ++i) {
    if (!sf_display_contents[i] || i == HWC_DISPLAY_VIRTUAL)
        continue;

    DrmConnector *c = ctx->drm.GetConnectorFromType(i);
    if (!c || c->state() != DRM_MODE_CONNECTED) {
        continue;
    }
    hwc_drm_display_t *hd = &ctx->displays[c->display()];
    composition->SetMode3D(i, hd->stereo_mode);
  }

  for (size_t i = 0; i < ctx->comp_plane_group.size(); ++i) {
      if(ctx->comp_plane_group[i].composition_planes.size() > 0)
      {
          ret = composition->SetCompPlanes(ctx->comp_plane_group[i].display, ctx->comp_plane_group[i].composition_planes);
          if (ret) {
            ALOGE("%s: SetCompPlanes fail",__FUNCTION__);
            goto err;
          }
      }
      else
      {
          DrmHwcDisplayContents &display_contents = ctx->layer_contents[i];
          if (sf_display_contents[i])
          {
              signal_all_fence(display_contents, sf_display_contents[i]);
              ctx->drm.ClearDisplay(i);
          }
      }
  }

  /* rk:
   * we use loop to call QueueComposition to avoid the release fence leak.
   * Before:
   *    QueueComposition
   *      DrmComposition::Plan
   *        DrmDisplayComposition::Plan[0]---> return ok,it will create release fences for display 0
   *        DrmDisplayComposition::Plan[1]---> return err
   *      return err to DrmComposition::Plan,then return err to QueueComposition,
   *      it has no change to call DrmDisplayCompositor::QueueComposition(push composition in composite Queue.)
   *      So even you call ClearDisplay,the release fences of display 0 still cann't be signal.
   * There are two workrounds:
   * 1. You can call SignalCompositionDone or DrmDisplayComposition.reset(NULL) for display 0 before return to QueueComposition.
   *     Tip: DrmDisplayComposition.reset(NULL) will call SignalCompositionDone in ~DrmDisplayComposition.
   * 2. Use loop to separate the success and the error processing.(<----current workround.)
   *
   */
  for (size_t i = 0; i < HWC_NUM_PHYSICAL_DISPLAY_TYPES; ++i) {
    if (!sf_display_contents[i])
      continue;

    ret = ctx->drm.compositor()->QueueComposition(composition, i);
    if (ret) {
      ALOGE("%s: QueueComposition fail for display=%zu",__FUNCTION__,i);
      DrmHwcDisplayContents &display_contents = ctx->layer_contents[i];
      signal_all_fence(display_contents, sf_display_contents[i]);
      ctx->drm.ClearDisplay(i);
    }
  }

  for (size_t i = 0; i < num_displays; ++i) {
    hwc_display_contents_1_t *dc = sf_display_contents[i];
    bool bFindDisplay = false;
    if (!dc  || i == HWC_DISPLAY_VIRTUAL)
      continue;

    DrmConnector *c = ctx->drm.GetConnectorFromType(i);
    if (!c || c->state() != DRM_MODE_CONNECTED) {
      DrmHwcDisplayContents &display_contents = ctx->layer_contents[i];
      signal_all_fence(display_contents, sf_display_contents[i]);
      ctx->drm.ClearDisplay(i);
      continue;
    }
    hwc_drm_display_t *hd = &ctx->displays[c->display()];

    for (auto &fail_display : fail_displays) {
        if( i == fail_display )
        {
            bFindDisplay = true;
            ALOGD_IF(log_level(DBG_DEBUG),"%s:line=%d,Find fail display %zu",__FUNCTION__,__LINE__,i);
            break;
        }
    }
    if(bFindDisplay)
        continue;

    size_t num_dc_layers = dc->numHwLayers;
    for (size_t j = 0; j < num_dc_layers; ++j) {
      hwc_layer_1_t *layer = &dc->hwLayers[j];
      if (layer->flags & HWC_SKIP_LAYER)
        continue;
      hwc_add_layer_to_retire_fence(layer, dc);
    }
  }

  delete composition;
  composition = NULL;

#if RK_INVALID_REFRESH
  hwc_static_screen_opt_set(ctx->isGLESComp);
#endif
  ALOGD_IF(log_level(DBG_VERBOSE),"----------------------------frame=%d end----------------------------",get_frame());

  return ret;

err:
    ALOGE("%s: not normal frame happen",__FUNCTION__);
    for (size_t i = 0; i < num_displays; ++i) {
        hwc_display_contents_1_t *dc = sf_display_contents[i];
        if (!dc || i == HWC_DISPLAY_VIRTUAL)
          continue;

        int num_layers = dc->numHwLayers;
        for (int j = 0; j < num_layers; j++) {
          hwc_layer_1_t *layer = &dc->hwLayers[j];
          dump_layer(ctx->gralloc, true, layer, j);
        }

        DrmHwcDisplayContents &display_contents = ctx->layer_contents[i];
        signal_all_fence(display_contents, sf_display_contents[i]);
    }
    if(NULL != composition)
    {
      delete composition;
      composition = NULL;
    }
    ctx->drm.ClearAllDisplay();
    return -EINVAL;
}

static int hwc_event_control(struct hwc_composer_device_1 *dev, int display,
                             int event, int enabled) {
  if (event != HWC_EVENT_VSYNC || (enabled != 0 && enabled != 1))
    return -EINVAL;

  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  if (display == HWC_DISPLAY_PRIMARY)
    return ctx->primary_vsync_worker.VSyncControl(enabled);
  else if (display == HWC_DISPLAY_EXTERNAL)
    return ctx->extend_vsync_worker.VSyncControl(enabled);

  ALOGE("Can't support vsync control for display %d\n", display);
  return -EINVAL;
}

static int hwc_set_power_mode(struct hwc_composer_device_1 *dev, int display,
                              int mode) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;

  uint64_t dpmsValue = 0;
  switch (mode) {
    case HWC_POWER_MODE_OFF:
      dpmsValue = DRM_MODE_DPMS_OFF;
      break;

    /* We can't support dozing right now, so go full on */
    case HWC_POWER_MODE_DOZE:
    case HWC_POWER_MODE_DOZE_SUSPEND:
    case HWC_POWER_MODE_NORMAL:
      dpmsValue = DRM_MODE_DPMS_ON;
      break;
  };

  int fb_blank = 0;
  if(dpmsValue == DRM_MODE_DPMS_OFF)
      fb_blank = FB_BLANK_POWERDOWN;
  else if(dpmsValue == DRM_MODE_DPMS_ON)
      fb_blank = FB_BLANK_UNBLANK;
  else
      ALOGE("dpmsValue is invalid value= %" PRIu64 "",dpmsValue);
  if(fb_blank != ctx->fb_blanked && ctx->fb_fd > 0){
    int err = ioctl(ctx->fb_fd, FBIOBLANK, fb_blank);
    ALOGD_IF(log_level(DBG_DEBUG),"%s Notice fb_blank to fb=%d", __FUNCTION__, fb_blank);
    if (err < 0) {
        if (errno == EBUSY)
            ALOGD("fb_blank ioctl failed display=%d,fb_blank=%d,dpmsValue=%" PRIu64 "",
                    display,fb_blank,dpmsValue);
        else
            ALOGE("fb_blank ioctl failed(%s) display=%d,fb_blank=%d,dpmsValue=%" PRIu64 "",
                    strerror(errno),display,fb_blank,dpmsValue);
        return -errno;
    }
  }
  ctx->fb_blanked = fb_blank;
  DrmConnector *connector = ctx->drm.GetConnectorFromType(display);
  if (!connector) {
    ALOGE("%s:Failed to get connector for display %d line=%d", __FUNCTION__,display,__LINE__);
    return -ENODEV;
  }

  //In tv mode, we still need update the hdmi force_disconnect.
  //Pg: sleep(force_disconnect will be true) ==> plug out HDMI(switch to tv mode) ==> wake up ==> plug in HDMI(force_disconnect still be true)
  //Workround:                                                                            ==> update HDMI's force_disconnect
  if(connector->get_type() == DRM_MODE_CONNECTOR_TV)
  {
    for (auto &conn : ctx->drm.connectors())
    {
        if(conn->get_type() == DRM_MODE_CONNECTOR_HDMIA)
        {
            conn->force_disconnect(dpmsValue == DRM_MODE_DPMS_OFF);
            break;
        }
    }
  }

  connector->force_disconnect(dpmsValue == DRM_MODE_DPMS_OFF);

  //If process of device boot have two connector's status is connected,
  //the first setPowerMode will to set drmMode-id for every connector
  //in order to avoid some initialization problems.
  for (int i = 0; i < HWC_NUM_PHYSICAL_DISPLAY_TYPES; i++) {
    DrmConnector *conn = ctx->drm.GetConnectorFromType(i);
    if (!conn || conn->state() != DRM_MODE_CONNECTED)
      continue;
    hwc_drm_display_t *hd = &ctx->displays[conn->display()];
    update_display_bestmode(hd, i, conn);
    DrmMode drmmode = conn->best_mode();
    conn->set_current_mode(drmmode);
  }

  ctx->drm.DisplayChanged();
  ctx->drm.UpdateDisplayRoute();
  ctx->drm.ClearDisplay();

  return 0;
}

static int hwc_query(struct hwc_composer_device_1 * /* dev */, int what,
                     int *value) {
  switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
      *value = 0; /* TODO: We should do this */
      break;
    case HWC_VSYNC_PERIOD:
      ALOGW("Query for deprecated vsync value, returning 60Hz");
      *value = 1000 * 1000 * 1000 / 60;
      break;
    case HWC_DISPLAY_TYPES_SUPPORTED:
      *value = HWC_DISPLAY_PRIMARY_BIT | HWC_DISPLAY_EXTERNAL_BIT |
               HWC_DISPLAY_VIRTUAL_BIT;
      break;
  }
  return 0;
}

static void hwc_register_procs(struct hwc_composer_device_1 *dev,
                               hwc_procs_t const *procs) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;

  ctx->procs = procs;

  ctx->primary_vsync_worker.SetProcs(procs);
  ctx->extend_vsync_worker.SetProcs(procs);
  ctx->hotplug_handler.Init(&ctx->displays, &ctx->drm, procs);
  ctx->drm.event_listener()->RegisterHotplugHandler(&ctx->hotplug_handler);
}

static int hwc_get_display_configs(struct hwc_composer_device_1 *dev,
                                   int display, uint32_t *configs,
                                   size_t *num_configs) {
  if (!num_configs)
    return 0;

  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  DrmConnector *connector = ctx->drm.GetConnectorFromType(display);
  if (!connector) {
    ALOGE("%s:Failed to get connector for display %d line=%d", __FUNCTION__,display,__LINE__);
    return -ENODEV;
  }

  hwc_drm_display_t *hd = &ctx->displays[connector->display()];
  if (!hd->active)
    return -ENODEV;

  int ret = connector->UpdateModes();
  if (ret) {
    ALOGE("Failed to update display modes %d", ret);
    return ret;
  }

  if (connector->state() != DRM_MODE_CONNECTED && display == HWC_DISPLAY_EXTERNAL) {
    ALOGE("connector is not connected with display %d", display);
    return -ENODEV;
  }

  update_display_bestmode(hd, display, connector);
  DrmMode mode = connector->best_mode();
  connector->set_current_mode(mode);

  char framebuffer_size[PROPERTY_VALUE_MAX];
  uint32_t width = 0, height = 0 , vrefresh = 0 ;
  if (display == HWC_DISPLAY_PRIMARY)
    property_get("persist." PROPERTY_TYPE ".framebuffer.main", framebuffer_size, "use_baseparameter");
  else if(display == HWC_DISPLAY_EXTERNAL)
    property_get("persist." PROPERTY_TYPE ".framebuffer.aux", framebuffer_size, "use_baseparameter");
  /*
   * if unset framebuffer_size, get it from baseparameter , by libin
   */
  if(hwc_have_baseparameter() && !strcmp(framebuffer_size,"use_baseparameter")){
    int res = 0;
    res = hwc_get_baseparameter_config(framebuffer_size,display,BP_FB_SIZE,0);
    if(res)
        ALOGW("BP: hwc get baseparameter config err ,res = %d",res);
  }

  sscanf(framebuffer_size, "%dx%d@%d", &width, &height, &vrefresh);
  if (width && height) {
    hd->framebuffer_width = width;
    hd->framebuffer_height = height;
    hd->vrefresh = vrefresh ? vrefresh : 60;
  } else if (mode.h_display() && mode.v_display() && mode.v_refresh()) {
    hd->framebuffer_width = mode.h_display();
    hd->framebuffer_height = mode.v_display();
    hd->vrefresh = mode.v_refresh();
    /*
     * Limit to 1080p if large than 2160p
     */
    if (hd->framebuffer_height >= 2160 && hd->framebuffer_width >= hd->framebuffer_height) {
      hd->framebuffer_width = hd->framebuffer_width * (1080.0 / hd->framebuffer_height);
      hd->framebuffer_height = 1080;
    }
  } else {
    hd->framebuffer_width = 1920;
    hd->framebuffer_height = 1080;
    hd->vrefresh = 60;
    ALOGE("Failed to find available display mode for display %d\n", display);
  }

  hd->rel_xres = mode.h_display();
  hd->rel_yres = mode.v_display();
  hd->v_total = mode.v_total();

  // AFBDC limit
  bool disable_afbdc = false;
  if(display == HWC_DISPLAY_PRIMARY){
#ifdef TARGET_BOARD_PLATFORM_RK3399
    if(hd->framebuffer_width > 2560 || hd->framebuffer_width % 16 != 0 || hd->framebuffer_height % 8 != 0)
       disable_afbdc = true;
#elif TARGET_BOARD_PLATFORM_RK3368
    if(hd->framebuffer_width > 2048 || hd->framebuffer_width % 16 != 0 || hd->framebuffer_height % 4 != 0)
       disable_afbdc = true;
#elif TARGET_BOARD_PLATFORM_RK3326
    if(hd->framebuffer_width > 1920 || hd->framebuffer_width % 16 != 0 || hd->framebuffer_height % 8 != 0)
       disable_afbdc = true;
#endif
    if(disable_afbdc){
      property_set( PROPERTY_TYPE ".gralloc.disable_afbc", "1");
      ALOGI("%s:line=%d primary framebuffer size %dx%d not support AFBDC, to disable AFBDC\n",
               __FUNCTION__, __LINE__, hd->framebuffer_width,hd->framebuffer_height);
    }
  }
  *num_configs = 1;
  configs[0] = connector->display();

  return 0;
}

static float getDefaultDensity(uint32_t width, uint32_t height) {
    // Default density is based on TVs: 1080p displays get XHIGH density,
    // lower-resolution displays get TV density. Maybe eventually we'll need
    // to update it for 4K displays, though hopefully those just report
    // accurate DPI information to begin with. This is also used for virtual
    // displays and even primary displays with older hwcomposers, so be
    // careful about orientation.

    uint32_t h = width < height ? width : height;
    if (h >= 1080) return ACONFIGURATION_DENSITY_XHIGH;
    else           return ACONFIGURATION_DENSITY_TV;
}

static int hwc_get_display_attributes(struct hwc_composer_device_1 *dev,
                                      int display, uint32_t config,
                                      const uint32_t *attributes,
                                      int32_t *values) {
  UN_USED(config);
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  DrmConnector *c = ctx->drm.GetConnectorFromType(display);
  if (!c) {
    ALOGE("Failed to get DrmConnector for display %d", display);
    return -ENODEV;
  }
  hwc_drm_display_t *hd = &ctx->displays[c->display()];
  if (!hd->active)
    return -ENODEV;
  uint32_t mm_width = c->mm_width();
  uint32_t mm_height = c->mm_height();
  int w = hd->framebuffer_width;
  int h = hd->framebuffer_height;
  int vrefresh = hd->vrefresh;

  for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; ++i) {
    switch (attributes[i]) {
      case HWC_DISPLAY_VSYNC_PERIOD:
        values[i] = 1000 * 1000 * 1000 / vrefresh;
        break;
      case HWC_DISPLAY_WIDTH:
        values[i] = w;
        break;
      case HWC_DISPLAY_HEIGHT:
        values[i] = h;
        break;
      case HWC_DISPLAY_DPI_X:
        /* Dots per 1000 inches */
        values[i] = mm_width ? (w * UM_PER_INCH) / mm_width : getDefaultDensity(w,h)*1000;
        break;
      case HWC_DISPLAY_DPI_Y:
        /* Dots per 1000 inches */
        values[i] =
            mm_height ? (h * UM_PER_INCH) / mm_height : getDefaultDensity(w,h)*1000;
        break;
    }
  }
  return 0;
}

static int hwc_get_active_config(struct hwc_composer_device_1 *dev,
                                 int display) {
  UN_USED(dev);
  UN_USED(display);
  return 0;
}

static int hwc_set_active_config(struct hwc_composer_device_1 *dev, int display,
                                 int index) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;

  UN_USED(index);
  DrmConnector *c = ctx->drm.GetConnectorFromType(display);
  if (!c) {
    ALOGE("%s:Failed to get connector for display %d line=%d", __FUNCTION__,display,__LINE__);
    return -ENODEV;
  }

  if (c->state() != DRM_MODE_CONNECTED) {
    /*
     * fake primary display if primary is not connected.
     */
    if (display == HWC_DISPLAY_PRIMARY)
      return 0;

    return -ENODEV;
  }

  hwc_drm_display_t *hd = &ctx->displays[c->display()];


  DrmMode mode = c->best_mode();
  if (!mode.id()) {
    ALOGE("Could not find active mode for display=%d", display);
    return -ENOENT;
  }
  hd->w_scale = (float)mode.h_display() / hd->framebuffer_width;
  hd->h_scale = (float)mode.v_display() / hd->framebuffer_height;

  c->set_current_mode(mode);
  ctx->drm.UpdateDisplayRoute();

  return 0;
}

static int hwc_device_close(struct hw_device_t *dev) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)dev;

#if RK_CTS_WORKROUND
  if(ctx->regFile)
  {
    fclose(ctx->regFile);
    ctx->regFile = NULL;
  }
#endif

#if RK_INVALID_REFRESH
    free_thread_pamaters(&ctx->mRefresh);
#endif
#if 0
    if(ctx->fd_3d >= 0)
    {
        close(ctx->fd_3d);
        ctx->fd_3d = -1;
    }
    free_thread_pamaters(&ctx->mControlStereo);
#endif
  delete ctx;
  return 0;
}

/*
 * TODO: This function sets the active config to the first one in the list. This
 * should be fixed such that it selects the preferred mode for the display, or
 * some other, saner, method of choosing the config.
 */
static int hwc_set_initial_config(struct hwc_context_t *ctx, int display) {
  uint32_t config;
  size_t num_configs = 1;
  int ret = hwc_get_display_configs(&ctx->device, display, &config,
                                    &num_configs);
  if (ret || !num_configs)
    return 0;

  ret = hwc_set_active_config(&ctx->device, display, 0);
  if (ret) {
    ALOGE("Failed to set active config d=%d ret=%d", display, ret);
    return ret;
  }

  return ret;
}

static int hwc_initialize_display(struct hwc_context_t *ctx, int display) {
    hwc_drm_display_t *hd = &ctx->displays[display];
    hd->ctx = ctx;
    hd->gralloc = ctx->gralloc;
#if RK_VIDEO_UI_OPT
    hd->iUiFd = -1;
    hd->bHideUi = false;
#endif
    hd->framebuffer_width = 0;
    hd->framebuffer_height = 0;
    hd->rel_xres = 0;
    hd->rel_yres = 0;
    hd->v_total = 0;
    hd->w_scale = 1.0;
    hd->h_scale = 1.0;
    hd->active = true;
    hd->last_hdmi_status = HDMI_ON;
    hd->isHdr = false;
    memset(&hd->last_hdr_metadata, 0, sizeof(hd->last_hdr_metadata));
    hd->colorimetry = 0;
    hd->hotplug_timeline = 0;
    hd->display_timeline = 0;
    hd->is_3d = false;
    hd->hasEotfPlane = false;
    hd->bPreferMixDown = false;

#if RK_RGA_PREPARE_ASYNC
    hd->rgaBuffer_index = 0;
    hd->mUseRga = false;
#endif
#if RK_ROTATE_VIDEO_MODE
    hd->bRotateVideoMode = false;
#endif
    return 0;
}

static int hwc_enumerate_displays(struct hwc_context_t *ctx) {
  int ret, num_connectors = 0;

  for (auto &conn : ctx->drm.connectors()) {
    ret = hwc_initialize_display(ctx, conn->display());
    if (ret) {
      ALOGE("Failed to initialize display %d", conn->display());
      return ret;
    }
    num_connectors++;
  }
#if 0
  ret = hwc_set_initial_config(ctx, HWC_DISPLAY_PRIMARY);
  if (ret) {
    ALOGE("Failed to set initial config for primary display ret=%d", ret);
    return ret;
  }

  ret = hwc_set_initial_config(ctx, HWC_DISPLAY_EXTERNAL);
  if (ret) {
    ALOGE("Failed to set initial config for extend display ret=%d", ret);
//    return ret;
  }
#endif

  ret = ctx->primary_vsync_worker.Init(&ctx->drm, HWC_DISPLAY_PRIMARY);
  if (ret) {
    ALOGE("Failed to create event worker for primary display %d\n", ret);
    return ret;
  }

  if (num_connectors > 1) {
    ret = ctx->extend_vsync_worker.Init(&ctx->drm, HWC_DISPLAY_EXTERNAL);
    if (ret) {
      ALOGE("Failed to create event worker for extend display %d\n", ret);
      return ret;
    }
  }

  ret = ctx->virtual_compositor_worker.Init();
  if (ret) {
    ALOGE("Failed to initialize virtual compositor worker");
    return ret;
  }
  return 0;
}

#if RK_INVALID_REFRESH
static void hwc_static_screen_opt_handler(int sig)
{
    hwc_context_t* ctx = g_ctx;
    if (sig == SIGALRM) {
        ctx->mOneWinOpt = true;
        pthread_mutex_lock(&ctx->mRefresh.mlk);
        ctx->mRefresh.count = 100;
        ALOGD_IF(log_level(DBG_VERBOSE),"hwc_static_screen_opt_handler:mRefresh.count=%d",ctx->mRefresh.count);
        pthread_mutex_unlock(&ctx->mRefresh.mlk);
        pthread_cond_signal(&ctx->mRefresh.cond);
    }

    return;
}

void  *invalidate_refresh(void *arg)
{
    hwc_context_t* ctx = (hwc_context_t*)arg;
    int count = 0;
    int nMaxCnt = 25;
    unsigned int nSleepTime = 200;

    pthread_cond_wait(&ctx->mRefresh.cond,&ctx->mRefresh.mtx);
    while(true) {
        for(count = 0; count < nMaxCnt; count++) {
            usleep(nSleepTime*1000);
            pthread_mutex_lock(&ctx->mRefresh.mlk);
            count = ctx->mRefresh.count;
            ctx->mRefresh.count ++;
            ALOGD_IF(log_level(DBG_VERBOSE),"invalidate_refresh mRefresh.count=%d",ctx->mRefresh.count);
            pthread_mutex_unlock(&ctx->mRefresh.mlk);
            ctx->procs->invalidate(ctx->procs);
        }
        pthread_cond_wait(&ctx->mRefresh.cond,&ctx->mRefresh.mtx);
        count = 0;
    }

    pthread_exit(NULL);
    return NULL;
}
#endif

#if 0
void* hwc_control_3dmode_thread(void *arg)
{
    hwc_context_t* ctx = (hwc_context_t*)arg;
    int ret = -1;
    int needStereo = 0;

    ALOGD("hwc_control_3dmode_thread creat");
    pthread_cond_wait(&ctx->mControlStereo.cond,&ctx->mControlStereo.mtx);
    while(true) {
        pthread_mutex_lock(&ctx->mControlStereo.mlk);
        needStereo = ctx->mControlStereo.count;
        pthread_mutex_unlock(&ctx->mControlStereo.mlk);
        ret = hwc_control_3dmode(ctx->fb_3d, 2, READ_3D_MODE);
        if(needStereo != ret) {
            hwc_control_3dmode(ctx, needStereo,WRITE_3D_MODE);
            ALOGI_IF(log_level(DBG_VERBOSE),"change stereo mode %d to %d",ret,needStereo);
        }
        ALOGD_IF(log_level(DBG_VERBOSE),"mControlStereo.count=%d",needStereo);
        pthread_cond_wait(&ctx->mControlStereo.cond,&ctx->mControlStereo.mtx);
    }
    ALOGD("hwc_control_3dmode_thread exit");
    pthread_exit(NULL);
    return NULL;
}
#endif

static int hwc_device_open(const struct hw_module_t *module, const char *name,
                           struct hw_device_t **dev) {
  if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
    ALOGE("Invalid module name- %s", name);
    return -EINVAL;
  }

  init_rk_debug();
  hwc_get_baseparameter_config(NULL,0,BP_UPDATE,0);

  std::unique_ptr<hwc_context_t> ctx(new hwc_context_t());
  if (!ctx) {
    ALOGE("Failed to allocate hwc context");
    return -ENOMEM;
  }

  int ret = ctx->drm.Init();
  if (ret) {
    ALOGE("Can't initialize Drm object %d", ret);
    return ret;
  }
#if USE_GRALLOC_4
    ctx->gralloc =  NULL;
#else   // USE_GRALLOC_4

  ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                      (const hw_module_t **)&ctx->gralloc);
  if (ret) {
    ALOGE("Failed to open gralloc module %d", ret);
    return ret;
  }
#endif  // USE_GRALLOC_4

  ctx->drm.setGralloc(ctx->gralloc);

  ret = ctx->dummy_timeline.Init();
  if (ret) {
    ALOGE("Failed to create dummy sw sync timeline %d", ret);
    return ret;
  }

  ctx->importer.reset(Importer::CreateInstance(&ctx->drm));
  if (!ctx->importer) {
    ALOGE("Failed to create importer instance");
    return ret;
  }

  ret = hwc_enumerate_displays(ctx.get());
  if (ret) {
    ALOGE("Failed to enumerate displays: %s", strerror(ret));
    return ret;
  }

  ctx->device.common.tag = HARDWARE_DEVICE_TAG;
  ctx->device.common.version = HWC_DEVICE_API_VERSION_1_4;
  ctx->device.common.module = const_cast<hw_module_t *>(module);
  ctx->device.common.close = hwc_device_close;

  ctx->device.dump = hwc_dump;
  ctx->device.prepare = hwc_prepare;
  ctx->device.set = hwc_set;
  ctx->device.eventControl = hwc_event_control;
  ctx->device.setPowerMode = hwc_set_power_mode;
  ctx->device.query = hwc_query;
  ctx->device.registerProcs = hwc_register_procs;
  ctx->device.getDisplayConfigs = hwc_get_display_configs;
  ctx->device.getDisplayAttributes = hwc_get_display_attributes;
  ctx->device.getActiveConfig = hwc_get_active_config;
  ctx->device.setActiveConfig = hwc_set_active_config;
  ctx->device.setCursorPositionAsync = NULL; /* TODO: Add cursor */


  g_ctx = ctx.get();

    ctx->fb_fd = open("/dev/graphics/fb0", O_RDWR, 0);
    if(ctx->fb_fd < 0)
    {
         ALOGE("Open fb0 fail in %s",__FUNCTION__);
    }

    ctx->hdmi_status_fd = open(HDMI_STATUS_PATH, O_RDWR, 0);
    if(ctx->hdmi_status_fd < 0)
    {
         ALOGE("Open hdmi_status_fd fail in %s",__FUNCTION__);
         //return -1;
    }
    ctx->dp_status_fd = open(DP_STATUS_PATH, O_RDWR, 0);
    if(ctx->hdmi_status_fd < 0)
    {
         ALOGE("Open hdmi_status_fd fail in %s",__FUNCTION__);
         //return -1;
    }

#if RK_CTS_WORKROUND
    ctx->regFile = fopen(VIEW_CTS_FILE, "r");
    if(ctx->regFile == NULL)
    {
        ALOGE("%s open fail errno=0x%x  (%s)",__FUNCTION__, errno,strerror(errno));
    }
#endif

  hwc_init_version();

    ctx->hdr_video_compose_by_gles = hwc_get_bool_property( PROPERTY_TYPE ".hwc.hdr_video_by_gles","false");
    ALOGI("HWC property : hdr_video_by_gles = %s",ctx->hdr_video_compose_by_gles ? "True" : "False");

#if RK_INVALID_REFRESH
    ctx->mOneWinOpt = false;
    ctx->isGLESComp = false;
    init_thread_pamaters(&ctx->mRefresh);
    pthread_t invalidate_refresh_th;
    if (pthread_create(&invalidate_refresh_th, NULL, invalidate_refresh, ctx.get()))
    {
        ALOGE("Create invalidate_refresh_th thread error .");
    }

    signal(SIGALRM, hwc_static_screen_opt_handler);
#endif

#if 0
    init_thread_pamaters(&ctx->mControlStereo);
    ctx->fd_3d = open("/sys/class/display/HDMI/3dmode", O_RDWR, 0);
    if(ctx->fd_3d < 0){
        ALOGE("open /sys/class/display/HDMI/3dmode fail");
    }

    pthread_t thread_3d;
    if (pthread_create(&thread_3d, NULL, hwc_control_3dmode_thread, ctx.get()))
    {
        ALOGE("Create hwc_control_3dmode_thread thread error .");
    }

#endif

  *dev = &ctx->device.common;
  ctx.release();

  return 0;
}
}

static struct hw_module_methods_t hwc_module_methods = {
  .open = android::hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
  .common = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = HWC_HARDWARE_MODULE_ID,
    .name = "DRM hwcomposer module",
    .author = "The Android Open Source Project",
    .methods = &hwc_module_methods,
    .dso = NULL,
    .reserved = {0},
  }
};
