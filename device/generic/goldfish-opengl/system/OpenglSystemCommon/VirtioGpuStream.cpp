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

#include "VirtioGpuStream.h"

#include <cros_gralloc_handle.h>
#include <drm/virtgpu_drm.h>
#include <xf86drm.h>

#include <sys/types.h>
#include <sys/mman.h>

#include <errno.h>
#include <unistd.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

// In a virtual machine, there should only be one GPU
#define RENDERNODE_MINOR 128

// Maximum size of readback / response buffer in bytes
#define MAX_CMDRESPBUF_SIZE (10*PAGE_SIZE)

// Attributes use to allocate our response buffer
// Similar to virgl's fence objects
#define PIPE_BUFFER             0
#define VIRGL_FORMAT_R8_UNORM   64
#define VIRGL_BIND_CUSTOM       (1 << 17)

// Conservative; see virgl_winsys.h
#define VIRGL_MAX_CMDBUF_DWORDS (16*1024)
#define VIRGL_MAX_CMDBUF_SIZE   (4*VIRGL_MAX_CMDBUF_DWORDS)

struct VirtioGpuCmd {
    uint32_t op;
    uint32_t cmdSize;
    unsigned char buf[0];
} __attribute__((packed));

bool VirtioGpuProcessPipe::processPipeInit(HostConnectionType, renderControl_encoder_context_t *rcEnc)
{
  union {
      uint64_t proto;
      struct {
          int pid;
          int tid;
      } id;
  } puid = {
      .id.pid = getpid(),
      .id.tid = gettid(),
  };
  rcEnc->rcSetPuid(rcEnc, puid.proto);
  return true;
}

VirtioGpuStream::VirtioGpuStream(size_t bufSize) :
    IOStream(0U),
    m_fd(-1),
    m_bufSize(bufSize),
    m_buf(nullptr),
    m_cmdResp_rh(0U),
    m_cmdResp_bo(0U),
    m_cmdResp(nullptr),
    m_cmdRespPos(0U),
    m_cmdPos(0U),
    m_flushPos(0U),
    m_allocSize(0U),
    m_allocFlushSize(0U)
{
}

