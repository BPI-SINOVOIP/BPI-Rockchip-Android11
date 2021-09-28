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
package com.android.cts.appbinding.app;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.util.Log;

import org.junit.Test;

import androidx.test.InstrumentationRegistry;

/**
 * Called from the host side test to enable / disable the .MyService service.
 *
 * We need it because "pm disable/enable" won't work on an unrooted device.
 */
public class MyEnabler {
    private static final String TAG = "CtsAppBindingHostTestCases";
    @Test
    public void enableService() {
        final Context context = InstrumentationRegistry.getContext();

        final ComponentName cn = new ComponentName(context,
                "com.android.cts.appbinding.app.MyService");
        context.getPackageManager().setComponentEnabledSetting(cn,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);

        Log.w(TAG, "Enabled " + cn);
    }

    @Test
    public void disableService() {
        final Context context = InstrumentationRegistry.getContext();

        final ComponentName cn = new ComponentName(context,
                "com.android.cts.appbinding.app.MyService");
        context.getPackageManager().setComponentEnabledSetting(cn,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);

        Log.w(TAG, "Disabled " + cn);
    }
}
