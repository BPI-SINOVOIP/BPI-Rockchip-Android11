/*
 * Copyright 2019 The Android Open Source Project
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

#ifndef NO_BINDER
#include <binder/IPCThreadState.h>
#include <binderthreadstate/CallerUtils.h>
#endif // NO_BINDER
#include <hwbinder/IPCThreadState.h>
#include <private/gui/BufferQueueThreadState.h>
#include <unistd.h>

namespace android {

uid_t BufferQueueThreadState::getCallingUid() {
#ifndef NO_BINDER
    if (getCurrentServingCall() == BinderCallType::HWBINDER) {
        return hardware::IPCThreadState::self()->getCallingUid();
    }
    return IPCThreadState::self()->getCallingUid();
#else // NO_BINDER
    return hardware::IPCThreadState::self()->getCallingUid();
#endif // NO_BINDER
}

pid_t BufferQueueThreadState::getCallingPid() {
#ifndef NO_BINDER
    if (getCurrentServingCall() == BinderCallType::HWBINDER) {
        return hardware::IPCThreadState::self()->getCallingPid();
    }
    return IPCThreadState::self()->getCallingPid();
#else // NO_BINDER
    return hardware::IPCThreadState::self()->getCallingPid();
#endif // NO_BINDER
}

} // namespace android
