/*
 * Copyright (C) 2014-2017 Intel Corporation.
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
#ifndef CAMERA3_HAL_ITEMPOOL_H_
#define CAMERA3_HAL_ITEMPOOL_H_

#include <vector>
#include <mutex>
#include <utils/Errors.h>

/**
 * \class ItemPool
 *
 * Pool of items. This class creates a pool of items and manages the acquisition
 * and release of them. This class is thread safe, i.e. it can be called from
 * multiple threads.
 *
 *
 */

NAMESPACE_DECLARATION {
template <class ItemType>
class ItemPool {
public:
    ItemPool();
    virtual ~ItemPool();

    status_t init(size_t poolSize);
    void     deInit(void);
    status_t acquireItem(ItemType **item);
    status_t releaseItem(ItemType *item);

    int availableItems() { return mItemsInPool.size(); }
    unsigned int size() { return mPoolSize; }
    bool isEmpty() { return mItemsInPool.isEmpty(); }

private:
    // prevent copy constructor and assignment operator
    ItemPool(const ItemPool& other);
    ItemPool& operator=(const ItemPool& other);

private: /* members*/
    ItemType           *mAllocatedItems;
    unsigned int        mPoolSize;  /* This is the pool capacity */
    std::vector<ItemType*>   mItemsInPool; /* Stores the items of mAllocatedItems */
    std::mutex          mLock; /* protect mItemsInPool */
    bool                mInitialized;
};

template <class ItemType>
ItemPool<ItemType>::ItemPool():
    mAllocatedItems(nullptr),
    mPoolSize(0),
    mInitialized(false)
{
    mItemsInPool.clear();
}

template <class ItemType>
status_t
ItemPool<ItemType>::init(size_t poolSize)
{
    if (mInitialized) {
        LOGW("trying to initialize twice the pool");
        deInit();
    }
    mAllocatedItems = new ItemType[poolSize];

    mItemsInPool.reserve(poolSize);
    for (size_t i = 0; i < poolSize; i++)
        mItemsInPool.push_back(&mAllocatedItems[i]);

    mPoolSize = poolSize;
    mInitialized = true;
    return NO_ERROR;
}

template <class ItemType>
void
ItemPool<ItemType>::deInit()
{
    if (mAllocatedItems) {
        delete [] mAllocatedItems;
        mAllocatedItems = nullptr;
    }

    mItemsInPool.clear();
    mPoolSize = 0;
    mInitialized = false;
}

template <class ItemType>
ItemPool<ItemType>::~ItemPool()
{
    if (mAllocatedItems) {
        delete [] mAllocatedItems;
        mAllocatedItems = nullptr;
    }
    mItemsInPool.clear();
}

template <class ItemType>
status_t
ItemPool<ItemType>::acquireItem(ItemType **item)
{
    status_t status = NO_ERROR;
    std::lock_guard<std::mutex> l(mLock);
    LOGD("%s size is %zu", __FUNCTION__, mItemsInPool.size());
    if (item == nullptr) {
        LOGE("Invalid parameter to acquire item from the pool");
        return BAD_VALUE;
    }

    if (!mItemsInPool.empty()) {
        *item = mItemsInPool.front();
        mItemsInPool.erase(mItemsInPool.begin());
    } else {
        status = INVALID_OPERATION;
        LOGW("Pool is empty, cannot acquire item");
    }
    return status;
}

template <class ItemType>
status_t
ItemPool<ItemType>::releaseItem(ItemType *item)
{
    status_t status = NO_ERROR;
    std::lock_guard<std::mutex> l(mLock);

    if (item == nullptr)
        return BAD_VALUE;

    bool belongs = false;
    for (unsigned int i = 0; i <  mPoolSize; i++)
        if (item == &(mAllocatedItems[i])) {
            belongs = true;
            break;
        }

    if (mItemsInPool.size() == mPoolSize) {
        LOGW("Trying to release an Item (%p) when the pool is full", item);
        belongs = false;
    }

    if (belongs) {
        mItemsInPool.push_back(item);
    } else {
        status = BAD_VALUE;
        LOGW("Trying to release an Item (%p) that doesn't belong to this pool",
                item);
    }
    LOGD("%s size is %zu", __FUNCTION__, mItemsInPool.size());
    return status;
}
} NAMESPACE_DECLARATION_END
#endif /* CAMERA3_HAL_ITEMPOOL_H_ */
