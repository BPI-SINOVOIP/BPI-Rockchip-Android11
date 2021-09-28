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
#include "AddressSpaceStream.h"

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

AddressSpaceStream* createAddressSpaceStream(size_t ignored_bufSize) {
    // Ignore incoming ignored_bufSize
    (void)ignored_bufSize;

    auto handle = goldfish_address_space_open();
    address_space_handle_t child_device_handle;

    if (!goldfish_address_space_set_subdevice_type(handle, GoldfishAddressSpaceSubdeviceType::Graphics, &child_device_handle)) {
        ALOGE("AddressSpaceStream::create failed (initial device create)\n");
        goldfish_address_space_close(handle);
        return nullptr;
    }

    struct goldfish_address_space_ping request;
    request.metadata = ASG_GET_RING;
    if (!goldfish_address_space_ping(child_device_handle, &request)) {
        ALOGE("AddressSpaceStream::create failed (get ring)\n");
        goldfish_address_space_close(child_device_handle);
        return nullptr;
    }

    uint64_t ringOffset = request.metadata;

    request.metadata = ASG_GET_BUFFER;
    if (!goldfish_address_space_ping(child_device_handle, &request)) {
        ALOGE("AddressSpaceStream::create failed (get buffer)\n");
        goldfish_address_space_close(child_device_handle);
        return nullptr;
    }

    uint64_t bufferOffset = request.metadata;
    uint64_t bufferSize = request.size;

    if (!goldfish_address_space_claim_shared(
        child_device_handle, ringOffset, sizeof(asg_ring_storage))) {
        ALOGE("AddressSpaceStream::create failed (claim ring storage)\n");
        goldfish_address_space_close(child_device_handle);
        return nullptr;
    }

    if (!goldfish_address_space_claim_shared(
        child_device_handle, bufferOffset, bufferSize)) {
        ALOGE("AddressSpaceStream::create failed (claim buffer storage)\n");
        goldfish_address_space_unclaim_shared(child_device_handle, ringOffset);
        goldfish_address_space_close(child_device_handle);
        return nullptr;
    }

    char* ringPtr = (char*)goldfish_address_space_map(
        child_device_handle, ringOffset, sizeof(struct asg_ring_storage));

    if (!ringPtr) {
        ALOGE("AddressSpaceStream::create failed (map ring storage)\n");
        goldfish_address_space_unclaim_shared(child_device_handle, bufferOffset);
        goldfish_address_space_unclaim_shared(child_device_handle, ringOffset);
        goldfish_address_space_close(child_device_handle);
        return nullptr;
    }

    char* bufferPtr = (char*)goldfish_address_space_map(
        child_device_handle, bufferOffset, bufferSize);

    if (!bufferPtr) {
        ALOGE("AddressSpaceStream::create failed (map buffer storage)\n");
        goldfish_address_space_unmap(ringPtr, sizeof(struct asg_ring_storage));
        goldfish_address_space_unclaim_shared(child_device_handle, bufferOffset);
        goldfish_address_space_unclaim_shared(child_device_handle, ringOffset);
        goldfish_address_space_close(child_device_handle);
        return nullptr;
    }

    struct asg_context context =
        asg_context_create(
            ringPtr, bufferPtr, bufferSize);

    request.metadata = ASG_SET_VERSION;
    request.size = 1; // version 1

    if (!goldfish_address_space_ping(child_device_handle, &request)) {
        ALOGE("AddressSpaceStream::create failed (get buffer)\n");
        goldfish_address_space_unmap(bufferPtr, bufferSize);
        goldfish_address_space_unmap(ringPtr, sizeof(struct asg_ring_storage));
        goldfish_address_space_unclaim_shared(child_device_handle, bufferOffset);
        goldfish_address_space_unclaim_shared(child_device_handle, ringOffset);
        goldfish_address_space_close(child_device_handle);
        return nullptr;
    }

    uint32_t version = request.size;

    context.ring_config->transfer_mode = 1;
    context.ring_config->host_consumed_pos = 0;
    context.ring_config->guest_write_pos = 0;

    AddressSpaceStream* res =
        new AddressSpaceStream(
            child_device_handle, version, context,
            ringOffset, bufferOffset);

    return res;
}

