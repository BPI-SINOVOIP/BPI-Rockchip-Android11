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

import com.google.common.io.ByteStreams;
import com.google.common.io.Files;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.FileUtil;

import java.io.File;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.runner.RunWith;
import org.junit.Test;

/**
 * Tests ANGLE Rules File Opt-In/Out functionality.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class CtsAngleRulesFileTest extends BaseHostJUnit4Test {

    private final String TAG = this.getClass().getSimpleName();

    private File mRulesFile;
    private String mWhiteList;

    // Rules Files
    private static final String RULES_FILE_EMPTY = "emptyRules.json";
    private static final String RULES_FILE_ENABLE_ANGLE = "enableAngleRules.json";

    private void pushRulesFile(String hostFilename) throws Exception {
        byte[] rulesFileBytes =
                ByteStreams.toByteArray(getClass().getResourceAsStream("/" + hostFilename));

        Assert.assertTrue("Loaded empty rules file", rulesFileBytes.length > 0); // sanity check
        mRulesFile = File.createTempFile("rulesFile", "tempRules.json");
        Files.write(rulesFileBytes, mRulesFile);

        Assert.assertTrue(getDevice().pushFile(mRulesFile, DEVICE_TEMP_RULES_FILE_PATH));

        setProperty(getDevice(), PROPERTY_TEMP_RULES_FILE, DEVICE_TEMP_RULES_FILE_PATH);
    }

    private void setAndValidateAngleDevOptionWhitelist(String whiteList) throws Exception {
        // SETTINGS_GLOBAL_WHITELIST
        setGlobalSetting(getDevice(), SETTINGS_GLOBAL_WHITELIST, whiteList);

        String devOption = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_WHITELIST);
        Assert.assertEquals(
            "Developer option '" + SETTINGS_GLOBAL_WHITELIST +
                "' was not set correctly: '" + devOption + "'",
            whiteList, devOption);
    }

    @Before
    public void setUp() throws Exception {
        clearSettings(getDevice());

        mWhiteList = getGlobalSetting(getDevice(), SETTINGS_GLOBAL_WHITELIST);

        final String whitelist = ANGLE_DRIVER_TEST_PKG + "," + ANGLE_DRIVER_TEST_SEC_PKG;
        setAndValidateAngleDevOptionWhitelist(whitelist);
    }

    @After
    public void tearDown() throws Exception {
        clearSettings(getDevice());

        setAndValidateAngleDevOptionWhitelist(mWhiteList);

        FileUtil.deleteFile(mRulesFile);
    }

    /**
     * Test ANGLE is not loaded when an empty rules file is used.
     */
    @Test
    public void testEmptyRulesFile() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        pushRulesFile(RULES_FILE_EMPTY);

        installPackage(ANGLE_DRIVER_TEST_APP);

        runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_NATIVE_METHOD);
    }

    /**
     * Test ANGLE is loaded for only the PKG the rules file specifies.
     */
    @Test
    public void testEnableAngleRulesFile() throws Exception {
        Assume.assumeTrue(isAngleLoadable(getDevice()));

        pushRulesFile(RULES_FILE_ENABLE_ANGLE);

        installPackage(ANGLE_DRIVER_TEST_APP);
        installPackage(ANGLE_DRIVER_TEST_SEC_APP);

        runDeviceTests(ANGLE_DRIVER_TEST_PKG,
                ANGLE_DRIVER_TEST_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_ANGLE_METHOD);

        runDeviceTests(ANGLE_DRIVER_TEST_SEC_PKG,
                ANGLE_DRIVER_TEST_SEC_PKG + "." + ANGLE_DRIVER_TEST_CLASS,
                ANGLE_DRIVER_TEST_NATIVE_METHOD);
    }
}
