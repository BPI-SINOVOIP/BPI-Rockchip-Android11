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

#ifndef _UI_INPUT_INPUTDISPATCHER_INPUTDISPATCHERFACTORY_H
#define _UI_INPUT_INPUTDISPATCHER_INPUTDISPATCHERFACTORY_H

#include <utils/RefBase.h>

#include "InputDispatcherInterface.h"
#include "InputDispatcherPolicyInterface.h"

namespace android {

// This factory method is used to encapsulate implementation details in internal header files.
sp<InputDispatcherInterface> createInputDispatcher(
        const sp<InputDispatcherPolicyInterface>& policy);

} // namespace android

#endif // _UI_INPUT_INPUTDISPATCHER_INPUTDISPATCHERFACTORY_H
