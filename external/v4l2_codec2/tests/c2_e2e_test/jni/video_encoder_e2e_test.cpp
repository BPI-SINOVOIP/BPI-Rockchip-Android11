// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>
#include <inttypes.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <utils/Log.h>

#include "common.h"
#include "mediacodec_encoder.h"

namespace android {

// Environment to store test stream data for all test cases.
class C2VideoEncoderTestEnvironment;

namespace {
// Default initial bitrate.
const unsigned int kDefaultBitrate = 2000000;
// Default ratio of requested_subsequent_bitrate_ to initial_bitrate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentBitrateRatio = 2.0;
// Default initial framerate.
const unsigned int kDefaultFramerate = 30;
// Default ratio of requested_subsequent_framerate_ to initial_framerate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentFramerateRatio = 0.1;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
const double kBitrateTolerance = 0.1;
// The minimum number of encoded frames. If the frame number of the input stream
// is less than this value, then we circularly encode the input stream.
constexpr size_t kMinNumEncodedFrames = 300;
// The percentiles to measure for encode latency.
constexpr int kLoggedLatencyPercentiles[] = {50, 75, 95};

C2VideoEncoderTestEnvironment* g_env;
}  // namespace

// Store the arguments passed from command line.
struct CmdlineArgs {
    std::string test_stream_data;
    bool run_at_fps = false;
    size_t num_encoded_frames = 0;
};

class C2VideoEncoderTestEnvironment : public testing::Environment {
public:
    explicit C2VideoEncoderTestEnvironment(const CmdlineArgs& args) : args_(args) {}

    void SetUp() override { ParseTestStreamData(); }

    // The syntax of test stream is:
    // "input_file_path:width:height:profile:output_file_path:requested_bitrate
    //  :requested_framerate:requestedSubsequentBitrate
    //  :requestedSubsequentFramerate:pixelFormat"
    // - |input_file_path| is YUV raw stream. Its format must be |pixelFormat|
    //   (see http://www.fourcc.org/yuv.php#IYUV).
    // - |width| and |height| are in pixels.
    // - |profile| to encode into (values of VideoCodecProfile).
    //   NOTE: Only H264PROFILE_MAIN(1) is supported. Now we ignore this value.
    // - |output_file_path| filename to save the encoded stream to (optional).
    //   The format for H264 is Annex-B byte stream.
    // - |requested_bitrate| requested bitrate in bits per second.
    //   Bitrate is only forced for tests that test bitrate.
    // - |requested_framerate| requested initial framerate.
    // - |requestedSubsequentBitrate| bitrate to switch to in the middle of the
    //   stream. NOTE: This value is not supported yet.
    // - |requestedSubsequentFramerate| framerate to switch to in the middle of
    //   the stream. NOTE: This value is not supported yet.
    // - |pixelFormat| is the VideoPixelFormat of |input_file_path|.
    //   NOTE: Only PIXEL_FORMAT_I420 is supported. Now we just ignore this value.
    void ParseTestStreamData() {
        std::vector<std::string> fields = SplitString(args_.test_stream_data, ':');
        ALOG_ASSERT(fields.size() >= 3U, "The fields of test_stream_data is not enough: %s",
                    args_.test_stream_data.c_str());
        ALOG_ASSERT(fields.size() <= 10U, "The fields of test_stream_data is too much: %s",
                    args_.test_stream_data.c_str());

        input_file_path_ = fields[0];
        int width = std::stoi(fields[1]);
        int height = std::stoi(fields[2]);
        visible_size_ = Size(width, height);
        ASSERT_FALSE(visible_size_.IsEmpty());

        if (fields.size() >= 4 && !fields[3].empty()) {
            int profile = stoi(fields[3]);
            if (profile != 1) printf("[WARN] Only H264PROFILE_MAIN(1) is supported.\n");
        }

        if (fields.size() >= 5 && !fields[4].empty()) {
            output_file_path_ = fields[4];
        }

        if (fields.size() >= 6 && !fields[5].empty()) {
            requested_bitrate_ = stoi(fields[5]);
            ASSERT_GT(requested_bitrate_, 0);
        } else {
            requested_bitrate_ = kDefaultBitrate;
        }

        if (fields.size() >= 7 && !fields[6].empty()) {
            requested_framerate_ = std::stoi(fields[6]);
            ASSERT_GT(requested_framerate_, 0);
        } else {
            requested_framerate_ = kDefaultFramerate;
        }

        if (fields.size() >= 8 && !fields[7].empty()) {
            requested_subsequent_bitrate_ = std::stoi(fields[7]);
            ASSERT_GT(requested_subsequent_bitrate_, 0);
        } else {
            requested_subsequent_bitrate_ = requested_bitrate_ * kDefaultSubsequentBitrateRatio;
        }

        if (fields.size() >= 9 && !fields[8].empty()) {
            requested_subsequent_framerate_ = std::stoi(fields[8]);
            ASSERT_GT(requested_subsequent_framerate_, 0);
        } else {
            requested_subsequent_framerate_ =
                    requested_framerate_ * kDefaultSubsequentFramerateRatio;
        }

        if (fields.size() >= 10 && !fields[9].empty()) {
            int format = std::stoi(fields[9]);
            if (format != 1 /* PIXEL_FORMAT_I420 */) printf("[WARN] Only I420 is suppported.\n");
        }
    }

