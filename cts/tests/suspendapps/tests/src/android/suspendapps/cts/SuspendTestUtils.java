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

import static android.app.Activity.RESULT_CANCELED;
import static android.app.Activity.RESULT_OK;
import static android.suspendapps.cts.Constants.ALL_TEST_PACKAGES;
import static android.suspendapps.cts.Constants.DEVICE_ADMIN_COMPONENT;
import static android.suspendapps.cts.Constants.DEVICE_ADMIN_PACKAGE;
import static android.suspendapps.cts.Constants.TEST_APP_PACKAGE_NAME;
import static android.suspendapps.cts.Constants.TEST_PACKAGE_ARRAY;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.SuspendDialogInfo;
import android.os.BaseBundle;
import android.os.Bundle;
import android.os.Handler;
import android.os.PersistableBundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.SystemUtil;
import com.android.internal.util.ArrayUtils;
import com.android.suspendapps.suspendtestapp.SuspendTestActivity;
import com.android.suspendapps.testdeviceadmin.TestCommsReceiver;

import libcore.util.EmptyArray;

import java.util.Arrays;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public class SuspendTestUtils {

    private SuspendTestUtils() {
    }

    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    // Wrapping the suspend/unsuspend methods in another class is needed to avoid error in pre-Q
    // devices. Direct methods of the test class are enumerated and visited by JUnit, and the
    // Q+ class SuspendDialogInfo being a parameter type triggers an exception. See b/129913414.
    static void suspend(PersistableBundle appExtras,
            PersistableBundle launcherExtras, SuspendDialogInfo dialogInfo) throws Exception {
        suspendAndAssertResult(TEST_PACKAGE_ARRAY, appExtras, launcherExtras, dialogInfo,
                EmptyArray.STRING);
    }

    static void suspendAndAssertResult(String[] packagesToSuspend, PersistableBundle appExtras,
            PersistableBundle launcherExtras, SuspendDialogInfo dialogInfo,
            @NonNull String[] expectedToFail) throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();
        final String[] failed = SystemUtil.callWithShellPermissionIdentity(
                () -> packageManager.setPackagesSuspended(packagesToSuspend, true, appExtras,
                        launcherExtras, dialogInfo));
        if (failed == null || failed.length != expectedToFail.length
                || !ArrayUtils.containsAll(failed, expectedToFail)) {
            fail("setPackagesSuspended failure: failed packages: " + Arrays.toString(failed)
                    + "; Expected to fail: " + Arrays.toString(expectedToFail));
        }
    }

    static void unsuspendAll() throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();
        final String[] unchangedPackages = SystemUtil.callWithShellPermissionIdentity(() ->
                packageManager.setPackagesSuspended(ALL_TEST_PACKAGES, false, null, null,
                        (SuspendDialogInfo) null));
        assertTrue("setPackagesSuspended returned non-empty list", unchangedPackages.length == 0);
    }

    static Bundle createSingleKeyBundle(String key, String value) {
        final Bundle extras = new Bundle(1);
        extras.putString(key, value);
        return extras;
    }

    static void addAndAssertProfileOwner() {
        SystemUtil.runShellCommand("dpm set-profile-owner --user cur " + DEVICE_ADMIN_COMPONENT,
                output -> output.startsWith("Success"));
    }

    static void removeDeviceAdmin() {
        SystemUtil.runShellCommand("dpm remove-active-admin --user cur " + DEVICE_ADMIN_COMPONENT);
    }

    /**
     * Uses broadcasts to request a specific action from device admin via {@link TestCommsReceiver}
     *
     * @return true if the request succeeded
     */
    static boolean requestDpmAction(String action, @Nullable Bundle extras, Handler resultHandler)
            throws InterruptedException {
        final Intent requestIntent = new Intent(action)
                .setClassName(DEVICE_ADMIN_PACKAGE, TestCommsReceiver.class.getName())
                .addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        if (extras != null) {
            requestIntent.putExtras(extras);
        }
        final CountDownLatch resultLatch = new CountDownLatch(1);
        final AtomicInteger result = new AtomicInteger(RESULT_CANCELED);
        getContext().sendOrderedBroadcast(requestIntent, null,
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        result.set(getResultCode());
                        resultLatch.countDown();
                    }
                }, resultHandler, RESULT_CANCELED, null, null);
        assertTrue("Broadcast " + requestIntent.getAction() + " timed out",
                resultLatch.await(10, TimeUnit.SECONDS));
        return result.get() == RESULT_OK;
    }

    static boolean areSameExtras(BaseBundle expected, BaseBundle received) {
        if (expected == null || received == null) {
            return expected == received;
        }
        final Set<String> keys = expected.keySet();
        if (keys.size() != received.keySet().size()) {
            return false;
        }
        for (String key : keys) {
            if (!Objects.equals(expected.get(key), received.get(key))) {
                return false;
            }
        }
        return true;
    }

    static void assertSameExtras(String message, BaseBundle expected, BaseBundle received) {
        if (!areSameExtras(expected, received)) {
            fail(message + ": [expected: " + expected + "; received: " + received + "]");
        }
    }

    static void startTestAppActivity(@Nullable Bundle extras) {
        final Intent testActivity = new Intent()
                .setComponent(new ComponentName(TEST_APP_PACKAGE_NAME,
                        SuspendTestActivity.class.getCanonicalName()))
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (extras != null) {
            testActivity.putExtras(extras);
        }
        getContext().startActivity(testActivity);
    }

    static PersistableBundle createExtras(String keyPrefix, long lval, String sval,
            double dval) {
        final PersistableBundle extras = new PersistableBundle(3);
        extras.putLong(keyPrefix + ".LONG_VALUE", lval);
        extras.putDouble(keyPrefix + ".DOUBLE_VALUE", dval);
        extras.putString(keyPrefix + ".STRING_VALUE", sval);
        return extras;
    }
}
