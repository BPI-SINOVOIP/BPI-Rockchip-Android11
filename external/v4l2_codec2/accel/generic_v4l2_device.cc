// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Note: ported from Chromium commit head: 8c9190713ed9

#include "generic_v4l2_device.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <algorithm>
#include <memory>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"

#include "macros.h"

namespace media {

GenericV4L2Device::GenericV4L2Device() {}

GenericV4L2Device::~GenericV4L2Device() {
  CloseDevice();
}

int GenericV4L2Device::Ioctl(int request, void* arg) {
  DCHECK(device_fd_.is_valid());
  return HANDLE_EINTR(ioctl(device_fd_.get(), request, arg));
}

bool GenericV4L2Device::Poll(bool poll_device, bool* event_pending) {
  struct pollfd pollfds[2];
  nfds_t nfds;
  int pollfd = -1;

  pollfds[0].fd = device_poll_interrupt_fd_.get();
  pollfds[0].events = POLLIN | POLLERR;
  nfds = 1;

  if (poll_device) {
    DVLOGF(5) << "adding device fd to poll() set";
    pollfds[nfds].fd = device_fd_.get();
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR | POLLPRI;
    pollfd = nfds;
    nfds++;
  }

  if (HANDLE_EINTR(poll(pollfds, nfds, -1)) == -1) {
    VPLOGF(1) << "poll() failed";
    return false;
  }
  *event_pending = (pollfd != -1 && pollfds[pollfd].revents & POLLPRI);
  return true;
}

void* GenericV4L2Device::Mmap(void* addr,
                              unsigned int len,
                              int prot,
                              int flags,
                              unsigned int offset) {
  DCHECK(device_fd_.is_valid());
  return mmap(addr, len, prot, flags, device_fd_.get(), offset);
}

void GenericV4L2Device::Munmap(void* addr, unsigned int len) {
  munmap(addr, len);
}

bool GenericV4L2Device::SetDevicePollInterrupt() {
  DVLOGF(4);

  const uint64_t buf = 1;
  if (HANDLE_EINTR(write(device_poll_interrupt_fd_.get(), &buf, sizeof(buf))) ==
      -1) {
    VPLOGF(1) << "write() failed";
    return false;
  }
  return true;
}

bool GenericV4L2Device::ClearDevicePollInterrupt() {
  DVLOGF(5);

  uint64_t buf;
  if (HANDLE_EINTR(read(device_poll_interrupt_fd_.get(), &buf, sizeof(buf))) ==
      -1) {
    if (errno == EAGAIN) {
      // No interrupt flag set, and we're reading nonblocking.  Not an error.
      return true;
    } else {
      VPLOGF(1) << "read() failed";
      return false;
    }
  }
  return true;
}

bool GenericV4L2Device::Initialize() {
  DVLOGF(3);
  static bool v4l2_functions_initialized = PostSandboxInitialization();
  if (!v4l2_functions_initialized) {
    VLOGF(1) << "Failed to initialize LIBV4L2 libs";
    return false;
  }

  return true;
}

bool GenericV4L2Device::Open(Type type, uint32_t v4l2_pixfmt) {
  DVLOGF(3);
  std::string path = GetDevicePathFor(type, v4l2_pixfmt);

  if (path.empty()) {
    VLOGF(1) << "No devices supporting " << FourccToString(v4l2_pixfmt)
             << " for type: " << static_cast<int>(type);
    return false;
  }

  if (!OpenDevicePath(path, type)) {
    VLOGF(1) << "Failed opening " << path;
    return false;
  }

  device_poll_interrupt_fd_.reset(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
  if (!device_poll_interrupt_fd_.is_valid()) {
    VLOGF(1) << "Failed creating a poll interrupt fd";
    return false;
  }

  return true;
}

std::vector<base::ScopedFD> GenericV4L2Device::GetDmabufsForV4L2Buffer(
    int index,
    size_t num_planes,
    enum v4l2_buf_type buf_type) {
  DVLOGF(3);
  DCHECK(V4L2_TYPE_IS_MULTIPLANAR(buf_type));

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < num_planes; ++i) {
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type = buf_type;
    expbuf.index = index;
    expbuf.plane = i;
    expbuf.flags = O_CLOEXEC;
    if (Ioctl(VIDIOC_EXPBUF, &expbuf) != 0) {
      dmabuf_fds.clear();
      break;
    }

    dmabuf_fds.push_back(base::ScopedFD(expbuf.fd));
  }

  return dmabuf_fds;
}

std::vector<uint32_t> GenericV4L2Device::PreferredInputFormat(Type type) {
  if (type == Type::kEncoder)
    return {V4L2_PIX_FMT_NV12M, V4L2_PIX_FMT_NV12};

  return {};
}

std::vector<uint32_t> GenericV4L2Device::GetSupportedImageProcessorPixelformats(
    v4l2_buf_type buf_type) {
  std::vector<uint32_t> supported_pixelformats;

  Type type = Type::kImageProcessor;
  const auto& devices = GetDevicesForType(type);
  for (const auto& device : devices) {
    if (!OpenDevicePath(device.first, type)) {
      VLOGF(1) << "Failed opening " << device.first;
      continue;
    }

    std::vector<uint32_t> pixelformats =
        EnumerateSupportedPixelformats(buf_type);

    supported_pixelformats.insert(supported_pixelformats.end(),
                                  pixelformats.begin(), pixelformats.end());
    CloseDevice();
  }

  return supported_pixelformats;
}

VideoDecodeAccelerator::SupportedProfiles
GenericV4L2Device::GetSupportedDecodeProfiles(const size_t num_formats,
                                              const uint32_t pixelformats[]) {
  VideoDecodeAccelerator::SupportedProfiles supported_profiles;

  Type type = Type::kDecoder;
  const auto& devices = GetDevicesForType(type);
  for (const auto& device : devices) {
    if (!OpenDevicePath(device.first, type)) {
      VLOGF(1) << "Failed opening " << device.first;
      continue;
    }

    const auto& profiles =
        EnumerateSupportedDecodeProfiles(num_formats, pixelformats);
    supported_profiles.insert(supported_profiles.end(), profiles.begin(),
                              profiles.end());
    CloseDevice();
  }

  return supported_profiles;
}

VideoEncodeAccelerator::SupportedProfiles
GenericV4L2Device::GetSupportedEncodeProfiles() {
  VideoEncodeAccelerator::SupportedProfiles supported_profiles;

  Type type = Type::kEncoder;
  const auto& devices = GetDevicesForType(type);
  for (const auto& device : devices) {
    if (!OpenDevicePath(device.first, type)) {
      VLOGF(1) << "Failed opening " << device.first;
      continue;
    }

    const auto& profiles = EnumerateSupportedEncodeProfiles();
    supported_profiles.insert(supported_profiles.end(), profiles.begin(),
                              profiles.end());
    CloseDevice();
  }

  return supported_profiles;
}

bool GenericV4L2Device::IsImageProcessingSupported() {
  const auto& devices = GetDevicesForType(Type::kImageProcessor);
  return !devices.empty();
}

bool GenericV4L2Device::IsJpegDecodingSupported() {
  const auto& devices = GetDevicesForType(Type::kJpegDecoder);
  return !devices.empty();
}

bool GenericV4L2Device::IsJpegEncodingSupported() {
  const auto& devices = GetDevicesForType(Type::kJpegEncoder);
  return !devices.empty();
}

bool GenericV4L2Device::OpenDevicePath(const std::string& path, Type type) {
  DCHECK(!device_fd_.is_valid());

  device_fd_.reset(
      HANDLE_EINTR(open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC)));
  if (!device_fd_.is_valid())
    return false;

