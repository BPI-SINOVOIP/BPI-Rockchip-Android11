// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Note: ported from Chromium commit head: 602bc8fa60fa
// Note: only necessary functions are ported.
// Note: some OS-specific defines have been removed
// Note: WrapExternalSharedMemory() has been removed in Chromium, but is still
// present here. Porting the code to a newer version of VideoFrame is not
// useful, as this is only a temporary step and all usage of VideoFrame will
// be removed.

#ifndef VIDEO_FRAME_H_
#define VIDEO_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "rect.h"
#include "size.h"
#include "video_frame_layout.h"
#include "video_frame_metadata.h"
#include "video_pixel_format.h"

#include "base/files/scoped_file.h"

namespace media {

class VideoFrame : public base::RefCountedThreadSafe<VideoFrame> {
 public:
  enum {
    kFrameSizeAlignment = 16,
    kFrameSizePadding = 16,

    kFrameAddressAlignment = VideoFrameLayout::kBufferAddressAlignment
  };

  enum {
    kMaxPlanes = 4,

    kYPlane = 0,
    kARGBPlane = kYPlane,
    kUPlane = 1,
    kUVPlane = kUPlane,
    kVPlane = 2,
    kAPlane = 3,
  };

  // Defines the pixel storage type. Differentiates between directly accessible
  // |data_| and pixels that are only indirectly accessible and not via mappable
  // memory.
  // Note that VideoFrames of any StorageType can also have Texture backing,
  // with "classical" GPU Driver-only textures identified as STORAGE_OPAQUE.
  enum StorageType {
    STORAGE_UNKNOWN = 0,
    STORAGE_OPAQUE = 1,  // We don't know how VideoFrame's pixels are stored.
    STORAGE_UNOWNED_MEMORY = 2,  // External, non owned data pointers.
    STORAGE_OWNED_MEMORY = 3,  // VideoFrame has allocated its own data buffer.
    STORAGE_SHMEM = 4,         // Pixels are backed by Shared Memory.
    // TODO(mcasas): Consider turning this type into STORAGE_NATIVE
    // based on the idea of using this same enum value for both DMA
    // buffers on Linux and CVPixelBuffers on Mac (which currently use
    // STORAGE_UNOWNED_MEMORY) and handle it appropriately in all cases.
    STORAGE_DMABUFS = 5,  // Each plane is stored into a DmaBuf.
    STORAGE_MOJO_SHARED_BUFFER = 6,
    STORAGE_LAST = STORAGE_MOJO_SHARED_BUFFER,
  };

  // Call prior to CreateFrame to ensure validity of frame configuration. Called
  // automatically by VideoDecoderConfig::IsValidConfig().
  static bool IsValidConfig(VideoPixelFormat format,
                            StorageType storage_type,
                            const Size& coded_size,
                            const Rect& visible_rect,
                            const Size& natural_size);

  // Creates a new frame in system memory with given parameters. Buffers for the
  // frame are allocated but not initialized. The caller must not make
  // assumptions about the actual underlying size(s), but check the returned
  // VideoFrame instead.
  static scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat format,
                                               const Size& coded_size,
                                               const Rect& visible_rect,
                                               const Size& natural_size,
                                               base::TimeDelta timestamp);

  // Creates a new frame in system memory with given parameters. Buffers for the
  // frame are allocated but not initialized. The caller should specify the
  // physical buffer size and strides if needed in |layout| parameter.
  static scoped_refptr<VideoFrame> CreateFrameWithLayout(
      const VideoFrameLayout& layout,
      const Rect& visible_rect,
      const Size& natural_size,
      base::TimeDelta timestamp,
      bool zero_initialize_memory);

  // Legacy wrapping of old SharedMemoryHandle objects. Deprecated, use one of
  // the shared memory region wrappers above instead.
  static scoped_refptr<VideoFrame> WrapExternalSharedMemory(
      VideoPixelFormat format,
      const Size& coded_size,
      const Rect& visible_rect,
      const Size& natural_size,
      uint8_t* data,
      size_t data_size,
      base::SharedMemoryHandle handle,
      size_t shared_memory_offset,
      base::TimeDelta timestamp);

