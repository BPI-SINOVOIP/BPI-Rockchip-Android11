// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Note: ported from Chromium commit head: 602bc8fa60fa
// Note: only necessary functions are ported.
// Note: some shared memory-related functionality here is no longer present in
// Chromium.

#include "video_frame.h"

#include <algorithm>
#include <climits>
#include <limits>
#include <numeric>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media_limits.h"

namespace media {

// Note: moved from Chromium media/base/timestamp_constants.h
// Indicates an invalid or missing timestamp.
constexpr base::TimeDelta kNoTimestamp =
    base::TimeDelta::FromMicroseconds(std::numeric_limits<int64_t>::min());

namespace {

// Helper to provide Rect::Intersect() as an expression.
Rect Intersection(Rect a, const Rect& b) {
  a.Intersect(b);
  return a;
}

// Note: moved from Chromium base/bits.h which is not included in libchrome.
// Round down |size| to a multiple of alignment, which must be a power of two.
size_t AlignDown(size_t size, size_t alignment) {
  DCHECK(base::bits::IsPowerOfTwo(alignment));
  return size & ~(alignment - 1);
}

}  // namespace

// Static constexpr class for generating unique identifiers for each VideoFrame.
static base::AtomicSequenceNumber g_unique_id_generator;

static std::string StorageTypeToString(
    const VideoFrame::StorageType storage_type) {
  switch (storage_type) {
    case VideoFrame::STORAGE_UNKNOWN:
      return "UNKNOWN";
    case VideoFrame::STORAGE_OPAQUE:
      return "OPAQUE";
    case VideoFrame::STORAGE_UNOWNED_MEMORY:
      return "UNOWNED_MEMORY";
    case VideoFrame::STORAGE_OWNED_MEMORY:
      return "OWNED_MEMORY";
    case VideoFrame::STORAGE_SHMEM:
      return "SHMEM";
    case VideoFrame::STORAGE_DMABUFS:
      return "DMABUFS";
    case VideoFrame::STORAGE_MOJO_SHARED_BUFFER:
      return "MOJO_SHARED_BUFFER";
  }

  NOTREACHED() << "Invalid StorageType provided: " << storage_type;
  return "INVALID";
}

// static
bool VideoFrame::IsStorageTypeMappable(VideoFrame::StorageType storage_type) {
  return
      // This is not strictly needed but makes explicit that, at VideoFrame
      // level, DmaBufs are not mappable from userspace.
      storage_type != VideoFrame::STORAGE_DMABUFS &&
      (storage_type == VideoFrame::STORAGE_UNOWNED_MEMORY ||
       storage_type == VideoFrame::STORAGE_OWNED_MEMORY ||
       storage_type == VideoFrame::STORAGE_SHMEM ||
       storage_type == VideoFrame::STORAGE_MOJO_SHARED_BUFFER);
}

// If it is required to allocate aligned to multiple-of-two size overall for the
// frame of pixel |format|.
static bool RequiresEvenSizeAllocation(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_BGRA:
      return false;
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_P016LE:
      return true;
    case PIXEL_FORMAT_UNKNOWN:
      break;
  }
  NOTREACHED() << "Unsupported video frame format: " << format;
  return false;
}