  return true;
}

void GenericV4L2Device::CloseDevice() {
  DVLOGF(3);
  device_fd_.reset();
}

// static
bool GenericV4L2Device::PostSandboxInitialization() {
  return true;
}

void GenericV4L2Device::EnumerateDevicesForType(Type type) {
  // video input/output devices are registered as /dev/videoX in V4L2.
  static const std::string kVideoDevicePattern = "/dev/video";

  std::string device_pattern;
  v4l2_buf_type buf_type;
  switch (type) {
    case Type::kDecoder:
      device_pattern = kVideoDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      break;
    case Type::kEncoder:
      device_pattern = kVideoDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      break;
    default:
      LOG(ERROR) << "Only decoder and encoder types are supported!!";
      return;
  }

  std::vector<std::string> candidate_paths;

  // TODO(posciak): Remove this legacy unnumbered device once
  // all platforms are updated to use numbered devices.
  candidate_paths.push_back(device_pattern);

  // We are sandboxed, so we can't query directory contents to check which
  // devices are actually available. Try to open the first 10; if not present,
  // we will just fail to open immediately.
  for (int i = 0; i < 10; ++i) {
    candidate_paths.push_back(
        base::StringPrintf("%s%d", device_pattern.c_str(), i));
  }

  Devices devices;
  for (const auto& path : candidate_paths) {
    if (!OpenDevicePath(path, type))
      continue;

    const auto& supported_pixelformats =
        EnumerateSupportedPixelformats(buf_type);
    if (!supported_pixelformats.empty()) {
      DVLOGF(3) << "Found device: " << path;
      devices.push_back(std::make_pair(path, supported_pixelformats));
    }

    CloseDevice();
  }

  DCHECK_EQ(devices_by_type_.count(type), 0u);
  devices_by_type_[type] = devices;
}

const GenericV4L2Device::Devices& GenericV4L2Device::GetDevicesForType(
    Type type) {
  if (devices_by_type_.count(type) == 0)
    EnumerateDevicesForType(type);

  DCHECK_NE(devices_by_type_.count(type), 0u);
  return devices_by_type_[type];
}

std::string GenericV4L2Device::GetDevicePathFor(Type type, uint32_t pixfmt) {
  const Devices& devices = GetDevicesForType(type);

  for (const auto& device : devices) {
    if (std::find(device.second.begin(), device.second.end(), pixfmt) !=
        device.second.end())
      return device.first;
  }

  return std::string();
}

}  //  namespace media
