/*
 * Copyright (C) 2019 The Android Open Source Project
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
#define LOG_TAG "NativeMuxerTest"
#include <log/log.h>

#include <NdkMediaExtractor.h>
#include <NdkMediaFormat.h>
#include <NdkMediaMuxer.h>
#include <fcntl.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <vector>

#include "NativeMediaCommon.h"

/**
 * MuxerNativeTestHelper breaks a media file to elements that a muxer can use to rebuild its clone.
 * While testing muxer, if the test doesn't use MediaCodecs class to generate elementary stream,
 * but uses MediaExtractor, this class will be handy
 */
class MuxerNativeTestHelper {
  public:
    explicit MuxerNativeTestHelper(const char* srcPath, const char* mime = nullptr,
                                   int frameLimit = -1)
        : mSrcPath(srcPath), mMime(mime), mTrackCount(0), mBuffer(nullptr) {
        mFrameLimit = frameLimit < 0 ? INT_MAX : frameLimit;
        splitMediaToMuxerParameters();
    }

    ~MuxerNativeTestHelper() {
        for (auto format : mFormat) AMediaFormat_delete(format);
        delete[] mBuffer;
        for (const auto& buffInfoTrack : mBufferInfo) {
            for (auto info : buffInfoTrack) delete info;
        }
    }

    int getTrackCount() { return mTrackCount; }

    bool registerTrack(AMediaMuxer* muxer);

    bool insertSampleData(AMediaMuxer* muxer);

    bool muxMedia(AMediaMuxer* muxer);

    bool combineMedias(AMediaMuxer* muxer, MuxerNativeTestHelper* that, const int* repeater);

    bool isSubsetOf(MuxerNativeTestHelper* that);

    void offsetTimeStamp(int trackID, long tsOffset, int sampleOffset);

  private:
    void splitMediaToMuxerParameters();

    static const int STTS_TOLERANCE_US = 100;
    const char* mSrcPath;
    const char* mMime;
    int mTrackCount;
    std::vector<AMediaFormat*> mFormat;
    uint8_t* mBuffer;
    std::vector<std::vector<AMediaCodecBufferInfo*>> mBufferInfo;
    std::map<int, int> mInpIndexMap;
    std::vector<int> mTrackIdxOrder;
    int mFrameLimit;
    // combineMedias() uses local version of this variable
    std::map<int, int> mOutIndexMap;
};

void MuxerNativeTestHelper::splitMediaToMuxerParameters() {
    FILE* ifp = fopen(mSrcPath, "rbe");
    int fd;
    int fileSize;
    if (ifp) {
        struct stat buf {};
        stat(mSrcPath, &buf);
        fileSize = buf.st_size;
        fd = fileno(ifp);
    } else {
        return;
    }
    AMediaExtractor* extractor = AMediaExtractor_new();
    if (extractor == nullptr) {
        fclose(ifp);
        return;
    }
    // Set up MediaExtractor to read from the source.
    media_status_t status = AMediaExtractor_setDataSourceFd(extractor, fd, 0, fileSize);
    if (AMEDIA_OK != status) {
        AMediaExtractor_delete(extractor);
        fclose(ifp);
        return;
    }

    // Set up MediaFormat
    int index = 0;
    for (size_t trackID = 0; trackID < AMediaExtractor_getTrackCount(extractor); trackID++) {
        AMediaExtractor_selectTrack(extractor, trackID);
        AMediaFormat* format = AMediaExtractor_getTrackFormat(extractor, trackID);
        if (mMime == nullptr) {
            mTrackCount++;
            mFormat.push_back(format);
            mInpIndexMap[trackID] = index++;
        } else {
            const char* mime;
            bool hasKey = AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
            if (hasKey && !strcmp(mime, mMime)) {
                mTrackCount++;
                mFormat.push_back(format);
                mInpIndexMap[trackID] = index;
                break;
            } else {
                AMediaFormat_delete(format);
                AMediaExtractor_unselectTrack(extractor, trackID);
            }
        }
    }

    if (mTrackCount <= 0) {
        AMediaExtractor_delete(extractor);
        fclose(ifp);
        return;
    }

    // Set up location for elementary stream
    int bufferSize = ((fileSize + 127) >> 7) << 7;
    // Ideally, Sum of return values of extractor.readSampleData(...) should not exceed
    // source file size. But in case of Vorbis, aosp extractor appends an additional 4 bytes to
    // the data at every readSampleData() call. bufferSize <<= 1 empirically large enough to
    // hold the excess 4 bytes per read call
    bufferSize <<= 1;
    mBuffer = new uint8_t[bufferSize];
    if (mBuffer == nullptr) {
        mTrackCount = 0;
        AMediaExtractor_delete(extractor);
        fclose(ifp);
        return;
    }

    // Let MediaExtractor do its thing
    bool sawEOS = false;
    int frameCount = 0;
    int offset = 0;
    mBufferInfo.resize(mTrackCount);
    while (!sawEOS && frameCount < mFrameLimit) {
        auto* bufferInfo = new AMediaCodecBufferInfo();
        bufferInfo->offset = offset;
        bufferInfo->size =
                AMediaExtractor_readSampleData(extractor, mBuffer + offset, (bufferSize - offset));
        if (bufferInfo->size < 0) {
            sawEOS = true;
        } else {
            bufferInfo->presentationTimeUs = AMediaExtractor_getSampleTime(extractor);
            bufferInfo->flags = AMediaExtractor_getSampleFlags(extractor);
            int trackID = AMediaExtractor_getSampleTrackIndex(extractor);
            mTrackIdxOrder.push_back(trackID);
            mBufferInfo[(mInpIndexMap.at(trackID))].push_back(bufferInfo);
            AMediaExtractor_advance(extractor);
            frameCount++;
        }
        offset += bufferInfo->size;
    }

    AMediaExtractor_delete(extractor);
    fclose(ifp);
}

