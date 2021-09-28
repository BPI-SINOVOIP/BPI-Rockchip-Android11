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

#include <cstdint>
#include <cstdio>
#include <cstdlib>

class IOStream {
  public:
    IOStream(unsigned char* buf, size_t bufSize) : m_buf(buf), m_bufSize(bufSize) {
    }

    unsigned char* alloc(size_t len) {
        if (m_allocSize + len > m_bufSize) {
            printf("Command response is too large.\n");
            return nullptr;
        }
        unsigned char* buf = m_buf + m_allocSize;
        m_allocSize += len;
        return buf;
    }

    int flush() {
        m_flushSize = m_allocSize;
        return 0;
    }

    void* getDmaForReading(uint64_t guest_paddr) {
        // GLDMA is not supported, so we don't have to implement this. Just keep
        // a stub here to keep the generated decoder stubs happy
        return nullptr;
    }

    void unlockDma(uint64_t guest_paddr) {
        // GLDMA is not supported, so we don't have to implement this. Just keep
        // a stub here to keep the generated decoder stubs happy
    }

    size_t getFlushSize() {
        return m_flushSize;
    }

  private:
    size_t m_allocSize = 0U;
    size_t m_flushSize = 0U;
    unsigned char* m_buf;
    size_t m_bufSize;
};
