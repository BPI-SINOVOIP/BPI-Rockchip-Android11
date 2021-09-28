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
#ifndef _DRM_GRALLOC_H_
#define _DRM_GRALLOC_H_
#include "rockchip/drmtype.h"
#if USE_GRALLOC_4
#include "rockchip/drmgralloc4.h"
#endif


#include <hardware/gralloc.h>
#include <map>
#include <vector>

namespace android {

class DrmGralloc{
public:
	static DrmGralloc* getInstance(){
		static DrmGralloc drmGralloc_;
		return &drmGralloc_;
	}

  void set_drm_version(int version);
  int hwc_get_handle_width(buffer_handle_t hnd);
  int hwc_get_handle_height(buffer_handle_t hnd);
  int hwc_get_handle_format(buffer_handle_t hnd);
  int hwc_get_handle_stride(buffer_handle_t hnd);
  int hwc_get_handle_byte_stride(buffer_handle_t hnd);
  int hwc_get_handle_byte_stride_workround(buffer_handle_t hnd);
  int hwc_get_handle_usage(buffer_handle_t hnd);
  int hwc_get_handle_size(buffer_handle_t hnd);
  int hwc_get_handle_attributes(buffer_handle_t hnd, std::vector<int> *attrs);
  int hwc_get_handle_attibute(buffer_handle_t hnd, attribute_flag_t flag);
  int hwc_get_handle_primefd(buffer_handle_t hnd);
  int hwc_get_handle_name(buffer_handle_t hnd, std::string &name);
  int hwc_get_handle_buffer_id(buffer_handle_t hnd, uint64_t *buffer_id);
  void* hwc_get_handle_lock(buffer_handle_t hnd, int width, int height);
  int hwc_get_handle_unlock(buffer_handle_t hnd);
  uint32_t hwc_get_handle_phy_addr(buffer_handle_t hnd);
  uint64_t hwc_get_handle_format_modifier(buffer_handle_t hnd);
  uint32_t hwc_get_handle_fourcc_format(buffer_handle_t hnd);
  uint64_t hwc_get_handle_internal_format(buffer_handle_t hnd);

private:
	DrmGralloc();
	~DrmGralloc();
	DrmGralloc(const DrmGralloc&);
	DrmGralloc& operator=(const DrmGralloc&);

  int drmVersion_;
#if USE_GRALLOC_4
#else
  const gralloc_module_t *gralloc_;
#endif
};
}

#endif

