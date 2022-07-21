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
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "drm-vop-3588"

#include "rockchip/platform/drmvop3588.h"
#include "drmdevice.h"

#include <log/log.h>

namespace android {

void Vop3588::Init(){

  ctx.state.bMultiAreaEnable = hwc_get_bool_property("vendor.hwc.multi_area_enable","true");

  ctx.state.bMultiAreaScaleEnable = hwc_get_bool_property("vendor.hwc.multi_area_scale_mode","true");
}

bool Vop3588::SupportPlatform(uint32_t soc_id){
  switch(soc_id){
    case 0x3588:
      return true;
    default:
      break;
  }
  return false;
}

int Vop3588::TryHwcPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers,
    std::vector<PlaneGroup *> &plane_groups,
    DrmCrtc *crtc,
    bool gles_policy) {
  int ret;
  // Get PlaneGroup
  if(plane_groups.size()==0){
    ALOGE("%s,line=%d can't get plane_groups size=%zu",__FUNCTION__,__LINE__,plane_groups.size());
    return -1;
  }

  // Init context
  InitContext(layers,plane_groups,crtc,gles_policy);

#ifdef USE_LIBSVEP
  // Try to match rga policy
  if(ctx.state.setHwcPolicy.count(HWC_SVEP_OVERLAY_LOPICY)){
    ret = TrySvepPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else{
      ALOGD_IF(LogLevel(DBG_DEBUG),"Match rga policy fail, try to match other policy.");
    }
  }
#endif

  // Try to match overlay policy
  if(ctx.state.setHwcPolicy.count(HWC_OVERLAY_LOPICY)){
    ret = TryOverlayPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else{
      ALOGD_IF(LogLevel(DBG_DEBUG),"Match overlay policy fail, try to match other policy.");
      TryMix();
    }
  }


  // Try to match mix policy
  if(ctx.state.setHwcPolicy.count(HWC_MIX_LOPICY)){
    ret = TryMixPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else{
      ALOGD_IF(LogLevel(DBG_DEBUG),"Match mix policy fail, try to match other policy.");
      ctx.state.setHwcPolicy.insert(HWC_GLES_POLICY);
    }
  }

  // Try to match GLES policy
  if(ctx.state.setHwcPolicy.count(HWC_GLES_POLICY)){
    ret = TryGLESPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }

  ALOGE("%s,%d Can't match HWC policy",__FUNCTION__,__LINE__);
  return -1;
}

bool Vop3588::HasLayer(std::vector<DrmHwcLayer*>& layer_vector,DrmHwcLayer *layer){
        for (std::vector<DrmHwcLayer*>::const_iterator iter = layer_vector.begin();
               iter != layer_vector.end(); ++iter) {
            if((*iter)->uId_==layer->uId_)
                return true;
          }

          return false;
}

int Vop3588::IsXIntersect(hwc_rect_t* rec,hwc_rect_t* rec2){
    if(rec2->top == rec->top)
        return 1;
    else if(rec2->top < rec->top)
    {
        if(rec2->bottom > rec->top)
            return 1;
        else
            return 0;
    }
    else
    {
        if(rec->bottom > rec2->top  )
            return 1;
        else
            return 0;
    }
    return 0;
}


bool Vop3588::IsRec1IntersectRec2(hwc_rect_t* rec1, hwc_rect_t* rec2){
    int iMaxLeft,iMaxTop,iMinRight,iMinBottom;
    ALOGD_IF(LogLevel(DBG_DEBUG),"is_not_intersect: rec1[%d,%d,%d,%d],rec2[%d,%d,%d,%d]",rec1->left,rec1->top,
        rec1->right,rec1->bottom,rec2->left,rec2->top,rec2->right,rec2->bottom);

    iMaxLeft = rec1->left > rec2->left ? rec1->left: rec2->left;
    iMaxTop = rec1->top > rec2->top ? rec1->top: rec2->top;
    iMinRight = rec1->right <= rec2->right ? rec1->right: rec2->right;
    iMinBottom = rec1->bottom <= rec2->bottom ? rec1->bottom: rec2->bottom;

    if(iMaxLeft > iMinRight || iMaxTop > iMinBottom)
        return false;
    else
        return true;

    return false;
}

bool Vop3588::IsLayerCombine(DrmHwcLayer * layer_one,DrmHwcLayer * layer_two){
    if(!ctx.state.bMultiAreaEnable)
      return false;

    // 8K display mode must to disable MultilArea Mode.
    if(ctx.state.b8kMode_)
      return false;

    //multi region only support RGBA888 RGBX8888 RGB888 565 BGRA888 NV12
    if(layer_one->iFormat_ >= HAL_PIXEL_FORMAT_YCrCb_NV12_10
        || layer_two->iFormat_ >= HAL_PIXEL_FORMAT_YCrCb_NV12_10
        || (layer_one->iFormat_ != layer_two->iFormat_)
        || (layer_one->bAfbcd_ != layer_two->bAfbcd_)
        || layer_one->alpha!= layer_two->alpha
        || ((layer_one->bScale_ || layer_two->bScale_) && !ctx.state.bMultiAreaScaleEnable)
        || IsRec1IntersectRec2(&layer_one->display_frame,&layer_two->display_frame)
        || IsXIntersect(&layer_one->display_frame,&layer_two->display_frame)
        )
    {
        ALOGD_IF(LogLevel(DBG_DEBUG),"is_layer_combine layer one alpha=%d,is_scale=%d",layer_one->alpha,layer_one->bScale_);
        ALOGD_IF(LogLevel(DBG_DEBUG),"is_layer_combine layer two alpha=%d,is_scale=%d",layer_two->alpha,layer_two->bScale_);
        return false;
    }

    return true;
}

int Vop3588::CombineLayer(LayerMap& layer_map,std::vector<DrmHwcLayer*> &layers,uint32_t iPlaneSize){

    /*Group layer*/
    int zpos = 0;
    size_t i,j;
    uint32_t sort_cnt=0;
    bool is_combine = false;

    layer_map.clear();

    for (i = 0; i < layers.size(); ) {
        if(!layers[i]->bUse_)
            continue;

        sort_cnt=0;
        if(i == 0)
        {
            layer_map[zpos].push_back(layers[0]);
        }

        for(j = i+1; j < layers.size(); j++) {
            DrmHwcLayer *layer_one = layers[j];
            //layer_one.index = j;
            is_combine = false;

            for(size_t k = 0; k <= sort_cnt; k++ ) {
                DrmHwcLayer *layer_two = layers[j-1-k];
                //layer_two.index = j-1-k;
                //juage the layer is contained in layer_vector
                bool bHasLayerOne = HasLayer(layer_map[zpos],layer_one);
                bool bHasLayerTwo = HasLayer(layer_map[zpos],layer_two);

                //If it contain both of layers,then don't need to go down.
                if(bHasLayerOne && bHasLayerTwo)
                    continue;

                if(IsLayerCombine(layer_one,layer_two)) {
                    //append layer into layer_vector of layer_map_.
                    if(!bHasLayerOne && !bHasLayerTwo)
                    {
                        layer_map[zpos].emplace_back(layer_one);
                        layer_map[zpos].emplace_back(layer_two);
                        is_combine = true;
                    }
                    else if(!bHasLayerTwo)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->uId_==layer_one->uId_)
                                    continue;

                            if(!IsLayerCombine(*iter,layer_two))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                            layer_map[zpos].emplace_back(layer_two);
                    }
                    else if(!bHasLayerOne)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->uId_==layer_two->uId_)
                                    continue;

                            if(!IsLayerCombine(*iter,layer_one))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                            layer_map[zpos].emplace_back(layer_one);
                    }
                }

                if(!is_combine)
                {
                    //if it cann't combine two layer,it need start a new group.
                    if(!bHasLayerOne)
                    {
                        zpos++;
                        layer_map[zpos].emplace_back(layer_one);
                    }
                    is_combine = false;
                    break;
                }
             }
             sort_cnt++; //update sort layer count
             if(!is_combine)
             {
                break;
             }
        }

        if(is_combine)  //all remain layer or limit MOST_WIN_ZONES layer is combine well,it need start a new group.
            zpos++;
        if(sort_cnt)
            i+=sort_cnt;    //jump the sort compare layers.
        else
            i++;
    }

  // RK3588 sort layer by ypos
  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        if(iter->second.size() > 1) {
            for(uint32_t i=0;i < iter->second.size()-1;i++) {
                for(uint32_t j=i+1;j < iter->second.size();j++) {
                     if(iter->second[i]->display_frame.top > iter->second[j]->display_frame.top) {
                        ALOGD_IF(LogLevel(DBG_DEBUG),"swap %d and %d",iter->second[i]->uId_,iter->second[j]->uId_);
                        std::swap(iter->second[i],iter->second[j]);
                     }
                 }
            }
        }
  }


  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        ALOGD_IF(LogLevel(DBG_DEBUG),"layer map id=%d,size=%zu",iter->first,iter->second.size());
        for(std::vector<DrmHwcLayer*>::const_iterator iter_layer = iter->second.begin();
            iter_layer != iter->second.end();++iter_layer)
        {
             ALOGD_IF(LogLevel(DBG_DEBUG),"\tlayer id=%u , name=%s",(*iter_layer)->uId_,(*iter_layer)->sLayerName_.c_str());
        }
  }

    if((int)layer_map.size() > iPlaneSize)
    {
        ALOGD_IF(LogLevel(DBG_DEBUG),"map size=%zu should not bigger than plane size=%d", layer_map.size(), iPlaneSize);
        return -1;
    }

    return 0;

}

bool Vop3588::HasGetNoAfbcUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_afbc(); }
                       );
  }
  return usable_planes.size() > 0;;
}

bool Vop3588::HasGetNoYuvUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_yuv(); }
                       );
  }
  return usable_planes.size() > 0;;
}

bool Vop3588::HasGetNoScaleUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_scale(); }
                       );
  }
  return usable_planes.size() > 0;;
}

bool Vop3588::HasGetNoAlphaUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->alpha_property().id(); }
                       );
  }
  return usable_planes.size() > 0;
}

bool Vop3588::HasGetNoEotfUsablePlanes(DrmCrtc *crtc, std::vector<PlaneGroup *> &plane_groups) {
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_hdr2sdr(); }
                       );
  }
  return usable_planes.size() > 0;
}

