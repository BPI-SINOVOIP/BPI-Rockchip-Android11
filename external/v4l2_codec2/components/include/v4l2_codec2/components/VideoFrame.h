// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMPONENTS_VIDEO_FRAME_H
#define ANDROID_V4L2_CODEC2_COMPONENTS_VIDEO_FRAME_H

#include <memory>
#include <vector>

#include <C2Buffer.h>

#include <rect.h>

namespace android {

// Wrap C2GraphicBlock and provide essiential information from C2GraphicBlock.
class VideoFrame {
public:
    // Create the instance from C2GraphicBlock. return nullptr if any error occurs.
    static std::unique_ptr<VideoFrame> Create(std::shared_ptr<C2GraphicBlock> block);
    ~VideoFrame();

    // Return the file descriptors of the corresponding buffer.
    const std::vector<int>& getFDs() const;

    // Getter and setter of the visible rectangle.
    void setVisibleRect(const media::Rect& visibleRect);
    const media::Rect& getVisibleRect() const;

    // Getter and setter of the bitstream ID of the corresponding input bitstream.
    void setBitstreamId(int32_t bitstreamId);
    int32_t getBitstreamId() const;

    // Get the read-only C2GraphicBlock, should be called after calling setVisibleRect().
    C2ConstGraphicBlock getGraphicBlock();

private:
    VideoFrame(std::shared_ptr<C2GraphicBlock> block, std::vector<int> fds);

    std::shared_ptr<C2GraphicBlock> mGraphicBlock;
    std::vector<int> mFds;
    media::Rect mVisibleRect;
    int32_t mBitstreamId = -1;
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMPONENTS_VIDEO_FRAME_H
