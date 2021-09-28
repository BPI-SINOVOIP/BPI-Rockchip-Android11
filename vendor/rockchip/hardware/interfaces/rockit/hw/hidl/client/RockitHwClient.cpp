/*
 * Copyright (C) 2019 Rockchip Electronics Co. LTD
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

 //#define LOG_NDEBUG 0
#define LOG_TAG "RockitHwClient"

#include "include/RockitHwClient.h"

#include <utils/Log.h>
#include <dlfcn.h>
#include <fcntl.h>

#include <media/Metadata.h>

#include <rockchip/hardware/rockit/hw/1.0/types.h>
#include <rockchip/hardware/rockit/hw/1.0/IRockitHwService.h>
#include <rockchip/hardware/rockit/hw/1.0/IRockitHwInterface.h>


using namespace ::rockchip::hardware::rockit::hw::V1_0;

namespace android
{

typedef struct _RockitHwCtx {
     sp<IRockitHwInterface> mService;
} RockitHwCtx;

static int status2Int(Status status) {
    switch (status) {
    case Status::OK:
        return 0;
    default:
        return -1;
    }
}

RockitHwClient::RockitHwClient() {
    RockitHwCtx* ctx = (RockitHwCtx *)malloc(sizeof(RockitHwCtx));
    memset(ctx, 0, sizeof(RockitHwCtx));
    mCtx = (void *)ctx;

    sp<IRockitHwService> service = IRockitHwService::getService();
    if (service.get() != NULL) {
        Status fnStatus;
        service->create(
                [&fnStatus, ctx](
                        Status status, const sp<IRockitHwInterface>& hwInterface) {
                    fnStatus = status;
                    ctx->mService = hwInterface;
                });
    } else {
        ALOGD("%s service = NULL", __FUNCTION__);
    }
}

RockitHwClient::~RockitHwClient() {
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    sp<IRockitHwService> service = IRockitHwService::getService();
    if (service.get() != NULL && ctx) {
        service->destroy(ctx->mService);
    }

    if (ctx) {
        if (ctx->mService.get() != NULL)
             ctx->mService.clear();
        ctx->mService = NULL;
        free(ctx);
        ctx = NULL;
    }

    mCtx = NULL;
}

int RockitHwClient::init(int type, void* param) {
    RTHWParamPairs* pairs = (RTHWParamPairs*)param;
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    RockitHWParamPairs hw;
    Status status = Status::NO_INIT;
    if (pairs != NULL && ctx != NULL && ctx->mService.get() != NULL) {
        RTHWParam2RockitHWParam(hw, pairs);
        status = ctx->mService->init((RockitHWType)type, hw);
    }
    return status2Int(status);
}

int RockitHwClient::enqueue(RTHWBuffer& buffer) {
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    RockitHWBuffer hwBuffer;
    Status status = Status::NO_INIT;
    if (ctx != NULL && ctx->mService.get() != NULL) {
        RTBuffer2HwBuffer(buffer, hwBuffer);
        status = ctx->mService->enqueue(hwBuffer);
    }
    return status2Int(status);
}

int RockitHwClient::dequeue(RTHWBuffer* buffer) {
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    RockitHWBuffer hwBuffer;
    Status status = Status::NO_INIT;
    if (buffer != NULL) {
        initRTHWBuffer(buffer);
        if (ctx != NULL && ctx->mService.get() != NULL) {
            ctx->mService->dequeue(
                    [&status, buffer](
                            Status status1, const RockitHWBuffer& hw) {
                        status = status1;
                        if (status == Status::OK) {
                            HwBuffer2RTBuffer(hw, (RTHWBuffer*)buffer);
                        }
                    });
        }
    }

    return status2Int(status);
}

int RockitHwClient::commitBuffer(RTHWBuffer& buffer) {
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    RockitHWBuffer hwBuffer;
    Status status = Status::NO_INIT;
    if (ctx != NULL && ctx->mService.get() != NULL) {
        RTBuffer2HwBuffer(buffer, hwBuffer);
        status = ctx->mService->commitBuffer(hwBuffer);
    }

    return status2Int(status);
}

int RockitHwClient::giveBackBuffer(RTHWBuffer& buffer) {
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    RockitHWBuffer hwBuffer;
    Status status = Status::NO_INIT;
    if (ctx != NULL && ctx->mService.get() != NULL) {
        RTBuffer2HwBuffer(buffer, hwBuffer);
        status = ctx->mService->giveBackBuffer(hwBuffer);
    }

    return status2Int(status);
}

int RockitHwClient::process(RockitHWBufferList& list) {
    (void)list;
    return 0;
}

int RockitHwClient::control(uint32_t cmd, void* param) {
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    Status status = Status::NO_INIT;
    if (ctx != NULL && ctx->mService.get() != NULL) {
        RockitHWParamPairs hw;
        RTHWParamPairs* rt = (RTHWParamPairs*)param;
        RTHWParam2RockitHWParam(hw, rt);
        status = ctx->mService->control(cmd, hw);
    }
    return status2Int(status);
}

int RockitHwClient::query(uint32_t cmd, void* param) {
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    Status status = Status::NO_INIT;
    if (ctx != NULL && ctx->mService.get() != NULL) {
        RockitHWParamPairs hw;
        RTHWParamPairs* rt = (RTHWParamPairs*)param;
        RTHWParam2RockitHWParam(hw, rt);
        ctx->mService->query(cmd,
                    [&status, rt](
                            Status status1, const RockitHWParamPairs& hw) {
                        status = status1;
                        if (status == Status::OK) {
                            RockitHWParam2RTHWParam(hw, *rt);
                        }
                    });
    }
    return status2Int(status);
}

int RockitHwClient::flush() {
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    Status status = Status::NO_INIT;
    if (ctx != NULL && ctx->mService.get() != NULL) {
        status = ctx->mService->flush();
    }

    return status2Int(status);
}

int RockitHwClient::reset() {
    RockitHwCtx* ctx = (RockitHwCtx *)mCtx;
    Status status = Status::NO_INIT;
    if (ctx != NULL && ctx->mService.get() != NULL) {
        status = ctx->mService->reset();
    }

    return status2Int(status);
}

void RockitHwClient::initRTHWBuffer(RTHWBuffer* rt) {
    if (rt) {
        rt->bufferId = 0;
        rt->length = 0;
        rt->size = 0;
        rt->pair.counter = 0;
    }
}

void RockitHwClient::RTBuffer2HwBuffer(RTHWBuffer& rt, RockitHWBuffer& hw) {
    hw.bufferType = rt.bufferType;
    hw.bufferId = rt.bufferId;
    hw.size = rt.size;
    hw.length = rt.length;

    RTHWParam2RockitHWParam(hw.pair, &rt.pair);
}

void RockitHwClient::HwBuffer2RTBuffer (const RockitHWBuffer& hw, RTHWBuffer* rt) {
    if (rt != NULL) {
        rt->bufferType = hw.bufferType;
        rt->bufferId = hw.bufferId;
        rt->size = hw.size;
        rt->length = hw.length;

        RockitHWParam2RTHWParam(hw.pair, rt->pair);
    }
}

void RockitHwClient::copyRockitHWParam(const RockitHWBuffer& src, RockitHWBuffer& dst) {
    dst.bufferType = src.bufferType;
    dst.bufferId = src.bufferId;
    dst.size = src.size;
    dst.length = src.length;
    dst.pair.counter = dst.pair.counter;

    if (dst.pair.counter > 0) {
        dst.pair.pairs.resize(dst.pair.counter);
        for (int i = 0; i < dst.pair.counter; i++) {
            dst.pair.pairs[i].key = src.pair.pairs[i].key;
            dst.pair.pairs[i].value = src.pair.pairs[i].value;
        }
    }
}

void RockitHwClient::RTHWParam2RockitHWParam(RockitHWParamPairs& hw, RTHWParamPairs* rt) {
    if (rt == NULL) {
        hw.counter = 0;
        hw.pairs.resize(1);
        // set default value
        hw.pairs[0].key = 0xffffffff;
        hw.pairs[0].value = 1;
    } else {
        hw.counter = rt->counter;
        hw.pairs.resize(hw.counter);
        for (int i = 0; i < hw.counter; i++) {
            hw.pairs[i].key = rt->pairs[i].key;
            hw.pairs[i].value = rt->pairs[i].value;
        }
    }
}

void RockitHwClient::RockitHWParam2RTHWParam(const RockitHWParamPairs& hw, RTHWParamPairs& rt) {
    rt.counter = 0;
    if (hw.counter > 0) {
        if(rt.pairs == NULL) {
            rt.pairs = (RTHWParamPair*)malloc(sizeof(RTHWParamPair)*rt.counter);
            ALOGD("RockitHWParam2RTHWParam: malloc buffer, need to free");
        }

        if (rt.pairs != NULL) {
            rt.counter = hw.counter;
            for (int i = 0; i < hw.counter; i++) {
                rt.pairs[i].key = hw.pairs[i].key;
                rt.pairs[i].value = hw.pairs[i].value;
            }
        }
    }
}





}  // namespace android
