/*
 * Copyright (C) 2018 The Android Open Source Project
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

#pragma once

#include "HostConnection.h"
#include "IOStream.h"

#include <stdlib.h>

/* This file implements an IOStream that uses VIRTGPU_EXECBUFFER ioctls on a
 * virtio-gpu DRM rendernode device to communicate with the host.
 */

struct VirtioGpuCmd;

class VirtioGpuProcessPipe : public ProcessPipe
{
public:
    virtual bool processPipeInit(HostConnectionType connType, renderControl_encoder_context_t *rcEnc);
};

class VirtioGpuStream : public IOStream
{
public:
    explicit VirtioGpuStream(size_t bufSize);
    ~VirtioGpuStream();

    int connect();
    ProcessPipe *getProcessPipe() { return &m_processPipe; }

    // override IOStream so we can see non-rounded allocation sizes
    virtual unsigned char *alloc(size_t len)
    {
        return static_cast<unsigned char *>(allocBuffer(len));
    }

    // override IOStream so we can model the caller's writes
    virtual int flush();

    virtual void *allocBuffer(size_t minSize);
    virtual int writeFully(const void *buf, size_t len);
    virtual const unsigned char *readFully(void *buf, size_t len);
    virtual int commitBuffer(size_t size);
    virtual const unsigned char* commitBufferAndReadFully(size_t size, void *buf, size_t len)
    {
        return commitBuffer(size) ? nullptr : readFully(buf, len);
    }
    virtual const unsigned char *read(void *buf, size_t *inout_len) final
    {
        return readFully(buf, *inout_len);
    }

    bool valid()
    {
        return m_fd >= 0 && m_cmdResp_bo > 0 && m_cmdResp;
    }

    int getRendernodeFd() { return m_fd; }

private:
    // rendernode fd
    int m_fd;

    // command memory buffer
    size_t m_bufSize;
    unsigned char *m_buf;

    // response buffer res handle
    uint32_t m_cmdResp_rh;

    // response buffer ttm buffer object
    uint32_t m_cmdResp_bo;

    // user mapping of response buffer object
    VirtioGpuCmd *m_cmdResp;

    // byte offset to read cursor for last response
    size_t m_cmdRespPos;

    // byte offset to command being assembled
    size_t m_cmdPos;

    // byte offset to flush cursor
    size_t m_flushPos;

    // byte counter of allocs since last command boundary
    size_t m_allocSize;

    // bytes of an alloc flushed through flush() API
    size_t m_allocFlushSize;

    // Fake process pipe implementation
    VirtioGpuProcessPipe m_processPipe;

    // commits all commands, resets buffer offsets
    int commitAll();
};
