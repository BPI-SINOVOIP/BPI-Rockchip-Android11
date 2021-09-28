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

#ifndef ROCKIT_METADATA_RETRIEVER_H_
#define ROCKIT_METADATA_RETRIEVER_H_

#include <android/IMediaExtractor.h>
#include <media/MediaMetadataRetrieverInterface.h>
#include <utils/KeyedVector.h>

#include "RTLibDefine.h"
#include "rt_metadata.h"
#include "RTMetadataRetrieverInterface.h"
#include "RTMetadataRetrieverCallback.h"


namespace android {

class DataSource;
struct ImageDecoder;
struct FrameRect;

struct RockitRetrieverCtx{
    void  *mLibFd;
    createMetaDataRetrieverFunc      *mCreateRetrieverFunc;
    destroyMetaDataRetrieverFunc     *mDestroyRetrieverFunc;
    createRockitMetaDataFunc         *mCreateMetaDataFunc;
    destroyRockitMetaDataFunc        *mDestroyMetaDataFunc;
    RTMetadataRetrieverInterface     *mRetriever;
    RtMetaData                       *mRtMetaData;
    RTMetadataRetrieverCallback      *mCallBack;
};

struct RockitMetadataRetriever : public MediaMetadataRetrieverBase {
public:
    RockitMetadataRetriever();
    virtual ~RockitMetadataRetriever();

    virtual status_t setDataSource(
            const sp<IMediaHTTPService> &httpService,
            const char *url,
            const KeyedVector<String8, String8> *headers);

    virtual status_t setDataSource(int fd, int64_t offset, int64_t length);
    virtual status_t setDataSource(const sp<DataSource>& source, const char *mime);

    virtual sp<IMemory> getFrameAtTime(
            int64_t timeUs, int option, int colorFormat, bool metaOnly);
    virtual sp<IMemory> getImageAtIndex(
            int index, int colorFormat, bool metaOnly, bool thumbnail);
    virtual sp<IMemory> getImageRectAtIndex(
            int index, int colorFormat, int left, int top, int right, int bottom);
    virtual sp<IMemory> getFrameAtIndex(
            int frameIndex, int colorFormat, bool metaOnly);

    virtual MediaAlbumArt *extractAlbumArt();
    virtual const char *extractMetadata(int keyCode);

protected:
    void createMetadataRetriever();
    void destoryMetadataRetriever();
    bool isEnable();
    bool check();

     sp<IMemory> getFrameInternal(int64_t timeUs, int option, int colorFormat, bool metaOnly);

private:
    Mutex mLock;
    bool mEnable;

    bool mParsedMetaData;
    KeyedVector<int, String8> mMetaData;
    MediaAlbumArt *mAlbumArt;

   // sp<ImageDecoder> mImageDecoder;
    int mLastImageIndex;

    RockitRetrieverCtx  *mCtx;

private:
    RockitMetadataRetriever(const RockitMetadataRetriever &);
    RockitMetadataRetriever &operator=(const RockitMetadataRetriever &);

    // Delete album art and clear metadata.
    void clearMetadata();
    void clearRTMetadata();
    void parseMetaData();
    void prepareUrlHeader(const KeyedVector<String8, String8> *headers, String8 *str);
    bool getDstColorFormat(int colorFormat, int *dstFormat, int32_t *dstBpp);
};

}  // namespace android

#endif // ROCKIT_METADATA_RETRIEVER_H_
