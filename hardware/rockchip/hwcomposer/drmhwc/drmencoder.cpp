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

#include "drmcrtc.h"
#include "drmencoder.h"
#include "drmresources.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace android {

DrmEncoder::DrmEncoder(DrmResources *drm, drmModeEncoderPtr e, DrmCrtc *current_crtc,
                       const std::vector<DrmCrtc *> &possible_crtcs)
    : id_(e->encoder_id),
      crtc_(current_crtc),
      drm_(drm),
      type_(e->encoder_type),
      possible_crtcs_(possible_crtcs),
      encoder_(e) {
}

uint32_t DrmEncoder::id() const {
  return id_;
}

uint32_t DrmEncoder::type() const {
  return type_;
}
DrmCrtc *DrmEncoder::crtc() const {
  return crtc_;
}

void DrmEncoder::set_crtc(DrmCrtc *crtc) {
  crtc_ = crtc;
}

void DrmEncoder::dump_encoder(std::ostringstream *out) const {
    *out << encoder_->encoder_id << "\t"
         << encoder_->crtc_id << "\t"
         << drm_->encoder_type_str(encoder_->encoder_type) << "\t"
         << std::hex << encoder_->possible_crtcs << "\t"
         << std::hex << encoder_->possible_clones << "\n";
}
}
