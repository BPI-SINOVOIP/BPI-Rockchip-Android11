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
 * limitations under the License.
 */
package com.android.car.developeroptions.applications.specialaccess;

import static com.android.car.developeroptions.Utils.isSystemAlertWindowEnabled;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;

import com.android.car.developeroptions.core.BasePreferenceController;

public class SystemAlertWindowPreferenceController extends BasePreferenceController {
    public SystemAlertWindowPreferenceController(Context context, String preferenceKey) {
        super(context, preferenceKey);
    }

    @Override
    public int getAvailabilityStatus() {
        return isSystemAlertWindowEnabled(mContext)
                ? AVAILABLE_UNSEARCHABLE : UNSUPPORTED_ON_DEVICE ;
    }
}
