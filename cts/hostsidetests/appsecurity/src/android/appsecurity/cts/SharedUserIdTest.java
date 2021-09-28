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
 * limitations under the License
 */

package android.appsecurity.cts;

import static org.junit.Assert.assertNotNull;

import android.platform.test.annotations.AppModeFull;

import com.android.ddmlib.Log;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Set of tests that verify behavior related to apps with shared user IDs
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class SharedUserIdTest extends BaseAppSecurityTest {

    // testSharedUidDifferentCerts constants
    private static final String SHARED_UI_APK = "CtsSharedUidInstall.apk";
    private static final String SHARED_UI_PKG = "com.android.cts.shareuidinstall";
    private static final String SHARED_UI_DIFF_CERT_APK = "CtsSharedUidInstallDiffCert.apk";
    private static final String SHARED_UI_DIFF_CERT_PKG =
            "com.android.cts.shareuidinstalldiffcert";
    private static final String LOG_TAG = "SharedUserIdTest";
    public static final String SHARED_UI_TEST_CLASS =
            "com.android.cts.shareduidinstallapp.SharedUidPackageTest";
    public static final String SHARED_UI_TEST_METHOD = "testSelfIsOnlyMemberOfSharedUserIdPackages";

    @Before
    public void setUp() throws Exception {
        Utils.prepareSingleUser(getDevice());
        assertNotNull(getBuild());
    }

    /**
     * Test that an app that declares the same shared uid as an existing app, cannot be installed
     * if it is signed with a different certificate.
     */
    @Test
    @AppModeFull(reason = "Instant applications can't define shared UID")
    public void testSharedUidDifferentCerts() throws Exception {
        Log.i(LOG_TAG, "installing apks with shared uid, but different certs");
        try {
            getDevice().uninstallPackage(SHARED_UI_PKG);
            getDevice().uninstallPackage(SHARED_UI_DIFF_CERT_PKG);

            new InstallMultiple().addFile(SHARED_UI_APK).run();
            runDeviceTests(SHARED_UI_PKG, SHARED_UI_TEST_CLASS, SHARED_UI_TEST_METHOD);
            new InstallMultiple().addFile(SHARED_UI_DIFF_CERT_APK)
                    .runExpectingFailure("INSTALL_FAILED_SHARED_USER_INCOMPATIBLE");
            runDeviceTests(SHARED_UI_PKG, SHARED_UI_TEST_CLASS, SHARED_UI_TEST_METHOD);
        } finally {
            getDevice().uninstallPackage(SHARED_UI_PKG);
            getDevice().uninstallPackage(SHARED_UI_DIFF_CERT_PKG);
        }
    }
}
