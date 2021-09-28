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

package com.android.compatibility.common.util;

import android.os.SystemProperties;

/**
 * Provides utilities to deal with user status.
 */
public final class UserUtils {

    private static final String TAG = UserUtils.class.getSimpleName();
    private static final String SYS_PROP_HEADLESS_SYSTEM_USER = "ro.fw.mu.headless_system_user";

    private UserUtils() {
        throw new AssertionError("Should not be instantiated");
    }

    /**
     * Tells if the device is in headless system user mode.
     */
    public static boolean isHeadlessSystemUserMode() {
        return SystemProperties.getBoolean(SYS_PROP_HEADLESS_SYSTEM_USER, false);
    }
}
