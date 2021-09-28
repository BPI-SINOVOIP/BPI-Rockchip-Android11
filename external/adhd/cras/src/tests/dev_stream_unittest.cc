// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "audio_thread_log.h"
#include "byte_buffer.h"
#include "cras_audio_area.h"
#include "cras_rstream.h"
#include "cras_shm.h"
#include "cras_types.h"
#include "dev_stream.h"
}

namespace {

extern "C" {
struct audio_thread_event_log* atlog;
// For audio_thread_log.h use.
int atlog_rw_shm_fd;
int atlog_ro_shm_fd;
unsigned int max_frames_for_conversion(unsigned int stream_frames,
                                       unsigned int stream_rate,
                                       unsigned int device_rate);
};

static struct timespec clock_gettime_retspec;
static struct timespec cb_ts;

static const int kBufferFrames = 1024;
static const struct cras_audio_format fmt_s16le_44_1 = {
    SND_PCM_FORMAT_S16_LE,
    44100,
    2,
};
static const struct cras_audio_format fmt_s16le_48 = {
    SND_PCM_FORMAT_S16_LE,
    48000,
    2,
};
static const struct cras_audio_format fmt_s16le_48_mono = {
    SND_PCM_FORMAT_S16_LE,
    48000,
    1,
};
static const struct cras_audio_format fmt_s16le_8 = {
    SND_PCM_FORMAT_S16_LE,
    8000,
    2,
};

struct cras_audio_area_copy_call {
  const struct cras_audio_area* dst;
  unsigned int dst_offset;
  unsigned int dst_format_bytes;
  const struct cras_audio_area* src;
  unsigned int src_offset;
  float software_gain_scaler;
};

struct fmt_conv_call {
  struct cras_fmt_conv* conv;
  uint8_t* in_buf;
  uint8_t* out_buf;
  size_t in_frames;
  size_t out_frames;
};

struct mix_add_call {
  int16_t* dst;
  int16_t* src;
  unsigned int count;
  unsigned int index;
  int mute;
  float mix_vol;
};

struct rstream_get_readable_call {
  struct cras_rstream* rstream;
  unsigned int offset;
  unsigned int num_called;
};

static int config_format_converter_called;
static const struct cras_audio_format* config_format_converter_from_fmt;
static int config_format_converter_frames;
static struct cras_fmt_conv* config_format_converter_conv;
static struct cras_audio_format in_fmt;
static struct cras_audio_format out_fmt;
static struct cras_audio_area_copy_call copy_area_call;
static struct fmt_conv_call conv_frames_call;
static int cras_audio_area_create_num_channels_val;
static int cras_fmt_conversion_needed_val;
static int cras_fmt_conv_set_linear_resample_rates_called;
static float cras_fmt_conv_set_linear_resample_rates_from;
static float cras_fmt_conv_set_linear_resample_rates_to;

static unsigned int rstream_playable_frames_ret;
static struct mix_add_call mix_add_call;
static struct rstream_get_readable_call rstream_get_readable_call;
static unsigned int rstream_get_readable_num;
static uint8_t* rstream_get_readable_ptr;

static struct cras_audio_format* cras_rstream_post_processing_format_val;
static int cras_rstream_audio_ready_called;
static int cras_rstream_audio_ready_count;
static int cras_rstream_is_pending_reply_ret;
static int cras_rstream_flush_old_audio_messages_called;
static int cras_server_metrics_missed_cb_event_called;

static char* atlog_name;

class CreateSuite : public testing::Test {
 protected:
  virtual void SetUp() {
    in_fmt.format = SND_PCM_FORMAT_S16_LE;
    out_fmt.format = SND_PCM_FORMAT_S16_LE;
    in_fmt.num_channels = 2;
    out_fmt.num_channels = 2;

    SetupShm(&rstream_.shm);

    rstream_.stream_id = 0x10001;
    rstream_.buffer_frames = kBufferFrames;
    rstream_.cb_threshold = kBufferFrames / 2;
    rstream_.is_draining = 0;
    rstream_.stream_type = CRAS_STREAM_TYPE_DEFAULT;
    rstream_.direction = CRAS_STREAM_OUTPUT;
    rstream_.format.format = SND_PCM_FORMAT_S16_LE;
    rstream_.format.num_channels = 2;
    rstream_.format = fmt_s16le_44_1;
    rstream_.flags = 0;
    rstream_.num_missed_cb = 0;

    config_format_converter_from_fmt = NULL;
    config_format_converter_called = 0;
    cras_fmt_conversion_needed_val = 0;
    cras_fmt_conv_set_linear_resample_rates_called = 0;

    cras_rstream_audio_ready_called = 0;
    cras_rstream_audio_ready_count = 0;
    cras_rstream_is_pending_reply_ret = 0;
    cras_rstream_flush_old_audio_messages_called = 0;
    cras_server_metrics_missed_cb_event_called = 0;

    memset(&copy_area_call, 0xff, sizeof(copy_area_call));
    memset(&conv_frames_call, 0xff, sizeof(conv_frames_call));

    ASSERT_FALSE(asprintf(&atlog_name, "/ATlog-%d", getpid()) < 0);
    /* To avoid un-used variable warning. */
    atlog_rw_shm_fd = atlog_ro_shm_fd = -1;
    atlog = audio_thread_event_log_init(atlog_name);

    devstr.stream = &rstream_;
    devstr.conv = NULL;
    devstr.conv_buffer = NULL;
    devstr.conv_buffer_size_frames = 0;

    area = (struct cras_audio_area*)calloc(
        1, sizeof(*area) + 2 * sizeof(struct cras_channel_area));
    area->num_channels = 2;
    channel_area_set_channel(&area->channels[0], CRAS_CH_FL);
    channel_area_set_channel(&area->channels[1], CRAS_CH_FR);
    area->channels[0].step_bytes = 4;
    area->channels[0].buf = (uint8_t*)(cap_buf);
    area->channels[1].step_bytes = 4;
    area->channels[1].buf = (uint8_t*)(cap_buf + 1);
    area->frames = kBufferFrames;

    stream_area = (struct cras_audio_area*)calloc(
        1, sizeof(*area) + 2 * sizeof(struct cras_channel_area));
    stream_area->num_channels = 2;
    rstream_.audio_area = stream_area;
    int16_t* shm_samples = (int16_t*)rstream_.shm->samples;
    stream_area->channels[0].step_bytes = 4;
    stream_area->channels[0].buf = (uint8_t*)(shm_samples);
    stream_area->channels[1].step_bytes = 4;
    stream_area->channels[1].buf = (uint8_t*)(shm_samples + 1);
  }

  virtual void TearDown() {
    free(area);
    free(stream_area);
    free(rstream_.shm->header);
    free(rstream_.shm->samples);
    free(rstream_.shm);
    audio_thread_event_log_deinit(atlog, atlog_name);
    free(atlog_name);
  }