VirtioGpuStream::~VirtioGpuStream()
{
    if (m_cmdResp) {
        munmap(m_cmdResp, MAX_CMDRESPBUF_SIZE);
    }

    if (m_cmdResp_bo > 0U) {
        drm_gem_close gem_close = {
            .handle = m_cmdResp_bo,
        };
        drmIoctl(m_fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
    }

    if (m_fd >= 0) {
        close(m_fd);
    }

    free(m_buf);
}

int VirtioGpuStream::connect()
{
    if (m_fd < 0) {
        m_fd = drmOpenRender(RENDERNODE_MINOR);
        if (m_fd < 0) {
            ERR("%s: failed with fd %d (%s)", __func__, m_fd, strerror(errno));
            return -1;
        }
    }

    if (!m_cmdResp_bo) {
        drm_virtgpu_resource_create create = {
            .target     = PIPE_BUFFER,
            .format     = VIRGL_FORMAT_R8_UNORM,
            .bind       = VIRGL_BIND_CUSTOM,
            .width      = MAX_CMDRESPBUF_SIZE,
            .height     = 1U,
            .depth      = 1U,
            .array_size = 0U,
            .size       = MAX_CMDRESPBUF_SIZE,
            .stride     = MAX_CMDRESPBUF_SIZE,
        };
        int ret = drmIoctl(m_fd, DRM_IOCTL_VIRTGPU_RESOURCE_CREATE, &create);
        if (ret) {
            ERR("%s: failed with %d allocating command response buffer (%s)",
                __func__, ret, strerror(errno));
            return -1;
        }
        m_cmdResp_bo = create.bo_handle;
        if (!m_cmdResp_bo) {
            ERR("%s: no handle when allocating command response buffer",
                __func__);
            return -1;
        }
        m_cmdResp_rh = create.res_handle;
        if (create.size != MAX_CMDRESPBUF_SIZE) {
	    ERR("%s: command response buffer wrongly sized, create.size=%zu "
		"!= %zu", __func__,
		static_cast<size_t>(create.size),
		static_cast<size_t>(MAX_CMDRESPBUF_SIZE));
	    abort();
	}
    }

    if (!m_cmdResp) {
        drm_virtgpu_map map = {
            .handle = m_cmdResp_bo,
        };
        int ret = drmIoctl(m_fd, DRM_IOCTL_VIRTGPU_MAP, &map);
        if (ret) {
            ERR("%s: failed with %d mapping command response buffer (%s)",
                __func__, ret, strerror(errno));
            return -1;
        }
        m_cmdResp = static_cast<VirtioGpuCmd *>(mmap64(nullptr,
                                                       MAX_CMDRESPBUF_SIZE,
                                                       PROT_READ, MAP_SHARED,
                                                       m_fd, map.offset));
        if (m_cmdResp == MAP_FAILED) {
            ERR("%s: failed with %d mmap'ing command response buffer (%s)",
                __func__, ret, strerror(errno));
            return -1;
        }
    }

    return 0;
}

int VirtioGpuStream::flush()
{
    int ret = commitBuffer(m_allocSize - m_allocFlushSize);
    if (ret)
        return ret;
    m_allocFlushSize = m_allocSize;
    return 0;
}

void *VirtioGpuStream::allocBuffer(size_t minSize)
{
    if (m_buf) {
        // Try to model the alloc() calls being made by the user. They should be
        // obeying the protocol and using alloc() for anything they don't write
        // with writeFully(), so we can know if this alloc() is for part of a
        // command, or not. If it is not for part of a command, we are starting
        // a new command, and should increment m_cmdPos.
        VirtioGpuCmd *cmd = reinterpret_cast<VirtioGpuCmd *>(&m_buf[m_cmdPos]);
        if (m_allocSize + minSize > cmd->cmdSize) {
            m_allocFlushSize = 0U;
            m_allocSize = 0U;
            // This might also be a convenient point to flush commands
            if (m_cmdPos + cmd->cmdSize + minSize > m_bufSize) {
                if (commitAll() < 0) {
                    ERR("%s: command flush failed", __func__);
                    m_flushPos = 0U;
                    m_bufSize = 0U;
                    m_cmdPos = 0U;
                    free(m_buf);
                    m_buf = nullptr;
                    return nullptr;
                }
            } else {
                m_cmdPos += cmd->cmdSize;
                m_flushPos = m_cmdPos;
            }
        }
    }

    // Update m_allocSize here, before minSize is tampered with below
    m_allocSize += minSize;

    // Make sure anything we already have written to the buffer is retained
    minSize += m_flushPos;

    size_t allocSize = (m_bufSize < minSize ? minSize : m_bufSize);
    if (!m_buf) {
        m_buf = static_cast<unsigned char *>(malloc(allocSize));
    } else if (m_bufSize < allocSize) {
        unsigned char *p = static_cast<unsigned char *>(realloc(m_buf, allocSize));
        if (!p) {
            free(m_buf);
        }
        m_buf = p;
    }
    if (!m_buf) {
        ERR("%s: alloc (%zu) failed\n", __func__, allocSize);
        m_allocFlushSize = 0U;
        m_allocSize = 0U;
        m_flushPos = 0U;
        m_bufSize = 0U;
        m_cmdPos = 0U;
    } else {
        m_bufSize = allocSize;
    }
    if (m_flushPos == 0 && m_cmdPos == 0) {
      // During initialization, HostConnection will send an empty command
      // packet to check the connection is good, but it doesn't obey the usual
      // line protocol. This is a 4 byte write to [0], which is our 'op' field,
      // and we don't have an op=0 so it's OK. We fake up a valid length, and
      // overload this workaround by putting the res_handle for the readback
      // buffer in the command payload, patched in just before we submit.
      VirtioGpuCmd *cmd = reinterpret_cast<VirtioGpuCmd *>(&m_buf[m_cmdPos]);
      cmd->op = 0U;
      cmd->cmdSize = sizeof(*cmd) + sizeof(__u32);
    }
    return m_buf + m_cmdPos;
}

// For us, writeFully() means to write a command without any header, directly
// into the buffer stream. We can use the packet frame written directly to the
// stream to verify this write is within bounds, then update the counter.

int VirtioGpuStream::writeFully(const void *buf, size_t len)
{
    if (!valid())
        return -1;

    if (!buf) {
        if (len > 0) {
            // If len is non-zero, buf must not be NULL. Otherwise the pipe would
            // be in a corrupted state, which is lethal for the emulator.
            ERR("%s: failed, buf=NULL, len %zu, lethal error, exiting",
                __func__, len);
            abort();
        }
        return 0;
    }

    VirtioGpuCmd *cmd = reinterpret_cast<VirtioGpuCmd *>(&m_buf[m_cmdPos]);

    if (m_flushPos < sizeof(*cmd)) {
        ERR("%s: writeFully len %zu would overwrite command header, "
            "cmd_pos=%zu, flush_pos=%zu, lethal error, exiting", __func__,
            len, m_cmdPos, m_flushPos);
        abort();
    }

    if (m_flushPos + len > cmd->cmdSize) {
        ERR("%s: writeFully len %zu would overflow the command bounds, "
            "cmd_pos=%zu, flush_pos=%zu, cmdsize=%zu, lethal error, exiting",
            __func__, len, m_cmdPos, m_flushPos, cmd->cmdSize);
        abort();
    }

    if (len > VIRGL_MAX_CMDBUF_SIZE) {
        ERR("%s: Large command (%zu bytes) exceeds virgl limits",
            __func__, len);
        /* Fall through */
    }

    memcpy(&m_buf[m_flushPos], buf, len);
    commitBuffer(len);
    m_allocSize += len;
    return 0;
}

const unsigned char *VirtioGpuStream::readFully(void *buf, size_t len)
{
    if (!valid())
        return nullptr;

    if (!buf) {
        if (len > 0) {
            // If len is non-zero, buf must not be NULL. Otherwise the pipe would
            // be in a corrupted state, which is lethal for the emulator.
            ERR("%s: failed, buf=NULL, len %zu, lethal error, exiting.",
                __func__, len);
            abort();
        }
        return nullptr;
    }

    // Read is too big for current architecture
    if (len > MAX_CMDRESPBUF_SIZE - sizeof(*m_cmdResp)) {
        ERR("%s: failed, read too large, len %zu, lethal error, exiting.",
            __func__, len);
        abort();
    }

    // Commit all outstanding write commands (if any)
    if (commitAll() < 0) {
        ERR("%s: command flush failed", __func__);
        return nullptr;
    }

    if (len > 0U && m_cmdRespPos == 0U) {
        // When we are about to read for the first time, wait for the virtqueue
        // to drain to this command, otherwise the data could be stale
        drm_virtgpu_3d_wait wait = {
            .handle = m_cmdResp_bo,
        };
        int ret = drmIoctl(m_fd, DRM_IOCTL_VIRTGPU_WAIT, &wait);
        if (ret) {
            ERR("%s: failed with %d waiting for response buffer (%s)",
                __func__, ret, strerror(errno));
            // Fall through, hope for the best
        }
    }

    // Most likely a protocol implementation error
    if (m_cmdResp->cmdSize - sizeof(*m_cmdResp) < m_cmdRespPos + len) {
        ERR("%s: failed, op %zu, len %zu, cmdSize %zu, pos %zu, lethal "
            "error, exiting.", __func__, m_cmdResp->op, len,
            m_cmdResp->cmdSize, m_cmdRespPos);
        abort();
    }

    memcpy(buf, &m_cmdResp->buf[m_cmdRespPos], len);

    if (m_cmdRespPos + len == m_cmdResp->cmdSize - sizeof(*m_cmdResp)) {
        m_cmdRespPos = 0U;
    } else {
        m_cmdRespPos += len;
    }

    return reinterpret_cast<const unsigned char *>(buf);
}

int VirtioGpuStream::commitBuffer(size_t size)
{
    if (m_flushPos + size > m_bufSize) {
        ERR("%s: illegal commit size %zu, flushPos %zu, bufSize %zu",
            __func__, size, m_flushPos, m_bufSize);
        return -1;
    }
    m_flushPos += size;
    return 0;
}

int VirtioGpuStream::commitAll()
{
    size_t pos = 0U, numFlushed = 0U;
    while (pos < m_flushPos) {
        VirtioGpuCmd *cmd = reinterpret_cast<VirtioGpuCmd *>(&m_buf[pos]);

        // Should never happen
        if (pos + cmd->cmdSize > m_bufSize) {
            ERR("%s: failed, pos %zu, cmdSize %zu, bufSize %zu, lethal "
                "error, exiting.", __func__, pos, cmd->cmdSize, m_bufSize);
            abort();
        }

        // Saw dummy command; patch it with res handle
        if (cmd->op == 0) {
            *(uint32_t *)cmd->buf = m_cmdResp_rh;
        }

        // Flush a single command
        drm_virtgpu_execbuffer execbuffer = {
            .size           = cmd->cmdSize,
            .command        = reinterpret_cast<__u64>(cmd),
            .bo_handles     = reinterpret_cast<__u64>(&m_cmdResp_bo),
            .num_bo_handles = 1U,
        };
        int ret = drmIoctl(m_fd, DRM_IOCTL_VIRTGPU_EXECBUFFER, &execbuffer);
        if (ret) {
            ERR("%s: failed with %d executing command buffer (%s)",  __func__,
                ret, strerror(errno));
            return -1;
        }

        pos += cmd->cmdSize;
        numFlushed++;
    }

    if (pos > m_flushPos) {
        ERR("%s: aliasing, flushPos %zu, pos %zu, probably ok", __func__,
            m_flushPos, pos);
        /* Fall through */
    }

    m_flushPos = 0U;
    m_cmdPos = 0U;
    return 0;
}
