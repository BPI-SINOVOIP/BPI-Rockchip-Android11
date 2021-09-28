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

package com.android.car.am;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

import com.android.car.R;

/**
 * Activity to block top activity after suspend to RAM in case of guest user.
 *
 * <p> Guest user resuming will cause user switch to another guest user.
 * For better user experience, no screen from previous user should be displayed.
 */

public class ContinuousBlankActivity extends Activity {
    private static final String TAG = "CAR.BLANK";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_continuous_blank);
        Log.i(TAG, "ContinuousBlankActivity created:");
    }
}