  void SetupShm(struct cras_audio_shm** shm_out) {
    int16_t* buf;
    struct cras_audio_shm* shm;
    uint32_t used_size;

    shm = static_cast<struct cras_audio_shm*>(
        calloc(1, sizeof(struct cras_audio_shm)));

    shm->header = static_cast<struct cras_audio_shm_header*>(
        calloc(1, sizeof(struct cras_audio_shm_header)));
    cras_shm_set_frame_bytes(shm, 4);
    used_size = kBufferFrames * cras_shm_frame_bytes(shm);
    cras_shm_set_used_size(shm, used_size);

    shm->samples = static_cast<uint8_t*>(
        calloc(1, cras_shm_calculate_samples_size(used_size)));
    shm->samples_info.length = cras_shm_calculate_samples_size(used_size);

    buf = (int16_t*)shm->samples;
    for (size_t i = 0; i < kBufferFrames * 2; i++)
      buf[i] = i;
    cras_shm_set_mute(shm, 0);
    cras_shm_set_volume_scaler(shm, 1.0);

    *shm_out = shm;
  }

  void SetUpFmtConv(unsigned int in_rate,
                    unsigned int out_rate,
                    unsigned int conv_buf_size) {
    in_fmt.frame_rate = in_rate;
    out_fmt.frame_rate = out_rate;
    cras_fmt_conversion_needed_val = 1;

    devstr.conv = (struct cras_fmt_conv*)0xdead;
    devstr.conv_buffer =
        (struct byte_buffer*)byte_buffer_create(conv_buf_size * 4);
    devstr.conv_buffer_size_frames = kBufferFrames * 2;

    devstr.conv_area = (struct cras_audio_area*)calloc(
        1, sizeof(*area) + 2 * sizeof(*area->channels));
    devstr.conv_area->num_channels = 2;
    devstr.conv_area->channels[0].step_bytes = 4;
    devstr.conv_area->channels[0].buf = (uint8_t*)(devstr.conv_buffer->bytes);
    devstr.conv_area->channels[1].step_bytes = 4;
    devstr.conv_area->channels[1].buf =
        (uint8_t*)(devstr.conv_buffer->bytes + 1);
  }

