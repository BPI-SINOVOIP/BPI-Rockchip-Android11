/*
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

#include "drmmode.h"
#include "drmresources.h"

#include <stdint.h>
#include <math.h>
#include <string>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <log/log.h>

namespace android {

DrmMode::DrmMode(drmModeModeInfoPtr m)
    : id_(0),
      clock_(m->clock),
      h_display_(m->hdisplay),
      h_sync_start_(m->hsync_start),
      h_sync_end_(m->hsync_end),
      h_total_(m->htotal),
      h_skew_(m->hskew),
      v_display_(m->vdisplay),
      v_sync_start_(m->vsync_start),
      v_sync_end_(m->vsync_end),
      v_total_(m->vtotal),
      v_scan_(m->vscan),
      v_refresh_(m->vrefresh),
      flags_(m->flags),
      type_(m->type),
      name_(m->name) {
      interlaced_ = !!(flags_ & DRM_MODE_FLAG_INTERLACE);
}

DrmMode::~DrmMode()
{
  if (blob_id_ && fd_) {
    struct drm_mode_destroy_blob destroy_blob;
    memset(&destroy_blob, 0, sizeof(destroy_blob));
    destroy_blob.blob_id = (__u32)blob_id_;
    int ret = drmIoctl(fd_, DRM_IOCTL_MODE_DESTROYPROPBLOB, &destroy_blob);
    if (ret)
      ALOGE("Failed to destroy mode property blob %d, ret=%d", blob_id_, ret);
  }
}

bool DrmMode::operator==(const drmModeModeInfo &m) const {
  return clock_ == m.clock && h_display_ == m.hdisplay &&
         h_sync_start_ == m.hsync_start && h_sync_end_ == m.hsync_end &&
         h_total_ == m.htotal && h_skew_ == m.hskew &&
         v_display_ == m.vdisplay && v_sync_start_ == m.vsync_start &&
         v_sync_end_ == m.vsync_end && v_total_ == m.vtotal &&
         v_scan_ == m.vscan && flags_ == m.flags && type_ == m.type;
}

bool DrmMode::operator==(const DrmMode &m) const {
  return clock_ == m.clock() && h_display_ == m.h_display()&&
         h_sync_start_ == m.h_sync_start() && h_sync_end_ == m.h_sync_end() &&
         h_total_ == m.h_total() && h_skew_ == m.h_skew() &&
         v_display_ == m.v_display() && v_sync_start_ == m.v_sync_start() &&
         v_sync_end_ == m.v_sync_end() && v_total_ == m.v_total() &&
         v_scan_ == m.v_scan() && flags_ == m.flags() && type_ == m.type();
}

bool DrmMode::equal(const DrmMode &m) const {
  if (clock_ == m.clock() && h_display_ == m.h_display()&&
      h_sync_start_ == m.h_sync_start() && h_sync_end_ == m.h_sync_end() &&
      h_total_ == m.h_total()&&
      v_display_ == m.v_display() && v_sync_start_ == m.v_sync_start() &&
      v_sync_end_ == m.v_sync_end() && v_total_ == m.v_total()
      && flags_ == m.flags())
        return true;
  return false;
}

bool DrmMode::equal(uint32_t width, uint32_t height, uint32_t vrefresh,
                    bool interlaced) const
{
  if (h_display_ == width && v_display_ == height &&
      interlaced_ == interlaced && v_refresh_ == vrefresh)
    return true;
  return false;
}

bool DrmMode::equal(uint32_t width, uint32_t height, float vrefresh,
                    uint32_t hsync_start, uint32_t hsync_end, uint32_t htotal,
                    uint32_t vsync_start, uint32_t vsync_end, uint32_t vtotal,
                    uint32_t flags) const
{
  float v_refresh = clock_ / (float)(v_total_ * h_total_) * 1000.0f;
  uint32_t flags_temp;
  if (flags_ & DRM_MODE_FLAG_INTERLACE)
    v_refresh *= 2;
  if (flags_ & DRM_MODE_FLAG_DBLSCAN)
    v_refresh /= 2;
  if (v_scan_ > 1)
    v_refresh /= v_scan_ ;

  /* vrefresh within 1 HZ */
  if (fabs(v_refresh - vrefresh) > 1.0f)
    return false;

  if (h_display_ == width && v_display_ == height &&
      hsync_start == h_sync_start_ && hsync_end == h_sync_end_ &&
      vsync_start == v_sync_start_ && vsync_end == v_sync_end_ &&
      htotal == h_total_ && vtotal == v_total_ && flags == flags_)
    return true;

  if (h_display_ == width && v_display_ == height &&
      hsync_start == h_sync_start_ && hsync_end == h_sync_end_ &&
      vsync_start == v_sync_start_ && vsync_end == v_sync_end_ &&
      htotal == h_total_ && vtotal == v_total_ ) {
        flags_temp = DRM_MODE_FLAG_PHSYNC|DRM_MODE_FLAG_NHSYNC|DRM_MODE_FLAG_PVSYNC|
                         DRM_MODE_FLAG_NVSYNC|DRM_MODE_FLAG_INTERLACE|
                         DRM_MODE_FLAG_420_MASK;
        if((flags & flags_temp) == (flags_ & flags_temp))
          return true;
      }
  return false;
}


