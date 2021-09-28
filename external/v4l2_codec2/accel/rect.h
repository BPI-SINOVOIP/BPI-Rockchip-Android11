// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Note: ported from Chromium commit head: 8a796386c11a
// Note: only necessary functions are ported from gfx::Rect

// Defines a simple integer rectangle class.  The containment semantics
// are array-like; that is, the coordinate (x, y) is considered to be
// contained by the rectangle, but the coordinate (x + width, y) is not.
// The class will happily let you create malformed rectangles (that is,
// rectangles with negative width and/or height), but there will be assertions
// in the operations (such as Contains()) to complain in this case.

#ifndef RECT_H_
#define RECT_H_

#include <string>

#include "base/strings/stringprintf.h"
#include "size.h"

namespace media {

// Helper struct for rect to replace gfx::Rect usage from original code.
// Only partial functions of gfx::Rect is implemented here.
class Rect {
 public:
  constexpr Rect() = default;
  constexpr Rect(int width, int height) : size_(width, height) {}
  constexpr Rect(int x, int y, int width, int height)
      : x_(x),
        y_(y),
        size_(GetClampedValue(x, width), GetClampedValue(y, height)) {}
  constexpr explicit Rect(const Size& size) : size_(size) {}

  constexpr int x() const { return x_; }
  // Sets the X position while preserving the width.
  void set_x(int x) {
    x_ = x;
    size_.set_width(GetClampedValue(x, width()));
  }

  constexpr int y() const { return y_; }
  // Sets the Y position while preserving the height.
  void set_y(int y) {
    y_ = y;
    size_.set_height(GetClampedValue(y, height()));
  }

  constexpr int width() const { return size_.width(); }
  void set_width(int width) { size_.set_width(GetClampedValue(x(), width)); }

  constexpr int height() const { return size_.height(); }
  void set_height(int height) {
    size_.set_height(GetClampedValue(y(), height));
  }

  constexpr const Size& size() const { return size_; }
  void set_size(const Size& size) {
    set_width(size.width());
    set_height(size.height());
  }

  constexpr int right() const { return x() + width(); }
  constexpr int bottom() const { return y() + height(); }

  void SetRect(int x, int y, int width, int height) {
    set_x(x);
    set_y(y);
    // Ensure that width and height remain valid.
    set_width(width);
    set_height(height);
  }

  // Returns true if the area of the rectangle is zero.
  bool IsEmpty() const { return size_.IsEmpty(); }

  // Returns true if this rectangle contains the specified rectangle.
  bool Contains(const Rect& rect) const {
    return (rect.x() >= x() && rect.right() <= right() && rect.y() >= y() &&
            rect.bottom() <= bottom());
  }

  // Computes the intersection of this rectangle with the given rectangle.
  void Intersect(const Rect& rect) {
    if (IsEmpty() || rect.IsEmpty()) {
      SetRect(0, 0, 0, 0);  // Throws away empty position.
      return;
    }

    int left = std::max(x(), rect.x());
    int top = std::max(y(), rect.y());
    int new_right = std::min(right(), rect.right());
    int new_bottom = std::min(bottom(), rect.bottom());

    if (left >= new_right || top >= new_bottom) {
      SetRect(0, 0, 0, 0);  // Throws away empty position.
      return;
    }

    SetRect(left, top, new_right - left, new_bottom - top);
  }

  std::string ToString() const {
    return base::StringPrintf("(%d,%d) %s",
                              x_, y_, size().ToString().c_str());
  }

 private:
  int x_ = 0;
  int y_ = 0;
  Size size_;

  // Returns true iff a+b would overflow max int.
  static constexpr bool AddWouldOverflow(int a, int b) {
    // In this function, GCC tries to make optimizations that would only work if
    // max - a wouldn't overflow but it isn't smart enough to notice that a > 0.
    // So cast everything to unsigned to avoid this.  As it is guaranteed that
    // max - a and b are both already positive, the cast is a noop.
    //
    // This is intended to be: a > 0 && max - a < b
    return a > 0 && b > 0 &&
           static_cast<unsigned>(std::numeric_limits<int>::max() - a) <
               static_cast<unsigned>(b);
  }

  // Clamp the size to avoid integer overflow in bottom() and right().
  // This returns the width given an origin and a width.
  // TODO(enne): this should probably use base::ClampAdd, but that
  // function is not a constexpr.
  static constexpr int GetClampedValue(int origin, int size) {
    return AddWouldOverflow(origin, size)
               ? std::numeric_limits<int>::max() - origin
               : size;
  }
};

inline bool operator==(const Rect& lhs, const Rect& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y() && lhs.size() == rhs.size();
}

inline bool operator!=(const Rect& lhs, const Rect& rhs) {
  return !(lhs == rhs);
}

}  // namespace media

#endif  // RECT_H_