// Creates VideoFrameLayout for tightly packed frame.
static base::Optional<VideoFrameLayout> GetDefaultLayout(
    VideoPixelFormat format,
    const Size& coded_size) {
  std::vector<ColorPlaneLayout> planes;

  switch (format) {
    case PIXEL_FORMAT_I420: {
      int uv_width = (coded_size.width() + 1) / 2;
      int uv_height = (coded_size.height() + 1) / 2;
      int uv_stride = uv_width;
      int uv_size = uv_stride * uv_height;
      planes = std::vector<ColorPlaneLayout>{
          ColorPlaneLayout(coded_size.width(), 0, coded_size.GetArea()),
          ColorPlaneLayout(uv_stride, coded_size.GetArea(), uv_size),
          ColorPlaneLayout(uv_stride, coded_size.GetArea() + uv_size, uv_size),
      };
      break;
    }

    case PIXEL_FORMAT_Y16:
      planes = std::vector<ColorPlaneLayout>{ColorPlaneLayout(
          coded_size.width() * 2, 0, coded_size.GetArea() * 2)};
      break;

    case PIXEL_FORMAT_ARGB:
      planes = std::vector<ColorPlaneLayout>{ColorPlaneLayout(
          coded_size.width() * 4, 0, coded_size.GetArea() * 4)};
      break;

    case PIXEL_FORMAT_NV12: {
      int uv_width = (coded_size.width() + 1) / 2;
      int uv_height = (coded_size.height() + 1) / 2;
      int uv_stride = uv_width * 2;
      int uv_size = uv_stride * uv_height;
      planes = std::vector<ColorPlaneLayout>{
          ColorPlaneLayout(coded_size.width(), 0, coded_size.GetArea()),
          ColorPlaneLayout(uv_stride, coded_size.GetArea(), uv_size),
      };
      break;
    }

    default:
      // TODO(miu): This function should support any pixel format.
      // http://crbug.com/555909 .
      DLOG(ERROR)
          << "Only PIXEL_FORMAT_I420, PIXEL_FORMAT_Y16, PIXEL_FORMAT_NV12, "
             "and PIXEL_FORMAT_ARGB formats are supported: "
          << VideoPixelFormatToString(format);
      return base::nullopt;
  }

  return VideoFrameLayout::CreateWithPlanes(format, coded_size, planes);
}

