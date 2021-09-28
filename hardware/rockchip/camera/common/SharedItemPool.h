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
#ifndef CAMERA3_HAL_SHAREDITEMPOOL_H_
#define CAMERA3_HAL_SHAREDITEMPOOL_H_

#include <vector>
#include <pthread.h>
#include "LogHelper.h"
#include "CommonUtilMacros.h"
/**
 * RnD define to track the acquisition and release of items in the pool
 * This is potentially very verbose.
 * If you want to use this define this tag in the cpp file that has the pools
 * that you want to debug. Before the include that imports this header.
 *
 * DO NOT just define it here!!
 */
#ifdef POOL_DEBUG
#define LOGP LOGE
#else
#define LOGP(...)
#endif

/**
 * \class SharediItemPool
 *
 * Pool of ref counted items. This class creates a pool of items and manages
 * the acquisition of them. When all references to this item have disappeared
 * The item is returned to the pool.
 *
 * This class is thread safe, i.e. it can be called from  multiple threads.
 * When the element is recycled to the pool it can be reset via a client
 * provided method.
 *
 */

NAMESPACE_DECLARATION {
template <class ItemType>
class SharedItemPool {
public:
    SharedItemPool(const char *name = "Unnamed");

    ~SharedItemPool();

    /**
     * Initializes the capacity of the pool. It allocates the objects.
     * optionally it will take function to reset the item before recycling
     * it to the pool.
     * This method is thread safe.
     *
     * \param capacity[IN]: Number of items the pool will hold
     * \param resetter[IN]: Function to reset the item before recycling to the
     *                      pool.
     * \return INVALID_OPERATION: if trying to initialize twice.
     * \return NO_INIT: if mutex initialization failed.
     * \return NO_MEMORY: If it could not allocate all the items in the pool.
     * \return OK: If everything went ok.
     */
    status_t init(int32_t capacity,  void (*resetter)(ItemType*) = nullptr);

    bool isFull();

    /**
     * Free the resources of the pool
     *
     * \return OK: if everything went ok.
     * \return UNKNOWN_ERROR: if there was a problem acquiring the lock.
     * \return other value coming from mutex destruction.
     */
    status_t deInit();

    /**
     * Acquire an item from the pool.
     * This method is thread safe. Access to the internal acquire/release
     * methods are protected.
     * BUT the thread-safety for the utilization of the item after it has been
     * acquired is the user's responsibility.
     * Be careful not to provide the same item to multiple threads that write
     * into it.
     *
     * \param item[OUT] shared pointer to an item.
     * \return OK
     * \return UNKNOWN_ERROR: issues with the mutex lock
     * \return INVALID_OPERATION: Pool is empty, you cannot acquire.
     */
    status_t acquireItem(std::shared_ptr<ItemType> &item);

    /**
     * Returns the number of currently available items
     * It there would be issues acquiring the lock the method returns 0
     * available items.
     *
     * \return item count
     */
    size_t availableItems();

private:
    status_t _releaseItem(ItemType *item);

    class ItemDeleter
    {
    public:
        ItemDeleter(SharedItemPool *pool) : mPool(pool) {};
        void operator()(ItemType *item) const {
            mPool->_releaseItem(item);
        }
    private:
        SharedItemPool* mPool;
    };
private: /* members */
    std::vector<ItemType*>   mAvailable; /* SharedItemPool doesn't have ownership */
    ItemType           *mAllocated;
    size_t              mCapacity;
    ItemDeleter         mDeleter;
    pthread_mutex_t     mMutex; /* protects mAvailable, mAllocated, mCapacity, mTraceReturns */
    bool                mTraceReturns;
    const char         *mName;
    void (*mResetter)(ItemType*);
};

} NAMESPACE_DECLARATION_END
#include "SharedItemPool.cpp"

#endif /* CAMERA3_HAL_SHAREDITEMPOOL_H_ */
