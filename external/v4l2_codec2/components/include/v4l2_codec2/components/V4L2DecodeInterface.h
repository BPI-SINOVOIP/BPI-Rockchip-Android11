// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMPONENTS_V4L2_DECODE_INTERFACE_H
#define ANDROID_V4L2_CODEC2_COMPONENTS_V4L2_DECODE_INTERFACE_H

#include <memory>
#include <string>

#include <C2Config.h>
#include <util/C2InterfaceHelper.h>

#include <size.h>
#include <v4l2_codec2/common/VideoTypes.h>

namespace android {

class V4L2DecodeInterface : public C2InterfaceHelper {
public:
    V4L2DecodeInterface(const std::string& name, const std::shared_ptr<C2ReflectorHelper>& helper);
    V4L2DecodeInterface(const V4L2DecodeInterface&) = delete;
    V4L2DecodeInterface& operator=(const V4L2DecodeInterface&) = delete;
    ~V4L2DecodeInterface() = default;

    // interfaces for the client component.
    c2_status_t status() const { return mInitStatus; }
    C2BlockPool::local_id_t getBlockPoolId() const { return mOutputBlockPoolIds->m.values[0]; }
    std::optional<VideoCodec> getVideoCodec() const { return mVideoCodec; }
    media::Size getMaxSize() const { return mMaxSize; }
    media::Size getMinSize() const { return mMinSize; }

    size_t getInputBufferSize() const;
    c2_status_t queryColorAspects(
            std::shared_ptr<C2StreamColorAspectsInfo::output>* targetColorAspects);

private:
    // Configurable parameter setters.
    static C2R ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::input>& info);
    static C2R SizeSetter(bool mayBlock, C2P<C2StreamPictureSizeInfo::output>& videoSize);
    static C2R MaxInputBufferSizeCalculator(bool mayBlock,
                                            C2P<C2StreamMaxBufferSizeInfo::input>& me,
                                            const C2P<C2StreamPictureSizeInfo::output>& size);

    template <typename T>
    static C2R DefaultColorAspectsSetter(bool mayBlock, C2P<T>& def);

    static C2R MergedColorAspectsSetter(bool mayBlock,
                                        C2P<C2StreamColorAspectsInfo::output>& merged,
                                        const C2P<C2StreamColorAspectsTuning::output>& def,
                                        const C2P<C2StreamColorAspectsInfo::input>& coded);

    // The input format kind; should be C2FormatCompressed.
    std::shared_ptr<C2StreamBufferTypeSetting::input> mInputFormat;
    // The memory usage flag of input buffer; should be BufferUsage::VIDEO_DECODER.
    std::shared_ptr<C2StreamUsageTuning::input> mInputMemoryUsage;
    // The output format kind; should be C2FormatVideo.
    std::shared_ptr<C2StreamBufferTypeSetting::output> mOutputFormat;
    // The MIME type of input port.
    std::shared_ptr<C2PortMediaTypeSetting::input> mInputMediaType;
    // The MIME type of output port; should be MEDIA_MIMETYPE_VIDEO_RAW.
    std::shared_ptr<C2PortMediaTypeSetting::output> mOutputMediaType;
    // The number of additional output frames that might need to be generated before an output
    // buffer can be released by the component; only used for H264 because H264 may reorder the
    // output frames.
    std::shared_ptr<C2PortDelayTuning::output> mOutputDelay;
    // The input codec profile and level. For now configuring this parameter is useless since
    // the component always uses fixed codec profile to initialize accelerator. It is only used
    // for the client to query supported profile and level values.
    // TODO: use configured profile/level to initialize accelerator.
    std::shared_ptr<C2StreamProfileLevelInfo::input> mProfileLevel;
    // Decoded video size for output.
    std::shared_ptr<C2StreamPictureSizeInfo::output> mSize;
    // Maximum size of one input buffer.
    std::shared_ptr<C2StreamMaxBufferSizeInfo::input> mMaxInputSize;
    // The suggested usage of input buffer allocator ID.
    std::shared_ptr<C2PortAllocatorsTuning::input> mInputAllocatorIds;
    // The suggested usage of output buffer allocator ID.
    std::shared_ptr<C2PortAllocatorsTuning::output> mOutputAllocatorIds;
    // The suggested usage of output buffer allocator ID with surface.
    std::shared_ptr<C2PortSurfaceAllocatorTuning::output> mOutputSurfaceAllocatorId;
    // Component uses this ID to fetch corresponding output block pool from platform.
    std::shared_ptr<C2PortBlockPoolsTuning::output> mOutputBlockPoolIds;
    // The color aspects parsed from input bitstream. This parameter should be configured by
    // component while decoding.
    std::shared_ptr<C2StreamColorAspectsInfo::input> mCodedColorAspects;
    // The default color aspects specified by requested output format. This parameter should be
    // configured by client.
    std::shared_ptr<C2StreamColorAspectsTuning::output> mDefaultColorAspects;
    // The combined color aspects by |mCodedColorAspects| and |mDefaultColorAspects|, and the
    // former has higher priority. This parameter is used for component to provide color aspects
    // as C2Info in decoded output buffers.
    std::shared_ptr<C2StreamColorAspectsInfo::output> mColorAspects;

    c2_status_t mInitStatus;
    std::optional<VideoCodec> mVideoCodec;
    media::Size mMinSize;
    media::Size mMaxSize;
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMPONENTS_V4L2_DECODE_INTERFACE_H
