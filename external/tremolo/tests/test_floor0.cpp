/*
 * test case not really written for gtest, but wrapped so it'll cooperate.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "Utils_test"

#include <gtest/gtest.h>

#include <string.h>
#define REF_COUNT     1
#define DECODE_PACKET 1

extern "C" {
#include <Tremolo/codec_internal.h>

int _vorbis_unpack_books(vorbis_info *vi, oggpack_buffer *opb);
int _vorbis_unpack_info(vorbis_info *vi, oggpack_buffer *opb);
int _vorbis_unpack_comment(vorbis_comment *vc, oggpack_buffer *opb);
}

const uint8_t packInfoData[] = { 0x00, 0x00, 0x00, 0x00, 0x02, 0x80, 0xBB, 0x00,
        0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xBB, 0x01, 0xFF, 0xFF, 0xFF, 0xFF };

unsigned char unpackBookData[] = { 0x00, 0x42, 0x43, 0x56, 0x1E, 0x00, 0x10,
        0x00, 0x00, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x10, 0x0A, 0xFF, 0x00, 0x00,
        0x00, 0x06, 0xD0, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x1D, 0x00, 0x00, 0x00,
        0x2C, 0x00, 0x03, 0x3C, 0x51, 0x04, 0x34, 0x4F, 0x04, 0x00, 0x40, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xCB, 0x00, 0x40, 0x00, 0x00, 0x01, 0x4F, 0xF4,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0xFF, 0xFF, 0xFF, 0xFF };

unsigned char bufData[] = { 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x00, 0xE7,
        0x00, 0x00, 0xE9, 0x00 };

static void makeBitReader(const void *data, size_t size, ogg_buffer *buf,
                          ogg_reference *ref, oggpack_buffer *bits) {
    buf->data = (uint8_t *) data;
    buf->size = size;
    buf->refcount = REF_COUNT;

    ref->buffer = buf;
    ref->length = size;
    oggpack_readinit(bits, ref);
}

// below here is where we're trying to bridge the gtest and non-gtest world

namespace android {

class Floor0Test : public ::testing::Test {
};

//
// the test, since it compiles with ubsan, should fault and die if the patch
// is not in place.  at least that's the idea.
TEST_F(Floor0Test, Test1) {

    ogg_buffer buf;
    ogg_reference ref;
    oggpack_buffer bits;

    memset(&buf, 0, sizeof(ogg_buffer));
    memset(&ref, 0, sizeof(ogg_reference));
    memset(&bits, 0, sizeof(oggpack_buffer));

    makeBitReader(packInfoData, sizeof(packInfoData), &buf, &ref, &bits);

    vorbis_info *mVi = new vorbis_info;
    vorbis_info_init(mVi);

    int ret = _vorbis_unpack_info(mVi, &bits);
    if (!ret) {
        memset(&buf, 0, sizeof(ogg_buffer));
        memset(&ref, 0, sizeof(ogg_reference));
        memset(&bits, 0, sizeof(oggpack_buffer));

        makeBitReader(unpackBookData, sizeof(unpackBookData), &buf, &ref,
                      &bits);

        ret = _vorbis_unpack_books(mVi, &bits);
        if (!ret) {
            ogg_packet pack;
            memset(&pack, 0, sizeof(ogg_packet));
            memset(&buf, 0, sizeof(ogg_buffer));
            memset(&ref, 0, sizeof(ogg_reference));

            vorbis_dsp_state *mState = new vorbis_dsp_state;
            vorbis_dsp_init(mState, mVi);

            buf.data = bufData;
            buf.size = sizeof(bufData);
            buf.refcount = REF_COUNT;

            ref.buffer = &buf;
            ref.length = buf.size;

            pack.packet = &ref;
            pack.bytes = ref.length;

            vorbis_dsp_synthesis(mState, &pack, DECODE_PACKET);
        }
    }
}

}  // namespace android
