// Fast memory copying and comparison routines.
//   strings::fastmemcmp_inlined() replaces memcmp()
//   strings::memcpy_inlined() replaces memcpy()
//   strings::memeq(a, b, n) replaces memcmp(a, b, n) == 0
//
// strings::*_inlined() routines are inline versions of the
// routines exported by this module.  Sometimes using the inlined
// versions is faster.  Measure before using the inlined versions.

#ifndef DYNAMIC_DEPTH_INTERNAL_STRINGS_FASTMEM_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_STRINGS_FASTMEM_H_  // NOLINT

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "base/integral_types.h"
#include "base/macros.h"
#include "base/port.h"

namespace dynamic_depth {
namespace strings {

// Return true if the n bytes at a equal the n bytes at b.
// The regions are allowed to overlap.
//
// The performance is similar to the performance of memcmp(), but faster for
// moderately-sized inputs, or inputs that share a common prefix and differ
// somewhere in their last 8 bytes. Further optimizations can be added later
// if it makes sense to do so.  Alternatively, if the compiler & runtime improve
// to eliminate the need for this, we can remove it.  Please keep this in sync
// with google_internal::gg_memeq() in //third_party/stl/gcc3/string.
inline bool memeq(const char* a, const char* b, size_t n) {
  size_t n_rounded_down = n & ~static_cast<size_t>(7);
  if (PREDICT_FALSE(n_rounded_down == 0)) {  // n <= 7
    return memcmp(a, b, n) == 0;
  }
  // n >= 8
  uint64 u = UNALIGNED_LOAD64(a) ^ UNALIGNED_LOAD64(b);
  uint64 v = UNALIGNED_LOAD64(a + n - 8) ^ UNALIGNED_LOAD64(b + n - 8);
  if ((u | v) != 0) {  // The first or last 8 bytes differ.
    return false;
  }
  // The next line forces n to be a multiple of 8.
  n = n_rounded_down;
  if (n >= 80) {
    // In 2013 or later, this should be fast on long strings.
    return memcmp(a, b, n) == 0;
  }
  // Now force n to be a multiple of 16.  Arguably, a "switch" would be smart
  // here, but there's a difficult-to-evaluate code size vs. speed issue.  The
  // current approach often re-compares some bytes (worst case is if n initially
  // was 16, 32, 48, or 64), but is fairly short.
  size_t e = n & 8;
  a += e;
  b += e;
  n -= e;
  // n is now in {0, 16, 32, ...}.  Process 0 or more 16-byte chunks.
  while (n > 0) {
    uint64 x = UNALIGNED_LOAD64(a) ^ UNALIGNED_LOAD64(b);
    uint64 y = UNALIGNED_LOAD64(a + 8) ^ UNALIGNED_LOAD64(b + 8);
    if ((x | y) != 0) {
      return false;
    }
    a += 16;
    b += 16;
    n -= 16;
  }
  return true;
}

inline int fastmemcmp_inlined(const void* va, const void* vb, size_t n) {
  const unsigned char* pa = static_cast<const unsigned char*>(va);
  const unsigned char* pb = static_cast<const unsigned char*>(vb);
  switch (n) {
    default:
      return memcmp(va, vb, n);
    case 7:
      if (*pa != *pb) return *pa < *pb ? -1 : +1;
      ++pa;
      ++pb;
      FALLTHROUGH_INTENDED;
    case 6:
      if (*pa != *pb) return *pa < *pb ? -1 : +1;
      ++pa;
      ++pb;
      FALLTHROUGH_INTENDED;
    case 5:
      if (*pa != *pb) return *pa < *pb ? -1 : +1;
      ++pa;
      ++pb;
      FALLTHROUGH_INTENDED;
    case 4:
      if (*pa != *pb) return *pa < *pb ? -1 : +1;
      ++pa;
      ++pb;
      FALLTHROUGH_INTENDED;
    case 3:
      if (*pa != *pb) return *pa < *pb ? -1 : +1;
      ++pa;
      ++pb;
      FALLTHROUGH_INTENDED;
    case 2:
      if (*pa != *pb) return *pa < *pb ? -1 : +1;
      ++pa;
      ++pb;
      FALLTHROUGH_INTENDED;
    case 1:
      if (*pa != *pb) return *pa < *pb ? -1 : +1;
      FALLTHROUGH_INTENDED;
    case 0:
      break;
  }
  return 0;
}

// The standard memcpy operation is slow for variable small sizes.
// This implementation inlines the optimal realization for sizes 1 to 16.
// To avoid code bloat don't use it in case of not performance-critical spots,
// nor when you don't expect very frequent values of size <= 16.
inline void memcpy_inlined(char* dst, const char* src, size_t size) {
  // Compiler inlines code with minimal amount of data movement when third
  // parameter of memcpy is a constant.
  switch (size) {
    case 1:
      memcpy(dst, src, 1);
      break;
    case 2:
      memcpy(dst, src, 2);
      break;
    case 3:
      memcpy(dst, src, 3);
      break;
    case 4:
      memcpy(dst, src, 4);
      break;
    case 5:
      memcpy(dst, src, 5);
      break;
    case 6:
      memcpy(dst, src, 6);
      break;
    case 7:
      memcpy(dst, src, 7);
      break;
    case 8:
      memcpy(dst, src, 8);
      break;
    case 9:
      memcpy(dst, src, 9);
      break;
    case 10:
      memcpy(dst, src, 10);
      break;
    case 11:
      memcpy(dst, src, 11);
      break;
    case 12:
      memcpy(dst, src, 12);
      break;
    case 13:
      memcpy(dst, src, 13);
      break;
    case 14:
      memcpy(dst, src, 14);
      break;
    case 15:
      memcpy(dst, src, 15);
      break;
    case 16:
      memcpy(dst, src, 16);
      break;
    default:
      memcpy(dst, src, size);
      break;
  }
}

}  // namespace strings
}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INTERNAL_STRINGS_FASTMEM_H_  // NOLINT
