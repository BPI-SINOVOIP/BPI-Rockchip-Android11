/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
package android.appsecurity.cts;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class IsolatedSplitsTests extends BaseAppSecurityTest {
    private static final String PKG = "com.android.cts.isolatedsplitapp";
    private static final String TEST_CLASS = PKG + ".SplitAppTest";

    /* The feature hierarchy looks like this:

        APK_BASE <- APK_FEATURE_A <- APK_FEATURE_B
            ^------ APK_FEATURE_C

     */
    private static final String APK_BASE = "CtsIsolatedSplitApp.apk";
    private static final String APK_BASE_pl = "CtsIsolatedSplitApp_pl.apk";
    private static final String APK_FEATURE_A = "CtsIsolatedSplitAppFeatureA.apk";
    private static final String APK_FEATURE_A_pl = "CtsIsolatedSplitAppFeatureA_pl.apk";
    private static final String APK_FEATURE_B = "CtsIsolatedSplitAppFeatureB.apk";
    private static final String APK_FEATURE_B_pl = "CtsIsolatedSplitAppFeatureB_pl.apk";
    private static final String APK_FEATURE_C = "CtsIsolatedSplitAppFeatureC.apk";
    private static final String APK_FEATURE_C_pl = "CtsIsolatedSplitAppFeatureC_pl.apk";

    @Before
    public void setUp() throws Exception {
        Utils.prepareSingleUser(getDevice());
        getDevice().uninstallPackage(PKG);
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(PKG);
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallBase_full() throws Exception {
        testInstallBase(false);
    }

    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallBase_instant() throws Exception {
        testInstallBase(true);
    }

    private void testInstallBase(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_BASE).run();
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS, "shouldLoadDefault");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallBaseAndConfigSplit_full() throws Exception {
        testInstallBaseAndConfigSplit(false);
    }

    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallBaseAndConfigSplit_instant() throws Exception {
        testInstallBaseAndConfigSplit(true);
    }

    private void testInstallBaseAndConfigSplit(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_BASE).addFile(APK_BASE_pl).run();
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS, "shouldLoadPolishLocale");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallMissingDependency_full() throws Exception {
        testInstallMissingDependency(false);
    }

    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallMissingDependency_instant() throws Exception {
        testInstallMissingDependency(true);
    }

    private void testInstallMissingDependency(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_BASE).addFile(APK_FEATURE_B).runExpectingFailure();
    }

    @Test
    @AppModeFull(reason = "b/109878606; instant applications can't send broadcasts to manifest "
            + "receivers")
    public void testInstallOneFeatureSplit_full() throws Exception {
        testInstallOneFeatureSplit(false);
    }

    private void testInstallOneFeatureSplit(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_BASE).addFile(APK_FEATURE_A).run();
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS, "shouldLoadDefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureADefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureAReceivers");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallOneFeatureSplitAndConfigSplits_full() throws Exception {
        testInstallOneFeatureSplitAndConfigSplits(false);
    }

    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallOneFeatureSplitAndConfigSplits_instant() throws Exception {
        testInstallOneFeatureSplitAndConfigSplits(true);
    }

    private void testInstallOneFeatureSplitAndConfigSplits(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_BASE).addFile(APK_FEATURE_A).addFile(APK_BASE_pl)
                .addFile(APK_FEATURE_A_pl).run();
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS, "shouldLoadPolishLocale");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureAPolishLocale");
    }

    @Test
    @AppModeFull(reason = "b/109878606; instant applications can't send broadcasts to manifest "
            + "receivers")
    public void testInstallDependentFeatureSplits_full() throws Exception {
        testInstallDependentFeatureSplits(false);
    }

    private void testInstallDependentFeatureSplits(boolean instant) throws Exception {
        new InstallMultiple(instant)
                .addFile(APK_BASE).addFile(APK_FEATURE_A).addFile(APK_FEATURE_B).run();
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS, "shouldLoadDefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureADefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureBDefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureAAndBReceivers");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallDependentFeatureSplitsAndConfigSplits_full() throws Exception {
        testInstallDependentFeatureSplitsAndConfigSplits(false);
    }

    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallDependentFeatureSplitsAndConfigSplits_instant() throws Exception {
        testInstallDependentFeatureSplitsAndConfigSplits(true);
    }

    private void testInstallDependentFeatureSplitsAndConfigSplits(boolean instant)
            throws Exception {
        new InstallMultiple(instant).addFile(APK_BASE).addFile(APK_FEATURE_A).addFile(APK_FEATURE_B)
                .addFile(APK_BASE_pl).addFile(APK_FEATURE_A_pl).addFile(APK_FEATURE_B_pl).run();
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS, "shouldLoadPolishLocale");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureAPolishLocale");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureBPolishLocale");
    }

    @Test
    @AppModeFull(reason = "b/109878606; instant applications can't send broadcasts to manifest "
            + "receivers")
    public void testInstallAllFeatureSplits_full() throws Exception {
        testInstallAllFeatureSplits(false);
    }

    private void testInstallAllFeatureSplits(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_BASE).addFile(APK_FEATURE_A).addFile(APK_FEATURE_B)
                .addFile(APK_FEATURE_C).run();
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS, "shouldLoadDefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureADefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureBDefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureCDefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureAAndBAndCReceivers");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallAllFeatureSplitsAndConfigSplits_full() throws Exception {
        testInstallAllFeatureSplitsAndConfigSplits(false);
    }

    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallAllFeatureSplitsAndConfigSplits_instant() throws Exception {
        testInstallAllFeatureSplitsAndConfigSplits(true);
    }

    private void testInstallAllFeatureSplitsAndConfigSplits(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_BASE).addFile(APK_FEATURE_A).addFile(APK_FEATURE_B)
                .addFile(APK_FEATURE_C).addFile(APK_BASE_pl).addFile(APK_FEATURE_A_pl)
                .addFile(APK_FEATURE_C_pl).run();
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS, "shouldLoadDefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureADefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureBDefault");
        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, TEST_CLASS,
                "shouldLoadFeatureCDefault");
    }
}
