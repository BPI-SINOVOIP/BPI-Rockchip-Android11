/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * author: kevin.chen@rock-chips.com
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "JpegParser"
#include <utils/Log.h>

#include "JpegParser.h"
#include "BitReader.h"

static MPP_RET jpegd_skip_section(BitReader *br)
{
    uint32_t len = 0;

    if (br->numBitsLeft() < 2)
        return MPP_ERR_READ_BIT;

    len = br->getBits(16);
    if (len < 2 /* invalid marker */ || (uint32_t)len - 2 > br->numBitsLeft()) {
        /* too short length or bytes is not enough */
        return MPP_ERR_READ_BIT;
    }

    if (len > 2)
        br->skipBits((len - 2) * 8);

    return MPP_OK;
}

/* return the 8 bit start code value and update the search
   state. Return -1 if no start code found */
int32_t jpeg_find_marker(const uint8_t **pbuf_ptr, const uint8_t *buf_end)
{
    const uint8_t *buf_ptr;
    unsigned int v, v2;
    int val;
    int skipped = 0;

    buf_ptr = *pbuf_ptr;
    while (buf_end - buf_ptr > 1) {
        v  = *buf_ptr++;
        v2 = *buf_ptr;
        if ((v == 0xff) && (v2 >= 0xc0) && (v2 <= 0xfe) && buf_ptr < buf_end) {
            val = *buf_ptr++;
            goto found;
        } else if ((v == 0x89) && (v2 == 0x50)) {
            ALOGV("input img maybe png format,check it");
        }
        skipped++;
    }
    buf_ptr = buf_end;
    val = -1;

found:
    ALOGV("find_marker skipped %d bytes", skipped);
    *pbuf_ptr = buf_ptr;
    return val;
}

MPP_RET jpeg_decode_sof(BitReader *br, uint32_t *out_width, uint32_t *out_height)
{
    uint32_t len, bits;
    uint32_t width, height, nb_components;

    len = br->getBits(16);
    if (len > br->numBitsLeft()) {
        ALOGE("sof0: len %d is too large", len);
        return MPP_NOK;
    }

    bits = br->getBits(8);
    if (bits > 16 || bits < 1) {
        /* usually bits is 8 */
        ALOGE("sof0: bits %d is invalid", bits);
        return MPP_NOK;
    }

    height = br->getBits(16);
    width = br->getBits(16);

    ALOGV("sof0: picture: %dx%d", width, height);

    nb_components = br->getBits(8);
    if ((nb_components != 1) && (nb_components != MAX_COMPONENTS)) {
        ALOGE("sof0: components number %d error", nb_components);
        return MPP_NOK;
    }

    if (len != (8 + (3 * nb_components)))
        ALOGE("sof0: error, len(%d) mismatch nb_components(%d)",
              len, nb_components);

    *out_width = width;
    *out_height = height;

    return MPP_OK;
}

MPP_RET jpeg_parser_get_dimens(char *buf, size_t buf_size,
                               uint32_t *out_width, uint32_t *out_height)
{
    const uint8_t *buf_end, *buf_ptr;
    int32_t start_code;
    uint8_t *ubuf = (uint8_t*)buf;

    buf_ptr = ubuf;
    buf_end = ubuf + buf_size;

    if (buf_size < 4 || *buf_ptr != 0xFF || *(buf_ptr + 1) != SOI) {
        // not jpeg
        return MPP_ERR_STREAM;
    }

    while (buf_ptr < buf_end) {
        int section_finish = 1;
        /* find start marker */
        start_code = jpeg_find_marker(&buf_ptr, buf_end);
        if (start_code < 0) {
            ALOGV("start code not found");
        }

        ALOGV("marker = 0x%x, avail_size_in_buf = %d\n",
              start_code, (int)(buf_end - buf_ptr));

        /* setup bit read context */
        BitReader br(buf_ptr, buf_end - buf_ptr);

        switch (start_code) {
        case SOI:
            /* nothing to do on SOI */
            break;
        case SOF0:
            if (MPP_OK == jpeg_decode_sof(&br, out_width, out_height)) {
                return MPP_OK;
            } else {
                return MPP_NOK;
            }
        case DHT:
        case DQT:
        case COM:
        case EOI:
        case SOS:
        case DRI:
            // TODO process
            break;
        case SOF2:
        case SOF3:
        case SOF5:
        case SOF6:
        case SOF7:
        case SOF9:
        case SOF10:
        case SOF11:
        case SOF13:
        case SOF14:
        case SOF15:
        case SOF48:
        case LSE:
        case JPG:
            section_finish = 0;
            ALOGD("jpeg: unsupported coding type (0x%x)", start_code);
            break;
        default:
            section_finish = 0;
            ALOGV("unsupported coding type 0x%x switch.", start_code);
            break;
        }

        if (!section_finish) {
            if (MPP_OK != jpegd_skip_section(&br)) {
                ALOGV("Fail to skip section 0xFF%02x!", start_code);
                return MPP_NOK;
            }
        }

        buf_ptr = br.data();
    }

    return MPP_NOK;
}
