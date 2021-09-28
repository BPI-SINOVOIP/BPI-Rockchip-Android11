//#define LOG_NDEBUG 0
#define LOG_TAG "JpegEncoderTest"
#include <utils/Log.h>

#include <string.h>
#include <stdlib.h>
#include "MpiJpegEncoder.h"

#define PACKET_SIZE             2048

int main()
{
    bool ret = false;
    char *buf = NULL;
    size_t size = PACKET_SIZE;

    buf = (char *)malloc(size);
    if (NULL == buf) {
        ALOGE("dec_test malloc failed");
        return 0;
    }

    MpiJpegEncoder encoder;
    MpiJpegEncoder::OutputPacket_t pktOut;

    ret = encoder.prepareEncoder();
    if (!ret) {
        ALOGE("failed to prepare JPEG encoder");
        goto ENCODE_OUT;
    }

    ret = encoder.updateEncodeCfg(720 /*width*/, 1080 /*height*/,
                                  MpiJpegEncoder::INPUT_FMT_YUV420SP);
    if (!ret) {
        ALOGE("failed to update encode config");
        goto ENCODE_OUT;
    }

    memset(&pktOut, 0, sizeof(MpiJpegEncoder::OutputPacket_t));
    ret = encoder.encodeFrame(buf, &pktOut);
    if (!ret) {
        ALOGE("failed to encode packet");
        goto ENCODE_OUT;
    }

    /* TODO - Get diaplay for the PacketOut.
       * - Pakcet address: pktOut.data
       * - Pakcet size: pktOut.size */

    /* Output frame buffers within limits, so release frame buffer if one
       frame has been display successful. */
    encoder.deinitOutputPacket(&pktOut);

ENCODE_OUT:
    encoder.flushBuffer();

    if (buf)
        free(buf);

    return 0;
}
