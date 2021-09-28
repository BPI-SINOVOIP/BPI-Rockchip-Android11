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

#define LOG_TAG "hwc-drm-crtc"

#include "drmcrtc.h"
#include "drmresources.h"

#include <stdint.h>
#include <xf86drmMode.h>

#ifdef ANDROID_P
#include <log/log.h>
#else
#include <cutils/log.h>
#endif


namespace android {

DrmCrtc::DrmCrtc(DrmResources *drm, drmModeCrtcPtr c, unsigned pipe)
    : drm_(drm),
      id_(c->crtc_id),
      pipe_(pipe),
      display_(-1),
      x_(c->x),
      y_(c->y),
      width_(c->width),
      height_(c->height),
      b_afbc_(false),
      mode_(&c->mode),
      mode_valid_(c->mode_valid),
      crtc_(c) {
}

int DrmCrtc::Init() {
  int ret = drm_->GetCrtcProperty(*this, "ACTIVE", &active_property_);
  if (ret) {
    ALOGE("Failed to get ACTIVE property");
    return ret;
  }

  ret = drm_->GetCrtcProperty(*this, "MODE_ID", &mode_property_);
  if (ret) {
    ALOGE("Failed to get MODE_ID property");
    return ret;
  }

  ret = drm_->GetCrtcProperty(*this, "FEATURE", &feature_property_);
  if (ret)
    ALOGE("Could not get FEATURE property");

    uint64_t feature=0;
    feature_property_.set_feature("afbdc");
    feature_property_.value(&feature);
    b_afbc_ = (feature ==1)?true:false;

  can_overscan_ = true;
  ret = drm_->GetCrtcProperty(*this, "left margin", &left_margin_property_);
  if (ret) {
    ALOGE("Failed to get left margin property");
    can_overscan_ = false;
  }
  ret = drm_->GetCrtcProperty(*this, "right margin", &right_margin_property_);
  if (ret) {
    ALOGE("Failed to get right margin property");
    can_overscan_ = false;
  }
  ret = drm_->GetCrtcProperty(*this, "top margin", &top_margin_property_);
  if (ret) {
    ALOGE("Failed to get top margin property");
    can_overscan_ = false;
  }
  ret = drm_->GetCrtcProperty(*this, "bottom margin", &bottom_margin_property_);
  if (ret) {
    ALOGE("Failed to get bottom margin property");
    can_overscan_ = false;
  }

  uint64_t alpha_scale = 0;
  can_alpha_scale_ = true;
  ret = drm_->GetCrtcProperty(*this, "ALPHA_SCALE", &alpha_scale_property_);
  if (ret) {
    ALOGE("Failed to get alpha_scale_property property");
  }
  alpha_scale_property_.value(&alpha_scale);
  if(alpha_scale == 0)
  {
    can_alpha_scale_ = false;
  }

  return 0;
}

bool DrmCrtc::get_afbc() const {
    return b_afbc_;
}

bool DrmCrtc::get_alpha_scale() const {
    return can_alpha_scale_;
}

uint32_t DrmCrtc::id() const {
  return id_;
}

unsigned DrmCrtc::pipe() const {
  return pipe_;
}

bool DrmCrtc::can_overscan() const {
  return can_overscan_;
}

const DrmProperty &DrmCrtc::active_property() const {
  return active_property_;
}

const DrmProperty &DrmCrtc::mode_property() const {
  return mode_property_;
}

const DrmProperty &DrmCrtc::left_margin_property() const {
  return left_margin_property_;
}

const DrmProperty &DrmCrtc::right_margin_property() const {
  return right_margin_property_;
}

const DrmProperty &DrmCrtc::top_margin_property() const {
  return top_margin_property_;
}

const DrmProperty &DrmCrtc::bottom_margin_property() const {
  return bottom_margin_property_;
}

const DrmProperty &DrmCrtc::alpha_scale_property() const {
  return alpha_scale_property_;
}

void DrmCrtc::dump_crtc(std::ostringstream *out) const
{

	*out << crtc_->crtc_id << "\t"
	     << crtc_->buffer_id << "\t"
	     << "(" << crtc_->x << "," << crtc_->y << ")\t("
	     << crtc_->width << "x" << crtc_->height << ")\n";

	drm_->dump_mode(&crtc_->mode, out);

	drm_->DumpCrtcProperty(*this,out);
}

}
