// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_CODEC_TEST_MEDIACODEC_ENCODER_H_
#define ARC_CODEC_TEST_MEDIACODEC_ENCODER_H_

#include <memory>
#include <string>

#include <media/NdkMediaCodec.h>

#include "common.h"

namespace android {

// Wrapper class to manipulate a MediaCodec video encoder.
class MediaCodecEncoder {
public:
    // Checks the argument and create MediaCodecEncoder instance.
    static std::unique_ptr<MediaCodecEncoder> Create(std::string input_path, Size visible_size);
    MediaCodecEncoder() = delete;
    ~MediaCodecEncoder();

    using EncodeInputBufferCb = std::function<void(uint64_t /* time_us */)>;
    // The callback function that is called when output buffer is ready.
    using OutputBufferReadyCb =
            std::function<void(const uint8_t* /* data */, const AMediaCodecBufferInfo& /* info */)>;
    void SetEncodeInputBufferCb(const EncodeInputBufferCb& cb);
    void SetOutputBufferReadyCb(const OutputBufferReadyCb& cb);

    // Encoder manipulation methods.
    // Rewind the frame index to the beginning of the input stream.
    void Rewind();

    // Wrapper of AMediaCodec_configure.
    bool Configure(int32_t bitrate, int32_t framerate);

    // Wrapper of AMediaCodec_start.
    bool Start();

    // Encode the test stream. After encoding, send EOS request to the
    // encoder. Return true if EOS output buffer is received.
    bool Encode();

    // Wrapper of AMediaCodec_stop.
    bool Stop();

    // Setter and getter method of |num_encoded_frames_|;
    void set_num_encoded_frames(size_t num_encoded_frames);
    size_t num_encoded_frames() const;
    // Setter method of |run_at_fps_|.
    void set_run_at_fps(bool run_at_fps);

private:
    MediaCodecEncoder(AMediaCodec* codec, std::unique_ptr<InputFileStream> inputFile, Size size,
                      size_t bufferSize, size_t numTotalFrames);

    // Read the content from the |input_file_| and feed into the input buffer.
    // |index| is the index of the target input buffer.
    bool FeedInputBuffer(size_t index);

    // Feed an empty buffer with EOS flag.
    // |index| is the index of the target input buffer.
    bool FeedEOSInputBuffer(size_t index);

    // Receive the output buffer and write into the |mOutputFile|.
    // |index| is the index of the target output buffer.
    // |info| is the buffer info of the target output buffer.
    bool ReceiveOutputBuffer(size_t index, const AMediaCodecBufferInfo& info);

    // The resolution of the input video stream.
    const Size kVisibleSize;
    // The buffer size of one frame, i.e. (width * height * 1.5) because now
    // only YUV I420 is supported.
    const size_t kBufferSize;
    // The frame number of the input video stream.
    const size_t kNumTotalFrames;

    // The target mediacodec encoder.
    AMediaCodec* codec_;
    // The number of frames to encode.
    size_t num_encoded_frames_;
    // The input video raw stream file. The file size must be the multiple of
    // |kBufferSize|.
    std::unique_ptr<InputFileStream> input_file_;
    // The target output bitrate.
    int bitrate_ = 192000;
    // The target output framerate.
    int framerate_ = 30;
    // Encode the data at the |framerate_|.
    bool run_at_fps_ = false;

    // The callback function which is called right before queueing one input
    // buffer.
    EncodeInputBufferCb encode_input_buffer_cb_;
    // The callback function which is called when an output buffer is ready.
    OutputBufferReadyCb output_buffer_ready_cb_;

    // The frame index that indicates which frame is sent to the encoder at
    // next round.
    size_t input_frame_index_ = 0;
};

}  // namespace android

#endif  // ARC_CODEC_TEST_MEDIACODEC_ENCODER_H_