bool Vop3588::GetCrtcSupported(const DrmCrtc &crtc, uint32_t possible_crtc_mask) {
  return !!((1 << crtc.pipe()) & possible_crtc_mask);
}

bool Vop3588::HasPlanesWithSize(DrmCrtc *crtc, int layer_size, std::vector<PlaneGroup *> &plane_groups) {
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(GetCrtcSupported(*crtc, (*iter)->possible_crtcs) && !(*iter)->bUse &&
                (*iter)->planes.size() == (size_t)layer_size)
                return true;
  }
  return false;
}

int Vop3588::MatchPlane(std::vector<DrmCompositionPlane> *composition_planes,
                   std::vector<PlaneGroup *> &plane_groups,
                   DrmCompositionPlane::Type type, DrmCrtc *crtc,
                   std::pair<int, std::vector<DrmHwcLayer*>> layers, int zpos, bool match_best=false) {

  uint32_t layer_size = layers.second.size();
  bool b_yuv=false,b_scale=false,b_alpha=false,b_hdr2sdr=false,b_afbc=false;
  std::vector<PlaneGroup *> ::const_iterator iter;
  uint64_t rotation = 0;
  uint64_t alpha = 0xFF;
  uint16_t eotf = TRADITIONAL_GAMMA_SDR;
  bool bMulArea = layer_size > 0 ? true : false;
  bool b8kMode = ctx.state.b8kMode_;

  //loop plane groups.
  for (iter = plane_groups.begin();
     iter != plane_groups.end(); ++iter) {
     uint32_t combine_layer_count = 0;
     ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,last zpos=%d,group(%" PRIu64 ") zpos=%d,group bUse=%d,crtc=0x%x,"
                                   "current_crtc=0x%x,possible_crtcs=0x%x",
                                   __LINE__, zpos, (*iter)->share_id, (*iter)->zpos, (*iter)->bUse,
                                   (1<<crtc->pipe()), (*iter)->current_crtc_,(*iter)->possible_crtcs);
      //find the match zpos plane group
      if(!(*iter)->bUse && !(*iter)->bReserved && (((1<<crtc->pipe()) & (*iter)->current_crtc_) > 0))
      {
          ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,layer_size=%d,planes size=%zu",__LINE__,layer_size,(*iter)->planes.size());

          //find the match combine layer count with plane size.
          if(layer_size <= (*iter)->planes.size())
          {
              //loop layer
              for(std::vector<DrmHwcLayer*>::const_iterator iter_layer= layers.second.begin();
                  iter_layer != layers.second.end();++iter_layer)
              {
                  //reset is_match to false
                  (*iter_layer)->bMatch_ = false;

                  if(match_best || (*iter_layer)->iBestPlaneType > 0){
                      if(!((*iter)->win_type & (*iter_layer)->iBestPlaneType)){
                          ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d, plane_group win-type = 0x%" PRIx64 " , layer best-type = %x, not match ",
                          __LINE__,(*iter)->win_type, (*iter_layer)->iBestPlaneType);
                          continue;
                      }
                  }

                  //loop plane
                  for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
                      !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane)
                  {
                      ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,crtc=0x%x,%s is_use=%d,possible_crtc_mask=0x%x",__LINE__,(1<<crtc->pipe()),
                              (*iter_plane)->name(),(*iter_plane)->is_use(),(*iter_plane)->get_possible_crtc_mask());

                      if(!(*iter_plane)->is_use() && (*iter_plane)->GetCrtcSupported(*crtc))
                      {
                          bool bNeed = false;

                          // Cluster 0, 初始化 Cluster 图层匹配参数
                          if((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER0_WIN0){
                                ctx.state.bClu0Used = false;
                                ctx.state.iClu0UsedZ = -1;
                                ctx.state.bClu0TwoWinMode = true;
                                ctx.state.iClu0UsedDstXOffset = 0;
                          }
                          // Cluster 1, 初始化 Cluster 图层匹配参数
                          if((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER1_WIN0){
                                ctx.state.bClu1Used = false;
                                ctx.state.iClu1UsedZ = -1;
                                ctx.state.bClu1TwoWinMode = true;
                                ctx.state.iClu1UsedDstXOffset = 0;
                          }

                          // Cluster 2, 初始化 Cluster 图层匹配参数
                          if((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER2_WIN0){
                                ctx.state.bClu2Used = false;
                                ctx.state.iClu2UsedZ = -1;
                                ctx.state.bClu2TwoWinMode = true;
                                ctx.state.iClu2UsedDstXOffset = 0;
                          }
                          // Cluster 3, 初始化 Cluster 图层匹配参数
                          if((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER3_WIN0){
                                ctx.state.bClu3Used = false;
                                ctx.state.iClu3UsedZ = -1;
                                ctx.state.bClu3TwoWinMode = true;
                                ctx.state.iClu3UsedDstXOffset = 0;
                          }
                          // 保证Cluster two-win-mode zpos 连续，否则关闭two-win模式
                          if(ctx.state.bClu0Used && ((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER0_WIN1) > 0){
                            if(((zpos - ctx.state.iClu0UsedZ) != 1 && !(zpos == ctx.state.iClu0UsedZ)) ||
                               (ctx.state.iClu0UsedFormat != (*iter_layer)->uFourccFormat_) ||
                               (ctx.state.iClu0UsedAfbc != (*iter_layer)->bAfbcd_)){
                            ctx.state.bClu0TwoWinMode = false;
                            }
                          }

                          // 保证Cluster two-win-mode zpos 连续，否则关闭two-win模式
                          if(ctx.state.bClu1Used && ((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER1_WIN1) > 0){
                            if(((zpos - ctx.state.iClu1UsedZ) != 1 && !(zpos == ctx.state.iClu1UsedZ)) ||
                               (ctx.state.iClu1UsedFormat != (*iter_layer)->uFourccFormat_) ||
                               (ctx.state.iClu1UsedAfbc != (*iter_layer)->bAfbcd_)){
                            ctx.state.bClu1TwoWinMode = false;
                            }
                          }

                          // 保证Cluster two-win-mode zpos 连续，否则关闭two-win模式
                          if(ctx.state.bClu2Used && ((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER2_WIN1) > 0){
                            if(((zpos - ctx.state.iClu2UsedZ) != 1 && !(zpos == ctx.state.iClu2UsedZ)) ||
                               (ctx.state.iClu2UsedFormat != (*iter_layer)->uFourccFormat_) ||
                               (ctx.state.iClu2UsedAfbc != (*iter_layer)->bAfbcd_)){
                            ctx.state.bClu2TwoWinMode = false;
                            }
                          }

                          // 保证Cluster two-win-mode zpos 连续，否则关闭two-win模式
                          if(ctx.state.bClu3Used && ((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER3_WIN1) > 0){
                            if(((zpos - ctx.state.iClu3UsedZ) != 1 && !(zpos == ctx.state.iClu3UsedZ)) ||
                               (ctx.state.iClu3UsedFormat != (*iter_layer)->uFourccFormat_) ||
                               (ctx.state.iClu3UsedAfbc != (*iter_layer)->bAfbcd_)){
                            ctx.state.bClu3TwoWinMode = false;
                            }
                          }

                          // 其他的Cluster限制条件
                          if(((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER0_WIN1) > 0){
                            if(!ctx.state.bClu0TwoWinMode){
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s disable Cluster two win mode",(*iter_plane)->name());
                              continue;
                            }
                            // Two-Win 模式offset需要奇偶对齐
                            int dst_x_offset = (*iter_layer)->display_frame.left;
                            if((ctx.state.iClu0UsedDstXOffset % 2) !=  (dst_x_offset % 2)){
                              ctx.state.bClu0TwoWinMode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s can't overlay win0-dst-x=%d,win1-dst-x=%d",(*iter_plane)->name(),ctx.state.iClu0UsedDstXOffset,dst_x_offset);
                              continue;
                            }
                            // 输入输出分辨率要求小于2048
                            int src_w = (*iter_layer)->source_crop.right - (*iter_layer)->source_crop.left;
                            int dst_w = (*iter_layer)->display_frame.right - (*iter_layer)->display_frame.left;
                            if(src_w > 2048 || dst_w > 2048){
                              ctx.state.bClu0TwoWinMode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s can't overlay src_w=%d, dst_w=%d",(*iter_plane)->name(),src_w,dst_w);
                              continue;
                            }
                          }
                          // 其他的Cluster限制条件
                          if(((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER1_WIN1) > 0){
                            if(!ctx.state.bClu1TwoWinMode){
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s disable Cluster two win mode",(*iter_plane)->name());
                              continue;
                            }
                            // Two-Win 模式offset需要奇偶对齐
                            int dst_x_offset = (*iter_layer)->display_frame.left;
                            if((ctx.state.iClu1UsedDstXOffset % 2) !=  (dst_x_offset % 2)){
                              ctx.state.bClu1TwoWinMode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s can't overlay win0-dst-x=%d,win1-dst-x=%d",(*iter_plane)->name(),ctx.state.iClu1UsedDstXOffset,dst_x_offset);
                              continue;
                            }
                            // 输入输出分辨率要求小于2048
                            int src_w = (*iter_layer)->source_crop.right - (*iter_layer)->source_crop.left;
                            int dst_w = (*iter_layer)->display_frame.right - (*iter_layer)->display_frame.left;
                            if(src_w > 2048 || dst_w > 2048){
                              ctx.state.bClu1TwoWinMode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s can't overlay src_w=%d, dst_w=%d",(*iter_plane)->name(),src_w,dst_w);
                              continue;
                            }
                          }
                          // 其他的Cluster限制条件
                          if(((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER2_WIN1) > 0){
                            if(!ctx.state.bClu2TwoWinMode){
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s disable Cluster two win mode",(*iter_plane)->name());
                              continue;
                            }
                            // Two-Win 模式offset需要奇偶对齐
                            int dst_x_offset = (*iter_layer)->display_frame.left;
                            if((ctx.state.iClu2UsedDstXOffset % 2) !=  (dst_x_offset % 2)){
                              ctx.state.bClu2TwoWinMode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s can't overlay win0-dst-x=%d,win1-dst-x=%d",(*iter_plane)->name(),ctx.state.iClu1UsedDstXOffset,dst_x_offset);
                              continue;
                            }
                            // 输入输出分辨率要求小于2048
                            int src_w = (*iter_layer)->source_crop.right - (*iter_layer)->source_crop.left;
                            int dst_w = (*iter_layer)->display_frame.right - (*iter_layer)->display_frame.left;
                            if(src_w > 2048 || dst_w > 2048){
                              ctx.state.bClu2TwoWinMode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s can't overlay src_w=%d, dst_w=%d",(*iter_plane)->name(),src_w,dst_w);
                              continue;
                            }
                          }
                          // 其他的Cluster限制条件
                          if(((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER3_WIN1) > 0){
                            if(!ctx.state.bClu3TwoWinMode){
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s disable Cluster two win mode",(*iter_plane)->name());
                              continue;
                            }
                            // Two-Win 模式offset需要奇偶对齐
                            int dst_x_offset = (*iter_layer)->display_frame.left;
                            if((ctx.state.iClu3UsedDstXOffset % 2) !=  (dst_x_offset % 2)){
                              ctx.state.bClu3TwoWinMode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s can't overlay win0-dst-x=%d,win1-dst-x=%d",(*iter_plane)->name(),ctx.state.iClu1UsedDstXOffset,dst_x_offset);
                              continue;
                            }
                            // 输入输出分辨率要求小于2048
                            int src_w = (*iter_layer)->source_crop.right - (*iter_layer)->source_crop.left;
                            int dst_w = (*iter_layer)->display_frame.right - (*iter_layer)->display_frame.left;
                            if(src_w > 2048 || dst_w > 2048){
                              ctx.state.bClu3TwoWinMode = false;
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s can't overlay src_w=%d, dst_w=%d",(*iter_plane)->name(),src_w,dst_w);
                              continue;
                            }
                          }

                          // Format
                          if((*iter_plane)->is_support_format((*iter_layer)->uFourccFormat_,(*iter_layer)->bAfbcd_)){
                            bNeed = true;
                          }else{
                            // FB-Target 如果匹配失败，尝试反转AFBC压缩格式再匹配
                            if((*iter_layer)->bFbTarget_ &&
                               (hwc_get_int_property("vendor.gralloc.no_afbc_for_fb_target_layer","0") == 0) &&
                               (*iter_plane)->is_support_format((*iter_layer)->uFourccFormat_,!(*iter_layer)->bAfbcd_)){
                                (*iter_layer)->bAfbcd_ = !(*iter_layer)->bAfbcd_;
                            }else{
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support fourcc=0x%x afbcd = %d",(*iter_plane)->name(),(*iter_layer)->uFourccFormat_,(*iter_layer)->bAfbcd_);
                              continue;
                            }
                          }

                          // Input info
                          int input_w = (int)((*iter_layer)->source_crop.right - (*iter_layer)->source_crop.left);
                          int input_h = (int)((*iter_layer)->source_crop.bottom - (*iter_layer)->source_crop.top);
                          if(b8kMode ? (*iter_plane)->is_support_input_8k(input_w,input_h) : \
                                       (*iter_plane)->is_support_input(input_w,input_h)){
                            bNeed = true;
                          }else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support intput (%d,%d), max_input_range is (%d,%d)",
                                    (*iter_plane)->name(),input_w,input_h,(*iter_plane)->get_input_w_max(),(*iter_plane)->get_input_h_max());
                            continue;

                          }

                          // Output info
                          int output_w = (*iter_layer)->display_frame.right - (*iter_layer)->display_frame.left;
                          int output_h = (*iter_layer)->display_frame.bottom - (*iter_layer)->display_frame.top;

                          if(b8kMode ? (*iter_plane)->is_support_output_8k(output_w,output_h) : \
                                       (*iter_plane)->is_support_output(output_w,output_h)){
                            bNeed = true;
                          }else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support output (%d,%d), max_input_range is (%d,%d)",
                                    (*iter_plane)->name(),output_w,output_h,(*iter_plane)->get_output_w_max(),(*iter_plane)->get_output_h_max());
                            continue;

                          }

                          // Scale
                          // RK3588 源数据宽大于4096时，缩放系数需要做调整：
                          //   Cluster:目前仅支持0.9-1.1的缩放;
                          //   Esmart：可以支持 0.125-8 缩放；
                          bool b8kScaleMode = false;
                          if(b8kMode && (input_w > 4096))
                            b8kScaleMode = true;

                          if((b8kScaleMode ? (*iter_plane)->is_support_scale_8k((*iter_layer)->fHScaleMul_) : \
                                       (*iter_plane)->is_support_scale((*iter_layer)->fHScaleMul_))   &&
                             (b8kScaleMode ? (*iter_plane)->is_support_scale_8k((*iter_layer)->fVScaleMul_) : \
                                       (*iter_plane)->is_support_scale((*iter_layer)->fVScaleMul_)))
                            bNeed = true;
                          else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support scale factor(%f,%f)",
                                    (*iter_plane)->name(), (*iter_layer)->fHScaleMul_, (*iter_layer)->fVScaleMul_);
                            continue;
                          }

                          // Alpha
                          if ((*iter_layer)->blending == DrmHwcBlending::kPreMult)
                              alpha = (*iter_layer)->alpha;
                          b_alpha = (*iter_plane)->alpha_property().id()?true:false;
                          if(alpha != 0xFF)
                          {
                              if(!b_alpha)
                              {
                                  ALOGV("layer id=%d, %s",(*iter_layer)->uId_,(*iter_plane)->name());
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support alpha,layer alpha=0x%x,alpha id=%d",
                                          (*iter_plane)->name(),(*iter_layer)->alpha,(*iter_plane)->alpha_property().id());
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }

                          // HDR
                          bool hdr_layer = (*iter_layer)->bHdr_;
                          b_hdr2sdr = crtc->get_hdr();
                          if(hdr_layer){
                              if(!b_hdr2sdr){
                                  ALOGV("layer id=%d, %s",(*iter_layer)->uId_,(*iter_plane)->name());
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support hdr layer,layer hdr=%d, crtc can_hdr=%d",
                                          (*iter_plane)->name(),hdr_layer,b_hdr2sdr);
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }

                          // Only YUV use Cluster rotate
                          if(b8kMode ? (*iter_plane)->is_support_transform_8k((*iter_layer)->transform) : \
                                       (*iter_plane)->is_support_transform((*iter_layer)->transform)){

                            if(((*iter_plane)->win_type() & PLANE_RK3588_ALL_CLUSTER_MASK) &&
                                !(*iter_layer)->bAfbcd_ && (*iter_layer)->transform != DRM_MODE_ROTATE_0){
                              // Cluster only rotate afbc format
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support noAfbc(%d) layer transform",
                                        (*iter_plane)->name(), (*iter_layer)->bAfbcd_);
                              continue;
                            }
                            if(((*iter_layer)->transform & (DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_270)) != 0){
                              // Cluster rotate must 64 align
                              if(((*iter_layer)->iStride_ % 64 != 0)){
                                ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support layer transform(xmirror or 90 or 270) 0x%x and iStride_ = %d",
                                        (*iter_plane)->name(), (*iter_layer)->transform,(*iter_layer)->iStride_);
                                continue;
                              }
                            }

                            if(((*iter_layer)->transform & (DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_270)) != 0){
                              //Cluster rotate input_h must <= 2048
                              if(input_h > 2048){
                                ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support layer transform(90 or 270) 0x%x and input_h = %d",
                                        (*iter_plane)->name(), (*iter_layer)->transform,input_h);
                                continue;
                              }
                            }
                          }else{
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support layer transform 0x%x, support 0x%x",
                                      (*iter_plane)->name(), (*iter_layer)->transform,(*iter_plane)->get_transform());
                              continue;
                          }

                          ALOGD_IF(LogLevel(DBG_DEBUG),"MatchPlane: match layer id=%d, %s, zops = %d",(*iter_layer)->uId_,
                              (*iter_plane)->name(),zpos);
                          //Find the match plane for layer,it will be commit.
                          composition_planes->emplace_back(type, (*iter_plane), crtc, (*iter_layer)->iDrmZpos_);
                          (*iter_layer)->bMatch_ = true;
                          (*iter_plane)->set_use(true);
                          composition_planes->back().set_zpos(zpos);
                          combine_layer_count++;

                          // Cluster disable two win mode?
                          if((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER0_WIN0){
                              ctx.state.bClu0Used = true;
                              ctx.state.iClu0UsedZ = zpos;
                              ctx.state.iClu0UsedDstXOffset = (*iter_layer)->display_frame.left;
                              ctx.state.iClu0UsedFormat = (*iter_layer)->uFourccFormat_;
                              ctx.state.iClu0UsedAfbc = (*iter_layer)->bAfbcd_;
                              if(input_w > 2048  ||
                                 output_w > 2048 ||
                                 eotf != TRADITIONAL_GAMMA_SDR ||
                                 ((*iter_layer)->transform & (DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_270)) != 0 ||
                                 b8kMode){
                                  ctx.state.bClu0TwoWinMode = false;
                              }else{
                                  ctx.state.bClu0TwoWinMode = true;
                              }
                          }else if((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER1_WIN0){
                              ctx.state.bClu1Used = true;
                              ctx.state.iClu1UsedZ = zpos;
                              ctx.state.iClu1UsedDstXOffset = (*iter_layer)->display_frame.left;
                              ctx.state.iClu1UsedFormat = (*iter_layer)->uFourccFormat_;
                              ctx.state.iClu1UsedAfbc = (*iter_layer)->bAfbcd_;
                              if(input_w > 2048  ||
                                 output_w > 2048 ||
                                 eotf != TRADITIONAL_GAMMA_SDR ||
                                 ((*iter_layer)->transform & (DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_270)) != 0 ||
                                 b8kMode){
                                  ctx.state.bClu1TwoWinMode = false;
                              }else{
                                  ctx.state.bClu1TwoWinMode = true;
                              }
                          }else if((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER2_WIN0){
                              ctx.state.bClu2Used = true;
                              ctx.state.iClu2UsedZ = zpos;
                              ctx.state.iClu2UsedDstXOffset = (*iter_layer)->display_frame.left;
                              ctx.state.iClu2UsedFormat = (*iter_layer)->uFourccFormat_;
                              ctx.state.iClu2UsedAfbc = (*iter_layer)->bAfbcd_;
                              if(input_w > 2048  ||
                                 output_w > 2048 ||
                                 eotf != TRADITIONAL_GAMMA_SDR ||
                                 ((*iter_layer)->transform & (DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_270)) != 0 ||
                                 b8kMode){
                                  ctx.state.bClu2TwoWinMode = false;
                              }else{
                                  ctx.state.bClu2TwoWinMode = true;
                              }
                          }else if((*iter_plane)->win_type() & PLANE_RK3588_CLUSTER3_WIN0){
                              ctx.state.bClu3Used = true;
                              ctx.state.iClu3UsedZ = zpos;
                              ctx.state.iClu3UsedDstXOffset = (*iter_layer)->display_frame.left;
                              ctx.state.iClu3UsedFormat = (*iter_layer)->uFourccFormat_;
                              ctx.state.iClu3UsedAfbc = (*iter_layer)->bAfbcd_;
                              if(input_w > 2048  ||
                                 output_w > 2048 ||
                                 eotf != TRADITIONAL_GAMMA_SDR ||
                                 ((*iter_layer)->transform & (DRM_MODE_ROTATE_90 | DRM_MODE_ROTATE_270)) != 0 ||
                                 b8kMode){
                                  ctx.state.bClu3TwoWinMode = false;
                              }else{
                                  ctx.state.bClu3TwoWinMode = true;
                              }
                          }
                          break;
                      }
                  }
              }
              if(combine_layer_count == layer_size)
              {
                  ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d all match",__LINE__);
                  (*iter)->bUse = true;
                  return 0;
              }
          }
      }

  }
  return -1;
}

void Vop3588::ResetPlaneGroups(std::vector<PlaneGroup *> &plane_groups){
  for (auto &plane_group : plane_groups){
    for(auto &p : plane_group->planes)
      p->set_use(false);
      plane_group->bUse = false;
  }
  return;
}

void Vop3588::ResetLayer(std::vector<DrmHwcLayer*>& layers){
    for (auto &drmHwcLayer : layers){
      drmHwcLayer->bMatch_ = false;
    }
    return;
}

int Vop3588::MatchBestPlanes(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  composition->clear();
  LayerMap layer_map;
  int ret = CombineLayer(layer_map, layers, plane_groups.size());

  // Fill up the remaining planes
  int zpos = 0;
  for (auto i = layer_map.begin(); i != layer_map.end(); i = layer_map.erase(i)) {
    ret = MatchPlane(composition, plane_groups, DrmCompositionPlane::Type::kLayer,
                      crtc, std::make_pair(i->first, i->second),zpos, true);
    // We don't have any planes left
    if (ret == -ENOENT){
      ALOGD_IF(LogLevel(DBG_DEBUG),"Failed to match all layer, try other HWC policy ret = %d,line = %d",ret,__LINE__);
      ResetLayer(layers);
      ResetPlaneGroups(plane_groups);
      return ret;
    }else if (ret) {
      ALOGD_IF(LogLevel(DBG_DEBUG),"Failed to match all layer, try other HWC policy ret = %d, line = %d",ret,__LINE__);
      ResetLayer(layers);
      ResetPlaneGroups(plane_groups);
      return ret;
    }
    zpos++;
  }

  return 0;
}


int Vop3588::MatchPlanes(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  composition->clear();
  LayerMap layer_map;
  int ret = CombineLayer(layer_map, layers, plane_groups.size());

  // Fill up the remaining planes
  int zpos = 0;
  for (auto i = layer_map.begin(); i != layer_map.end(); i = layer_map.erase(i)) {
    ret = MatchPlane(composition, plane_groups, DrmCompositionPlane::Type::kLayer,
                      crtc, std::make_pair(i->first, i->second),zpos);
    if (ret) {
      ALOGD_IF(LogLevel(DBG_DEBUG),"Failed to match all layer, try other HWC policy ret = %d, line = %d",ret,__LINE__);
      ResetLayer(layers);
      ResetPlaneGroups(plane_groups);
      composition->clear();
      return ret;
    }
    zpos++;
  }
  return 0;
}
int  Vop3588::GetPlaneGroups(DrmCrtc *crtc, std::vector<PlaneGroup *>&out_plane_groups){
  DrmDevice *drm = crtc->getDrmDevice();
  out_plane_groups.clear();
  std::vector<PlaneGroup *> all_plane_groups = drm->GetPlaneGroups();
  for(auto &plane_group : all_plane_groups){
    if(plane_group->acquire(1 << crtc->pipe()))
      out_plane_groups.push_back(plane_group);
  }

  return out_plane_groups.size() > 0 ? 0 : -1;
}

void Vop3588::ResetLayerFromTmpExceptFB(std::vector<DrmHwcLayer*>& layers,
                                              std::vector<DrmHwcLayer*>& tmp_layers){
  for (auto i = layers.begin(); i != layers.end();){
      if((*i)->bFbTarget_){
          tmp_layers.emplace_back(std::move(*i));
          i = layers.erase(i);
          continue;
      }
      i++;
  }
  for (auto i = tmp_layers.begin(); i != tmp_layers.end();){
    if((*i)->bFbTarget_){
      i++;
      continue;
    }
    layers.emplace_back(std::move(*i));
    i = tmp_layers.erase(i);
  }
  //sort
  for (auto i = layers.begin(); i != layers.end()-1; i++){
     for (auto j = i+1; j != layers.end(); j++){
        if((*i)->iZpos_ > (*j)->iZpos_){
           std::swap(*i, *j);
        }
     }
  }

  return;
}


void Vop3588::ResetLayerFromTmp(std::vector<DrmHwcLayer*>& layers,
                                              std::vector<DrmHwcLayer*>& tmp_layers){
  for (auto i = tmp_layers.begin(); i != tmp_layers.end();){
         layers.emplace_back(std::move(*i));
         i = tmp_layers.erase(i);
     }
     //sort
     for (auto i = layers.begin(); i != layers.end()-1; i++){
         for (auto j = i+1; j != layers.end(); j++){
             if((*i)->iZpos_ > (*j)->iZpos_){
                 std::swap(*i, *j);
             }
         }
     }

    return;
}

void Vop3588::MoveFbToTmp(std::vector<DrmHwcLayer*>& layers,
                                       std::vector<DrmHwcLayer*>& tmp_layers){
  for (auto i = layers.begin(); i != layers.end();){
      if((*i)->bFbTarget_){
          tmp_layers.emplace_back(std::move(*i));
          i = layers.erase(i);
          continue;
      }
      i++;
  }
  int zpos = 0;
  for(auto &layer : layers){
    layer->iDrmZpos_ = zpos;
    zpos++;
  }

  zpos = 0;
  for(auto &layer : tmp_layers){
    layer->iDrmZpos_ = zpos;
    zpos++;
  }
  return;
}

void Vop3588::OutputMatchLayer(int iFirst, int iLast,
                                          std::vector<DrmHwcLayer *>& layers,
                                          std::vector<DrmHwcLayer *>& tmp_layers){

  if(iFirst < 0 || iLast < 0 || iFirst > iLast)
  {
      HWC2_ALOGD_IF_DEBUG("invalid value iFirst=%d, iLast=%d", iFirst, iLast);
      return;
  }

  int interval = layers.size()-1-iLast;
  ALOGD_IF(LogLevel(DBG_DEBUG), "OutputMatchLayer iFirst=%d,iLast,=%d,interval=%d",iFirst,iLast,interval);
  for (auto i = layers.begin() + iFirst; i != layers.end() - interval;)
  {
      //move gles layers
      tmp_layers.emplace_back(std::move(*i));
      i = layers.erase(i);
  }

  //add fb layer.
  int pos = iFirst;
  for (auto i = tmp_layers.begin(); i != tmp_layers.end();)
  {
      if((*i)->bFbTarget_){
          layers.insert(layers.begin() + pos, std::move(*i));
          pos++;
          i = tmp_layers.erase(i);
          continue;
      }
      i++;
  }
  int zpos = 0;
  for(auto &layer : layers){
    layer->iDrmZpos_ = zpos;
    zpos++;
  }
  return;
}
int Vop3588::TryOverlayPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  std::vector<DrmHwcLayer*> tmp_layers;
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmp(layers,tmp_layers);
    return -1;
  }
  return 0;
}

