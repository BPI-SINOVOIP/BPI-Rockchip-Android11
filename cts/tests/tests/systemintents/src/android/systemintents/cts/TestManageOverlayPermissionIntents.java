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

package android.systemintents.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.testng.Assert.assertTrue;
import static org.testng.Assert.expectThrows;

import android.app.Instrumentation;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.provider.Settings;
import android.support.test.uiautomator.UiDevice;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class TestManageOverlayPermissionIntents {
    private static final String PERMISSION_INTERNAL_SYSTEM_WINDOW =
            "android.permission.INTERNAL_SYSTEM_WINDOW";

    private Context mContext;
    private PackageManager mPackageManager;
    private UiDevice mUiDevice;

    @Before
    public void setUp() throws Exception {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = instrumentation.getContext();
        mPackageManager = mContext.getPackageManager();
        mUiDevice = UiDevice.getInstance(instrumentation);
    }

    @After
    public void tearDown() throws Exception {
        mUiDevice.pressHome();
    }

    @Test
    public void testStartManageAppOverlayPermissionIntent_whenCallerHasPermission_succeedsOrThrowsActivityNotFound() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_APP_OVERLAY_PERMISSION);
        intent.setData(Uri.fromParts("package", mContext.getPackageName(), null));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        SystemUtil.runWithShellPermissionIdentity(() -> {
            assertEquals("Shell must have INTERNAL_SYSTEM_WINDOW permission to run test",
                    PackageManager.PERMISSION_GRANTED,
                    mContext.checkSelfPermission(PERMISSION_INTERNAL_SYSTEM_WINDOW));

            try {
                mContext.startActivity(intent);
            } catch (ActivityNotFoundException e) {
                // Fall through - it's allowed to not handle the intent
            }
        });

        // ActivityNotFoundException or no exception thrown
    }

    @Test
    public void testStartManageAppOverlayPermissionIntent_whenCallerDoesNotHavePermission_throwsSecurityExceptionOrActivityNotFound() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_APP_OVERLAY_PERMISSION);
        intent.setData(Uri.fromParts("package", mContext.getPackageName(), null));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        RuntimeException e = expectThrows(RuntimeException.class,
                () -> mContext.startActivity(intent));

        assertTrue(e instanceof ActivityNotFoundException || e instanceof SecurityException);
    }


    @Test
    public void testManageOverlayPermissionIntentWithDataResolvesToSameIntentWithoutData() {
        Intent genericIntent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION);
        Intent appSpecificIntent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION);
        appSpecificIntent.setData(Uri.fromParts("package", mContext.getPackageName(), null));

        ResolveInfo genericResolveInfo = mPackageManager.resolveActivity(genericIntent, 0);
        ResolveInfo appSpecificResolveInfo = mPackageManager.resolveActivity(appSpecificIntent, 0);

        String errorMessage =
                "ACTION_MANAGE_OVERLAY_PERMISSION intent with data and without data should "
                        + "resolve to the same activity";
        if (genericResolveInfo == null) {
            assertNull(errorMessage, appSpecificResolveInfo);
            return;
        }
        assertNotNull(errorMessage, appSpecificResolveInfo);
        ActivityInfo genericActivity = genericResolveInfo.activityInfo;
        ActivityInfo appActivity = appSpecificResolveInfo.activityInfo;
        assertEquals(errorMessage, genericActivity.packageName, appActivity.packageName);
        assertEquals(errorMessage, genericActivity.name, appActivity.name);
    }
}