  // Creates a frame which indicates end-of-stream.
  static scoped_refptr<VideoFrame> CreateEOSFrame();

  static size_t NumPlanes(VideoPixelFormat format);

  // Returns the required allocation size for a (tightly packed) frame of the
  // given coded size and format.
  static size_t AllocationSize(VideoPixelFormat format, const Size& coded_size);

  // Returns the plane Size (in bytes) for a plane of the given coded size
  // and format.
  static Size PlaneSize(VideoPixelFormat format,
                        size_t plane,
                        const Size& coded_size);

  // Returns horizontal bits per pixel for given |plane| and |format|.
  static int PlaneHorizontalBitsPerPixel(VideoPixelFormat format, size_t plane);

  // Returns bits per pixel for given |plane| and |format|.
  static int PlaneBitsPerPixel(VideoPixelFormat format, size_t plane);

  // Returns the number of bytes per row for the given plane, format, and width.
  // The width may be aligned to format requirements.
  static size_t RowBytes(size_t plane, VideoPixelFormat format, int width);

  // Returns the number of bytes per element for given |plane| and |format|.
  static int BytesPerElement(VideoPixelFormat format, size_t plane);

  // Calculates strides for each plane based on |format| and |coded_size|.
  static std::vector<int32_t> ComputeStrides(VideoPixelFormat format,
                                             const Size& coded_size);

  // Returns the number of rows for the given plane, format, and height.
  // The height may be aligned to format requirements.
  static size_t Rows(size_t plane, VideoPixelFormat format, int height);

  // Returns the number of columns for the given plane, format, and width.
  // The width may be aligned to format requirements.
  static size_t Columns(size_t plane, VideoPixelFormat format, int width);

  // Returns true if |frame| is accesible mapped in the VideoFrame memory space.
  // static
  static bool IsStorageTypeMappable(VideoFrame::StorageType storage_type);

  // Returns true if |frame| is accessible and mapped in the VideoFrame memory
  // space. If false, clients should refrain from accessing data(),
  // visible_data() etc.
  bool IsMappable() const;

  const VideoFrameLayout& layout() const { return layout_; }

  VideoPixelFormat format() const { return layout_.format(); }
  StorageType storage_type() const { return storage_type_; }

  // The full dimensions of the video frame data.
  const Size& coded_size() const { return layout_.coded_size(); }
  // A subsection of [0, 0, coded_size().width(), coded_size.height()]. This
  // can be set to "soft-apply" a cropping. It determines the pointers into
  // the data returned by visible_data().
  const Rect& visible_rect() const { return visible_rect_; }
  // Specifies that the |visible_rect| section of the frame is supposed to be
  // scaled to this size when being presented. This can be used to represent
  // anamorphic frames, or to "soft-apply" any custom scaling.
  const Size& natural_size() const { return natural_size_; }

  int stride(size_t plane) const {
    DCHECK(IsValidPlane(plane, format()));
    DCHECK_LT(plane, layout_.num_planes());
    return layout_.planes()[plane].stride;
  }

  // Returns the number of bytes per row and number of rows for a given plane.
  //
  // As opposed to stride(), row_bytes() refers to the bytes representing
  // frame data scanlines (coded_size.width() pixels, without stride padding).
  int row_bytes(size_t plane) const;
  int rows(size_t plane) const;

  // Returns pointer to the buffer for a given plane, if this is an
  // IsMappable() frame type. The memory is owned by VideoFrame object and must
  // not be freed by the caller.
  const uint8_t* data(size_t plane) const {
    DCHECK(IsValidPlane(plane, format()));
    DCHECK(IsMappable());
    return data_[plane];
  }
  uint8_t* data(size_t plane) {
    DCHECK(IsValidPlane(plane, format()));
    DCHECK(IsMappable());
    return data_[plane];
  }

