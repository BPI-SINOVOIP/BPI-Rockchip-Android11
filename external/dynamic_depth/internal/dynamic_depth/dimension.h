#ifndef DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_DIMENSION_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_DIMENSION_H_  // NOLINT

namespace dynamic_depth {

// A struct that contains the width and height of a size or the x and y
// coordinates of a point.
struct Dimension {
  Dimension(int w, int h) : width(w), height(h) {}
  int width;
  int height;

  inline bool operator==(const Dimension& other) const {
    return width == other.width && height == other.height;
  }

  inline bool operator!=(const Dimension& other) const {
    return !(*this == other);
  }
};

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_DIMENSION_H_  // NOLINT
