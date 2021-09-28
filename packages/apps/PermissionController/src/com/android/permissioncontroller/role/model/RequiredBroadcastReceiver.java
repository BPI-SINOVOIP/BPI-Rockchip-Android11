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

package com.android.permissioncontroller.role.model;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.UserHandle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.permissioncontroller.role.utils.UserUtils;

import java.util.List;

/**
 * Specifies a required {@code BroadcastReceiver} for an application to qualify for a {@link Role}.
 */
public class RequiredBroadcastReceiver extends RequiredComponent {

    public RequiredBroadcastReceiver(@NonNull IntentFilterData intentFilterData,
            @Nullable String permission) {
        super(intentFilterData, permission);
    }

    @NonNull
    @Override
    protected List<ResolveInfo> queryIntentComponentsAsUser(@NonNull Intent intent, int flags,
            @NonNull UserHandle user, @NonNull Context context) {
        Context userContext = UserUtils.getUserContext(context, user);
        PackageManager userPackageManager = userContext.getPackageManager();
        return userPackageManager.queryBroadcastReceivers(intent, flags);
    }

    @NonNull
    @Override
    protected ComponentName getComponentComponentName(@NonNull ResolveInfo resolveInfo) {
        return new ComponentName(resolveInfo.activityInfo.packageName,
                resolveInfo.activityInfo.name);
    }

    @Nullable
    @Override
    protected String getComponentPermission(@NonNull ResolveInfo resolveInfo) {
        return resolveInfo.activityInfo.permission;
    }
}