bool MuxerNativeTestHelper::registerTrack(AMediaMuxer* muxer) {
    for (int trackID = 0; trackID < mTrackCount; trackID++) {
        int dstIndex = AMediaMuxer_addTrack(muxer, mFormat[trackID]);
        if (dstIndex < 0) return false;
        mOutIndexMap[trackID] = dstIndex;
    }
    return true;
}

bool MuxerNativeTestHelper::insertSampleData(AMediaMuxer* muxer) {
    // write all registered tracks in interleaved order
    int* frameCount = new int[mTrackCount]{0};
    for (int trackID : mTrackIdxOrder) {
        int index = mInpIndexMap.at(trackID);
        AMediaCodecBufferInfo* info = mBufferInfo[index][frameCount[index]];
        if (AMediaMuxer_writeSampleData(muxer, mOutIndexMap.at(index), mBuffer, info) !=
            AMEDIA_OK) {
            delete[] frameCount;
            return false;
        }
        ALOGV("Track: %d Timestamp: %d", trackID, (int)info->presentationTimeUs);
        frameCount[index]++;
    }
    delete[] frameCount;
    ALOGV("Total track samples %d", (int)mTrackIdxOrder.size());
    return true;
}

bool MuxerNativeTestHelper::muxMedia(AMediaMuxer* muxer) {
    return (registerTrack(muxer) && (AMediaMuxer_start(muxer) == AMEDIA_OK) &&
            insertSampleData(muxer) && (AMediaMuxer_stop(muxer) == AMEDIA_OK));
}

bool MuxerNativeTestHelper::combineMedias(AMediaMuxer* muxer, MuxerNativeTestHelper* that,
                                          const int* repeater) {
    if (that == nullptr) return false;
    if (repeater == nullptr) return false;

    // register tracks
    int totalTracksToAdd = repeater[0] * this->mTrackCount + repeater[1] * that->mTrackCount;
    int outIndexMap[totalTracksToAdd];
    MuxerNativeTestHelper* group[2]{this, that};
    for (int k = 0, idx = 0; k < 2; k++) {
        for (int j = 0; j < repeater[k]; j++) {
            for (AMediaFormat* format : group[k]->mFormat) {
                int dstIndex = AMediaMuxer_addTrack(muxer, format);
                if (dstIndex < 0) return false;
                outIndexMap[idx++] = dstIndex;
            }
        }
    }
    // start
    if (AMediaMuxer_start(muxer) != AMEDIA_OK) return false;
    // write sample data
    // write all registered tracks in planar order viz all samples of a track A then all
    // samples of track B, ...
    for (int k = 0, idx = 0; k < 2; k++) {
        for (int j = 0; j < repeater[k]; j++) {
            for (int i = 0; i < group[k]->mTrackCount; i++) {
                for (int p = 0; p < group[k]->mBufferInfo[i].size(); p++) {
                    AMediaCodecBufferInfo* info = group[k]->mBufferInfo[i][p];
                    if (AMediaMuxer_writeSampleData(muxer, outIndexMap[idx], group[k]->mBuffer,
                                                    info) != AMEDIA_OK) {
                        return false;
                    }
                    ALOGV("Track: %d Timestamp: %d", outIndexMap[idx],
                          (int)info->presentationTimeUs);
                }
                idx++;
            }
        }
    }
    // stop
    return (AMediaMuxer_stop(muxer) == AMEDIA_OK);
}

