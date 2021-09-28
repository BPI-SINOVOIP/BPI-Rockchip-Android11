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
#define LOG_TAG "RockitHwClientInterface"

#include "include/RockitHwClientInterface.h"
#include "include/RockitHwClient.h"


#include <utils/Log.h>
#include <dlfcn.h>
#include <fcntl.h>

#include <media/Metadata.h>

namespace android
{

RockitHwClientInterface::RockitHwClientInterface() {
    mClient = new RockitHwClient();
}

RockitHwClientInterface::~RockitHwClientInterface() {
    if (mClient) {
        mClient->reset();

        delete mClient;
        mClient = NULL;
    }
}

int RockitHwClientInterface::init(int type, void* param) {
    if (mClient) {
        return mClient->init(type, param);
    }
    return -1;
}

int RockitHwClientInterface::enqueue(RTHWBuffer& buffer) {
    if (mClient) {
        return mClient->enqueue(buffer);
    }
    return -1;
}

int RockitHwClientInterface::dequeue(RTHWBuffer* buffer) {
    if (mClient && buffer) {
        return mClient->dequeue(buffer);
    }
    return -1;
}

int RockitHwClientInterface::commitBuffer(RTHWBuffer& buffer) {
    if (mClient) {
        return mClient->commitBuffer(buffer);
    }
    return -1;
}

int RockitHwClientInterface::giveBackBuffer(RTHWBuffer& buffer) {
    if (mClient) {
        return mClient->giveBackBuffer(buffer);
    }
    return -1;
}

int RockitHwClientInterface::process(RockitHWBufferList& list) {
    if (mClient) {
        return mClient->process(list);
    }
    return -1;
}

int RockitHwClientInterface::control(uint32_t cmd, void* param) {
    if (mClient) {
        return mClient->control(cmd, param);
    }
    return -1;
}

int RockitHwClientInterface::query(uint32_t cmd, void* param) {
    if (mClient) {
        return mClient->query(cmd, param);
    }
    return -1;
}

int RockitHwClientInterface::flush() {
    if (mClient) {
        return mClient->flush();
    }
    return -1;
}

int RockitHwClientInterface::reset() {
    if (mClient) {
        return mClient->reset();
    }
    return -1;
}

}  // namespace android
