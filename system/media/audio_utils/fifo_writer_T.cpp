/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <atomic>
#include <stdlib.h>
#include <string.h>
#include <audio_utils/fifo_writer_T.h>

template <typename T>
static inline void memcpyWords(T *dst, const T *src, uint32_t count)
{
    switch (count) {
    case 0: break;
// TODO templatize here also, but first confirm no performance regression compared to current
#define _(n) \
    case n: { \
        struct s##n { T a[n]; }; \
        *(struct s##n *)dst = *(const struct s##n *)src; \
        break; \
    }
    _(1) _(2) _(3) _(4) _(5) _(6) _(7) _(8) _(9) _(10) _(11) _(12) _(13) _(14) _(15) _(16)
#undef _
    default:
        memcpy(dst, src, count * sizeof(T));
        break;
    }
}

template <typename T>
audio_utils_fifo_writer_T<T>::audio_utils_fifo_writer_T(audio_utils_fifo& fifo) :
    mLocalRear(0), mFrameCountP2(fifo.mFrameCountP2), mBuffer((T *) fifo.mBuffer),
    mWriterRear(fifo.mWriterRear)
{
    if (fifo.mFrameSize != sizeof(T) || fifo.mFudgeFactor != 0) {
        abort();
    }
}

template <typename T>
audio_utils_fifo_writer_T<T>::~audio_utils_fifo_writer_T()
{
}

template <typename T>
void audio_utils_fifo_writer_T<T>::write(const T *buffer, uint32_t count)
        __attribute__((no_sanitize("integer")))     // mLocalRear += can wrap
{
    uint32_t availToWrite = mFrameCountP2;
    if (availToWrite > count) {
        availToWrite = count;
    }
    uint32_t rearOffset = mLocalRear & (mFrameCountP2 - 1);
    uint32_t part1 = mFrameCountP2 - rearOffset;
    if (part1 >  availToWrite) {
        part1 = availToWrite;
    }
    memcpyWords(&mBuffer[rearOffset], buffer, part1);
    // TODO apply this simplification to other copies of the code
    uint32_t part2 = availToWrite - part1;
    memcpyWords(&mBuffer[0], &buffer[part1], part2);
    mLocalRear += availToWrite;
}

// Instantiate for the specific types we need, which is currently just int32_t and int64_t.

template class audio_utils_fifo_writer_T<int32_t>;
template class audio_utils_fifo_writer_T<int64_t>;