    Size visible_size() const { return visible_size_; }
    std::string input_file_path() const { return input_file_path_; }
    std::string output_file_path() const { return output_file_path_; }
    int requested_bitrate() const { return requested_bitrate_; }
    int requested_framerate() const { return requested_framerate_; }
    int requested_subsequent_bitrate() const { return requested_subsequent_bitrate_; }
    int requested_subsequent_framerate() const { return requested_subsequent_framerate_; }

    bool run_at_fps() const { return args_.run_at_fps; }
    size_t num_encoded_frames() const { return args_.num_encoded_frames; }

private:
    const CmdlineArgs args_;

    Size visible_size_;
    std::string input_file_path_;
    std::string output_file_path_;

    int requested_bitrate_;
    int requested_framerate_;
    int requested_subsequent_bitrate_;
    int requested_subsequent_framerate_;
};

class C2VideoEncoderE2ETest : public testing::Test {
public:
    // Callback functions of getting output buffers from encoder.
    void WriteOutputBufferToFile(const uint8_t* data, const AMediaCodecBufferInfo& info) {
        if (output_file_.is_open()) {
            output_file_.write(reinterpret_cast<const char*>(data), info.size);
            if (output_file_.fail()) {
                printf("[ERR] Failed to write encoded buffer into file.\n");
            }
        }
    }

    void AccumulateOutputBufferSize(const uint8_t* /* data */, const AMediaCodecBufferInfo& info) {
        total_output_buffer_size_ += info.size;
    }

protected:
    void SetUp() override {
        encoder_ = MediaCodecEncoder::Create(g_env->input_file_path(), g_env->visible_size());
        ASSERT_TRUE(encoder_);
        encoder_->Rewind();

        ASSERT_TRUE(encoder_->Configure(static_cast<int32_t>(g_env->requested_bitrate()),
                                        static_cast<int32_t>(g_env->requested_framerate())));
        ASSERT_TRUE(encoder_->Start());
    }

    void TearDown() override {
        EXPECT_TRUE(encoder_->Stop());

        output_file_.close();
        encoder_.reset();
    }

    bool CreateOutputFile() {
        if (g_env->output_file_path().empty()) return false;

        output_file_.open(g_env->output_file_path(), std::ofstream::binary);
        if (!output_file_.is_open()) {
            printf("[ERR] Failed to open file: %s\n", g_env->output_file_path().c_str());
            return false;
        }
        return true;
    }

    double CalculateAverageBitrate(size_t num_frames, unsigned int framerate) const {
        return 1.0f * total_output_buffer_size_ * 8 / num_frames * framerate;
    }

    // The wrapper of the mediacodec encoder.
    std::unique_ptr<MediaCodecEncoder> encoder_;

    // The output file to write the encoded video bitstream.
    std::ofstream output_file_;
    // Used to accumulate the output buffer size.
    size_t total_output_buffer_size_;
};

class LatencyRecorder {
public:
    void OnEncodeInputBuffer(uint64_t time_us) {
        auto res = start_times_.insert(std::make_pair(time_us, GetNowUs()));
        ASSERT_TRUE(res.second);
    }

    void OnOutputBufferReady(const uint8_t* /* data */, const AMediaCodecBufferInfo& info) {
        // Ignore the CSD buffer and the empty EOS buffer.
        if (!(info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) && info.size != 0)
            end_times_[info.presentationTimeUs] = GetNowUs();
    }