  struct dev_stream devstr;
  struct cras_audio_area* area;
  struct cras_audio_area* stream_area;
  int16_t cap_buf[kBufferFrames * 2];
  struct cras_rstream rstream_;
};

TEST_F(CreateSuite, CaptureNoSRC) {
  float software_gain_scaler = 10;

  dev_stream_capture(&devstr, area, 0, software_gain_scaler);

  EXPECT_EQ(stream_area, copy_area_call.dst);
  EXPECT_EQ(0, copy_area_call.dst_offset);
  EXPECT_EQ(4, copy_area_call.dst_format_bytes);
  EXPECT_EQ(area, copy_area_call.src);
  EXPECT_EQ(software_gain_scaler, copy_area_call.software_gain_scaler);
}

TEST_F(CreateSuite, CaptureSRCSmallConverterBuffer) {
  float software_gain_scaler = 10;
  unsigned int conv_buf_avail_at_input_rate;
  int nread;

  SetUpFmtConv(44100, 32000, kBufferFrames / 4);
  nread = dev_stream_capture(&devstr, area, 0, software_gain_scaler);

  // |nread| is bound by small converter buffer size (kBufferFrames / 4)
  conv_buf_avail_at_input_rate = cras_frames_at_rate(
      out_fmt.frame_rate, (kBufferFrames / 4), in_fmt.frame_rate);

  EXPECT_EQ(conv_buf_avail_at_input_rate, nread);
  EXPECT_EQ((struct cras_fmt_conv*)0xdead, conv_frames_call.conv);
  EXPECT_EQ((uint8_t*)cap_buf, conv_frames_call.in_buf);
  EXPECT_EQ(devstr.conv_buffer->bytes, conv_frames_call.out_buf);

  EXPECT_EQ(conv_buf_avail_at_input_rate, conv_frames_call.in_frames);

  // Expect number of output frames is limited by the size of converter buffer.
  EXPECT_EQ(kBufferFrames / 4, conv_frames_call.out_frames);

  EXPECT_EQ(stream_area, copy_area_call.dst);
  EXPECT_EQ(0, copy_area_call.dst_offset);
  EXPECT_EQ(4, copy_area_call.dst_format_bytes);
  EXPECT_EQ(devstr.conv_area, copy_area_call.src);
  EXPECT_EQ(software_gain_scaler, copy_area_call.software_gain_scaler);

  free(devstr.conv_area);
  byte_buffer_destroy(&devstr.conv_buffer);
}

TEST_F(CreateSuite, CaptureSRCLargeConverterBuffer) {
  float software_gain_scaler = 10;
  unsigned int stream_avail_at_input_rate;
  int nread;

  SetUpFmtConv(44100, 32000, kBufferFrames * 2);
  nread = dev_stream_capture(&devstr, area, 0, software_gain_scaler);

  // Available frames at stream side is bound by cb_threshold, which
  // equals to kBufferFrames / 2.
  stream_avail_at_input_rate = cras_frames_at_rate(
      out_fmt.frame_rate, (kBufferFrames / 2), in_fmt.frame_rate);

  EXPECT_EQ(stream_avail_at_input_rate, nread);
  EXPECT_EQ((struct cras_fmt_conv*)0xdead, conv_frames_call.conv);
  EXPECT_EQ((uint8_t*)cap_buf, conv_frames_call.in_buf);
  EXPECT_EQ(devstr.conv_buffer->bytes, conv_frames_call.out_buf);

  // Expect number of input frames is limited by |stream_avail_at_input_rate|
  // at format conversion.
  EXPECT_EQ(stream_avail_at_input_rate, conv_frames_call.in_frames);

  // Expect number of output frames is limited by the size of converter buffer.
  EXPECT_EQ(kBufferFrames * 2, conv_frames_call.out_frames);

  EXPECT_EQ(stream_area, copy_area_call.dst);
  EXPECT_EQ(0, copy_area_call.dst_offset);
  EXPECT_EQ(4, copy_area_call.dst_format_bytes);
  EXPECT_EQ(devstr.conv_area, copy_area_call.src);
  EXPECT_EQ(software_gain_scaler, copy_area_call.software_gain_scaler);

  free(devstr.conv_area);
  byte_buffer_destroy(&devstr.conv_buffer);
}

TEST_F(CreateSuite, CreateSRC44to48) {
  struct dev_stream* dev_stream;

  rstream_.format = fmt_s16le_44_1;
  in_fmt.frame_rate = 44100;   // Input to converter is stream rate.
  out_fmt.frame_rate = 48000;  // Output from converter is device rate.
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_48, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  // Converter tmp and output buffers are large enough for device output.
  unsigned int device_frames =
      cras_frames_at_rate(in_fmt.frame_rate, kBufferFrames, out_fmt.frame_rate);
  EXPECT_LE(kBufferFrames, device_frames);  // Sanity check.
  EXPECT_LE(device_frames, config_format_converter_frames);
  EXPECT_LE(device_frames, dev_stream->conv_buffer_size_frames);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, CreateSRC44from48Input) {
  struct dev_stream* dev_stream;
  struct cras_audio_format processed_fmt = fmt_s16le_48;

  processed_fmt.num_channels = 1;
  rstream_.format = fmt_s16le_44_1;
  rstream_.direction = CRAS_STREAM_INPUT;
  in_fmt.frame_rate = 48000;   // Input to converter is device rate.
  out_fmt.frame_rate = 44100;  // Output from converter is stream rate.
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  cras_rstream_post_processing_format_val = &processed_fmt;
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_48, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  // Converter tmp and output buffers are large enough for device input.
  unsigned int device_frames =
      cras_frames_at_rate(out_fmt.frame_rate, kBufferFrames, in_fmt.frame_rate);
  EXPECT_LE(kBufferFrames, device_frames);  // Sanity check.
  EXPECT_LE(device_frames, config_format_converter_frames);
  EXPECT_EQ(&processed_fmt, config_format_converter_from_fmt);
  EXPECT_LE(device_frames, dev_stream->conv_buffer_size_frames);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, CreateSRC48to44) {
  struct dev_stream* dev_stream;

  rstream_.format = fmt_s16le_48;
  in_fmt.frame_rate = 48000;   // Stream rate.
  out_fmt.frame_rate = 44100;  // Device rate.
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_44_1, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  // Converter tmp and output buffers are large enough for stream input.
  EXPECT_LE(kBufferFrames, config_format_converter_frames);
  EXPECT_LE(kBufferFrames, dev_stream->conv_buffer_size_frames);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, CreateSRC48from44Input) {
  struct dev_stream* dev_stream;

  rstream_.format = fmt_s16le_48;
  rstream_.direction = CRAS_STREAM_INPUT;
  in_fmt.frame_rate = 44100;   // Device rate.
  out_fmt.frame_rate = 48000;  // Stream rate.
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_44_1, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  // Converter tmp and output buffers are large enough for stream output.
  EXPECT_LE(kBufferFrames, config_format_converter_frames);
  EXPECT_LE(kBufferFrames, dev_stream->conv_buffer_size_frames);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, CreateSRC8to48) {
  struct dev_stream* dev_stream;

  rstream_.format = fmt_s16le_8;
  in_fmt.frame_rate = 8000;    // Stream rate.
  out_fmt.frame_rate = 48000;  // Device rate.
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_48, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  // Converter tmp and output buffers are large enough for device output.
  unsigned int device_frames =
      cras_frames_at_rate(in_fmt.frame_rate, kBufferFrames, out_fmt.frame_rate);
  EXPECT_LE(kBufferFrames, device_frames);  // Sanity check.
  EXPECT_LE(device_frames, config_format_converter_frames);
  EXPECT_LE(device_frames, dev_stream->conv_buffer_size_frames);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, CreateSRC8from48Input) {
  struct dev_stream* dev_stream;

  rstream_.format = fmt_s16le_8;
  rstream_.direction = CRAS_STREAM_INPUT;
  in_fmt.frame_rate = 48000;  // Device rate.
  out_fmt.frame_rate = 8000;  // Stream rate.
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_48, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  // Converter tmp and output buffers are large enough for device input.
  unsigned int device_frames =
      cras_frames_at_rate(out_fmt.frame_rate, kBufferFrames, in_fmt.frame_rate);
  EXPECT_LE(kBufferFrames, device_frames);  // Sanity check.
  EXPECT_LE(device_frames, config_format_converter_frames);
  EXPECT_LE(device_frames, dev_stream->conv_buffer_size_frames);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, CreateSRC48to8) {
  struct dev_stream* dev_stream;

  rstream_.format = fmt_s16le_48;
  in_fmt.frame_rate = 48000;  // Stream rate.
  out_fmt.frame_rate = 8000;  // Device rate.
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_8, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  // Converter tmp and output buffers are large enough for stream input.
  EXPECT_LE(kBufferFrames, config_format_converter_frames);
  EXPECT_LE(kBufferFrames, dev_stream->conv_buffer_size_frames);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, CreateSRC48from8Input) {
  struct dev_stream* dev_stream;

  rstream_.format = fmt_s16le_48;
  rstream_.direction = CRAS_STREAM_INPUT;
  in_fmt.frame_rate = 8000;    // Device rate.
  out_fmt.frame_rate = 48000;  // Stream rate.
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_8, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  // Converter tmp and output buffers are large enough for stream output.
  EXPECT_LE(kBufferFrames, config_format_converter_frames);
  EXPECT_LE(kBufferFrames, dev_stream->conv_buffer_size_frames);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, CreateSRC48MonoFrom44StereoInput) {
  struct dev_stream* dev_stream;

  rstream_.format = fmt_s16le_48_mono;
  rstream_.direction = CRAS_STREAM_INPUT;
  in_fmt.frame_rate = 44100;   // Device rate.
  out_fmt.frame_rate = 48000;  // Stream rate.
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_44_1, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  // Converter tmp and output buffers are large enough for stream output.
  EXPECT_LE(kBufferFrames, config_format_converter_frames);
  EXPECT_LE(kBufferFrames, dev_stream->conv_buffer_size_frames);
  EXPECT_EQ(dev_stream->conv_buffer_size_frames * 4,
            dev_stream->conv_buffer->max_size);
  EXPECT_EQ(2, cras_audio_area_create_num_channels_val);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, CaptureAvailConvBufHasSamples) {
  struct dev_stream* dev_stream;
  unsigned int avail;

  rstream_.format = fmt_s16le_48;
  rstream_.direction = CRAS_STREAM_INPUT;
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream =
      dev_stream_create(&rstream_, 0, &fmt_s16le_44_1, (void*)0x55, &cb_ts);
  EXPECT_EQ(1, config_format_converter_called);
  EXPECT_NE(static_cast<byte_buffer*>(NULL), dev_stream->conv_buffer);
  EXPECT_LE(
      cras_frames_at_rate(in_fmt.frame_rate, kBufferFrames, out_fmt.frame_rate),
      dev_stream->conv_buffer_size_frames);
  EXPECT_EQ(dev_stream->conv_buffer_size_frames * 4,
            dev_stream->conv_buffer->max_size);
  EXPECT_EQ(2, cras_audio_area_create_num_channels_val);

  buf_increment_write(dev_stream->conv_buffer, 50 * 4);
  avail = dev_stream_capture_avail(dev_stream);

  EXPECT_EQ(cras_frames_at_rate(48000, 512 - 50, 44100), avail);

  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, SetDevRateNotMasterDev) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;

