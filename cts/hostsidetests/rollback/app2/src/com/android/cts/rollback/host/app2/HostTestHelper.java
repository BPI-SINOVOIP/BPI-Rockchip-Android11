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

package com.android.cts.rollback.host.app2;

import static com.android.cts.rollback.lib.RollbackInfoSubject.assertThat;

import static com.google.common.truth.Truth.assertThat;

import android.Manifest;
import android.content.rollback.RollbackInfo;

import com.android.cts.install.lib.InstallUtils;
import com.android.cts.install.lib.TestApp;
import com.android.cts.rollback.lib.Rollback;
import com.android.cts.rollback.lib.RollbackUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.IOException;

/**
 * On-device helper test methods used for host-driven rollback tests.
 */
@RunWith(JUnit4.class)
public class HostTestHelper {
    /**
     * Adopts common permissions needed to test rollbacks.
     */
    @Before
    public void setup() throws InterruptedException, IOException {
        InstallUtils.adoptShellPermissionIdentity(
                    Manifest.permission.INSTALL_PACKAGES,
                    Manifest.permission.DELETE_PACKAGES,
                    Manifest.permission.TEST_MANAGE_ROLLBACKS);
    }

    /**
     * Drops adopted shell permissions.
     */
    @After
    public void teardown() throws InterruptedException, IOException {
        InstallUtils.dropShellPermissionIdentity();
    }

    @Test
    public void testApkRollbackByAnotherInstaller_Phase2() throws Exception {
        RollbackInfo rollbackA = RollbackUtils.waitForAvailableRollback(TestApp.A);
        assertThat(rollbackA).packagesContainsExactly(Rollback.from(TestApp.A2).to(TestApp.A1));
        RollbackUtils.rollback(rollbackA.getRollbackId());
        // TODO(b/127452064): fix the rollback bug and enable the assertion
        // Rollback should fail since TestApp.A is installed by another installer.
        // assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(2);
    }
}