AddressSpaceStream::AddressSpaceStream(
    address_space_handle_t handle,
    uint32_t version,
    struct asg_context context,
    uint64_t ringOffset,
    uint64_t writeBufferOffset) :
    IOStream(context.ring_config->flush_interval),
    m_tmpBuf(0),
    m_tmpBufSize(0),
    m_tmpBufXferSize(0),
    m_usingTmpBuf(0),
    m_readBuf(0),
    m_read(0),
    m_readLeft(0),
    m_handle(handle),
    m_version(version),
    m_context(context),
    m_ringOffset(ringOffset),
    m_writeBufferOffset(writeBufferOffset),
    m_writeBufferSize(context.ring_config->buffer_size),
    m_writeBufferMask(m_writeBufferSize - 1),
    m_buf((unsigned char*)context.buffer),
    m_writeStart(m_buf),
    m_writeStep(context.ring_config->flush_interval),
    m_notifs(0),
    m_written(0) {
    // We'll use this in the future, but at the moment,
    // it's a potential compile Werror.
    (void)m_version;
}

AddressSpaceStream::~AddressSpaceStream() {
    goldfish_address_space_unmap(m_context.to_host, sizeof(struct asg_ring_storage));
    goldfish_address_space_unmap(m_context.buffer, m_writeBufferSize);
    goldfish_address_space_unclaim_shared(m_handle, m_ringOffset);
    goldfish_address_space_unclaim_shared(m_handle, m_writeBufferOffset);
    goldfish_address_space_close(m_handle);
    if (m_readBuf) free(m_readBuf);
    if (m_tmpBuf) free(m_tmpBuf);
}

size_t AddressSpaceStream::idealAllocSize(size_t len) {
    if (len > m_writeStep) return len;
    return m_writeStep;
}

void *AddressSpaceStream::allocBuffer(size_t minSize) {
    if (!m_readBuf) {
        m_readBuf = (unsigned char*)malloc(kReadSize);
    }

    size_t allocSize =
        (m_writeStep < minSize ? minSize : m_writeStep);

    if (m_writeStep < allocSize) {
        if (!m_tmpBuf) {
            m_tmpBufSize = allocSize * 2;
            m_tmpBuf = (unsigned char*)malloc(m_tmpBufSize);
        }

        if (m_tmpBufSize < allocSize) {
            m_tmpBufSize = allocSize * 2;
            m_tmpBuf = (unsigned char*)realloc(m_tmpBuf, m_tmpBufSize);
        }

        if (!m_usingTmpBuf) {
            flush();
        }

        m_usingTmpBuf = true;
        m_tmpBufXferSize = allocSize;
        return m_tmpBuf;
    } else {
        if (m_usingTmpBuf) {
            writeFully(m_tmpBuf, m_tmpBufXferSize);
            m_usingTmpBuf = false;
            m_tmpBufXferSize = 0;
        }

        return m_writeStart;
    }
};

int AddressSpaceStream::commitBuffer(size_t size)
{
    if (size == 0) return 0;

    if (m_usingTmpBuf) {
        writeFully(m_tmpBuf, size);
        m_tmpBufXferSize = 0;
        m_usingTmpBuf = false;
        return 0;
    } else {
        int res = type1Write(m_writeStart - m_buf, size);
        advanceWrite();
        return res;
    }
}

