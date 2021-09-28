// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #define LOG_NDEBUG 0
#define LOG_TAG "Decoder_E2E"

#include <getopt.h>

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <utils/Log.h>

#include "common.h"
#include "e2e_test_jni.h"
#include "mediacodec_decoder.h"
#include "video_frame.h"

namespace android {

// Environment to store test video data for all test cases.
class C2VideoDecoderTestEnvironment;

namespace {
C2VideoDecoderTestEnvironment* g_env;

}  // namespace

class C2VideoDecoderTestEnvironment : public testing::Environment {
public:
    C2VideoDecoderTestEnvironment(bool loop, bool use_sw_decoder, const std::string& data,
                                  const std::string& output_frames_path, ANativeWindow* surface,
                                  ConfigureCallback* cb)
          : loop_(loop),
            use_sw_decoder_(use_sw_decoder),
            test_video_data_(data),
            output_frames_path_(output_frames_path),
            surface_(surface),
            configure_cb_(cb) {}

    void SetUp() override { ParseTestVideoData(); }

    // The syntax of test video data is:
    // "input_file_path:width:height:num_frames:num_fragments:min_fps_render:
    //  min_fps_no_render:video_codec_profile[:output_file_path]"
    // - |input_file_path| is compressed video stream in H264 Annex B (NAL) format
    //   (H264) or IVF (VP8/9).
    // - |width| and |height| are visible frame size in pixels.
    // - |num_frames| is the number of picture frames for the input stream.
    // - |num_fragments| is the number of AU (H264) or frame (VP8/9) in the input
    //   stream. (Unused. Test will automatically parse the number.)
    // - |min_fps_render| and |min_fps_no_render| are minimum frames/second speeds
    //   expected to be achieved with and without rendering respective.
    //   (The former is unused because no rendering case here.)
    //   (The latter is Optional.)
    // - |video_codec_profile| is the VideoCodecProfile set during Initialization.
    // - |frame_rate| the expected framerate of the video.
    void ParseTestVideoData() {
        std::vector<std::string> fields = SplitString(test_video_data_, ':');
        ASSERT_EQ(fields.size(), 9U)
                << "The number of fields of test_video_data is not 9: " << test_video_data_;

        input_file_path_ = fields[0];
        int width = std::stoi(fields[1]);
        int height = std::stoi(fields[2]);
        visible_size_ = Size(width, height);
        ASSERT_FALSE(visible_size_.IsEmpty());

        configure_cb_->OnSizeChanged(width, height);

        num_frames_ = std::stoi(fields[3]);
        ASSERT_GT(num_frames_, 0);

        // Unused fields[4] --> num_fragments
        // Unused fields[5] --> min_fps_render

        if (!fields[6].empty()) {
            min_fps_no_render_ = std::stoi(fields[6]);
        }

        video_codec_profile_ = static_cast<VideoCodecProfile>(std::stoi(fields[7]));
        ASSERT_NE(VideoCodecProfileToType(video_codec_profile_), VideoCodecType::UNKNOWN);

        frame_rate_ = std::stoi(fields[8]);
    }

    // Get the corresponding frame-wise golden MD5 file path.
    std::string GoldenMD5FilePath() const { return input_file_path_ + ".frames.md5"; }

    std::string output_frames_path() const { return output_frames_path_; }

    std::string input_file_path() const { return input_file_path_; }
    Size visible_size() const { return visible_size_; }
    int num_frames() const { return num_frames_; }
    int min_fps_no_render() const { return min_fps_no_render_; }
    VideoCodecProfile video_codec_profile() const { return video_codec_profile_; }
    int frame_rate() const { return frame_rate_; }
    ConfigureCallback* configure_cb() const { return configure_cb_; }
    bool loop() const { return loop_; }
    bool use_sw_decoder() const { return use_sw_decoder_; }

    ANativeWindow* surface() const { return surface_; }

protected:
    bool loop_;
    bool use_sw_decoder_;
    std::string test_video_data_;
    std::string output_frames_path_;

