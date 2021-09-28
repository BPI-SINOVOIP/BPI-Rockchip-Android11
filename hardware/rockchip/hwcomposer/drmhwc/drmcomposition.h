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

#ifndef ANDROID_DRM_COMPOSITION_H_
#define ANDROID_DRM_COMPOSITION_H_

#include "drmhwcomposer.h"
#include "drmdisplaycomposition.h"
#include "drmplane.h"
#include "platform.h"

#include <map>
#include <vector>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

namespace android {

class DrmDisplayCompositor;

struct DrmCompositionDisplayLayersMap {
  int display;
  bool geometry_changed = true;
  std::vector<DrmHwcLayer> layers;

  DrmCompositionDisplayLayersMap() = default;

};

struct DrmCompositionDisplayPlane {
  int display;
  std::vector<DrmCompositionPlane> composition_planes;

  DrmCompositionDisplayPlane() = default;
  DrmCompositionDisplayPlane(DrmCompositionDisplayPlane &&rhs) =
      default;
};

class DrmComposition {
 public:
  DrmComposition(DrmResources *drm, Importer *importer, Planner *planner);

  int Init(uint64_t frame_no);

  int SetLayers(size_t num_displays, DrmCompositionDisplayLayersMap *maps);
  int SetMode3D(int display, Mode3D mode);
  int SetDpmsMode(int display, uint32_t dpms_mode);
  int SetDisplayMode(int display, const DrmMode &display_mode);
  int SetCompPlanes(int display, std::vector<DrmCompositionPlane>& composition_planes);

  std::unique_ptr<DrmDisplayComposition> TakeDisplayComposition(int display);
  DrmDisplayComposition *GetDisplayComposition(int display);

  int Plan(std::map<int, DrmDisplayCompositor> &compositor_map, int display);
  int DisableUnusedPlanes(int display);

 private:
  DrmComposition(const DrmComposition &) = delete;

  DrmResources *drm_;
  Importer *importer_;
  Planner *planner_;

  std::vector<DrmPlane *> primary_planes_;
  std::vector<DrmPlane *> overlay_planes_;
  std::vector<DrmCompositionDisplayPlane> comp_plane_group;

  /*
   * This _must_ be read-only after it's passed to QueueComposition. Otherwise
   * locking is required to maintain consistency across the compositor threads.
   */
  std::map<int, std::unique_ptr<DrmDisplayComposition>> composition_map_;
};
}

#endif  // ANDROID_DRM_COMPOSITION_H_