  rstream_.format = fmt_s16le_48;
  rstream_.direction = CRAS_STREAM_INPUT;
  rstream_.master_dev.dev_id = 4;
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream = dev_stream_create(&rstream_, dev_id, &fmt_s16le_44_1,
                                 (void*)0x55, &cb_ts);

  dev_stream_set_dev_rate(dev_stream, 44100, 1.01, 1.0, 0);
  EXPECT_EQ(1, cras_fmt_conv_set_linear_resample_rates_called);
  EXPECT_EQ(44100, cras_fmt_conv_set_linear_resample_rates_from);
  EXPECT_EQ(44541, cras_fmt_conv_set_linear_resample_rates_to);

  dev_stream_set_dev_rate(dev_stream, 44100, 1.01, 1.0, 1);
  EXPECT_EQ(2, cras_fmt_conv_set_linear_resample_rates_called);
  EXPECT_EQ(44100, cras_fmt_conv_set_linear_resample_rates_from);
  EXPECT_LE(44541, cras_fmt_conv_set_linear_resample_rates_to);

  dev_stream_set_dev_rate(dev_stream, 44100, 1.0, 1.01, -1);
  EXPECT_EQ(3, cras_fmt_conv_set_linear_resample_rates_called);
  EXPECT_EQ(44100, cras_fmt_conv_set_linear_resample_rates_from);
  EXPECT_GE(43663, cras_fmt_conv_set_linear_resample_rates_to);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, SetDevRateMasterDev) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;
  unsigned int expected_ts_nsec;

  rstream_.format = fmt_s16le_48;
  rstream_.direction = CRAS_STREAM_INPUT;
  rstream_.master_dev.dev_id = dev_id;
  config_format_converter_conv = reinterpret_cast<struct cras_fmt_conv*>(0x33);
  dev_stream = dev_stream_create(&rstream_, dev_id, &fmt_s16le_44_1,
                                 (void*)0x55, &cb_ts);

  dev_stream_set_dev_rate(dev_stream, 44100, 1.01, 1.0, 0);
  EXPECT_EQ(1, cras_fmt_conv_set_linear_resample_rates_called);
  EXPECT_EQ(44100, cras_fmt_conv_set_linear_resample_rates_from);
  EXPECT_EQ(44100, cras_fmt_conv_set_linear_resample_rates_to);
  expected_ts_nsec = 1000000000.0 * kBufferFrames / 2.0 / 48000.0 / 1.01;
  EXPECT_EQ(0, rstream_.sleep_interval_ts.tv_sec);
  EXPECT_EQ(expected_ts_nsec, rstream_.sleep_interval_ts.tv_nsec);

  dev_stream_set_dev_rate(dev_stream, 44100, 1.01, 1.0, 1);
  EXPECT_EQ(2, cras_fmt_conv_set_linear_resample_rates_called);
  EXPECT_EQ(44100, cras_fmt_conv_set_linear_resample_rates_from);
  EXPECT_LE(44100, cras_fmt_conv_set_linear_resample_rates_to);
  expected_ts_nsec = 1000000000.0 * kBufferFrames / 2.0 / 48000.0 / 1.01;
  EXPECT_EQ(0, rstream_.sleep_interval_ts.tv_sec);
  EXPECT_EQ(expected_ts_nsec, rstream_.sleep_interval_ts.tv_nsec);

  dev_stream_set_dev_rate(dev_stream, 44100, 1.0, 1.33, -1);
  EXPECT_EQ(3, cras_fmt_conv_set_linear_resample_rates_called);
  EXPECT_EQ(44100, cras_fmt_conv_set_linear_resample_rates_from);
  EXPECT_GE(44100, cras_fmt_conv_set_linear_resample_rates_to);
  expected_ts_nsec = 1000000000.0 * kBufferFrames / 2.0 / 48000.0;
  EXPECT_EQ(0, rstream_.sleep_interval_ts.tv_sec);
  EXPECT_EQ(expected_ts_nsec, rstream_.sleep_interval_ts.tv_nsec);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, StreamMixNoFrames) {
  struct dev_stream dev_stream;
  struct cras_audio_format fmt;

  dev_stream.conv = NULL;
  rstream_playable_frames_ret = 0;
  fmt.num_channels = 2;
  fmt.format = SND_PCM_FORMAT_S16_LE;
  EXPECT_EQ(0, dev_stream_mix(&dev_stream, &fmt, 0, 3));
}

TEST_F(CreateSuite, StreamMixNoConv) {
  struct dev_stream dev_stream;
  const unsigned int nfr = 100;
  struct cras_audio_format fmt;

  dev_stream.conv = NULL;
  dev_stream.stream = reinterpret_cast<cras_rstream*>(0x5446);
  rstream_playable_frames_ret = nfr;
  rstream_get_readable_num = nfr;
  rstream_get_readable_ptr = reinterpret_cast<uint8_t*>(0x4000);
  rstream_get_readable_call.num_called = 0;
  fmt.num_channels = 2;
  fmt.format = SND_PCM_FORMAT_S16_LE;
  EXPECT_EQ(nfr, dev_stream_mix(&dev_stream, &fmt, (uint8_t*)0x5000, nfr));
  EXPECT_EQ((int16_t*)0x5000, mix_add_call.dst);
  EXPECT_EQ((int16_t*)0x4000, mix_add_call.src);
  EXPECT_EQ(200, mix_add_call.count);
  EXPECT_EQ(1, mix_add_call.index);
  EXPECT_EQ(dev_stream.stream, rstream_get_readable_call.rstream);
  EXPECT_EQ(0, rstream_get_readable_call.offset);
  EXPECT_EQ(1, rstream_get_readable_call.num_called);
}