// static
bool VideoFrame::IsValidConfig(VideoPixelFormat format,
                               StorageType storage_type,
                               const Size& coded_size,
                               const Rect& visible_rect,
                               const Size& natural_size) {
  // Check maximum limits for all formats.
  int coded_size_area = coded_size.GetCheckedArea().ValueOrDefault(INT_MAX);
  int natural_size_area = natural_size.GetCheckedArea().ValueOrDefault(INT_MAX);
  static_assert(limits::kMaxCanvas < INT_MAX, "");
  if (coded_size_area > limits::kMaxCanvas ||
      coded_size.width() > limits::kMaxDimension ||
      coded_size.height() > limits::kMaxDimension || visible_rect.x() < 0 ||
      visible_rect.y() < 0 || visible_rect.right() > coded_size.width() ||
      visible_rect.bottom() > coded_size.height() ||
      natural_size_area > limits::kMaxCanvas ||
      natural_size.width() > limits::kMaxDimension ||
      natural_size.height() > limits::kMaxDimension) {
    return false;
  }

  // TODO(mcasas): Remove parameter |storage_type| when the opaque storage types
  // comply with the checks below. Right now we skip them.
  if (!IsStorageTypeMappable(storage_type))
    return true;

  // Make sure new formats are properly accounted for in the method.
  static_assert(PIXEL_FORMAT_MAX == 32,
                "Added pixel format, please review IsValidConfig()");

  if (format == PIXEL_FORMAT_UNKNOWN) {
    return coded_size.IsEmpty() && visible_rect.IsEmpty() &&
           natural_size.IsEmpty();
  }

  // Check that software-allocated buffer formats are not empty.
  return !coded_size.IsEmpty() && !visible_rect.IsEmpty() &&
         !natural_size.IsEmpty();
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrame(VideoPixelFormat format,
                                                  const Size& coded_size,
                                                  const Rect& visible_rect,
                                                  const Size& natural_size,
                                                  base::TimeDelta timestamp) {
  return CreateFrameInternal(format, coded_size, visible_rect, natural_size,
                             timestamp, false);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalSharedMemory(
    VideoPixelFormat format,
    const Size& coded_size,
    const Rect& visible_rect,
    const Size& natural_size,
    uint8_t* data,
    size_t data_size,
    base::SharedMemoryHandle handle,
    size_t data_offset,
    base::TimeDelta timestamp) {
  auto layout = GetDefaultLayout(format, coded_size);
  if (!layout)
    return nullptr;
  return WrapExternalStorage(STORAGE_SHMEM, *layout, visible_rect, natural_size,
                             data, data_size, timestamp, nullptr, nullptr,
                             handle, data_offset);
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEOSFrame() {
  auto layout = VideoFrameLayout::Create(PIXEL_FORMAT_UNKNOWN, Size());
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }
  scoped_refptr<VideoFrame> frame =
      new VideoFrame(*layout, STORAGE_UNKNOWN, Rect(), Size(), kNoTimestamp);
  frame->metadata()->SetBoolean(VideoFrameMetadata::END_OF_STREAM, true);
  return frame;
}

// static
size_t VideoFrame::NumPlanes(VideoPixelFormat format) {
  return VideoFrameLayout::NumPlanes(format);
}

// static
size_t VideoFrame::AllocationSize(VideoPixelFormat format,
                                  const Size& coded_size) {
  size_t total = 0;
  for (size_t i = 0; i < NumPlanes(format); ++i)
    total += PlaneSize(format, i, coded_size).GetArea();
  return total;
}

// static
Size VideoFrame::PlaneSize(VideoPixelFormat format,
                           size_t plane,
                           const Size& coded_size) {
  DCHECK(IsValidPlane(plane, format));

  int width = coded_size.width();
  int height = coded_size.height();
  if (RequiresEvenSizeAllocation(format)) {
    // Align to multiple-of-two size overall. This ensures that non-subsampled
    // planes can be addressed by pixel with the same scaling as the subsampled
    // planes.
    width = base::bits::Align(width, 2);
    height = base::bits::Align(height, 2);
  }

  const Size subsample = SampleSize(format, plane);
  DCHECK(width % subsample.width() == 0);
  DCHECK(height % subsample.height() == 0);
  return Size(BytesPerElement(format, plane) * width / subsample.width(),
              height / subsample.height());
}

// static
int VideoFrame::PlaneHorizontalBitsPerPixel(VideoPixelFormat format,
                                            size_t plane) {
  DCHECK(IsValidPlane(plane, format));
  const int bits_per_element = 8 * BytesPerElement(format, plane);
  const int horiz_pixels_per_element = SampleSize(format, plane).width();
  DCHECK_EQ(bits_per_element % horiz_pixels_per_element, 0);
  return bits_per_element / horiz_pixels_per_element;
}

// static
int VideoFrame::PlaneBitsPerPixel(VideoPixelFormat format, size_t plane) {
  DCHECK(IsValidPlane(plane, format));
  return PlaneHorizontalBitsPerPixel(format, plane) /
         SampleSize(format, plane).height();
}

// static
size_t VideoFrame::RowBytes(size_t plane, VideoPixelFormat format, int width) {
  DCHECK(IsValidPlane(plane, format));
  return BytesPerElement(format, plane) * Columns(plane, format, width);
}

// static
int VideoFrame::BytesPerElement(VideoPixelFormat format, size_t plane) {
  DCHECK(IsValidPlane(format, plane));
  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
      return 4;
    case PIXEL_FORMAT_RGB24:
      return 3;
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_P016LE:
      return 2;
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21: {
      static const int bytes_per_element[] = {1, 2};
      DCHECK_LT(plane, base::size(bytes_per_element));
      return bytes_per_element[plane];
    }
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
      return 1;
    case PIXEL_FORMAT_MJPEG:
      return 0;
    case PIXEL_FORMAT_UNKNOWN:
      break;
  }
  NOTREACHED();
  return 0;
}

// static
std::vector<int32_t> VideoFrame::ComputeStrides(VideoPixelFormat format,
                                                const Size& coded_size) {
  std::vector<int32_t> strides;
  const size_t num_planes = NumPlanes(format);
  if (num_planes == 1) {
    strides.push_back(RowBytes(0, format, coded_size.width()));
  } else {
    for (size_t plane = 0; plane < num_planes; ++plane) {
      strides.push_back(base::bits::Align(
          RowBytes(plane, format, coded_size.width()), kFrameAddressAlignment));
    }
  }
  return strides;
}

// static
size_t VideoFrame::Rows(size_t plane, VideoPixelFormat format, int height) {
  DCHECK(IsValidPlane(plane, format));
  const int sample_height = SampleSize(format, plane).height();
  return base::bits::Align(height, sample_height) / sample_height;
}

// static
size_t VideoFrame::Columns(size_t plane, VideoPixelFormat format, int width) {
  DCHECK(IsValidPlane(plane, format));
  const int sample_width = SampleSize(format, plane).width();
  return base::bits::Align(width, sample_width) / sample_width;
}

bool VideoFrame::IsMappable() const {
  return IsStorageTypeMappable(storage_type_);
}

int VideoFrame::row_bytes(size_t plane) const {
  return RowBytes(plane, format(), coded_size().width());
}

int VideoFrame::rows(size_t plane) const {
  return Rows(plane, format(), coded_size().height());
}

const uint8_t* VideoFrame::visible_data(size_t plane) const {
  DCHECK(IsValidPlane(plane, format()));
  DCHECK(IsMappable());

  // Calculate an offset that is properly aligned for all planes.
  const Size alignment = CommonAlignment(format());
  const int offset_x = AlignDown(visible_rect_.x(), alignment.width());
  const int offset_y = AlignDown(visible_rect_.y(), alignment.height());

  const Size subsample = SampleSize(format(), plane);
  DCHECK(offset_x % subsample.width() == 0);
  DCHECK(offset_y % subsample.height() == 0);
  return data(plane) +
         stride(plane) * (offset_y / subsample.height()) +  // Row offset.
         BytesPerElement(format(), plane) *                 // Column offset.
             (offset_x / subsample.width());
}

uint8_t* VideoFrame::visible_data(size_t plane) {
  return const_cast<uint8_t*>(
      static_cast<const VideoFrame*>(this)->visible_data(plane));
}

base::ReadOnlySharedMemoryRegion* VideoFrame::read_only_shared_memory_region()
    const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK(read_only_shared_memory_region_ &&
         read_only_shared_memory_region_->IsValid());
  return read_only_shared_memory_region_;
}