  // Returns pointer to the data in the visible region of the frame, for
  // IsMappable() storage types. The returned pointer is offsetted into the
  // plane buffer specified by visible_rect().origin(). Memory is owned by
  // VideoFrame object and must not be freed by the caller.
  const uint8_t* visible_data(size_t plane) const;
  uint8_t* visible_data(size_t plane);

  // Returns a pointer to the read-only shared-memory region, if present.
  base::ReadOnlySharedMemoryRegion* read_only_shared_memory_region() const;

  // Returns a pointer to the unsafe shared memory handle, if present.
  base::UnsafeSharedMemoryRegion* unsafe_shared_memory_region() const;

  // Returns the legacy SharedMemoryHandle, if present.
  base::SharedMemoryHandle shared_memory_handle() const;

  // Returns the offset into the shared memory where the frame data begins.
  size_t shared_memory_offset() const;

  // Returns a vector containing the backing DmaBufs for this frame. The number
  // of returned DmaBufs will be equal or less than the number of planes of
  // the frame. If there are less, this means that the last FD contains the
  // remaining planes.
  // Note that the returned FDs are still owned by the VideoFrame. This means
  // that the caller shall not close them, or use them after the VideoFrame is
  // destroyed. For such use cases, use media::DuplicateFDs() to obtain your
  // own copy of the FDs.
  const std::vector<base::ScopedFD>& DmabufFds() const;

  // Returns true if |frame| has DmaBufs.
  bool HasDmaBufs() const;

  void AddReadOnlySharedMemoryRegion(base::ReadOnlySharedMemoryRegion* region);
  void AddUnsafeSharedMemoryRegion(base::UnsafeSharedMemoryRegion* region);

  // Legacy, use one of the Add*SharedMemoryRegion methods above instead.
  void AddSharedMemoryHandle(base::SharedMemoryHandle handle);

  // Adds a callback to be run when the VideoFrame is about to be destroyed.
  // The callback may be run from ANY THREAD, and so it is up to the client to
  // ensure thread safety.  Although read-only access to the members of this
  // VideoFrame is permitted while the callback executes (including
  // VideoFrameMetadata), clients should not assume the data pointers are
  // valid.
  void AddDestructionObserver(base::OnceClosure callback);

  // Returns a dictionary of optional metadata.  This contains information
  // associated with the frame that downstream clients might use for frame-level
  // logging, quality/performance optimizations, signaling, etc.
  //
  // TODO(miu): Move some of the "extra" members of VideoFrame (below) into
  // here as a later clean-up step.
  const VideoFrameMetadata* metadata() const { return &metadata_; }
  VideoFrameMetadata* metadata() { return &metadata_; }

  // The time span between the current frame and the first frame of the stream.
  // This is the media timestamp, and not the reference time.
  // See VideoFrameMetadata::REFERENCE_TIME for details.
  base::TimeDelta timestamp() const { return timestamp_; }
  void set_timestamp(base::TimeDelta timestamp) { timestamp_ = timestamp; }

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString();

  // Unique identifier for this video frame; generated at construction time and
  // guaranteed to be unique within a single process.
  int unique_id() const { return unique_id_; }

  // Returns the number of bits per channel.
  size_t BitDepth() const;

 protected:
  friend class base::RefCountedThreadSafe<VideoFrame>;

  // Clients must use the static factory/wrapping methods to create a new frame.
  // Derived classes should create their own factory/wrapping methods, and use
  // this constructor to do basic initialization.
  VideoFrame(const VideoFrameLayout& layout,
             StorageType storage_type,
             const Rect& visible_rect,
             const Size& natural_size,
             base::TimeDelta timestamp);

  virtual ~VideoFrame();

  // Creates a summary of the configuration settings provided as parameters.
  static std::string ConfigToString(const VideoPixelFormat format,
                                    const VideoFrame::StorageType storage_type,
                                    const Size& coded_size,
                                    const Rect& visible_rect,
                                    const Size& natural_size);

