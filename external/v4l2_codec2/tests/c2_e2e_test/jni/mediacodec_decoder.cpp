// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #define LOG_NDEBUG 0
#define LOG_TAG "MediaCodecDecoder"

#include "mediacodec_decoder.h"

#include <assert.h>
#include <inttypes.h>

#include <utility>
#include <vector>

#include <media/NdkMediaFormat.h>
#include <utils/Log.h>

namespace android {
namespace {
constexpr int64_t kSecToNs = 1000000000;
// The timeout of AMediaCodec_dequeueOutputBuffer function calls.
constexpr int kTimeoutWaitForOutputUs = 1000;  // 1 millisecond

// The timeout of AMediaCodec_dequeueInputBuffer function calls.
constexpr int kTimeoutWaitForInputUs = 1000;  // 1 millisecond

// The maximal retry times of doDecode routine.
// The maximal tolerable interval between two dequeued outputs will be:
//   kTimeoutWaitForOutputUs * kTimeoutMaxRetries = 500 milliseconds
constexpr size_t kTimeoutMaxRetries = 500;

// Helper function to get possible C2 hardware decoder names from |type|.
std::vector<const char*> GetC2VideoDecoderNames(VideoCodecType type) {
    switch (type) {
    case VideoCodecType::H264:
        return {"c2.v4l2.avc.decoder", "c2.vda.avc.decoder"};
    case VideoCodecType::VP8:
        return {"c2.v4l2.vp8.decoder", "c2.vda.vp8.decoder"};
    case VideoCodecType::VP9:
        return {"c2.v4l2.vp9.decoder", "c2.vda.vp9.decoder"};
    default:  // unknown type
        return {};
    }
}

// Helper function to get possible software decoder names from |type|.
std::vector<const char*> GetSwVideoDecoderNames(VideoCodecType type) {
    switch (type) {
    case VideoCodecType::H264:
        return {"OMX.google.h264.decoder"};
    case VideoCodecType::VP8:
        return {"OMX.google.vp8.decoder"};
    case VideoCodecType::VP9:
        return {"OMX.google.vp9.decoder"};
    default:  // unknown type
        return {};
    }
}

const uint32_t BUFFER_FLAG_CODEC_CONFIG = AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;
const char* FORMAT_KEY_SLICE_HEIGHT = AMEDIAFORMAT_KEY_SLICE_HEIGHT;

int64_t GetCurrentTimeNs() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * UINT64_C(1000000000) + now.tv_nsec;
}

int64_t RoundUp(int64_t n, int64_t multiple) {
    return ((n + (multiple - 1)) / multiple) * multiple;
}

}  // namespace

// static
std::unique_ptr<MediaCodecDecoder> MediaCodecDecoder::Create(const std::string& input_path,
                                                             VideoCodecProfile profile,
                                                             bool use_sw_decoder,
                                                             const Size& video_size, int frame_rate,
                                                             ANativeWindow* surface,
                                                             bool render_on_release, bool loop) {
    if (video_size.IsEmpty()) {
        ALOGE("Size is not valid: %dx%d", video_size.width, video_size.height);
        return nullptr;
    }

    VideoCodecType type = VideoCodecProfileToType(profile);

    std::unique_ptr<EncodedDataHelper> encoded_data_helper(new EncodedDataHelper(input_path, type));
    if (!encoded_data_helper->IsValid()) {
        ALOGE("EncodedDataHelper is not created for file: %s", input_path.c_str());
        return nullptr;
    }

    AMediaCodec* codec = nullptr;
    auto decoder_names =
            use_sw_decoder ? GetSwVideoDecoderNames(type) : GetC2VideoDecoderNames(type);
    for (const auto& decoder_name : decoder_names) {
        codec = AMediaCodec_createCodecByName(decoder_name);
        if (codec) {
            ALOGD("Created mediacodec decoder by name: %s", decoder_name);
            break;
        }
    }
    if (!codec) {
        ALOGE("Failed to create mediacodec decoder.");
        return nullptr;
    }

    auto ret = std::unique_ptr<MediaCodecDecoder>(
            new MediaCodecDecoder(codec, std::move(encoded_data_helper), type, video_size,
                                  frame_rate, surface, render_on_release, loop));

    AMediaCodecOnAsyncNotifyCallback cb{
            .onAsyncInputAvailable =
                    [](AMediaCodec* codec, void* userdata, int32_t index) {
                        reinterpret_cast<MediaCodecDecoder*>(userdata)->OnAsyncInputAvailable(
                                index);
                    },
            .onAsyncOutputAvailable =
                    [](AMediaCodec* codec, void* userdata, int32_t index,
                       AMediaCodecBufferInfo* buffer_info) {
                        reinterpret_cast<MediaCodecDecoder*>(userdata)->OnAsyncOutputAvailable(
                                index, buffer_info);
                    },
            .onAsyncFormatChanged =
                    [](AMediaCodec* codec, void* userdata, AMediaFormat* format) {
                        reinterpret_cast<MediaCodecDecoder*>(userdata)->OnAsyncFormatChanged(
                                format);
                    },
            .onAsyncError =
                    [](AMediaCodec* codec, void* userdata, media_status_t error, int32_t code,
                       const char* detail) {
                        ALOGE("Error %d (%d) %s", error, code, detail);
                        assert(false);
                    }};

    auto status = AMediaCodec_setAsyncNotifyCallback(codec, cb, ret.get());
    if (status != AMEDIA_OK) {
        ALOGE("Failed to set async callback.");
        return nullptr;
    }

    return ret;
}

