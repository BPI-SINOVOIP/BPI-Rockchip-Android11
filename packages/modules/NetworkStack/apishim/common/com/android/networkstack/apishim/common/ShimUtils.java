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

package com.android.networkstack.apishim.common;

import android.os.Build;

/**
 * Utility class for API shims.
 */
public final class ShimUtils {
    /**
     * Check whether the device release or development API level is strictly higher than the passed
     * in level.
     *
     * On a development build (codename != REL), the device will have the same API level as the
     * last stable release, even though some additional APIs may be available. In this method the
     * device API level is considered to be higher if the device supports a stable SDK with a higher
     * version number, or if the device supports a development version of a SDK that has a higher
     * version number.
     *
     * @return True if the device supports an SDK that has or will have a higher version number,
     *         even if still in development.
     */
    public static boolean isReleaseOrDevelopmentApiAbove(int apiLevel) {
        // In-development API n+1 will have SDK_INT == n and CODENAME != REL.
        // Stable API n has SDK_INT == n and CODENAME == REL.
        final int devApiLevel = Build.VERSION.SDK_INT
                + ("REL".equals(Build.VERSION.CODENAME) ? 0 : 1);
        return devApiLevel > apiLevel;
    }

    /**
     * Check whether the device supports in-development or final R networking APIs.
     */
    public static boolean isAtLeastR() {
        return isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q);
    }
}