const unsigned char *AddressSpaceStream::readFully(void *ptr, size_t totalReadSize)
{

    unsigned char* userReadBuf = static_cast<unsigned char*>(ptr);

    if (!userReadBuf) {
        if (totalReadSize > 0) {
            ALOGE("AddressSpaceStream::commitBufferAndReadFully failed, userReadBuf=NULL, totalReadSize %zu, lethal"
                    " error, exiting.", totalReadSize);
            abort();
        }
        return nullptr;
    }

    // Advance buffered read if not yet consumed.
    size_t remaining = totalReadSize;
    size_t bufferedReadSize =
        m_readLeft < remaining ? m_readLeft : remaining;

    if (bufferedReadSize) {
        memcpy(userReadBuf,
               m_readBuf + (m_read - m_readLeft),
               bufferedReadSize);
        remaining -= bufferedReadSize;
        m_readLeft -= bufferedReadSize;
    }

    if (!remaining) return userReadBuf;

    // Read up to kReadSize bytes if all buffered read has been consumed.
    size_t maxRead = m_readLeft ? 0 : kReadSize;
    ssize_t actual = 0;

    if (maxRead) {
        actual = speculativeRead(m_readBuf, maxRead);

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
                   m_readBuf + (m_read - m_readLeft),
                   bufferedReadSize);
            remaining -= bufferedReadSize;
            m_readLeft -= bufferedReadSize;
            continue;
        }

        actual = speculativeRead(m_readBuf, kReadSize);

        if (actual == 0) {
            ALOGD("%s: Failed reading from pipe: %d", __FUNCTION__,  errno);
            return NULL;
        }

        if (actual > 0) {
            m_read = m_readLeft = actual;
            continue;
        }
    }

    return userReadBuf;
}

const unsigned char *AddressSpaceStream::read(void *buf, size_t *inout_len) {
    unsigned char* dst = (unsigned char*)buf;
    size_t wanted = *inout_len;
    ssize_t actual = speculativeRead(dst, wanted);

    if (actual >= 0) {
        *inout_len = actual;
    } else {
        return nullptr;
    }

    return (const unsigned char*)dst;
}

int AddressSpaceStream::writeFully(const void *buf, size_t size)
{
    ensureConsumerFinishing();
    ensureType3Finished();
    ensureType1Finished();

    m_context.ring_config->transfer_size = size;
    m_context.ring_config->transfer_mode = 3;

    size_t sent = 0;
    size_t quarterRingSize = m_writeBufferSize / 4;
    size_t chunkSize = size < quarterRingSize ? size : quarterRingSize;
    const uint8_t* bufferBytes = (const uint8_t*)buf;

    while (sent < size) {
        size_t remaining = size - sent;
        size_t sendThisTime = remaining < chunkSize ? remaining : chunkSize;

        long sentChunks =
            ring_buffer_view_write(
                m_context.to_host_large_xfer.ring,
                &m_context.to_host_large_xfer.view,
                bufferBytes + sent, sendThisTime, 1);

        if (*(m_context.host_state) != ASG_HOST_STATE_CAN_CONSUME) {
            notifyAvailable();
        }

        if (sentChunks == 0) {
            ring_buffer_yield();
        }

        sent += sentChunks * sendThisTime;

        if (isInError()) {
            return -1;
        }
    }

    ensureType3Finished();
    m_context.ring_config->transfer_mode = 1;
    m_written += size;
    return 0;
}

const unsigned char *AddressSpaceStream::commitBufferAndReadFully(
    size_t writeSize, void *userReadBufPtr, size_t totalReadSize) {

    if (m_usingTmpBuf) {
        writeFully(m_tmpBuf, writeSize);
        m_usingTmpBuf = false;
        m_tmpBufXferSize = 0;
        return readFully(userReadBufPtr, totalReadSize);
    } else {
        commitBuffer(writeSize);
        return readFully(userReadBufPtr, totalReadSize);
    }
}

bool AddressSpaceStream::isInError() const {
    return 1 == m_context.ring_config->in_error;
}

ssize_t AddressSpaceStream::speculativeRead(unsigned char* readBuffer, size_t trySize) {
    ensureConsumerFinishing();
    ensureType3Finished();
    ensureType1Finished();

    size_t actuallyRead = 0;
    while (!actuallyRead) {
        uint32_t readAvail =
            ring_buffer_available_read(
                m_context.from_host_large_xfer.ring,
                &m_context.from_host_large_xfer.view);

        if (!readAvail) {
            ring_buffer_yield();
            continue;
        }

        uint32_t toRead = readAvail > trySize ?  trySize : readAvail;

        long stepsRead = ring_buffer_view_read(
            m_context.from_host_large_xfer.ring,
            &m_context.from_host_large_xfer.view,
            readBuffer, toRead, 1);

        actuallyRead += stepsRead * toRead;

        if (isInError()) {
            return -1;
        }
    }

    return actuallyRead;
}