MediaCodecDecoder::MediaCodecDecoder(AMediaCodec* codec,
                                     std::unique_ptr<EncodedDataHelper> encoded_data_helper,
                                     VideoCodecType type, const Size& size, int frame_rate,
                                     ANativeWindow* surface, bool render_on_release, bool loop)
      : codec_(codec),
        encoded_data_helper_(std::move(encoded_data_helper)),
        type_(type),
        input_visible_size_(size),
        frame_rate_(frame_rate),
        surface_(surface),
        render_on_release_(render_on_release),
        looping_(loop) {}

MediaCodecDecoder::~MediaCodecDecoder() {
    if (codec_ != nullptr) {
        AMediaCodec_delete(codec_);
    }
}

void MediaCodecDecoder::AddOutputBufferReadyCb(const OutputBufferReadyCb& cb) {
    output_buffer_ready_cbs_.push_back(cb);
}

void MediaCodecDecoder::AddOutputFormatChangedCb(const OutputFormatChangedCb& cb) {
    output_format_changed_cbs_.push_back(cb);
}

void MediaCodecDecoder::OnAsyncInputAvailable(int32_t idx) {
    std::lock_guard<std::mutex> lock(event_queue_mut_);
    event_queue_.push({.type = INPUT_AVAILABLE, .idx = idx});
    event_queue_cv_.notify_one();
}

void MediaCodecDecoder::OnAsyncOutputAvailable(int32_t idx, AMediaCodecBufferInfo* info) {
    std::lock_guard<std::mutex> lock(event_queue_mut_);
    event_queue_.push({.type = OUTPUT_AVAILABLE, .idx = idx, .info = *info});
    event_queue_cv_.notify_one();
}

void MediaCodecDecoder::OnAsyncFormatChanged(AMediaFormat* format) {
    std::lock_guard<std::mutex> lock(event_queue_mut_);
    event_queue_.push({.type = FORMAT_CHANGED});
    event_queue_cv_.notify_one();
}

void MediaCodecDecoder::Rewind() {
    encoded_data_helper_->Rewind();
    input_fragment_index_ = 0;
}

bool MediaCodecDecoder::Configure() {
    ALOGD("configure: mime=%s, width=%d, height=%d", GetMimeType(type_), input_visible_size_.width,
          input_visible_size_.height);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, GetMimeType(type_));
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, input_visible_size_.width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, input_visible_size_.height);
    media_status_t ret =
            AMediaCodec_configure(codec_, format, surface_, nullptr /* crtpto */, 0 /* flag */);
    AMediaFormat_delete(format);
    if (ret != AMEDIA_OK) {
        ALOGE("Configure return error: %d", ret);
        return false;
    }
    return true;
}

bool MediaCodecDecoder::Start() {
    media_status_t ret = AMediaCodec_start(codec_);
    if (ret != AMEDIA_OK) {
        ALOGE("Start return error: %d", ret);
        return false;
    }
    return true;
}