base::UnsafeSharedMemoryRegion* VideoFrame::unsafe_shared_memory_region()
    const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK(unsafe_shared_memory_region_ &&
         unsafe_shared_memory_region_->IsValid());
  return unsafe_shared_memory_region_;
}

base::SharedMemoryHandle VideoFrame::shared_memory_handle() const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK(shared_memory_handle_.IsValid());
  return shared_memory_handle_;
}

size_t VideoFrame::shared_memory_offset() const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK((read_only_shared_memory_region_ &&
          read_only_shared_memory_region_->IsValid()) ||
         (unsafe_shared_memory_region_ &&
          unsafe_shared_memory_region_->IsValid()) ||
         shared_memory_handle_.IsValid());
  return shared_memory_offset_;
}

const std::vector<base::ScopedFD>& VideoFrame::DmabufFds() const {
  DCHECK_EQ(storage_type_, STORAGE_DMABUFS);

  return dmabuf_fds_;
}

bool VideoFrame::HasDmaBufs() const {
  return !dmabuf_fds_.empty();
}

void VideoFrame::AddReadOnlySharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion* region) {
  storage_type_ = STORAGE_SHMEM;
  DCHECK(SharedMemoryUninitialized());
  DCHECK(region && region->IsValid());
  read_only_shared_memory_region_ = region;
}

void VideoFrame::AddUnsafeSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion* region) {
  storage_type_ = STORAGE_SHMEM;
  DCHECK(SharedMemoryUninitialized());
  DCHECK(region && region->IsValid());
  unsafe_shared_memory_region_ = region;
}

void VideoFrame::AddSharedMemoryHandle(base::SharedMemoryHandle handle) {
  storage_type_ = STORAGE_SHMEM;
  DCHECK(SharedMemoryUninitialized());
  shared_memory_handle_ = handle;
}