// returns true if 'this' stream is a subset of 'o'. That is all tracks in current media
// stream are present in ref media stream
bool MuxerNativeTestHelper::isSubsetOf(MuxerNativeTestHelper* that) {
    if (this == that) return true;
    if (that == nullptr) return false;

    for (int i = 0; i < mTrackCount; i++) {
        AMediaFormat* thisFormat = mFormat[i];
        const char* thisMime = nullptr;
        AMediaFormat_getString(thisFormat, AMEDIAFORMAT_KEY_MIME, &thisMime);
        int j = 0;
        for (; j < that->mTrackCount; j++) {
            AMediaFormat* thatFormat = that->mFormat[j];
            const char* thatMime = nullptr;
            AMediaFormat_getString(thatFormat, AMEDIAFORMAT_KEY_MIME, &thatMime);
            if (thisMime != nullptr && thatMime != nullptr && !strcmp(thisMime, thatMime)) {
                if (!isCSDIdentical(thisFormat, thatFormat)) continue;
                if (mBufferInfo[i].size() == that->mBufferInfo[j].size()) {
                    int tolerance =
                            !strncmp(thisMime, "video/", strlen("video/")) ? STTS_TOLERANCE_US : 0;
                    tolerance += 1; // rounding error
                    int k = 0;
                    for (; k < mBufferInfo[i].size(); k++) {
                        AMediaCodecBufferInfo* thisInfo = mBufferInfo[i][k];
                        AMediaCodecBufferInfo* thatInfo = that->mBufferInfo[j][k];
                        if (thisInfo->flags != thatInfo->flags) {
                            break;
                        }
                        if (thisInfo->size != thatInfo->size) {
                            break;
                        } else if (memcmp(mBuffer + thisInfo->offset,
                                          that->mBuffer + thatInfo->offset, thisInfo->size)) {
                            break;
                        }
                        if (abs(thisInfo->presentationTimeUs - thatInfo->presentationTimeUs) >
                            tolerance) {
                            break;
                        }
                    }
                    if (k == mBufferInfo[i].size()) break;
                }
            }
        }
        if (j == that->mTrackCount) {
            ALOGV("For mime %s, Couldn't find a match", thisMime);
            return false;
        }
    }
    return true;
}

void MuxerNativeTestHelper::offsetTimeStamp(int trackID, long tsOffset, int sampleOffset) {
    // offset pts of samples from index sampleOffset till the end by tsOffset
    if (trackID < mTrackCount) {
        for (int i = sampleOffset; i < mBufferInfo[trackID].size(); i++) {
            AMediaCodecBufferInfo* info = mBufferInfo[trackID][i];
            info->presentationTimeUs += tsOffset;
        }
    }
}

static bool isCodecContainerPairValid(MuxerFormat format, const char* mime) {
    static const std::map<MuxerFormat, std::vector<const char*>> codecListforType = {
            {OUTPUT_FORMAT_MPEG_4,
             {AMEDIA_MIMETYPE_VIDEO_MPEG4, AMEDIA_MIMETYPE_VIDEO_H263, AMEDIA_MIMETYPE_VIDEO_AVC,
              AMEDIA_MIMETYPE_VIDEO_HEVC, AMEDIA_MIMETYPE_AUDIO_AAC}},
            {OUTPUT_FORMAT_WEBM,
             {AMEDIA_MIMETYPE_VIDEO_VP8, AMEDIA_MIMETYPE_VIDEO_VP9, AMEDIA_MIMETYPE_AUDIO_VORBIS,
              AMEDIA_MIMETYPE_AUDIO_OPUS}},
            {OUTPUT_FORMAT_THREE_GPP,
             {AMEDIA_MIMETYPE_VIDEO_MPEG4, AMEDIA_MIMETYPE_VIDEO_H263, AMEDIA_MIMETYPE_VIDEO_AVC,
              AMEDIA_MIMETYPE_AUDIO_AAC, AMEDIA_MIMETYPE_AUDIO_AMR_NB,
              AMEDIA_MIMETYPE_AUDIO_AMR_WB}},
            {OUTPUT_FORMAT_OGG, {AMEDIA_MIMETYPE_AUDIO_OPUS}},
    };

    if (format == OUTPUT_FORMAT_MPEG_4 &&
        strncmp(mime, "application/", strlen("application/")) == 0)
        return true;

    auto it = codecListforType.find(format);
    if (it != codecListforType.end())
        for (auto it2 : it->second)
            if (strcmp(it2, mime) == 0) return true;

    return false;
}

