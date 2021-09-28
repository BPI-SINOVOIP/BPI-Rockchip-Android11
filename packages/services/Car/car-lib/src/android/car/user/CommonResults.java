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

package android.car.user;

/**
 * Defines status for common result objects.
 */
final class CommonResults {

    /**
     * Operation was is successful for both HAL and Android.
     */
    public static final int STATUS_SUCCESSFUL = 1;

    /**
     * Operation failed on Android.
     */
    public static final int STATUS_ANDROID_FAILURE = 2;

    /**
     * Operation failed on HAL.
     */
    public static final int STATUS_HAL_FAILURE = 3;

    /**
     * Operation failed due to an error communication with HAL (like timeout).
     */
    public static final int STATUS_HAL_INTERNAL_FAILURE = 4;

    /**
     * Operation failed due to invalid request.
     */
    public static final int STATUS_INVALID_REQUEST = 5;

    /**
     * Reference for common status - anything higher than this can be used for custom status
     */
    static final int LAST_COMMON_STATUS = 100;

    private CommonResults() {
        throw new UnsupportedOperationException();
    }
}
