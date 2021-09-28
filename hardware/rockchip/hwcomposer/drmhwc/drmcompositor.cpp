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

#define LOG_TAG "hwc-drm-compositor"

#include "drmcompositor.h"
#include "drmdisplaycompositor.h"
#include "drmresources.h"
#include "platform.h"

#include <sstream>
#include <stdlib.h>

#ifdef ANDROID_P
#include <log/log.h>
#else
#include <cutils/log.h>

#endif

namespace android {

DrmCompositor::DrmCompositor(DrmResources *drm) : drm_(drm), frame_no_(0) {
}

DrmCompositor::~DrmCompositor() {
}

int DrmCompositor::Init() {
  int i;
  for (i = 0; i < HWC_NUM_PHYSICAL_DISPLAY_TYPES; i++) {
    int ret = compositor_map_[i].Init(drm_, i);
    if (ret) {
      ALOGE("Failed to initialize display compositor for %d", i);
      return ret;
    }
  }
  planner_ = Planner::CreateInstance(drm_);
  if (!planner_) {
    ALOGE("Failed to create planner instance for composition");
    return -ENOMEM;
  }

  return 0;
}

DrmComposition* DrmCompositor::CreateComposition(
    Importer *importer, unsigned int frame_no) {
  DrmComposition* composition(
      new DrmComposition(drm_, importer, planner_.get()));
  frame_no_ = frame_no;
  int ret = composition->Init(frame_no);
  if (ret) {
    ALOGE("Failed to initialize drm composition %d", ret);
    return nullptr;
  }
  return composition;
}

int DrmCompositor::QueueComposition(
    DrmComposition* composition, int dispaly) {
  int ret = 0;

  if(!composition || dispaly >= HWC_NUM_PHYSICAL_DISPLAY_TYPES)
  {
    ALOGE("%s:invalid input parameter display=%d,composition=%p", __FUNCTION__, dispaly,composition);
    return -1;
  }

  //If it return successful,it will create release fence.
  ret = composition->Plan(compositor_map_, dispaly);
  if(ret)
  {
    ALOGE("%s:Plan fail for display %d", __FUNCTION__, dispaly);
    return ret;
  }

  ret = composition->DisableUnusedPlanes(dispaly);
  if(ret)
  {
    ALOGE("%s:DisableUnusedPlanes fail for display %d", __FUNCTION__, dispaly);
    return ret;
  }

  //It will push composition in composite Queue.
  ret = compositor_map_[dispaly].QueueComposition(
      composition->TakeDisplayComposition(dispaly));
  if(ret)
  {
    ALOGE("%s: Failed to queue composition for display %d", __FUNCTION__, dispaly);
    return ret;
  }

  return ret;
}

int DrmCompositor::Composite() {
  /*
   * This shouldn't be called, we should be calling Composite() on the display
   * compositors directly.
   */
  ALOGE("Calling base drm compositor Composite() function");
  return -EINVAL;
}

void DrmCompositor::ClearDisplay(int display) {
  compositor_map_[display].ClearDisplay();
}

void DrmCompositor::Dump(std::ostringstream *out) const {
  *out << "DrmCompositor stats:\n";
  int i;
  for (i = 0; i < HWC_NUM_PHYSICAL_DISPLAY_TYPES; i++) {
    compositor_map_[i].Dump(out);
  }
}
}
