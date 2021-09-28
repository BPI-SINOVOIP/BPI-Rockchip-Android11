/*
 * Copyright 2020 The Android Open Source Project
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

#include <private/android/choreographer.h>

using namespace android;

AChoreographer* AChoreographer_getInstance() {
    return AChoreographer_routeGetInstance();
}
void AChoreographer_postFrameCallback(AChoreographer* choreographer,
                                      AChoreographer_frameCallback callback, void* data) {
    return AChoreographer_routePostFrameCallback(choreographer, callback, data);
}
void AChoreographer_postFrameCallbackDelayed(AChoreographer* choreographer,
                                             AChoreographer_frameCallback callback, void* data,
                                             long delayMillis) {
    return AChoreographer_routePostFrameCallbackDelayed(choreographer, callback, data, delayMillis);
}
void AChoreographer_postFrameCallback64(AChoreographer* choreographer,
                                        AChoreographer_frameCallback64 callback, void* data) {
    return AChoreographer_routePostFrameCallback64(choreographer, callback, data);
}
void AChoreographer_postFrameCallbackDelayed64(AChoreographer* choreographer,
                                               AChoreographer_frameCallback64 callback, void* data,
                                               uint32_t delayMillis) {
    return AChoreographer_routePostFrameCallbackDelayed64(choreographer, callback, data,
                                                          delayMillis);
}
void AChoreographer_registerRefreshRateCallback(AChoreographer* choreographer,
                                                AChoreographer_refreshRateCallback callback,
                                                void* data) {
    return AChoreographer_routeRegisterRefreshRateCallback(choreographer, callback, data);
}
void AChoreographer_unregisterRefreshRateCallback(AChoreographer* choreographer,
                                                  AChoreographer_refreshRateCallback callback,
                                                  void* data) {
    return AChoreographer_routeUnregisterRefreshRateCallback(choreographer, callback, data);
}
