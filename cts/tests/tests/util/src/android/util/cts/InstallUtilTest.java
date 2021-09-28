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

package android.util.cts;

import static com.google.common.truth.Truth.assertThat;

import android.Manifest;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.platform.test.annotations.AppModeFull;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.install.lib.Install;
import com.android.cts.install.lib.InstallUtils;
import com.android.cts.install.lib.LocalIntentSender;
import com.android.cts.install.lib.TestApp;
import com.android.cts.install.lib.Uninstall;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;

@SmallTest
@RunWith(AndroidJUnit4.class)
@AppModeFull(reason = "Instant apps cannot create installer sessions")
/**
 * Test for cts.install.lib.
 * <p>This test also tries to showcase how to use the library.
 */
public class InstallUtilTest {
    /**
     * Drops adopted shell permissions and uninstalls the test apps.
     */
    @After
    public void teardown() throws InterruptedException, IOException {
        // Good tests clean up after themselves.
        // Remember that other tests will be using the same test apps.
        Uninstall.packages(TestApp.A, TestApp.B);

        InstallUtils.dropShellPermissionIdentity();
    }

    /**
     * Adopts common permissions needed to test rollbacks and uninstalls the
     * test apps.
     */
    @Before
    public void setup() throws InterruptedException, IOException {
        InstallUtils.adoptShellPermissionIdentity(
                    Manifest.permission.INSTALL_PACKAGES,
                    Manifest.permission.DELETE_PACKAGES);
        // Better tests work regardless of whether other tests clean up after themselves or not.
        Uninstall.packages(TestApp.A, TestApp.B);
    }

    /**
     * Asserts that the resource streams have the same content.
     */
    private void assertSameResource(TestApp a, TestApp b) throws Exception {
        try (InputStream is1 = a.getResourceStream(a.getResourceNames()[0]);
             InputStream is2 = b.getResourceStream(b.getResourceNames()[0]);) {
            byte[] buf1 = new byte[64];
            byte[] buf2 = new byte[64];
            while (true) {
                int n1 = is1.read(buf1);
                int n2 = is2.read(buf2);
                assertThat(n1).isEqualTo(n2);
                if (n1 == -1) break;
                assertThat(buf1).isEqualTo(buf2);
            }
        }
    }

    @Test
    public void testNativeFilePathTestApp() throws Exception {
        // Install the apps
        Install.multi(TestApp.A1, TestApp.B2).commit();
        // Create TestApps using the native file path of the installed apps
        TestApp a1 = new TestApp(TestApp.A1.toString(), TestApp.A1.getPackageName(),
                TestApp.A1.getVersionCode(), false,
                new File(InstallUtils.getPackageInfo(TestApp.A).applicationInfo.sourceDir));
        TestApp b2 = new TestApp(TestApp.B2.toString(), TestApp.B2.getPackageName(),
                TestApp.B2.getVersionCode(), false,
                new File(InstallUtils.getPackageInfo(TestApp.B).applicationInfo.sourceDir));

        // Assert that the resource streams have the same content
        assertSameResource(TestApp.A1, a1);
        assertSameResource(TestApp.B2, b2);
        // Verify the TestApp constructor which takes a native file path
        assertThat(a1.toString()).isEqualTo(TestApp.A1.toString());
        assertThat(a1.getPackageName()).isEqualTo(TestApp.A1.getPackageName());
        assertThat(a1.getVersionCode()).isEqualTo(TestApp.A1.getVersionCode());
        assertThat(b2.toString()).isEqualTo(TestApp.B2.toString());
        assertThat(b2.getPackageName()).isEqualTo(TestApp.B2.getPackageName());
        assertThat(b2.getVersionCode()).isEqualTo(TestApp.B2.getVersionCode());
        // Verify TestApps with native file paths can also be installed correctly
        Install.multi(a1, b2).addInstallFlags(PackageManager.INSTALL_REPLACE_EXISTING).commit();
        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(1);
        assertThat(InstallUtils.getInstalledVersion(TestApp.B)).isEqualTo(2);
    }

