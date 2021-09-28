// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMPONENTS_BITSTREAMBUFFER_H
#define ANDROID_V4L2_CODEC2_COMPONENTS_BITSTREAMBUFFER_H

#include <stdint.h>

#include <base/files/scoped_file.h>

namespace android {

// The BitstreamBuffer class can be used to store encoded video data.
// Note: The BitstreamBuffer does not take ownership of the data. The file descriptor is not
//       duplicated and the caller is responsible for keeping the data alive.
struct BitstreamBuffer {
    BitstreamBuffer(const int32_t id, int dmabuf_fd, const size_t offset, const size_t size)
          : id(id), dmabuf_fd(dmabuf_fd), offset(offset), size(size) {}
    ~BitstreamBuffer() = default;

    const int32_t id;
    int dmabuf_fd;
    const size_t offset;
    const size_t size;
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMPONENTS_BITSTREAMBUFFER_H
