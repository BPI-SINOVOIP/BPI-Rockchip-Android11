/*
 * Copyright (C) 2020 Rockchip Electronics Co.Ltd.
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
#ifndef ANDROID_DRM_VOP_3399_H_
#define ANDROID_DRM_VOP_3399_H_

#include "platform.h"
#include "drmdevice.h"

#include <cutils/properties.h>

namespace android {
class DrmDevice;

// This plan stage places as many layers on dedicated planes as possible (first
// come first serve), and then sticks the rest in a precomposition plane (if
// needed).
class Vop3399 : public Planner::PlanStage {

typedef std::map<int, std::vector<DrmHwcLayer*>> LayerMap;

typedef enum tagComposeMode{
   HWC_OVERLAY_LOPICY,
   HWC_MIX_SKIP_LOPICY,
   HWC_MIX_VIDEO_LOPICY,
   HWC_MIX_UP_LOPICY,
   HWC_MIX_DOWN_LOPICY,
   HWC_MIX_LOPICY,
   HWC_GLES_POLICY,
   HWC_RGA_OVERLAY_LOPICY,
   HWC_3D_LOPICY,
   HWC_DEBUG_POLICY
}ComposeMode;

typedef struct RequestContext{
  int iSkipCnt=0;

  // Afbcd info
  int iAfbcdCnt=0;
  int iAfbcdScaleCnt=0;
  int iAfbcdYuvCnt=0;
  int iAfcbdLargeYuvCnt=0;
  int iAfbcdRotateCnt=0;
  int iAfbcdHdrCnt=0;

  // No Afbcd info
  int iCnt=0;
  int iScaleCnt=0;
  int iYuvCnt=0;
  int iLargeYuvCnt=0;
  int iRotateCnt=0;
  int iHdrCnt=0;
} ReqCtx;

typedef struct SupportContext{
  // Afbcd info
  int iAfbcdCnt=0;
  int iAfbcdScaleCnt=0;
  int iAfbcdYuvCnt=0;
  int iAfbcdRotateCnt=0;
  int iAfbcdHdrCnt=0;

  // No Afbcd info
  int iCnt=0;
  int iScaleCnt=0;
  int iYuvCnt=0;
  int iRotateCnt=0;
  int iHdrCnt=0;

  // Reserved DrmPlane
  char arrayReservedPlaneName[PROPERTY_VALUE_MAX] = {0};
} SupCtx;

typedef struct StateContext{
  // Commit mirror function
  bool bCommitMirrorMode=false;
  DrmCrtc *pCrtcMirror=NULL;

  // Multi area
  bool bMultiAreaEnable=false;
  bool bMultiAreaScaleEnable=false;
  bool bMultiAreaMode=false;

  // Video state
  bool bLargeVideo=false;
  bool bDisableFBAfbcd=false;

  // Soc id
  int iSocId=0;
  std::set<ComposeMode> setHwcPolicy;
} StaCtx;

typedef struct DrmVop2Context{
  ReqCtx request;
  SupCtx support;
  StaCtx state;
} Vop2Ctx;

 public:
  Vop3399(){ Init(); }
  void Init();
  bool SupportPlatform(uint32_t soc_id);
  int TryHwcPolicy(std::vector<DrmCompositionPlane> *composition,
                   std::vector<DrmHwcLayer*> &layers,
                   std::vector<PlaneGroup *> &plane_groups,
                   DrmCrtc *crtc,
                   bool gles_policy);
  // Try to assign DrmPlane to display
  int TryAssignPlane(DrmDevice* drm, const std::map<int,int> map_dpys);

 protected:
  int TryOverlayPolicy(std::vector<DrmCompositionPlane> *composition,
                        std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
                        std::vector<PlaneGroup *> &plane_groups);
  int TryMixSkipPolicy(std::vector<DrmCompositionPlane> *composition,
                        std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
                        std::vector<PlaneGroup *> &plane_groups);
  int TryMixVideoPolicy(std::vector<DrmCompositionPlane> *composition,
                        std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
                        std::vector<PlaneGroup *> &plane_groups);
  int TryMixUpPolicy(std::vector<DrmCompositionPlane> *composition,
                        std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
                        std::vector<PlaneGroup *> &plane_groups);
  int TryMixDownPolicy(std::vector<DrmCompositionPlane> *composition,
                        std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
                        std::vector<PlaneGroup *> &plane_groups);
  int TryMixPolicy(std::vector<DrmCompositionPlane> *composition,
                        std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
                        std::vector<PlaneGroup *> &plane_groups);
  int TryGLESPolicy(std::vector<DrmCompositionPlane> *composition,
                        std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
                        std::vector<PlaneGroup *> &plane_groups);
  int MatchPlanes(std::vector<DrmCompositionPlane> *composition,
                      std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
                      std::vector<PlaneGroup *> &plane_groups);
  int MatchBestPlanes(std::vector<DrmCompositionPlane> *composition,
                      std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
                      std::vector<PlaneGroup *> &plane_groups);
  bool TryOverlay();
  void TryMix();
  void InitCrtcMirror(std::vector<DrmHwcLayer*> &layers,std::vector<PlaneGroup *> &plane_groups,DrmCrtc *crtc);
  void UpdateResevedPlane(DrmCrtc *crtc);
  bool CheckGLESLayer(DrmHwcLayer* layers);
  void InitStateContext(
      std::vector<DrmHwcLayer*> &layers,
      std::vector<PlaneGroup *> &plane_groups,
      DrmCrtc *crtc);
  void InitRequestContext(std::vector<DrmHwcLayer*> &layers);
  void InitSupportContext(
      std::vector<PlaneGroup *> &plane_groups,
      DrmCrtc *crtc);
  int InitContext(std::vector<DrmHwcLayer*> &layers,
      std::vector<PlaneGroup *> &plane_groups,
      DrmCrtc *crtc,
      bool gles_policy);

  bool HasLayer(std::vector<DrmHwcLayer*>& layer_vector,DrmHwcLayer *layer);
  int  IsXIntersect(hwc_rect_t* rec,hwc_rect_t* rec2);
  bool IsRec1IntersectRec2(hwc_rect_t* rec1, hwc_rect_t* rec2);
  bool IsLayerCombine(DrmHwcLayer *layer_one,DrmHwcLayer *layer_two);
  bool HasGetNoAfbcUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups);
  bool HasGetNoYuvUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups);
  bool HasGetNoScaleUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups);
  bool HasGetNoAlphaUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups);
  bool HasGetNoEotfUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups);
  bool GetCrtcSupported(const DrmCrtc &crtc, uint32_t possible_crtc_mask);
  bool HasPlanesWithSize(DrmCrtc *crtc, int layer_size, std::vector<PlaneGroup *> &plane_groups);
  int  CombineLayer(LayerMap& layer_map,std::vector<DrmHwcLayer*>& layers,uint32_t iPlaneSize);
  int  GetPlaneGroups(DrmCrtc *crtc, std::vector<PlaneGroup *>&out_plane_groups);
  void ResetLayerFromTmpExceptFB(std::vector<DrmHwcLayer*>& layers, std::vector<DrmHwcLayer*>& tmp_layers);
  void ResetLayerFromTmp(std::vector<DrmHwcLayer*>& layers, std::vector<DrmHwcLayer*>& tmp_layers);
  void MoveFbToTmp(std::vector<DrmHwcLayer*>& layers,std::vector<DrmHwcLayer*>& tmp_layers);
  void OutputMatchLayer(int iFirst, int iLast,
                        std::vector<DrmHwcLayer *>& out_layers,
                        std::vector<DrmHwcLayer *>& tmp_layers);
  void ResetPlaneGroups(std::vector<PlaneGroup *> &plane_groups);
  void ResetLayer(std::vector<DrmHwcLayer*>& layers);
  int  MatchPlane(std::vector<DrmCompositionPlane> *composition_planes,
                     std::vector<PlaneGroup *> &plane_groups,
                     DrmCompositionPlane::Type type, DrmCrtc *crtc,
                     std::pair<int, std::vector<DrmHwcLayer*>> layers, int zpos, bool match_best);
  int  MatchPlaneMirror(std::vector<DrmCompositionPlane> *composition_planes,
                     std::vector<PlaneGroup *> &plane_groups,
                     DrmCompositionPlane::Type type, DrmCrtc *crtc,
                     std::pair<int, std::vector<DrmHwcLayer*>> layers, int zpos, bool match_best);
 private:
  Vop2Ctx ctx;
};

}  // namespace android
#endif