static jboolean nativeTestSetLocation(JNIEnv* env, jobject, jint jformat, jstring jsrcPath,
                                      jstring jdstPath) {
    bool isPass = true;
    bool isGeoDataSupported;
    const float atlanticLat = 14.59f;
    const float atlanticLong = 28.67f;
    const float tooFarNorth = 90.5f;
    const float tooFarWest = -180.5f;
    const float tooFarSouth = -90.5f;
    const float tooFarEast = 180.5f;
    const float annapurnaLat = 28.59f;
    const float annapurnaLong = 83.82f;
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    if (ofp) {
        AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)jformat);
        media_status_t status = AMediaMuxer_setLocation(muxer, tooFarNorth, atlanticLong);
        if (status == AMEDIA_OK) {
            isPass = false;
            ALOGE("setLocation succeeds on bad args: (%f, %f)", tooFarNorth, atlanticLong);
        }
        status = AMediaMuxer_setLocation(muxer, tooFarSouth, atlanticLong);
        if (status == AMEDIA_OK) {
            isPass = false;
            ALOGE("setLocation succeeds on bad args: (%f, %f)", tooFarSouth, atlanticLong);
        }
        status = AMediaMuxer_setLocation(muxer, atlanticLat, tooFarWest);
        if (status == AMEDIA_OK) {
            isPass = false;
            ALOGE("setLocation succeeds on bad args: (%f, %f)", atlanticLat, tooFarWest);
        }
        status = AMediaMuxer_setLocation(muxer, atlanticLat, tooFarEast);
        if (status == AMEDIA_OK) {
            isPass = false;
            ALOGE("setLocation succeeds on bad args: (%f, %f)", atlanticLat, tooFarEast);
        }
        status = AMediaMuxer_setLocation(muxer, tooFarNorth, tooFarWest);
        if (status == AMEDIA_OK) {
            isPass = false;
            ALOGE("setLocation succeeds on bad args: (%f, %f)", tooFarNorth, tooFarWest);
        }
        status = AMediaMuxer_setLocation(muxer, atlanticLat, atlanticLong);
        isGeoDataSupported = (status == AMEDIA_OK);
        if (isGeoDataSupported) {
            status = AMediaMuxer_setLocation(muxer, annapurnaLat, annapurnaLong);
            if (status != AMEDIA_OK) {
                isPass = false;
                ALOGE("setLocation fails on args: (%f, %f)", annapurnaLat, annapurnaLong);
            }
        } else {
            isPass &= ((MuxerFormat)jformat != OUTPUT_FORMAT_MPEG_4 &&
                       (MuxerFormat)jformat != OUTPUT_FORMAT_THREE_GPP);
        }
        const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
        auto* mediaInfo = new MuxerNativeTestHelper(csrcPath);
        if (mediaInfo->registerTrack(muxer) && AMediaMuxer_start(muxer) == AMEDIA_OK) {
            status = AMediaMuxer_setLocation(muxer, atlanticLat, atlanticLong);
            if (status == AMEDIA_OK) {
                isPass = false;
                ALOGE("setLocation succeeds after starting the muxer");
            }
            if (mediaInfo->insertSampleData(muxer) && AMediaMuxer_stop(muxer) == AMEDIA_OK) {
                status = AMediaMuxer_setLocation(muxer, atlanticLat, atlanticLong);
                if (status == AMEDIA_OK) {
                    isPass = false;
                    ALOGE("setLocation succeeds after stopping the muxer");
                }
            } else {
                isPass = false;
                ALOGE("failed to writeSampleData or stop muxer");
            }
        } else {
            isPass = false;
            ALOGE("failed to addTrack or start muxer");
        }
        delete mediaInfo;
        env->ReleaseStringUTFChars(jsrcPath, csrcPath);
        AMediaMuxer_delete(muxer);
        fclose(ofp);
    } else {
        isPass = false;
        ALOGE("failed to open output file %s", cdstPath);
    }
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSetOrientationHint(JNIEnv* env, jobject, jint jformat, jstring jsrcPath,
                                             jstring jdstPath) {
    bool isPass = true;
    bool isOrientationSupported;
    const int badRotation[] = {360, 45, -90};
    const int oldRotation = 90;
    const int currRotation = 180;
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    if (ofp) {
        AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)jformat);
        media_status_t status;
        for (int degrees : badRotation) {
            status = AMediaMuxer_setOrientationHint(muxer, degrees);
            if (status == AMEDIA_OK) {
                isPass = false;
                ALOGE("setOrientationHint succeeds on bad args: %d", degrees);
            }
        }
        status = AMediaMuxer_setOrientationHint(muxer, oldRotation);
        isOrientationSupported = (status == AMEDIA_OK);
        if (isOrientationSupported) {
            status = AMediaMuxer_setOrientationHint(muxer, currRotation);
            if (status != AMEDIA_OK) {
                isPass = false;
                ALOGE("setOrientationHint fails on args: %d", currRotation);
            }
        } else {
            isPass &= ((MuxerFormat)jformat != OUTPUT_FORMAT_MPEG_4 &&
                       (MuxerFormat)jformat != OUTPUT_FORMAT_THREE_GPP);
        }
        const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
        auto* mediaInfo = new MuxerNativeTestHelper(csrcPath);
        if (mediaInfo->registerTrack(muxer) && AMediaMuxer_start(muxer) == AMEDIA_OK) {
            status = AMediaMuxer_setOrientationHint(muxer, currRotation);
            if (status == AMEDIA_OK) {
                isPass = false;
                ALOGE("setOrientationHint succeeds after starting the muxer");
            }
            if (mediaInfo->insertSampleData(muxer) && AMediaMuxer_stop(muxer) == AMEDIA_OK) {
                status = AMediaMuxer_setOrientationHint(muxer, currRotation);
                if (status == AMEDIA_OK) {
                    isPass = false;
                    ALOGE("setOrientationHint succeeds after stopping the muxer");
                }
            } else {
                isPass = false;
                ALOGE("failed to writeSampleData or stop muxer");
            }
        } else {
            isPass = false;
            ALOGE("failed to addTrack or start muxer");
        }
        delete mediaInfo;
        env->ReleaseStringUTFChars(jsrcPath, csrcPath);
        AMediaMuxer_delete(muxer);
        fclose(ofp);
    } else {
        isPass = false;
        ALOGE("failed to open output file %s", cdstPath);
    }
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestMultiTrack(JNIEnv* env, jobject, jint jformat, jstring jsrcPathA,
                                     jstring jsrcPathB, jstring jrefPath, jstring jdstPath) {
    bool isPass = true;
    const char* csrcPathA = env->GetStringUTFChars(jsrcPathA, nullptr);
    const char* csrcPathB = env->GetStringUTFChars(jsrcPathB, nullptr);
    auto* mediaInfoA = new MuxerNativeTestHelper(csrcPathA);
    auto* mediaInfoB = new MuxerNativeTestHelper(csrcPathB);
    if (mediaInfoA->getTrackCount() == 1 && mediaInfoB->getTrackCount() == 1) {
        const char* crefPath = env->GetStringUTFChars(jrefPath, nullptr);
        // number of times to repeat {mSrcFileA, mSrcFileB} in Output
        int numTracks[][2]{{1, 1}, {2, 0}, {0, 2}, {1, 2}, {2, 1}};
        // prepare reference
        FILE* rfp = fopen(crefPath, "wbe+");
        if (rfp) {
            AMediaMuxer* muxer = AMediaMuxer_new(fileno(rfp), (OutputFormat)jformat);
            bool muxStatus = mediaInfoA->combineMedias(muxer, mediaInfoB, numTracks[0]);
            AMediaMuxer_delete(muxer);
            fclose(rfp);
            if (muxStatus) {
                auto* refInfo = new MuxerNativeTestHelper(crefPath);
                if (!mediaInfoA->isSubsetOf(refInfo) || !mediaInfoB->isSubsetOf(refInfo)) {
                    isPass = false;
                    ALOGE("testMultiTrack: inputs: %s %s, fmt: %d, error ! muxing src A and src B "
                          "failed", csrcPathA, csrcPathB, jformat);
                } else {
                    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
                    for (int i = 1; i < sizeof(numTracks) / sizeof(numTracks[0]) && isPass; i++) {
                        FILE* ofp = fopen(cdstPath, "wbe+");
                        if (ofp) {
                            muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)jformat);
                            bool status =
                                    mediaInfoA->combineMedias(muxer, mediaInfoB, numTracks[i]);
                            AMediaMuxer_delete(muxer);
                            fclose(ofp);
                            if (status) {
                                auto* dstInfo = new MuxerNativeTestHelper(cdstPath);
                                if (!dstInfo->isSubsetOf(refInfo)) {
                                    isPass = false;
                                    ALOGE("testMultiTrack: inputs: %s %s, fmt: %d, error ! muxing "
                                          "src A: %d, src B: %d failed", csrcPathA, csrcPathB,
                                          jformat, numTracks[i][0], numTracks[i][1]);
                                }
                                delete dstInfo;
                            } else {
                                if ((MuxerFormat)jformat != OUTPUT_FORMAT_MPEG_4) {
                                    isPass = false;
                                    ALOGE("testMultiTrack: inputs: %s %s, fmt: %d, error ! muxing "
                                          "src A: %d, src B: %d failed", csrcPathA, csrcPathB,
                                          jformat, numTracks[i][0], numTracks[i][1]);
                                }
                            }
                        } else {
                            isPass = false;
                            ALOGE("failed to open output file %s", cdstPath);
                        }
                    }
                    env->ReleaseStringUTFChars(jdstPath, cdstPath);
                }
                delete refInfo;
            } else {
                if ((MuxerFormat)jformat != OUTPUT_FORMAT_OGG) {
                    isPass = false;
                    ALOGE("testMultiTrack: inputs: %s %s, fmt: %d, error ! muxing src A and src B "
                          "failed", csrcPathA, csrcPathB, jformat);
                }
            }
        } else {
            isPass = false;
            ALOGE("failed to open reference output file %s", crefPath);
        }
        env->ReleaseStringUTFChars(jrefPath, crefPath);
    } else {
        isPass = false;
        if (mediaInfoA->getTrackCount() != 1) {
            ALOGE("error: file %s, track count exp/rec - %d/%d", csrcPathA, 1,
                  mediaInfoA->getTrackCount());
        }
        if (mediaInfoB->getTrackCount() != 1) {
            ALOGE("error: file %s, track count exp/rec - %d/%d", csrcPathB, 1,
                  mediaInfoB->getTrackCount());
        }
    }
    env->ReleaseStringUTFChars(jsrcPathA, csrcPathA);
    env->ReleaseStringUTFChars(jsrcPathB, csrcPathB);
    delete mediaInfoA;
    delete mediaInfoB;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestOffsetPts(JNIEnv* env, jobject, jint format, jstring jsrcPath,
                                    jstring jdstPath, jintArray joffsetIndices) {
    bool isPass = true;
    const int OFFSET_TS = 111000;
    jsize len = env->GetArrayLength(joffsetIndices);
    jint* coffsetIndices = env->GetIntArrayElements(joffsetIndices, nullptr);
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    auto* mediaInfo = new MuxerNativeTestHelper(csrcPath);
    if (mediaInfo->getTrackCount() != 0) {
        for (int trackID = 0; trackID < mediaInfo->getTrackCount() && isPass; trackID++) {
            for (int i = 0; i < len; i++) {
                mediaInfo->offsetTimeStamp(trackID, OFFSET_TS, coffsetIndices[i]);
            }
            FILE* ofp = fopen(cdstPath, "wbe+");
            if (ofp) {
                AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)format);
                mediaInfo->muxMedia(muxer);
                AMediaMuxer_delete(muxer);
                fclose(ofp);
                auto* outInfo = new MuxerNativeTestHelper(cdstPath);
                isPass = mediaInfo->isSubsetOf(outInfo);
                if (!isPass) {
                    ALOGE("Validation failed after adding timestamp offset to track %d", trackID);
                }
                delete outInfo;
            } else {
                isPass = false;
                ALOGE("failed to open output file %s", cdstPath);
            }
            for (int i = len - 1; i >= 0; i--) {
                mediaInfo->offsetTimeStamp(trackID, -OFFSET_TS, coffsetIndices[i]);
            }
        }
    } else {
        isPass = false;
        ALOGE("no valid track found in input file %s", csrcPath);
    }
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    env->ReleaseIntArrayElements(joffsetIndices, coffsetIndices, 0);
    delete mediaInfo;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSimpleMux(JNIEnv* env, jobject, jstring jsrcPath, jstring jdstPath,
                                    jstring jmime, jstring jselector) {
    bool isPass = true;
    const char* cmime = env->GetStringUTFChars(jmime, nullptr);
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cselector = env->GetStringUTFChars(jselector, nullptr);
    auto* mediaInfo = new MuxerNativeTestHelper(csrcPath, cmime);
    static const std::map<MuxerFormat, const char*> formatStringPair = {
            {OUTPUT_FORMAT_MPEG_4, "mp4"},
            {OUTPUT_FORMAT_WEBM, "webm"},
            {OUTPUT_FORMAT_THREE_GPP, "3gp"},
            {OUTPUT_FORMAT_HEIF, "heif"},
            {OUTPUT_FORMAT_OGG, "ogg"}};
    if (mediaInfo->getTrackCount() == 1) {
        const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
        for (int fmt = OUTPUT_FORMAT_START; fmt <= OUTPUT_FORMAT_LIST_END && isPass; fmt++) {
            auto it = formatStringPair.find((MuxerFormat)fmt);
            if (it == formatStringPair.end() || strstr(cselector, it->second) == nullptr) {
                continue;
            }
            if (fmt == OUTPUT_FORMAT_WEBM) continue;  // TODO(b/146923551)
            FILE* ofp = fopen(cdstPath, "wbe+");
            if (ofp) {
                AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)fmt);
                bool muxStatus = mediaInfo->muxMedia(muxer);
                bool result = true;
                AMediaMuxer_delete(muxer);
                fclose(ofp);
                if (muxStatus) {
                    auto* outInfo = new MuxerNativeTestHelper(cdstPath, cmime);
                    result = mediaInfo->isSubsetOf(outInfo);
                    delete outInfo;
                }
                if ((muxStatus && !result) ||
                    (!muxStatus && isCodecContainerPairValid((MuxerFormat)fmt, cmime))) {
                    isPass = false;
                    ALOGE("error: file %s, mime %s, output != clone(input) for format %d", csrcPath,
                          cmime, fmt);
                }
            } else {
                isPass = false;
                ALOGE("error: file %s, mime %s, failed to open output file %s", csrcPath, cmime,
                      cdstPath);
            }
        }
        env->ReleaseStringUTFChars(jdstPath, cdstPath);
    } else {
        isPass = false;
        ALOGE("error: file %s, mime %s, track count exp/rec - %d/%d", csrcPath, cmime, 1,
              mediaInfo->getTrackCount());
    }
    env->ReleaseStringUTFChars(jselector, cselector);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    env->ReleaseStringUTFChars(jmime, cmime);
    delete mediaInfo;
    return static_cast<jboolean>(isPass);
}