    std::string input_file_path_;
    Size visible_size_;
    int num_frames_ = 0;
    int min_fps_no_render_ = 0;
    VideoCodecProfile video_codec_profile_;
    int frame_rate_ = 0;

    ANativeWindow* surface_;
    ConfigureCallback* configure_cb_;
};

// The struct to record output formats.
struct OutputFormat {
    Size coded_size;
    Size visible_size;
    int32_t color_format = 0;
};

// The helper class to validate video frame by MD5 and output to I420 raw stream
// if needed.
class VideoFrameValidator {
public:
    VideoFrameValidator() = default;
    ~VideoFrameValidator() { output_file_.close(); }

    // Set |md5_golden_path| as the path of golden frame-wise MD5 file. Return
    // false if the file is failed to read.
    bool SetGoldenMD5File(const std::string& md5_golden_path) {
        golden_md5_file_ = std::unique_ptr<InputFileASCII>(new InputFileASCII(md5_golden_path));
        return golden_md5_file_->IsValid();
    }

    // Set |output_frames_path| as the path for output raw I420 stream. Return
    // false if the file is failed to open.
    bool SetOutputFile(const std::string& output_frames_path) {
        if (output_frames_path.empty()) return false;

        output_file_.open(output_frames_path, std::ofstream::binary);
        if (!output_file_.is_open()) {
            printf("[ERR] Failed to open file: %s\n", output_frames_path.c_str());
            return false;
        }
        printf("[LOG] Decode output to file: %s\n", output_frames_path.c_str());
        write_to_file_ = true;
        return true;
    }

    // Callback function of output buffer ready to validate frame data by
    // VideoFrameValidator, write into file if needed.
    void VerifyMD5(const uint8_t* data, size_t buffer_size, int output_index) {
        ASSERT_TRUE(data != nullptr);

        std::string golden;
        ASSERT_TRUE(golden_md5_file_ && golden_md5_file_->IsValid());
        ASSERT_TRUE(golden_md5_file_->ReadLine(&golden))
                << "Failed to read golden MD5 at frame#" << output_index;

        std::unique_ptr<VideoFrame> video_frame =
                VideoFrame::Create(data, buffer_size, output_format_.coded_size,
                                   output_format_.visible_size, output_format_.color_format);
        ASSERT_TRUE(video_frame) << "Failed to create video frame on VerifyMD5 at frame#"
                                 << output_index;

        ASSERT_TRUE(video_frame->VerifyMD5(golden)) << "MD5 mismatched at frame#" << output_index;

        // Update color_format.
        output_format_.color_format = video_frame->color_format();
    }

    // Callback function of output buffer ready to validate frame data by
    // VideoFrameValidator, write into file if needed.
    void OutputToFile(const uint8_t* data, size_t buffer_size, int output_index) {
        if (!write_to_file_) return;

        ASSERT_TRUE(data != nullptr);

        std::unique_ptr<VideoFrame> video_frame =
                VideoFrame::Create(data, buffer_size, output_format_.coded_size,
                                   output_format_.visible_size, output_format_.color_format);
        ASSERT_TRUE(video_frame) << "Failed to create video frame on OutputToFile at frame#"
                                 << output_index;
        if (!video_frame->WriteFrame(&output_file_)) {
            printf("[ERR] Failed to write output buffer into file.\n");
            // Stop writing frames to file once it is failed.
            write_to_file_ = false;
        }
    }

    // Callback function of output format changed to update output format.
    void UpdateOutputFormat(const Size& coded_size, const Size& visible_size,
                            int32_t color_format) {
        output_format_.coded_size = coded_size;
        output_format_.visible_size = visible_size;
        output_format_.color_format = color_format;
    }

private:
    // The wrapper of input MD5 golden file.
    std::unique_ptr<InputFileASCII> golden_md5_file_;
    // The output file to write the decoded raw video.
    std::ofstream output_file_;

    // Only output video frame to file if True.
    bool write_to_file_ = false;
    // This records output format, color_format might be revised in flexible
    // format case.
    OutputFormat output_format_;
};

class C2VideoDecoderE2ETest : public testing::Test {
public:
    //  Callback function of output buffer ready to count frame.
    void CountFrame(const uint8_t* /* data */, size_t /* buffer_size */, int /* output_index */) {
        decoded_frames_++;
    }