/*************************mix SidebandStream*************************
   DisplayId=0, Connector 345, Type = HDMI-A-1, Connector state = DRM_MODE_CONNECTED , frame_no = 6611
  ------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------
    id  |  z  |  sf-type  |  hwc-type |       handle       |  transform  |    blnd    |     source crop (l,t,r,b)      |          frame         | dataspace  | name
  ------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------
   0050 | 000 |  Sideband |    Device | 000000000000000000 | None        | None       |    0.0,    0.0,   -1.0,   -1.0 |    0,    0, 1920, 1080 |          0 | allocateBuffer
   0059 | 001 |    Device |    Client | 00b40000751ec3ec30 | None        | Premultipl | 1829.0,   20.0, 1900.0,   59.0 | 1829,   20, 1900,   59 |          0 | com.tencent.start.tv/com.tencent.start.ui.PlayActivity#0
   0071 | 002 |    Device |    Client | 00b40000751ec403d0 | None        | Premultipl |    0.0,    0.0,  412.0, 1080.0 | 1508,    0, 1920, 1080 |          0 | PopupWindow:55de2f2#0
  ------+-----+-----------+-----------+--------------------+-------------+------------+--------------------------------+------------------------+------------+------------
************************************************************/
int Vop3588::TryMixSidebandPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  int iPlaneSize = plane_groups.size();
  std::pair<int, int> layer_indices(-1, -1);

  if((int)layers.size() < 4)
    layer_indices.first = layers.size() - 2 <= 0 ? 1 : layers.size() - 2;
  else
    layer_indices.first = 3;
  layer_indices.second = layers.size() - 1;
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix sideband (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
  OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmpExceptFB(layers,tmp_layers);
    for(--layer_indices.first; layer_indices.first > 0; --layer_indices.first){
      ResetLayerFromTmpExceptFB(layers,tmp_layers);
      ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix sideband (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
      OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
      int ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(!ret)
        return ret;
      else{
        ResetLayerFromTmp(layers,tmp_layers);
     }
   }
 }

  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

int Vop3588::TryMixSkipPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);

  int skipCnt = 0;

  int iPlaneSize = plane_groups.size();

  if(iPlaneSize == 0){
    ALOGE_IF(LogLevel(DBG_DEBUG), "%s:line=%d, iPlaneSize = %d, skip TryMixSkipPolicy",
              __FUNCTION__,__LINE__,iPlaneSize);
  }

  std::vector<DrmHwcLayer *> tmp_layers;
  // Since we can't composite HWC_SKIP_LAYERs by ourselves, we'll let SF
  // handle all layers in between the first and last skip layers. So find the
  // outer indices and mark everything in between as HWC_FRAMEBUFFER
  std::pair<int, int> skip_layer_indices(-1, -1);

  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  //caculate the first and last skip layer
  int i = 0;
  for (auto &layer : layers) {
    if (!layer->bSkipLayer_ && !layer->bGlesCompose_){
      i++;
      continue;
    }

    if (skip_layer_indices.first == -1)
      skip_layer_indices.first = i;
    skip_layer_indices.second = i;
    i++;
  }

  if(skip_layer_indices.first != -1){
    skipCnt = skip_layer_indices.second - skip_layer_indices.first + 1;
  }else{
    ALOGE_IF(LogLevel(DBG_DEBUG), "%s:line=%d, can't find any skip layer, first = %d, second = %d",
              __FUNCTION__,__LINE__,skip_layer_indices.first,skip_layer_indices.second);
    ResetLayerFromTmp(layers,tmp_layers);
    return -1;
  }

  HWC2_ALOGD_IF_DEBUG("mix skip (%d,%d)",skip_layer_indices.first, skip_layer_indices.second);
  OutputMatchLayer(skip_layer_indices.first, skip_layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret){
    return ret;
  }else{
    ResetLayerFromTmpExceptFB(layers,tmp_layers);
    int first = skip_layer_indices.first;
    int last = skip_layer_indices.second;
    // 建议zpos大的图层走GPU合成
    for(last++; last < layers.size(); last++){
      HWC2_ALOGD_IF_DEBUG("mix skip (%d,%d)",skip_layer_indices.first, skip_layer_indices.second);
      OutputMatchLayer(first, last, layers, tmp_layers);
      ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(ret){
        ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,first, last);
        ResetLayerFromTmpExceptFB(layers,tmp_layers);
        continue;
      }else{
        return ret;
      }
    }

    last = layers.size() - 1;
    // 逐步建议知道zpos=0走GPU合成，即全GPU合成
    for(first--; first >= 0; first--){
      HWC2_ALOGD_IF_DEBUG("mix skip (%d,%d)",skip_layer_indices.first, skip_layer_indices.second);
      OutputMatchLayer(first, last, layers, tmp_layers);
      ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(ret){
        ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,first, last);
        ResetLayerFromTmpExceptFB(layers,tmp_layers);
        continue;
      }else{
        return ret;
      }
    }
  }
  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

