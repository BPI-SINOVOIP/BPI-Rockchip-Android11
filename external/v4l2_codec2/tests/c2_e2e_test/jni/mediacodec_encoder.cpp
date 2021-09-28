// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #define LOG_NDEBUG 0
#define LOG_TAG "MediaCodecEncoder"

#include "mediacodec_encoder.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <media/NdkMediaFormat.h>
#include <utils/Log.h>

namespace android {
namespace {
// The values are defined at
// <android_root>/frameworks/base/media/java/android/media/MediaCodecInfo.java.
constexpr int32_t COLOR_FormatYUV420Planar = 19;
constexpr int32_t BITRATE_MODE_CBR = 2;

// The time interval between two key frames.
constexpr int32_t kIFrameIntervalSec = 10;

// The timeout of AMediaCodec function calls.
constexpr int kTimeoutUs = 1000;  // 1ms.

// The tolenrance period between two input buffers are enqueued,
// and the period between submitting EOS input buffer to receiving the EOS
// output buffer.
constexpr int kBufferPeriodTimeoutUs = 1000000;  // 1 sec

// Helper function to get possible encoder names from |type|.
// Note: A single test APK is built for both ARC++ and ARCVM, so both the C2 VEA encoder and the new
// V4L2 encoder names need to be specified here.
std::vector<const char*> GetArcVideoEncoderNames(VideoCodecType type) {
    switch (type) {
    case VideoCodecType::H264:
        return {"c2.v4l2.avc.encoder", "c2.vea.avc.encoder"};
    default:  // unsupported type: VP8, VP9, or unknown
        return {};
    }
}

}  // namespace

// static
std::unique_ptr<MediaCodecEncoder> MediaCodecEncoder::Create(std::string input_path,
                                                             Size visible_size) {
    if (visible_size.width <= 0 || visible_size.height <= 0 || visible_size.width % 2 == 1 ||
        visible_size.height % 2 == 1) {
        ALOGE("Size is not valid: %dx%d", visible_size.width, visible_size.height);
        return nullptr;
    }
    size_t buffer_size = visible_size.width * visible_size.height * 3 / 2;

    std::unique_ptr<InputFileStream> input_file(new InputFileStream(input_path));
    if (!input_file->IsValid()) {
        ALOGE("Failed to open file: %s", input_path.c_str());
        return nullptr;
    }
    int file_size = input_file->GetLength();
    if (file_size < 0 || file_size % buffer_size != 0) {
        ALOGE("Stream byte size (%d) is not a multiple of frame byte size (%zu).", file_size,
              buffer_size);
        return nullptr;
    }

    AMediaCodec* codec = nullptr;
    // Only H264 is supported now.
    auto encoder_names = GetArcVideoEncoderNames(VideoCodecType::H264);
    for (const auto& encoder_name : encoder_names) {
        codec = AMediaCodec_createCodecByName(encoder_name);
        if (codec) {
            ALOGD("Created mediacodec encoder by name: %s", encoder_name);
            break;
        }
    }
    if (!codec) {
        ALOGE("Failed to create mediacodec encoder.");
        return nullptr;
    }

    return std::unique_ptr<MediaCodecEncoder>(new MediaCodecEncoder(
            codec, std::move(input_file), visible_size, buffer_size, file_size / buffer_size));
}

MediaCodecEncoder::MediaCodecEncoder(AMediaCodec* codec,
                                     std::unique_ptr<InputFileStream> input_file, Size size,
                                     size_t buffer_size, size_t num_total_frames)
      : kVisibleSize(size),
        kBufferSize(buffer_size),
        kNumTotalFrames(num_total_frames),
        codec_(codec),
        num_encoded_frames_(num_total_frames),
        input_file_(std::move(input_file)) {}

MediaCodecEncoder::~MediaCodecEncoder() {
    if (codec_ != nullptr) {
        AMediaCodec_delete(codec_);
    }
}

void MediaCodecEncoder::SetEncodeInputBufferCb(const EncodeInputBufferCb& cb) {
    encode_input_buffer_cb_ = cb;
}

void MediaCodecEncoder::SetOutputBufferReadyCb(const OutputBufferReadyCb& cb) {
    output_buffer_ready_cb_ = cb;
}

void MediaCodecEncoder::Rewind() {
    input_frame_index_ = 0;
    input_file_->Rewind();
}

bool MediaCodecEncoder::Configure(int32_t bitrate, int32_t framerate) {
    ALOGV("Configure encoder bitrate=%d, framerate=%d", bitrate, framerate);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "video/avc");
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatYUV420Planar);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BITRATE_MODE, BITRATE_MODE_CBR);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, kIFrameIntervalSec);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, kVisibleSize.width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, kVisibleSize.height);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, framerate);
    bool ret = AMediaCodec_configure(codec_, format, nullptr /* surface */, nullptr /* crtpto */,
                                     AMEDIACODEC_CONFIGURE_FLAG_ENCODE) == AMEDIA_OK;
    AMediaFormat_delete(format);
    if (ret) {
        bitrate_ = bitrate;
        framerate_ = framerate;
    }
    return ret;
}

