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
#include "QemuPipeStream.h"
#include <qemu_pipe_bp.h>

#if PLATFORM_SDK_VERSION < 26
#include <cutils/log.h>
#else
#include <log/log.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static const size_t kReadSize = 512 * 1024;
static const size_t kWriteOffset = kReadSize;

QemuPipeStream::QemuPipeStream(size_t bufSize) :
    IOStream(bufSize),
    m_sock((QEMU_PIPE_HANDLE)(-1)),
    m_bufsize(bufSize),
    m_buf(NULL),
    m_read(0),
    m_readLeft(0)
{
}

QemuPipeStream::QemuPipeStream(QEMU_PIPE_HANDLE sock, size_t bufSize) :
    IOStream(bufSize),
    m_sock(sock),
    m_bufsize(bufSize),
    m_buf(NULL),
    m_read(0),
    m_readLeft(0)
{
}

QemuPipeStream::~QemuPipeStream()
{
    if (valid()) {
        flush();
        qemu_pipe_close(m_sock);
    }
    if (m_buf != NULL) {
        free(m_buf);
    }
}


int QemuPipeStream::connect(void)
{
    m_sock = qemu_pipe_open("opengles");
    if (!valid()) {
        ALOGE("%s: failed to connect to opengles pipe", __FUNCTION__);
        qemu_pipe_print_error(m_sock);
        return -1;
    }
    return 0;
}

void *QemuPipeStream::allocBuffer(size_t minSize)
{
    // Add dedicated read buffer space at the front of the buffer.
    minSize += kReadSize;

    size_t allocSize = (m_bufsize < minSize ? minSize : m_bufsize);
    if (!m_buf) {
        m_buf = (unsigned char *)malloc(allocSize);
    }
    else if (m_bufsize < allocSize) {
        unsigned char *p = (unsigned char *)realloc(m_buf, allocSize);
        if (p != NULL) {
            m_buf = p;
            m_bufsize = allocSize;
        } else {
            ERR("realloc (%zu) failed\n", allocSize);
            free(m_buf);
            m_buf = NULL;
            m_bufsize = 0;
        }
    }

    return m_buf + kWriteOffset;
};

int QemuPipeStream::commitBuffer(size_t size)
{
    if (size == 0) return 0;
    return writeFully(m_buf + kWriteOffset, size);
}

int QemuPipeStream::writeFully(const void *buf, size_t len)
{
    return qemu_pipe_write_fully(m_sock, buf, len);
}

QEMU_PIPE_HANDLE QemuPipeStream::getSocket() const {
    return m_sock;
}

const unsigned char *QemuPipeStream::readFully(void *buf, size_t len)
{
    return commitBufferAndReadFully(0, buf, len);
}

const unsigned char *QemuPipeStream::commitBufferAndReadFully(size_t writeSize, void *userReadBufPtr, size_t totalReadSize) {

    unsigned char* userReadBuf = static_cast<unsigned char*>(userReadBufPtr);

    if (!valid()) return NULL;

    if (!userReadBuf) {
        if (totalReadSize > 0) {
            ALOGE("QemuPipeStream::commitBufferAndReadFully failed, userReadBuf=NULL, totalReadSize %zu, lethal"
                    " error, exiting.", totalReadSize);
            abort();
        }
        if (!writeSize) {
            return NULL;
        }
    }

    // Advance buffered read if not yet consumed.
    size_t remaining = totalReadSize;
    size_t bufferedReadSize = m_readLeft < remaining ? m_readLeft : remaining;
    if (bufferedReadSize) {
        memcpy(userReadBuf, m_buf + (m_read - m_readLeft), bufferedReadSize);
        remaining -= bufferedReadSize;
        m_readLeft -= bufferedReadSize;
    }

    // Early out if nothing left to do.
    if (!writeSize && !remaining) {
        return userReadBuf;
    }

    writeFully(m_buf + kWriteOffset, writeSize);

    // Now done writing. Early out if no reading left to do.
    if (!remaining) {
        return userReadBuf;
    }

    // Read up to kReadSize bytes if all buffered read has been consumed.
    size_t maxRead = m_readLeft ? 0 : kReadSize;

    ssize_t actual = 0;

    if (maxRead) {
        actual = qemu_pipe_read(m_sock, m_buf, maxRead);
        // Updated buffered read size.
        if (actual > 0) {
            m_read = m_readLeft = actual;
        }

        if (actual == 0) {
            ALOGD("%s: end of pipe", __FUNCTION__);
            return NULL;
        }
    }

    // Consume buffered read and read more if necessary.
    while (remaining) {
        bufferedReadSize = m_readLeft < remaining ? m_readLeft : remaining;
        if (bufferedReadSize) {
            memcpy(userReadBuf + (totalReadSize - remaining),
                   m_buf + (m_read - m_readLeft),
                   bufferedReadSize);
            remaining -= bufferedReadSize;
            m_readLeft -= bufferedReadSize;
            continue;
        }

        actual = qemu_pipe_read(m_sock, m_buf, kReadSize);

        if (actual == 0) {
            ALOGD("%s: Failed reading from pipe: %d", __FUNCTION__,  errno);
            return NULL;
        }

        if (actual > 0) {
            m_read = m_readLeft = actual;
            continue;
        }

        if (!qemu_pipe_try_again(actual)) {
            ALOGD("%s: Error reading from pipe: %d", __FUNCTION__, errno);
            return NULL;
        }
    }

    return userReadBuf;
}

const unsigned char *QemuPipeStream::read( void *buf, size_t *inout_len)
{
    //DBG(">> QemuPipeStream::read %d\n", *inout_len);
    if (!valid()) return NULL;
    if (!buf) {
      ERR("QemuPipeStream::read failed, buf=NULL");
      return NULL;  // do not allow NULL buf in that implementation
    }

    int n = recv(buf, *inout_len);

    if (n > 0) {
        *inout_len = n;
        return (const unsigned char *)buf;
    }

    //DBG("<< QemuPipeStream::read %d\n", *inout_len);
    return NULL;
}

int QemuPipeStream::recv(void *buf, size_t len)
{
    if (!valid()) return int(ERR_INVALID_SOCKET);
    char* p = (char *)buf;
    int ret = 0;
    while(len > 0) {
        int res = qemu_pipe_read(m_sock, p, len);
        if (res > 0) {
            p += res;
            ret += res;
            len -= res;
            continue;
        }
        if (res == 0) { /* EOF */
             break;
        }
        if (qemu_pipe_try_again(res)) {
            continue;
        }

        /* A real error */
        if (ret == 0)
            ret = -1;
        break;
    }
    return ret;
}