bool DrmMode::equal(uint32_t width, uint32_t height, uint32_t vrefresh,
                     uint32_t flag, uint32_t clk, bool interlaced) const
{
  ALOGV("DrmMode h=%d,v=%d,interlaced=%d,v_refresh_=%d,flags=%d,clk=%d",
         h_display_, v_display_, interlaced_, v_refresh_, flags_,clock_);
  if (h_display_ == width && v_display_ == height &&
      interlaced_ == interlaced && v_refresh_ == vrefresh &&
      flags_ == flag && clock_ == clk)
    return true;
  return false;
}

void DrmMode::ToDrmModeModeInfo(drm_mode_modeinfo *m) const {
  m->clock = clock_;
  m->hdisplay = h_display_;
  m->hsync_start = h_sync_start_;
  m->hsync_end = h_sync_end_;
  m->htotal = h_total_;
  m->hskew = h_skew_;
  m->vdisplay = v_display_;
  m->vsync_start = v_sync_start_;
  m->vsync_end = v_sync_end_;
  m->vtotal = v_total_;
  m->vscan = v_scan_;
  m->vrefresh = v_refresh_;
  m->flags = flags_;
  m->type = type_;
  strncpy(m->name, name_.c_str(), DRM_DISPLAY_MODE_LEN);
}

uint32_t DrmMode::id() const {
  return id_;
}

void DrmMode::set_id(uint32_t id) {
  id_ = id;
}

uint32_t DrmMode::clock() const {
  return clock_;
}

uint32_t DrmMode::h_display() const {
  return h_display_;
}

uint32_t DrmMode::h_sync_start() const {
  return h_sync_start_;
}

uint32_t DrmMode::h_sync_end() const {
  return h_sync_end_;
}

uint32_t DrmMode::h_total() const {
  return h_total_;
}

uint32_t DrmMode::h_skew() const {
  return h_skew_;
}

uint32_t DrmMode::v_display() const {
  return v_display_;
}

uint32_t DrmMode::v_sync_start() const {
  return v_sync_start_;
}

uint32_t DrmMode::v_sync_end() const {
  return v_sync_end_;
}

uint32_t DrmMode::v_total() const {
  return v_total_;
}

uint32_t DrmMode::v_scan() const {
  return v_scan_;
}

float DrmMode::v_refresh() const {
  return v_refresh_ ? v_refresh_ * 1.0f :
                      clock_ / (float)(v_total_ * h_total_) * 1000.0f;
}

uint32_t DrmMode::flags() const {
  return flags_;
}

uint32_t DrmMode::interlaced() const {
    return interlaced_;
}

uint32_t DrmMode::type() const {
  return type_;
}

std::string DrmMode::name() const {
  return name_;
}
}
