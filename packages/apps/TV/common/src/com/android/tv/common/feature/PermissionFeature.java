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
 * limitations under the License
 */

package com.android.tv.common.feature;

import android.content.Context;
import android.content.pm.PackageManager;

/** A feature that is only available when {@code permissionName} is granted. */
public class PermissionFeature implements Feature {

    public static final PermissionFeature DVB_DEVICE_PERMISSION =
            new PermissionFeature("android.permission.DVB_DEVICE");

    private final String permissionName;

    private PermissionFeature(String permissionName) {
        this.permissionName = permissionName;
    }

    @Override
    public boolean isEnabled(Context context) {
        return context.checkSelfPermission(permissionName) == PackageManager.PERMISSION_GRANTED;
    }
}
