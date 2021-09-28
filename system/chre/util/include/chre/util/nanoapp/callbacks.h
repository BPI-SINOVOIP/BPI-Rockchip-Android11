/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef CHRE_UTIL_CALLBACKS_H_
#define CHRE_UTIL_CALLBACKS_H_

#include <cstddef>

namespace chre {

/**
 * Implementation of a chreMessageFreeFunction that frees the given message
 * using the appropriate free function depending on whether this is included by
 * a nanoapp or the CHRE framework. This should be used when no other work needs
 * to be done after a nanoapp sends a message to the host.
 *
 * @see chreMessageFreeFunction
 */
void heapFreeMessageCallback(void *message, size_t messageSize);

}  // namespace chre

#endif  // CHRE_UTIL_CALLBACKS_H_
