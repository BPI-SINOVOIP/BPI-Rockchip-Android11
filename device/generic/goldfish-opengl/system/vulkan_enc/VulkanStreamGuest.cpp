// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "VulkanStreamGuest.h"

#include "IOStream.h"
#include "ResourceTracker.h"

#include "android/base/Pool.h"
#include "android/base/Tracing.h"

#include <vector>

#include <log/log.h>
#include <inttypes.h>

namespace goldfish_vk {

class VulkanStreamGuest::Impl : public android::base::Stream {
public:
    Impl(IOStream* stream) : mStream(stream) {
        unsetHandleMapping();
        mFeatureBits = ResourceTracker::get()->getStreamFeatures();
    }

    ~Impl() { }

    bool valid() { return true; }

    void alloc(void **ptrAddr, size_t bytes) {
        if (!bytes) {
            *ptrAddr = nullptr;
            return;
        }

        *ptrAddr = mPool.alloc(bytes);
    }

    ssize_t write(const void *buffer, size_t size) override {
        uint8_t* streamBuf = (uint8_t*)mStream->alloc(size);
        memcpy(streamBuf, buffer, size);
        return size;
    }

    ssize_t read(void *buffer, size_t size) override {
        if (!mStream->readback(buffer, size)) {
            ALOGE("FATAL: Could not read back %zu bytes", size);
            abort();
        }
        return size;
    }

    void clearPool() {
        mPool.freeAll();
    }

    void setHandleMapping(VulkanHandleMapping* mapping) {
        mCurrentHandleMapping = mapping;
    }

    void unsetHandleMapping() {
        mCurrentHandleMapping = &mDefaultHandleMapping;
    }

    VulkanHandleMapping* handleMapping() const {
        return mCurrentHandleMapping;
    }

    void flush() {
        commitWrite();
    }

    uint32_t getFeatureBits() const {
        return mFeatureBits;
    }

private:
    size_t oustandingWriteBuffer() const {
        return mWritePos;
    }

    size_t remainingWriteBufferSize() const {
        return mWriteBuffer.size() - mWritePos;
    }

    void commitWrite() {
        AEMU_SCOPED_TRACE("VulkanStreamGuest device write");
        mStream->flush();
    }

    ssize_t bufferedWrite(const void *buffer, size_t size) {
        if (size > remainingWriteBufferSize()) {
            mWriteBuffer.resize((mWritePos + size) << 1);
        }
        memcpy(mWriteBuffer.data() + mWritePos, buffer, size);
        mWritePos += size;
        return size;
    }

    android::base::Pool mPool { 8, 4096, 64 };

    size_t mWritePos = 0;
    std::vector<uint8_t> mWriteBuffer;
    IOStream* mStream = nullptr;
    DefaultHandleMapping mDefaultHandleMapping;
    VulkanHandleMapping* mCurrentHandleMapping;
    uint32_t mFeatureBits = 0;
};

VulkanStreamGuest::VulkanStreamGuest(IOStream *stream) :
    mImpl(new VulkanStreamGuest::Impl(stream)) { }

VulkanStreamGuest::~VulkanStreamGuest() = default;

bool VulkanStreamGuest::valid() {
    return mImpl->valid();
}

void VulkanStreamGuest::alloc(void** ptrAddr, size_t bytes) {
    mImpl->alloc(ptrAddr, bytes);
}

void VulkanStreamGuest::loadStringInPlace(char** forOutput) {
    size_t len = getBe32();

    alloc((void**)forOutput, len + 1);

    memset(*forOutput, 0x0, len + 1);

    if (len > 0) read(*forOutput, len);
}

void VulkanStreamGuest::loadStringArrayInPlace(char*** forOutput) {
    size_t count = getBe32();

    if (!count) {
        *forOutput = nullptr;
        return;
    }

    alloc((void**)forOutput, count * sizeof(char*));

    char **stringsForOutput = *forOutput;

    for (size_t i = 0; i < count; i++) {
        loadStringInPlace(stringsForOutput + i);
    }
}


ssize_t VulkanStreamGuest::read(void *buffer, size_t size) {
    return mImpl->read(buffer, size);
}

ssize_t VulkanStreamGuest::write(const void *buffer, size_t size) {
    return mImpl->write(buffer, size);
}

void VulkanStreamGuest::clearPool() {
    mImpl->clearPool();
}

void VulkanStreamGuest::setHandleMapping(VulkanHandleMapping* mapping) {
    mImpl->setHandleMapping(mapping);
}

void VulkanStreamGuest::unsetHandleMapping() {
    mImpl->unsetHandleMapping();
}

VulkanHandleMapping* VulkanStreamGuest::handleMapping() const {
    return mImpl->handleMapping();
}

void VulkanStreamGuest::flush() {
    mImpl->flush();
}

uint32_t VulkanStreamGuest::getFeatureBits() const {
    return mImpl->getFeatureBits();
}

VulkanCountingStream::VulkanCountingStream() : VulkanStreamGuest(nullptr) { }
VulkanCountingStream::~VulkanCountingStream() = default;

ssize_t VulkanCountingStream::read(void*, size_t size) {
    m_read += size;
    return size;
}

ssize_t VulkanCountingStream::write(const void*, size_t size) {
    m_written += size;
    return size;
}

void VulkanCountingStream::rewind() {
    m_written = 0;
    m_read = 0;
}

} // namespace goldfish_vk
