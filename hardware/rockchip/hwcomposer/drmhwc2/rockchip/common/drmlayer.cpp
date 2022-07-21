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
#define LOG_TAG "hwc-drm-utils"

#include "drmlayer.h"
#include "platform.h"

#include <drm_fourcc.h>

#include <log/log.h>
#include <ui/GraphicBufferMapper.h>
#include <cutils/properties.h>


#define hwcMIN(x, y)			(((x) <= (y)) ?  (x) :  (y))
#define hwcMAX(x, y)			(((x) >= (y)) ?  (x) :  (y))
#define IS_ALIGN(val,align)    (((val)&(align-1))==0)
#ifndef ALIGN
#define ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))
#endif
#define ALIGN_DOWN( value, base)	(value & (~(base-1)) )

namespace android {

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

int DrmHwcBuffer::ImportBuffer(buffer_handle_t handle, Importer *importer) {
  int ret = importer->ImportBuffer(handle, &bo_);
  if (ret)
    return ret;

  if (importer_ != NULL) {
    importer_->ReleaseBuffer(&bo_);
  }

  importer_ = importer;

  return 0;
}

int DrmHwcBuffer::SetBoInfo(uint32_t fd, uint32_t width,
                            uint32_t height, uint32_t format,
                            uint32_t hal_format, uint64_t modifier,
                            uint32_t usage, uint32_t byte_stride,
                            uint32_t gem_handle){
  bo_.fd = fd;
  bo_.width = width;
  bo_.height = height;
  bo_.usage = usage;
  bo_.hal_format = hal_format;
  bo_.format = format;
  bo_.modifier = modifier;
  bo_.byte_stride = byte_stride;
  bo_.gem_handles[0] = gem_handle;
  return 0;
}

int DrmHwcNativeHandle::CopyBufferHandle(buffer_handle_t handle, int width,
                                         int height, int layerCount, int format,
                                         int usage, int stride) {
  native_handle_t *handle_copy;
  GraphicBufferMapper &gm(GraphicBufferMapper::get());
  int ret;

#ifdef HWC2_USE_OLD_GB_IMPORT
  UNUSED(width);
  UNUSED(height);
  UNUSED(layerCount);
  UNUSED(format);
  UNUSED(usage);
  UNUSED(stride);
  ret = gm.importBuffer(handle, const_cast<buffer_handle_t *>(&handle_copy));
#else
  ret = gm.importBuffer(handle, width, height, layerCount, format, usage,
                        stride, const_cast<buffer_handle_t *>(&handle_copy));
#endif
  if (ret) {
    ALOGE("Failed to import buffer handle %d", ret);
    return ret;
  }

  Clear();

  handle_ = handle_copy;

  return 0;
}

DrmHwcNativeHandle::~DrmHwcNativeHandle() {
  Clear();
}

void DrmHwcNativeHandle::Clear() {
  if (handle_ != NULL) {
    GraphicBufferMapper &gm(GraphicBufferMapper::get());
    int ret = gm.freeBuffer(handle_);
    if (ret) {
      ALOGE("Failed to free buffer handle %d", ret);
    }
    handle_ = NULL;
  }
}

int DrmHwcLayer::ImportBuffer(Importer *importer) {

  buffer.SetBoInfo(iFd_, iWidth_, iHeight_, uFourccFormat_,
                   iFormat_, uModifier_, iUsage, iByteStride_, uGemHandle_);

  int ret = buffer.ImportBuffer(sf_handle, importer);
  if (ret)
    return ret;

  const hwc_drm_bo *bo = buffer.operator->();

  // Fix YUV can't importBuffer bug.
  // layerCount is always 1 and pixel_stride is always 0.
  ret = handle.CopyBufferHandle(sf_handle, bo->width, bo->height, 1/*bo->layer_cnt*/,
                                bo->hal_format, bo->usage, 0/*bo->pixel_stride*/);
  if (ret)
    return ret;

  gralloc_buffer_usage = bo->usage;

  return 0;
}
int DrmHwcLayer::Init() {
  bYuv_ = IsYuvFormat(iFormat_,uFourccFormat_);
  bScale_  = IsScale(source_crop, display_frame, transform);
  iSkipLine_  = GetSkipLine();
  bAfbcd_ = IsAfbcModifier(uModifier_);
  bSkipLayer_ = IsSkipLayer();
  //bGlesCompose_ = IsGlesCompose();

  // HDR
  bHdr_ = IsHdr(iUsage, eDataSpace_);

  uColorSpace = GetColorSpace(eDataSpace_);
  if(bHdr_){
    uColorSpace = V4L2_COLORSPACE_BT2020;
  }
  uEOTF = GetEOTF(eDataSpace_);
  return 0;
}

int DrmHwcLayer::InitFromDrmHwcLayer(DrmHwcLayer *src_layer,
                                     Importer *importer) {
  blending = src_layer->blending;
  sf_handle = src_layer->sf_handle;
  acquire_fence = AcquireFence::NO_FENCE;
  display_frame = src_layer->display_frame;
  alpha = src_layer->alpha;
  source_crop = src_layer->source_crop;
  transform = src_layer->transform;
  return ImportBuffer(importer);
}

void DrmHwcLayer::SetBlend(HWC2::BlendMode blend) {
  switch (blend) {
    case HWC2::BlendMode::None:
      blending = DrmHwcBlending::kNone;
      break;
    case HWC2::BlendMode::Premultiplied:
      blending = DrmHwcBlending::kPreMult;
      break;
    case HWC2::BlendMode::Coverage:
      blending = DrmHwcBlending::kCoverage;
      break;
    default:
      ALOGE("Unknown blending mode b=%d", blend);
      blending = DrmHwcBlending::kNone;
      break;
  }
}

void DrmHwcLayer::SetSourceCrop(hwc_frect_t const &crop) {
  source_crop = crop;
}

#define OVERSCAN_MIN_VALUE              (80)
#define OVERSCAN_MAX_VALUE              (100)
void DrmHwcLayer::SetDisplayFrame(hwc_rect_t const &frame,
                                  hwc2_drm_display_t *ctx) {
  float left_scale   = 1;
  float right_scale  = 1;
  float top_scale    = 1;
  float bottom_scale = 1;
  if(!ctx->bStandardSwitchResolution){
    left_scale   = ctx->rel_xres / (float)ctx->framebuffer_width;
    right_scale  = ctx->rel_xres / (float)ctx->framebuffer_width;
    top_scale    = ctx->rel_yres / (float)ctx->framebuffer_height;
    bottom_scale = ctx->rel_yres / (float)ctx->framebuffer_height;
  }

  display_frame.left   = (int)(frame.left   * left_scale  ) + ctx->rel_xoffset;
  display_frame.right  = (int)(frame.right  * right_scale ) + ctx->rel_xoffset;
  display_frame.top    = (int)(frame.top    * top_scale   ) + ctx->rel_yoffset;
  display_frame.bottom = (int)(frame.bottom * bottom_scale) + ctx->rel_yoffset;

  // RK3588 硬件未提供Overscan功能，所以需要利用Scale实现Overscan效果
  if(isRK3588(ctx->soc_id)){
    int left_margin = 100, right_margin= 100, top_margin = 100, bottom_margin = 100;
    sscanf(ctx->overscan_value, "overscan %d,%d,%d,%d", &left_margin,
                                                        &top_margin,
                                                        &right_margin,
                                                        &bottom_margin);

    HWC2_ALOGD_IF_DEBUG("overscan(%d,%d,%d,%d)",left_margin,
                                                top_margin,
                                                right_margin,
                                                bottom_margin);


    float left_margin_f, right_margin_f, top_margin_f, bottom_margin_f;
    float lscale = 0, tscale = 0, rscale = 0, bscale = 0;
    int disp_old_l,disp_old_t,disp_old_r,disp_old_b;
    int dst_w = (int)(display_frame.right - display_frame.left);
    int dst_h = (int)(display_frame.bottom - display_frame.top);

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

    display_frame.left = ((int)(display_frame.left  * (1.0 - lscale - rscale)) + (int)(ctx->rel_xres * lscale));
    display_frame.top =  ((int)(display_frame.top   * (1.0 - tscale - bscale)) + (int)(ctx->rel_yres * tscale));
    dst_w -= ((int)(dst_w * lscale) + (int)(dst_w * rscale));
    dst_h -= ((int)(dst_h * tscale) + (int)(dst_h * bscale));
    display_frame.right  = display_frame.left + dst_w;
    display_frame.bottom = display_frame.top  + dst_h;
  }
}

void DrmHwcLayer::SetDisplayFrameMirror(hwc_rect_t const &frame) {
  display_frame_mirror = frame;
}

void DrmHwcLayer::SetTransform(HWC2::Transform sf_transform) {
  switch (sf_transform) {
      case HWC2::Transform::None:
        transform = DRM_MODE_ROTATE_0;
        break;
      case HWC2::Transform::FlipH :
        transform = DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X;
        break;
      case HWC2::Transform::FlipV :
        transform = DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_Y;
        break;
      case HWC2::Transform::Rotate90 :
        transform = DRM_MODE_ROTATE_90;
        break;
      case HWC2::Transform::Rotate180 :
        //transform = DRM_MODE_ROTATE_180;
        transform = DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y;
        break;
      case HWC2::Transform::Rotate270 :
        transform = DRM_MODE_ROTATE_270;
        break;
      case HWC2::Transform::FlipHRotate90 :
        transform = DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_90;
        break;
      case HWC2::Transform::FlipVRotate90 :
        transform = DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_90;
        break;
      default:
        transform = -1;
        ALOGE_IF(LogLevel(DBG_DEBUG),"Unknow sf transform 0x%x",sf_transform);
  }
}
bool DrmHwcLayer::IsYuvFormat(int format, uint32_t fource_format){

  switch(fource_format){
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
    case DRM_FORMAT_NV16:
    case DRM_FORMAT_NV61:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV422:
    case DRM_FORMAT_YVU422:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_YVU444:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_YUV420_8BIT:
    case DRM_FORMAT_YUV420_10BIT:
      return true;
    default:
      break;
  }

  switch(format){
    case HAL_PIXEL_FORMAT_YCrCb_NV12:
    case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
    case HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO:
    case HAL_PIXEL_FORMAT_YCbCr_422_SP_10:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP_10:
    case HAL_PIXEL_FORMAT_YCBCR_422_I:
    case HAL_PIXEL_FORMAT_YUV420_8BIT_I:
    case HAL_PIXEL_FORMAT_YUV420_10BIT_I:
    case HAL_PIXEL_FORMAT_Y210:
      return true;
    default:
      return false;
  }
}
bool DrmHwcLayer::IsScale(hwc_frect_t &source_crop, hwc_rect_t &display_frame, int transform){
  int src_w, src_h, dst_w, dst_h;
  src_w = (int)(source_crop.right - source_crop.left);
  src_h = (int)(source_crop.bottom - source_crop.top);
  dst_w = (int)(display_frame.right - display_frame.left);
  dst_h = (int)(display_frame.bottom - display_frame.top);

  if((transform == DrmHwcTransform::kRotate90) || (transform == DrmHwcTransform::kRotate270)){
    if(bYuv_){
        //rga need this alignment.
        src_h = ALIGN_DOWN(src_h, 8);
        src_w = ALIGN_DOWN(src_w, 2);
    }
    fHScaleMul_ = (float) (src_h)/(dst_w);
    fVScaleMul_ = (float) (src_w)/(dst_h);
  } else {
    fHScaleMul_ = (float) (src_w)/(dst_w);
    fVScaleMul_ = (float) (src_h)/(dst_h);
  }
  return (fHScaleMul_ != 1.0 ) || ( fVScaleMul_ != 1.0);
}

bool DrmHwcLayer::IsHdr(int usage, android_dataspace_t dataspace){
  if(((usage & 0x0F000000) == HDR_ST2084_USAGE ||
      (usage & 0x0F000000) == HDR_HLG_USAGE)){
    return true;
  }

  if(((dataspace & HAL_DATASPACE_TRANSFER_ST2084) == HAL_DATASPACE_TRANSFER_ST2084) ||
     ((dataspace & HAL_DATASPACE_TRANSFER_HLG) == HAL_DATASPACE_TRANSFER_HLG)){
    return true;
  }

  return false;
}
bool DrmHwcLayer::IsAfbcModifier(uint64_t modifier){
  if(bFbTarget_){
    return hwc_get_int_property("vendor.gralloc.no_afbc_for_fb_target_layer","0") == 0;
  }else
    return AFBC_FORMAT_MOD_BLOCK_SIZE_16x16 == (modifier & AFBC_FORMAT_MOD_BLOCK_SIZE_16x16);             // for Midgard gralloc r14
}

bool DrmHwcLayer::IsSkipLayer(){
  return (!sf_handle ? true:false);
}

/*
 * CLUSTER_AFBC_DECODE_MAX_RATE = 3.2
 * (src(W*H)/dst(W*H))/(aclk/dclk) > CLUSTER_AFBC_DECODE_MAX_RATE to use GLES compose.
 * Notes: (4096,1714)=>(1080,603) appear( DDR 1560M ), CLUSTER_AFBC_DECODE_MAX_RATE=2.839350
 * Notes: (4096,1714)=>(1200,900) appear( DDR 1056M ), CLUSTER_AFBC_DECODE_MAX_RATE=2.075307
 */
#define CLUSTER_AFBC_DECODE_MAX_RATE 2.0
bool DrmHwcLayer::IsGlesCompose(){
  // RK356x can't overlay RGBA1010102
  if(iFormat_ == HAL_PIXEL_FORMAT_RGBA_1010102)
    return true;


  int act_w = static_cast<int>(source_crop.right - source_crop.left);
  int act_h = static_cast<int>(source_crop.bottom - source_crop.top);
  int dst_w = static_cast<int>(display_frame.right - display_frame.left);
  int dst_h = static_cast<int>(display_frame.bottom - display_frame.top);

  // RK platform VOP can't display src/dst w/h < 4 layer.
  if(act_w < 4 || act_h < 4 || dst_w < 4 || dst_h < 4){
    ALOGD_IF(LogLevel(DBG_DEBUG),"[%s]：[%dx%d] => [%dx%d] too small to use GLES composer.",
              sLayerName_.c_str(),act_w,act_h,dst_w,dst_h);
    return true;
  }

  // RK356x Cluster can't overlay act_w % 4 != 0 afbcd layer.
  if(bAfbcd_){
    if(act_w % 4 != 0)
      return true;
    //  (src(W*H)/dst(W*H))/(aclk/dclk) > rate = CLUSTER_AFBC_DECODE_MAX_RATE, Use GLES compose
    if(uAclk_ > 0 && uDclk_ > 0){
        char value[PROPERTY_VALUE_MAX];
        property_get("vendor.hwc.cluster_afbc_decode_max_rate", value, "0");
        double cluster_afbc_decode_max_rate = atof(value);

        ALOGD_IF(LogLevel(DBG_VERBOSE),"[%s]：scale too large(%f) to use GLES composer, allow_rate = %f, "
                  "property_rate=%f, fHScaleMul_ = %f, fVScaleMul_ = %f, uAclk_ = %d, uDclk_=%d ",
                  sLayerName_.c_str(),(fHScaleMul_ * fVScaleMul_) / (uAclk_/(uDclk_ * 1.0)),
                  cluster_afbc_decode_max_rate ,CLUSTER_AFBC_DECODE_MAX_RATE,
                  fHScaleMul_ ,fVScaleMul_ ,uAclk_ ,uDclk_);
      if(cluster_afbc_decode_max_rate > 0){
        if((fHScaleMul_ * fVScaleMul_) / (uAclk_/(uDclk_ * 1.0)) > cluster_afbc_decode_max_rate){
          ALOGD_IF(LogLevel(DBG_DEBUG),"[%s]：scale too large(%f) to use GLES composer, allow_rate = %f, "
                    "property_rate=%f, fHScaleMul_ = %f, fVScaleMul_ = %f, uAclk_ = %d, uDclk_=%d ",
                    sLayerName_.c_str(),(fHScaleMul_ * fVScaleMul_) / (uAclk_/(uDclk_ * 1.0)), CLUSTER_AFBC_DECODE_MAX_RATE,
                    cluster_afbc_decode_max_rate, fHScaleMul_ ,fVScaleMul_ ,uAclk_ ,uDclk_);
          return true;
        }
      }else if((fHScaleMul_ * fVScaleMul_) / (uAclk_/(uDclk_ * 1.0)) > CLUSTER_AFBC_DECODE_MAX_RATE){
        ALOGD_IF(LogLevel(DBG_DEBUG),"[%s]：scale too large(%f) to use GLES composer, allow_rate = %f, "
                  "property_rate=%f, fHScaleMul_ = %f, fVScaleMul_ = %f, uAclk_ = %d, uDclk_=%d ",
                  sLayerName_.c_str(),(fHScaleMul_ * fVScaleMul_) / (uAclk_/(uDclk_ * 1.0)), CLUSTER_AFBC_DECODE_MAX_RATE,
                  cluster_afbc_decode_max_rate, fHScaleMul_ ,fVScaleMul_ ,uAclk_ ,uDclk_);
        return true;
      }
    }
  }

  // RK356x Esmart can't overlay act_w % 16 == 1 and fHScaleMul_ < 1.0 layer.
  if(!bAfbcd_){
    if(act_w % 16 == 1 && fHScaleMul_ < 1.0)
      return true;

    int dst_w = static_cast<int>(display_frame.right - display_frame.left);
    if(dst_w % 2 == 1 && fHScaleMul_ < 1.0)
      return true;
  }

  if(transform == -1){
    return true;
  }

  switch(sf_composition){
    case HWC2::Composition::Client:
    //case HWC2::Composition::Sideband:
    case HWC2::Composition::SolidColor:
      return true;
    default:
      break;
  }

  return false;
}
int DrmHwcLayer::GetSkipLine(){
    int skip_line = 0;
    if(bYuv_){
      if(iWidth_ >= 3840){
        if(fHScaleMul_ > 1.0 || fVScaleMul_ > 1.0){
            skip_line = 2;
        }
        if(iFormat_ == HAL_PIXEL_FORMAT_YCrCb_NV12_10 && fHScaleMul_ >= (3840 / 1600)){
            skip_line = 3;
        }
      }
      int video_skipline = property_get_int32("vendor.video.skipline", 0);
      if (video_skipline == 2){
        skip_line = 2;
      }else if(video_skipline == 3){
        skip_line = 3;
      }
    }
    return (skip_line >= 0 ? skip_line : 0);
}

#define CONTAIN_VALUE(value,mask) ((dataspace & mask) == value)
v4l2_colorspace DrmHwcLayer::GetColorSpace(android_dataspace_t dataspace){
  if (CONTAIN_VALUE(HAL_DATASPACE_STANDARD_BT2020, HAL_DATASPACE_STANDARD_MASK)){
      return V4L2_COLORSPACE_BT2020;
  }
  else if (CONTAIN_VALUE(HAL_DATASPACE_STANDARD_BT601_625, HAL_DATASPACE_STANDARD_MASK) &&
          CONTAIN_VALUE(HAL_DATASPACE_TRANSFER_SMPTE_170M, HAL_DATASPACE_TRANSFER_MASK)){
      if (CONTAIN_VALUE(HAL_DATASPACE_RANGE_FULL, HAL_DATASPACE_RANGE_MASK))
          return V4L2_COLORSPACE_JPEG;
      else if (CONTAIN_VALUE(HAL_DATASPACE_RANGE_LIMITED, HAL_DATASPACE_RANGE_MASK))
          return V4L2_COLORSPACE_SMPTE170M;
  }
  else if (CONTAIN_VALUE(HAL_DATASPACE_STANDARD_BT601_525, HAL_DATASPACE_STANDARD_MASK) &&
          CONTAIN_VALUE(HAL_DATASPACE_TRANSFER_SMPTE_170M, HAL_DATASPACE_TRANSFER_MASK) &&
          CONTAIN_VALUE(HAL_DATASPACE_RANGE_LIMITED, HAL_DATASPACE_RANGE_MASK)){
      return V4L2_COLORSPACE_SMPTE170M;
  }
  else if (CONTAIN_VALUE(HAL_DATASPACE_STANDARD_BT709, HAL_DATASPACE_STANDARD_MASK) &&
      CONTAIN_VALUE(HAL_DATASPACE_TRANSFER_SMPTE_170M, HAL_DATASPACE_TRANSFER_MASK) &&
      CONTAIN_VALUE(HAL_DATASPACE_RANGE_LIMITED, HAL_DATASPACE_RANGE_MASK)){
      return V4L2_COLORSPACE_REC709;
  }
  else if (CONTAIN_VALUE(HAL_DATASPACE_TRANSFER_SRGB, HAL_DATASPACE_TRANSFER_MASK)){
      return V4L2_COLORSPACE_SRGB;
  }
  //ALOGE("Unknow colorspace 0x%x",colorspace);
  return V4L2_COLORSPACE_DEFAULT;

}

supported_eotf_type DrmHwcLayer::GetEOTF(android_dataspace_t dataspace){
  if(bYuv_){
    if((dataspace & HAL_DATASPACE_TRANSFER_MASK) == HAL_DATASPACE_TRANSFER_ST2084){
        ALOGD_IF(LogLevel(DBG_VERBOSE),"%s:line=%d has st2084",__FUNCTION__,__LINE__);
        return SMPTE_ST2084;
    }else{
        //ALOGE("Unknow etof %d",eotf);
        return TRADITIONAL_GAMMA_SDR;
    }
  }

  return TRADITIONAL_GAMMA_SDR;
}

void DrmHwcLayer::UpdateAndStoreInfoFromDrmBuffer(buffer_handle_t handle,
      int fd, int format, int w, int h, int stride, int byte_stride,
      int size, int usage, uint32_t fourcc, uint64_t modefier,
      std::string name, hwc_frect_t &intput_crop, uint64_t buffer_id,
      uint32_t gemhandle){

  storeLayerInfo_.valid_       = true;
  storeLayerInfo_.sf_handle    = sf_handle;
  storeLayerInfo_.transform    = transform;
  storeLayerInfo_.source_crop  = source_crop;
  storeLayerInfo_.display_frame  = display_frame;
  storeLayerInfo_.iFd_         = iFd_;
  storeLayerInfo_.iFormat_     = iFormat_;
  storeLayerInfo_.iWidth_      = iWidth_;
  storeLayerInfo_.iHeight_     = iHeight_;
  storeLayerInfo_.iStride_     = iStride_;
  storeLayerInfo_.iByteStride_ = iByteStride_;
  storeLayerInfo_.iSize_       = iSize_;
  storeLayerInfo_.iUsage = iUsage;
  storeLayerInfo_.uFourccFormat_ = uFourccFormat_;
  storeLayerInfo_.uModifier_     = uModifier_;
  storeLayerInfo_.sLayerName_    = sLayerName_;
  storeLayerInfo_.uBufferId_    = uBufferId_;
  storeLayerInfo_.uGemHandle_   = uGemHandle_;

  sf_handle      = handle;
  iFd_           = fd;
  iFormat_       = format;
  iWidth_        = w;
  iHeight_       = h;
  iStride_       = stride;
  iByteStride_   = byte_stride;
  iSize_         = size;
  iUsage         = usage;
  uFourccFormat_ = fourcc;
  uModifier_     = modefier;
  sLayerName_    = name;
  uBufferId_     = buffer_id;
  uGemHandle_    = gemhandle;

  iBestPlaneType = PLANE_RK3588_ALL_ESMART_MASK;

  source_crop.left   = intput_crop.left;
  source_crop.top    = intput_crop.top;
  source_crop.right  = intput_crop.right;
  source_crop.bottom = intput_crop.bottom;

  transform = DRM_MODE_ROTATE_0;
  Init();
  HWC2_ALOGD_IF_DEBUG(
        "SvepTransform : LayerId[%u] Fourcc=%c%c%c%c Buf=[%4d,%4d,%4d,%4d]  src=[%5.0f,%5.0f,%5.0f,%5.0f] dis=[%4d,%4d,%4d,%4d] Transform=%-8.8s(0x%x)\n"
        "                            Fourcc=%c%c%c%c Buf=[%4d,%4d,%4d,%4d]  src=[%5.0f,%5.0f,%5.0f,%5.0f] dis=[%4d,%4d,%4d,%4d] Transform=%-8.8s(0x%x)\n",
             uId_,
             storeLayerInfo_.uFourccFormat_,storeLayerInfo_.uFourccFormat_>>8,
             storeLayerInfo_.uFourccFormat_>>16,storeLayerInfo_.uFourccFormat_>>24,
             storeLayerInfo_.iWidth_,storeLayerInfo_.iHeight_,storeLayerInfo_.iStride_,storeLayerInfo_.iSize_,
             storeLayerInfo_.source_crop.left,storeLayerInfo_.source_crop.top,
             storeLayerInfo_.source_crop.right,storeLayerInfo_.source_crop.bottom,
             storeLayerInfo_.display_frame.left,storeLayerInfo_.display_frame.top,
             storeLayerInfo_.display_frame.right,storeLayerInfo_.display_frame.bottom,
             TransformToString(storeLayerInfo_.transform).c_str(),storeLayerInfo_.transform,
             uFourccFormat_,uFourccFormat_>>8,uFourccFormat_>>16,uFourccFormat_>>24,iWidth_,iHeight_,iStride_,iSize_,
             source_crop.left,source_crop.top,source_crop.right,source_crop.bottom,
             display_frame.left,display_frame.top,display_frame.right,display_frame.bottom,
             TransformToString(transform).c_str(),transform);
  return;
}

void DrmHwcLayer::ResetInfoFromStore(){
  if(!storeLayerInfo_.valid_){
    HWC2_ALOGE("ResetInfoFromStore fail, There may be some errors.");
    return;
  }

  sf_handle    = storeLayerInfo_.sf_handle;
  transform    = storeLayerInfo_.transform;
  source_crop  = storeLayerInfo_.source_crop;
  iFd_         = storeLayerInfo_.iFd_;
  iFormat_     = storeLayerInfo_.iFormat_;
  iWidth_      = storeLayerInfo_.iWidth_ ;
  iHeight_     = storeLayerInfo_.iHeight_;
  iStride_     = storeLayerInfo_.iStride_;
  iByteStride_ = storeLayerInfo_.iByteStride_;
  iUsage       = storeLayerInfo_.iUsage;
  uFourccFormat_ = storeLayerInfo_.uFourccFormat_;
  uModifier_     = storeLayerInfo_.uModifier_;
  sLayerName_    = storeLayerInfo_.sLayerName_;
  uBufferId_     = storeLayerInfo_.uBufferId_;
  uGemHandle_    = storeLayerInfo_.uGemHandle_;

  Init();
  HWC2_ALOGD_IF_DEBUG(
             "reset:DrmHwcLayer[%4u] Buffer[w/h/s/format]=[%4d,%4d,%4d,%4d] Fourcc=%c%c%c%c Transform=%-8.8s(0x%x) Blend[a=%d]=%-8.8s "
             "source_crop[l,t,r,b]=[%5.0f,%5.0f,%5.0f,%5.0f] display_frame[l,t,r,b]=[%4d,%4d,%4d,%4d],skip=%d,afbcd=%d,gles=%d\n",
             uId_,iWidth_,iHeight_,iStride_,iFormat_,uFourccFormat_,uFourccFormat_>>8,uFourccFormat_>>16,uFourccFormat_>>24,
             TransformToString(transform).c_str(),transform,alpha,BlendingToString(blending).c_str(),
             source_crop.left,source_crop.top,source_crop.right,source_crop.bottom,
             display_frame.left,display_frame.top,display_frame.right,display_frame.bottom,bSkipLayer_,bAfbcd_,bGlesCompose_);

  memset(&storeLayerInfo_, 0x00, sizeof(storeLayerInfo_));
  storeLayerInfo_.valid_ = false;

  return;
}



std::string DrmHwcLayer::TransformToString(uint32_t transform) const{
  switch (transform) {
      case DRM_MODE_ROTATE_0:
        return "None";
      case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X:
        return "FlipH";
      case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_Y:
        return "FlipV";
      case DRM_MODE_ROTATE_90:
        return "Rotate90";
      case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y:
        return "Rotate180";
      case DRM_MODE_ROTATE_270:
        return "Rotate270";
      case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_90:
        return "FlipHRotate90";
      case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_90:
        return "FlipVRotate90";
      default:
        return "Unknown";
  }
}
std::string DrmHwcLayer::BlendingToString(DrmHwcBlending blending) const{
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

int DrmHwcLayer::DumpInfo(String8 &out){
    if(bFbTarget_)
      out.appendFormat( "DrmHwcFBtar[%4u] Buffer[w/h/s/format]=[%4d,%4d,%4d,%4d] Fourcc=%c%c%c%c Transform=%-8.8s(0x%x) Blend[a=%d]=%-8.8s "
                    "source_crop[l,t,r,b]=[%5.0f,%5.0f,%5.0f,%5.0f] display_frame[l,t,r,b]=[%4d,%4d,%4d,%4d],afbcd=%d\n",
                   uId_,iWidth_,iHeight_,iStride_,iFormat_,uFourccFormat_,uFourccFormat_>>8,uFourccFormat_>>16,uFourccFormat_>>24,
                   TransformToString(transform).c_str(),transform,alpha,BlendingToString(blending).c_str(),
                   source_crop.left,source_crop.top,source_crop.right,source_crop.bottom,
                   display_frame.left,display_frame.top,display_frame.right,display_frame.bottom,bAfbcd_);
    else
      out.appendFormat( "DrmHwcLayer[%4u] Buffer[w/h/s/format]=[%4d,%4d,%4d,%4d] Fourcc=%c%c%c%c Transform=%-8.8s(0x%x) Blend[a=%d]=%-8.8s "
                        "source_crop[l,t,r,b]=[%5.0f,%5.0f,%5.0f,%5.0f] display_frame[l,t,r,b]=[%4d,%4d,%4d,%4d],skip=%d,afbcd=%d\n",
                       uId_,iWidth_,iHeight_,iStride_,iFormat_,uFourccFormat_,uFourccFormat_>>8,uFourccFormat_>>16,uFourccFormat_>>24,
                       TransformToString(transform).c_str(),transform,alpha,BlendingToString(blending).c_str(),
                       source_crop.left,source_crop.top,source_crop.right,source_crop.bottom,
                       display_frame.left,display_frame.top,display_frame.right,display_frame.bottom,bSkipLayer_,bAfbcd_);
    return 0;
}

int DrmHwcLayer::DumpData(){
  if(!sf_handle){
    ALOGI_IF(LogLevel(DBG_INFO),"%s,line=%d LayerId=%u Buffer is null.",__FUNCTION__,__LINE__,uId_);
    return -1;
  }

  DrmGralloc *drm_gralloc = DrmGralloc::getInstance();
  if(!drm_gralloc){
    ALOGI_IF(LogLevel(DBG_INFO),"%s,line=%d LayerId=%u drm_gralloc is null.",__FUNCTION__,__LINE__,uId_);
    return -1;
  }
  void* cpu_addr = NULL;
  static int frame_cnt =0;
  int width,height,stride,byte_stride,format,size;
  int ret = 0;
  width  = drm_gralloc->hwc_get_handle_attibute(sf_handle,ATT_WIDTH);
  height = drm_gralloc->hwc_get_handle_attibute(sf_handle,ATT_HEIGHT);
  stride = drm_gralloc->hwc_get_handle_attibute(sf_handle,ATT_STRIDE);
  format = drm_gralloc->hwc_get_handle_attibute(sf_handle,ATT_FORMAT);
  size   = drm_gralloc->hwc_get_handle_attibute(sf_handle,ATT_SIZE);
  byte_stride = drm_gralloc->hwc_get_handle_attibute(sf_handle,ATT_BYTE_STRIDE);

  cpu_addr = drm_gralloc->hwc_get_handle_lock(sf_handle,width,height);
  if(ret){
    ALOGE("%s,line=%d, LayerId=%u, lock fail ret = %d ",__FUNCTION__,__LINE__,uId_,ret);
    return ret;
  }
  FILE * pfile = NULL;
  char data_name[100] ;
  system("mkdir /data/dump/ && chmod /data/dump/ 777 ");
  sprintf(data_name,"/data/dump/%d_%15.15s_id-%d_%dx%d_f-%d.bin",
          frame_cnt++,sLayerName_.size() < 5 ? "unset" : sLayerName_.c_str(),
          uId_,stride,height,format);

  pfile = fopen(data_name,"wb");
  if(pfile)
  {
      fwrite((const void *)cpu_addr,(size_t)(size),1,pfile);
      fflush(pfile);
      fclose(pfile);
      ALOGD(" dump surface layer_id=%d ,data_name %s,w:%d,h:%d,stride :%d,size=%d,cpu_addr=%p",
          uId_,data_name,width,height,byte_stride,size,cpu_addr);
  }
  else
  {
      ALOGE("Open %s fail", data_name);
      ALOGD(" dump surface layer_id=%d ,data_name %s,w:%d,h:%d,stride :%d,size=%d,cpu_addr=%p",
          uId_,data_name,width,height,byte_stride,size,cpu_addr);
  }


  ret = drm_gralloc->hwc_get_handle_unlock(sf_handle);
  if(ret){
    ALOGE("%s,line=%d, LayerId=%u, unlock fail ret = %d ",__FUNCTION__,__LINE__,uId_,ret);
    return ret;
  }
  return 0;
}


}  // namespace android