    @Test
    public void testCommitSingleTestApp() throws Exception {
        // Assert that the test app was not previously installed
        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(-1);
        // Install version 1 of TestApp.A
        // Install#commit() asserts that the installation succeeds, so if it fails,
        // an AssertionError would be thrown.
        Install.single(TestApp.A1).commit();
        // Even though the install session of TestApp.A1 is guaranteed to be committed by this stage
        // it's still good practice to assert that the installed version of the app is the desired
        // one. This is due to the fact that not all committed sessions are finalized sessions, i.e.
        // staged install session.
        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(1);
        // No need to uninstall test app, as #teardown will do the job.
    }

    @Test
    public void testCommitMultiTestApp() throws Exception {
        // Assert that the test app was not previously installed.
        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(-1);
        assertThat(InstallUtils.getInstalledVersion(TestApp.B)).isEqualTo(-1);
        // Install version 1 of TestApp.A and version 2 of TestApp.B in one atomic install.
        // Same notes as the single install case apply in the multi install case.
        Install.multi(TestApp.A1, TestApp.B2).commit();

        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(1);
        assertThat(InstallUtils.getInstalledVersion(TestApp.B)).isEqualTo(2);
    }

    @Test
    public void testOpenAndAbandonSessionForSingleApk() throws Exception {
        int sessionId = Install.single(TestApp.A1).createSession();
        try (PackageInstaller.Session session
                     = InstallUtils.openPackageInstallerSession(sessionId)) {
            assertThat(session).isNotNull();

            // TODO: is there a way to verify that the APK has been written?

            // At this stage, the session can be directly manipulated using
            // PackageInstaller.Session API, i.e., it can be abandoned.
            session.abandon();
            // TODO: maybe add session callback and verify that session was abandoned?
            // Assert session has not been installed
            assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(-1);
        }
    }

    @Test
    public void testOpenAndCommitSessionForSingleApk() throws Exception {
        assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(-1);
        int sessionId = Install.single(TestApp.A1).createSession();
        try (PackageInstaller.Session session
                     = InstallUtils.openPackageInstallerSession(sessionId)) {
            assertThat(session).isNotNull();

            // Session can be committed directly, but a BroadcastReceiver must be provided.
            session.commit(LocalIntentSender.getIntentSender());
            InstallUtils.assertStatusSuccess(LocalIntentSender.getIntentSenderResult());

            // Verify app has been installed
            assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(1);
        }
    }

    @Test
    public void testOpenSingleSessionWithParameters() throws Exception {
        int sessionId = Install.single(TestApp.A1).setStaged().createSession();
        try (PackageInstaller.Session session
                     = InstallUtils.openPackageInstallerSession(sessionId)) {
            assertThat(session.isStaged()).isTrue();

            session.abandon();
        }
    }

    @Test
    public void testOpenSessionForMultiPackageSession() throws Exception {
        int parentSessionId = Install.multi(TestApp.A1, TestApp.B1).setStaged().createSession();
        try (PackageInstaller.Session parentSession
                     = InstallUtils.openPackageInstallerSession(parentSessionId)) {
            assertThat(parentSession.isMultiPackage()).isTrue();

            assertThat(parentSession.isStaged()).isTrue();

            // Child sessions are consistent with the parent parameters
            int[] childSessionIds = parentSession.getChildSessionIds();
            assertThat(childSessionIds.length).isEqualTo(2);
            for (int childSessionId : childSessionIds) {
                try (PackageInstaller.Session childSession
                             = InstallUtils.openPackageInstallerSession(childSessionId)) {
                    assertThat(childSession.isStaged()).isTrue();
                }
            }

            parentSession.abandon();

            // Verify apps have not been installed
            assertThat(InstallUtils.getInstalledVersion(TestApp.A)).isEqualTo(-1);
            assertThat(InstallUtils.getInstalledVersion(TestApp.B)).isEqualTo(-1);
        }
    }

    @Test
    public void testMutateInstallFlags() throws Exception {
        PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(
                PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        params.setInstallAsApex();
        params.setStaged();
        InstallUtils.mutateInstallFlags(params, 0x00080000);
        final Class<?> clazz = params.getClass();
        Field installFlagsField = clazz.getDeclaredField("installFlags");
        int installFlags = installFlagsField.getInt(params);
        assertThat(installFlags & 0x00080000).isEqualTo(0x00080000);
    }
}
