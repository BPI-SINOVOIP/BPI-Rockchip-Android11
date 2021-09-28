// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Note: ported from Chromium commit head: 57ec858cddff

#ifndef MEDIA_COLOR_PLANE_LAYOUT_H_
#define MEDIA_COLOR_PLANE_LAYOUT_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>

namespace media {

// Encapsulates a color plane's memory layout: (stride, offset, size)
// stride: in bytes of a plane. Note that stride can be negative if the image
//         layout is bottom-up.
// offset: in bytes of a plane, which stands for the offset of a start point of
//         a color plane from a buffer FD.
// size:   in bytes of a plane. This |size| bytes data must contain all the data
//         a decoder will access (e.g. visible area and padding).
struct ColorPlaneLayout {
  ColorPlaneLayout();
  ColorPlaneLayout(int32_t stride, size_t offset, size_t size);
  ~ColorPlaneLayout();

  bool operator==(const ColorPlaneLayout& rhs) const;
  bool operator!=(const ColorPlaneLayout& rhs) const;

  int32_t stride = 0;
  size_t offset = 0;
  size_t size = 0;
};

// Outputs ColorPlaneLayout to stream.
std::ostream& operator<<(std::ostream& ostream, const ColorPlaneLayout& plane);

}  // namespace media

#endif  // MEDIA_COLOR_PLANE_LAYOUT_H_
