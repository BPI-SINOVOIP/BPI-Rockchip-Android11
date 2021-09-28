/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 *
 * author: kevin.chen@rock-chips.com
 */

#include "BitReader.h"

BitReader::BitReader(const uint8_t *data, size_t size)
    : mData(data),
      mSize(size),
      mReservoir(0),
      mNumBitsLeft(0),
      mOverRead(false)
{
}

BitReader::~BitReader()
{
}

bool BitReader::fillReservoir()
{
    if (mSize == 0) {
        mOverRead = true;
        return false;
    }

    mReservoir = 0;
    size_t i;
    for (i = 0; mSize > 0 && i < 4; ++i) {
        mReservoir = (mReservoir << 8) | *mData;

        ++mData;
        --mSize;
    }

    mNumBitsLeft = 8 * i;
    mReservoir <<= 32 - mNumBitsLeft;
    return true;
}

uint32_t BitReader::getBits(size_t n)
{
    uint32_t ret;
    CHECK(getBitsGraceful(n, &ret));
    return ret;
}

uint32_t BitReader::getBitsWithFallback(size_t n, uint32_t fallback)
{
    uint32_t ret = fallback;
    (void)getBitsGraceful(n, &ret);
    return ret;
}

bool BitReader::getBitsGraceful(size_t n, uint32_t *out)
{
    if (n > 32) {
        return false;
    }

    uint32_t result = 0;
    while (n > 0) {
        if (mNumBitsLeft == 0) {
            if (!fillReservoir()) {
                return false;
            }
        }

        size_t m = n;
        if (m > mNumBitsLeft) {
            m = mNumBitsLeft;
        }

        result = (result << m) | (mReservoir >> (32 - m));
        mReservoir <<= m;
        mNumBitsLeft -= m;

        n -= m;
    }

    *out = result;
    return true;
}

bool BitReader::skipBits(size_t n)
{
    uint32_t dummy;
    while (n > 32) {
        if (!getBitsGraceful(32, &dummy)) {
            return false;
        }
        n -= 32;
    }

    if (n > 0) {
        return getBitsGraceful(n, &dummy);
    }
    return true;
}

void BitReader::putBits(uint32_t x, size_t n)
{
    if (mOverRead) {
        return;
    }

    while (mNumBitsLeft + n > 32) {
        mNumBitsLeft -= 8;
        --mData;
        ++mSize;
    }

    mReservoir = (mReservoir >> n) | (x << (32 - n));
    mNumBitsLeft += n;
}

size_t BitReader::numBitsLeft() const
{
    return mSize * 8 + mNumBitsLeft;
}

const uint8_t *BitReader::data() const
{
    return mData - (mNumBitsLeft + 7) / 8;
}