#ifdef USE_LIBSVEP

bool Vop3588::TrySvepOverlay(){
  ctx.state.setHwcPolicy.insert(HWC_SVEP_OVERLAY_LOPICY);
  return true;
}

int Vop3588::TrySvepPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  std::vector<DrmHwcLayer*> tmp_layers;
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  //save fb into tmp_layers

  // 0. SVEP模块初始化
  if(svep_ == NULL){
    svep_ = Svep::Get(true);
    if(svep_ == NULL){
      HWC2_ALOGD_IF_DEBUG("Svep is NULL, plase check License.");
      return -1;
    }
  }

  bool rga_layer_ready = false;
  bool use_laster_rga_layer = false;
  std::shared_ptr<DrmBuffer> dst_buffer;

  // 以下参数更新后需要强制触发svep处理更新图像数据
  char value[PROPERTY_VALUE_MAX];
  property_get(SVEP_MODE_NAME, value, "0");
  int svep_mode = atoi(value);
  property_get(SVEP_ENHANCEMENT_RATE_NAME, value, "5");
  int enhancement_rate = atoi(value);
  property_get(SVEP_CONTRAST_MODE_NAME, value, "0");
  int contrast_mode = atoi(value);
  property_get(SVEP_CONTRAST_MODE_OFFSET, value, "50");
  int contrast_offset = atoi(value);
  static uint64_t last_buffer_id = 0;
  static int last_svep_mode = 0;
  static int last_enhancement_rate = 0;
  static int last_contrast_mode = 0;
  static int last_contrast_offset = 0;

  for(auto &drmLayer : layers){
    if(drmLayer->iWidth_ <= 4096 &&
       (drmLayer->bYuv_ || strstr(drmLayer->sLayerName_.c_str(), "SurfaceView"))){
        ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
        // 部分参数变化后需要强制更新
        if(last_svep_mode != svep_mode ||
           last_buffer_id != drmLayer->uBufferId_  ||
           last_enhancement_rate != enhancement_rate ||
           last_contrast_mode != contrast_mode ||
           last_contrast_offset != contrast_offset){
          ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
          // 1. Init Ctx
          int ret = svep_->InitCtx(svepCtx_);
          if(ret){
            HWC2_ALOGE("Svep ctx init fail");
            continue;
          }
          // 2. Set buffer Info
          SvepImageInfo src;
          src.mBufferInfo_.iFd_     = drmLayer->iFd_;
          src.mBufferInfo_.iWidth_  = drmLayer->iWidth_;
          src.mBufferInfo_.iHeight_ = drmLayer->iHeight_;
          src.mBufferInfo_.iFormat_ = drmLayer->iFormat_;
          src.mBufferInfo_.iStride_ = drmLayer->iStride_;
          src.mBufferInfo_.iSize_   = drmLayer->iSize_;
          src.mBufferInfo_.uBufferId_ = drmLayer->uBufferId_;
          src.mBufferInfo_.uDataSpace_ = (uint64_t)drmLayer->eDataSpace_;
          if(drmLayer->bAfbcd_){
            if(drmLayer->iFormat_ == HAL_PIXEL_FORMAT_YUV420_8BIT_I){
              src.mBufferInfo_.iFormat_ = HAL_PIXEL_FORMAT_YCrCb_NV12;
            }
            src.mBufferInfo_.uBufferMask_ = SVEP_AFBC_FORMATE;
          }

          src.mCrop_.iLeft_  = (int)drmLayer->source_crop.left;
          src.mCrop_.iTop_   = (int)drmLayer->source_crop.top;
          src.mCrop_.iRight_ = (int)drmLayer->source_crop.right;
          src.mCrop_.iBottom_= (int)drmLayer->source_crop.bottom;

          ret = svep_->SetSrcImage(svepCtx_,
                                   src,
                                   (ctx.state.b8kMode_ ? SVEP_OUTPUT_8K_MODE : SVEP_MODE_NONE));
          if(ret){
            printf("Svep SetSrcImage fail\n");
            continue;
          }

          // 3. Get dst info
          SvepImageInfo require;
          ret = svep_->GetDstRequireInfo(svepCtx_, require);
          if(ret){
            printf("Svep GetDstRequireInfo fail\n");
            continue;
          }

          // 4. Alloc dst_buffer
            dst_buffer = bufferQueue_->DequeueDrmBuffer(require.mBufferInfo_.iWidth_,
                                                            require.mBufferInfo_.iHeight_,
                                                            require.mBufferInfo_.iFormat_,
                                                           "SVEP-SurfaceView");

          if(dst_buffer == NULL){
            HWC2_ALOGD_IF_DEBUG("DequeueDrmBuffer fail!, skip this policy.");
            continue;
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
            continue;
          }

          ret = svep_->SetEnhancementRate(svepCtx_, enhancement_rate);
          if(ret){
            printf("Svep SetEnhancementRate fail\n");
            continue;
          }

          ret = svep_->SetOsdMode(svepCtx_, SVEP_OSD_ENABLE_VIDEO, SVEP_OSD_VIDEO_STR);
          if(ret){
            printf("Svep SetOsdMode fail\n");
            continue;
          }

          hwc_frect_t source_crop;
          source_crop.left   = require.mCrop_.iLeft_;
          source_crop.top    = require.mCrop_.iTop_;
          source_crop.right  = require.mCrop_.iRight_;
          source_crop.bottom = require.mCrop_.iBottom_;
          drmLayer->UpdateAndStoreInfoFromDrmBuffer(dst_buffer->GetHandle(),
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
          rga_layer_ready = true;
          drmLayer->bUseSvep_ = true;
          drmLayer->iBestPlaneType = PLANE_RK3588_ALL_ESMART_MASK;
        }else{
          dst_buffer = bufferQueue_->BackDrmBuffer();

          if(dst_buffer == NULL){
            HWC2_ALOGD_IF_DEBUG("DequeueDrmBuffer fail!, skip this policy.");
            break;
          }

          hwc_frect_t source_crop;
          source_crop.left   = svepCtx_.mDst_.mCrop_.iLeft_;
          source_crop.top    = svepCtx_.mDst_.mCrop_.iTop_;
          source_crop.right  = svepCtx_.mDst_.mCrop_.iRight_;
          source_crop.bottom = svepCtx_.mDst_.mCrop_.iBottom_;
          drmLayer->UpdateAndStoreInfoFromDrmBuffer(dst_buffer->GetHandle(),
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
          use_laster_rga_layer = true;
          drmLayer->bUseSvep_ = true;
          drmLayer->iBestPlaneType = PLANE_RK3588_ALL_ESMART_MASK;
          drmLayer->pSvepBuffer_ = dst_buffer;
          break;
        }
      }
  }
  if(rga_layer_ready){
    ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d rga layer ready, to matchPlanes",__FUNCTION__,__LINE__);
    int ret = 0;
    if(ctx.request.iSkipCnt > 0){
      ret = TryMixSkipPolicy(composition,layers,crtc,plane_groups);
    }else{
      ret = TryOverlayPolicy(composition,layers,crtc,plane_groups);
      if(ret){
        ret = TryMixVideoPolicy(composition,layers,crtc,plane_groups);
      }
    }
    if(!ret){ // Match sucess, to call im2d interface
      for(auto &drmLayer : layers){
        if(drmLayer->bUseSvep_){
          int output_fence = 0;
          ret = svep_->RunAsync(svepCtx_, &output_fence);
          if(ret){
            HWC2_ALOGD_IF_DEBUG("RunAsync fail!");
            drmLayer->bUseSvep_ = false;
          }
          dst_buffer->SetFinishFence(dup(output_fence));
          drmLayer->pSvepBuffer_ = dst_buffer;
          drmLayer->acquire_fence = sp<AcquireFence>(new AcquireFence(output_fence));
          bufferQueue_->QueueBuffer(dst_buffer);
          last_buffer_id = svepCtx_.mSrc_.mBufferInfo_.uBufferId_;
          last_svep_mode = svep_mode;
          last_contrast_mode = contrast_mode;
          last_enhancement_rate = enhancement_rate;
          last_contrast_offset = contrast_offset;
          return ret;
        }
      }
      ResetLayerFromTmp(layers,tmp_layers);
      return ret;
    }else{ // Match fail, skip rga policy
      HWC2_ALOGD_IF_DEBUG(" MatchPlanes fail! reset DrmHwcLayer.");
      for(auto &drmLayer : layers){
        if(drmLayer->bUseSvep_){
          bufferQueue_->QueueBuffer(dst_buffer);
          drmLayer->ResetInfoFromStore();
          drmLayer->bUseSvep_ = false;
        }
      }
      ResetLayerFromTmp(layers,tmp_layers);
      return -1;
    }
  }else if(use_laster_rga_layer){
    ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d rga layer ready, to matchPlanes",__FUNCTION__,__LINE__);
    int ret = -1;
    if(ctx.request.iSkipCnt > 0){
      ret = TryMixSkipPolicy(composition,layers,crtc,plane_groups);
    }else{
      ret = TryOverlayPolicy(composition,layers,crtc,plane_groups);
      if(ret){
        ret = TryMixVideoPolicy(composition,layers,crtc,plane_groups);
      }
    }
    if(!ret){ // Match sucess, to call im2d interface
      HWC2_ALOGD_IF_DEBUG("Use last rga layer.");
      return ret;
    }
  }
  HWC2_ALOGD_IF_DEBUG("fail!, No layer use RGA policy.");
  ResetLayerFromTmp(layers,tmp_layers);
  return -1;
}
#endif
/*************************mix video*************************
 Video ovelay
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
       HWC | 711aa61700 | 0000 | 0000 | 00 | 0100 | ? 00000017  |    0.0,    0.0, 3840.0, 2160.0 |  600,  562, 1160,  982 | SurfaceView - MediaView
      GLES | 711ab1e580 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,  560.0,  420.0 |  600,  562, 1160,  982 | MediaView
      GLES | 70b34c9c80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
      GLES | 70b34c9080 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
      GLES | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 | 1136, 1194, 1175, 1243 | Sprite
************************************************************/
int Vop3588::TryMixVideoPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  int iPlaneSize = plane_groups.size();
  std::pair<int, int> layer_indices(-1, -1);

  if((int)layers.size() < 4)
    layer_indices.first = layers.size() - 2 <= 0 ? 1 : layers.size() - 2;
  else
    layer_indices.first = 3;
  layer_indices.second = layers.size() - 1;
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
  OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmpExceptFB(layers,tmp_layers);
    for(--layer_indices.first; layer_indices.first > 0; --layer_indices.first){
      ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
      OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
      int ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(!ret)
        return ret;
      else{
        ResetLayerFromTmpExceptFB(layers,tmp_layers);
        continue;
      }
    }
  }
  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

