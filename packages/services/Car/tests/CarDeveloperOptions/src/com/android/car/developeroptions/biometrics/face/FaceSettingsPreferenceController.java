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

package com.android.car.developeroptions.biometrics.face;

import android.content.Context;

import com.android.car.developeroptions.core.TogglePreferenceController;

/**
 * Abstract base class for all face settings toggles.
 */
public abstract class FaceSettingsPreferenceController extends TogglePreferenceController {

    private int mUserId;

    public FaceSettingsPreferenceController(Context context, String preferenceKey) {
        super(context, preferenceKey);
    }

    public void setUserId(int userId) {
        mUserId = userId;
    }

    protected int getUserId() {
        return mUserId;
    }
}
