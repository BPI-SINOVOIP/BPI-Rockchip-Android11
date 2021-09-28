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

#ifndef ANDROID_DRM_MODE_H_
#define ANDROID_DRM_MODE_H_

#include <stdint.h>
#include <string>
#include <xf86drmMode.h>

#define DRM_MODE_FLAG_420_MASK			(0x03<<23)

namespace android {

class DrmMode {
 public:
  DrmMode() = default;
  DrmMode(drmModeModeInfoPtr m);
  ~DrmMode();

  bool operator==(const drmModeModeInfo &m) const;
  bool operator==(const DrmMode &m) const;
  bool equal(const DrmMode &m) const;
  bool equal_no_flag_and_type(const DrmMode &m) const;
  bool equal(uint32_t width, uint32_t height, uint32_t vrefresh, bool interlaced) const;
  bool equal(uint32_t width, uint32_t height, uint32_t vrefresh,
                     uint32_t flag, uint32_t clk, bool interlaced) const;
  bool equal(uint32_t width, uint32_t height, float vrefresh,
             uint32_t hsync_start, uint32_t hsync_end, uint32_t htotal,
             uint32_t vsync_start, uint32_t vsync_end, uint32_t vtotal,
             uint32_t flag) const;
  void ToDrmModeModeInfo(drm_mode_modeinfo *m) const;

  uint32_t id() const;
  void set_id(uint32_t id);

  uint32_t clock() const;

  uint32_t h_display() const;
  uint32_t h_sync_start() const;
  uint32_t h_sync_end() const;
  uint32_t h_total() const;
  uint32_t h_skew() const;

  uint32_t v_display() const;
  uint32_t v_sync_start() const;
  uint32_t v_sync_end() const;
  uint32_t v_total() const;
  uint32_t v_scan() const;
  float v_refresh() const;

  uint32_t flags() const;
  uint32_t interlaced() const;
  uint32_t type() const;

  std::string name() const;

 private:
  int fd_ = 0;
  uint32_t id_ = 0;
  uint32_t blob_id_ = 0;

  uint32_t clock_ = 0;

  uint32_t h_display_ = 0;
  uint32_t h_sync_start_ = 0;
  uint32_t h_sync_end_ = 0;
  uint32_t h_total_ = 0;
  uint32_t h_skew_ = 0;

  uint32_t v_display_ = 0;
  uint32_t v_sync_start_ = 0;
  uint32_t v_sync_end_ = 0;
  uint32_t v_total_ = 0;
  uint32_t v_scan_ = 0;
  uint32_t v_refresh_ = 0;

  uint32_t flags_ = 0;
  uint32_t type_ = 0;
  int interlaced_ =0;

  std::string name_;
};
}

#endif  // ANDROID_DRM_MODE_H_