bool MediaCodecDecoder::Decode() {
    while (!output_done_) {
        CodecEvent evt;
        {
            std::unique_lock<std::mutex> lock(event_queue_mut_);
            while (event_queue_.empty()) {
                event_queue_cv_.wait(lock);
            }
            evt = event_queue_.front();
            event_queue_.pop();
        }

        bool success;
        switch (evt.type) {
        case INPUT_AVAILABLE:
            success = EnqueueInputBuffers(evt.idx);
            break;
        case OUTPUT_AVAILABLE:
            success = DequeueOutputBuffer(evt.idx, evt.info);
            break;
        case FORMAT_CHANGED:
            success = GetOutputFormat();
            break;
        }
        assert(success);
    }
    return true;
}

bool MediaCodecDecoder::EnqueueInputBuffers(int32_t index) {
    if (index < 0) {
        ALOGE("Unknown error while dequeueInputBuffer: %zd", index);
        return false;
    }

    if (looping_ && encoded_data_helper_->ReachEndOfStream()) {
        encoded_data_helper_->Rewind();
    }

    if (encoded_data_helper_->ReachEndOfStream()) {
        if (!FeedEOSInputBuffer(index)) return false;
        input_done_ = true;
    } else {
        if (!FeedInputBuffer(index)) return false;
    }
    return true;
}

bool MediaCodecDecoder::DequeueOutputBuffer(int32_t index, AMediaCodecBufferInfo info) {
    if (index < 0) {
        ALOGE("Unknown error while dequeueOutputBuffer: %zd", index);
        return false;
    }

    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) output_done_ = true;

    const uint64_t now = GetCurrentTimeNs();
    bool render_frame = render_on_release_;
    if (base_timestamp_ns_ == 0) {
        assert(received_outputs_ == 0);
        // The first output buffer is dequeued. Set the base timestamp.
        base_timestamp_ns_ = now;
    } else if (now > GetReleaseTimestampNs(received_outputs_)) {
        drop_frame_count_++;
        ALOGD("Drop frame #%d: deadline %" PRIu64 "us, actual %" PRIu64 "us", drop_frame_count_,
              (received_outputs_ * 1000000 / frame_rate_), (now - base_timestamp_ns_) / 1000);
        render_frame = false;  // We don't render the dropped frame.
    }

    if (!ReceiveOutputBuffer(index, info, render_frame)) return false;

    return true;
}

bool MediaCodecDecoder::Stop() {
    return AMediaCodec_stop(codec_) == AMEDIA_OK;
}

bool MediaCodecDecoder::FeedInputBuffer(size_t index) {
    assert(!encoded_data_helper_->ReachEndOfStream());

    size_t buf_size = 0;
    uint8_t* buf = AMediaCodec_getInputBuffer(codec_, index, &buf_size);
    if (!buf) {
        ALOGE("Failed to getInputBuffer: index=%zu", index);
        return false;
    }

    auto fragment = encoded_data_helper_->GetNextFragment();
    assert(fragment);

    if (buf_size < fragment->data.size()) {
        ALOGE("Input buffer size is not enough: buf_size=%zu, data_size=%zu", buf_size,
              fragment->data.size());
        return false;
    }

    memcpy(reinterpret_cast<char*>(buf), fragment->data.data(), fragment->data.size());

    uint32_t input_flag = 0;
    if (fragment->csd_flag) input_flag |= BUFFER_FLAG_CODEC_CONFIG;

    // We don't parse the display order of each bitstream buffer. Let's trust the order of received
    // output buffers from |codec_|.
    uint64_t timestamp_us = 0;

    ALOGV("queueInputBuffer(index=%zu, offset=0, size=%zu, time=%" PRIu64 ", flags=%u) #%d", index,
          fragment->data.size(), timestamp_us, input_flag, input_fragment_index_);
    media_status_t status = AMediaCodec_queueInputBuffer(
            codec_, index, 0 /* offset */, fragment->data.size(), timestamp_us, input_flag);
    if (status != AMEDIA_OK) {
        ALOGE("Failed to queueInputBuffer: %d", status);
        return false;
    }
    ++input_fragment_index_;
    return true;
}

