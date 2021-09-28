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

#ifndef RT_METADATA_RETRIEVER_CALLBACK_H_
#define RT_METADATA_RETRIEVER_CALLBACK_H_

#include "rt_metadata.h"

namespace android {

struct RTFrameRect {
    int left, top, right, bottom;
};

class RTMetadataRetrieverCallback {
public:
    RTMetadataRetrieverCallback();
    virtual ~RTMetadataRetrieverCallback();

    virtual int init(RtMetaData* meta);
    virtual int fillVideoFrame(RtMetaData* meta);

    virtual sp<IMemory> extractFrame(RTFrameRect *rect);
    virtual sp<IMemory> extractFrames();

    void setFrame(const sp<IMemory> &frameMem) { mFrameMemory = frameMem; }
protected:
    virtual sp<IMemory> allocVideoFrame(RtMetaData* meta);
    virtual uint8_t fetch_data(uint8_t *line, uint32_t num);
    virtual int convert10bitTo8bit(uint8_t *src, uint8_t *dst);
    virtual int transfRT2OmxColorFormat(int src);
    virtual void freeVideoFrame(sp<IMemory> frame);

private:
    void* mCtx;
    sp<IMemory> mFrameMemory;
};

}

#endif  //  RT_METADATA_RETRIEVER_CALLBACK_H_