/*************************mix up*************************
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
       HWC | 711aa61e80 | 0000 | 0000 | 00 | 0100 | RGBx_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.systemui.ImageWallpaper
       HWC | 711ab1ef00 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.launcher3/com.android.launcher3.Launcher
       HWC | 711aa61700 | 0000 | 0000 | 00 | 0100 | ? 00000017  |    0.0,    0.0, 3840.0, 2160.0 |  600,  562, 1160,  982 | SurfaceView - MediaView
      GLES | 711ab1e580 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,  560.0,  420.0 |  600,  562, 1160,  982 | MediaView
      GLES | 70b34c9c80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
      GLES | 70b34c9080 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
      GLES | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 | 1136, 1194, 1175, 1243 | Sprite
************************************************************/
int Vop3588::TryMixUpPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  int iPlaneSize = plane_groups.size();

  if(ctx.request.iAfbcdCnt == 0){
    for(auto &plane_group : plane_groups){
      if(plane_group->win_type & PLANE_RK3588_ALL_CLUSTER_MASK)
        iPlaneSize--;
    }
  }

  if(iPlaneSize == 0){
    ALOGE_IF(LogLevel(DBG_DEBUG), "%s:line=%d, iPlaneSize = %d, skip TryMixSkipPolicy",
              __FUNCTION__,__LINE__,iPlaneSize);
  }



  std::pair<int, int> layer_indices(-1, -1);

  if((int)layers.size() < 4)
    layer_indices.first = layers.size() - 2 <= 0 ? 1 : layers.size() - 2;
  else
    layer_indices.first = 3;
  layer_indices.second = layers.size() - 1;
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
  OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmpExceptFB(layers,tmp_layers);
    for(--layer_indices.first; layer_indices.first > 0; --layer_indices.first){
      ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
      OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
      int ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(!ret)
        return ret;
      else{
        ResetLayerFromTmpExceptFB(layers,tmp_layers);
        continue;
      }
    }
  }

  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

