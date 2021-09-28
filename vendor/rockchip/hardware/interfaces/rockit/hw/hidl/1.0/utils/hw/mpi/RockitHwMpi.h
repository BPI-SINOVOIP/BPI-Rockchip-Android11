/*
 * Copyright (C) 2019 Rockchip Electronics Co. LTD
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
#ifndef ANDROID_ROCKIT_HW_MPI_H
#define ANDROID_ROCKIT_HW_MPI_H

#include "RockitHwInterface.h"
#include "utils/Mutex.h"

namespace rockchip {
namespace hardware {
namespace rockit {
namespace hw {
namespace V1_0 {
namespace utils {

using namespace ::android;

class RockitHwMpi : public RockitHwInterface {
public:
    RockitHwMpi();
    virtual ~RockitHwMpi();

    virtual int init(const RockitHWParamPairs& pairs);
    virtual int commitBuffer(const RockitHWBuffer& buffer);
    virtual int giveBackBuffer(const RockitHWBuffer& buffer);
    virtual int process(const RockitHWBufferList& list);
    virtual int enqueue(const RockitHWBuffer& buffer);
    virtual int dequeue(RockitHWBuffer& buffer);
    virtual int control(int cmd, const RockitHWParamPairs& param);
    virtual int query(int cmd, RockitHWParamPairs& out);
    virtual int flush();
    virtual int reset();

protected:
    virtual void* findMppBuffer(int fd);
    virtual void* findDataBuffer(int fd);
    virtual void cleanMppBuffer();
    virtual void cleanMppBuffer(int site);
    virtual void dumpMppBufferList();
    virtual int bufferReady();
    virtual int addDataBufferList(int fd, int mapfd, void* data, int size);
    virtual void freeDataBufferList();
    virtual void freeDataBuffer(int idx);
protected:
    void* mCtx;
    void* mInput;
    void* mOutput;
    int   mDrmFd;
    int   mWStride;
    int   mHStride;
    bool  mDebug;
    Mutex mLock;
};

}  // namespace utils
}  // namespace V1_0
}  // namespace hw
}  // namespace rockit
}  // namespace hardware
}  // namespace rockchip

#endif