  // Returns true if |plane| is a valid plane index for the given |format|.
  static bool IsValidPlane(size_t plane, VideoPixelFormat format);

  // Returns |dimensions| adjusted to appropriate boundaries based on |format|.
  static Size DetermineAlignedSize(VideoPixelFormat format,
                                   const Size& dimensions);

  void set_data(size_t plane, uint8_t* ptr) {
    DCHECK(IsValidPlane(plane, format()));
    DCHECK(ptr);
    data_[plane] = ptr;
  }

 private:
  static scoped_refptr<VideoFrame> WrapExternalStorage(
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
      size_t data_offset);

  static scoped_refptr<VideoFrame> CreateFrameInternal(
      VideoPixelFormat format,
      const Size& coded_size,
      const Rect& visible_rect,
      const Size& natural_size,
      base::TimeDelta timestamp,
      bool zero_initialize_memory);

  bool SharedMemoryUninitialized();

  // Returns true if |plane| is a valid plane index for the given |format|.
  static bool IsValidPlane(VideoPixelFormat format, size_t plane);

  // Returns the pixel size of each subsample for a given |plane| and |format|.
  // E.g. 2x2 for the U-plane in PIXEL_FORMAT_I420.
  static Size SampleSize(VideoPixelFormat format, size_t plane);

  // Return the alignment for the whole frame, calculated as the max of the
  // alignment for each individual plane.
  static Size CommonAlignment(VideoPixelFormat format);

  void AllocateMemory(bool zero_initialize_memory);

  // Calculates plane size.
  // It first considers buffer size layout_ object provides. If layout's
  // number of buffers equals to number of planes, and buffer size is assigned
  // (non-zero), it returns buffers' size.
  // Otherwise, it uses the first (num_buffers - 1) assigned buffers' size as
  // plane size. Then for the rest unassigned planes, calculates their size
  // based on format, coded size and stride for the plane.
  std::vector<size_t> CalculatePlaneSize() const;

  // VideFrameLayout (includes format, coded_size, and strides).
  const VideoFrameLayout layout_;

  // Storage type for the different planes.
  StorageType storage_type_;  // TODO(mcasas): make const

  // Width, height, and offsets of the visible portion of the video frame. Must
  // be a subrect of |coded_size_|. Can be odd with respect to the sample
  // boundaries, e.g. for formats with subsampled chroma.
  const Rect visible_rect_;

  // Width and height of the visible portion of the video frame
  // (|visible_rect_.size()|) with aspect ratio taken into account.
  const Size natural_size_;

  // Array of data pointers to each plane.
  // TODO(mcasas): we don't know on ctor if we own |data_| or not. Change
  // to std::unique_ptr<uint8_t, AlignedFreeDeleter> after refactoring
  // VideoFrame.
  uint8_t* data_[kMaxPlanes];

  // Shared memory handle and associated offset inside it, if this frame is a
  // STORAGE_SHMEM one.  Pointers to unowned shared memory regions. At most one
  // of the memory regions will be set.
  base::ReadOnlySharedMemoryRegion* read_only_shared_memory_region_ = nullptr;
  base::UnsafeSharedMemoryRegion* unsafe_shared_memory_region_ = nullptr;

  // Legacy handle.
  base::SharedMemoryHandle shared_memory_handle_;

  // If this is a STORAGE_SHMEM frame, the offset of the data within the shared
  // memory.
  size_t shared_memory_offset_;

  class DmabufHolder;

  // Dmabufs for the frame, used when storage is STORAGE_DMABUFS. Size is either
  // equal or less than the number of planes of the frame. If it is less, then
  // the memory area represented by the last FD contains the remaining planes.
  std::vector<base::ScopedFD> dmabuf_fds_;

  std::vector<base::OnceClosure> done_callbacks_;

  base::TimeDelta timestamp_;

  VideoFrameMetadata metadata_;

  // Generated at construction time.
  const int unique_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoFrame);
};

}  // namespace media

#endif  // VIDEO_FRAME_H_
