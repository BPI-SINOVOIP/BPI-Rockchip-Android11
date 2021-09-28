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

#define LOG_TAG "hwc-drm-composition"

#include "drmcomposition.h"
#include "drmcrtc.h"
#include "drmplane.h"
#include "drmresources.h"
#include "platform.h"

#include <stdlib.h>
#include "hwc_rockchip.h"

#ifdef ANDROID_P
#include <log/log.h>
#include <libsync/sw_sync.h>
#else
#include <cutils/log.h>
#include <sw_sync.h>
#endif

#include <cutils/properties.h>

#include <sync/sync.h>

namespace android {

DrmComposition::DrmComposition(DrmResources *drm, Importer *importer,
                               Planner *planner)
    : drm_(drm), importer_(importer), planner_(planner) {
  char use_overlay_planes_prop[PROPERTY_VALUE_MAX];
  property_get( PROPERTY_TYPE ".hwc.drm.use_overlay_planes", use_overlay_planes_prop, "1");
  bool use_overlay_planes = atoi(use_overlay_planes_prop);
  for (auto &plane : drm->sort_planes()) {
    if (plane->type() == DRM_PLANE_TYPE_PRIMARY)
      primary_planes_.push_back(plane);
    else if (use_overlay_planes && (plane->type() == DRM_PLANE_TYPE_OVERLAY ||
            plane->type() == DRM_PLANE_TYPE_CURSOR))
      overlay_planes_.push_back(plane);
  }
}

int DrmComposition::Init(uint64_t frame_no) {
  int i;

  for (i = 0; i < HWC_NUM_PHYSICAL_DISPLAY_TYPES; i++) {
    composition_map_[i].reset(new DrmDisplayComposition());
    if (!composition_map_[i]) {
      ALOGE("Failed to allocate new display composition\n");
      return -ENOMEM;
    }

    DrmConnector *c = drm_->GetConnectorFromType(i);
    if (!c || c->state() != DRM_MODE_CONNECTED)
      continue;
    DrmCrtc *crtc = drm_->GetCrtcFromConnector(c);
    if (!crtc)
      continue;

    int ret = composition_map_[i]->Init(drm_, crtc, importer_, planner_, frame_no);
    if (ret) {
      ALOGE("Failed to init display composition for %d", c->display());
      return ret;
    }
  }
  return 0;
}

int DrmComposition::SetLayers(size_t num_displays,
                              DrmCompositionDisplayLayersMap *maps) {
  int ret = 0;
  for (size_t display_index = 0; display_index < num_displays;
       display_index++) {
    DrmCompositionDisplayLayersMap &map = maps[display_index];
    int display = map.display;

    if (!composition_map_[display]->crtc())
      continue;

    ret = composition_map_[display]->SetLayers(
        map.layers.data(), map.layers.size(), map.geometry_changed);
    if (ret)
      return ret;
  }

  return 0;
}

int DrmComposition::SetMode3D(int display, Mode3D mode) {
  return composition_map_[display]->SetMode3D(mode);
}

int DrmComposition::SetDpmsMode(int display, uint32_t dpms_mode) {
  return composition_map_[display]->SetDpmsMode(dpms_mode);
}

int DrmComposition::SetDisplayMode(int display, const DrmMode &display_mode) {
  return composition_map_[display]->SetDisplayMode(display_mode);
}

int DrmComposition::SetCompPlanes(int display, std::vector<DrmCompositionPlane>& composition_planes) {
    return composition_map_[display]->SetCompPlanes(composition_planes);
}

std::unique_ptr<DrmDisplayComposition> DrmComposition::TakeDisplayComposition(
    int display) {
  return std::move(composition_map_[display]);
}

int DrmComposition::Plan(std::map<int, DrmDisplayCompositor> &compositor_map, int display) {
  int ret = 0;
  if (!composition_map_[display]->crtc())
  {
     ALOGE("%s: crtc is null", __FUNCTION__);
     return 0;
  }
  DrmDisplayComposition *comp = GetDisplayComposition(display);
  ret = comp->Plan(compositor_map[display].squash_state(), &primary_planes_,
                   &overlay_planes_);
  if (ret) {
    ALOGE("Failed to plan composition for dislay %d", display);
    return ret;
  }

  return 0;
}

int DrmComposition::DisableUnusedPlanes(int display) {
std::vector<PlaneGroup *>& plane_groups = drm_->GetPlaneGroups();

  DrmCrtc *crtc = composition_map_[display]->crtc();
  if (!crtc)
  {
     ALOGE("%s: crtc is null", __FUNCTION__);
     return 0;
  }

  DrmDisplayComposition *comp = GetDisplayComposition(display);

  /*
   * Leave empty compositions alone
   * TODO: re-visit this and potentially disable leftover planes after the
   *       active compositions have gobbled up all they can
   */
  if (comp->type() == DRM_COMPOSITION_TYPE_EMPTY ||
      comp->type() == DRM_COMPOSITION_TYPE_MODESET)
  {
    return 0;
  }

  //loop plane groups.
  for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
     iter != plane_groups.end(); ++iter) {
      //loop plane
      for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
          !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane) {
          if ((*iter_plane)->GetCrtcSupported(*crtc) && !(*iter_plane)->is_use()) {
              ALOGD_IF(log_level(DBG_DEBUG),"DisableUnusedPlanes plane_groups plane id=%d",(*iter_plane)->id());
              comp->AddPlaneDisable(*iter_plane);
             // break;
          }
      }
  }
  return 0;
}

DrmDisplayComposition *DrmComposition::GetDisplayComposition(int display) {
  return composition_map_[display].get();
}
}
