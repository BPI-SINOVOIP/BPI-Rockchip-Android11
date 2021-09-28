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

package com.android.tests.atomicinstall;

import static com.android.cts.install.lib.InstallUtils.assertStatusSuccess;
import static com.android.cts.install.lib.InstallUtils.getInstalledVersion;
import static com.android.cts.install.lib.InstallUtils.openPackageInstallerSession;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.Manifest;
import android.content.pm.PackageInstaller;

import androidx.test.InstrumentationRegistry;

import com.android.cts.install.lib.Install;
import com.android.cts.install.lib.InstallUtils;
import com.android.cts.install.lib.LocalIntentSender;
import com.android.cts.install.lib.TestApp;
import com.android.cts.install.lib.Uninstall;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Tests for multi-package (a.k.a. atomic) installs.
 */
@RunWith(JUnit4.class)
public class AtomicInstallTest {

    public static final String TEST_APP_CORRUPT_FILENAME = "corrupt.apk";
    private static final TestApp CORRUPT_TESTAPP = new TestApp(
            "corrupt", "com.corrupt", 1, false, TEST_APP_CORRUPT_FILENAME);

    private void adoptShellPermissions() {
        InstrumentationRegistry
                .getInstrumentation()
                .getUiAutomation()
                .adoptShellPermissionIdentity(Manifest.permission.INSTALL_PACKAGES,
                    Manifest.permission.DELETE_PACKAGES);
    }

    @Before
    public void setup() throws Exception {
        adoptShellPermissions();

        Uninstall.packages(TestApp.A, TestApp.B);
    }

    @After
    public void dropShellPermissions() {
        InstrumentationRegistry
                .getInstrumentation()
                .getUiAutomation()
                .dropShellPermissionIdentity();
    }

    @Test
    public void testInstallTwoApks() throws Exception {
        Install.multi(TestApp.A1, TestApp.B1).commit();
        assertThat(getInstalledVersion(TestApp.A)).isEqualTo(1);
        assertThat(getInstalledVersion(TestApp.B)).isEqualTo(1);
    }

    @Test
    public void testInstallTwoApksDowngradeFail() throws Exception {
        Install.multi(TestApp.A2, TestApp.B1).commit();
        assertThat(getInstalledVersion(TestApp.A)).isEqualTo(2);
        assertThat(getInstalledVersion(TestApp.B)).isEqualTo(1);

        InstallUtils.commitExpectingFailure(AssertionError.class,
                "INSTALL_FAILED_VERSION_DOWNGRADE", Install.multi(TestApp.A1, TestApp.B1));
        assertThat(getInstalledVersion(TestApp.A)).isEqualTo(2);
        assertThat(getInstalledVersion(TestApp.B)).isEqualTo(1);
    }

    @Test
    public void testFailInconsistentMultiPackageCommit() throws Exception {
        // Test inconsistency in staged settings
        Install parentStaged = Install.multi(Install.single(TestApp.A1)).setStaged();
        Install childStaged = Install.multi(Install.single(TestApp.A1).setStaged());

        assertInconsistentStagedSettings(parentStaged);
        assertThat(getInstalledVersion(TestApp.A)).isEqualTo(-1);
        assertInconsistentStagedSettings(childStaged);
        assertThat(getInstalledVersion(TestApp.A)).isEqualTo(-1);

        // Test inconsistency in rollback settings
        Install parentEnabledRollback = Install.multi(Install.single(TestApp.A1))
                .setEnableRollback();
        Install childEnabledRollback = Install.multi(
                Install.single(TestApp.A1).setEnableRollback());

        assertInconsistentRollbackSettings(parentEnabledRollback);
        assertThat(getInstalledVersion(TestApp.A)).isEqualTo(-1);
        assertInconsistentRollbackSettings(childEnabledRollback);
        assertThat(getInstalledVersion(TestApp.A)).isEqualTo(-1);
    }

    @Test
    public void testChildFailurePropagated() throws Exception {
        // Create a child session that "inherits" from a non-existent package. This
        // causes the session commit to fail with a PackageManagerException.
        Install childInstall = Install.single(TestApp.A1).setSessionMode(
                PackageInstaller.SessionParams.MODE_INHERIT_EXISTING);
        Install parentInstall = Install.multi(childInstall);

        InstallUtils.commitExpectingFailure(AssertionError.class, "Missing existing base package",
                parentInstall);

        assertThat(getInstalledVersion(TestApp.A)).isEqualTo(-1);
    }

    @Test
    public void testEarlyFailureFailsAll() throws Exception {
        InstallUtils.commitExpectingFailure(AssertionError.class, "Failed to parse",
                Install.multi(TestApp.A1, TestApp.B1, CORRUPT_TESTAPP));
        assertThat(getInstalledVersion(TestApp.A)).isEqualTo(-1);
        assertThat(getInstalledVersion(TestApp.B)).isEqualTo(-1);
    }

    @Test
    public void testInvalidStateScenarios() throws Exception {
        int parentSessionId = Install.multi(TestApp.A1, TestApp.B1).createSession();
        try (PackageInstaller.Session parentSession =
                     openPackageInstallerSession(parentSessionId)) {
            for (int childSessionId : parentSession.getChildSessionIds()) {
                try (PackageInstaller.Session childSession =
                             openPackageInstallerSession(childSessionId)) {
                    try {
                        childSession.commit(LocalIntentSender.getIntentSender());
                        fail("Should not be able to commit a child session!");
                    } catch (IllegalStateException e) {
                        // ignore
                    }
                    try {
                        childSession.abandon();
                        fail("Should not be able to abandon a child session!");
                    } catch (IllegalStateException e) {
                        // ignore
                    }
                }
            }
            int toAbandonSessionId = Install.single(TestApp.A1).createSession();
            try (PackageInstaller.Session toAbandonSession =
                         openPackageInstallerSession(toAbandonSessionId)) {
                toAbandonSession.abandon();
                try {
                    parentSession.addChildSessionId(toAbandonSessionId);
                    fail("Should not be able to add abandoned child session!");
                } catch (RuntimeException e) {
                    // ignore
                }
            }

            parentSession.commit(LocalIntentSender.getIntentSender());
            assertStatusSuccess(LocalIntentSender.getIntentSenderResult());
        }
    }

    private static void assertInconsistentStagedSettings(Install install) {
        assertInconsistentSettings("inconsistent staged settings", install);
    }

    private static void assertInconsistentRollbackSettings(Install install) {
        assertInconsistentSettings("inconsistent rollback settings", install);
    }

    private static void assertInconsistentSettings(String failMessage, Install install) {
        InstallUtils.commitExpectingFailure(AssertionError.class, failMessage, install);
    }
}
