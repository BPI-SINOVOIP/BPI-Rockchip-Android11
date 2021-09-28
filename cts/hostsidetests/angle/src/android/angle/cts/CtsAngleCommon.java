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

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import java.util.HashMap;
import java.util.Map;

class CtsAngleCommon {
    // General
    static final int NUM_ATTEMPTS = 5;
    static final int REATTEMPT_SLEEP_MSEC = 5000;

    // Settings.Global
    static final String SETTINGS_GLOBAL_ALL_USE_ANGLE = "angle_gl_driver_all_angle";
    static final String SETTINGS_GLOBAL_DRIVER_PKGS = "angle_gl_driver_selection_pkgs";
    static final String SETTINGS_GLOBAL_DRIVER_VALUES = "angle_gl_driver_selection_values";
    static final String SETTINGS_GLOBAL_WHITELIST = "angle_whitelist";
    static final String SETTINGS_GLOBAL_ANGLE_IN_USE_DIALOG_BOX = "show_angle_in_use_dialog_box";

    // System Properties
    static final String PROPERTY_GFX_ANGLE_SUPPORTED = "ro.gfx.angle.supported";
    static final String PROPERTY_TEMP_RULES_FILE = "debug.angle.rules";

    // Rules File
    static final String DEVICE_TEMP_RULES_FILE_DIRECTORY = "/data/local/tmp";
    static final String DEVICE_TEMP_RULES_FILE_FILENAME = "a4a_rules.json";
    static final String DEVICE_TEMP_RULES_FILE_PATH = DEVICE_TEMP_RULES_FILE_DIRECTORY + "/" + DEVICE_TEMP_RULES_FILE_FILENAME;

    // ANGLE
    static final String ANGLE_DRIVER_TEST_PKG = "com.android.angleIntegrationTest.driverTest";
    static final String ANGLE_DRIVER_TEST_SEC_PKG = "com.android.angleIntegrationTest.driverTestSecondary";
    static final String ANGLE_DRIVER_TEST_CLASS = "AngleDriverTestActivity";
    static final String ANGLE_DRIVER_TEST_DEFAULT_METHOD = "testUseDefaultDriver";
    static final String ANGLE_DRIVER_TEST_ANGLE_METHOD = "testUseAngleDriver";
    static final String ANGLE_DRIVER_TEST_NATIVE_METHOD = "testUseNativeDriver";
    static final String ANGLE_DRIVER_TEST_APP = "CtsAngleDriverTestCases.apk";
    static final String ANGLE_DRIVER_TEST_SEC_APP = "CtsAngleDriverTestCasesSecondary.apk";
    static final String ANGLE_DRIVER_TEST_ACTIVITY =
            ANGLE_DRIVER_TEST_PKG + "/com.android.angleIntegrationTest.common.AngleIntegrationTestActivity";
    static final String ANGLE_DRIVER_TEST_SEC_ACTIVITY =
            ANGLE_DRIVER_TEST_SEC_PKG + "/com.android.angleIntegrationTest.common.AngleIntegrationTestActivity";
    static final String ANGLE_MAIN_ACTIVTY = "android.app.action.ANGLE_FOR_ANDROID";

    enum OpenGlDriverChoice {
        DEFAULT,
        NATIVE,
        ANGLE
    }

    static final Map<OpenGlDriverChoice, String> sDriverGlobalSettingMap = buildDriverGlobalSettingMap();
    static Map<OpenGlDriverChoice, String> buildDriverGlobalSettingMap() {
        Map<OpenGlDriverChoice, String> map = new HashMap<>();
        map.put(OpenGlDriverChoice.DEFAULT, "default");
        map.put(OpenGlDriverChoice.ANGLE, "angle");
        map.put(OpenGlDriverChoice.NATIVE, "native");

        return map;
    }

    static final Map<OpenGlDriverChoice, String> sDriverTestMethodMap = buildDriverTestMethodMap();
    static Map<OpenGlDriverChoice, String> buildDriverTestMethodMap() {
        Map<OpenGlDriverChoice, String> map = new HashMap<>();
        map.put(OpenGlDriverChoice.DEFAULT, ANGLE_DRIVER_TEST_DEFAULT_METHOD);
        map.put(OpenGlDriverChoice.ANGLE, ANGLE_DRIVER_TEST_ANGLE_METHOD);
        map.put(OpenGlDriverChoice.NATIVE, ANGLE_DRIVER_TEST_NATIVE_METHOD);

        return map;
    }

    static String getGlobalSetting(ITestDevice device, String globalSetting) throws Exception {
        return device.getSetting("global", globalSetting);
    }

    static void setGlobalSetting(ITestDevice device, String globalSetting, String value) throws Exception {
        device.setSetting("global", globalSetting, value);
        device.executeShellCommand("am refresh-settings-cache");
    }

    static void clearSettings(ITestDevice device) throws Exception {
        // Cached Activity Manager settings
        setGlobalSetting(device, SETTINGS_GLOBAL_ALL_USE_ANGLE, "0");
        setGlobalSetting(device, SETTINGS_GLOBAL_ANGLE_IN_USE_DIALOG_BOX, "0");
        setGlobalSetting(device, SETTINGS_GLOBAL_DRIVER_PKGS, "\"\"");
        setGlobalSetting(device, SETTINGS_GLOBAL_DRIVER_VALUES, "\"\"");
        setGlobalSetting(device, SETTINGS_GLOBAL_WHITELIST, "\"\"");

        // Properties
        setProperty(device, PROPERTY_TEMP_RULES_FILE, "\"\"");
    }

    static boolean isAngleLoadable(ITestDevice device) throws Exception {
        String angleSupported = device.getProperty(PROPERTY_GFX_ANGLE_SUPPORTED);

        return (angleSupported != null) && (angleSupported.equals("true"));
    }

    static void startActivity(ITestDevice device, String action) throws Exception {
        // Run the ANGLE activity so it'll clear up any 'default' settings.
        device.executeShellCommand("am start --user " + device.getCurrentUser() +
                " -S -W -a \"" + action + "\"");
    }

    static void stopPackage(ITestDevice device, String pkgName) throws Exception {
        device.executeShellCommand("am force-stop " + pkgName);
    }

    /**
     * Work around the fact that INativeDevice.enableAdbRoot() is not supported.
     */
    static void setProperty(ITestDevice device, String property, String value) throws Exception {
        device.executeShellCommand("setprop " + property + " " + value);
    }
}
