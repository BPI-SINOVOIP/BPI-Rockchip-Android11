/*
 * Copyright (C) 2017 The Android Open Source Project
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
package android.appsecurity.cts;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import android.platform.test.annotations.AppModeFull;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull(reason = "Overlays cannot be instant apps")
public class OverlayHostTest extends BaseAppSecurityTest {

    // Test applications
    private static final String TARGET_OVERLAYABLE_APK = "CtsOverlayTarget.apk";
    private static final String TARGET_NO_OVERLAYABLE_APK = "CtsOverlayTargetNoOverlayable.apk";

    private static final String OVERLAY_ANDROID_APK = "CtsOverlayAndroid.apk";
    private static final String OVERLAY_ALL_APK = "CtsOverlayPolicyAll.apk";
    private static final String OVERLAY_ALL_NO_NAME_APK = "CtsOverlayPolicyAllNoName.apk";
    private static final String OVERLAY_ALL_NO_NAME_DIFFERENT_CERT_APK =
            "CtsOverlayPolicyAllNoNameDifferentCert.apk";
    private static final String OVERLAY_ALL_PIE_APK = "CtsOverlayPolicyAllPie.apk";
    private static final String OVERLAY_PRODUCT_APK = "CtsOverlayPolicyProduct.apk";
    private static final String OVERLAY_SYSTEM_APK = "CtsOverlayPolicySystem.apk";
    private static final String OVERLAY_VENDOR_APK = "CtsOverlayPolicyVendor.apk";
    private static final String OVERLAY_DIFFERENT_SIGNATURE_APK = "CtsOverlayPolicySignatureDifferent.apk";

    // Test application package names
    private static final String TARGET_PACKAGE = "com.android.cts.overlay.target";
    private static final String OVERLAY_ANDROID_PACKAGE = "com.android.cts.overlay.android";
    private static final String OVERLAY_ALL_PACKAGE = "com.android.cts.overlay.all";
    private static final String OVERLAY_PRODUCT_PACKAGE = "com.android.cts.overlay.policy.product";
    private static final String OVERLAY_SYSTEM_PACKAGE = "com.android.cts.overlay.policy.system";
    private static final String OVERLAY_VENDOR_PACKAGE = "com.android.cts.overlay.policy.vendor";
    private static final String OVERLAY_DIFFERENT_SIGNATURE_PACKAGE = "com.android.cts.overlay.policy.signature";

    // Test application test class
    private static final String TEST_APP_APK = "CtsOverlayApp.apk";
    private static final String TEST_APP_PACKAGE = "com.android.cts.overlay.app";
    private static final String TEST_APP_CLASS = "com.android.cts.overlay.app.OverlayableTest";

    // Overlay states
    private static final String STATE_DISABLED = "STATE_DISABLED";
    private static final String STATE_ENABLED = "STATE_ENABLED";
    private static final String STATE_NO_IDMAP = "STATE_NO_IDMAP";

    private static final long OVERLAY_WAIT_TIMEOUT = 10000; // 10 seconds

    @Before
    public void setUp() throws Exception {
        new InstallMultiple().addFile(TEST_APP_APK).run();
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(TEST_APP_PACKAGE);
    }

    private String getStateForOverlay(String overlayPackage) throws Exception {
        String result = getDevice().executeShellCommand("cmd overlay dump");

        String overlayPackageForCurrentUser = overlayPackage + ":" + getDevice().getCurrentUser();

        int startIndex = result.indexOf(overlayPackageForCurrentUser);
        if (startIndex < 0) {
            return null;
        }

        int endIndex = result.indexOf('}', startIndex);
        assertTrue(endIndex > startIndex);

        int stateIndex = result.indexOf("mState", startIndex);
        assertTrue(startIndex < stateIndex && stateIndex < endIndex);

        int colonIndex = result.indexOf(':', stateIndex);
        assertTrue(stateIndex < colonIndex && colonIndex < endIndex);

        int endLineIndex = result.indexOf('\n', colonIndex);
        assertTrue(colonIndex < endLineIndex && endLineIndex < endIndex);
        return result.substring(colonIndex + 2, endLineIndex);
    }

    private void waitForOverlayState(String overlayPackage, String state) throws Exception {
        boolean overlayFound = false;
        long startTime = System.currentTimeMillis();

        while (!overlayFound && (System.currentTimeMillis() - startTime < OVERLAY_WAIT_TIMEOUT)) {
            String result = getStateForOverlay(overlayPackage);
            overlayFound = state.equals(result);
        }

        assertTrue(overlayFound);
    }

    private void assertFailToGenerateIdmap(String overlayApk, String overlayPackage)
            throws Exception {
        try {
            getDevice().uninstallPackage(TARGET_PACKAGE);
            getDevice().uninstallPackage(overlayPackage);
            assertFalse(getDevice().getInstalledPackageNames().contains(OVERLAY_ALL_PACKAGE));
            assertFalse(getDevice().getInstalledPackageNames().contains(overlayPackage));

            new InstallMultiple().addFile(TARGET_OVERLAYABLE_APK).run();
            new InstallMultiple().addFile(overlayApk).run();

            waitForOverlayState(overlayPackage, STATE_NO_IDMAP);
            getDevice().executeShellCommand("cmd overlay enable  --user current " + overlayPackage);
            waitForOverlayState(overlayPackage, STATE_NO_IDMAP);
        } finally {
            getDevice().uninstallPackage(TARGET_PACKAGE);
            getDevice().uninstallPackage(overlayPackage);
        }
    }

    private void runOverlayDeviceTest(String targetApk, String overlayApk, String overlayPackage,
            String testMethod)
            throws Exception {
        try {
            getDevice().uninstallPackage(TARGET_PACKAGE);
            getDevice().uninstallPackage(overlayPackage);
            assertFalse(getDevice().getInstalledPackageNames().contains(TARGET_PACKAGE));
            assertFalse(getDevice().getInstalledPackageNames().contains(overlayPackage));

            new InstallMultiple().addFile(overlayApk).run();
            new InstallMultiple().addFile(targetApk).run();

            waitForOverlayState(overlayPackage, STATE_DISABLED);
            getDevice().executeShellCommand("cmd overlay enable --user current " + overlayPackage);
            waitForOverlayState(overlayPackage, STATE_ENABLED);

            runDeviceTests(TEST_APP_PACKAGE, TEST_APP_CLASS, testMethod, false /* instant */);
        } finally {
            getDevice().uninstallPackage(TARGET_PACKAGE);
            getDevice().uninstallPackage(overlayPackage);
        }
    }

    /**
     * Overlays that target android and are not signed with the platform signature must not be
     * installed successfully.
     */
    @Test
    public void testCannotInstallTargetAndroidNotPlatformSigned() throws Exception {
        try {
            getDevice().uninstallPackage(OVERLAY_ANDROID_PACKAGE);
            assertFalse(getDevice().getInstalledPackageNames().contains(OVERLAY_ANDROID_PACKAGE));

            // Try to install the overlay, but expect an error.
            new InstallMultiple().addFile(OVERLAY_ANDROID_APK).runExpectingFailure();

            // The install should have failed.
            assertFalse(getDevice().getInstalledPackageNames().contains(OVERLAY_ANDROID_PACKAGE));

            // The package of the installed overlay should not appear in the overlay manager list.
            assertFalse(getDevice().executeShellCommand("cmd overlay list --user current ")
                    .contains(" " + OVERLAY_ANDROID_PACKAGE + "\n"));
        } finally {
            getDevice().uninstallPackage(OVERLAY_ANDROID_PACKAGE);
        }
    }

    /**
     * Overlays that target a pre-Q sdk and that are not signed with the platform signature must not
     * be installed.
     **/
    @Test
    public void testCannotInstallPieOverlayNotPlatformSigned() throws Exception {
        try {
            getDevice().uninstallPackage(OVERLAY_ALL_PACKAGE);
            assertFalse(getDevice().getInstalledPackageNames().contains(OVERLAY_ALL_PACKAGE));

            // Try to install the overlay, but expect an error.
            new InstallMultiple().addFile(OVERLAY_ALL_PIE_APK).runExpectingFailure();

            // The install should have failed.
            assertFalse(getDevice().getInstalledPackageNames().contains(OVERLAY_ALL_PACKAGE));

            // The package of the installed overlay should not appear in the overlay manager list.
            assertFalse(getDevice().executeShellCommand("cmd overlay list")
                    .contains(" " + OVERLAY_ALL_PACKAGE + "\n"));
        } finally {
            getDevice().uninstallPackage(OVERLAY_ALL_PACKAGE);
        }
    }

    /**
     * Overlays that target Q or higher, that do not specify an android:targetName, and that are
     * not signed with the same signature as the target package must not be installed.
     **/
    @Test
    public void testCannotInstallDifferentSignaturesNoName() throws Exception {
        try {
            getDevice().uninstallPackage(TARGET_PACKAGE);
            getDevice().uninstallPackage(OVERLAY_ALL_PACKAGE);
            assertFalse(getDevice().getInstalledPackageNames().contains(TARGET_PACKAGE));
            assertFalse(getDevice().getInstalledPackageNames().contains(OVERLAY_ALL_PACKAGE));

            // Try to install the overlay, but expect an error.
            new InstallMultiple().addFile(TARGET_NO_OVERLAYABLE_APK).run();
            new InstallMultiple().addFile(
                    OVERLAY_ALL_NO_NAME_DIFFERENT_CERT_APK).runExpectingFailure();

            // The install should have failed.
            assertFalse(getDevice().getInstalledPackageNames().contains(OVERLAY_ALL_PACKAGE));

            // The package of the installed overlay should not appear in the overlay manager list.
            assertFalse(getDevice().executeShellCommand("cmd overlay list --user current")
                    .contains(" " + OVERLAY_ALL_PACKAGE + "\n"));
        } finally {
            getDevice().uninstallPackage(OVERLAY_ALL_PACKAGE);
        }
    }

    /**
     * Overlays that target Q or higher, that do not specify an android:targetName, and are
     * installed before the target must not be allowed to successfully generate an idmap if the
     * overlay is not signed with the same signature as the target package.
     **/
    @Test
    public void testFailIdmapDifferentSignaturesNoName() throws Exception {
        assertFailToGenerateIdmap(OVERLAY_ALL_NO_NAME_APK, OVERLAY_ALL_PACKAGE);
    }

    /**
     * Overlays that target Q or higher, that do not specify an android:targetName, and are
     * installed before the target must be allowed to successfully generate an idmap if the
     * overlay is signed with the same signature as the target package.
     **/
    @Test
    public void testSameSignatureNoOverlayableSucceeds() throws Exception {
        String testMethod = "testSameSignatureNoOverlayableSucceeds";
        runOverlayDeviceTest(TARGET_NO_OVERLAYABLE_APK, OVERLAY_ALL_NO_NAME_APK,
                OVERLAY_ALL_PACKAGE, testMethod);
    }

    /**
     * Overlays installed on the data partition may only overlay resources defined under the public
     * and signature policies if the overlay is signed with the same signature as the target.
     */
    @Test
    public void testOverlayPolicyAll() throws Exception {
        String testMethod = "testOverlayPolicyAll";
        runOverlayDeviceTest(TARGET_OVERLAYABLE_APK, OVERLAY_ALL_APK, OVERLAY_ALL_PACKAGE,
                testMethod);
    }

    @Test
    public void testOverlayPolicyAllNoNameFails() throws Exception {
        assertFailToGenerateIdmap(OVERLAY_ALL_NO_NAME_APK, OVERLAY_ALL_PACKAGE);
    }

    @Test
    public void testOverlayPolicyProductFails() throws Exception {
        assertFailToGenerateIdmap(OVERLAY_PRODUCT_APK, OVERLAY_PRODUCT_PACKAGE);
    }

    @Test
    public void testOverlayPolicySystemFails() throws Exception {
        assertFailToGenerateIdmap(OVERLAY_SYSTEM_APK, OVERLAY_SYSTEM_PACKAGE);
    }

    @Test
    public void testOverlayPolicyVendorFails() throws Exception {
        assertFailToGenerateIdmap(OVERLAY_VENDOR_APK, OVERLAY_VENDOR_PACKAGE);
    }

    @Test
    public void testOverlayPolicyDifferentSignatureFails() throws Exception {
        assertFailToGenerateIdmap(OVERLAY_DIFFERENT_SIGNATURE_APK,
                OVERLAY_DIFFERENT_SIGNATURE_PACKAGE);
    }

    @Test
    public void testFrameworkDoesNotDefineOverlayable() throws Exception {
        String testMethod = "testFrameworkDoesNotDefineOverlayable";
        runDeviceTests(TEST_APP_PACKAGE, TEST_APP_CLASS, testMethod, false /* instant */);
    }

    /** Overlays must not overlay assets. */
    @Test
    public void testCannotOverlayAssets() throws Exception {
        String testMethod = "testCannotOverlayAssets";
        runOverlayDeviceTest(TARGET_OVERLAYABLE_APK, OVERLAY_ALL_APK, OVERLAY_ALL_PACKAGE,
                testMethod);
    }
}
