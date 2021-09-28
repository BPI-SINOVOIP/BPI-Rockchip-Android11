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
package android.platform.test.scenario.performancelaunch.hermeticapp;

import android.content.Context;
import android.content.Intent;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.Until;
import android.support.test.uiautomator.By;

import androidx.test.InstrumentationRegistry;

import org.junit.AfterClass;

/** Performance helper to open application and exits after. */
public class PerformanceBase {

    private static final String PACKAGE = "com.android.performanceLaunch";

    private static UiDevice mDevice;

    public PerformanceBase() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    public void open(String actvity) {

        // Launch the app
        Context context = InstrumentationRegistry.getContext();
        final Intent intent = context.getPackageManager().getLaunchIntentForPackage(PACKAGE);
        // Clear out any previous instances
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
        intent.setClassName(PACKAGE, actvity);
        context.startActivity(intent);

        mDevice.wait(Until.hasObject(By.pkg(PACKAGE).depth(0)), 5000);
    }

    @AfterClass
    public static void close() {
        mDevice.pressHome();
    }
}