/*************************mix down*************************
 Sprite layer
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
      GLES | 711aa61e80 | 0000 | 0000 | 00 | 0100 | RGBx_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.systemui.ImageWallpaper
      GLES | 711ab1ef00 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.launcher3/com.android.launcher3.Launcher
      GLES | 711aa61100 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
       HWC | 711ec5ad80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
       HWC | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 |  941,  810,  980,  859 | Sprite
************************************************************/
int Vop3588::TryMixDownPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;

  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  std::pair<int, int> layer_indices(-1, -1);
  int iPlaneSize = plane_groups.size();
  layer_indices.first = 0;
  layer_indices.second = 0;
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix down (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
  OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret){
    return ret;

  }else{
    ResetLayerFromTmpExceptFB(layers,tmp_layers);
    for(int i = 1; i < layers.size(); i++){
      layer_indices.first = 0;
      layer_indices.second = i;
      ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix down (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
      OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
      int ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(!ret)
        return ret;
      else{
        ResetLayerFromTmpExceptFB(layers,tmp_layers);
        continue;
      }
    }
  }
  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

int Vop3588::TryMixPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  int ret;

  if(ctx.state.setHwcPolicy.count(HWC_SIDEBAND_LOPICY)){
    ret = TryMixSidebandPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else
      return ret;
  }

  if(ctx.state.setHwcPolicy.count(HWC_MIX_SKIP_LOPICY)){
    ret = TryMixSkipPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else
      return ret;
  }

  if(ctx.state.setHwcPolicy.count(HWC_MIX_VIDEO_LOPICY)){
    ret = TryMixVideoPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }
  if(ctx.state.setHwcPolicy.count(HWC_MIX_UP_LOPICY)){
    ret = TryMixUpPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;

  }
  if(ctx.state.setHwcPolicy.count(HWC_MIX_DOWN_LOPICY)){
    ret = TryMixDownPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }
  return -1;
}

int Vop3588::TryGLESPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  std::vector<DrmHwcLayer*> fb_target;
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  //save fb into tmp_layers
  MoveFbToTmp(layers, fb_target);

  if(fb_target.size()==1){
    DrmHwcLayer* fb_layer = fb_target[0];
    // If there is a Cluster layer, FB enables AFBC
    if(ctx.support.iAfbcdCnt > 0){
      ctx.state.bDisableFBAfbcd = false;

      // Check FB target property
      ctx.state.bDisableFBAfbcd =
        hwc_get_int_property("vendor.gralloc.no_afbc_for_fb_target_layer","0") > 0;

      // If FB-target unable to meet the scaling requirements, AFBC must be disable.
      if((fb_layer->fHScaleMul_ > 4.0 || fb_layer->fHScaleMul_ < 0.25) ||
         (fb_layer->fVScaleMul_ > 4.0 || fb_layer->fVScaleMul_ < 0.25) ){
        ctx.state.bDisableFBAfbcd = true;
        ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d FB-target over max scale factor,"
              " FB-target must disable AFBC(%d).",
             __FUNCTION__,__LINE__,ctx.state.bDisableFBAfbcd);
      }
      if(ctx.state.bDisableFBAfbcd){
        fb_layer->bAfbcd_ = false;
      }else{
        fb_layer->bAfbcd_ = true;
        ALOGD_IF(LogLevel(DBG_DEBUG),"%s,line=%d Has Cluster Plane, FB enables AFBC",__FUNCTION__,__LINE__);
      }
    }else{
        fb_layer->bAfbcd_ = false;
        ALOGD_IF(LogLevel(DBG_DEBUG),"%s,line=%d Has Cluster Plane, FB enables AFBC",__FUNCTION__,__LINE__);
    }
  }

  int ret = MatchPlanes(composition,fb_target,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmp(layers,fb_target);
    return -1;
  }
  return 0;
}

