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

package com.android.car.settings.testutils;

import android.content.Context;
import android.content.pm.ServiceInfo;
import android.service.autofill.AutofillServiceInfo;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

@Implements(AutofillServiceInfo.class)
public class ShadowAutofillServiceInfo {

    private static String sSettingsActivity;

    public void __constructor__(Context context, ServiceInfo si) {
        // Do nothing when constructed in code.
    }

    @Resetter
    public static void reset() {
        sSettingsActivity = null;
    }

    @Implementation
    protected String getSettingsActivity() {
        return sSettingsActivity;
    }

    public static void setSettingsActivity(String settingsActivity) {
        sSettingsActivity = settingsActivity;
    }
}