void AddressSpaceStream::notifyAvailable() {
    struct goldfish_address_space_ping request;
    request.metadata = ASG_NOTIFY_AVAILABLE;
    goldfish_address_space_ping(m_handle, &request);
    ++m_notifs;
}

uint32_t AddressSpaceStream::getRelativeBufferPos(uint32_t pos) {
    return pos & m_writeBufferMask;
}

void AddressSpaceStream::advanceWrite() {
    m_writeStart += m_context.ring_config->flush_interval;

    if (m_writeStart == m_buf + m_context.ring_config->buffer_size) {
        m_writeStart = m_buf;
    }
}

void AddressSpaceStream::ensureConsumerFinishing() {
    uint32_t currAvailRead = ring_buffer_available_read(m_context.to_host, 0);

    while (currAvailRead) {
        ring_buffer_yield();
        uint32_t nextAvailRead = ring_buffer_available_read(m_context.to_host, 0);

        if (nextAvailRead != currAvailRead) {
            break;
        }

        if (*(m_context.host_state) != ASG_HOST_STATE_CAN_CONSUME) {
            notifyAvailable();
            break;
        }
    }
}

void AddressSpaceStream::ensureType1Finished() {
    ensureConsumerFinishing();

    uint32_t currAvailRead =
        ring_buffer_available_read(m_context.to_host, 0);

    while (currAvailRead) {
        ring_buffer_yield();
        currAvailRead = ring_buffer_available_read(m_context.to_host, 0);
        if (isInError()) {
            return;
        }
    }
}

void AddressSpaceStream::ensureType3Finished() {
    uint32_t availReadLarge =
        ring_buffer_available_read(
            m_context.to_host_large_xfer.ring,
            &m_context.to_host_large_xfer.view);
    while (availReadLarge) {
        ring_buffer_yield();
        availReadLarge =
            ring_buffer_available_read(
                m_context.to_host_large_xfer.ring,
                &m_context.to_host_large_xfer.view);
        if (*(m_context.host_state) != ASG_HOST_STATE_CAN_CONSUME) {
            notifyAvailable();
        }
        if (isInError()) {
            return;
        }
    }
}

int AddressSpaceStream::type1Write(uint32_t bufferOffset, size_t size) {
    size_t sent = 0;
    size_t sizeForRing = sizeof(struct asg_type1_xfer);

    struct asg_type1_xfer xfer = {
        bufferOffset,
        (uint32_t)size,
    };

    uint8_t* writeBufferBytes = (uint8_t*)(&xfer);

    uint32_t maxOutstanding = 1;
    uint32_t maxSteps = m_context.ring_config->buffer_size /
            m_context.ring_config->flush_interval;

    if (maxSteps > 1) maxOutstanding = maxSteps >> 1;

    uint32_t ringAvailReadNow = ring_buffer_available_read(m_context.to_host, 0);

    while (ringAvailReadNow >= maxOutstanding) {
        ensureConsumerFinishing();
        ring_buffer_yield();
        ringAvailReadNow = ring_buffer_available_read(m_context.to_host, 0);
    }

    while (sent < sizeForRing) {

        long sentChunks = ring_buffer_write(
            m_context.to_host,
            writeBufferBytes + sent,
            sizeForRing - sent, 1);

        if (*(m_context.host_state) != ASG_HOST_STATE_CAN_CONSUME) {
            notifyAvailable();
        }

        if (sentChunks == 0) {
            ring_buffer_yield();
        }

        sent += sentChunks * (sizeForRing - sent);

        if (isInError()) {
            return -1;
        }
    }

    ensureConsumerFinishing();
    m_written += size;

    float mb = (float)m_written / 1048576.0f;
    if (mb > 100.0f) {
        ALOGD("%s: %f mb in %d notifs. %f mb/notif\n", __func__,
              mb, m_notifs, m_notifs ? mb / (float)m_notifs : 0.0f);
        m_notifs = 0;
        m_written = 0;
    }

    return 0;
}