void Vop3588::UpdateResevedPlane(DrmCrtc *crtc){
  // Reserved DrmPlane
  char reserved_plane_name[PROPERTY_VALUE_MAX] = {0};
  hwc_get_string_property("vendor.hwc.reserved_plane_name","NULL",reserved_plane_name);

  if(strlen(ctx.support.arrayReservedPlaneName) == 0 ||
     strcmp(reserved_plane_name,ctx.support.arrayReservedPlaneName)){
    int reserved_plane_win_type = 0;
    strncpy(ctx.support.arrayReservedPlaneName,reserved_plane_name,strlen(reserved_plane_name)+1);
    DrmDevice *drm = crtc->getDrmDevice();
    std::vector<PlaneGroup *> all_plane_groups = drm->GetPlaneGroups();
    for(auto &plane_group : all_plane_groups){
      for(auto &p : plane_group->planes){
        if(!strcmp(p->name(),ctx.support.arrayReservedPlaneName)){
          plane_group->bReserved = true;
          reserved_plane_win_type = plane_group->win_type;
          ALOGI("%s,line=%d Reserved DrmPlane %s , win_type = 0x%x",
            __FUNCTION__,__LINE__,ctx.support.arrayReservedPlaneName,reserved_plane_win_type);
          break;
        }else{
          plane_group->bReserved = false;
        }
      }
    }
    // RK3566 must reserved a extra DrmPlane.
    if(ctx.state.iSocId == 0x3566 || ctx.state.iSocId == 0x3566a){
      switch(reserved_plane_win_type){
        case DRM_PLANE_TYPE_CLUSTER0_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_CLUSTER1_WIN0;
          break;
        case DRM_PLANE_TYPE_CLUSTER0_WIN1:
          reserved_plane_win_type |= DRM_PLANE_TYPE_CLUSTER0_WIN0;
          break;
        case DRM_PLANE_TYPE_ESMART0_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_ESMART1_WIN0;
          break;
        case DRM_PLANE_TYPE_ESMART1_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_ESMART0_WIN0;
          break;
        case DRM_PLANE_TYPE_SMART0_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_SMART1_WIN0;
          break;
        case DRM_PLANE_TYPE_SMART1_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_SMART0_WIN0;
          break;
        default:
          reserved_plane_win_type = 0;
          break;
      }
      for(auto &plane_group : all_plane_groups){
        if(reserved_plane_win_type & plane_group->win_type){
          plane_group->bReserved = true;
          ALOGI_IF(1 || LogLevel(DBG_DEBUG),"%s,line=%d CommirMirror Reserved win_type = 0x%x",
            __FUNCTION__,__LINE__,reserved_plane_win_type);
          break;
        }else{
          plane_group->bReserved = false;
        }
      }
    }
  }
  return;
}

/*
 * CLUSTER_AFBC_DECODE_MAX_RATE = 3.2
 * (src(W*H)/dst(W*H))/(aclk/dclk) > CLUSTER_AFBC_DECODE_MAX_RATE to use GLES compose.
 * Notes: (4096,1714)=>(1080,603) appear( DDR 1560M ), CLUSTER_AFBC_DECODE_MAX_RATE=2.839350
 * Notes: (4096,1714)=>(1200,900) appear( DDR 1056M ), CLUSTER_AFBC_DECODE_MAX_RATE=2.075307
 */
#define CLUSTER_AFBC_DECODE_MAX_RATE 2.0
bool Vop3588::CheckGLESLayer(DrmHwcLayer *layer){
  // RK356x can't overlay RGBA1010102
  // if(layer->iFormat_ == HAL_PIXEL_FORMAT_RGBA_1010102){
  //   HWC2_ALOGD_IF_DEBUG("[%s]：RGBA1010102 format, not support overlay.",
  //             layer->sLayerName_.c_str());
  //   return true;
  // }


  int act_w = static_cast<int>(layer->source_crop.right - layer->source_crop.left);
  int act_h = static_cast<int>(layer->source_crop.bottom - layer->source_crop.top);
  int dst_w = static_cast<int>(layer->display_frame.right - layer->display_frame.left);
  int dst_h = static_cast<int>(layer->display_frame.bottom - layer->display_frame.top);

  // RK platform VOP can't display src/dst w/h < 4 layer.
  if(act_w < 4 || act_h < 4 || dst_w < 4 || dst_h < 4){
    HWC2_ALOGD_IF_DEBUG("[%s]：[%dx%d] => [%dx%d] too small to use GLES composer.",
              layer->sLayerName_.c_str(),act_w,act_h,dst_w,dst_h);
    return true;
  }

  // RK356x Cluster can't overlay act_w % 4 != 0 afbcd layer.
  if(layer->bAfbcd_){
    // RK3588 不存在以下限制
    // if(act_w % 4 != 0){
    //   HWC2_ALOGD_IF_DEBUG("[%s]：act_w=%d Cluster must act_w %% 4 != 0.",
    //           layer->sLayerName_.c_str(),act_w);
    //   return true;
    // }

    // RK3588 Cluster性能与RK356x差异比较大，这部分限制暂时先关闭。
    //  (src(W*H)/dst(W*H))/(aclk/dclk) > rate = CLUSTER_AFBC_DECODE_MAX_RATE, Use GLES compose
    // if(layer->uAclk_ > 0 && layer->uDclk_ > 0){
    //     char value[PROPERTY_VALUE_MAX];
    //     property_get("vendor.hwc.cluster_afbc_decode_max_rate", value, "0");
    //     double cluster_afbc_decode_max_rate = atof(value);

    //     HWC2_ALOGD_IF_VERBOSE("[%s]：scale-rate=%f, allow_rate = %f, "
    //               "property_rate=%f, fHScaleMul_ = %f, fVScaleMul_ = %f, uAclk_ = %d, uDclk_=%d ",
    //               layer->sLayerName_.c_str(),
    //               (layer->fHScaleMul_ * layer->fVScaleMul_) / (layer->uAclk_/(layer->uDclk_ * 1.0)),
    //               cluster_afbc_decode_max_rate ,CLUSTER_AFBC_DECODE_MAX_RATE,
    //               layer->fHScaleMul_ ,layer->fVScaleMul_ ,layer->uAclk_ ,layer->uDclk_);
    //   if(cluster_afbc_decode_max_rate > 0){
    //     if((layer->fHScaleMul_ * layer->fVScaleMul_) / (layer->uAclk_/(layer->uDclk_ * 1.0)) > cluster_afbc_decode_max_rate){
    //       HWC2_ALOGD_IF_DEBUG("[%s]：scale too large(%f) to use GLES composer, allow_rate = %f, "
    //                 "property_rate=%f, fHScaleMul_ = %f, fVScaleMul_ = %f, uAclk_ = %d, uDclk_=%d ",
    //                 layer->sLayerName_.c_str(),
    //                 (layer->fHScaleMul_ * layer->fVScaleMul_) / (layer->uAclk_/(layer->uDclk_ * 1.0)),
    //                 CLUSTER_AFBC_DECODE_MAX_RATE,
    //                 cluster_afbc_decode_max_rate, layer->fHScaleMul_ ,
    //                 layer->fVScaleMul_ ,layer->uAclk_ ,layer->uDclk_);
    //       return true;
    //     }
    //   }else if((layer->fHScaleMul_ * layer->fVScaleMul_) / (layer->uAclk_/(layer->uDclk_ * 1.0)) > CLUSTER_AFBC_DECODE_MAX_RATE){
    //     HWC2_ALOGD_IF_DEBUG("[%s]：scale too large(%f) to use GLES composer, allow_rate = %f, "
    //               "property_rate=%f, fHScaleMul_ = %f, fVScaleMul_ = %f, uAclk_ = %d, uDclk_=%d ",
    //               layer->sLayerName_.c_str(),
    //               (layer->fHScaleMul_ * layer->fVScaleMul_) / (layer->uAclk_/(layer->uDclk_ * 1.0)),
    //               CLUSTER_AFBC_DECODE_MAX_RATE,
    //               cluster_afbc_decode_max_rate, layer->fHScaleMul_ ,
    //               layer->fVScaleMul_ ,layer->uAclk_ ,layer->uDclk_);
    //     return true;
    //   }
    // }
  }

  // RK356x Esmart can't overlay act_w % 16 == 1 and fHScaleMul_ < 1.0 layer.
  if(!layer->bAfbcd_){
    if(act_w % 16 == 1 && layer->fHScaleMul_ < 1.0){
      HWC2_ALOGD_IF_DEBUG("[%s]：RK356x Esmart can't overlay act_w %% 16 == 1 and fHScaleMul_ < 1.0 layer.",
              layer->sLayerName_.c_str());
      return true;
    }

    int dst_w = static_cast<int>(layer->display_frame.right - layer->display_frame.left);
    if(dst_w % 2 == 1 && layer->fHScaleMul_ < 1.0){
      HWC2_ALOGD_IF_DEBUG("[%s]：RK356x Esmart can't overlay dst_w %% 2 == 1 and fHScaleMul_ < 1.0 layer.",
              layer->sLayerName_.c_str());
      return true;
    }
  }

  if(layer->transform == -1){
    HWC2_ALOGD_IF_DEBUG("[%s]：RK356x Esmart can't overlay dst_w %% 2 == 1 and fHScaleMul_ < 1.0 layer.",
            layer->sLayerName_.c_str());
    return true;
  }

  switch(layer->sf_composition){
    //case HWC2::Composition::Sideband:
    case HWC2::Composition::SolidColor:
      HWC2_ALOGD_IF_DEBUG("[%s]：sf_composition =0x%x not support overlay.",
              layer->sLayerName_.c_str(),layer->sf_composition);
      return true;
    case HWC2::Composition::Client:
      if(layer->bYuv_ && layer->sf_handle != NULL){
        return false;
      }else{
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

void Vop3588::InitRequestContext(std::vector<DrmHwcLayer*> &layers){

  // Collect layer info
  ctx.request.iAfbcdCnt=0;
  ctx.request.iAfbcdScaleCnt=0;
  ctx.request.iAfbcdYuvCnt=0;
  ctx.request.iAfcbdLargeYuvCnt=0;
  ctx.request.iAfbcdRotateCnt=0;
  ctx.request.iAfbcdHdrCnt=0;

  ctx.request.iScaleCnt=0;
  ctx.request.iYuvCnt=0;
  ctx.request.iLargeYuvCnt=0;
  ctx.request.iSkipCnt=0;
  ctx.request.iRotateCnt=0;
  ctx.request.iHdrCnt=0;

  ctx.request.bSidebandStreamMode=false;

  for(auto &layer : layers){
    if(CheckGLESLayer(layer)){
      layer->bGlesCompose_ = true;
    }else{
      layer->bGlesCompose_ = false;
    }

    if(layer->bFbTarget_)
      continue;

    if(layer->bSkipLayer_ || layer->bGlesCompose_){
      ctx.request.iSkipCnt++;
      continue;
    }

    if(layer->bSidebandStreamLayer_)
      ctx.request.bSidebandStreamMode=true;

    if(layer->bAfbcd_){
      ctx.request.iAfbcdCnt++;

      if(layer->bScale_)
        ctx.request.iAfbcdScaleCnt++;

      if(layer->bYuv_){
        ctx.request.iAfbcdYuvCnt++;
        int dst_w = static_cast<int>(layer->display_frame.right - layer->display_frame.left);
        if(layer->iWidth_ > 2048 || layer->bHdr_ || dst_w > 2048){
          ctx.request.iAfcbdLargeYuvCnt++;
        }
      }

      if(layer->transform != DRM_MODE_ROTATE_0)
        ctx.request.iAfbcdRotateCnt++;

      if(layer->bHdr_)
        ctx.request.iAfbcdHdrCnt++;

    }else{

      ctx.request.iCnt++;

      if(layer->bScale_)
        ctx.request.iScaleCnt++;

      if(layer->bYuv_){
        ctx.request.iYuvCnt++;
        if(layer->iWidth_ > 2048){
          ctx.request.iLargeYuvCnt++;
        }
      }

      if(layer->transform != DRM_MODE_ROTATE_0)
        ctx.request.iRotateCnt++;

      if(layer->bHdr_)
        ctx.request.iHdrCnt++;
    }
  }
  return;
}

void Vop3588::InitSupportContext(
    std::vector<PlaneGroup *> &plane_groups,
    DrmCrtc *crtc){
  // Collect Plane resource info
  ctx.support.iAfbcdCnt=0;
  ctx.support.iAfbcdScaleCnt=0;
  ctx.support.iAfbcdYuvCnt=0;
  ctx.support.iAfbcdRotateCnt=0;
  ctx.support.iAfbcdHdrCnt=0;

  ctx.support.iCnt=0;
  ctx.support.iScaleCnt=0;
  ctx.support.iYuvCnt=0;
  ctx.support.iRotateCnt=0;
  ctx.support.iHdrCnt=0;

  // Update DrmPlane
  UpdateResevedPlane(crtc);

  for(auto &plane_group : plane_groups){
    if(plane_group->bReserved)
      continue;
    for(auto &p : plane_group->planes){
      if(p->get_afbc()){

        ctx.support.iAfbcdCnt++;

        if(p->get_scale())
          ctx.support.iAfbcdScaleCnt++;

        if(p->get_yuv())
          ctx.support.iAfbcdYuvCnt++;

        if(p->get_rotate())
          ctx.support.iAfbcdRotateCnt++;

        if(p->get_hdr2sdr())
          ctx.support.iAfbcdHdrCnt++;

      }else{

        ctx.support.iCnt++;

        if(p->get_scale())
          ctx.support.iScaleCnt++;

        if(p->get_yuv())
          ctx.support.iYuvCnt++;

        if(p->get_rotate())
          ctx.support.iRotateCnt++;

        if(p->get_hdr2sdr())
          ctx.support.iHdrCnt++;
      }
      break;
    }
  }
  return;
}

void Vop3588::InitStateContext(
    std::vector<DrmHwcLayer*> &layers,
    std::vector<PlaneGroup *> &plane_groups,
    DrmCrtc *crtc){
  ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d bMultiAreaEnable=%d, bMultiAreaScaleEnable=%d",
            __FUNCTION__,__LINE__,ctx.state.bMultiAreaEnable,ctx.state.bMultiAreaScaleEnable);

  // 8K Mode
  DrmDevice *drm = crtc->getDrmDevice();
  DrmConnector *conn = drm->GetConnectorForDisplay(crtc->display());
  if(conn && conn->state() == DRM_MODE_CONNECTED){
    DrmMode mode = conn->current_mode();
    if(ctx.state.b8kMode_ != mode.is_8k_mode()){
      HWC2_ALOGD_IF_DEBUG("%s 8K Mode.", mode.is_8k_mode() ? "Enter" : "Quit");
    }
    ctx.state.b8kMode_ = mode.is_8k_mode();
    if(ctx.state.b8kMode_){
      // 8K mode Rreserved Cluster-1 and Esmart-1
      for(auto &plane_group : plane_groups){
        for(auto &p : plane_group->planes){
          if(p->win_type() & PLANE_RK3588_ALL_CLUSTER1_MASK ||
              p->win_type() & PLANE_RK3588_ALL_CLUSTER3_MASK||
              p->win_type() & PLANE_RK3588_ALL_ESMART1_MASK ||
              p->win_type() & PLANE_RK3588_ALL_ESMART3_MASK){
            plane_group->bReserved = true;
            HWC2_ALOGD_IF_DEBUG("Reserved 8K plane name=%s", p->name());

          }
        }
      }
      for(auto &layer : layers){
        if(layer->bFbTarget_){
            HWC2_ALOGD_IF_DEBUG("8K Mode, disable Fb-target Afbc");
            layer->bAfbcd_ = 0;
        }
      }
    }else{
      for(auto &plane_group : plane_groups){
        for(auto &p : plane_group->planes){
          plane_group->bReserved = false;
        }
      }
    }
  }

  // FB-target need disable AFBCD?
  ctx.state.bDisableFBAfbcd = false;
  for(auto &layer : layers){
    if(layer->bFbTarget_){
      if(ctx.support.iAfbcdCnt == 0){
        ctx.state.bDisableFBAfbcd = true;
        ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d No Cluster must to overlay Video, FB-target must disable AFBC(%d).",
            __FUNCTION__,__LINE__,ctx.state.bDisableFBAfbcd);
      }

      if(ctx.request.iAfcbdLargeYuvCnt > 0 && ctx.support.iAfbcdYuvCnt <= 2){
        ctx.state.bDisableFBAfbcd = true;
        ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d All Cluster must to overlay Video, FB-target must disable AFBC(%d).",
            __FUNCTION__,__LINE__,ctx.state.bDisableFBAfbcd);
      }

      // If FB-target unable to meet the scaling requirements, AFBC must be disable.
      if((layer->fHScaleMul_ > 4.0 || layer->fHScaleMul_ < 0.25) ||
         (layer->fVScaleMul_ > 4.0 || layer->fVScaleMul_ < 0.25) ){
        ctx.state.bDisableFBAfbcd = true;
        ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d FB-target over max scale factor, FB-target must disable AFBC(%d).",
             __FUNCTION__,__LINE__,ctx.state.bDisableFBAfbcd);
      }

      if(ctx.state.bDisableFBAfbcd){
        layer->bAfbcd_ = 0;
      }
      break;
    }
  }
  return;
}

