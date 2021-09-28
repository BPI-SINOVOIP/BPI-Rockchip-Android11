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

package android.permission.cts;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.OnPermissionsChangedListener;
import android.platform.test.annotations.AppModeFull;

import androidx.annotation.NonNull;
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@AppModeFull(reason = "Instant apps cannot access properties of other apps")
@RunWith(AndroidJUnit4ClassRunner.class)
public class PermissionUpdateListenerTest {
    private static final String APK =
            "/data/local/tmp/cts/permissions/"
                    + "CtsAppThatRequestsCalendarContactsBodySensorCustomPermission.apk";
    private static final String PACKAGE_NAME =
            "android.permission.cts.appthatrequestcustompermission";
    private static final String PERMISSION_NAME = "android.permission.READ_CONTACTS";
    private static final int TIMEOUT = 30000;

    private static final Context sContext =
            InstrumentationRegistry.getInstrumentation().getContext();
    private static final PackageManager sPm = sContext.getPackageManager();
    private static int sUid;

    @BeforeClass
    public static void installApp() throws PackageManager.NameNotFoundException {
        runShellCommand("pm install -r " + APK);
        sUid = sPm.getPackageUid(PACKAGE_NAME, 0);
    }

    @AfterClass
    public static void unInstallApp() {
        runShellCommand("pm uninstall " + PACKAGE_NAME);
    }

    private class LatchWithPermissionsChangedListener extends CountDownLatch
            implements OnPermissionsChangedListener {

        LatchWithPermissionsChangedListener() {
            super(1);
        }

        public void onPermissionsChanged(int uid) {
            if (uid == sUid) {
                countDown();
            }
        }
    }

    private void waitForLatchAndRemoveListener(@NonNull LatchWithPermissionsChangedListener latch)
            throws Exception {
        latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
        runWithShellPermissionIdentity(() -> sPm.removeOnPermissionsChangeListener(latch));
        assertThat(latch.getCount()).isEqualTo((long) 0);
    }

    @Test
    public void grantNotifiesListener() throws Exception {
        LatchWithPermissionsChangedListener listenerCalled =
                new LatchWithPermissionsChangedListener();

        runWithShellPermissionIdentity(() -> {
            sPm.revokeRuntimePermission(PACKAGE_NAME, PERMISSION_NAME, sContext.getUser());
            sPm.addOnPermissionsChangeListener(listenerCalled);
            sPm.grantRuntimePermission(PACKAGE_NAME, PERMISSION_NAME, sContext.getUser());
        });
        waitForLatchAndRemoveListener(listenerCalled);
    }

    @Test
    public void revokeNotifiesListener() throws Exception {
        LatchWithPermissionsChangedListener listenerCalled =
                new LatchWithPermissionsChangedListener();

        runWithShellPermissionIdentity(() -> {
            sPm.grantRuntimePermission(PACKAGE_NAME, PERMISSION_NAME, sContext.getUser());
            sPm.addOnPermissionsChangeListener(listenerCalled);
            sPm.revokeRuntimePermission(PACKAGE_NAME, PERMISSION_NAME, sContext.getUser());
        });
        waitForLatchAndRemoveListener(listenerCalled);
    }

    @Test
    public void updateFlagsNotifiesListener() throws Exception {
        LatchWithPermissionsChangedListener listenerCalled =
                new LatchWithPermissionsChangedListener();

        runWithShellPermissionIdentity(() -> {
            sPm.addOnPermissionsChangeListener(listenerCalled);
            int flag = PackageManager.FLAG_PERMISSION_USER_SET;
            sPm.updatePermissionFlags(PERMISSION_NAME, PACKAGE_NAME, flag, flag,
                    sContext.getUser());
        });
        waitForLatchAndRemoveListener(listenerCalled);
    }
}
