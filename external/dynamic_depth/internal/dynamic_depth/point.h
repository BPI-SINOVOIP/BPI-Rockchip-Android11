#ifndef DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_POINT_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_POINT_H_  // NOLINT

namespace dynamic_depth {

// A struct that contains the width and height of a size or the x and y
// coordinates of a point.
template <typename T>
struct Point {
  Point(T x_coord, T y_coord) : x(x_coord), y(y_coord) {}
  T x;
  T y;

  inline bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }

  inline bool operator!=(const Point& other) const { return !(*this == other); }
};

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_POINT_H_  // NOLINT
