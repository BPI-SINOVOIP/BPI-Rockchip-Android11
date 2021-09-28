/*
 * Copyright (C) 2014 The Android Open Source Project
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

import static org.junit.Assert.assertNotNull;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashMap;

/**
 * Tests that verify installing of various split APKs from host side.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class SplitTests extends BaseAppSecurityTest {
    static final String PKG_NO_RESTART = "com.android.cts.norestart";
    static final String APK_NO_RESTART_BASE = "CtsNoRestartBase.apk";
    static final String APK_NO_RESTART_FEATURE = "CtsNoRestartFeature.apk";

    static final String APK_NEED_SPLIT_BASE = "CtsNeedSplitApp.apk";
    static final String APK_NEED_SPLIT_FEATURE = "CtsNeedSplitFeature.apk";
    static final String APK_NEED_SPLIT_CONFIG = "CtsNeedSplitApp_xxhdpi-v4.apk";

    static final String PKG = "com.android.cts.splitapp";
    static final String CLASS = PKG + ".SplitAppTest";

    static final String APK = "CtsSplitApp.apk";

    static final String APK_mdpi = "CtsSplitApp_mdpi-v4.apk";
    static final String APK_hdpi = "CtsSplitApp_hdpi-v4.apk";
    static final String APK_xhdpi = "CtsSplitApp_xhdpi-v4.apk";
    static final String APK_xxhdpi = "CtsSplitApp_xxhdpi-v4.apk";

    private static final String APK_v7 = "CtsSplitApp_v7.apk";
    private static final String APK_fr = "CtsSplitApp_fr.apk";
    private static final String APK_de = "CtsSplitApp_de.apk";

    private static final String APK_x86 = "CtsSplitApp_x86.apk";
    private static final String APK_x86_64 = "CtsSplitApp_x86_64.apk";
    private static final String APK_armeabi_v7a = "CtsSplitApp_armeabi-v7a.apk";
    private static final String APK_armeabi = "CtsSplitApp_armeabi.apk";
    private static final String APK_arm64_v8a = "CtsSplitApp_arm64-v8a.apk";
    private static final String APK_mips64 = "CtsSplitApp_mips64.apk";
    private static final String APK_mips = "CtsSplitApp_mips.apk";

    private static final String APK_DIFF_REVISION = "CtsSplitAppDiffRevision.apk";
    private static final String APK_DIFF_REVISION_v7 = "CtsSplitAppDiffRevision_v7.apk";

    private static final String APK_DIFF_VERSION = "CtsSplitAppDiffVersion.apk";
    private static final String APK_DIFF_VERSION_v7 = "CtsSplitAppDiffVersion_v7.apk";

    private static final String APK_DIFF_CERT = "CtsSplitAppDiffCert.apk";
    private static final String APK_DIFF_CERT_v7 = "CtsSplitAppDiffCert_v7.apk";

    private static final String APK_FEATURE = "CtsSplitAppFeature.apk";
    private static final String APK_FEATURE_v7 = "CtsSplitAppFeature_v7.apk";

    static final HashMap<String, String> ABI_TO_APK = new HashMap<>();

    static {
        ABI_TO_APK.put("x86", APK_x86);
        ABI_TO_APK.put("x86_64", APK_x86_64);
        ABI_TO_APK.put("armeabi-v7a", APK_armeabi_v7a);
        ABI_TO_APK.put("armeabi", APK_armeabi);
        ABI_TO_APK.put("arm64-v8a", APK_arm64_v8a);
        ABI_TO_APK.put("mips64", APK_mips64);
        ABI_TO_APK.put("mips", APK_mips);
    }

    @Before
    public void setUp() throws Exception {
        Utils.prepareSingleUser(getDevice());
        getDevice().uninstallPackage(PKG);
        getDevice().uninstallPackage(PKG_NO_RESTART);
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(PKG);
        getDevice().uninstallPackage(PKG_NO_RESTART);
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testSingleBase_full() throws Exception {
        testSingleBase(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testSingleBase_instant() throws Exception {
        testSingleBase(true);
    }
    private void testSingleBase(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).run();
        runDeviceTests(PKG, CLASS, "testSingleBase");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDensitySingle_full() throws Exception {
        testDensitySingle(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDensitySingle_instant() throws Exception {
        testDensitySingle(true);
    }
    private void testDensitySingle(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_mdpi).run();
        runDeviceTests(PKG, CLASS, "testDensitySingle");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDensityAll_full() throws Exception {
        testDensityAll(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDensityAll_instant() throws Exception {
        testDensityAll(true);
    }
    private void testDensityAll(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_mdpi).addFile(APK_hdpi).addFile(APK_xhdpi)
                .addFile(APK_xxhdpi).run();
        runDeviceTests(PKG, CLASS, "testDensityAll");
    }

    /**
     * Install first with low-resolution resources, then add a split that offers
     * higher-resolution resources.
     */
    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDensityBest_full() throws Exception {
        testDensityBest(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDensityBest_instant() throws Exception {
        testDensityBest(true);
    }
    private void testDensityBest(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_mdpi).run();
        runDeviceTests(PKG, CLASS, "testDensityBest1");

        // Now splice in an additional split which offers better resources
        new InstallMultiple(instant).inheritFrom(PKG).addFile(APK_xxhdpi).run();
        runDeviceTests(PKG, CLASS, "testDensityBest2");
    }

    /**
     * Verify that an API-based split can change enabled/disabled state of
     * manifest elements.
     */
    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testApi_full() throws Exception {
        testApi(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testApi_instant() throws Exception {
        testApi(true);
    }
    private void testApi(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_v7).run();
        runDeviceTests(PKG, CLASS, "testApi");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testLocale_full() throws Exception {
        testLocale(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testLocale_instant() throws Exception {
        testLocale(true);
    }
    private void testLocale(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_de).addFile(APK_fr).run();
        runDeviceTests(PKG, CLASS, "testLocale");
    }

    /**
     * Install test app with <em>single</em> split that exactly matches the
     * currently active ABI. This also explicitly forces ABI when installing.
     */
    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testNativeSingle_full() throws Exception {
        testNativeSingle(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testNativeSingle_instant() throws Exception {
        testNativeSingle(true);
    }
    private void testNativeSingle(boolean instant) throws Exception {
        final String abi = getAbi().getName();
        final String apk = ABI_TO_APK.get(abi);
        assertNotNull("Failed to find APK for ABI " + abi, apk);

        new InstallMultiple(instant).addFile(APK).addFile(apk).run();
        runDeviceTests(PKG, CLASS, "testNative");
    }

    /**
     * Install test app with <em>single</em> split that exactly matches the
     * currently active ABI. This variant <em>does not</em> force the ABI when
     * installing, instead exercising the system's ability to choose the ABI
     * through inspection of the installed app.
     */
    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testNativeSingleNatural_full() throws Exception {
        testNativeSingleNatural(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testNativeSingleNatural_instant() throws Exception {
        testNativeSingleNatural(true);
    }
    private void testNativeSingleNatural(boolean instant) throws Exception {
        final String abi = getAbi().getName();
        final String apk = ABI_TO_APK.get(abi);
        assertNotNull("Failed to find APK for ABI " + abi, apk);

        new InstallMultiple(instant).useNaturalAbi().addFile(APK).addFile(apk).run();
        runDeviceTests(PKG, CLASS, "testNative");
    }

    /**
     * Install test app with <em>all</em> possible ABI splits. This also
     * explicitly forces ABI when installing.
     */
    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testNativeAll_full() throws Exception {
        testNativeAll(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testNativeAll_instant() throws Exception {
        testNativeAll(true);
    }
    private void testNativeAll(boolean instant) throws Exception {
        final InstallMultiple inst = new InstallMultiple(instant).addFile(APK);
        for (String apk : ABI_TO_APK.values()) {
            inst.addFile(apk);
        }
        inst.run();
        runDeviceTests(PKG, CLASS, "testNative");
    }

    /**
     * Install test app with <em>all</em> possible ABI splits. This variant
     * <em>does not</em> force the ABI when installing, instead exercising the
     * system's ability to choose the ABI through inspection of the installed
     * app.
     */
    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testNativeAllNatural_full() throws Exception {
        testNativeAllNatural(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testNativeAllNatural_instant() throws Exception {
        testNativeAllNatural(true);
    }
    private void testNativeAllNatural(boolean instant) throws Exception {
        final InstallMultiple inst = new InstallMultiple(instant).useNaturalAbi().addFile(APK);
        for (String apk : ABI_TO_APK.values()) {
            inst.addFile(apk);
        }
        inst.run();
        runDeviceTests(PKG, CLASS, "testNative");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDuplicateBase_full() throws Exception {
        testDuplicateBase(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDuplicateBase_instant() throws Exception {
        testDuplicateBase(true);
    }
    private void testDuplicateBase(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK).runExpectingFailure();
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDuplicateSplit_full() throws Exception {
        testDuplicateSplit(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDuplicateSplit_instant() throws Exception {
        testDuplicateSplit(true);
    }
    private void testDuplicateSplit(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_v7).addFile(APK_v7).runExpectingFailure();
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDiffCert_full() throws Exception {
        testDiffCert(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDiffCert_instant() throws Exception {
        testDiffCert(true);
    }
    private void testDiffCert(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_DIFF_CERT_v7).runExpectingFailure();
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDiffCertInherit_full() throws Exception {
        testDiffCertInherit(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDiffCertInherit_instant() throws Exception {
        testDiffCertInherit(true);
    }
    private void testDiffCertInherit(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).run();
        new InstallMultiple(instant).inheritFrom(PKG).addFile(APK_DIFF_CERT_v7).runExpectingFailure();
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDiffVersion_full() throws Exception {
        testDiffVersion(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDiffVersion_instant() throws Exception {
        testDiffVersion(true);
    }
    private void testDiffVersion(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_DIFF_VERSION_v7).runExpectingFailure();
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDiffVersionInherit_full() throws Exception {
        testDiffVersionInherit(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDiffVersionInherit_instant() throws Exception {
        testDiffVersionInherit(true);
    }
    private void testDiffVersionInherit(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).run();
        new InstallMultiple(instant).inheritFrom(PKG).addFile(APK_DIFF_VERSION_v7).runExpectingFailure();
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDiffRevision_full() throws Exception {
        testDiffRevision(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDiffRevision_instant() throws Exception {
        testDiffRevision(true);
    }
    private void testDiffRevision(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_DIFF_REVISION_v7).run();
        runDeviceTests(PKG, CLASS, "testRevision0_12");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDiffRevisionInheritBase_full() throws Exception {
        testDiffRevisionInheritBase(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDiffRevisionInheritBase_instant() throws Exception {
        testDiffRevisionInheritBase(true);
    }
    private void testDiffRevisionInheritBase(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_v7).run();
        runDeviceTests(PKG, CLASS, "testRevision0_0");
        new InstallMultiple(instant).inheritFrom(PKG).addFile(APK_DIFF_REVISION_v7).run();
        runDeviceTests(PKG, CLASS, "testRevision0_12");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDiffRevisionInheritSplit_full() throws Exception {
        testDiffRevisionInheritSplit(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDiffRevisionInheritSplit_instant() throws Exception {
        testDiffRevisionInheritSplit(true);
    }
    private void testDiffRevisionInheritSplit(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_v7).run();
        runDeviceTests(PKG, CLASS, "testRevision0_0");
        new InstallMultiple(instant).inheritFrom(PKG).addFile(APK_DIFF_REVISION).run();
        runDeviceTests(PKG, CLASS, "testRevision12_0");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testDiffRevisionDowngrade_full() throws Exception {
        testDiffRevisionDowngrade(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testDiffRevisionDowngrade_instant() throws Exception {
        testDiffRevisionDowngrade(true);
    }
    private void testDiffRevisionDowngrade(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_DIFF_REVISION_v7).run();
        new InstallMultiple(instant).inheritFrom(PKG).addFile(APK_v7).runExpectingFailure();
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testFeatureBase_full() throws Exception {
        testFeatureBase(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testFeatureBase_instant() throws Exception {
        testFeatureBase(true);
    }
    private void testFeatureBase(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_FEATURE).run();
        runDeviceTests(PKG, CLASS, "testFeatureBase");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testFeatureApi_full() throws Exception {
        testFeatureApi(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testFeatureApi_instant() throws Exception {
        testFeatureApi(true);
    }
    private void testFeatureApi(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).addFile(APK_FEATURE).addFile(APK_FEATURE_v7).run();
        runDeviceTests(PKG, CLASS, "testFeatureApi");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInheritUpdatedBase() throws Exception {
        // TODO: flesh out this test
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInheritUpdatedSplit() throws Exception {
        // TODO: flesh out this test
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testFeatureWithoutRestart_full() throws Exception {
        testFeatureWithoutRestart(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testFeatureWithoutRestart_instant() throws Exception {
        testFeatureWithoutRestart(true);
    }
    private void testFeatureWithoutRestart(boolean instant) throws Exception {
        // always install as a full app; we're testing that the instant app can be
        // updated without restarting and need a broadcast receiver to ensure the
        // correct behaviour. So, this component must be visible to instant apps.
        new InstallMultiple().addFile(APK).run();

        new InstallMultiple(instant).addFile(APK_NO_RESTART_BASE).run();
        runDeviceTests(PKG, CLASS, "testBaseInstalled", instant);
        new InstallMultiple(instant)
                .addArg("--dont-kill")
                .inheritFrom(PKG_NO_RESTART)
                .addFile(APK_NO_RESTART_FEATURE)
                .run();
        runDeviceTests(PKG, CLASS, "testFeatureInstalled", instant);
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testRequiredSplitMissing_full() throws Exception {
        testSingleBase(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testRequiredSplitMissing_instant() throws Exception {
        testSingleBase(true);
    }
    private void testRequiredSplitMissing(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_NEED_SPLIT_BASE)
                .runExpectingFailure("INSTALL_FAILED_MISSING_SPLIT");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testRequiredSplitInstalledFeature_full() throws Exception {
        testSingleBase(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testRequiredSplitInstalledFeature_instant() throws Exception {
        testSingleBase(true);
    }
    private void testRequiredSplitInstalledFeature(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_NEED_SPLIT_BASE).addFile(APK_NEED_SPLIT_FEATURE)
                .run();
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testRequiredSplitInstalledConfig_full() throws Exception {
        testSingleBase(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testRequiredSplitInstalledConfig_instant() throws Exception {
        testSingleBase(true);
    }
    private void testRequiredSplitInstalledConfig(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_NEED_SPLIT_BASE).addFile(APK_NEED_SPLIT_CONFIG)
                .run();
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testRequiredSplitRemoved_full() throws Exception {
        testSingleBase(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testRequiredSplitRemoved_instant() throws Exception {
        testSingleBase(true);
    }
    private void testRequiredSplitRemoved(boolean instant) throws Exception {
        // start with a base and two splits
        new InstallMultiple(instant)
                .addFile(APK_NEED_SPLIT_BASE)
                .addFile(APK_NEED_SPLIT_FEATURE)
                .addFile(APK_NEED_SPLIT_CONFIG)
                .run();
        // it's okay to remove one of the splits
        new InstallMultiple(instant).inheritFrom(PKG).removeSplit("split_feature").run();
        // but, not to remove all of them
        new InstallMultiple(instant).inheritFrom(PKG).removeSplit("split_config.xxhdpi")
                .runExpectingFailure("INSTALL_FAILED_MISSING_SPLIT");
    }

    /**
     * Verify that installing a new version of app wipes code cache.
     */
    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testClearCodeCache_full() throws Exception {
        testClearCodeCache(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testClearCodeCache_instant() throws Exception {
        testClearCodeCache(true);
    }
    private void testClearCodeCache(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK).run();
        runDeviceTests(PKG, CLASS, "testCodeCacheWrite");
        new InstallMultiple(instant).addArg("-r").addFile(APK_DIFF_VERSION).run();
        runDeviceTests(PKG, CLASS, "testCodeCacheRead");
    }
}
