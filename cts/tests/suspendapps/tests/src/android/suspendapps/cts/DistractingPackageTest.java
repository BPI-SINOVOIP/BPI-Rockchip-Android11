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

package android.suspendapps.cts;

import static android.content.Intent.EXTRA_PACKAGE_NAME;
import static android.content.pm.PackageManager.RESTRICTION_HIDE_FROM_SUGGESTIONS;
import static android.content.pm.PackageManager.RESTRICTION_HIDE_NOTIFICATIONS;
import static android.suspendapps.cts.Constants.ALL_TEST_PACKAGES;
import static android.suspendapps.cts.Constants.DEVICE_ADMIN_COMPONENT;
import static android.suspendapps.cts.Constants.TEST_APP_PACKAGE_NAME;
import static android.suspendapps.cts.Constants.TEST_PACKAGE_ARRAY;
import static android.suspendapps.cts.SuspendTestUtils.addAndAssertProfileOwner;
import static android.suspendapps.cts.SuspendTestUtils.createSingleKeyBundle;
import static android.suspendapps.cts.SuspendTestUtils.removeDeviceAdmin;
import static android.suspendapps.cts.SuspendTestUtils.requestDpmAction;

import static com.android.suspendapps.testdeviceadmin.TestCommsReceiver.ACTION_ADD_USER_RESTRICTION;
import static com.android.suspendapps.testdeviceadmin.TestCommsReceiver.ACTION_BLOCK_UNINSTALL;
import static com.android.suspendapps.testdeviceadmin.TestCommsReceiver.ACTION_UNBLOCK_UNINSTALL;
import static com.android.suspendapps.testdeviceadmin.TestCommsReceiver.EXTRA_USER_RESTRICTION;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.annotation.NonNull;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.LauncherApps;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.FeatureUtil;
import com.android.compatibility.common.util.SystemUtil;
import com.android.internal.util.ArrayUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class DistractingPackageTest {
    private Context mContext;
    private PackageManager mPackageManager;
    private Handler mHandler;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mPackageManager = mContext.getPackageManager();
        mHandler = new Handler(Looper.getMainLooper());
    }

    private int getCurrentUserId() {
        final String result = SystemUtil.runShellCommand("am get-current-user",
                output -> !output.isEmpty());
        return Integer.parseInt(result.trim());
    }

    private void setDistractionFlagsAndAssertResult(String[] packagesToRestrict,
            int distractionFlags, @NonNull String[] expectedToFail) throws Exception {
        final String[] failed = SystemUtil.callWithShellPermissionIdentity(
                () -> mPackageManager.setDistractingPackageRestrictions(packagesToRestrict,
                        distractionFlags));
        if (failed == null || failed.length != expectedToFail.length
                || !ArrayUtils.containsAll(failed, expectedToFail)) {
            fail("setDistractingPackageRestrictions failure: failed packages: " + Arrays.toString(
                    failed) + "; expected to fail: " + Arrays.toString(expectedToFail));
        }
    }

    @Test
    public void testShouldHideFromSuggestions() throws Exception {
        final int currentUserId = getCurrentUserId();
        final LauncherApps launcherApps = mContext.getSystemService(LauncherApps.class);
        assertFalse("shouldHideFromSuggestions true before setting the flag",
                launcherApps.shouldHideFromSuggestions(TEST_APP_PACKAGE_NAME,
                        UserHandle.of(currentUserId)));
        setDistractionFlagsAndAssertResult(TEST_PACKAGE_ARRAY, RESTRICTION_HIDE_FROM_SUGGESTIONS,
                ArrayUtils.emptyArray(String.class));
        assertTrue("shouldHideFromSuggestions false after setting the flag",
                launcherApps.shouldHideFromSuggestions(TEST_APP_PACKAGE_NAME,
                        UserHandle.of(currentUserId)));
    }

    @Test
    public void testCannotRestrictWhenUninstallBlocked() throws Exception {
        assumeTrue(FeatureUtil.hasSystemFeature(PackageManager.FEATURE_DEVICE_ADMIN));
        addAndAssertProfileOwner();
        assertTrue("Block uninstall request failed", requestDpmAction(ACTION_BLOCK_UNINSTALL,
                createSingleKeyBundle(EXTRA_PACKAGE_NAME, TEST_APP_PACKAGE_NAME), mHandler));
        setDistractionFlagsAndAssertResult(ALL_TEST_PACKAGES, RESTRICTION_HIDE_NOTIFICATIONS,
                TEST_PACKAGE_ARRAY);
    }

    private void assertCannotRestrictUnderUserRestriction(String userRestriction) throws Exception {
        addAndAssertProfileOwner();
        final Bundle extras = createSingleKeyBundle(EXTRA_USER_RESTRICTION, userRestriction);
        assertTrue("Request to add restriction" + userRestriction + " failed",
                requestDpmAction(ACTION_ADD_USER_RESTRICTION, extras, mHandler));
        setDistractionFlagsAndAssertResult(ALL_TEST_PACKAGES, RESTRICTION_HIDE_FROM_SUGGESTIONS,
                ALL_TEST_PACKAGES);
    }

    @Test
    public void testCannotRestrictUnderDisallowAppsControl() throws Exception {
        assumeTrue(FeatureUtil.hasSystemFeature(PackageManager.FEATURE_DEVICE_ADMIN));
        assertCannotRestrictUnderUserRestriction(UserManager.DISALLOW_APPS_CONTROL);
    }

    @Test
    public void testCannotRestrictUnderDisallowUninstallApps() throws Exception {
        assumeTrue(FeatureUtil.hasSystemFeature(PackageManager.FEATURE_DEVICE_ADMIN));
        assertCannotRestrictUnderUserRestriction(UserManager.DISALLOW_UNINSTALL_APPS);
    }

    @After
    public void tearDown() throws Exception {
        final DevicePolicyManager dpm = mContext.getSystemService(DevicePolicyManager.class);
        if (dpm.isAdminActive(ComponentName.unflattenFromString(DEVICE_ADMIN_COMPONENT))) {
            final Bundle extras = createSingleKeyBundle(EXTRA_PACKAGE_NAME, TEST_APP_PACKAGE_NAME);
            requestDpmAction(ACTION_UNBLOCK_UNINSTALL, extras, mHandler);
            removeDeviceAdmin();
        }
        setDistractionFlagsAndAssertResult(ALL_TEST_PACKAGES, PackageManager.RESTRICTION_NONE,
                ArrayUtils.emptyArray(String.class));
    }
}
