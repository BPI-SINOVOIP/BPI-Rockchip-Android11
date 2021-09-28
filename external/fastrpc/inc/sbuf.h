/**
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SBUF_H
#define SBUF_H

#include <string.h>
#include <stdint.h>
#include "AEEstd.h"

/**
 * lightweight serialize/deserialize buffer.

   For example

   struct sbuf;
   //initialize empty buffer;
   sbuf_init(&sbuf, 0, 0, 0);

   //fill it with data
   sbuf_align(&sbuf, 8);
   sbuf_write(&sbuf, ptr1, 10);
   sbuf_align(&sbuf, 8);
   sbuf_write(&sbuf, ptr2, 20);

   //allocate the memory needed
   mem = malloc(sbuf_needed(&sbuf));

   //initialize with the data
   sbuf_init(&sbuf, 0, mem, sbuf_needed(&sbuf));

   //fill it with data, since it has memory, it will actually copy
   sbuf_align(&sbuf, 8);
   sbuf_write(&sbuf, ptr1, 10);
   sbuf_align(&sbuf, 8);
   sbuf_write(&sbuf, ptr2, 20);

   See sbuf_q.c for more examples
 */


struct sbuf {
   uintptr_t buf;      //! start of valid memory
   uintptr_t bufEnd;   //! end of valid memory
   uintptr_t bufStart; //! start with optinal offset from valid mem
   uintptr_t bufCur;   //! current position, could be outside of valid range
};

/**
 * @param buf, the buffer structure instance
 * @param offset, this value indicates how far ahead the data buffer is
 *                start = data - offset
 * @param data, the valid memory
 * @param dataLen, the length ov valid memory
 */
static __inline void sbuf_init(struct sbuf* buf, int offset, void* data, int dataLen) {
   buf->buf = (uintptr_t)data;
   buf->bufStart = buf->bufCur = (uintptr_t)data - offset;
   buf->bufEnd = (uintptr_t)data + dataLen;
}

//! move the current pointer by len
static __inline void sbuf_advance(struct sbuf* buf, int len) {
   buf->bufCur += len;
}

/**
 * @retval, the amount of memory needed for everything from the start (with the offset)
 * to the current position of the buffer
 */
static __inline int sbuf_needed(struct sbuf* buf) {
   return buf->bufCur - buf->bufStart;
}
/**
 * @retval, the space left in the buffer. A negative value indicates overflow.
 *          A positive value includes the offset.
 */
static __inline int sbuf_left(struct sbuf* buf) {
   return buf->bufEnd - buf->bufCur;
}

//! @retval the current head pointer
static __inline void* sbuf_head(struct sbuf* buf) {
   return (void*)buf->bufCur;
}

//! @retval true if the current pointer is valid
static __inline int sbuf_valid(struct sbuf* buf) {
   return buf->bufCur >= buf->buf && buf->bufCur < buf->bufEnd;
}

//! advance the head pointer so the "needed" is aligned to the align value
#define _SBUF_ALIGN(x, y) (((x) + ((y)-1)) & ~((y)-1))
static __inline void sbuf_align(struct sbuf* buf, uint32_t align) {
   sbuf_advance(buf, _SBUF_ALIGN(sbuf_needed(buf), align) - sbuf_needed(buf));
}

/**
 * Write to the buffer.
 * @param src, the memory to read from.  Will write srcLen bytes to buf from src
 *             from the buf's current position.  Only the valid portion of data will
 *             be written.
 * @param srcLen, the length of src.  The buffer will be advanced by srcLen.
 */
static __inline void sbuf_write(struct sbuf* buf, void *psrc, int srcLen) {
   uintptr_t src = (uintptr_t)psrc;
   if(buf->bufCur + srcLen > buf->buf) {
      int writeLen;
      if(buf->bufCur < buf->buf) {
         int len = buf->buf - buf->bufCur;
         srcLen -= len;
         src += len;
         sbuf_advance(buf, len);
      }
      writeLen = STD_MIN(srcLen, sbuf_left(buf));
      if(writeLen > 0) {
         std_memsmove((void*)buf->bufCur, buf->bufEnd - buf->bufCur, (void*)src, writeLen);
      }
   }
   sbuf_advance(buf, srcLen);
}

/**
 * Read from the buffer into dst.
 * @param dst, the data to write to. Will write dstLen to dst from buf
 *             from the current position of buf.  Only valid memory
 *             will be written to dst.  Invalid overlapping memory will
 *             remain untouched.
 * @param dstLen, the length of dst.  buf will be advanced by dstLen
 */
static __inline void sbuf_read(struct sbuf* buf, void *pdst, int dstLen) {
   uintptr_t dst = (uintptr_t)pdst;
   if(buf->bufCur + dstLen > buf->buf) {
      int readLen;
      if(buf->bufCur < buf->buf) {
         int len = buf->buf - buf->bufCur;
         dstLen -= len;
         dst += len;
         sbuf_advance(buf, len);
      }
      readLen = STD_MIN(dstLen, sbuf_left(buf));
      if(readLen > 0) {
         std_memsmove((void*)dst, dstLen, (void*)buf->bufCur, readLen);
      }
   }
   sbuf_advance(buf, dstLen);
}

#endif
