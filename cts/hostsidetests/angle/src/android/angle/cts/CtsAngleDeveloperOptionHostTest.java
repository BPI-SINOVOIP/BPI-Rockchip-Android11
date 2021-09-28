/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.angle.cts;

import static android.angle.cts.CtsAngleCommon.*;

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests ANGLE Developer Option Opt-In/Out functionality.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class CtsAngleDeveloperOptionHostTest extends BaseHostJUnit4Test {

    private static final String TAG = CtsAngleDeveloperOptionHostTest.class.getSimpleName();

    private void setAndValidateAngleDevOptionPkgDriver(String pkgName, String driverValue) throws Exception {
        CLog.logAndDisplay(LogLevel.INFO, "Updating Global.Settings: pkgName = '" +
                pkgName + "', driverValue = '" + driverValue + "'");

        setGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_PKGS, pkgName);
        setGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_VALUES, driverValue);

        String devOption = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_PKGS);
        Assert.assertEquals(
                "Developer option '" + SETTINGS_GLOBAL_DRIVER_PKGS +
                        "' was not set correctly: '" + devOption + "'",
                pkgName, devOption);

        devOption = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_VALUES);
        Assert.assertEquals(
                "Developer option '" + SETTINGS_GLOBAL_DRIVER_VALUES +
                        "' was not set correctly: '" + devOption + "'",
                driverValue, devOption);
    }

    private void setAndValidatePkgDriver(String pkgName, OpenGlDriverChoice driver) throws Exception {
        stopPackage(getDevice(), pkgName);

        setAndValidateAngleDevOptionPkgDriver(pkgName, sDriverGlobalSettingMap.get(driver));

        startActivity(getDevice(), ANGLE_MAIN_ACTIVTY);

        CLog.logAndDisplay(LogLevel.INFO, "Validating driver selection (" +
                driver + ") with method '" + sDriverTestMethodMap.get(driver) + "'");

        runDeviceTests(pkgName, pkgName + "." + ANGLE_DRIVER_TEST_CLASS,
                sDriverTestMethodMap.get(driver));
    }

    private void installApp(String appName) throws Exception {
        for (int i = 0; i < NUM_ATTEMPTS; i++)
        {
            try {
                installPackage(appName);
                return;
            } catch(Exception e) {
                Thread.sleep(REATTEMPT_SLEEP_MSEC);
            }
        }
    }

    @Before
    public void setUp() throws Exception {
        clearSettings(getDevice());

        stopPackage(getDevice(), ANGLE_DRIVER_TEST_PKG);
        stopPackage(getDevice(), ANGLE_DRIVER_TEST_SEC_PKG);
    }

    @After
    public void tearDown() throws Exception {
        clearSettings(getDevice());
    }

    /**
     * Test ANGLE is loaded when the 'Use ANGLE for all' Developer Option is enabled.
     */
    @Test
    public void testEnableAngleForAll() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);
        installApp(ANGLE_DRIVER_TEST_SEC_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.DEFAULT));
        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_SEC_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.DEFAULT));

        setGlobalSetting(getDevice(), SETTINGS_GLOBAL_ALL_USE_ANGLE, "1");

        runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_ANGLE_METHOD);
        runDeviceTests(ANGLE_DRIVER_TEST_SEC_PKG,
                ANGLE_DRIVER_TEST_SEC_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_ANGLE_METHOD);
    }

    /**
     * Test ANGLE is not loaded when the Developer Option is set to 'default'.
     */
    @Test
    public void testUseDefaultDriver() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.DEFAULT));

        runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_DEFAULT_METHOD);
    }

    /**
     * Test ANGLE is loaded when the Developer Option is set to 'angle'.
     */
    @Test
    public void testUseAngleDriver() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.ANGLE));

        runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_ANGLE_METHOD);
    }

    /**
     * Test ANGLE is not loaded when the Developer Option is set to 'native'.
     */
    @Test
    public void testUseNativeDriver() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.NATIVE));

        runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_NATIVE_METHOD);
    }

    /**
     * Test ANGLE is not loaded for any apps when the Developer Option list lengths mismatch.
     */
    @Test
    public void testSettingsLengthMismatch() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);
        installApp(ANGLE_DRIVER_TEST_SEC_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG + "," +
                        ANGLE_DRIVER_TEST_SEC_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.ANGLE));

        runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_DEFAULT_METHOD);

        runDeviceTests(ANGLE_DRIVER_TEST_SEC_PKG,
                ANGLE_DRIVER_TEST_SEC_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_DEFAULT_METHOD);
    }

    /**
     * Test ANGLE is not loaded when the Developer Option is invalid.
     */
    @Test
    public void testUseInvalidDriver() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG, "timtim");

        runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_DEFAULT_METHOD);
    }

    /**
     * Test the Developer Options can be updated to/from each combination.
     */
    @Test
    public void testUpdateDriverValues() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);

        for (OpenGlDriverChoice firstDriver : OpenGlDriverChoice.values()) {
            for (OpenGlDriverChoice secondDriver : OpenGlDriverChoice.values()) {
                CLog.logAndDisplay(LogLevel.INFO, "Testing updating Global.Settings from '" +
                        firstDriver + "' to '" + secondDriver + "'");

                setAndValidatePkgDriver(ANGLE_DRIVER_TEST_PKG, firstDriver);
                setAndValidatePkgDriver(ANGLE_DRIVER_TEST_PKG, secondDriver);
            }
        }
    }

    /**
     * Test different PKGs can have different developer option values.
     * Primary: ANGLE
     * Secondary: Native
     */
    @Test
    public void testMultipleDevOptionsAngleNative() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);
        installApp(ANGLE_DRIVER_TEST_SEC_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG + "," +
                        ANGLE_DRIVER_TEST_SEC_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.ANGLE) + "," +
                        sDriverGlobalSettingMap.get(OpenGlDriverChoice.NATIVE));

        runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_ANGLE_METHOD);

        runDeviceTests(ANGLE_DRIVER_TEST_SEC_PKG,
                ANGLE_DRIVER_TEST_SEC_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_NATIVE_METHOD);
    }

    /**
     * Test the Developer Options for a second PKG can be updated to/from each combination.
     */
    @Test
    public void testMultipleUpdateDriverValues() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);
        installApp(ANGLE_DRIVER_TEST_SEC_APP);

        // Set the first PKG to always use ANGLE
        setAndValidatePkgDriver(ANGLE_DRIVER_TEST_PKG, OpenGlDriverChoice.ANGLE);

        for (OpenGlDriverChoice firstDriver : OpenGlDriverChoice.values()) {
            for (OpenGlDriverChoice secondDriver : OpenGlDriverChoice.values()) {
                CLog.logAndDisplay(LogLevel.INFO, "Testing updating Global.Settings from '" +
                        firstDriver + "' to '" + secondDriver + "'");

                setAndValidateAngleDevOptionPkgDriver(
                        ANGLE_DRIVER_TEST_PKG + "," + ANGLE_DRIVER_TEST_SEC_PKG,
                        sDriverGlobalSettingMap.get(OpenGlDriverChoice.ANGLE) + "," +
                                sDriverGlobalSettingMap.get(firstDriver));

                startActivity(getDevice(), ANGLE_MAIN_ACTIVTY);

                CLog.logAndDisplay(LogLevel.INFO, "Validating driver selection (" +
                        firstDriver + ") with method '" + sDriverTestMethodMap.get(firstDriver) + "'");

                runDeviceTests(ANGLE_DRIVER_TEST_SEC_PKG,
                        ANGLE_DRIVER_TEST_SEC_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                        sDriverTestMethodMap.get(firstDriver));

                setAndValidateAngleDevOptionPkgDriver(
                        ANGLE_DRIVER_TEST_PKG + "," + ANGLE_DRIVER_TEST_SEC_PKG,
                        sDriverGlobalSettingMap.get(OpenGlDriverChoice.ANGLE) + "," +
                                sDriverGlobalSettingMap.get(secondDriver));

                startActivity(getDevice(), ANGLE_MAIN_ACTIVTY);

                CLog.logAndDisplay(LogLevel.INFO, "Validating driver selection (" +
                        secondDriver + ") with method '" + sDriverTestMethodMap.get(secondDriver) + "'");

                runDeviceTests(ANGLE_DRIVER_TEST_SEC_PKG,
                        ANGLE_DRIVER_TEST_SEC_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                        sDriverTestMethodMap.get(secondDriver));

                // Make sure the first PKG's driver value was not modified
                startActivity(getDevice(), ANGLE_MAIN_ACTIVTY);

                String devOptionPkg = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_PKGS);
                String devOptionValue = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_VALUES);
                CLog.logAndDisplay(LogLevel.INFO, "Validating: PKG name = '" +
                        devOptionPkg + "', driver value = '" + devOptionValue + "'");

                runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                        ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                        ANGLE_DRIVER_TEST_ANGLE_METHOD);
            }
        }
    }

    /**
     * Test setting a driver to 'default' does not keep the value in the settings when the ANGLE
     * activity runs and cleans things up.
     */
    @Test
    public void testDefaultNotInSettings() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        // Install the package so the setting isn't removed because the package isn't present.
        installApp(ANGLE_DRIVER_TEST_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.DEFAULT));

        // Run the ANGLE activity so it'll clear up any 'default' settings.
        startActivity(getDevice(), ANGLE_MAIN_ACTIVTY);

        String devOptionPkg = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_PKGS);
        String devOptionValue = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_VALUES);
        CLog.logAndDisplay(LogLevel.INFO, "Validating: PKG name = '" +
                devOptionPkg + "', driver value = '" + devOptionValue + "'");

        Assert.assertEquals(
                "Invalid developer option: " + SETTINGS_GLOBAL_DRIVER_PKGS + " = '" + devOptionPkg + "'",
                "", devOptionPkg);
        Assert.assertEquals(
                "Invalid developer option: " + SETTINGS_GLOBAL_DRIVER_VALUES + " = '" + devOptionValue + "'",
                "", devOptionValue);
    }

    /**
     * Test uninstalled PKGs have their settings removed.
     */
    @Test
    public void testUninstalledPkgsNotInSettings() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        uninstallPackage(getDevice(), ANGLE_DRIVER_TEST_PKG);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.NATIVE));

        // Run the ANGLE activity so it'll clear up any 'default' settings.
        startActivity(getDevice(), ANGLE_MAIN_ACTIVTY);

        String devOptionPkg = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_PKGS);
        String devOptionValue = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_VALUES);
        CLog.logAndDisplay(LogLevel.INFO, "Validating: PKG name = '" +
                devOptionPkg + "', driver value = '" + devOptionValue + "'");

        Assert.assertEquals(
                "Invalid developer option: " + SETTINGS_GLOBAL_DRIVER_PKGS + " = '" + devOptionPkg + "'",
                "", devOptionPkg);
        Assert.assertEquals(
                "Invalid developer option: " + SETTINGS_GLOBAL_DRIVER_VALUES + " = '" + devOptionValue + "'",
                "", devOptionValue);
    }

    /**
     * Test different PKGs can have different developer option values.
     * Primary: ANGLE
     * Secondary: Default
     *
     * Verify the PKG set to 'default' is removed from the settings.
     */
    @Test
    public void testMultipleDevOptionsAngleDefault() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_APP);
        installApp(ANGLE_DRIVER_TEST_SEC_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG + "," + ANGLE_DRIVER_TEST_SEC_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.ANGLE) + "," +
                        sDriverGlobalSettingMap.get(OpenGlDriverChoice.DEFAULT));

        // Run the ANGLE activity so it'll clear up any 'default' settings.
        startActivity(getDevice(), ANGLE_MAIN_ACTIVTY);

        String devOption = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_PKGS);
        Assert.assertEquals(
                "Invalid developer option: " + SETTINGS_GLOBAL_DRIVER_PKGS + " = '" + devOption + "'",
                ANGLE_DRIVER_TEST_PKG, devOption);

        devOption = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_VALUES);
        Assert.assertEquals(
                "Invalid developer option: " + SETTINGS_GLOBAL_DRIVER_VALUES + " = '" + devOption + "'",
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.ANGLE), devOption);
    }

    /**
     * Test different PKGs can have different developer option values.
     * Primary: ANGLE
     * Secondary: Default
     *
     * Verify the uninstalled PKG is removed from the settings.
     */
    @Test
    public void testMultipleDevOptionsAngleNativeUninstall() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        installApp(ANGLE_DRIVER_TEST_SEC_APP);

        setAndValidateAngleDevOptionPkgDriver(ANGLE_DRIVER_TEST_PKG + "," + ANGLE_DRIVER_TEST_SEC_PKG,
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.ANGLE) + "," +
                        sDriverGlobalSettingMap.get(OpenGlDriverChoice.NATIVE));

        // Run the ANGLE activity so it'll clear up any 'default' settings.
        startActivity(getDevice(), ANGLE_MAIN_ACTIVTY);

        String devOptionPkg = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_PKGS);
        String devOptionValue = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_DRIVER_VALUES);
        CLog.logAndDisplay(LogLevel.INFO, "Validating: PKG name = '" +
                devOptionPkg + "', driver value = '" + devOptionValue + "'");

        Assert.assertEquals(
                "Invalid developer option: " + SETTINGS_GLOBAL_DRIVER_PKGS + " = '" + devOptionPkg + "'",
                ANGLE_DRIVER_TEST_SEC_PKG, devOptionPkg);
        Assert.assertEquals(
                "Invalid developer option: " + SETTINGS_GLOBAL_DRIVER_VALUES + " = '" + devOptionValue + "'",
                sDriverGlobalSettingMap.get(OpenGlDriverChoice.NATIVE), devOptionValue);
    }

    /**
     * Test that the "ANGLE In Use" dialog box can be enabled when ANGLE is used.
     *
     * We can't actually make sure the dialog box shows up, just that enabling it and attempting to
     * show it doesn't cause a crash or prevent ANGLE from being enabled.
     */
    @Test
    public void testAngleInUseDialogBoxWithAngle() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        setGlobalSetting(getDevice(), SETTINGS_GLOBAL_ANGLE_IN_USE_DIALOG_BOX, "1");

        testUseAngleDriver();
    }

    /**
     * Test that the "ANGLE In Use" dialog box can be enabled when Native is used.
     *
     * We can't actually make sure the dialog box shows up, just that enabling it and attempting to
     * show it doesn't cause a crash.
     */
    @Test
    public void testAngleInUseDialogBoxWithNative() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        setGlobalSetting(getDevice(), SETTINGS_GLOBAL_ANGLE_IN_USE_DIALOG_BOX, "1");

        testUseNativeDriver();
    }
}