bool MediaCodecEncoder::Start() {
    return AMediaCodec_start(codec_) == AMEDIA_OK;
}

bool MediaCodecEncoder::Encode() {
    const int64_t input_period = run_at_fps_ ? (1000000 / framerate_) : 0;
    const int64_t start_time = GetNowUs();

    bool input_done = false;
    bool output_done = false;
    int64_t last_enqueue_input_time = start_time;
    int64_t send_eos_time;
    while (!output_done) {
        // Feed input stream to the encoder.
        ssize_t index;
        if (!input_done && (GetNowUs() - start_time >= input_frame_index_ * input_period)) {
            index = AMediaCodec_dequeueInputBuffer(codec_, kTimeoutUs);
            if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                if (GetNowUs() - last_enqueue_input_time > kBufferPeriodTimeoutUs) {
                    ALOGE("Timeout to dequeue next input buffer.");
                    return false;
                }
            } else if (index >= 0) {
                ALOGV("input buffer index: %zu", index);
                if (input_frame_index_ == num_encoded_frames_) {
                    if (!FeedEOSInputBuffer(index)) return false;

                    input_done = true;
                    send_eos_time = GetNowUs();
                } else {
                    if (!FeedInputBuffer(index)) return false;

                    last_enqueue_input_time = GetNowUs();
                }
            }
        }

        // Retrieve the encoded output buffer.
        AMediaCodecBufferInfo info;
        index = AMediaCodec_dequeueOutputBuffer(codec_, &info, kTimeoutUs);
        if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            if (input_done && GetNowUs() - send_eos_time > kBufferPeriodTimeoutUs) {
                ALOGE("Timeout to receive EOS output buffer.");
                return false;
            }
        } else if (index >= 0) {
            ALOGV("output buffer index: %zu", index);
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) output_done = true;
            if (!ReceiveOutputBuffer(index, info)) return false;
        }
    }
    return true;
}

bool MediaCodecEncoder::Stop() {
    return AMediaCodec_stop(codec_) == AMEDIA_OK;
}

void MediaCodecEncoder::set_num_encoded_frames(size_t num_encoded_frames) {
    num_encoded_frames_ = num_encoded_frames;
}

size_t MediaCodecEncoder::num_encoded_frames() const {
    return num_encoded_frames_;
}

void MediaCodecEncoder::set_run_at_fps(bool run_at_fps) {
    run_at_fps_ = run_at_fps;
}

bool MediaCodecEncoder::FeedInputBuffer(size_t index) {
    ALOGV("input buffer index: %zu", index);
    uint64_t time_us = input_frame_index_ * 1000000 / framerate_;

    size_t out_size;
    uint8_t* buf = AMediaCodec_getInputBuffer(codec_, index, &out_size);
    if (!buf || out_size < kBufferSize) {
        ALOGE("Failed to getInputBuffer: index=%zu, buf=%p, out_size=%zu", index, buf, out_size);
        return false;
    }

    if (input_file_->Read(reinterpret_cast<char*>(buf), kBufferSize) != kBufferSize) {
        ALOGE("Failed to read buffer from file.");
        return false;
    }

    // We circularly encode the video stream if the frame number is not enough.
    ++input_frame_index_;
    if (input_frame_index_ % kNumTotalFrames == 0) {
        input_file_->Rewind();
    }

    if (encode_input_buffer_cb_) encode_input_buffer_cb_(time_us);

    media_status_t status = AMediaCodec_queueInputBuffer(codec_, index, 0 /* offset */, kBufferSize,
                                                         time_us, 0 /* flag */);
    if (status != AMEDIA_OK) {
        ALOGE("Failed to queueInputBuffer: %d", static_cast<int>(status));
        return false;
    }
    return true;
}

bool MediaCodecEncoder::FeedEOSInputBuffer(size_t index) {
    ALOGV("input buffer index: %zu", index);
    uint64_t time_us = input_frame_index_ * 1000000 / framerate_;

    media_status_t status =
            AMediaCodec_queueInputBuffer(codec_, index, 0 /* offset */, kBufferSize, time_us,
                                         AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
    if (status != AMEDIA_OK) {
        ALOGE("Failed to queueInputBuffer: %d", static_cast<int>(status));
        return false;
    }
    return true;
}

bool MediaCodecEncoder::ReceiveOutputBuffer(size_t index, const AMediaCodecBufferInfo& info) {
    size_t out_size;
    uint8_t* buf = AMediaCodec_getOutputBuffer(codec_, index, &out_size);
    if (!buf) {
        ALOGE("Failed to getOutputBuffer.");
        return false;
    }

    if (output_buffer_ready_cb_) output_buffer_ready_cb_(buf, info);

    media_status_t status = AMediaCodec_releaseOutputBuffer(codec_, index, false /* render */);
    if (status != AMEDIA_OK) {
        ALOGE("Failed to releaseOutputBuffer: %d", static_cast<int>(status));
        return false;
    }
    return true;
}

}  // namespace android