    void PrintResult() const {
        std::vector<int64_t> latency_times;
        for (auto const& start_kv : start_times_) {
            auto end_it = end_times_.find(start_kv.first);
            ASSERT_TRUE(end_it != end_times_.end());
            latency_times.push_back(end_it->second - start_kv.second);
        }
        std::sort(latency_times.begin(), latency_times.end());

        for (int percentile : kLoggedLatencyPercentiles) {
            size_t index =
                    static_cast<size_t>(std::ceil(0.01f * percentile * latency_times.size())) - 1;
            printf("Encode latency for the %dth percentile: %" PRId64 " us\n", percentile,
                   latency_times[index]);
        }
    }

private:
    // Measured time of enqueueing input buffers and dequeueing output buffers.
    // The key is the timestamp of the frame.
    std::map<uint64_t, int64_t> start_times_;
    std::map<uint64_t, int64_t> end_times_;
};

TEST_F(C2VideoEncoderE2ETest, TestSimpleEncode) {
    // Write the output buffers to file.
    if (CreateOutputFile()) {
        encoder_->SetOutputBufferReadyCb(std::bind(&C2VideoEncoderE2ETest::WriteOutputBufferToFile,
                                                   this, std::placeholders::_1,
                                                   std::placeholders::_2));
    }
    encoder_->set_run_at_fps(g_env->run_at_fps());
    if (g_env->num_encoded_frames()) encoder_->set_num_encoded_frames(g_env->num_encoded_frames());

    EXPECT_TRUE(encoder_->Encode());
}

TEST_F(C2VideoEncoderE2ETest, TestBitrate) {
    // Ensure the number of encoded frames is enough for bitrate test case.
    encoder_->set_num_encoded_frames(
            std::max(encoder_->num_encoded_frames(), kMinNumEncodedFrames));

    // Accumulate the size of the output buffers.
    total_output_buffer_size_ = 0;
    encoder_->SetOutputBufferReadyCb(std::bind(&C2VideoEncoderE2ETest::AccumulateOutputBufferSize,
                                               this, std::placeholders::_1, std::placeholders::_2));

    // TODO(akahuang): Verify bitrate switch at the middle of stream.
    EXPECT_TRUE(encoder_->Encode());

    double measured_bitrate =
            CalculateAverageBitrate(encoder_->num_encoded_frames(), g_env->requested_framerate());
    EXPECT_NEAR(measured_bitrate, g_env->requested_bitrate(),
                kBitrateTolerance * g_env->requested_bitrate());
}

TEST_F(C2VideoEncoderE2ETest, PerfFPS) {
    FPSCalculator fps_calculator;
    auto callback = [&fps_calculator](const uint8_t* /* data */,
                                      const AMediaCodecBufferInfo& /* info */) {
        ASSERT_TRUE(fps_calculator.RecordFrameTimeDiff());
    };

    // Record frame time differences of the output buffers.
    encoder_->SetOutputBufferReadyCb(callback);

    EXPECT_TRUE(encoder_->Encode());

    double measured_fps = fps_calculator.CalculateFPS();
    printf("Measured encoder FPS: %.4f\n", measured_fps);
}

TEST_F(C2VideoEncoderE2ETest, PerfLatency) {
    LatencyRecorder recorder;
    encoder_->SetEncodeInputBufferCb(
            std::bind(&LatencyRecorder::OnEncodeInputBuffer, &recorder, std::placeholders::_1));
    encoder_->SetOutputBufferReadyCb(std::bind(&LatencyRecorder::OnOutputBufferReady, &recorder,
                                               std::placeholders::_1, std::placeholders::_2));
    encoder_->set_run_at_fps(true);

    EXPECT_TRUE(encoder_->Encode());

    recorder.PrintResult();
}

}  // namespace android

bool GetOption(int argc, char** argv, android::CmdlineArgs* args) {
    const char* const optstring = "t:rn:";
    static const struct option opts[] = {
            {"test_stream_data", required_argument, nullptr, 't'},
            {"run_at_fps", no_argument, nullptr, 'r'},
            {"num_encoded_frames", required_argument, nullptr, 'n'},
            {nullptr, 0, nullptr, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, optstring, opts, nullptr)) != -1) {
        switch (opt) {
        case 't':
            args->test_stream_data = optarg;
            break;
        case 'r':
            args->run_at_fps = true;
            break;
        case 'n':
            args->num_encoded_frames = static_cast<size_t>(atoi(optarg));
            break;
        default:
            printf("[WARN] Unknown option: getopt_long() returned code 0x%x.\n", opt);
            break;
        }
    }

    if (args->test_stream_data.empty()) {
        printf("[ERR] Please assign test stream data by --test_stream_data\n");
        return false;
    }
    return true;
}

int RunEncoderTests(char** test_args, int test_args_count) {
    android::CmdlineArgs args;
    if (!GetOption(test_args_count, test_args, &args)) return EXIT_FAILURE;

    android::g_env = reinterpret_cast<android::C2VideoEncoderTestEnvironment*>(
            testing::AddGlobalTestEnvironment(new android::C2VideoEncoderTestEnvironment(args)));
    testing::InitGoogleTest(&test_args_count, test_args);
    return RUN_ALL_TESTS();
}
