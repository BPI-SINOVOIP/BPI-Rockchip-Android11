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

import static com.google.common.truth.Truth.assertThat;

import android.Manifest;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.platform.test.annotations.AppModeFull;

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

@AppModeFull(reason = "Tests properties of other app. Instant apps cannot interact with other apps")
@RunWith(AndroidJUnit4ClassRunner.class)
public class ActivityPermissionRationaleTest {
    private static final String APK =
            "/data/local/tmp/cts/permissions/CtsAppThatRunsRationaleTests.apk";
    private static final String PACKAGE_NAME = "android.permission.cts.appthatrunsrationaletests";
    private static final String PERMISSION_NAME = Manifest.permission.READ_CONTACTS;
    private static final String CALLBACK_KEY = "testactivitycallback";
    private static final String RESULT_KEY = "testactivityresult";
    private static final int TIMEOUT = 5000;

    private static Context sContext = InstrumentationRegistry.getInstrumentation().getContext();
    private static UiAutomation sUiAuto =
            InstrumentationRegistry.getInstrumentation().getUiAutomation();

    @BeforeClass
    public static void setUp() {
        runShellCommand("pm install -r " + APK);
        int flag = PackageManager.FLAG_PERMISSION_USER_SET;
        PermissionUtils.setPermissionFlags(PACKAGE_NAME, PERMISSION_NAME, flag, flag);
    }

    @AfterClass
    public static void unInstallApp() {
        runShellCommand("pm uninstall " + PACKAGE_NAME);
    }

    private void assertAppShowRationaleIs(boolean expected) throws Exception {
        CompletableFuture<Boolean> callbackReturn = new CompletableFuture<>();
        RemoteCallback cb = new RemoteCallback((Bundle result) ->
                callbackReturn.complete(result.getBoolean(RESULT_KEY)));
        Intent intent = new Intent();
        intent.setComponent(new ComponentName(PACKAGE_NAME, PACKAGE_NAME + ".TestActivity"));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(CALLBACK_KEY, cb);

        sContext.startActivity(intent);
        assertThat(callbackReturn.get(TIMEOUT, TimeUnit.MILLISECONDS)).isEqualTo(expected);
    }

    @Before
    public void clearData() {
        runShellCommand("pm clear --user " + sContext.getUserId()
                + " android.permission.cts.appthatrunsrationaletests");
        PermissionUtils.setPermissionFlags(PACKAGE_NAME, PERMISSION_NAME,
                PackageManager.FLAG_PERMISSION_POLICY_FIXED, 0);
    }

    @Test
    public void permissionGrantedNoRationale() throws Exception {
        sUiAuto.grantRuntimePermission(PACKAGE_NAME, PERMISSION_NAME);

        assertAppShowRationaleIs(false);
    }

    @Test
    public void policyFixedNoRationale() throws Exception {
        int flags = PackageManager.FLAG_PERMISSION_POLICY_FIXED;
        PermissionUtils.setPermissionFlags(PACKAGE_NAME, PERMISSION_NAME, flags, flags);

        assertAppShowRationaleIs(false);
    }

    @Test
    public void userFixedNoRationale() throws Exception {
        int flags = PackageManager.FLAG_PERMISSION_USER_FIXED;
        PermissionUtils.setPermissionFlags(PACKAGE_NAME, PERMISSION_NAME, flags, flags);

        assertAppShowRationaleIs(false);
    }

    @Test
    public void notUserSetNoRationale() throws Exception {
        PermissionUtils.setPermissionFlags(PACKAGE_NAME, PERMISSION_NAME,
                PackageManager.FLAG_PERMISSION_USER_SET, 0);

        assertAppShowRationaleIs(false);
    }

    @Test
    public void userSetNeedRationale() throws Exception {
        int flags = PackageManager.FLAG_PERMISSION_USER_SET;
        PermissionUtils.setPermissionFlags(PACKAGE_NAME, PERMISSION_NAME, flags, flags);

        assertAppShowRationaleIs(true);
    }
}
