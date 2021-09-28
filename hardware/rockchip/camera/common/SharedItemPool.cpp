/*
 * Copyright (C) 2014-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#include "SharedItemPool.h"

NAMESPACE_DECLARATION {

template <class ItemType>
SharedItemPool<ItemType>::SharedItemPool(const char*name):
    mAllocated(nullptr),
    mCapacity(0),
    mDeleter(this),
    mTraceReturns(false),
    mName(name),
    mResetter(nullptr)
{
    CLEAR(mMutex);
}

template <class ItemType>
SharedItemPool<ItemType>::~SharedItemPool()
{
    deInit();
}

template <class ItemType>
status_t SharedItemPool<ItemType>::init(int32_t capacity,  void (*resetter)(ItemType*))
{
    int32_t status = OK;
    if (mCapacity != 0) {
        LOGE("trying to initialize pool twice ?");
        return INVALID_OPERATION;
    }
    status = pthread_mutex_init(&mMutex, nullptr);
    if (status != 0) {
        LOGE("Pool mutex failed to initialize [%d]",status);
        return NO_INIT;
    }
    pthread_mutex_lock(&mMutex);
    mResetter = resetter;
    mCapacity = capacity;
    mAvailable.reserve(capacity);
    mAllocated = new ItemType[capacity];

    for (int32_t i = 0; i < capacity; i++) {
        mAvailable.push_back(&mAllocated[i]);
    }
    LOGI("Shared pool %s init with %d items", mName, capacity);
    pthread_mutex_unlock(&mMutex);
    return OK;
}

template <class ItemType>
bool SharedItemPool<ItemType>::isFull()
{
    int32_t status = pthread_mutex_lock (&mMutex);
    if (CC_UNLIKELY(status != 0)) {
        LOGE("Failed to lock mutex on availableItems @%s pool[%d]",
                mName, status);
        return false;
    }
    bool ret = (mAvailable.size() == mCapacity);
    status = pthread_mutex_unlock(&mMutex);
    if (CC_UNLIKELY(status != 0)) {
        LOGE("Failed to unlock mutex on availableItems @%s pool[%d]",
                mName, status);
        return false;
    }
    return ret;
}

template <class ItemType>
status_t SharedItemPool<ItemType>::deInit()
{
    int32_t status = pthread_mutex_lock(&mMutex);
    if (mCapacity == 0) {
        LOGI("Shared pool %s isn't initialized or already de-initialized",
                mName);
        status = pthread_mutex_unlock(&mMutex);
        if (CC_UNLIKELY(status != 0)) {
            LOGE("Failed to unlock mutex on acquire item @%s pool[%d]",
                mName, status);
            return UNKNOWN_ERROR;
        }
        return OK;
    }
    if (CC_UNLIKELY(status != 0)) {
        LOGE("Failed to lock mutex on destroying @%s pool[%d]",
                mName, status);
        return UNKNOWN_ERROR;
    }
    if (mAvailable.size() != mCapacity) {
        LOGE("Not all items are returned when destroying pool %s (%zu/%zu)!",
                mName, mAvailable.size(), mCapacity);
    }
    delete [] mAllocated;
    mAllocated = nullptr;
    mAvailable.clear();
    mCapacity = 0;
    status = pthread_mutex_unlock(&mMutex);
    status |= pthread_mutex_destroy(&mMutex);
    LOGI("Shared pool %s deinit done. Status %d", mName, status);
    return status;
}

template <class ItemType>
status_t SharedItemPool<ItemType>::acquireItem(std::shared_ptr<ItemType> &item)
{
    item.reset();
    int32_t status = pthread_mutex_lock(&mMutex);
    if (CC_UNLIKELY(status != 0)) {
        LOGE("Failed to lock mutex on acquire item @%s pool[%d]",
                mName, status);
        return UNKNOWN_ERROR;
    }
    if (mAvailable.empty()) {
        pthread_mutex_unlock(&mMutex);
        return INVALID_OPERATION;
    }
    std::shared_ptr<ItemType> sh(mAvailable[0], mDeleter);
    mAvailable.erase(mAvailable.begin());
    item = sh;
    status = pthread_mutex_unlock(&mMutex);
    if (CC_UNLIKELY(status != 0)) {
        LOGE("Failed to unlock mutex on acquire item @%s pool[%d]",
                mName, status);
        return UNKNOWN_ERROR;
    }
    LOGP("shared pool %s acquire items %p", mName, sh.get());
    return OK;
}

template <class ItemType>
size_t SharedItemPool<ItemType>::availableItems()
{
    int32_t status = pthread_mutex_lock (&mMutex);
    if (CC_UNLIKELY(status != 0)) {
        LOGE("Failed to lock mutex on availableItems @%s pool[%d]",
                mName, status);
        return 0;
    }
    size_t ret = mAvailable.size();
    status = pthread_mutex_unlock(&mMutex);
    if (CC_UNLIKELY(status != 0)) {
        LOGE("Failed to unlock mutex on availableItems @%s pool[%d]",
                mName, status);
        return 0;
    }
    return ret;
}

template <class ItemType>
status_t SharedItemPool<ItemType>::_releaseItem(ItemType *item)
{
    int32_t status = pthread_mutex_lock(&mMutex);
    if (CC_UNLIKELY(status != 0)) {
        LOGE("Failed to lock mutex on release item @%s pool[%d]",
                mName, status);
        return UNKNOWN_ERROR;
    }

    if (mResetter)
        mResetter(item);

    LOGP("shared pool %s returning item %p", mName, item);
    if (mTraceReturns) {
    PRINT_BACKTRACE_LINUX();
    }

    mAvailable.push_back(item);
    status = pthread_mutex_unlock(&mMutex);
    if (CC_UNLIKELY(status != 0)) {
        LOGE("Failed to unlock mutex on release item @%s pool[%d]",
                mName, status);
        return UNKNOWN_ERROR;
    }
    return OK;
}

} NAMESPACE_DECLARATION_END
