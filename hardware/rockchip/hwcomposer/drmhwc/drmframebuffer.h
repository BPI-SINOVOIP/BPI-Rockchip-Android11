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

#ifndef ANDROID_DRM_FRAMEBUFFER_
#define ANDROID_DRM_FRAMEBUFFER_

#include <stdint.h>

#include <sync/sync.h>

#include <ui/GraphicBuffer.h>

namespace android {

#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
struct DrmRgaBuffer {
  DrmRgaBuffer() : release_fence_fd_(-1) {
  }

  ~DrmRgaBuffer() {
    if (release_fence_fd() >= 0)
      close(release_fence_fd());
  }

  bool is_valid() {
    return buffer_ != NULL;
  }

  sp<GraphicBuffer> buffer() {
    return buffer_;
  }

  int release_fence_fd() {
    return release_fence_fd_;
  }

  void set_release_fence_fd(int fd) {
    if (release_fence_fd_ >= 0)
      close(release_fence_fd_);
    release_fence_fd_ = fd;
  }

  bool Allocate(uint32_t w, uint32_t h, int32_t format) {
    if (is_valid()) {
      if (buffer_->getWidth() == w && buffer_->getHeight() == h && buffer_->getPixelFormat() == format)
        return true;

      if (release_fence_fd_ >= 0) {
        if (sync_wait(release_fence_fd_, kReleaseWaitTimeoutMs) != 0) {
          ALOGE("Wait for release fence failed\n");
          return false;
        }
      }
      Clear();
    }
    ALOGD_IF(log_level(DBG_DEBUG), "RGA Allocate buffer %d x %d", w, h);
    buffer_ = new GraphicBuffer(w, h, format,
                                 GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN
#ifndef TARGET_PRODUCT_IOT_RK3229_EVB
                                 ,"DRM_HWC_RgaBuffer"
#endif
                                 );
    release_fence_fd_ = -1;
    return is_valid();
  }

  void Clear() {
    if (!is_valid())
      return;

    if (release_fence_fd_ >= 0) {
      close(release_fence_fd_);
      release_fence_fd_ = -1;
    }
    ALOGD_IF(log_level(DBG_DEBUG), "RGA free buffer %d x %d", buffer_->getWidth(), buffer_->getHeight());
    buffer_.clear();
  }

  int WaitReleased(int timeout_milliseconds) {
    if (!is_valid())
      return 0;
    if (release_fence_fd_ < 0)
      return 0;

    int ret = sync_wait(release_fence_fd_, timeout_milliseconds);
    return ret;
  }

  // Somewhat arbitrarily chosen, but wanted to stay below 3000ms, which is the
  // system timeout
  static const int kReleaseWaitTimeoutMs = 1500;

 private:
  sp<GraphicBuffer> buffer_;
  int release_fence_fd_;
};
#endif

struct DrmFramebuffer {
  DrmFramebuffer() : release_fence_fd_(-1) {
  }

  ~DrmFramebuffer() {
    if (release_fence_fd() >= 0)
      close(release_fence_fd());
  }

  bool is_valid() {
    return buffer_ != NULL;
  }

  sp<GraphicBuffer> buffer() {
    return buffer_;
  }

  int release_fence_fd() {
    return release_fence_fd_;
  }

  void set_release_fence_fd(int fd) {
    if (release_fence_fd_ >= 0)
      close(release_fence_fd_);
    release_fence_fd_ = fd;
  }

  bool Allocate(uint32_t w, uint32_t h) {
    if (is_valid()) {
      if (buffer_->getWidth() == w && buffer_->getHeight() == h)
        return true;

      if (release_fence_fd_ >= 0) {
        if (sync_wait(release_fence_fd_, kReleaseWaitTimeoutMs) != 0) {
          ALOGE("Wait for release fence failed\n");
          return false;
        }
      }
      Clear();
    }

    buffer_ = new GraphicBuffer(w, h, PIXEL_FORMAT_RGBA_8888,
                                GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_HW_RENDER |
                                    GRALLOC_USAGE_HW_COMPOSER
//close fbdc for pre-comp and squash layer.
#if USE_AFBC_LAYER
                                    | MAGIC_USAGE_FOR_AFBC_LAYER
#endif
#ifndef TARGET_PRODUCT_IOT_RK3229_EVB
				, "DRM_HWC_Framebuffer"
#endif
				);
    release_fence_fd_ = -1;
    return is_valid();
  }

  void Clear() {
    if (!is_valid())
      return;

    if (release_fence_fd_ >= 0) {
      close(release_fence_fd_);
      release_fence_fd_ = -1;
    }

    buffer_.clear();
  }

  int WaitReleased(int timeout_milliseconds) {
    if (!is_valid())
      return 0;
    if (release_fence_fd_ < 0)
      return 0;

    int ret = sync_wait(release_fence_fd_, timeout_milliseconds);
    return ret;
  }

  // Somewhat arbitrarily chosen, but wanted to stay below 3000ms, which is the
  // system timeout
  static const int kReleaseWaitTimeoutMs = 1500;

 private:
  sp<GraphicBuffer> buffer_;
  int release_fence_fd_;
};
}

#endif  // ANDROID_DRM_FRAMEBUFFER_
