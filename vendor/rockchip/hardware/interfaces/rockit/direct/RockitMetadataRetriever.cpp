/*
 * Copyright 2020 Rockchip Electronics Co. LTD
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

//#define LOG_NDEBUG 0
#define LOG_TAG "RockitMetadataRetriever"

#include <stdint.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <media/IMediaHTTPService.h>
#include <media/openmax/OMX_IVCommon.h>
#include <media/CharacterEncodingDetector.h>

#include "RockitMetadataRetriever.h"
#include "RTMediaMetaKeys.h"

namespace android {

#if 1
#define Debug ALOGV
#else
#define Debug ALOGD
#endif

enum RockitRetrieverMetaKey {
    RETRIEVER_KEY_METADATA,
    RETRIEVER_KEY_TRACKINFOR,
    RETRIEVER_KEY_DURATION,
    RETRIEVER_KEY_GET_ALBUMART,
};

RockitMetadataRetriever::RockitMetadataRetriever() {
    Debug("%s: this = %p in ", __FUNCTION__, this);
    mParsedMetaData = false;
    mAlbumArt = NULL;
    // property to disable MetadataRetriever
    mEnable = property_get_bool("media.rockit.thumbnail.enale", true);

    mLastImageIndex = -1;

    mCtx = (RockitRetrieverCtx*)malloc(sizeof(RockitRetrieverCtx));
    assert(mCtx != NULL);
    memset(mCtx, 0, sizeof(RockitRetrieverCtx));

    createMetadataRetriever();
    Debug("%s: this = %p out", __FUNCTION__, this);
}

RockitMetadataRetriever::~RockitMetadataRetriever() {
    Debug("%s: this = %p", __FUNCTION__, this);
    clearMetadata();
    destoryMetadataRetriever();
}

bool RockitMetadataRetriever::isEnable() {
    return mEnable;
}

bool RockitMetadataRetriever::check() {
    if ((mCtx == NULL) || (mCtx->mRetriever == NULL) || (mCtx->mRtMetaData == NULL))
        return false;

    return isEnable();
}

bool RockitMetadataRetriever::getDstColorFormat(int colorFormat, int *dstFormat, int *dstBpp) {
    switch (colorFormat) {
        case HAL_PIXEL_FORMAT_RGB_565: {
            *dstFormat = (int)OMX_COLOR_Format16bitRGB565;
            *dstBpp = 2;
            Debug("%s: RGB_565, dstFormat = %d", __FUNCTION__, *dstFormat);
            return true;
        }
        case HAL_PIXEL_FORMAT_RGBA_8888: {
            *dstFormat = (int)OMX_COLOR_Format32BitRGBA8888;
            *dstBpp = 4;
            Debug("%s: RGBA8888 dstFormat = %d", __FUNCTION__, *dstFormat);
            return true;
        }
        case HAL_PIXEL_FORMAT_BGRA_8888: {
            *dstFormat = (int)OMX_COLOR_Format32bitBGRA8888;
            *dstBpp = 4;
            Debug("%s: BGRA8888 dstFormat = %d", __FUNCTION__, *dstFormat);
            return true;
        }
        default: {
            ALOGE("Unsupported color format: %d", colorFormat);
            break;
        }
    }

    return false;
}

void RockitMetadataRetriever::createMetadataRetriever() {
    if (!isEnable())
        return ;

    mCtx->mLibFd = dlopen(ROCKIT_PLAYER_LIB_NAME, RTLD_LAZY);
    if (mCtx->mLibFd == NULL) {
        ALOGE("Cannot load library %s dlerror: %s", ROCKIT_PLAYER_LIB_NAME, dlerror());
        return ;
    }

    mCtx->mCreateRetrieverFunc = (createMetaDataRetrieverFunc *)dlsym(mCtx->mLibFd,
                                                        CREATE_METARETRIEVER_FUNC_NAME);
    if (mCtx->mCreateRetrieverFunc == NULL) {
        ALOGE("dlsym for create meta retriever failed, dlerror: %s", dlerror());
    }

    mCtx->mDestroyRetrieverFunc = (destroyMetaDataRetrieverFunc *)dlsym(mCtx->mLibFd,
                                                        DESTROY_METARETRIEVER_FUNC_NAME);
    if (mCtx->mDestroyRetrieverFunc == NULL) {
        ALOGE("dlsym for destroy meta retriever failed, dlerror: %s", dlerror());
    }

    mCtx->mRetriever = (RTMetadataRetrieverInterface *)mCtx->mCreateRetrieverFunc();
    if (mCtx->mRetriever == NULL) {
        ALOGE("create meta retriever failed, retriever is null");
    }

    mCtx->mCreateMetaDataFunc = (createRockitMetaDataFunc *)dlsym(mCtx->mLibFd,
                                                            CREATE_METADATA_FUNC_NAME);
    if (mCtx->mCreateMetaDataFunc == NULL) {
        ALOGE("dlsym for create meta data failed, dlerror: %s", dlerror());
    }

    mCtx->mDestroyMetaDataFunc = (destroyRockitMetaDataFunc *)dlsym(mCtx->mLibFd,
                                                        DESTROY_METADATA_FUNC_NAME);
    if (mCtx->mDestroyMetaDataFunc == NULL) {
        ALOGE("dlsym for destroy meta data failed, dlerror: %s", dlerror());
    }

    mCtx->mRtMetaData = (RtMetaData*)mCtx->mCreateMetaDataFunc();
    if (mCtx->mRtMetaData == NULL) {
        ALOGE("create meta failed, meta is null");
    }

    mCtx->mCallBack = new RTMetadataRetrieverCallback();

    ALOGV("retriever : %p", mCtx->mRetriever);
}

void RockitMetadataRetriever::destoryMetadataRetriever() {
    if (mCtx != NULL) {
        mCtx->mDestroyMetaDataFunc((void **)&mCtx->mRtMetaData);
        mCtx->mDestroyRetrieverFunc((void **)&mCtx->mRetriever);
        mCtx->mRetriever = NULL;
        mCtx->mRtMetaData = NULL;
        if (mCtx->mLibFd != NULL) {
            dlclose(mCtx->mLibFd);
            mCtx->mLibFd = NULL;
        }

        if (mCtx->mCallBack != NULL) {
            delete mCtx->mCallBack;
            mCtx->mCallBack = NULL;
        }
        free(mCtx);
    }
}

void RockitMetadataRetriever::prepareUrlHeader(
           const KeyedVector<String8, String8> *headers,
           String8 *str) {
    if (headers == NULL) {
        return;
    }

    for (size_t i = 0; i < headers->size(); ++i) {
        String8 line;
        line.append(headers->keyAt(i));
        line.append(": ");
        line.append(headers->valueAt(i));
        line.append("\r\n");

        str->append(line);
    }
}

status_t RockitMetadataRetriever::setDataSource(
            const sp<IMediaHTTPService> &httpService,
            const char *url,
            const KeyedVector<String8, String8> *headers) {
    (void)httpService;
    String8 str;

    Mutex::Autolock autoLock(mLock);
    if (!check())
        return UNKNOWN_ERROR;

    clearMetadata();

    prepareUrlHeader(headers, &str);
    int ret = mCtx->mRetriever->setDataSource(url, str.string());
    return (ret == 0) ? OK : UNKNOWN_ERROR;
}

status_t RockitMetadataRetriever::setDataSource(int fd, int64_t offset, int64_t length) {
    Mutex::Autolock autoLock(mLock);
    if (!check())
        return UNKNOWN_ERROR;

    clearMetadata();
    int ret = mCtx->mRetriever->setDataSource(fd, offset, length);
    return (ret == 0) ? OK : UNKNOWN_ERROR;
}

status_t RockitMetadataRetriever::setDataSource(const sp<DataSource>& source,
            const char *mime) {
    (void)source;
    (void)mime;

    Mutex::Autolock autoLock(mLock);
    if (!check())
        return UNKNOWN_ERROR;

    clearMetadata();
    Debug("%s: this = %p", __FUNCTION__, this);
    return OK;
}

sp<IMemory> RockitMetadataRetriever::getFrameAtTime(
            int64_t timeUs, int option, int colorFormat, bool metaOnly) {
    Mutex::Autolock autoLock(mLock);
    if (!check())
        return NULL;

    Debug("%s: this = %p", __FUNCTION__, this);
    if (timeUs < 0) {
        int64_t duration = 0;
        RtMetaData* meta = mCtx->mRtMetaData;
        meta->findInt64(kRetrieverKeyDuration, &duration);
        timeUs = duration / 3;
    }

    return getFrameInternal(timeUs, option, colorFormat, metaOnly);
}

sp<IMemory> RockitMetadataRetriever::getFrameAtIndex(
            int frameIndex, int colorFormat, bool metaOnly) {
    Mutex::Autolock autoLock(mLock);
    if (!check())
        return NULL;

    return getFrameInternal(
            frameIndex, MediaSource::ReadOptions::SEEK_FRAME_INDEX, colorFormat, metaOnly);
}

sp<IMemory> RockitMetadataRetriever::getFrameInternal(
            int64_t timeUs, int option, int colorFormat, bool metaOnly) {
    int dstFormat, dstBpp;

    Debug("%s: this = %p", __FUNCTION__, this);
    if (!getDstColorFormat(colorFormat, &dstFormat, &dstBpp)) {
        ALOGD("%s: this = %p, colorFormat = %d not support", __FUNCTION__, this, colorFormat);
        return NULL;
    }

    RTMetadataRetrieverCallback* callBackCtx = mCtx->mCallBack;
    if (callBackCtx == NULL) {
        ALOGE("new RockitMetadataRetrieverCallback fail");
        return NULL;
    }

    clearRTMetadata();

    mCtx->mRtMetaData->setInt64(kRetrieverFrameAtTime, timeUs);
    mCtx->mRtMetaData->setInt32(kRetrieverFrameOption, option);
    mCtx->mRtMetaData->setInt32(kRetrieverDstColorFormat, dstFormat);
    mCtx->mRtMetaData->setInt32(kRetrieverFrameMetaOnly, metaOnly);
    mCtx->mRtMetaData->setPointer(kRetrieverCallbackContext, (void*)callBackCtx);
    mCtx->mRtMetaData->setInt64(kRetrieverReadMaxTime, 3000000); // us

    mCtx->mRetriever->getFrameAtTime(mCtx->mRtMetaData);

    sp<IMemory> frame = callBackCtx->extractFrames();

    return frame;
}

sp<IMemory> RockitMetadataRetriever::getImageAtIndex(
            int index, int colorFormat, bool metaOnly, bool thumbnail) {
    (void)index;
    (void)colorFormat;
    (void)metaOnly;
    (void)thumbnail;

    Mutex::Autolock autoLock(mLock);
    if (!check())
        return NULL;

    return NULL;
}

sp<IMemory> RockitMetadataRetriever::getImageRectAtIndex(
            int index, int colorFormat, int left, int top, int right, int bottom) {
    (void)index;
    (void)colorFormat;
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;

    Mutex::Autolock autoLock(mLock);
    if (!check())
        return NULL;

    // FrameRect rect = {left, top, right, bottom};
    return NULL;
}

MediaAlbumArt* RockitMetadataRetriever::extractAlbumArt() {
    Mutex::Autolock autoLock(mLock);
    if (!check())
        return NULL;

    if (!mParsedMetaData) {
        parseMetaData();
        mParsedMetaData = true;
    }

    if (mAlbumArt) {
        return mAlbumArt->clone();
    }

    return NULL;
}

const char* RockitMetadataRetriever::extractMetadata(int keyCode) {
    Mutex::Autolock autoLock(mLock);
    if (!check())
        return NULL;

    if (!mParsedMetaData) {
        parseMetaData();
        mParsedMetaData = true;
    }

    ssize_t index = mMetaData.indexOfKey(keyCode);
    if (index < 0) {
        return NULL;
    }

    return mMetaData.valueAt(index).string();
}

void RockitMetadataRetriever::parseMetaData() {
    if (!check())
        return;

    clearRTMetadata();

    RtMetaData* meta = mCtx->mRtMetaData;
    meta->setInt32(kRetrieverKey, RETRIEVER_KEY_METADATA);
    int ret = mCtx->mRetriever->query(meta);
    if (!ret) {
        struct Map {
            int from;
            int to;
            const char *name;
        };
        static const Map kMap[] = {
            { kRetrieverKeyMIMEType, METADATA_KEY_MIMETYPE, NULL },
            { kRetrieverKeyCDTrackNumber, METADATA_KEY_CD_TRACK_NUMBER, "tracknumber" },
            { kRetrieverKeyDiscNumber, METADATA_KEY_DISC_NUMBER, "discnumber" },
            { kRetrieverKeyAlbum, METADATA_KEY_ALBUM, "album" },
            { kRetrieverKeyArtist, METADATA_KEY_ARTIST, "artist" },
            { kRetrieverKeyAlbumArtist, METADATA_KEY_ALBUMARTIST, "albumartist" },
            { kRetrieverKeyAuthor, METADATA_KEY_AUTHOR, NULL },
            { kRetrieverKeyComposer, METADATA_KEY_COMPOSER, "composer" },
            { kRetrieverKeyDate, METADATA_KEY_DATE, NULL },
            { kRetrieverKeyGenre, METADATA_KEY_GENRE, "genre" },
            { kRetrieverKeyTitle, METADATA_KEY_TITLE, "title" },
            { kRetrieverKeyYear, METADATA_KEY_YEAR, "year" },
            { kRetrieverKeyWriter, METADATA_KEY_WRITER, "writer" },
            { kRetrieverKeyCompilation, METADATA_KEY_COMPILATION, "compilation" },
            { kRetrieverKeyLocation, METADATA_KEY_LOCATION, NULL },
        };
        static const size_t kNumMapEntries = sizeof(kMap) / sizeof(kMap[0]);

        CharacterEncodingDetector *detector = new CharacterEncodingDetector();

        for (size_t i = 0; i < kNumMapEntries; ++i) {
            const char *value;
            if (meta->findCString(kMap[i].from, &value)) {
                if (kMap[i].name) {
                    // add to charset detector
                    detector->addTag(kMap[i].name, value);
                } else {
                    // directly add to output list
                    mMetaData.add(kMap[i].to, String8(value));
                }
            }
        }

        detector->detectAndConvert();
        int size = detector->size();
        if (size) {
            for (int i = 0; i < size; i++) {
                const char *name;
                const char *value;
                detector->getTag(i, &name, &value);
                for (size_t j = 0; j < kNumMapEntries; ++j) {
                    if (kMap[j].name && !strcmp(kMap[j].name, name)) {
                        mMetaData.add(kMap[j].to, String8(value));
                    }
                }
            }
        }
        delete detector;
    }

    meta->clear();
    meta->setInt32(kRetrieverKey, RETRIEVER_KEY_GET_ALBUMART);
    ret = mCtx->mRetriever->query(meta);
    void *data = NULL;
    int dataSize = 0;
    if (ret == 0) {
        meta->findPointer(kRetrieverAlbumArtData, &data);
        meta->findInt32(kRetrieverAlbumArtDataSize, &dataSize);
        if (data != NULL && dataSize > 0) {
            mAlbumArt = MediaAlbumArt::fromData(dataSize, data);
        }
    }

    int numTracks = 0;
    void* tracks = NULL;
    meta->clear();
    meta->setInt32(kRetrieverKey, RETRIEVER_KEY_TRACKINFOR);
    ret = mCtx->mRetriever->query(meta);

    meta->findInt32(kUserInvokeTracksCount, &numTracks);
    Debug("%s:numTracks = %d", __FUNCTION__, numTracks);

    char tmp[32];
    sprintf(tmp, "%d", numTracks);

    mMetaData.add(METADATA_KEY_NUM_TRACKS, String8(tmp));

    float captureFps;
    if (meta->findFloat(kKeyCaptureFramerate, &captureFps)) {
        sprintf(tmp, "%f", captureFps);
        mMetaData.add(METADATA_KEY_CAPTURE_FRAMERATE, String8(tmp));
    }

#if 0
    int64_t exifOffset, exifSize;
    if (meta->findInt64(kKeyExifOffset, &exifOffset)
     && meta->findInt64(kKeyExifSize, &exifSize)) {
        sprintf(tmp, "%lld", (long long)exifOffset);
        mMetaData.add(METADATA_KEY_EXIF_OFFSET, String8(tmp));
        sprintf(tmp, "%lld", (long long)exifSize);
        mMetaData.add(METADATA_KEY_EXIF_LENGTH, String8(tmp));
    }
#endif

    bool hasAudio = false;
    bool hasVideo = false;
    int32_t videoWidth = -1;
    int32_t videoHeight = -1;
    int32_t videoFrameCount = 0;
    int32_t audioBitrate = -1;
    int32_t rotationAngle = -1;
    int32_t imageCount = 0;
    int32_t imagePrimary = 0;
    int32_t imageWidth = -1;
    int32_t imageHeight = -1;
    int32_t imageRotation = -1;

    // The overall duration is the duration of the longest track.
    String8 timedTextLang;

    meta->findPointer(kUserInvokeTracksInfor, &tracks);
    RockitTrackInfor* trackInfor = (RockitTrackInfor*)tracks;

    if (trackInfor != NULL) {
        int32_t* reserved = (int32_t*)trackInfor->mReserved;
        for (size_t i = 0; i < numTracks; ++i) {
            if (!hasAudio && (trackInfor[i].mCodecType == RTTRACK_TYPE_AUDIO)) {
                hasAudio = true;
                audioBitrate = reserved[RES_AUDIO_BITRATE];
                int32_t bitsPerSample = reserved[RES_AUDIO_BIT_PER_SAMPLE];
                int32_t sampleRate = trackInfor[i].mSampleRate;
                if (bitsPerSample >= 0) {
                    sprintf(tmp, "%d", bitsPerSample);
                    mMetaData.add(METADATA_KEY_BITS_PER_SAMPLE, String8(tmp));
                }
                if (sampleRate >= 0) {
                    sprintf(tmp, "%d", sampleRate);
                    mMetaData.add(METADATA_KEY_SAMPLERATE, String8(tmp));
                }
                Debug("%s: samplerate = %d, bitsPerSample = %d",
                      __FUNCTION__, sampleRate, bitsPerSample);
            } else if (!hasVideo && (trackInfor[i].mCodecType == RTTRACK_TYPE_VIDEO)) {
                hasVideo = true;
                videoWidth = trackInfor[i].mWidth;
                videoHeight = trackInfor[i].mHeight;
                rotationAngle = reserved[RES_VIDEO_ROTATION];
                Debug("%s: w(%d) x h(%d), rotationAngle = %d",
                      __FUNCTION__, videoWidth, videoHeight, rotationAngle);
                #if 0
                if (!trackMeta->findInt32(kKeyFrameCount, &videoFrameCount)) {
                    videoFrameCount = 0;
                }

                parseColorAspects(trackMeta);
                #endif
            } /*else if (!strncasecmp("image/", mime, 6)) {
                int32_t isPrimary;
                if (trackMeta->findInt32(
                        kKeyTrackIsDefault, &isPrimary) && isPrimary) {
                    imagePrimary = imageCount;
                    CHECK(trackMeta->findInt32(kKeyWidth, &imageWidth));
                    CHECK(trackMeta->findInt32(kKeyHeight, &imageHeight));
                    if (!trackMeta->findInt32(kKeyRotation, &imageRotation)) {
                        imageRotation = 0;
                    }
                }
                imageCount++;
            } else if (!strcasecmp(mime, MEDIA_MIMETYPE_TEXT_3GPP)) {
                const char *lang;
                if (trackMeta->findCString(kKeyMediaLanguage, &lang)) {
                    timedTextLang.append(String8(lang));
                    timedTextLang.append(String8(":"));
                } else {
                    ALOGE("No language found for timed text");
                }
            }*/
        }
    }

    // To save the language codes for all timed text tracks
    // If multiple text tracks present, the format will look
    // like "eng:chi"
    if (!timedTextLang.isEmpty()) {
        mMetaData.add(METADATA_KEY_TIMED_TEXT_LANGUAGES, timedTextLang);
    }

    meta->clear();
    meta->setInt32(kRetrieverKey, RETRIEVER_KEY_DURATION);
    ret = mCtx->mRetriever->query(meta);

    int64_t duration = 0;
    meta->findInt64(kRetrieverKeyDuration, &duration);
    // The duration value is a string representing the duration in ms.
    sprintf(tmp, "%" PRId64, (duration + 500) / 1000);
    mMetaData.add(METADATA_KEY_DURATION, String8(tmp));

    if (hasAudio) {
        mMetaData.add(METADATA_KEY_HAS_AUDIO, String8("yes"));
    }

    if (hasVideo) {
        mMetaData.add(METADATA_KEY_HAS_VIDEO, String8("yes"));

        sprintf(tmp, "%d", videoWidth);
        mMetaData.add(METADATA_KEY_VIDEO_WIDTH, String8(tmp));

        sprintf(tmp, "%d", videoHeight);
        mMetaData.add(METADATA_KEY_VIDEO_HEIGHT, String8(tmp));

        sprintf(tmp, "%d", rotationAngle);
        mMetaData.add(METADATA_KEY_VIDEO_ROTATION, String8(tmp));

        if (videoFrameCount > 0) {
            sprintf(tmp, "%d", videoFrameCount);
            mMetaData.add(METADATA_KEY_VIDEO_FRAME_COUNT, String8(tmp));
        }
    }

    if (imageCount > 0) {
        mMetaData.add(METADATA_KEY_HAS_IMAGE, String8("yes"));

        sprintf(tmp, "%d", imageCount);
        mMetaData.add(METADATA_KEY_IMAGE_COUNT, String8(tmp));

        sprintf(tmp, "%d", imagePrimary);
        mMetaData.add(METADATA_KEY_IMAGE_PRIMARY, String8(tmp));

        sprintf(tmp, "%d", imageWidth);
        mMetaData.add(METADATA_KEY_IMAGE_WIDTH, String8(tmp));

        sprintf(tmp, "%d", imageHeight);
        mMetaData.add(METADATA_KEY_IMAGE_HEIGHT, String8(tmp));

        sprintf(tmp, "%d", imageRotation);
        mMetaData.add(METADATA_KEY_IMAGE_ROTATION, String8(tmp));
    }

    if (numTracks == 1 && hasAudio && audioBitrate >= 0) {
        sprintf(tmp, "%d", audioBitrate);
        mMetaData.add(METADATA_KEY_BITRATE, String8(tmp));
    } else {
        #if 0
        off64_t sourceSize;
        if (mSource != NULL && mSource->getSize(&sourceSize) == OK) {
            int64_t avgBitRate = (int64_t)(sourceSize * 8E6 / duration);

            sprintf(tmp, "%" PRId64, avgBitRate);
            mMetaData.add(METADATA_KEY_BITRATE, String8(tmp));
        }
        #endif
    }
#if 0
    if (numTracks == 1) {
        const char *fileMIME;

        if (meta->findCString(kKeyMIMEType, &fileMIME) &&
                !strcasecmp(fileMIME, "video/x-matroska")) {
            sp<MetaData> trackMeta = mExtractor->getTrackMetaData(0);
            const char *trackMIME;
            if (trackMeta != nullptr) {
                CHECK(trackMeta->findCString(kKeyMIMEType, &trackMIME));
            }
            if (!strncasecmp("audio/", trackMIME, 6)) {
                // The matroska file only contains a single audio track,
                // rewrite its mime type.
                mMetaData.add(
                        METADATA_KEY_MIMETYPE, String8("audio/x-matroska"));
            }
        }
    }
#endif
}

void RockitMetadataRetriever::clearMetadata() {
    mParsedMetaData = false;
    mMetaData.clear();
    delete mAlbumArt;
    mAlbumArt = NULL;

    clearRTMetadata();
}

void RockitMetadataRetriever::clearRTMetadata() {
    if (mCtx != NULL && mCtx->mRtMetaData != NULL) {
        mCtx->mRtMetaData->clear();
    }
}

}