int registerAndroidMediaV2CtsMuxerTestApi(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestSetOrientationHint", "(ILjava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestSetOrientationHint},
            {"nativeTestSetLocation", "(ILjava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestSetLocation},
    };
    jclass c = env->FindClass("android/mediav2/cts/MuxerTest$TestApi");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}

int registerAndroidMediaV2CtsMuxerTestMultiTrack(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestMultiTrack",
             "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestMultiTrack},
    };
    jclass c = env->FindClass("android/mediav2/cts/MuxerTest$TestMultiTrack");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}

int registerAndroidMediaV2CtsMuxerTestOffsetPts(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestOffsetPts", "(ILjava/lang/String;Ljava/lang/String;[I)Z",
             (void*)nativeTestOffsetPts},
    };
    jclass c = env->FindClass("android/mediav2/cts/MuxerTest$TestOffsetPts");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}

int registerAndroidMediaV2CtsMuxerTestSimpleMux(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestSimpleMux",
             "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestSimpleMux},
    };
    jclass c = env->FindClass("android/mediav2/cts/MuxerTest$TestSimpleMux");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}

extern int registerAndroidMediaV2CtsMuxerUnitTestApi(JNIEnv* env);

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsMuxerTestApi(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsMuxerTestMultiTrack(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsMuxerTestOffsetPts(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsMuxerTestSimpleMux(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsMuxerUnitTestApi(env) != JNI_OK) return JNI_ERR;
    return JNI_VERSION_1_6;
}