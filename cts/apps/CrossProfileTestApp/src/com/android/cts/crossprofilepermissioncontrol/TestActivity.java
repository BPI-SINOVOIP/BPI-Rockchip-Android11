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

package com.android.cts.crossprofilepermissioncontrol;

import android.app.Activity;
import android.app.AppOpsManager;
import android.content.pm.CrossProfileApps;
import android.os.Bundle;
import android.os.Process;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

/**
 * Test activity to check if the app can interact across profiles.
 */
public class TestActivity extends Activity {
    private static final String INTERACT_ACROSS_PROFILES =
            "android.permission.INTERACT_ACROSS_PROFILES";
    private AppOpsManager mAppOpsManager;
    private CrossProfileApps mCrossProfileApps;
    private TextView mTextView;
    private Button mSettingsButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        mTextView = findViewById(R.id.cross_profile_app_text);
        mSettingsButton = findViewById(R.id.cross_profile_settings);

        mAppOpsManager = getSystemService(AppOpsManager.class);
        mCrossProfileApps = getSystemService(CrossProfileApps.class);

        mSettingsButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startActivity(mCrossProfileApps.createRequestInteractAcrossProfilesIntent());
            }
        });
    }

    @Override
    protected void onStart() {
        super.onStart();
        updateText();
    }

    private void updateText() {
        final String op = AppOpsManager.permissionToOp(INTERACT_ACROSS_PROFILES);
        if (AppOpsManager.MODE_ALLOWED == mAppOpsManager.checkOpNoThrow(
                op, Process.myUid(), getPackageName())) {
            mTextView.setText(R.string.cross_profile_enabled);
        } else {
            mTextView.setText(R.string.cross_profile_disabled);
        }
    }
}