void VideoFrame::AddDestructionObserver(base::OnceClosure callback) {
  DCHECK(!callback.is_null());
  done_callbacks_.push_back(std::move(callback));
}

std::string VideoFrame::AsHumanReadableString() {
  if (metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM))
    return "end of stream";

  std::ostringstream s;
  s << ConfigToString(format(), storage_type_, coded_size(), visible_rect_,
                      natural_size_)
    << " timestamp:" << timestamp_.InMicroseconds();
  return s.str();
}

size_t VideoFrame::BitDepth() const {
  return media::BitDepth(format());
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalStorage(
    StorageType storage_type,
    const VideoFrameLayout& layout,
    const Rect& visible_rect,
    const Size& natural_size,
    uint8_t* data,
    size_t data_size,
    base::TimeDelta timestamp,
    base::ReadOnlySharedMemoryRegion* read_only_region,
    base::UnsafeSharedMemoryRegion* unsafe_region,
    base::SharedMemoryHandle handle,
    size_t data_offset) {
  DCHECK(IsStorageTypeMappable(storage_type));

  if (!IsValidConfig(layout.format(), storage_type, layout.coded_size(),
                     visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(layout.format(), storage_type,
                                  layout.coded_size(), visible_rect,
                                  natural_size);
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame = new VideoFrame(
      layout, storage_type, visible_rect, natural_size, timestamp);

  for (size_t i = 0; i < layout.planes().size(); ++i) {
    frame->data_[i] = data + layout.planes()[i].offset;
  }

  if (storage_type == STORAGE_SHMEM) {
    if (read_only_region || unsafe_region) {
      DCHECK(!handle.IsValid());
      DCHECK_NE(!!read_only_region, !!unsafe_region)
          << "Expected exactly one read-only or unsafe region for "
          << "STORAGE_SHMEM VideoFrame";
      if (read_only_region) {
        frame->read_only_shared_memory_region_ = read_only_region;
        DCHECK(frame->read_only_shared_memory_region_->IsValid());
      } else if (unsafe_region) {
        frame->unsafe_shared_memory_region_ = unsafe_region;
        DCHECK(frame->unsafe_shared_memory_region_->IsValid());
      }
      frame->shared_memory_offset_ = data_offset;
    } else {
      frame->AddSharedMemoryHandle(handle);
      frame->shared_memory_offset_ = data_offset;
    }
  }

  return frame;
}

VideoFrame::VideoFrame(const VideoFrameLayout& layout,
                       StorageType storage_type,
                       const Rect& visible_rect,
                       const Size& natural_size,
                       base::TimeDelta timestamp)
    : layout_(layout),
      storage_type_(storage_type),
      visible_rect_(Intersection(visible_rect, Rect(layout.coded_size()))),
      natural_size_(natural_size),
      shared_memory_offset_(0),
      timestamp_(timestamp),
      unique_id_(g_unique_id_generator.GetNext()) {
  DCHECK(IsValidConfig(format(), storage_type, coded_size(), visible_rect_,
                       natural_size_));
  DCHECK(visible_rect_ == visible_rect)
      << "visible_rect " << visible_rect.ToString() << " exceeds coded_size "
      << coded_size().ToString();
  memset(&data_, 0, sizeof(data_));
}

VideoFrame::~VideoFrame() {
  for (auto& callback : done_callbacks_)
    std::move(callback).Run();
}

// static
std::string VideoFrame::ConfigToString(const VideoPixelFormat format,
                                       const StorageType storage_type,
                                       const Size& coded_size,
                                       const Rect& visible_rect,
                                       const Size& natural_size) {
  return base::StringPrintf(
      "format:%s storage_type:%s coded_size:%s visible_rect:%s natural_size:%s",
      VideoPixelFormatToString(format).c_str(),
      StorageTypeToString(storage_type).c_str(), coded_size.ToString().c_str(),
      visible_rect.ToString().c_str(), natural_size.ToString().c_str());
}

// static
bool VideoFrame::IsValidPlane(size_t plane, VideoPixelFormat format) {
  DCHECK_LE(NumPlanes(format), static_cast<size_t>(kMaxPlanes));
  return (plane < NumPlanes(format));
}

// static
Size VideoFrame::DetermineAlignedSize(VideoPixelFormat format,
                                      const Size& dimensions) {
  const Size alignment = CommonAlignment(format);
  const Size adjusted =
      Size(base::bits::Align(dimensions.width(), alignment.width()),
           base::bits::Align(dimensions.height(), alignment.height()));
  DCHECK((adjusted.width() % alignment.width() == 0) &&
         (adjusted.height() % alignment.height() == 0));
  return adjusted;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrameInternal(
    VideoPixelFormat format,
    const Size& coded_size,
    const Rect& visible_rect,
    const Size& natural_size,
    base::TimeDelta timestamp,
    bool zero_initialize_memory) {
  // Since we're creating a new frame (and allocating memory for it ourselves),
  // we can pad the requested |coded_size| if necessary if the request does not
  // line up on sample boundaries. See discussion at http://crrev.com/1240833003
  const Size new_coded_size = DetermineAlignedSize(format, coded_size);
  auto layout = VideoFrameLayout::CreateWithStrides(
      format, new_coded_size, ComputeStrides(format, coded_size));
  if (!layout) {
    DLOG(ERROR) << "Invalid layout.";
    return nullptr;
  }

  return CreateFrameWithLayout(*layout, visible_rect, natural_size, timestamp,
                               zero_initialize_memory);
}

scoped_refptr<VideoFrame> VideoFrame::CreateFrameWithLayout(
    const VideoFrameLayout& layout,
    const Rect& visible_rect,
    const Size& natural_size,
    base::TimeDelta timestamp,
    bool zero_initialize_memory) {
  const StorageType storage = STORAGE_OWNED_MEMORY;
  if (!IsValidConfig(layout.format(), storage, layout.coded_size(),
                     visible_rect, natural_size)) {
    DLOG(ERROR) << __func__ << " Invalid config."
                << ConfigToString(layout.format(), storage, layout.coded_size(),
                                  visible_rect, natural_size);
    return nullptr;
  }

  scoped_refptr<VideoFrame> frame(new VideoFrame(
      std::move(layout), storage, visible_rect, natural_size, timestamp));
  frame->AllocateMemory(zero_initialize_memory);
  return frame;
}

bool VideoFrame::SharedMemoryUninitialized() {
  return !read_only_shared_memory_region_ && !unsafe_shared_memory_region_ &&
         !shared_memory_handle_.IsValid();
}

// static
bool VideoFrame::IsValidPlane(VideoPixelFormat format, size_t plane) {
  DCHECK_LE(NumPlanes(format), static_cast<size_t>(kMaxPlanes));
  return plane < NumPlanes(format);
}

// static
Size VideoFrame::SampleSize(VideoPixelFormat format, size_t plane) {
  DCHECK(IsValidPlane(format, plane));

  switch (plane) {
    case kYPlane:  // and kARGBPlane:
    case kAPlane:
      return Size(1, 1);

    case kUPlane:  // and kUVPlane:
    case kVPlane:
      switch (format) {
        case PIXEL_FORMAT_I444:
        case PIXEL_FORMAT_YUV444P9:
        case PIXEL_FORMAT_YUV444P10:
        case PIXEL_FORMAT_YUV444P12:
        case PIXEL_FORMAT_Y16:
          return Size(1, 1);

        case PIXEL_FORMAT_I422:
        case PIXEL_FORMAT_YUV422P9:
        case PIXEL_FORMAT_YUV422P10:
        case PIXEL_FORMAT_YUV422P12:
          return Size(2, 1);

        case PIXEL_FORMAT_YV12:
        case PIXEL_FORMAT_I420:
        case PIXEL_FORMAT_I420A:
        case PIXEL_FORMAT_NV12:
        case PIXEL_FORMAT_NV21:
        case PIXEL_FORMAT_YUV420P9:
        case PIXEL_FORMAT_YUV420P10:
        case PIXEL_FORMAT_YUV420P12:
        case PIXEL_FORMAT_P016LE:
          return Size(2, 2);

        case PIXEL_FORMAT_UNKNOWN:
        case PIXEL_FORMAT_YUY2:
        case PIXEL_FORMAT_ARGB:
        case PIXEL_FORMAT_XRGB:
        case PIXEL_FORMAT_RGB24:
        case PIXEL_FORMAT_MJPEG:
        case PIXEL_FORMAT_ABGR:
        case PIXEL_FORMAT_XBGR:
        case PIXEL_FORMAT_XR30:
        case PIXEL_FORMAT_XB30:
        case PIXEL_FORMAT_BGRA:
          break;
      }
  }
  NOTREACHED();
  return Size();
}

// static
Size VideoFrame::CommonAlignment(VideoPixelFormat format) {
  int max_sample_width = 0;
  int max_sample_height = 0;
  for (size_t plane = 0; plane < NumPlanes(format); ++plane) {
    const Size sample_size = SampleSize(format, plane);
    max_sample_width = std::max(max_sample_width, sample_size.width());
    max_sample_height = std::max(max_sample_height, sample_size.height());
  }
  return Size(max_sample_width, max_sample_height);
}

void VideoFrame::AllocateMemory(bool zero_initialize_memory) {
  DCHECK_EQ(storage_type_, STORAGE_OWNED_MEMORY);
  static_assert(0 == kYPlane, "y plane data must be index 0");

  std::vector<size_t> plane_size = CalculatePlaneSize();
  const size_t total_buffer_size =
      std::accumulate(plane_size.begin(), plane_size.end(), 0u);

  uint8_t* data = reinterpret_cast<uint8_t*>(
      base::AlignedAlloc(total_buffer_size, layout_.buffer_addr_align()));
  if (zero_initialize_memory) {
    memset(data, 0, total_buffer_size);
  }
  AddDestructionObserver(base::BindOnce(&base::AlignedFree, data));

  // Note that if layout.buffer_sizes is specified, color planes' layout is the
  // same as buffers'. See CalculatePlaneSize() for detail.
  for (size_t plane = 0, offset = 0; plane < NumPlanes(format()); ++plane) {
    data_[plane] = data + offset;
    offset += plane_size[plane];
  }
}

std::vector<size_t> VideoFrame::CalculatePlaneSize() const {
  // We have two cases for plane size mapping:
  // 1) If plane size is specified: use planes' size.
  // 2) VideoFrameLayout::size is unassigned: use legacy calculation formula.

  const size_t num_planes = NumPlanes(format());
  const auto& planes = layout_.planes();
  std::vector<size_t> plane_size(num_planes);
  bool plane_size_assigned = true;
  DCHECK_EQ(planes.size(), num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    plane_size[i] = planes[i].size;
    plane_size_assigned &= plane_size[i] != 0;
  }

  if (plane_size_assigned)
    return plane_size;

  // Reset plane size.
  std::fill(plane_size.begin(), plane_size.end(), 0u);
  for (size_t plane = 0; plane < num_planes; ++plane) {
    // These values were chosen to mirror ffmpeg's get_video_buffer().
    // TODO(dalecurtis): This should be configurable; eventually ffmpeg wants
    // us to use av_cpu_max_align(), but... for now, they just hard-code 32.
    const size_t height =
        base::bits::Align(rows(plane), kFrameAddressAlignment);
    const size_t width = std::abs(stride(plane));
    plane_size[plane] = width * height;
  }

  if (num_planes > 1) {
    // The extra line of UV being allocated is because h264 chroma MC
    // overreads by one line in some cases, see libavcodec/utils.c:
    // avcodec_align_dimensions2() and libavcodec/x86/h264_chromamc.asm:
    // put_h264_chroma_mc4_ssse3().
    DCHECK(IsValidPlane(format(), kUPlane));
    plane_size.back() += std::abs(stride(kUPlane)) + kFrameSizePadding;
  }
  return plane_size;
}

}  // namespace media
