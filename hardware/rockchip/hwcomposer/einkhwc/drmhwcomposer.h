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

#ifndef ANDROID_DRM_HWCOMPOSER_H_
#define ANDROID_DRM_HWCOMPOSER_H_

#include <stdbool.h>
#include <stdint.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include "autofd.h"
#include "separate_rects.h"
#include "drmhwcgralloc.h"
#include "hwc_debug.h"
#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
#include <RockchipRga.h>
#endif

/*hwc version*/
#define GHWC_VERSION                    "0.52"

/* hdr usage */
/*usage & 0x0F000000
  0x1000000 bt2020
  0x2000000 hdr10
  0x3000000 hlg
  0x4000000 dobly version
 */
#define HDRUSAGE                                       0x2000000

/* msleep for hotplug in event */
#define HOTPLUG_MSLEEP			(200)

// hdmi status path
#define HDMI_STATUS_PATH		"/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/status"

struct hwc_import_context;

int hwc_import_init(struct hwc_import_context **ctx);
int hwc_import_destroy(struct hwc_import_context *ctx);

int hwc_import_bo_create(int fd, struct hwc_import_context *ctx,
                         buffer_handle_t buf, struct hwc_drm_bo *bo);
bool hwc_import_bo_release(int fd, struct hwc_import_context *ctx,
                           struct hwc_drm_bo *bo);


namespace android {

#define UN_USED(arg)     (arg=arg)

#if USE_AFBC_LAYER
#ifdef TARGET_BOARD_PLATFORM_RK3368
#define HAL_FB_COMPRESSION_NONE                0
#else
#define GRALLOC_ARM_INTFMT_EXTENSION_BIT_START          32
/* This format will use AFBC */
#define	    GRALLOC_ARM_INTFMT_AFBC                     (1ULL << (GRALLOC_ARM_INTFMT_EXTENSION_BIT_START+0))
#endif

#define SKIP_BOOT                                       (1)
#define MAGIC_USAGE_FOR_AFBC_LAYER                      (0x88)
#endif

#if SKIP_BOOT
#define BOOT_COUNT       (2)
#endif

#define BOOT_GLES_COUNT  (5)

typedef enum tagMode3D
{
	NON_3D = 0,
	H_3D=1,
	V_3D=2,
	FPS_3D=8,
}Mode3D;

template <typename T>
using DrmHwcRect = separate_rects::Rect<T>;

enum DrmHwcTransform {
  kIdentity = 0,
  kFlipH = 1 << 0,
  kFlipV = 1 << 1,
  kRotate90 = 1 << 2,
  kRotate180 = 1 << 3,
  kRotate270 = 1 << 4,
  kRotate0 = 1 << 5
};

enum class DrmHwcBlending : int32_t {
  kNone = HWC_BLENDING_NONE,
  kPreMult = HWC_BLENDING_PREMULT,
  kCoverage = HWC_BLENDING_COVERAGE,
};
}

#endif