bool Vop3588::TryOverlay(){
  if(ctx.request.iAfbcdCnt <= ctx.support.iAfbcdCnt &&
     ctx.request.iScaleCnt <= ctx.support.iScaleCnt &&
     ctx.request.iYuvCnt <= ctx.support.iYuvCnt &&
     ctx.request.iRotateCnt <= ctx.support.iRotateCnt &&
     ctx.request.iSkipCnt == 0){
    ctx.state.setHwcPolicy.insert(HWC_OVERLAY_LOPICY);
    return true;
  }
  return false;
}

void Vop3588::TryMix(){
  ctx.state.setHwcPolicy.insert(HWC_MIX_LOPICY);
  ctx.state.setHwcPolicy.insert(HWC_MIX_UP_LOPICY);
  if(ctx.support.iYuvCnt > 0 || ctx.support.iAfbcdYuvCnt > 0)
    ctx.state.setHwcPolicy.insert(HWC_MIX_VIDEO_LOPICY);
  if(ctx.request.iSkipCnt > 0)
    ctx.state.setHwcPolicy.insert(HWC_MIX_SKIP_LOPICY);
  if(ctx.request.bSidebandStreamMode)
    ctx.state.setHwcPolicy.insert(HWC_SIDEBAND_LOPICY);

}

int Vop3588::InitContext(
    std::vector<DrmHwcLayer*> &layers,
    std::vector<PlaneGroup *> &plane_groups,
    DrmCrtc *crtc,
    bool gles_policy){

  ctx.state.setHwcPolicy.clear();
  ctx.state.iSocId = crtc->get_soc_id();

  InitRequestContext(layers);
  InitSupportContext(plane_groups,crtc);
  InitStateContext(layers,plane_groups,crtc);

  //force go into GPU
  int iMode = hwc_get_int_property("vendor.hwc.compose_policy","0");

  if((iMode!=1 || gles_policy) && iMode != 2){
    ctx.state.setHwcPolicy.insert(HWC_GLES_POLICY);
    ALOGD_IF(LogLevel(DBG_DEBUG),"Force use GLES compose, iMode=%d, gles_policy=%d, soc_id=%x",iMode,gles_policy,ctx.state.iSocId);
    return 0;
  }

  ALOGD_IF(LogLevel(DBG_DEBUG),"request:afbcd=%d,scale=%d,yuv=%d,rotate=%d,hdr=%d,skip=%d\n"
          "support:afbcd=%d,scale=%d,yuv=%d,rotate=%d,hdr=%d, %s,line=%d,",
          ctx.request.iAfbcdCnt,ctx.request.iScaleCnt,ctx.request.iYuvCnt,
          ctx.request.iRotateCnt,ctx.request.iHdrCnt,ctx.request.iSkipCnt,
          ctx.support.iAfbcdCnt,ctx.support.iScaleCnt,ctx.support.iYuvCnt,
          ctx.support.iRotateCnt,ctx.support.iHdrCnt,
          __FUNCTION__,__LINE__);

#ifdef USE_LIBSVEP
  int iSvepMode = hwc_get_int_property(SVEP_MODE_NAME,"0");
  // Match policy first
  HWC2_ALOGD_IF_DEBUG("%s=%d bSvepReady_=%d",SVEP_MODE_NAME, iSvepMode, bSvepReady_);
  if(iSvepMode == 1){
    DrmDevice *drm = crtc->getDrmDevice();
    DrmConnector *conn = drm->GetConnectorForDisplay(crtc->display());
    if(conn && conn->state() == DRM_MODE_CONNECTED &&
        conn->type() == DRM_MODE_CONNECTOR_HDMIA && conn->type_id() == 1){
      // Match policy first
      TrySvepOverlay();
    }
  }else if(iSvepMode == 2){
    ctx.state.setHwcPolicy.insert(HWC_GLES_POLICY);
    HWC2_ALOGD_IF_DEBUG("Force use GLES compose, %s=%d , soc_id=%x",
                                                                    SVEP_MODE_NAME,
                                                                    iSvepMode,
                                                                    ctx.state.iSocId);
    return 0;
  }
#endif

  if(!TryOverlay())
    TryMix();

  return 0;
}
}

