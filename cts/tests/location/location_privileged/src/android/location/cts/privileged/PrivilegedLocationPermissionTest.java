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

package android.location.cts.privileged;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertNotNull;

import android.Manifest;
import android.content.Context;
import android.location.LocationManager;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;


@RunWith(AndroidJUnit4.class)
public class PrivilegedLocationPermissionTest {

    private Context mContext;
    private LocationManager mLocationManager;

    @Before
    public void setUp() throws Exception {
        mContext = ApplicationProvider.getApplicationContext();
        mLocationManager = mContext.getSystemService(LocationManager.class);
        assertNotNull(mLocationManager);

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(Manifest.permission.LOCATION_HARDWARE);
    }

    @After
    public void tearDown() throws Exception {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    @Test
    public void testExtraLocationControllerPackage() {
        // Extra location controller API is only available after Q.
        if (VERSION.SDK_INT < VERSION_CODES.Q) {
            return;
        }
        String originalPackageName = mLocationManager.getExtraLocationControllerPackage();
        boolean originalPackageEnabeld = mLocationManager.isExtraLocationControllerPackageEnabled();

        // Test setting extra location controller package.
        String packageName = mContext.getPackageName();
        mLocationManager.setExtraLocationControllerPackage(packageName);
        assertWithMessage("Extra location controller package").that(
                mLocationManager.getExtraLocationControllerPackage()).isEqualTo(packageName);

        // Test enabling extra location controller package.
        mLocationManager.setExtraLocationControllerPackageEnabled(!originalPackageEnabeld);
        assertWithMessage("Extra location controller package enabled").that(
                mLocationManager.isExtraLocationControllerPackageEnabled()).isEqualTo(
                !originalPackageEnabeld);

        // Reset the original extra location controller package.
        mLocationManager.setExtraLocationControllerPackage(originalPackageName);
        mLocationManager.setExtraLocationControllerPackageEnabled(originalPackageEnabeld);
    }
}