TEST_F(CreateSuite, StreamMixNoConvTwoPass) {
  struct dev_stream dev_stream;
  const unsigned int nfr = 100;
  const unsigned int bytes_per_sample = 2;
  const unsigned int num_channels = 2;
  const unsigned int bytes_per_frame = bytes_per_sample * num_channels;
  struct cras_audio_format fmt;

  dev_stream.conv = NULL;
  dev_stream.stream = reinterpret_cast<cras_rstream*>(0x5446);
  rstream_playable_frames_ret = nfr;
  rstream_get_readable_num = nfr / 2;
  rstream_get_readable_ptr = reinterpret_cast<uint8_t*>(0x4000);
  rstream_get_readable_call.num_called = 0;
  fmt.num_channels = 2;
  fmt.format = SND_PCM_FORMAT_S16_LE;
  EXPECT_EQ(nfr, dev_stream_mix(&dev_stream, &fmt, (uint8_t*)0x5000, nfr));
  const unsigned int half_offset = nfr / 2 * bytes_per_frame;
  EXPECT_EQ((int16_t*)(0x5000 + half_offset), mix_add_call.dst);
  EXPECT_EQ((int16_t*)0x4000, mix_add_call.src);
  EXPECT_EQ(nfr / 2 * num_channels, mix_add_call.count);
  EXPECT_EQ(1, mix_add_call.index);
  EXPECT_EQ(dev_stream.stream, rstream_get_readable_call.rstream);
  EXPECT_EQ(nfr / 2, rstream_get_readable_call.offset);
  EXPECT_EQ(2, rstream_get_readable_call.num_called);
}

