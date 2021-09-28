// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_DBUS_FILE_DESCRIPTOR_H_
#define LIBBRILLO_BRILLO_DBUS_FILE_DESCRIPTOR_H_

#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace brillo {
namespace dbus_utils {

// This struct wraps file descriptors to give them a type other than int.
// Implicit conversions are provided because this should be as transparent
// a wrapper as possible to match the libchrome bindings below when this
// class is used by chromeos-dbus-bindings.
//
// Because we might pass these around and the calling code neither passes
// ownership nor knows when this will be destroyed, it actually dups the FD
// so that the calling code and binding code both have a clear handle on the
// lifetimes of their respective copies of the FD.
struct FileDescriptor {
  FileDescriptor() = default;
  FileDescriptor(int fd) : fd(dup(fd)) {}
  FileDescriptor(FileDescriptor&& other) : fd(std::move(other.fd)) {}
  FileDescriptor(base::ScopedFD&& other) : fd(std::move(other)) {}

  inline FileDescriptor& operator=(int new_fd) {
    fd.reset(dup(new_fd));
    return *this;
  }

  FileDescriptor& operator=(FileDescriptor&& other) {
    fd = std::move(other.fd);
    return *this;
  }

  FileDescriptor& operator=(base::ScopedFD&& other) {
    fd = std::move(other);
    return *this;
  }

  int release() { return fd.release(); }

  int get() const { return fd.get(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(FileDescriptor);

  base::ScopedFD fd;
};

}  // namespace dbus_utils
}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_DBUS_FILE_DESCRIPTOR_H_
