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

 #ifndef GRALLOC_COMMON_REGISTERED_HANDLE_POOL_H
 #define GRALLOC_COMMON_REGISTERED_HANDLE_POOL_H

#include <cutils/native_handle.h>
#include <mutex>
#include <unordered_set>
#include <algorithm>
#include <functional>

/* An unordered set to internally store / retrieve imported buffer handles */
class RegisteredHandlePool
{
public:
	/* Stores the buffer handle in the internal list */
	bool add(buffer_handle_t bufferHandle);

	/* Retrieves and removes the buffer handle from internal list */
	native_handle_t* remove(void* buffer);

	/* Retrieves the buffer handle from internal list */
	buffer_handle_t get(const void* buffer);

	/* Applies a function to each buffer handle */
	void for_each(std::function<void(const buffer_handle_t &)> fn);

private:
	std::mutex mutex;
	std::unordered_set<buffer_handle_t> bufPool;
};

#endif /* GRALLOC_COMMON_REGISTERED_HANDLE_POOL_H */