    // Callback function of output format changed to verify output format.
    void VerifyOutputFormat(const Size& coded_size, const Size& visible_size,
                            int32_t color_format) {
        ASSERT_FALSE(coded_size.IsEmpty());
        ASSERT_FALSE(visible_size.IsEmpty());
        ASSERT_LE(visible_size.width, coded_size.width);
        ASSERT_LE(visible_size.height, coded_size.height);
        printf("[LOG] Got format changed { coded_size: %dx%d, visible_size: %dx%d, "
               "color_format: 0x%x\n",
               coded_size.width, coded_size.height, visible_size.width, visible_size.height,
               color_format);
        output_format_.coded_size = coded_size;
        output_format_.visible_size = visible_size;
        output_format_.color_format = color_format;
    }

    virtual bool useSurface() = 0;
    virtual bool renderOnRelease() = 0;

protected:
    void SetUp() override {
        ANativeWindow* surface = useSurface() ? g_env->surface() : nullptr;
        decoder_ = MediaCodecDecoder::Create(g_env->input_file_path(), g_env->video_codec_profile(),
                                             g_env->use_sw_decoder(), g_env->visible_size(),
                                             g_env->frame_rate(), surface, renderOnRelease(),
                                             g_env->loop());

        ASSERT_TRUE(decoder_);
        g_env->configure_cb()->OnDecoderReady(decoder_.get());

        decoder_->Rewind();

        ASSERT_TRUE(decoder_->Configure());
        ASSERT_TRUE(decoder_->Start());

        decoder_->AddOutputBufferReadyCb(std::bind(&C2VideoDecoderE2ETest::CountFrame, this,
                                                   std::placeholders::_1, std::placeholders::_2,
                                                   std::placeholders::_3));
        decoder_->AddOutputFormatChangedCb(std::bind(&C2VideoDecoderE2ETest::VerifyOutputFormat,
                                                     this, std::placeholders::_1,
                                                     std::placeholders::_2, std::placeholders::_3));
    }

    void TearDown() override {
        if (!decoder_) {
            return;
        }
        EXPECT_TRUE(decoder_->Stop());

        EXPECT_EQ(g_env->visible_size().width, output_format_.visible_size.width);
        EXPECT_EQ(g_env->visible_size().height, output_format_.visible_size.height);

        if (g_env->loop()) {
            EXPECT_EQ(decoded_frames_ % g_env->num_frames(), 0);
        } else {
            EXPECT_EQ(g_env->num_frames(), decoded_frames_);
        }

        g_env->configure_cb()->OnDecoderReady(nullptr);
        decoder_.reset();
    }

    void TestFPSBody();

    // The wrapper of the mediacodec decoder.
    std::unique_ptr<MediaCodecDecoder> decoder_;