TEST_F(CreateSuite, DevStreamFlushAudioMessages) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;

  dev_stream = dev_stream_create(&rstream_, dev_id, &fmt_s16le_44_1,
                                 (void*)0x55, &cb_ts);

  dev_stream_flush_old_audio_messages(dev_stream);
  EXPECT_EQ(1, cras_rstream_flush_old_audio_messages_called);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, DevStreamIsPending) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;

  dev_stream = dev_stream_create(&rstream_, dev_id, &fmt_s16le_44_1,
                                 (void*)0x55, &cb_ts);

  // dev_stream_is_pending_reply is only a wrapper.
  cras_rstream_is_pending_reply_ret = 0;
  EXPECT_EQ(0, dev_stream_is_pending_reply(dev_stream));

  cras_rstream_is_pending_reply_ret = 1;
  EXPECT_EQ(1, dev_stream_is_pending_reply(dev_stream));

  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, StreamCanSend) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;
  int written_frames;
  int rc;
  struct timespec expected_next_cb_ts;

  rstream_.direction = CRAS_STREAM_INPUT;
  dev_stream = dev_stream_create(&rstream_, dev_id, &fmt_s16le_44_1,
                                 (void*)0x55, &cb_ts);

  // Assume there is a next_cb_ts on rstream.
  rstream_.next_cb_ts.tv_sec = 1;
  rstream_.next_cb_ts.tv_nsec = 0;

  // Case 1: Not enough samples. Time is not late enough.
  //         Stream can not send data to client.
  clock_gettime_retspec.tv_sec = 0;
  clock_gettime_retspec.tv_nsec = 0;
  rc = dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(0, cras_rstream_audio_ready_called);
  EXPECT_EQ(0, cras_server_metrics_missed_cb_event_called);
  EXPECT_EQ(0, rc);

  // Case 2: Not enough samples. Time is late enough.
  //         Stream can not send data to client.

  // Assume time is greater than next_cb_ts.
  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 500;
  // However, written frames is less than cb_threshold.
  // Stream still can not send samples to client.
  rc = dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(0, cras_rstream_audio_ready_called);
  EXPECT_EQ(0, cras_server_metrics_missed_cb_event_called);
  EXPECT_EQ(0, rc);

  // Case 3: Enough samples. Time is not late enough.
  //         Stream can not send data to client.

  // Assume time is less than next_cb_ts.
  clock_gettime_retspec.tv_sec = 0;
  clock_gettime_retspec.tv_nsec = 0;
  // Enough samples are written.
  written_frames = rstream_.cb_threshold + 10;
  cras_shm_buffer_written(rstream_.shm, written_frames);
  // Stream still can not send samples to client.
  rc = dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(0, cras_rstream_audio_ready_called);
  EXPECT_EQ(0, cras_server_metrics_missed_cb_event_called);
  EXPECT_EQ(0, rc);

  // Case 4: Enough samples. Time is late enough.
  //         Stream should send one cb_threshold to client.
  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 500;
  rc = dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(1, cras_rstream_audio_ready_called);
  EXPECT_EQ(rstream_.cb_threshold, cras_rstream_audio_ready_count);
  EXPECT_EQ(0, cras_server_metrics_missed_cb_event_called);
  EXPECT_EQ(0, rc);

  // Check next_cb_ts is increased by one sleep interval.
  expected_next_cb_ts.tv_sec = 1;
  expected_next_cb_ts.tv_nsec = 0;
  add_timespecs(&expected_next_cb_ts, &rstream_.sleep_interval_ts);
  EXPECT_EQ(expected_next_cb_ts.tv_sec, rstream_.next_cb_ts.tv_sec);
  EXPECT_EQ(expected_next_cb_ts.tv_nsec, rstream_.next_cb_ts.tv_nsec);

  // Reset stub data of interest.
  cras_rstream_audio_ready_called = 0;
  cras_rstream_audio_ready_count = 0;
  rstream_.next_cb_ts.tv_sec = 1;
  rstream_.next_cb_ts.tv_nsec = 0;

  // Case 5: Enough samples. Time is late enough and it is too late
  //         such that a new next_cb_ts is in the past.
  //         Stream should send one cb_threshold to client and reset schedule.
  clock_gettime_retspec.tv_sec = 2;
  clock_gettime_retspec.tv_nsec = 0;
  rc = dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(1, cras_rstream_audio_ready_called);
  EXPECT_EQ(rstream_.cb_threshold, cras_rstream_audio_ready_count);
  EXPECT_EQ(1, cras_server_metrics_missed_cb_event_called);
  EXPECT_EQ(0, rc);

  // Check next_cb_ts is rest to be now plus one sleep interval.
  expected_next_cb_ts.tv_sec = 2;
  expected_next_cb_ts.tv_nsec = 0;
  add_timespecs(&expected_next_cb_ts, &rstream_.sleep_interval_ts);
  EXPECT_EQ(expected_next_cb_ts.tv_sec, rstream_.next_cb_ts.tv_sec);
  EXPECT_EQ(expected_next_cb_ts.tv_nsec, rstream_.next_cb_ts.tv_nsec);

  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, StreamCanSendBulkAudio) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;
  int written_frames;
  int rc;
  struct timespec expected_next_cb_ts;

  rstream_.direction = CRAS_STREAM_INPUT;
  rstream_.flags |= BULK_AUDIO_OK;
  dev_stream = dev_stream_create(&rstream_, dev_id, &fmt_s16le_44_1,
                                 (void*)0x55, &cb_ts);

  // Assume there is a next_cb_ts on rstream.
  rstream_.next_cb_ts.tv_sec = 1;
  rstream_.next_cb_ts.tv_nsec = 0;

  // Case 1: Not enough samples. Time is not late enough.
  //         Bulk audio stream can not send data to client.
  clock_gettime_retspec.tv_sec = 0;
  clock_gettime_retspec.tv_nsec = 0;
  rc = dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(0, cras_rstream_audio_ready_called);
  EXPECT_EQ(0, rc);

  // Case 2: Not enough samples. Time is late enough.
  //         Bulk audio stream can not send data to client.

  // Assume time is greater than next_cb_ts.
  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 500;
  // However, written frames is less than cb_threshold.
  // Stream still can not send samples to client.
  rc = dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(0, cras_rstream_audio_ready_called);
  EXPECT_EQ(0, rc);

  // Case 3: Enough samples. Time is not late enough.
  //         Bulk audio stream CAN send data to client.

  // Assume time is less than next_cb_ts.
  clock_gettime_retspec.tv_sec = 0;
  clock_gettime_retspec.tv_nsec = 0;
  // Enough samples are written.
  written_frames = rstream_.cb_threshold + 10;
  cras_shm_buffer_written(rstream_.shm, written_frames);
  // Bulk audio stream can send all written samples to client.
  rc = dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(1, cras_rstream_audio_ready_called);
  EXPECT_EQ(written_frames, cras_rstream_audio_ready_count);
  EXPECT_EQ(0, rc);

  // Case 4: Enough samples. Time is late enough.
  //         Bulk audio stream can send all written samples to client.

  // Reset stub data of interest.
  cras_rstream_audio_ready_called = 0;
  cras_rstream_audio_ready_count = 0;
  rstream_.next_cb_ts.tv_sec = 1;
  rstream_.next_cb_ts.tv_nsec = 0;

  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 500;
  rc = dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(1, cras_rstream_audio_ready_called);
  EXPECT_EQ(written_frames, cras_rstream_audio_ready_count);
  EXPECT_EQ(0, rc);

  // Check next_cb_ts is increased by one sleep interval.
  expected_next_cb_ts.tv_sec = 1;
  expected_next_cb_ts.tv_nsec = 0;
  add_timespecs(&expected_next_cb_ts, &rstream_.sleep_interval_ts);
  EXPECT_EQ(expected_next_cb_ts.tv_sec, rstream_.next_cb_ts.tv_sec);
  EXPECT_EQ(expected_next_cb_ts.tv_nsec, rstream_.next_cb_ts.tv_nsec);

  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, TriggerOnlyStreamSendOnlyOnce) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;

  rstream_.direction = CRAS_STREAM_INPUT;
  dev_stream = dev_stream_create(&rstream_, dev_id, &fmt_s16le_44_1,
                                 (void*)0x55, &cb_ts);
  dev_stream->stream->flags = TRIGGER_ONLY;
  dev_stream->stream->triggered = 0;

  // Check first trigger callback called.
  cras_shm_buffer_written(rstream_.shm, rstream_.cb_threshold);
  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 0;
  dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(1, cras_rstream_audio_ready_called);
  EXPECT_EQ(1, dev_stream->stream->triggered);

  // No future callback will be called for TRIGGER_ONLY streams.
  cras_shm_buffer_written(rstream_.shm, rstream_.cb_threshold);
  clock_gettime_retspec.tv_sec = 2;
  clock_gettime_retspec.tv_nsec = 0;
  dev_stream_capture_update_rstream(dev_stream);
  EXPECT_EQ(1, cras_rstream_audio_ready_called);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, InputDevStreamWakeTimeByNextCbTs) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;
  int rc;
  unsigned int curr_level = 0;
  int written_frames;
  struct timespec level_tstamp = {.tv_sec = 1, .tv_nsec = 0};
  struct timespec wake_time_out = {.tv_sec = 0, .tv_nsec = 0};

  rstream_.direction = CRAS_STREAM_INPUT;
  dev_stream = dev_stream_create(&rstream_, dev_id, &fmt_s16le_44_1,
                                 (void*)0x55, &cb_ts);

  // Assume there is a next_cb_ts on rstream.
  rstream_.next_cb_ts.tv_sec = 1;
  rstream_.next_cb_ts.tv_nsec = 500000;

  // Assume there are enough samples for stream.
  written_frames = rstream_.cb_threshold + 10;
  cras_shm_buffer_written(rstream_.shm, written_frames);

  rc = dev_stream_wake_time(dev_stream, curr_level, &level_tstamp,
                            rstream_.cb_threshold, 0, &wake_time_out);

  // The next wake up time is determined by next_cb_ts on dev_stream.
  EXPECT_EQ(rstream_.next_cb_ts.tv_sec, wake_time_out.tv_sec);
  EXPECT_EQ(rstream_.next_cb_ts.tv_nsec, wake_time_out.tv_nsec);
  EXPECT_EQ(0, rc);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, InputDevStreamWakeTimeByDevice) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;
  int rc;
  unsigned int curr_level = 100;
  int written_frames;
  struct timespec level_tstamp = {.tv_sec = 1, .tv_nsec = 0};
  struct timespec wake_time_out = {.tv_sec = 0, .tv_nsec = 0};
  struct timespec expected_tstamp = {.tv_sec = 0, .tv_nsec = 0};
  struct timespec needed_time_for_device = {.tv_sec = 0, .tv_nsec = 0};
  int needed_frames_from_device = 0;

  rstream_.direction = CRAS_STREAM_INPUT;
  dev_stream =
      dev_stream_create(&rstream_, dev_id, &fmt_s16le_48, (void*)0x55, &cb_ts);

  // Assume there is a next_cb_ts on rstream, that is, 1.005 seconds.
  rstream_.next_cb_ts.tv_sec = 1;
  rstream_.next_cb_ts.tv_nsec = 5000000;  // 5ms

  // Assume there are not enough samples for stream.
  written_frames = 123;
  cras_shm_buffer_written(rstream_.shm, written_frames);

  // Compute wake up time for device level to reach enough samples
  // for one cb_threshold:
  // Device has 100 samples (48K rate).
  // Stream has 123 samples (44.1K rate)
  // cb_threshold = 512 samples.
  // Stream needs 512 - 123 = 389 samples.
  // Converted to device rate => 389 * 48000.0 / 44100 = 423.4 samples
  // => 424 samples.
  // Device needs another 424 - 100 = 324 samples.
  // Time for 252 samples = 324 / 48000 = 0.00675 sec.
  // So expected wake up time for samples is at level_tstamp + 0.00675 sec =
  // 1.00675 seconds.
  needed_frames_from_device =
      cras_frames_at_rate(44100, rstream_.cb_threshold - written_frames, 48000);
  needed_frames_from_device -= curr_level;
  cras_frames_to_time(needed_frames_from_device, 48000,
                      &needed_time_for_device);

  expected_tstamp.tv_sec = level_tstamp.tv_sec;
  expected_tstamp.tv_nsec = level_tstamp.tv_nsec;

  add_timespecs(&expected_tstamp, &needed_time_for_device);

  // Set the stub data for cras_fmt_conv_out_frames_to_in.
  out_fmt.frame_rate = 44100;
  in_fmt.frame_rate = 48000;

  rc = dev_stream_wake_time(dev_stream, curr_level, &level_tstamp,
                            rstream_.cb_threshold, 0, &wake_time_out);

  // The next wake up time is determined by needed time for device level
  // to reach enough samples for one cb_threshold.
  EXPECT_EQ(expected_tstamp.tv_sec, wake_time_out.tv_sec);
  EXPECT_EQ(expected_tstamp.tv_nsec, wake_time_out.tv_nsec);
  EXPECT_EQ(0, rc);

  // Assume current level is larger than cb_threshold.
  // The wake up time is determined by next_cb_ts.
  curr_level += rstream_.cb_threshold;
  rc = dev_stream_wake_time(dev_stream, curr_level, &level_tstamp,
                            rstream_.cb_threshold, 0, &wake_time_out);
  EXPECT_EQ(rstream_.next_cb_ts.tv_sec, wake_time_out.tv_sec);
  EXPECT_EQ(rstream_.next_cb_ts.tv_nsec, wake_time_out.tv_nsec);
  EXPECT_EQ(0, rc);
  dev_stream_destroy(dev_stream);
}

