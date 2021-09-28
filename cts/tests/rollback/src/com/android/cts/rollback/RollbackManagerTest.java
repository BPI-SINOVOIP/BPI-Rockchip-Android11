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

package com.android.cts.rollback;

import static com.android.cts.rollback.lib.RollbackInfoSubject.assertThat;
import static com.android.cts.rollback.lib.RollbackUtils.getRollbackManager;

import static com.google.common.truth.Truth.assertThat;

import android.Manifest;
import android.content.rollback.RollbackInfo;

import androidx.test.InstrumentationRegistry;

import com.android.cts.install.lib.Install;
import com.android.cts.install.lib.InstallUtils;
import com.android.cts.install.lib.TestApp;
import com.android.cts.install.lib.Uninstall;
import com.android.cts.rollback.lib.Rollback;
import com.android.cts.rollback.lib.RollbackUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.IOException;

/**
 * CTS Tests for RollbackManager APIs.
 */
@RunWith(JUnit4.class)
public class RollbackManagerTest {

    /**
     * Adopts common permissions needed to test rollbacks and uninstalls the
     * test apps.
     */
    @Before
    public void setup() throws InterruptedException, IOException {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(
                    Manifest.permission.INSTALL_PACKAGES,
                    Manifest.permission.DELETE_PACKAGES,
                    Manifest.permission.TEST_MANAGE_ROLLBACKS);

        Uninstall.packages(TestApp.A);
    }

    /**
     * Drops adopted shell permissions and uninstalls the test apps.
     */
    @After
    public void teardown() throws InterruptedException, IOException {
        Uninstall.packages(TestApp.A);

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    /**
     * Test basic rollbacks.
     */
    @Test
    public void testBasic() throws Exception {
        Install.single(TestApp.A1).commit();
        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(1);
        RollbackUtils.waitForRollbackGone(
                () -> getRollbackManager().getAvailableRollbacks(), TestApp.A);
        RollbackUtils.waitForRollbackGone(
                () -> getRollbackManager().getRecentlyCommittedRollbacks(), TestApp.A);

        Install.single(TestApp.A2).setEnableRollback().commit();
        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(2);
        InstallUtils.processUserData(TestApp.A);
        RollbackInfo available = RollbackUtils.waitForAvailableRollback(TestApp.A);
        assertThat(available).isNotStaged();
        assertThat(available).packagesContainsExactly(
                Rollback.from(TestApp.A2).to(TestApp.A1));
        assertThat(RollbackUtils.getCommittedRollback(TestApp.A)).isNull();

        RollbackUtils.rollback(available.getRollbackId(), TestApp.A2);
        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(1);
        InstallUtils.processUserData(TestApp.A);
        RollbackUtils.waitForUnavailableRollback(TestApp.A);
        RollbackInfo committed = RollbackUtils.getCommittedRollback(TestApp.A);
        assertThat(committed).isNotNull();
        assertThat(committed).hasRollbackId(available.getRollbackId());
        assertThat(committed).isNotStaged();
        assertThat(committed).packagesContainsExactly(
                Rollback.from(TestApp.A2).to(TestApp.A1));
        assertThat(committed).causePackagesContainsExactly(TestApp.A2);
    }

    @Test
    public void testGetRollbackDataPolicy() throws Exception {
        // TODO: To change to the following statement when
        // PackageManager.RollbackDataPolicy.WIPE is available.
        // final int rollBackDataPolicy = PackageManager.RollbackDataPolicy.WIPE;
        final int rollBackDataPolicy = 1;

        Install.single(TestApp.A1).commit();
        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(1);

        // Enable rollback with rollBackDataPolicy
        final int sessionId = Install.single(TestApp.A2).setEnableRollback(
                rollBackDataPolicy).createSession();

        try {
            assertThat(InstallUtils.getPackageInstaller().getSessionInfo(
                    sessionId).getRollbackDataPolicy()).isEqualTo(rollBackDataPolicy);
        } finally {
            // Abandon the session
            InstallUtils.getPackageInstaller().abandonSession(sessionId);
        }
    }
}