    // The counter of obtained decoded output frames.
    int decoded_frames_ = 0;
    // This records formats from output format change.
    OutputFormat output_format_;
};

class C2VideoDecoderSurfaceE2ETest : public C2VideoDecoderE2ETest {
private:
    bool useSurface() override { return true; }
    bool renderOnRelease() override { return true; }
};

class C2VideoDecoderSurfaceNoRenderE2ETest : public C2VideoDecoderE2ETest {
private:
    bool useSurface() override { return true; }
    bool renderOnRelease() override { return false; }
};

class C2VideoDecoderByteBufferE2ETest : public C2VideoDecoderE2ETest {
private:
    bool useSurface() override { return false; }
    bool renderOnRelease() override { return false; }
};

TEST_F(C2VideoDecoderByteBufferE2ETest, TestSimpleDecode) {
    VideoFrameValidator video_frame_validator;

    ASSERT_TRUE(video_frame_validator.SetGoldenMD5File(g_env->GoldenMD5FilePath()))
            << "Failed to open MD5 file: " << g_env->GoldenMD5FilePath();

    decoder_->AddOutputBufferReadyCb(std::bind(&VideoFrameValidator::VerifyMD5,
                                               &video_frame_validator, std::placeholders::_1,
                                               std::placeholders::_2, std::placeholders::_3));

    if (video_frame_validator.SetOutputFile(g_env->output_frames_path())) {
        decoder_->AddOutputBufferReadyCb(std::bind(&VideoFrameValidator::OutputToFile,
                                                   &video_frame_validator, std::placeholders::_1,
                                                   std::placeholders::_2, std::placeholders::_3));
    }

    decoder_->AddOutputFormatChangedCb(std::bind(&VideoFrameValidator::UpdateOutputFormat,
                                                 &video_frame_validator, std::placeholders::_1,
                                                 std::placeholders::_2, std::placeholders::_3));

    EXPECT_TRUE(decoder_->Decode());
}

void C2VideoDecoderE2ETest::TestFPSBody() {
    FPSCalculator fps_calculator;
    auto callback = [&fps_calculator](const uint8_t* /* data */, size_t /* buffer_size */,
                                      int /* output_index */) {
        ASSERT_TRUE(fps_calculator.RecordFrameTimeDiff());
    };

    decoder_->AddOutputBufferReadyCb(callback);

    EXPECT_TRUE(decoder_->Decode());

    double fps = fps_calculator.CalculateFPS();
    printf("[LOG] Measured decoder FPS: %.4f\n", fps);
    EXPECT_GE(fps, static_cast<double>(g_env->min_fps_no_render()));
    printf("[LOG] Dropped frames rate: %lf\n", decoder_->dropped_frame_rate());
}

TEST_F(C2VideoDecoderSurfaceE2ETest, TestFPS) {
    TestFPSBody();
}

TEST_F(C2VideoDecoderSurfaceNoRenderE2ETest, TestFPS) {
    TestFPSBody();
}

}  // namespace android

bool GetOption(int argc, char** argv, std::string* test_video_data, std::string* output_frames_path,
               bool* loop, bool* use_sw_decoder) {
    const char* const optstring = "t:o:";
    static const struct option opts[] = {
            {"test_video_data", required_argument, nullptr, 't'},
            {"output_frames_path", required_argument, nullptr, 'o'},
            {"loop", no_argument, nullptr, 'l'},
            {"use_sw_decoder", no_argument, nullptr, 's'},
            {nullptr, 0, nullptr, 0},
    };

    int opt;
    optind = 1;
    while ((opt = getopt_long(argc, argv, optstring, opts, nullptr)) != -1) {
        switch (opt) {
        case 't':
            *test_video_data = optarg;
            break;
        case 'o':
            *output_frames_path = optarg;
            break;
        case 'l':
            *loop = true;
            break;
        case 's':
            *use_sw_decoder = true;
            break;
        default:
            printf("[WARN] Unknown option: getopt_long() returned code 0x%x.\n", opt);
            break;
        }
    }

    if (test_video_data->empty()) {
        printf("[ERR] Please assign test video data by --test_video_data\n");
        return false;
    }
    return true;
}

int RunDecoderTests(char** test_args, int test_args_count, ANativeWindow* surface,
                    android::ConfigureCallback* cb) {
    std::string test_video_data;
    std::string output_frames_path;
    bool loop = false;
    bool use_sw_decoder = false;
    if (!GetOption(test_args_count, test_args, &test_video_data, &output_frames_path, &loop,
                   &use_sw_decoder)) {
        ALOGE("GetOption failed");
        return EXIT_FAILURE;
    }

    if (android::g_env == nullptr) {
        android::g_env = reinterpret_cast<android::C2VideoDecoderTestEnvironment*>(
                testing::AddGlobalTestEnvironment(new android::C2VideoDecoderTestEnvironment(
                        loop, use_sw_decoder, test_video_data, output_frames_path, surface, cb)));
    } else {
        ALOGE("Trying to reuse test process");
        return EXIT_FAILURE;
    }
    testing::InitGoogleTest(&test_args_count, test_args);

    return RUN_ALL_TESTS();
}
