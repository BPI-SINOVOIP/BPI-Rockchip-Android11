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

package android.suspendapps.permission.cts;

import static android.content.pm.PackageManager.RESTRICTION_HIDE_FROM_SUGGESTIONS;
import static android.content.pm.PackageManager.RESTRICTION_HIDE_NOTIFICATIONS;
import static android.content.pm.PackageManager.RESTRICTION_NONE;

import static org.junit.Assert.fail;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.SuspendDialogInfo;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.suspendapps.suspendtestapp.Constants;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.Callable;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class NegativePermissionsTest {
    private static final String TAG = NegativePermissionsTest.class.getSimpleName();
    private static final String[] PACKAGES_TO_SUSPEND = new String[] {
            Constants.PACKAGE_NAME,
            Constants.ANDROID_PACKAGE_NAME_2,
    };

    private PackageManager mPackageManager;

    @Before
    public void setUp() {
        final Context context = InstrumentationRegistry.getTargetContext();
        mPackageManager = context.getPackageManager();
    }

    private void assertSecurityException(Callable callable, String tag) {
        try {
            callable.call();
        } catch (SecurityException e) {
            // Passed.
            return;
        } catch (Exception e) {
            Log.e(TAG, "Unexpected exception while calling [" + tag + "]", e);
            fail("Unexpected exception while calling [" + tag + "]: " + e.getMessage());
        }
        fail("Call [" + tag + "] succeeded without permissions");
    }

    @Test
    public void setPackagesSuspended() {
        assertSecurityException(
                () -> mPackageManager.setPackagesSuspended(PACKAGES_TO_SUSPEND, true, null, null,
                        (SuspendDialogInfo) null), "setPackagesSuspended:true");
        assertSecurityException(
                () -> mPackageManager.setPackagesSuspended(PACKAGES_TO_SUSPEND, false, null, null,
                        (SuspendDialogInfo) null), "setPackagesSuspended:false");
    }

    @Test
    public void setPackagesSuspended_deprecated() {
        assertSecurityException(
                () -> mPackageManager.setPackagesSuspended(PACKAGES_TO_SUSPEND, true, null, null,
                        (String) null), "setPackagesSuspended:true");
        assertSecurityException(
                () -> mPackageManager.setPackagesSuspended(PACKAGES_TO_SUSPEND, false, null, null,
                        (String) null), "setPackagesSuspended:false");
    }

    @Test
    public void setDistractingPackageRestrictions() {
        assertSecurityException(
                () -> mPackageManager.setDistractingPackageRestrictions(PACKAGES_TO_SUSPEND,
                        RESTRICTION_HIDE_FROM_SUGGESTIONS),
                "setDistractingPackageRestrictions:HIDE_FROM_SUGGESTIONS");
        assertSecurityException(
                () -> mPackageManager.setDistractingPackageRestrictions(PACKAGES_TO_SUSPEND,
                        RESTRICTION_HIDE_FROM_SUGGESTIONS | RESTRICTION_HIDE_NOTIFICATIONS),
                "setDistractingPackageRestrictions:HIDE_FROM_SUGGESTIONS|HIDE_NOTIFICATIONS");
        assertSecurityException(
                () -> mPackageManager.setDistractingPackageRestrictions(PACKAGES_TO_SUSPEND,
                        RESTRICTION_HIDE_NOTIFICATIONS),
                "setDistractingPackageRestrictions:HIDE_NOTIFICATIONS");
        assertSecurityException(
                () -> mPackageManager.setDistractingPackageRestrictions(PACKAGES_TO_SUSPEND,
                        RESTRICTION_NONE), "setDistractingPackageRestrictions:NONE");
    }
}
