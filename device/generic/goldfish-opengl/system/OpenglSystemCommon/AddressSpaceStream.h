/*
* Copyright (C) 2011 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#ifndef __ADDRESS_SPACE_STREAM_H
#define __ADDRESS_SPACE_STREAM_H

#include "IOStream.h"

#include "address_space_graphics_types.h"
#include "goldfish_address_space.h"

class AddressSpaceStream;

AddressSpaceStream* createAddressSpaceStream(size_t bufSize);

class AddressSpaceStream : public IOStream {
public:
    explicit AddressSpaceStream(
        address_space_handle_t handle,
        uint32_t version,
        struct asg_context context,
        uint64_t ringOffset,
        uint64_t writeBufferOffset);
    ~AddressSpaceStream();

    virtual size_t idealAllocSize(size_t len);
    virtual void *allocBuffer(size_t minSize);
    virtual int commitBuffer(size_t size);
    virtual const unsigned char *readFully( void *buf, size_t len);
    virtual const unsigned char *read( void *buf, size_t *inout_len);
    virtual int writeFully(const void *buf, size_t len);
    virtual const unsigned char *commitBufferAndReadFully(size_t size, void *buf, size_t len);

private:
    bool isInError() const;
    ssize_t speculativeRead(unsigned char* readBuffer, size_t trySize);
    void notifyAvailable();
    uint32_t getRelativeBufferPos(uint32_t pos);
    void advanceWrite();
    void ensureConsumerFinishing();
    void ensureType1Finished();
    void ensureType3Finished();
    int type1Write(uint32_t offset, size_t size);

    unsigned char* m_tmpBuf;
    size_t m_tmpBufSize;
    size_t m_tmpBufXferSize;
    bool m_usingTmpBuf;

    unsigned char* m_readBuf;
    size_t m_read;
    size_t m_readLeft;

    address_space_handle_t m_handle;
    uint32_t m_version;
    struct asg_context m_context;

    uint64_t m_ringOffset;
    uint64_t m_writeBufferOffset;

    uint32_t m_writeBufferSize;
    uint32_t m_writeBufferMask;
    unsigned char* m_buf;
    unsigned char* m_writeStart;
    uint32_t m_writeStep;

    uint32_t m_notifs;
    uint32_t m_written;
};

#endif
