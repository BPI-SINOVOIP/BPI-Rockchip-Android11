// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IORAP_SRC_INODE2FILENAME_INODE_H_
#define IORAP_SRC_INODE2FILENAME_INODE_H_

#include <functional>
#include <ostream>
#include <string>

#include <stddef.h>

namespace iorap::inode2filename {

// Avoid polluting headers.
#if defined(__ANDROID__)
#  if !defined(__LP64__)
/* This historical accident means that we had a 32-bit dev_t on 32-bit architectures. */
using dev_t = uint32_t;
#  else
using dev_t = uint64_t;
#  endif
using ino_t = unsigned long;
#else
#  if !defined(__x86_64__)
using dev_t = unsigned long long;
using ino_t = unsigned long long;
#  else
using dev_t = unsigned long;
using ino_t = unsigned long;
#  endif
#endif

#ifdef makedev
#undef makedev
#endif

/** Combines `major` and `minor` into a device number. */
constexpr inline dev_t makedev(unsigned int major, unsigned int minor) {
  return
      (((major) & 0xfffff000ULL) << 32) | (((major) & 0xfffULL) << 8) |
      (((minor) & 0xffffff00ULL) << 12) | (((minor) & 0xffULL));
}

#ifdef major
#undef major
#endif

/** Extracts the major part of a device number. */
constexpr inline unsigned int major(dev_t dev) {
  return
      ((unsigned) ((((unsigned long long) (dev) >> 32) & 0xfffff000) | (((dev) >> 8) & 0xfff)));
}

#ifdef minor
#undef minor
#endif

/** Extracts the minor part of a device number. */
constexpr inline unsigned int minor(dev_t dev) {
  return
      ((unsigned) ((((dev) >> 12) & 0xffffff00) | ((dev) & 0xff)));
};
// Note: above definitions copied from sysmacros.h, to avoid polluting global namespace in a header.

/*
 * A convenient datum representing a (dev_t, ino_t) tuple.
 *
 * ino_t values may be reused across different devices (e.g. different partitions),
 * so we need the full tuple to uniquely identify an inode on a system.
 */
struct Inode {
  size_t device_major;  // dev_t = makedev(major, minor)
  size_t device_minor;
  size_t inode;         // ino_t = inode

  // "Major:minor:inode" OR "dev_t@inode"
  static bool Parse(const std::string& str, /*out*/Inode* out, /*out*/std::string* error_msg);

  constexpr bool operator==(const Inode& rhs) const {
    return device_major == rhs.device_major &&
        device_minor == rhs.device_minor &&
        inode == rhs.inode;
  }

  constexpr bool operator!=(const Inode& rhs) const {
    return !(*this == rhs);
  }

  Inode() = default;
  constexpr Inode(size_t device_major, size_t device_minor, size_t inode)
    : device_major{device_major}, device_minor{device_minor}, inode{inode} {
  }

  static constexpr Inode FromDeviceAndInode(dev_t dev, ino_t inode) {
    return Inode{major(dev), minor(dev), static_cast<size_t>(inode)};
  }

  constexpr dev_t GetDevice() const {
    return makedev(device_major, device_minor);
  }

  constexpr ino_t GetInode() const {
    return static_cast<ino_t>(inode);
  }
};

inline std::ostream& operator<<(std::ostream& os, const Inode& inode) {
  os << inode.device_major << ":" << inode.device_minor << ":" << inode.inode;
  return os;
}

}  // namespace iorap::inode2filename

namespace std {
  template <>
  struct hash<iorap::inode2filename::Inode> {
      using argument_type = iorap::inode2filename::Inode;
      using result_type = size_t;
      result_type operator()(argument_type const& s) const noexcept {
        // Hash the inode by using only the inode#. Ignore devices, we are extremely unlikely
        // to ever collide on the devices.
        result_type const h1 = std::hash<size_t>{}(s.inode);
        return h1;
      }
  };
}  // namespace std

namespace rxcpp {
template <class T, typename>
struct filtered_hash;

// support for the 'distinct' rx operator.
template <>
struct filtered_hash<iorap::inode2filename::Inode, void> : std::hash<iorap::inode2filename::Inode> {
};
}  // namespace rxcpp

#endif  // IORAP_SRC_INODE2FILENAME_INODE_H_
