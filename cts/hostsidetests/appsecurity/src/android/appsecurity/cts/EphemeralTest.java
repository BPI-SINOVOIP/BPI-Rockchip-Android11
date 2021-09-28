/*
 * Copyright (C) 2016 The Android Open Source Project
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
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.SecurityTest;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tests for ephemeral packages.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull(reason = "Already handles instant installs when needed")
public class EphemeralTest extends BaseAppSecurityTest {

    // a normally installed application
    private static final String NORMAL_APK = "CtsEphemeralTestsNormalApp.apk";
    private static final String NORMAL_PKG = "com.android.cts.normalapp";

    // the first ephemerally installed application
    private static final String EPHEMERAL_1_APK = "CtsEphemeralTestsEphemeralApp1.apk";
    private static final String EPHEMERAL_1_PKG = "com.android.cts.ephemeralapp1";

    // the second ephemerally installed application
    private static final String EPHEMERAL_2_APK = "CtsEphemeralTestsEphemeralApp2.apk";
    private static final String EPHEMERAL_2_PKG = "com.android.cts.ephemeralapp2";

    // a normally installed application with implicitly exposed components
    private static final String IMPLICIT_APK = "CtsEphemeralTestsImplicitApp.apk";
    private static final String IMPLICIT_PKG = "com.android.cts.implicitapp";

    // a normally installed application with no exposed components
    private static final String UNEXPOSED_APK = "CtsEphemeralTestsUnexposedApp.apk";
    private static final String UNEXPOSED_PKG = "com.android.cts.unexposedapp";

    // an application that gets upgraded from 'instant' to 'full'
    private static final String UPGRADED_APK = "CtsInstantUpgradeApp.apk";
    private static final String UPGRADED_PKG = "com.android.cts.instantupgradeapp";

    private static final String TEST_CLASS = ".ClientTest";
    private static final String WEBVIEW_TEST_CLASS = ".WebViewTest";

    private static final List<Map<String, String>> EXPECTED_EXPOSED_INTENTS =
            new ArrayList<>();
    static {
        // Framework intents we expect the system to expose to instant applications
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.CHOOSER",
                null, null));
        // Contact intents we expect the system to expose to instant applications
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.PICK", null, "vnd.android.cursor.dir/contact"));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.PICK", null, "vnd.android.cursor.dir/phone_v2"));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.PICK", null, "vnd.android.cursor.dir/email_v2"));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.PICK", null, "vnd.android.cursor.dir/postal-address_v2"));
        // Storage intents we expect the system to expose to instant applications
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.OPEN_DOCUMENT",
                "android.intent.category.OPENABLE", "\\*/\\*"));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.OPEN_DOCUMENT", null, "\\*/\\*"));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.GET_CONTENT",
                "android.intent.category.OPENABLE", "\\*/\\*"));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.GET_CONTENT", null, "\\*/\\*"));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.OPEN_DOCUMENT_TREE", null, null));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.CREATE_DOCUMENT",
                "android.intent.category.OPENABLE", "text/plain"));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.intent.action.CREATE_DOCUMENT", null, "text/plain"));
        /** Camera intents we expect the system to expose to instant applications */
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.media.action.IMAGE_CAPTURE", null, null));
        EXPECTED_EXPOSED_INTENTS.add(makeArgs(
                "android.media.action.VIDEO_CAPTURE", null, null));
    }

    private String mOldVerifierValue;
    private Boolean mIsUnsupportedDevice;

    @Before
    public void setUp() throws Exception {
        mIsUnsupportedDevice = isDeviceUnsupported();
        if (mIsUnsupportedDevice) {
            return;
        }

        Utils.prepareSingleUser(getDevice());
        assertNotNull(getAbi());
        assertNotNull(getBuild());

        uninstallTestPackages();
        installTestPackages();
    }

    @After
    public void tearDown() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        uninstallTestPackages();
    }

    @Test
    public void testNormalQuery() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), NORMAL_PKG, TEST_CLASS, "testQuery");
    }

    @Test
    public void testNormalStartNormal() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), NORMAL_PKG, TEST_CLASS, "testStartNormal");
    }

    @Test
    public void testNormalStartEphemeral() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), NORMAL_PKG, TEST_CLASS,
                "testStartEphemeral");
    }

    @Test
    public void testEphemeralQuery() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS, "testQuery");
    }

    @Test
    public void testEphemeralStartNormal() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartNormal");
    }

    // each connection to an exposed component needs to run in its own test to
    // avoid sharing state. once an instant app is exposed to a component, it's
    // exposed until the device restarts or the instant app is removed.
    @Test
    public void testEphemeralStartExposed01() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed01");
    }
    @Test
    public void testEphemeralStartExposed02() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed02");
    }
    @Test
    public void testEphemeralStartExposed03() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed03");
    }
    @Test
    public void testEphemeralStartExposed04() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed04");
    }
    @Test
    public void testEphemeralStartExposed05() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed05");
    }
    @Test
    public void testEphemeralStartExposed06() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed06");
    }
    @Test
    public void testEphemeralStartExposed07() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed07");
    }
    @Test
    public void testEphemeralStartExposed08() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed08");
    }
    @Test
    public void testEphemeralStartExposed09() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed09");
    }
    @Test
    public void testEphemeralStartExposed10() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed10");
    }
    @Test
    public void testEphemeralStartExposed11() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartExposed11");
    }

    @Test
    public void testEphemeralStartEphemeral() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartEphemeral");
    }

    @Test
    public void testEphemeralGetInstaller01() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        installEphemeralApp(EPHEMERAL_1_APK, "com.android.cts.normalapp");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testGetInstaller01");
    }
    @Test
    public void testEphemeralGetInstaller02() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        installApp(NORMAL_APK, "com.android.cts.normalapp");
        installEphemeralApp(EPHEMERAL_1_APK, "com.android.cts.normalapp");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testGetInstaller02");
    }
    @Test
    public void testEphemeralGetInstaller03() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        installApp(NORMAL_APK, "com.android.cts.normalapp");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testGetInstaller03");
    }

    @Test
    public void testExposedSystemActivities() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        for (Map<String, String> testArgs : EXPECTED_EXPOSED_INTENTS) {
            final boolean exposed = isIntentExposed(testArgs);
            if (exposed) {
                Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                        "testExposedActivity", testArgs);
            } else {
                CLog.w("Skip intent; " + dumpArgs(testArgs));
            }
        }
    }

    @Test
    public void testBuildSerialUnknown() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testBuildSerialUnknown");
    }

    @Test
    public void testPackageInfo() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testPackageInfo");
    }

    @Test
    public void testActivityInfo() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testActivityInfo");
    }

    @Test
    public void testWebViewLoads() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, WEBVIEW_TEST_CLASS,
                "testWebViewLoads");
    }

    @Test
    public void testInstallPermissionNotGranted() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testInstallPermissionNotGranted");
    }

    @Test
    public void testInstallPermissionGranted() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testInstallPermissionGranted");
    }

    @Test
    @SecurityTest(minPatchLevel = "2020-11")
    public void testInstallPermissionNotGrantedInPackageInfo() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testInstallPermissionNotGrantedInPackageInfo");
    }

    @Test
    @SecurityTest(minPatchLevel = "2020-11")
    public void testInstallPermissionGrantedInPackageInfo() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testInstallPermissionGrantedInPackageInfo");
    }

    /** Test for android.permission.INSTANT_APP_FOREGROUND_SERVICE */
    @Test
    public void testStartForegroundService() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        // Make sure the test package does not have INSTANT_APP_FOREGROUND_SERVICE
        getDevice().executeShellCommand("cmd package revoke " + EPHEMERAL_1_PKG
                + " android.permission.INSTANT_APP_FOREGROUND_SERVICE");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testStartForegroundService");
    }

    /** Test for android.permission.RECORD_AUDIO */
    @Test
    public void testRecordAudioPermission() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testRecordAudioPermission");
    }

    /** Test for android.permission.CAMERA */
    @Test
    public void testCameraPermission() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testCameraPermission");
    }

    /** Test for android.permission.READ_PHONE_NUMBERS */
    @Test
    public void testReadPhoneNumbersPermission() throws Exception {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testReadPhoneNumbersPermission");
    }

    /** Test for android.permission.ACCESS_COARSE_LOCATION */
    @Test
    public void testAccessCoarseLocationPermission() throws Throwable {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testAccessCoarseLocationPermission");
    }

    /** Test for android.permission.NETWORK */
    @Test
    public void testInternetPermission() throws Throwable {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testInternetPermission");
    }

    /** Test for android.permission.VIBRATE */
    @Test
    public void testVibratePermission() throws Throwable {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testVibratePermission");
    }

    /** Test for android.permission.WAKE_LOCK */
    @Test
    public void testWakeLockPermission() throws Throwable {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testWakeLockPermission");
    }

    /** Test for search manager */
    @Test
    public void testGetSearchableInfo() throws Throwable {
        if (mIsUnsupportedDevice) {
            return;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), EPHEMERAL_1_PKG, TEST_CLASS,
                "testGetSearchableInfo");
    }

    /** Test for upgrade from instant --> full */
    @Test
    public void testInstantAppUpgrade() throws Throwable {
        if (mIsUnsupportedDevice) {
            return;
        }
        installEphemeralApp(UPGRADED_APK);
        Utils.runDeviceTestsAsCurrentUser(getDevice(), UPGRADED_PKG, TEST_CLASS,
                "testInstantApplicationWritePreferences");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), UPGRADED_PKG, TEST_CLASS,
                "testInstantApplicationWriteFile");
        installFullApp(UPGRADED_APK);
        Utils.runDeviceTestsAsCurrentUser(getDevice(), UPGRADED_PKG, TEST_CLASS,
                "testFullApplicationReadPreferences");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), UPGRADED_PKG, TEST_CLASS,
                "testFullApplicationReadFile");
    }

    private static final HashMap<String, String> makeArgs(
            String action, String category, String mimeType) {
        if (action == null || action.length() == 0) {
            fail("action not specified");
        }
        final HashMap<String, String> testArgs = new HashMap<>();
        testArgs.put("action", action);
        if (category != null && category.length() > 0) {
            testArgs.put("category", category);
        }
        if (mimeType != null && mimeType.length() > 0) {
            testArgs.put("mime_type", mimeType);
        }
        return testArgs;
    }

    private static final String[] sUnsupportedFeatures = {
            "feature:android.hardware.type.watch",
            "android.hardware.type.embedded",
    };
    private boolean isDeviceUnsupported() throws Exception {
        for (String unsupportedFeature : sUnsupportedFeatures) {
            if (getDevice().hasFeature(unsupportedFeature)) {
                return true;
            }
        }
        return false;
    }

    private boolean isIntentExposed(Map<String, String> testArgs)
            throws Exception {
        final StringBuffer command = new StringBuffer();
        command.append("cmd package query-activities");
        command.append(" -a ").append(testArgs.get("action"));
        if (testArgs.get("category") != null) {
            command.append(" -c ").append(testArgs.get("category"));
        }
        if (testArgs.get("mime_type") != null) {
            command.append(" -t ").append(testArgs.get("mime_type"));
        }
        final String output = getDevice().executeShellCommand(command.toString()).trim();
        return !"No activities found".equals(output);
    }

    private static final String dumpArgs(Map<String, String> testArgs) {
        final StringBuffer dump = new StringBuffer();
        dump.append("action: ").append(testArgs.get("action"));
        if (testArgs.get("category") != null) {
            dump.append(", category: ").append(testArgs.get("category"));
        }
        if (testArgs.get("mime_type") != null) {
            dump.append(", type: ").append(testArgs.get("mime_type"));
        }
        return dump.toString();
    }

    private void installApp(String apk) throws Exception {
        new InstallMultiple(false /* instant */)
                .addFile(apk)
                .run();
    }

    private void installApp(String apk, String installer) throws Exception {
        new InstallMultiple(false /* instant */)
                .addFile(apk)
                .addArg("-i " + installer)
                .run();
    }

    private void installEphemeralApp(String apk) throws Exception {
        new InstallMultiple(true /* instant */)
                .addFile(apk)
                .run();
    }

    private void installEphemeralApp(String apk, String installer) throws Exception {
        new InstallMultiple(true /* instant */)
                .addFile(apk)
                .addArg("-i " + installer)
                .run();
    }

    private void installFullApp(String apk) throws Exception {
        new InstallMultiple(false /* instant */)
                .addFile(apk)
                .addArg("--full")
                .run();
    }

    private void installTestPackages() throws Exception {
        installApp(NORMAL_APK);
        installApp(UNEXPOSED_APK);
        installApp(IMPLICIT_APK);

        installEphemeralApp(EPHEMERAL_1_APK);
        installEphemeralApp(EPHEMERAL_2_APK);
    }

    private void uninstallTestPackages() throws Exception {
        uninstallPackage(NORMAL_PKG);
        uninstallPackage(UNEXPOSED_PKG);
        uninstallPackage(IMPLICIT_PKG);
        uninstallPackage(EPHEMERAL_1_PKG);
        uninstallPackage(EPHEMERAL_2_PKG);
        uninstallPackage(UPGRADED_PKG);
    }
}
