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
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.android.compatibility.common.util.ShellIdentityUtils;

/**
 * An activity that launches {@link CrossProfileResultReturnerActivity} for result, then displays
 * the string {@link #SUCCESS_MESSAGE} if successful.
 *
 * <p>Must be launched with intent extra {@link #TARGET_USER_EXTRA} with the numeric target user ID.
 */
public class CrossProfileResultCheckerActivity extends Activity {
    static final String SUCCESS_MESSAGE = "Successfully received cross-profile result.";
    static final String TARGET_USER_EXTRA = "TARGET_USER";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final Intent intent = getIntent();
        if (!intent.hasExtra(TARGET_USER_EXTRA)) {
            throw new IllegalStateException(
                    "CrossProfileResultCheckerActivity started without " + TARGET_USER_EXTRA);
        }
        setContentView(R.layout.cross_profile_result_checker);
        final Intent resultReturnerIntent =
                new Intent().setComponent(
                        CrossProfileResultReturnerActivity.buildComponentName(this));
        final UserHandle targetUser = intent.getParcelableExtra(TARGET_USER_EXTRA);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(
                getSystemService(CrossProfileApps.class),
                crossProfileApps -> crossProfileApps.startActivity(
                        resultReturnerIntent, targetUser, this));
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != CrossProfileResultReturnerActivity.RESULT_CODE) {
            throw new IllegalStateException("Unknown result code: " + resultCode);
        }
        final TextView textView = findViewById(R.id.cross_profile_result_checker_result);
        textView.setText(SUCCESS_MESSAGE);
    }

    static ComponentName buildComponentName(Context context) {
        return new ComponentName(context, CrossProfileResultCheckerActivity.class);
    }
}
