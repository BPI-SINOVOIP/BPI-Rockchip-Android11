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

import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.suspendapps.cts.Constants.ALL_TEST_PACKAGES;
import static android.suspendapps.cts.Constants.DEVICE_ADMIN_COMPONENT;
import static android.suspendapps.cts.Constants.DEVICE_ADMIN_PACKAGE;
import static android.suspendapps.cts.Constants.TEST_APP_PACKAGE_NAME;
import static android.suspendapps.cts.Constants.TEST_PACKAGE_ARRAY;
import static android.suspendapps.cts.SuspendTestUtils.areSameExtras;
import static android.suspendapps.cts.SuspendTestUtils.assertSameExtras;
import static android.suspendapps.cts.SuspendTestUtils.createExtras;
import static android.suspendapps.cts.SuspendTestUtils.removeDeviceAdmin;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.Manifest;
import android.app.AppOpsManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.LauncherApps;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.PersistableBundle;
import android.os.UserHandle;
import android.platform.test.annotations.SystemUserOnly;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.FeatureUtil;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

@RunWith(AndroidJUnit4.class)
@LargeTest
public class SuspendPackagesTest {
    private Context mContext;
    private PackageManager mPackageManager;
    private AppOpsManager mAppOpsManager;
    private TestAppInterface mTestAppInterface;

    private void addAndAssertDeviceOwner() {
        SystemUtil.runShellCommand("dpm set-device-owner --user cur " + DEVICE_ADMIN_COMPONENT,
                output -> output.startsWith("Success"));
    }

