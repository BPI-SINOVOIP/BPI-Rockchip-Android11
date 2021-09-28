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
import static android.os.UserManager.DISALLOW_APPS_CONTROL;
import static android.os.UserManager.DISALLOW_UNINSTALL_APPS;
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
import static com.android.suspendapps.testdeviceadmin.TestCommsReceiver.ACTION_SUSPEND;
import static com.android.suspendapps.testdeviceadmin.TestCommsReceiver.ACTION_UNBLOCK_UNINSTALL;
import static com.android.suspendapps.testdeviceadmin.TestCommsReceiver.ACTION_UNSUSPEND;
import static com.android.suspendapps.testdeviceadmin.TestCommsReceiver.EXTRA_USER_RESTRICTION;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.FeatureUtil;

import libcore.util.EmptyArray;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@LargeTest
public class DualSuspendTests {
    private Context mContext;
    private Handler mReceiverHandler;
    private TestAppInterface mTestAppInterface;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mReceiverHandler = new Handler(Looper.getMainLooper());
        assumeTrue("Skipping test that requires device admin",
                FeatureUtil.hasSystemFeature(PackageManager.FEATURE_DEVICE_ADMIN));
        addAndAssertProfileOwner();
    }

    private boolean setSuspendViaDpm(boolean suspend) throws Exception {
        return requestDpmAction(suspend ? ACTION_SUSPEND : ACTION_UNSUSPEND,
                createSingleKeyBundle(Intent.EXTRA_PACKAGE_NAME, TEST_APP_PACKAGE_NAME),
                mReceiverHandler);
    }

    @Test
    public void testMyPackageSuspended() throws Exception {
        mTestAppInterface = new TestAppInterface(mContext);

        mTestAppInterface.startListeningForBroadcast(Intent.ACTION_MY_PACKAGE_SUSPENDED);
        SuspendTestUtils.suspend(null, null, null);
        assertNotNull(Intent.ACTION_MY_PACKAGE_SUSPENDED + " not reported",
                mTestAppInterface.awaitBroadcast());

        mTestAppInterface.startListeningForBroadcast(Intent.ACTION_MY_PACKAGE_SUSPENDED);
        assertTrue("Suspend via dpm failed", setSuspendViaDpm(true));
        assertNotNull(Intent.ACTION_MY_PACKAGE_SUSPENDED + " not reported",
                mTestAppInterface.awaitBroadcast());
    }

    @Test
    public void testMyPackageUnsuspended() throws Exception {
        mTestAppInterface = new TestAppInterface(mContext);
        SuspendTestUtils.suspend(null, null, null);
        assertTrue("Suspend via dpm failed", setSuspendViaDpm(true));

        mTestAppInterface.startListeningForBroadcast(Intent.ACTION_MY_PACKAGE_UNSUSPENDED);
        SuspendTestUtils.unsuspendAll();
        final Intent unexpectedIntent = mTestAppInterface.awaitBroadcast(5_000);
        assertNull(unexpectedIntent + " reported after just one unsuspend", unexpectedIntent);

        assertTrue("Unsuspend via dpm failed", setSuspendViaDpm(false));
        assertNotNull(Intent.ACTION_MY_PACKAGE_UNSUSPENDED + " not reported after both unsuspends",
                mTestAppInterface.awaitBroadcast());
    }

    @Test
    public void testIsPackageSuspended() throws Exception {
        final PackageManager pm = mContext.getPackageManager();
        assertFalse(pm.isPackageSuspended(TEST_APP_PACKAGE_NAME));
        SuspendTestUtils.suspend(null, null, null);
        assertTrue("Suspend via dpm failed", setSuspendViaDpm(true));
        assertTrue("Package should be suspended by both",
                pm.isPackageSuspended(TEST_APP_PACKAGE_NAME));
        SuspendTestUtils.unsuspendAll();
        assertTrue("Package should be suspended by dpm",
                pm.isPackageSuspended(TEST_APP_PACKAGE_NAME));
        SuspendTestUtils.suspend(null, null, null);
        assertTrue("Unsuspend via dpm failed", setSuspendViaDpm(false));
        assertTrue("Package should be suspended by shell",
                pm.isPackageSuspended(TEST_APP_PACKAGE_NAME));
        SuspendTestUtils.unsuspendAll();
        assertFalse("Package should be suspended by neither",
                pm.isPackageSuspended(TEST_APP_PACKAGE_NAME));
    }

    private void assertDpmCanSuspendUnderUserRestriction(String userRestriction) throws Exception {
        mTestAppInterface = new TestAppInterface(mContext);
        final Bundle extra = createSingleKeyBundle(EXTRA_USER_RESTRICTION, userRestriction);
        assertTrue("Request to add restriction" + userRestriction + " failed",
                requestDpmAction(ACTION_ADD_USER_RESTRICTION, extra, mReceiverHandler));
        assertTrue("Request to suspend via dpm failed", setSuspendViaDpm(true));

        assertTrue("Test app not suspended", mTestAppInterface.isTestAppSuspended());
    }

    @Test
    public void testDpmCanSuspendUnderDisallowAppsControl() throws Exception {
        assertDpmCanSuspendUnderUserRestriction(DISALLOW_APPS_CONTROL);
    }

    @Test
    public void testDpmCanSuspendUnderDisallowUninstallApps() throws Exception {
        assertDpmCanSuspendUnderUserRestriction(DISALLOW_UNINSTALL_APPS);
    }

    @Test
    public void testUnsuspendedOnUninstallBlocked() throws Exception {
        mTestAppInterface = new TestAppInterface(mContext);
        SuspendTestUtils.suspendAndAssertResult(ALL_TEST_PACKAGES, null, null, null,
                EmptyArray.STRING);

        final Bundle extras = createSingleKeyBundle(EXTRA_PACKAGE_NAME, TEST_APP_PACKAGE_NAME);
        assertTrue("Block uninstall request failed", requestDpmAction(
                ACTION_BLOCK_UNINSTALL, extras, mReceiverHandler));

        // Package is unsuspended synchronously as part of setUninstallBlocked so the suspended
        // state should be immediately reflected
        assertFalse("Test app still suspended", mTestAppInterface.isTestAppSuspended());
    }

    private void assertUnsuspendedOnUserRestriction(String userRestriction) throws Exception {
        mTestAppInterface = new TestAppInterface(mContext);
        assertTrue("Test app not suspended before setting user restriction",
                mTestAppInterface.isTestAppSuspended());

        // Packages will be unsuspended asynchronously, so cannot check the suspended state directly
        mTestAppInterface.startListeningForBroadcast(Intent.ACTION_MY_PACKAGE_UNSUSPENDED);
        final Bundle extras = createSingleKeyBundle(EXTRA_USER_RESTRICTION, userRestriction);
        assertTrue("Request to add restriction " + userRestriction + " failed",
                requestDpmAction(ACTION_ADD_USER_RESTRICTION, extras, mReceiverHandler));

        assertNotNull(Intent.ACTION_MY_PACKAGE_UNSUSPENDED
                        + " not reported after setting restriction " + userRestriction,
                mTestAppInterface.awaitBroadcast());
    }

    @Test
    public void testUnsuspendedOnDisallowUninstallApps() throws Exception {
        SuspendTestUtils.suspend(null, null, null);
        assertUnsuspendedOnUserRestriction(DISALLOW_UNINSTALL_APPS);
    }

    @Test
    public void testUnsuspendedOnDisallowAppsControl() throws Exception {
        SuspendTestUtils.suspend(null, null, null);
        assertUnsuspendedOnUserRestriction(DISALLOW_APPS_CONTROL);
    }

    @Test
    public void testCannotSuspendWhenUninstallBlocked() throws Exception {
        final Bundle extras = createSingleKeyBundle(EXTRA_PACKAGE_NAME, TEST_APP_PACKAGE_NAME);
        assertTrue("Block uninstall request failed", requestDpmAction(
                ACTION_BLOCK_UNINSTALL, extras, mReceiverHandler));
        SuspendTestUtils.suspendAndAssertResult(ALL_TEST_PACKAGES, null, null, null,
                TEST_PACKAGE_ARRAY);
    }

    private void assertCannotSuspendUnderUserRestriction(String userRestriction) throws Exception {
        final Bundle extras = createSingleKeyBundle(EXTRA_USER_RESTRICTION, userRestriction);
        assertTrue("Request to add restriction" + userRestriction + " failed",
                requestDpmAction(ACTION_ADD_USER_RESTRICTION, extras, mReceiverHandler));
        SuspendTestUtils.suspendAndAssertResult(ALL_TEST_PACKAGES, null, null, null,
                ALL_TEST_PACKAGES);
    }

    @Test
    public void testCannotSuspendUnderDisallowAppsControl() throws Exception {
        assertCannotSuspendUnderUserRestriction(DISALLOW_APPS_CONTROL);
    }

    @Test
    public void testCannotSuspendUnderDisallowUninstallApps() throws Exception {
        assertCannotSuspendUnderUserRestriction(DISALLOW_UNINSTALL_APPS);
    }

    @After
    public void tearDown() throws Exception {
        if (mTestAppInterface != null) {
            mTestAppInterface.disconnect();
        }
        final DevicePolicyManager dpm = mContext.getSystemService(DevicePolicyManager.class);
        if (dpm.isAdminActive(ComponentName.unflattenFromString(DEVICE_ADMIN_COMPONENT))) {
            setSuspendViaDpm(false);
            final Bundle extras = createSingleKeyBundle(EXTRA_PACKAGE_NAME, TEST_APP_PACKAGE_NAME);
            requestDpmAction(ACTION_UNBLOCK_UNINSTALL, extras, mReceiverHandler);
        }
        removeDeviceAdmin();
        SuspendTestUtils.unsuspendAll();
    }
}