TEST_F(CreateSuite, UpdateNextWakeTime) {
  struct dev_stream* dev_stream;
  unsigned int dev_id = 9;
  struct timespec expected_next_cb_ts;

  rstream_.direction = CRAS_STREAM_OUTPUT;
  dev_stream = dev_stream_create(&rstream_, dev_id, &fmt_s16le_44_1,
                                 (void*)0x55, &cb_ts);

  // Case 1: The new next_cb_ts is greater than now. Do not need to reschedule.
  rstream_.next_cb_ts.tv_sec = 2;
  rstream_.next_cb_ts.tv_nsec = 0;
  clock_gettime_retspec.tv_sec = 2;
  clock_gettime_retspec.tv_nsec = 500;
  expected_next_cb_ts = rstream_.next_cb_ts;

  dev_stream_update_next_wake_time(dev_stream);
  EXPECT_EQ(0, cras_server_metrics_missed_cb_event_called);
  add_timespecs(&expected_next_cb_ts, &rstream_.sleep_interval_ts);
  EXPECT_EQ(expected_next_cb_ts.tv_sec, rstream_.next_cb_ts.tv_sec);
  EXPECT_EQ(expected_next_cb_ts.tv_nsec, rstream_.next_cb_ts.tv_nsec);

  // Case 2: The new next_cb_ts is less than now. Need to reset schedule.
  rstream_.next_cb_ts.tv_sec = 2;
  rstream_.next_cb_ts.tv_nsec = 0;
  clock_gettime_retspec.tv_sec = 3;
  clock_gettime_retspec.tv_nsec = 0;
  expected_next_cb_ts = clock_gettime_retspec;

  dev_stream_update_next_wake_time(dev_stream);
  EXPECT_EQ(1, cras_server_metrics_missed_cb_event_called);
  add_timespecs(&expected_next_cb_ts, &rstream_.sleep_interval_ts);
  EXPECT_EQ(expected_next_cb_ts.tv_sec, rstream_.next_cb_ts.tv_sec);
  EXPECT_EQ(expected_next_cb_ts.tv_nsec, rstream_.next_cb_ts.tv_nsec);
  dev_stream_destroy(dev_stream);
}

//  Test set_playback_timestamp.
TEST(DevStreamTimimg, SetPlaybackTimeStampSimple) {
  struct cras_timespec ts;

  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 0;
  cras_set_playback_timestamp(48000, 24000, &ts);
  EXPECT_EQ(1, ts.tv_sec);
  EXPECT_GE(ts.tv_nsec, 499900000);
  EXPECT_LE(ts.tv_nsec, 500100000);
}

TEST(DevStreamTimimg, SetPlaybackTimeStampWrap) {
  struct cras_timespec ts;

  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 750000000;
  cras_set_playback_timestamp(48000, 24000, &ts);
  EXPECT_EQ(2, ts.tv_sec);
  EXPECT_GE(ts.tv_nsec, 249900000);
  EXPECT_LE(ts.tv_nsec, 250100000);
}

TEST(DevStreamTimimg, SetPlaybackTimeStampWrapTwice) {
  struct cras_timespec ts;

  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 750000000;
  cras_set_playback_timestamp(48000, 72000, &ts);
  EXPECT_EQ(3, ts.tv_sec);
  EXPECT_GE(ts.tv_nsec, 249900000);
  EXPECT_LE(ts.tv_nsec, 250100000);
}

//  Test set_capture_timestamp.
TEST(DevStreamTimimg, SetCaptureTimeStampSimple) {
  struct cras_timespec ts;

  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 750000000;
  cras_set_capture_timestamp(48000, 24000, &ts);
  EXPECT_EQ(1, ts.tv_sec);
  EXPECT_GE(ts.tv_nsec, 249900000);
  EXPECT_LE(ts.tv_nsec, 250100000);
}

TEST(DevStreamTimimg, SetCaptureTimeStampWrap) {
  struct cras_timespec ts;

  clock_gettime_retspec.tv_sec = 1;
  clock_gettime_retspec.tv_nsec = 0;
  cras_set_capture_timestamp(48000, 24000, &ts);
  EXPECT_EQ(0, ts.tv_sec);
  EXPECT_GE(ts.tv_nsec, 499900000);
  EXPECT_LE(ts.tv_nsec, 500100000);
}

TEST(DevStreamTimimg, SetCaptureTimeStampWrapPartial) {
  struct cras_timespec ts;

  clock_gettime_retspec.tv_sec = 2;
  clock_gettime_retspec.tv_nsec = 750000000;
  cras_set_capture_timestamp(48000, 72000, &ts);
  EXPECT_EQ(1, ts.tv_sec);
  EXPECT_GE(ts.tv_nsec, 249900000);
  EXPECT_LE(ts.tv_nsec, 250100000);
}

TEST(MaxFramesForConverter, 8to48) {
  EXPECT_EQ(481, max_frames_for_conversion(80,       // Stream frames.
                                           8000,     // Stream rate.
                                           48000));  // Device rate.
}

TEST(MaxFramesForConverter, 48to8) {
  EXPECT_EQ(81, max_frames_for_conversion(80,      // Stream frames.
                                          48000,   // Stream rate.
                                          8000));  // Device rate.
}

