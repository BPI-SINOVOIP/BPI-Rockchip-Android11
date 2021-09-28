//#define LOG_NDEBUG 0
#define LOG_TAG "JpegDecoderTest"
#include <utils/Log.h>

#include <string.h>
#include <stdlib.h>
#include "MpiJpegDecoder.h"

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

    MpiJpegDecoder decoder;
    MpiJpegDecoder::OutputFrame_t frameOut;

    ret = decoder.prepareDecoder();
    if (!ret) {
        ALOGE("failed to prepare JPEG decoder");
        goto DECODE_OUT;
    }

    memset(&frameOut, 0, sizeof(MpiJpegDecoder::OutputFrame_t));
    ret = decoder.decodePacket(buf, size, &frameOut);
    if (!ret) {
        ALOGE("failed to decode packet");
        goto DECODE_OUT;
    }

    /* TODO - get diaplay for the frameOut.
          * - frame address: frameOut.MemVirAddr
          * - frame size: frameOut.OutputSize */

    /* Output frame buffers within limits, so release frame buffer if one
       frame has been display successful. */
    decoder.deinitOutputFrame(&frameOut);

DECODE_OUT:
    decoder.flushBuffer();

    if (buf)
        free(buf);

    return 0;
}
