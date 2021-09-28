// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMMON_FORMAT_CONVERTER_H
#define ANDROID_V4L2_CODEC2_COMMON_FORMAT_CONVERTER_H

#include <limits>
#include <queue>
#include <vector>

#include <C2Buffer.h>
#include <size.h>
#include <utils/StrongPointer.h>
#include <video_pixel_format.h>

namespace android {

class GraphicBuffer;

// ImplDefinedToRGBXMap can provide the layout for RGB-backed IMPLEMENTATION_DEFINED format case,
// which will be failed to map by C2AllocationGralloc::map(). When the instance is created, it will
// own the GraphicBuffer wrapped from input block and lock it, to provide the address, offset, and
// rowInc information. The GraphicBuffer will be unlocked and released under destruction.
class ImplDefinedToRGBXMap {
public:
    ~ImplDefinedToRGBXMap();
    ImplDefinedToRGBXMap() = delete;

    static std::unique_ptr<ImplDefinedToRGBXMap> Create(const C2ConstGraphicBlock& block);

    const uint8_t* addr() const { return mAddr; }
    int offset() const { return 0; }
    int rowInc() const { return mRowInc; }

private:
    ImplDefinedToRGBXMap(sp<GraphicBuffer> buf, uint8_t* addr, int rowInc);

    const sp<GraphicBuffer> mBuffer;
    const uint8_t* mAddr;
    const int mRowInc;
};

class FormatConverter {
public:
    ~FormatConverter() = default;

    FormatConverter(const FormatConverter&) = delete;
    FormatConverter& operator=(const FormatConverter&) = delete;

    // Create FormatConverter instance and initialize it, nullptr will be returned on
    // initialization error.
    static std::unique_ptr<FormatConverter> Create(media::VideoPixelFormat outFormat,
                                                        const media::Size& visibleSize,
                                                        uint32_t inputCount,
                                                        const media::Size& codedSize);

    // Convert the input block into the alternative block with required pixel format and return it,
    // or return the original block if zero-copy is applied.
    C2ConstGraphicBlock convertBlock(uint64_t frameIndex, const C2ConstGraphicBlock& inputBlock,
                                     c2_status_t* status /* non-null */);
    // Return the block ownership when VEA no longer needs it, or erase the zero-copy BlockEntry.
    c2_status_t returnBlock(uint64_t frameIndex);
    // Check if there is available block for conversion.
    bool isReady() const { return !mAvailableQueue.empty(); }

private:
    // The minimal number requirement of allocated buffers for conversion. This value is the same as
    // kMinInputBufferArraySize from CCodecBufferChannel.
    static constexpr uint32_t kMinInputBufferCount = 8;
    // The constant used by BlockEntry to indicate no frame is associated with the BlockEntry.
    static constexpr uint64_t kNoFrameAssociated = ~static_cast<uint64_t>(0);

    // There are 2 types of BlockEntry:
    // 1. If |mBlock| is an allocated graphic block (not nullptr). This BlockEntry is for
    //    conversion, and |mAssociatedFrameIndex| records the frame index of the input frame which
    //    is currently converted from. This is created on initialize() and released while
    //    FormatConverter is destroyed.
    // 2. If |mBlock| is nullptr. This BlockEntry is only used to record zero-copied frame index to
    //    |mAssociatedFrameIndex|. This is created on zero-copy is applied during convertBlock(),
    //    and released on returnBlock() in associated with returned frame index.
    struct BlockEntry {
        // Constructor of convertible entry.
        BlockEntry(std::shared_ptr<C2GraphicBlock> block) : mBlock(std::move(block)) {}
        // Constructir of zero-copy entry.
        BlockEntry(uint64_t frameIndex) : mAssociatedFrameIndex(frameIndex) {}

        std::shared_ptr<C2GraphicBlock> mBlock;
        uint64_t mAssociatedFrameIndex = kNoFrameAssociated;
    };

    FormatConverter() = default;

    // Initialize foramt converter. It will pre-allocate a set of graphic blocks as |codedSize| and
    // |outFormat|. This function should be called prior to other functions.
    c2_status_t initialize(media::VideoPixelFormat outFormat, const media::Size& visibleSize,
                           uint32_t inputCount, const media::Size& codedSize);

    // The array of block entries.
    std::vector<std::unique_ptr<BlockEntry>> mGraphicBlocks;
    // The queue of recording the raw pointers of available graphic blocks. The consumed block will
    // be popped on convertBlock(), and returned block will be pushed on returnBlock().
    std::queue<BlockEntry*> mAvailableQueue;
    // The temporary U/V plane memory allocation for ABGR to NV12 conversion. They should be
    // allocated on initialize().
    std::unique_ptr<uint8_t[]> mTempPlaneU;
    std::unique_ptr<uint8_t[]> mTempPlaneV;

    media::VideoPixelFormat mOutFormat = media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN;
    media::Size mVisibleSize;
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMMON_FORMAT_CONVERTER_H
