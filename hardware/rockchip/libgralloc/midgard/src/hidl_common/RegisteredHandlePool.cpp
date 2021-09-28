/*
 * Copyright (C) 2020 ARM Limited. All rights reserved.
 *
 * Copyright 2016 The Android Open Source Project
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

#include "RegisteredHandlePool.h"

bool RegisteredHandlePool::add(buffer_handle_t bufferHandle)
{
    std::lock_guard<std::mutex> lock(mutex);
    return bufPool.insert(bufferHandle).second;
}

native_handle_t* RegisteredHandlePool::remove(void* buffer)
{
    auto bufferHandle = static_cast<native_handle_t*>(buffer);

    std::lock_guard<std::mutex> lock(mutex);
    return bufPool.erase(bufferHandle) == 1 ? bufferHandle : nullptr;
}

buffer_handle_t RegisteredHandlePool::get(const void* buffer)
{
    auto bufferHandle = static_cast<buffer_handle_t>(buffer);

    std::lock_guard<std::mutex> lock(mutex);
    return bufPool.count(bufferHandle) == 1 ? bufferHandle : nullptr;
}

void RegisteredHandlePool::for_each(std::function<void(const buffer_handle_t &)> fn)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::for_each(bufPool.begin(), bufPool.end(), fn);
}