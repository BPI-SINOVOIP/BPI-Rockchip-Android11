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
package com.android.car.notification.testutils;

import android.app.ActivityManager;
import android.os.UserHandle;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

@Implements(value = ActivityManager.class)
public class ShadowActivityManager extends org.robolectric.shadows.ShadowActivityManager {
    private static final int DEFAULT_CURRENT_USER_ID = UserHandle.USER_SYSTEM;
    private static int sCurrentUserId = DEFAULT_CURRENT_USER_ID;

    @Resetter
    public static void reset() {
        sCurrentUserId = DEFAULT_CURRENT_USER_ID;
    }

    @Implementation
    protected static int getCurrentUser() {
        return sCurrentUserId;
    }

    public static void setCurrentUser(int userId) {
        sCurrentUserId = userId;
    }
}
