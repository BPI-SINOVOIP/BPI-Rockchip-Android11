/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.car.userlib;

import android.annotation.IntDef;
import android.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Callback used on async methods.
 *
 * @param <R> response type.
 */
public interface HalCallback<R> {

    int STATUS_INVALID = -1; // Used for logging purposes only
    int STATUS_OK = 1;
    int STATUS_HAL_SET_TIMEOUT = 2;
    int STATUS_HAL_RESPONSE_TIMEOUT = 3;
    int STATUS_WRONG_HAL_RESPONSE = 4;
    int STATUS_CONCURRENT_OPERATION = 5;
    int STATUS_HAL_NOT_SUPPORTED = 6;

    /** @hide */
    @IntDef(prefix = { "STATUS_" }, value = {
            STATUS_OK,
            STATUS_HAL_SET_TIMEOUT,
            STATUS_HAL_RESPONSE_TIMEOUT,
            STATUS_WRONG_HAL_RESPONSE,
            STATUS_CONCURRENT_OPERATION,
            STATUS_HAL_NOT_SUPPORTED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface HalCallbackStatus{}

    /**
     * Called when the HAL generated an event responding to that callback (or when an error
     * occurred).
     *
     * @param status status of the request.
     * @param response HAL response (or {@code null} in case of error).
     */
    void onResponse(@HalCallbackStatus int status, @Nullable R response);
}
