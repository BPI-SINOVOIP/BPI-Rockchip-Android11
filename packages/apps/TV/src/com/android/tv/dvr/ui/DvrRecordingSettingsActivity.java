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

package com.android.tv.dvr.ui;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import androidx.leanback.app.GuidedStepFragment;
import com.android.tv.R;
import com.android.tv.Starter;

/** Activity to show details view in DVR. */
public class DvrRecordingSettingsActivity extends Activity {

    /**
     * Name of the boolean flag to decide if the setting fragment should be translucent. Type:
     * boolean
     */
    public static final String IS_WINDOW_TRANSLUCENT = "windows_translucent";
    /**
     * Name of the program added for recording.
     */
    public static final String PROGRAM = "program";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Starter.start(this);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_dvr_series_settings);

        if (savedInstanceState == null) {
            DvrRecordingSettingsFragment settingFragment = new DvrRecordingSettingsFragment();
            settingFragment.setArguments(getIntent().getExtras());
            GuidedStepFragment.addAsRoot(this, settingFragment, R.id.dvr_settings_view_frame);
        }
    }

    @Override
    public void onAttachedToWindow() {
        if (!getIntent().getExtras().getBoolean(IS_WINDOW_TRANSLUCENT, true)) {
            getWindow()
                    .setBackgroundDrawable(
                            new ColorDrawable(getColor(R.color.common_tv_background)));
        }
    }
}