bool MediaCodecDecoder::FeedEOSInputBuffer(size_t index) {
    // Timestamp of EOS input buffer is undefined, use 0 here to test decoder
    // robustness.
    uint64_t timestamp_us = 0;

    ALOGV("queueInputBuffer(index=%zu) EOS", index);
    media_status_t status =
            AMediaCodec_queueInputBuffer(codec_, index, 0 /* offset */, 0 /* size */, timestamp_us,
                                         AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
    if (status != AMEDIA_OK) {
        ALOGE("Failed to queueInputBuffer EOS: %d", status);
        return false;
    }
    return true;
}

bool MediaCodecDecoder::ReceiveOutputBuffer(size_t index, const AMediaCodecBufferInfo& info,
                                            bool render_buffer) {
    size_t out_size = 0;
    uint8_t* buf = nullptr;
    if (!surface_) {
        buf = AMediaCodec_getOutputBuffer(codec_, index, &out_size);
        if (!buf) {
            ALOGE("Failed to getOutputBuffer(index=%zu)", index);
            return false;
        }
    }

    received_outputs_++;
    ALOGV("ReceiveOutputBuffer(index=%zu, size=%d, flags=%u) #%d", index, info.size, info.flags,
          received_outputs_);

    // Do not callback for dummy EOS output (info.size == 0)
    if (info.size > 0) {
        for (const auto& callback : output_buffer_ready_cbs_)
            callback(buf, info.size, received_outputs_);
    }

    media_status_t status =
            render_buffer ? AMediaCodec_releaseOutputBufferAtTime(
                                    codec_, index, GetReleaseTimestampNs(received_outputs_))
                          : AMediaCodec_releaseOutputBuffer(codec_, index, false /* render */);
    if (status != AMEDIA_OK) {
        ALOGE("Failed to releaseOutputBuffer(index=%zu): %d", index, status);
        return false;
    }
    return true;
}

bool MediaCodecDecoder::GetOutputFormat() {
    AMediaFormat* format = AMediaCodec_getOutputFormat(codec_);
    bool success = true;

    // Required formats
    int32_t width = 0;
    if (!AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width)) {
        ALOGE("Cannot find width in format.");
        success = false;
    }

    int32_t height = 0;
    if (!AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height)) {
        ALOGE("Cannot find height in format.");
        success = false;
    }

    int32_t color_format = 0;
    if (!AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &color_format)) {
        ALOGE("Cannot find color-format in format.");
        success = false;
    }

    // Optional formats
    int32_t crop_left = 0;
    int32_t crop_top = 0;
    int32_t crop_right = width - 1;
    int32_t crop_bottom = height - 1;
    // Crop info is only avaiable on NDK version >= Pie.
    if (!AMediaFormat_getRect(format, AMEDIAFORMAT_KEY_DISPLAY_CROP, &crop_left, &crop_top,
                              &crop_right, &crop_bottom)) {
        ALOGD("Cannot find crop window in format. Set as large as frame size.");
        crop_left = 0;
        crop_top = 0;
        crop_right = width - 1;
        crop_bottom = height - 1;
    }

    // In current exiting ARC video decoder crop origin is always at (0,0)
    if (crop_left != 0 || crop_top != 0) {
        ALOGE("Crop origin is not (0,0): (%d,%d)", crop_left, crop_top);
        success = false;
    }

    int32_t stride = 0;
    if (!AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_STRIDE, &stride)) {
        ALOGD("Cannot find stride in format. Set as frame width.");
        stride = width;
    }

    int32_t slice_height = 0;
    if (!AMediaFormat_getInt32(format, FORMAT_KEY_SLICE_HEIGHT, &slice_height)) {
        ALOGD("Cannot find slice-height in format. Set as frame height.");
        slice_height = height;
    }

    for (const auto& callback : output_format_changed_cbs_) {
        callback(Size(stride, slice_height),
                 Size(crop_right - crop_left + 1, crop_bottom - crop_top + 1), color_format);
    }
    return success;
}

int64_t MediaCodecDecoder::GetReleaseTimestampNs(size_t frame_order) {
    assert(base_timestamp_ns_ != 0);

    return base_timestamp_ns_ + frame_order * kSecToNs / frame_rate_;
}

double MediaCodecDecoder::dropped_frame_rate() const {
    assert(received_outputs_ > 0);

    return (double)drop_frame_count_ / (double)received_outputs_;
}

}  // namespace android