/* Stubs */
extern "C" {

int cras_rstream_audio_ready(struct cras_rstream* stream, size_t count) {
  cras_rstream_audio_ready_count = count;
  cras_rstream_audio_ready_called++;
  return 0;
}

int cras_rstream_request_audio(struct cras_rstream* stream,
                               const struct timespec* now) {
  return 0;
}

void cras_rstream_record_fetch_interval(struct cras_rstream* rstream,
                                        const struct timespec* now) {}

void cras_rstream_update_input_write_pointer(struct cras_rstream* rstream) {}

void cras_rstream_update_output_read_pointer(struct cras_rstream* rstream) {}

void cras_rstream_dev_offset_update(struct cras_rstream* rstream,
                                    unsigned int frames,
                                    unsigned int dev_id) {}

void cras_rstream_dev_attach(struct cras_rstream* rstream,
                             unsigned int dev_id,
                             void* dev_ptr) {}

void cras_rstream_dev_detach(struct cras_rstream* rstream,
                             unsigned int dev_id) {}

unsigned int cras_rstream_dev_offset(const struct cras_rstream* rstream,
                                     unsigned int dev_id) {
  return 0;
}

unsigned int cras_rstream_playable_frames(struct cras_rstream* rstream,
                                          unsigned int dev_id) {
  return rstream_playable_frames_ret;
}

float cras_rstream_get_volume_scaler(struct cras_rstream* rstream) {
  return 1.0;
}

uint8_t* cras_rstream_get_readable_frames(struct cras_rstream* rstream,
                                          unsigned int offset,
                                          size_t* frames) {
  rstream_get_readable_call.rstream = rstream;
  rstream_get_readable_call.offset = offset;
  rstream_get_readable_call.num_called++;
  *frames = rstream_get_readable_num;
  return rstream_get_readable_ptr;
}

int cras_rstream_get_mute(const struct cras_rstream* rstream) {
  return 0;
}
void cras_rstream_update_queued_frames(struct cras_rstream* rstream) {}

struct cras_audio_format* cras_rstream_post_processing_format(
    const struct cras_rstream* stream,
    void* dev_ptr) {
  return cras_rstream_post_processing_format_val;
}

int config_format_converter(struct cras_fmt_conv** conv,
                            enum CRAS_STREAM_DIRECTION dir,
                            const struct cras_audio_format* from,
                            const struct cras_audio_format* to,
                            unsigned int frames) {
  config_format_converter_called++;
  config_format_converter_from_fmt = from;
  config_format_converter_frames = frames;
  *conv = config_format_converter_conv;
  return 0;
}

void cras_fmt_conv_destroy(struct cras_fmt_conv* conv) {}

size_t cras_fmt_conv_convert_frames(struct cras_fmt_conv* conv,
                                    uint8_t* in_buf,
                                    uint8_t* out_buf,
                                    unsigned int* in_frames,
                                    unsigned int out_frames) {
  unsigned int ret;
  conv_frames_call.conv = conv;
  conv_frames_call.in_buf = in_buf;
  conv_frames_call.out_buf = out_buf;
  conv_frames_call.in_frames = *in_frames;
  ret = cras_frames_at_rate(in_fmt.frame_rate, *in_frames, out_fmt.frame_rate);
  conv_frames_call.out_frames = out_frames;
  if (ret > out_frames) {
    ret = out_frames;
    *in_frames =
        cras_frames_at_rate(out_fmt.frame_rate, ret, in_fmt.frame_rate);
  }

  return ret;
}

void cras_mix_add(snd_pcm_format_t fmt,
                  uint8_t* dst,
                  uint8_t* src,
                  unsigned int count,
                  unsigned int index,
                  int mute,
                  float mix_vol) {
  mix_add_call.dst = (int16_t*)dst;
  mix_add_call.src = (int16_t*)src;
  mix_add_call.count = count;
  mix_add_call.index = index;
  mix_add_call.mute = mute;
  mix_add_call.mix_vol = mix_vol;
}

struct cras_audio_area* cras_audio_area_create(int num_channels) {
  cras_audio_area_create_num_channels_val = num_channels;
  return NULL;
}

void cras_audio_area_destroy(struct cras_audio_area* area) {}

void cras_audio_area_config_buf_pointers(struct cras_audio_area* area,
                                         const struct cras_audio_format* fmt,
                                         uint8_t* base_buffer) {}

void cras_audio_area_config_channels(struct cras_audio_area* area,
                                     const struct cras_audio_format* fmt) {}

unsigned int cras_audio_area_copy(const struct cras_audio_area* dst,
                                  unsigned int dst_offset,
                                  const struct cras_audio_format* dst_fmt,
                                  const struct cras_audio_area* src,
                                  unsigned int src_offset,
                                  float software_gain_scaler) {
  copy_area_call.dst = dst;
  copy_area_call.dst_offset = dst_offset;
  copy_area_call.dst_format_bytes = cras_get_format_bytes(dst_fmt);
  copy_area_call.src = src;
  copy_area_call.src_offset = src_offset;
  copy_area_call.software_gain_scaler = software_gain_scaler;
  return src->frames;
}

size_t cras_fmt_conv_in_frames_to_out(struct cras_fmt_conv* conv,
                                      size_t in_frames) {
  return cras_frames_at_rate(in_fmt.frame_rate, in_frames, out_fmt.frame_rate);
}

size_t cras_fmt_conv_out_frames_to_in(struct cras_fmt_conv* conv,
                                      size_t out_frames) {
  return cras_frames_at_rate(out_fmt.frame_rate, out_frames, in_fmt.frame_rate);
}

const struct cras_audio_format* cras_fmt_conv_in_format(
    const struct cras_fmt_conv* conv) {
  return &in_fmt;
}

const struct cras_audio_format* cras_fmt_conv_out_format(
    const struct cras_fmt_conv* conv) {
  return &out_fmt;
}

int cras_fmt_conversion_needed(const struct cras_fmt_conv* conv) {
  return cras_fmt_conversion_needed_val;
}

void cras_fmt_conv_set_linear_resample_rates(struct cras_fmt_conv* conv,
                                             float from,
                                             float to) {
  cras_fmt_conv_set_linear_resample_rates_from = from;
  cras_fmt_conv_set_linear_resample_rates_to = to;
  cras_fmt_conv_set_linear_resample_rates_called++;
}

int cras_rstream_is_pending_reply(const struct cras_rstream* stream) {
  return cras_rstream_is_pending_reply_ret;
}

int cras_rstream_flush_old_audio_messages(struct cras_rstream* stream) {
  cras_rstream_flush_old_audio_messages_called++;
  return 0;
}

int cras_server_metrics_missed_cb_event(const struct cras_rstream* stream) {
  cras_server_metrics_missed_cb_event_called++;
  return 0;
}

//  From librt.
int clock_gettime(clockid_t clk_id, struct timespec* tp) {
  tp->tv_sec = clock_gettime_retspec.tv_sec;
  tp->tv_nsec = clock_gettime_retspec.tv_nsec;
  return 0;
}

}  // extern "C"

}  //  namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
