/*
* Copyright 2011 The Android Open Source Project
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

#ifndef __GRALLOC_CB_H__
#define __GRALLOC_CB_H__

#include <cutils/native_handle.h>
#include <qemu_pipe_types_bp.h>

const uint32_t CB_HANDLE_MAGIC_MASK = 0xFFFFFFF0;
const uint32_t CB_HANDLE_MAGIC_BASE = 0xABFABFA0;

#define CB_HANDLE_NUM_INTS(nfd) \
    ((sizeof(*this)-sizeof(native_handle_t)-nfd*sizeof(int32_t))/sizeof(int32_t))

struct cb_handle_t : public native_handle_t {
    cb_handle_t(int32_t p_bufferFd,
                QEMU_PIPE_HANDLE p_hostHandleRefCountFd,
                uint32_t p_magic,
                uint32_t p_hostHandle,
                int32_t p_usage,
                int32_t p_width,
                int32_t p_height,
                int32_t p_format,
                int32_t p_glFormat,
                int32_t p_glType,
                uint32_t p_bufSize,
                void* p_bufPtr,
                uint64_t p_mmapedOffset)
        : bufferFd(p_bufferFd),
          hostHandleRefCountFd(p_hostHandleRefCountFd),
          magic(p_magic),
          hostHandle(p_hostHandle),
          usage(p_usage),
          width(p_width),
          height(p_height),
          format(p_format),
          glFormat(p_glFormat),
          glType(p_glType),
          bufferSize(p_bufSize),
          mmapedOffsetLo(static_cast<uint32_t>(p_mmapedOffset)),
          mmapedOffsetHi(static_cast<uint32_t>(p_mmapedOffset >> 32)),
          lockedLeft(0),
          lockedTop(0),
          lockedWidth(0),
          lockedHeight(0) {
        version = sizeof(native_handle);
        numFds = ((bufferFd >= 0) ? 1 : 0) + (qemu_pipe_valid(hostHandleRefCountFd) ? 1 : 0);
        numInts = 0; // has to be overwritten in children classes
        setBufferPtr(p_bufPtr);
    }

    void* getBufferPtr() const {
        const uint64_t addr = (uint64_t(bufferPtrHi) << 32) | bufferPtrLo;
        return reinterpret_cast<void*>(static_cast<uintptr_t>(addr));
    }

    void setBufferPtr(void* ptr) {
        const uint64_t addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
        bufferPtrLo = uint32_t(addr);
        bufferPtrHi = uint32_t(addr >> 32);
    }

    uint64_t getMmapedOffset() const {
        return (uint64_t(mmapedOffsetHi) << 32) | mmapedOffsetLo;
    }

    uint32_t allocatedSize() const {
        return getBufferPtr() ? bufferSize : 0;
    }

    bool isValid() const {
        return (version == sizeof(native_handle))
               && (magic & CB_HANDLE_MAGIC_MASK) == CB_HANDLE_MAGIC_BASE;
    }

    static cb_handle_t* from(void* p) {
        if (!p) { return NULL; }
        cb_handle_t* cb = static_cast<cb_handle_t*>(p);
        return cb->isValid() ? cb : NULL;
    }

    static const cb_handle_t* from(const void* p) {
        return from(const_cast<void*>(p));
    }

    static cb_handle_t* from_unconst(const void* p) {
        return from(const_cast<void*>(p));
    }

    // fds
    int32_t bufferFd;       // underlying buffer file handle
    QEMU_PIPE_HANDLE hostHandleRefCountFd; // guest side refcounter to hostHandle

    // ints
    uint32_t magic;         // magic number in order to validate a pointer
    uint32_t hostHandle;    // the host reference to this buffer
    int32_t  usage;         // usage bits the buffer was created with
    int32_t  width;         // buffer width
    int32_t  height;        // buffer height
    int32_t  format;        // real internal pixel format format
    int32_t  glFormat;      // OpenGL format enum used for host h/w color buffer
    int32_t  glType;        // OpenGL type enum used when uploading to host
    uint32_t bufferSize;    // buffer size and location
    uint32_t bufferPtrLo;
    uint32_t bufferPtrHi;
    uint32_t mmapedOffsetLo;
    uint32_t mmapedOffsetHi;
    int32_t  lockedLeft;    // region of buffer locked for s/w write
    int32_t  lockedTop;
    int32_t  lockedWidth;
    int32_t  lockedHeight;
};

#endif //__GRALLOC_CB_H__
