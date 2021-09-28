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

package com.android.cts.crossprofileappstest;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.CrossProfileApps;
import android.os.Bundle;
import android.os.UserHandle;
import android.util.Log;

import androidx.annotation.Nullable;

import com.android.compatibility.common.util.ShellIdentityUtils;

/**
 * An activity that launches the {@link NonMainActivity} in a different user within the same task.
 *
 * <p>Logs its task ID with the following format:
 * "CrossProfileSameTaskLauncherActivity#taskId#[taskId]#", where [taskId] is the actual task ID,
 * such as NonMainActivity#taskId#4#.
 */
public class CrossProfileSameTaskLauncherActivity extends Activity {
    static final String TARGET_USER_EXTRA = "TARGET_USER";

    private static final String LOG_TAG = "CrossProfileSameTaskChe"; // 23 chars max

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final Intent intent = getIntent();
        if (!intent.hasExtra(TARGET_USER_EXTRA)) {
            throw new IllegalStateException(
                    "ActivityForwarder started without the extra: " + TARGET_USER_EXTRA);
        }
        setContentView(R.layout.cross_profile_same_task_launcher);
        Log.w(LOG_TAG, "CrossProfileSameTaskLauncherActivity#taskId#" + getTaskId() + "#");
        final UserHandle targetUser = intent.getParcelableExtra(TARGET_USER_EXTRA);
        final Intent nonMainActivityIntent = new Intent();
        nonMainActivityIntent.setComponent(NonMainActivity.getComponentName(this));
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(
                getSystemService(CrossProfileApps.class),
                crossProfileApps
                        -> crossProfileApps.startActivity(nonMainActivityIntent, targetUser, this));
    }

    static ComponentName getComponentName(Context context) {
        return new ComponentName(context, CrossProfileSameTaskLauncherActivity.class);
    }
}
