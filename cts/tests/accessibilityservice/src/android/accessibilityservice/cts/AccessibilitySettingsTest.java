/*
 * Copyright (C) 2010 The Android Open Source Project
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

package android.accessibilityservice.cts;

import static androidx.test.InstrumentationRegistry.getContext;

import static org.junit.Assert.assertTrue;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.Presubmit;
import android.provider.Settings;
import android.test.suitebuilder.annotation.MediumTest;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

/**
 * This test case is responsible to verify that the intent for launching
 * accessibility settings has an activity that handles it.
 */
@Presubmit
@RunWith(AndroidJUnit4.class)
public class AccessibilitySettingsTest {

    @Rule
    public final AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @MediumTest
    @AppModeFull
    @Test
    public void testAccessibilitySettingsIntentHandled() throws Throwable {
        PackageManager packageManager = getContext().getPackageManager();
        Intent intent = new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS);
        List<ResolveInfo> resolvedActivities = packageManager.queryIntentActivities(intent,
                PackageManager.MATCH_DEFAULT_ONLY);

        // make sure accessibility settings exist
        String message = "Accessibility settings activity must be launched via Intent " +
                "Settings.ACTION_ACCESSIBILITY_SETTINGS";
        assertTrue(message, !resolvedActivities.isEmpty());
    }
}

