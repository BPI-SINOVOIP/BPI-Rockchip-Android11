/*
 * Copyright 2015 The Android Open Source Project
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
 */

#ifndef GOLDFISH_H264_DEC_H_

#define GOLDFISH_H264_DEC_H_

#include "GoldfishVideoDecoderOMXComponent.h"
#include "MediaH264Decoder.h"
#include <sys/time.h>

#include <vector>
#include <map>

#include <gralloc_cb_bp.h>
#include <utils/RefBase.h>
#include <utils/threads.h>
#include <utils/Vector.h>
#include <utils/List.h>
#include <ui/GraphicBuffer.h>


namespace android {

/** Number of entries in the time-stamp array */
#define MAX_TIME_STAMPS 64

/** Maximum number of cores supported by the codec */
#define CODEC_MAX_NUM_CORES 4

#define CODEC_MAX_WIDTH     1920

#define CODEC_MAX_HEIGHT    1088

/** Input buffer size */
#define INPUT_BUF_SIZE (1024 * 1024)

#define MIN(a, b) ((a) < (b)) ? (a) : (b)

/** Used to remove warnings about unused parameters */
#define UNUSED(x) ((void)(x))

struct GoldfishAVCDec : public GoldfishVideoDecoderOMXComponent {
    GoldfishAVCDec(const char *name, const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData, OMX_COMPONENTTYPE **component, RenderMode renderMode);

protected:
    virtual ~GoldfishAVCDec();

    virtual void onQueueFilled(OMX_U32 portIndex);
    virtual void onPortFlushCompleted(OMX_U32 portIndex);
    virtual void onReset();
    virtual int getColorAspectPreference();

    virtual OMX_ERRORTYPE internalGetParameter(OMX_INDEXTYPE index, OMX_PTR params);

    virtual OMX_ERRORTYPE internalSetParameter(OMX_INDEXTYPE index, const OMX_PTR params);

    virtual OMX_ERRORTYPE getExtensionIndex(const char *name, OMX_INDEXTYPE *index);

private:
    // Number of input and output buffers
    enum {
        kNumBuffers = 8
    };

    RenderMode  mRenderMode = RenderMode::RENDER_BY_GUEST_CPU;
    bool mEnableAndroidNativeBuffers = false;
    std::map<void*, sp<ANativeWindowBuffer>> mNWBuffers;

    int getHostColorBufferId(void* header);

    size_t mNumCores;            // Number of cores to be uesd by the codec

    nsecs_t mTimeStart;   // Time at the start of decode()
    nsecs_t mTimeEnd;     // Time at the end of decode()

#ifdef FILE_DUMP_ENABLE
    char mInFile[200];
#endif /* FILE_DUMP_ENABLE */

    OMX_COLOR_FORMATTYPE mOmxColorFormat;    // OMX Color format

    bool mIsInFlush;        // codec is flush mode
    bool mReceivedEOS;      // EOS is receieved on input port

    // The input stream has changed to a different resolution, which is still supported by the
    // codec. So the codec is switching to decode the new resolution.
    bool mChangingResolution;
    bool mSignalledError;
    size_t mInputOffset;

    status_t initDecoder();
    status_t deInitDecoder();
    status_t setFlushMode();
    status_t setParams(size_t stride);
    void logVersion();
    status_t setNumCores();
    status_t resetDecoder();
    status_t resetPlugin();


    void readAndDiscardAllHostBuffers();

    bool setDecodeArgs(
            OMX_BUFFERHEADERTYPE *inHeader,
            OMX_BUFFERHEADERTYPE *outHeader);

    bool getVUIParams(h264_image_t& img);

    void copyImageData( OMX_BUFFERHEADERTYPE *outHeader, h264_image_t & img);

    std::unique_ptr<MediaH264Decoder> mContext;
    std::vector<uint8_t> mCsd0;
    std::vector<uint8_t> mCsd1;
    uint64_t mConsumedBytes = 0;
    uint8_t* mInPBuffer = nullptr;
    uint8_t* mOutHeaderBuf = nullptr;
    DISALLOW_EVIL_CONSTRUCTORS(GoldfishAVCDec);
};
#ifdef FILE_DUMP_ENABLE

#define INPUT_DUMP_PATH     "/sdcard/media/avcd_input"
#define INPUT_DUMP_EXT      "h264"

#define GENERATE_FILE_NAMES() {                         \
    strcpy(mInFile, "");                                \
    sprintf(mInFile, "%s_%lld.%s", INPUT_DUMP_PATH,     \
            (long long) mTimeStart,                     \
            INPUT_DUMP_EXT);                            \
}

#define CREATE_DUMP_FILE(m_filename) {                  \
    FILE *fp = fopen(m_filename, "wb");                 \
    if (fp != NULL) {                                   \
        fclose(fp);                                     \
    } else {                                            \
        ALOGD("Could not open file %s", m_filename);    \
    }                                                   \
}
#define DUMP_TO_FILE(m_filename, m_buf, m_size, m_offset)\
{                                                       \
    FILE *fp = fopen(m_filename, "ab");                 \
    if (fp != NULL && m_buf != NULL && m_offset == 0) { \
        int i;                                          \
        i = fwrite(m_buf, 1, m_size, fp);               \
        ALOGD("fwrite ret %d to write %d", i, m_size);  \
        if (i != (int) m_size) {                        \
            ALOGD("Error in fwrite, returned %d", i);   \
            perror("Error in write to file");           \
        }                                               \
    } else if (fp == NULL) {                            \
        ALOGD("Could not write to file %s", m_filename);\
    }                                                   \
    if (fp) {                                           \
        fclose(fp);                                     \
    }                                                   \
}
#else /* FILE_DUMP_ENABLE */
#define INPUT_DUMP_PATH
#define INPUT_DUMP_EXT
#define OUTPUT_DUMP_PATH
#define OUTPUT_DUMP_EXT
#define GENERATE_FILE_NAMES()
#define CREATE_DUMP_FILE(m_filename)
#define DUMP_TO_FILE(m_filename, m_buf, m_size, m_offset)
#endif /* FILE_DUMP_ENABLE */

} // namespace android

#endif  // GOLDFISH_H264_DEC_H_
