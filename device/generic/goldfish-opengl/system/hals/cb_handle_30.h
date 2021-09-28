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

#ifndef SYSTEM_HALS_CB_HANDLE_30_H
#define SYSTEM_HALS_CB_HANDLE_30_H

#include <gralloc_cb_bp.h>
#include "goldfish_address_space.h"

const uint32_t CB_HANDLE_MAGIC_30 = CB_HANDLE_MAGIC_BASE | 0x2;

struct cb_handle_30_t : public cb_handle_t {
    cb_handle_30_t(address_space_handle_t p_bufferFd,
                   QEMU_PIPE_HANDLE p_hostHandleRefCountFd,
                   uint32_t p_hostHandle,
                   int32_t p_usage,
                   int32_t p_width,
                   int32_t p_height,
                   int32_t p_format,
                   int32_t p_glFormat,
                   int32_t p_glType,
                   uint32_t p_bufSize,
                   void* p_bufPtr,
                   uint32_t p_mmapedSize,
                   uint64_t p_mmapedOffset,
                   uint32_t p_bytesPerPixel,
                   uint32_t p_stride)
            : cb_handle_t(p_bufferFd,
                          p_hostHandleRefCountFd,
                          CB_HANDLE_MAGIC_30,
                          p_hostHandle,
                          p_usage,
                          p_width,
                          p_height,
                          p_format,
                          p_glFormat,
                          p_glType,
                          p_bufSize,
                          p_bufPtr,
                          p_mmapedOffset),
              mmapedSize(p_mmapedSize),
              bytesPerPixel(p_bytesPerPixel),
              stride(p_stride) {
        numInts = CB_HANDLE_NUM_INTS(numFds);
    }

    bool isValid() const { return (version == sizeof(native_handle_t)) && (magic == CB_HANDLE_MAGIC_30); }

    static cb_handle_30_t* from(void* p) {
        if (!p) { return nullptr; }
        cb_handle_30_t* cb = static_cast<cb_handle_30_t*>(p);
        return cb->isValid() ? cb : nullptr;
    }

    static const cb_handle_30_t* from(const void* p) {
        return from(const_cast<void*>(p));
    }

    static cb_handle_30_t* from_unconst(const void* p) {
        return from(const_cast<void*>(p));
    }

    uint32_t mmapedSize;            // real allocation side
    uint32_t bytesPerPixel;
    uint32_t stride;
};

#endif // SYSTEM_HALS_CB_HANDLE_30_H
