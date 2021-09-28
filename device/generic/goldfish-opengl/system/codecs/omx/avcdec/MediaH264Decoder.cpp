/*
 * Copyright 2015 The Android Open Source Project
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

#include <utils/Log.h>

#include "MediaH264Decoder.h"
#include "goldfish_media_utils.h"
#include <string.h>

MediaH264Decoder::MediaH264Decoder(RenderMode renderMode) :mRenderMode(renderMode) {
  if (renderMode == RenderMode::RENDER_BY_HOST_GPU) {
      mVersion = 200;
  } else if (renderMode == RenderMode::RENDER_BY_GUEST_CPU) {
      mVersion = 100;
  }
}

void MediaH264Decoder::initH264Context(unsigned int width,
                                       unsigned int height,
                                       unsigned int outWidth,
                                       unsigned int outHeight,
                                       PixelFormat pixFmt) {
    auto transport = GoldfishMediaTransport::getInstance();
    if (!mHasAddressSpaceMemory) {
        int slot = transport->getMemorySlot();
        if (slot < 0) {
            ALOGE("ERROR: Failed to initH264Context: cannot get memory slot");
            return;
        }
        mAddressOffSet = static_cast<unsigned int>(slot) * 8 * (1 << 20);
        ALOGD("got memory lot %d addrr %x", slot, mAddressOffSet);
        mHasAddressSpaceMemory = true;
    }
    transport->writeParam(mVersion, 0, mAddressOffSet);
    transport->writeParam(width, 1, mAddressOffSet);
    transport->writeParam(height, 2, mAddressOffSet);
    transport->writeParam(outWidth, 3, mAddressOffSet);
    transport->writeParam(outHeight, 4, mAddressOffSet);
    transport->writeParam(static_cast<uint64_t>(pixFmt), 5, mAddressOffSet);
    transport->sendOperation(MediaCodecType::H264Codec,
                             MediaOperation::InitContext, mAddressOffSet);
    auto* retptr = transport->getReturnAddr(mAddressOffSet);
    mHostHandle = *(uint64_t*)(retptr);
    ALOGD("initH264Context: got handle to host %lld", mHostHandle);
}


void MediaH264Decoder::resetH264Context(unsigned int width,
                                       unsigned int height,
                                       unsigned int outWidth,
                                       unsigned int outHeight,
                                       PixelFormat pixFmt) {
    auto transport = GoldfishMediaTransport::getInstance();
    if (!mHasAddressSpaceMemory) {
        ALOGE("%s no address space memory", __func__);
        return;
    }
    transport->writeParam((uint64_t)mHostHandle, 0, mAddressOffSet);
    transport->writeParam(width, 1, mAddressOffSet);
    transport->writeParam(height, 2, mAddressOffSet);
    transport->writeParam(outWidth, 3, mAddressOffSet);
    transport->writeParam(outHeight, 4, mAddressOffSet);
    transport->writeParam(static_cast<uint64_t>(pixFmt), 5, mAddressOffSet);
    transport->sendOperation(MediaCodecType::H264Codec,
                             MediaOperation::Reset, mAddressOffSet);
    ALOGD("resetH264Context: done");
}


void MediaH264Decoder::destroyH264Context() {

    ALOGD("return memory lot %d addrr %x", (int)(mAddressOffSet >> 23), mAddressOffSet);
    auto transport = GoldfishMediaTransport::getInstance();
    transport->writeParam((uint64_t)mHostHandle, 0, mAddressOffSet);
    transport->sendOperation(MediaCodecType::H264Codec,
                             MediaOperation::DestroyContext, mAddressOffSet);
    transport->returnMemorySlot(mAddressOffSet >> 23);
    mHasAddressSpaceMemory = false;
}

h264_result_t MediaH264Decoder::decodeFrame(uint8_t* img, size_t szBytes, uint64_t pts) {
    ALOGD("decode frame: use handle to host %lld", mHostHandle);
    h264_result_t res = {0, 0};
    if (!mHasAddressSpaceMemory) {
        ALOGE("%s no address space memory", __func__);
        return res;
    }
    auto transport = GoldfishMediaTransport::getInstance();
    uint8_t* hostSrc = transport->getInputAddr(mAddressOffSet);
    if (img != nullptr && szBytes > 0) {
        memcpy(hostSrc, img, szBytes);
    }
    transport->writeParam((uint64_t)mHostHandle, 0, mAddressOffSet);
    transport->writeParam(transport->offsetOf((uint64_t)(hostSrc)) - mAddressOffSet, 1, mAddressOffSet);
    transport->writeParam((uint64_t)szBytes, 2, mAddressOffSet);
    transport->writeParam((uint64_t)pts, 3, mAddressOffSet);
    transport->sendOperation(MediaCodecType::H264Codec,
                             MediaOperation::DecodeImage, mAddressOffSet);


    auto* retptr = transport->getReturnAddr(mAddressOffSet);
    res.bytesProcessed = *(uint64_t*)(retptr);
    res.ret = *(int*)(retptr + 8);

    return res;
}

void MediaH264Decoder::flush() {
    if (!mHasAddressSpaceMemory) {
        ALOGE("%s no address space memory", __func__);
        return;
    }
    ALOGD("flush: use handle to host %lld", mHostHandle);
    auto transport = GoldfishMediaTransport::getInstance();
    transport->writeParam((uint64_t)mHostHandle, 0, mAddressOffSet);
    transport->sendOperation(MediaCodecType::H264Codec,
                             MediaOperation::Flush, mAddressOffSet);
}

h264_image_t MediaH264Decoder::getImage() {
    ALOGD("getImage: use handle to host %lld", mHostHandle);
    h264_image_t res { };
    if (!mHasAddressSpaceMemory) {
        ALOGE("%s no address space memory", __func__);
        return res;
    }
    auto transport = GoldfishMediaTransport::getInstance();
    uint8_t* dst = transport->getInputAddr(mAddressOffSet); // Note: reuse the same addr for input and output
    transport->writeParam((uint64_t)mHostHandle, 0, mAddressOffSet);
    transport->writeParam(transport->offsetOf((uint64_t)(dst)) - mAddressOffSet, 1, mAddressOffSet);
    transport->writeParam(-1, 2, mAddressOffSet);
    transport->sendOperation(MediaCodecType::H264Codec,
                             MediaOperation::GetImage, mAddressOffSet);
    auto* retptr = transport->getReturnAddr(mAddressOffSet);
    res.ret = *(int*)(retptr);
    if (res.ret >= 0) {
        res.data = dst;
        res.width = *(uint32_t*)(retptr + 8);
        res.height = *(uint32_t*)(retptr + 16);
        res.pts = *(uint32_t*)(retptr + 24);
        res.color_primaries = *(uint32_t*)(retptr + 32);
        res.color_range = *(uint32_t*)(retptr + 40);
        res.color_trc = *(uint32_t*)(retptr + 48);
        res.colorspace = *(uint32_t*)(retptr + 56);
    } else if (res.ret == (int)(Err::DecoderRestarted)) {
        res.width = *(uint32_t*)(retptr + 8);
        res.height = *(uint32_t*)(retptr + 16);
    }
    return res;
}


h264_image_t MediaH264Decoder::renderOnHostAndReturnImageMetadata(int hostColorBufferId) {
    ALOGD("%s: use handle to host %lld", __func__, mHostHandle);
    h264_image_t res { };
    if (hostColorBufferId < 0) {
      ALOGE("%s negative color buffer id %d", __func__, hostColorBufferId);
      return res;
    }
    ALOGD("%s send color buffer id %d", __func__, hostColorBufferId);
    if (!mHasAddressSpaceMemory) {
        ALOGE("%s no address space memory", __func__);
        return res;
    }
    auto transport = GoldfishMediaTransport::getInstance();
    uint8_t* dst = transport->getInputAddr(mAddressOffSet); // Note: reuse the same addr for input and output
    transport->writeParam((uint64_t)mHostHandle, 0, mAddressOffSet);
    transport->writeParam(transport->offsetOf((uint64_t)(dst)) - mAddressOffSet, 1, mAddressOffSet);
    transport->writeParam((uint64_t)hostColorBufferId, 2, mAddressOffSet);
    transport->sendOperation(MediaCodecType::H264Codec,
                             MediaOperation::GetImage, mAddressOffSet);
    auto* retptr = transport->getReturnAddr(mAddressOffSet);
    res.ret = *(int*)(retptr);
    if (res.ret >= 0) {
        res.data = dst; // note: the data could be junk
        res.width = *(uint32_t*)(retptr + 8);
        res.height = *(uint32_t*)(retptr + 16);
        res.pts = *(uint32_t*)(retptr + 24);
        res.color_primaries = *(uint32_t*)(retptr + 32);
        res.color_range = *(uint32_t*)(retptr + 40);
        res.color_trc = *(uint32_t*)(retptr + 48);
        res.colorspace = *(uint32_t*)(retptr + 56);
    } else if (res.ret == (int)(Err::DecoderRestarted)) {
        res.width = *(uint32_t*)(retptr + 8);
        res.height = *(uint32_t*)(retptr + 16);
    }
    return res;
}
