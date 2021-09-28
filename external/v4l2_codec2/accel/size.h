// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Note: ported from Chromium commit head: 4db7af61f923
// Note: only necessary functions are ported from gfx::Size

#ifndef SIZE_H_
#define SIZE_H_

#include <string>

#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"

namespace media {

// Helper struct for size to replace gfx::size usage from original code.
// Only partial functions of gfx::size is implemented here.
struct Size {
 public:
  constexpr Size() : width_(0), height_(0) {}
  constexpr Size(int width, int height)
      : width_(std::max(0, width)), height_(std::max(0, height)) {}

  Size& operator=(const Size& ps) {
    set_width(ps.width());
    set_height(ps.height());
    return *this;
  }

  constexpr int width() const { return width_; }
  constexpr int height() const { return height_; }

  void set_width(int width) { width_ = std::max(0, width); }
  void set_height(int height) { height_ = std::max(0, height); }

  // This call will CHECK if the area of this size would overflow int.
  int GetArea() const { return GetCheckedArea().ValueOrDie(); }

  // Returns a checked numeric representation of the area.
  base::CheckedNumeric<int> GetCheckedArea() const {
    base::CheckedNumeric<int> checked_area = width();
    checked_area *= height();
    return checked_area;
  }

  void SetSize(int width, int height) {
    set_width(width);
    set_height(height);
  }

  bool IsEmpty() const { return !width() || !height(); }

  std::string ToString() const {
    return base::StringPrintf("%dx%d", width(), height());
  }

 private:
  int width_;
  int height_;
};

inline bool operator==(const Size& lhs, const Size& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

inline bool operator!=(const Size& lhs, const Size& rhs) {
  return !(lhs == rhs);
}

}  // namespace media

#endif  // SIZE_H_
