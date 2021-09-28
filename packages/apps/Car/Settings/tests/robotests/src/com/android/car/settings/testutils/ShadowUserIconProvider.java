/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.car.settings.testutils;

import android.content.Context;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.UserManager;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import com.android.car.settings.users.UserIconProvider;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

@Implements(UserIconProvider.class)
public class ShadowUserIconProvider {
    @Implementation
    protected RoundedBitmapDrawable getRoundedUserIcon(UserInfo userInfo, Context context) {
        return null;
    }

    @Implementation
    protected RoundedBitmapDrawable getRoundedGuestDefaultIcon(Resources resources) {
        return null;
    }

    @Implementation
    protected Bitmap assignDefaultIcon(
            UserManager userManager, Resources resources, UserInfo userInfo) {
        return null;
    }
}