    private void addAndAssertDeviceAdmin() {
        SystemUtil.runShellCommand("dpm set-active-admin --user cur " + DEVICE_ADMIN_COMPONENT,
                output -> output.startsWith("Success"));
    }

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mPackageManager = mContext.getPackageManager();
        mAppOpsManager = mContext.getSystemService(AppOpsManager.class);
    }

    @Test
    public void testIsPackageSuspended() throws Exception {
        SuspendTestUtils.suspend(null, null, null);
        assertTrue("isPackageSuspended is false",
                mPackageManager.isPackageSuspended(TEST_APP_PACKAGE_NAME));
    }

    @Test
    public void testSuspendedStateFromApp() throws Exception {
        mTestAppInterface = new TestAppInterface(mContext);
        assertFalse(mTestAppInterface.isTestAppSuspended());
        assertNull(mTestAppInterface.getTestAppSuspendedExtras());

        final PersistableBundle appExtras = createExtras("testSuspendedStateFromApp", 20, "20",
                0.2);
        SuspendTestUtils.suspend(appExtras, null, null);

        final Bundle extrasFromApp = mTestAppInterface.getTestAppSuspendedExtras();
        final boolean suspendedFromApp = mTestAppInterface.isTestAppSuspended();
        assertTrue("isPackageSuspended from suspended app is false", suspendedFromApp);
        assertSameExtras("getSuspendedPackageAppExtras different to the ones supplied",
                appExtras, extrasFromApp);
    }

    @Test
    public void testMyPackageSuspendedUnsuspended() throws Exception {
        mTestAppInterface = new TestAppInterface(mContext);

        mTestAppInterface.startListeningForBroadcast(Intent.ACTION_MY_PACKAGE_SUSPENDED);
        final PersistableBundle appExtras = createExtras("testMyPackageSuspendBroadcasts", 1, "1",
                .1);
        SuspendTestUtils.suspend(appExtras, null, null);
        assertNotNull(Intent.ACTION_MY_PACKAGE_SUSPENDED + " not reported",
                mTestAppInterface.awaitBroadcast());

        mTestAppInterface.startListeningForBroadcast(Intent.ACTION_MY_PACKAGE_UNSUSPENDED);
        SuspendTestUtils.unsuspendAll();
        assertNotNull(Intent.ACTION_MY_PACKAGE_UNSUSPENDED + " not reported",
                mTestAppInterface.awaitBroadcast());
    }

    @Test
    public void testUpdatingAppExtras() throws Exception {
        mTestAppInterface = new TestAppInterface(mContext);

        mTestAppInterface.startListeningForBroadcast(Intent.ACTION_MY_PACKAGE_SUSPENDED);
        final PersistableBundle extras1 = createExtras("testUpdatingAppExtras", 1,
                "1", 0.1);
        SuspendTestUtils.suspend(extras1, null, null);
        Intent intentFromApp = mTestAppInterface.awaitBroadcast();
        assertNotNull(Intent.ACTION_MY_PACKAGE_SUSPENDED + " not reported", intentFromApp);
        Bundle receivedExtras = intentFromApp.getBundleExtra(Intent.EXTRA_SUSPENDED_PACKAGE_EXTRAS);
        assertSameExtras("Received app extras different to the ones supplied", extras1,
                receivedExtras);

        mTestAppInterface.startListeningForBroadcast(Intent.ACTION_MY_PACKAGE_SUSPENDED);
        final PersistableBundle extras2 = createExtras("testUpdatingAppExtras", 2,
                "2", 0.2);
        SuspendTestUtils.suspend(extras2, null, null);
        intentFromApp = mTestAppInterface.awaitBroadcast();
        assertNotNull(Intent.ACTION_MY_PACKAGE_SUSPENDED + " not reported", intentFromApp);
        receivedExtras = intentFromApp.getBundleExtra(Intent.EXTRA_SUSPENDED_PACKAGE_EXTRAS);
        assertSameExtras("Received app extras different to the ones supplied", extras2,
                receivedExtras);
    }

    @Test
    public void testCannotSuspendSelf() throws Exception {
        final String[] self = new String[]{mContext.getPackageName()};
        SuspendTestUtils.suspendAndAssertResult(self, null, null, null, self);
    }

    @Test
    public void testActivityStoppedOnSuspend() throws Exception {
        mTestAppInterface = new TestAppInterface(mContext);

        SuspendTestUtils.startTestAppActivity(null);
        assertNotNull("Test app's activity not started",
                mTestAppInterface.awaitTestActivityStart());
        SuspendTestUtils.suspend(null, null, null);
        assertTrue("Test app's activity not stopped on suspending the app",
                mTestAppInterface.awaitTestActivityStop());
    }

    @SystemUserOnly(reason = "Device owner can be set up only on system user")
    @Test
    public void testCanSuspendWhenDeviceOwner() throws Exception {
        assumeTrue(FeatureUtil.hasSystemFeature(PackageManager.FEATURE_DEVICE_ADMIN));
        addAndAssertDeviceOwner();
        SuspendTestUtils.suspend(null, null, null);
    }

    private int getCurrentUserId() {
        final String result = SystemUtil.runShellCommand("am get-current-user");
        return Integer.parseInt(result.trim());
    }

    @Test
    public void testLauncherCallback() throws Exception {
        final PersistableBundle suppliedExtras = createExtras("testLauncherCallback", 2, "2", 0.2);
        final int currentUserId = getCurrentUserId();
        final AtomicReference<String> callBackErrors = new AtomicReference<>("");
        final CountDownLatch oldCallbackLatch = new CountDownLatch(1);
        final CountDownLatch newCallbackLatch = new CountDownLatch(1);
        LauncherApps.Callback testCallback = new StubbedCallback() {
            @Override
            public void onPackagesSuspended(String[] packageNames, UserHandle user) {
                oldCallbackLatch.countDown();
            }

            @Override
            public void onPackagesSuspended(String[] packageNames, UserHandle user,
                    Bundle launcherExtras) {
                final StringBuilder errorString = new StringBuilder();
                if (!Arrays.equals(packageNames, TEST_PACKAGE_ARRAY)) {
                    errorString.append("Received unexpected packageNames in onPackagesSuspended: ");
                    errorString.append(Arrays.toString(packageNames));
                    errorString.append(". ");
                }
                if (user.getIdentifier() != currentUserId) {
                    errorString.append("Received wrong user " + user.getIdentifier()
                            + ", current user: " + currentUserId + ". ");
                }
                if (!areSameExtras(launcherExtras, suppliedExtras)) {
                    errorString.append("Unexpected launcherExtras, supplied: " + suppliedExtras
                            + ", received: " + launcherExtras + ". ");
                }
                callBackErrors.set(errorString.toString());
                newCallbackLatch.countDown();
            }
        };
        final LauncherApps launcherApps = mContext.getSystemService(LauncherApps.class);
        launcherApps.registerCallback(testCallback, new Handler(Looper.getMainLooper()));
        SuspendTestUtils.suspend(null, suppliedExtras, null);
        assertFalse("Old callback invoked", oldCallbackLatch.await(2, TimeUnit.SECONDS));
        assertTrue("New callback not invoked", newCallbackLatch.await(2, TimeUnit.SECONDS));
        final String errors = callBackErrors.get();
        assertTrue("Callbacks did not complete as expected: " + errors, errors.isEmpty());
        launcherApps.unregisterCallback(testCallback);
    }

    @Test
    public void testTestAppsSuspendable() throws Exception {
        final String[] unsuspendablePkgs = SystemUtil.callWithShellPermissionIdentity(() ->
                mPackageManager.getUnsuspendablePackages(ALL_TEST_PACKAGES));
        assertTrue("Normal test apps unsuspendable: " + Arrays.toString(unsuspendablePkgs),
                unsuspendablePkgs.length == 0);
    }

    @Test
    public void testDeviceAdminUnsuspendable() throws Exception {
        assumeTrue(FeatureUtil.hasSystemFeature(PackageManager.FEATURE_DEVICE_ADMIN));
        addAndAssertDeviceAdmin();
        final String[] unsuspendablePkgs = SystemUtil.callWithShellPermissionIdentity(() ->
                mPackageManager.getUnsuspendablePackages(new String[]{DEVICE_ADMIN_PACKAGE}));
        assertTrue("Device admin suspendable", (unsuspendablePkgs.length == 1)
                && DEVICE_ADMIN_PACKAGE.equals(unsuspendablePkgs[0]));
    }

    private void assertOpDisallowed(int op) throws Exception {
        final int testUid = mPackageManager.getPackageUid(TEST_APP_PACKAGE_NAME, 0);
        final int mode = SystemUtil.callWithShellPermissionIdentity(
                () -> mAppOpsManager.checkOpNoThrow(op, testUid, TEST_APP_PACKAGE_NAME));
        assertNotEquals("Op " + AppOpsManager.opToName(op) + " allowed while package is suspended",
                MODE_ALLOWED, mode);
    }

    @Test
    public void testOpRecordAudioOnSuspend() throws Exception {
        final int recordAudioOp = AppOpsManager.permissionToOpCode(
                Manifest.permission.RECORD_AUDIO);
        SuspendTestUtils.suspend(null, null, null);
        assertOpDisallowed(recordAudioOp);
    }

    @Test
    public void testOpCameraOnSuspend() throws Exception {
        final int cameraOp = AppOpsManager.permissionToOpCode(Manifest.permission.CAMERA);
        SuspendTestUtils.suspend(null, null, null);
        assertOpDisallowed(cameraOp);
    }

    @After
    public void tearDown() throws Exception {
        if (mTestAppInterface != null) {
            mTestAppInterface.disconnect();
        }
        removeDeviceAdmin();
        SuspendTestUtils.unsuspendAll();
    }

    private abstract static class StubbedCallback extends LauncherApps.Callback {
        @Override
        public void onPackageRemoved(String packageName, UserHandle user) {
        }

        @Override
        public void onPackageAdded(String packageName, UserHandle user) {
        }

        @Override
        public void onPackageChanged(String packageName, UserHandle user) {
        }

        @Override
        public void onPackagesAvailable(String[] packageNames, UserHandle user, boolean replacing) {
        }

        @Override
        public void onPackagesUnavailable(String[] packageNames, UserHandle user,
                boolean replacing) {
        }
    }
}
