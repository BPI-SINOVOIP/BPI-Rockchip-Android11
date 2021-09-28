#include <stddef.h>
#include <stdio.h>
#include "oi_codec_sbc.h"

#define CODEC_DATA_WORDS(numChannels, numBuffers)                              \
  (((sizeof(int32_t) * SBC_MAX_BLOCKS * (numChannels)*SBC_MAX_BANDS) +         \
    (sizeof(SBC_BUFFER_T) * SBC_MAX_CHANNELS * SBC_MAX_BANDS * (numBuffers)) + \
    (sizeof(uint32_t) - 1)) /                                                  \
   sizeof(uint32_t))

#define SBC_CODEC_FAST_FILTER_BUFFERS 27

#define SBC_MAX_CHANNELS 2
#define SBC_MAX_BANDS 8
#define SBC_MAX_BLOCKS 16
/* Minimum size of the bit allocation pool used to encode the stream */
#define SBC_MIN_BITPOOL 2
/* Maximum size of the bit allocation pool used to encode the stream */
#define SBC_MAX_BITPOOL 250
#define SBC_MAX_ONE_CHANNEL_BPS 320000
#define SBC_MAX_TWO_CHANNEL_BPS 512000

#define SBC_WBS_BITRATE 62000
#define SBC_WBS_BITPOOL 27
#define SBC_WBS_NROF_BLOCKS 16
#define SBC_WBS_FRAME_LEN 62
#define SBC_WBS_SAMPLES_PER_FRAME 128

#define SBC_HEADER_LEN 4
#define SBC_MAX_SAMPLES_PER_FRAME (SBC_MAX_BANDS * SBC_MAX_BLOCKS)

static OI_CODEC_SBC_DECODER_CONTEXT btif_a2dp_sink_context;
static uint32_t btif_a2dp_sink_context_data[CODEC_DATA_WORDS(
    2, SBC_CODEC_FAST_FILTER_BUFFERS)];

static int16_t
    btif_a2dp_sink_pcm_data[15 * SBC_MAX_SAMPLES_PER_FRAME * SBC_MAX_CHANNELS];

int LLVMFuzzerInitialize(int argc, char** argv) {
  (void)argc;
  (void)argv;
  OI_CODEC_SBC_DecoderReset(&btif_a2dp_sink_context,
                            btif_a2dp_sink_context_data,
                            sizeof(btif_a2dp_sink_context_data), 2, 2, 0);

  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* buf, size_t len) {
  uint32_t pcmBytes, availPcmBytes;
  int16_t* pcmDataPointer =
      btif_a2dp_sink_pcm_data; /* Will be overwritten on next packet receipt */
  availPcmBytes = sizeof(btif_a2dp_sink_pcm_data);

  pcmBytes = availPcmBytes;
  OI_CODEC_SBC_DecodeFrame(&btif_a2dp_sink_context, (const OI_BYTE**)&buf,
                           (uint32_t*)&len, (int16_t*)pcmDataPointer,
                           (uint32_t*)&pcmBytes);

  return 0;
}
