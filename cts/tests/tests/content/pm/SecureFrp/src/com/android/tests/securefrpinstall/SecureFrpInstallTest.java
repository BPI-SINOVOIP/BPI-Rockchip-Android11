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

package com.android.tests.atomicinstall;

import static org.junit.Assert.fail;

import android.Manifest;
import android.content.pm.PackageManager;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.SystemUtil;
import com.android.cts.install.lib.Install;
import com.android.cts.install.lib.TestApp;
import com.android.cts.install.lib.Uninstall;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests for package installation while Secure FRP mode is enabled. */
@RunWith(JUnit4.class)
public class SecureFrpInstallTest {

    private static PackageManager sPackageManager;

    private static void adoptShellPermissions() {
        InstrumentationRegistry
                .getInstrumentation()
                .getUiAutomation()
                .adoptShellPermissionIdentity(
                        Manifest.permission.INSTALL_PACKAGES, Manifest.permission.DELETE_PACKAGES);
    }

    private static void dropShellPermissions() {
        InstrumentationRegistry
                .getInstrumentation()
                .getUiAutomation()
                .dropShellPermissionIdentity();
    }

    private static void setSecureFrp(boolean secureFrp) throws Exception {
        adoptShellPermissions();
        SystemUtil.runShellCommand(InstrumentationRegistry.getInstrumentation(),
                "settings put --user 0 secure secure_frp_mode " + (secureFrp ? "1" : "0"));
        dropShellPermissions();
    }

    private static void assertInstalled() throws Exception {
        sPackageManager.getPackageInfo(TestApp.A, 0);
    }

    private static void assertNotInstalled() {
        try {
            sPackageManager.getPackageInfo(TestApp.A, 0);
            fail("Package should not be installed");
        } catch (PackageManager.NameNotFoundException expected) {
        }
    }

    @BeforeClass
    public static void initialize() {
        sPackageManager = InstrumentationRegistry
                .getInstrumentation()
                .getContext()
                .getPackageManager();
    }

    @Before
    public void setup() throws Exception {
        adoptShellPermissions();
        Uninstall.packages(TestApp.A);
        dropShellPermissions();
    }

    @After
    public void teardown() throws Exception {
        dropShellPermissions();
        setSecureFrp(false);
    }

    /** Tests a SecurityException is thrown while in secure FRP mode. */
    @Test
    public void testPackageInstallApi() throws Exception {
        setSecureFrp(true);
        try {
            Install.single(TestApp.A1).commit();
            fail("Expected a SecurityException");
        } catch (SecurityException expected) {
        }
        assertNotInstalled();
    }

    /** Tests can install when granted INSTALL_PACKAGES permission; even in secure FRP mode. */
    @Test
    public void testPackageInstallApiAsShell() throws Exception {
        setSecureFrp(true);
        adoptShellPermissions();
        Install.single(TestApp.A1).commit();
        dropShellPermissions();
        assertInstalled();
    }
}
