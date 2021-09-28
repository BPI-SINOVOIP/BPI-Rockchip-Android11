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

package com.android.compatibility.common.tradefed.targetprep;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.when;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;


/**
 * Unit test for {@link BusinessLogicPreparer}.
 */

@RunWith(JUnit4.class)
public class BusinessLogicPreparerTest {
    private ITestDevice mMockDevice;
    private IBuildInfo mMockBuildInfo;
    private BusinessLogicPreparer mPreparer;
    private Set<String> mPackages;
    private File mTmpDir;

    private static final String MEMORY_DEVICE_INFO_JSON =
            "{\n" +
            "    \"low_ram_device\": false,\n" +
            "    \"memory_class\": 192,\n" +
            "    \"large_memory_class\": 512,\n" +
            "    \"total_memory\": 1902936064\n" +
            "}\n";
    private static final String MANUFACTURER_PROPERTY = "ro.product.manufacturer";
    private static final String CONFIG_VERSION = "DYNAMIC_CONFIG_FILE:";
    private static final String CONFIG_FILE_CONTENT =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" +
                    "<dynamicConfig>\n" +
                    "    <entry key=\"remote_config_required\">\n" +
                    "        <value>false</value>\n" +
                    "    </entry>\n" +
                    "    <entry key=\"business_logic_device_features\">\n" +
                    "        <value>android.hardware.type.automotive</value>\n" +
                    "        <value>android.hardware.type.television</value>\n" +
                    "        <value>android.hardware.type.watch</value>\n" +
                    "        <value>android.hardware.type.embedded</value>\n" +
                    "        <value>android.hardware.type.pc</value>\n" +
                    "        <value>android.software.leanback</value>\n" +
                    "    </entry>\n" +
                    "    <entry key=\"business_logic_device_properties\">\n" +
                    "        <value>ro.product.brand</value>\n" +
                    "        <value>ro.product.first_api_level</value>\n" +
                    "        <value>ro.product.manufacturer</value>\n" +
                    "        <value>ro.product.model</value>\n" +
                    "        <value>ro.product.name</value>\n" +
                    "    </entry>\n" +
                    "    <entry key=\"business_logic_device_packages\">\n" +
                    "        <value>com.google.android.gms</value>\n" +
                    "        <value>com.android.vending</value>\n" +
                    "    </entry>\n" +
                    "    <entry key=\"business_logic_extended_device_info\">\n" +
                    "        <value>MemoryDeviceInfo:total_memory</value>\n" +
                    "        <value>MemoryDeviceInfo:low_ram_device</value>\n" +
                    "    </entry>\n" +
                    "</dynamicConfig>\n";
    private static final String LIST_FEATURE_QUERY = "pm list features";
    private static final String FEATURE_WATCH = "android.hardware.type.watch";
    private static final String FEATURE_LEANBACK = "android.software.leanback";
    private static final String FEATURES =
            "feature:" + FEATURE_WATCH + "\n" +
            "feature:android.hardware.audio.low_latency\n" +
            "feature:android.hardware.camera\n" +
            "feature:android.hardware.microphone\n" +
            "feature:android.hardware.nfc\n" +
            "feature:android.hardware.telephony\n" +
            "feature:android.hardware.wifi\n" +
            "feature:" + FEATURE_LEANBACK + "\n";
    private static final String GOOGLE_SETTINGS_QUERY =
            "content query --uri content://com.google.settings/partner";
    private static final String PARTNER_CONTENT =
            "Row: 0 _id=35, name=use_location_for_services, value=1\n" +
            "Row: 1 _id=57, name=network_location_opt_in, value=1\n" +
            "Row: 2 _id=162, name=data_store_version, value=3\n" +
            "Row: 3 _id=163, name=client_id, value=android-google\n" +
            "Row: 4 _id=164, name=search_client_id, value=ms-android-google\n";
    private static final String RO_BRAND = "ro.product.brand";
    private static final String RO_FIRST_API_LEVEL = "ro.product.first_api_level";
    private static final String RO_MANIFACTURER = "ro.product.manufacturer";
    private static final String RO_MODEL = "ro.product.model";
    private static final String RO_NAME = "ro.product.name";
    private static final long MEMORY_SIZE = 1902936064;

    private String serviceUrl = "https://androidpartner.googleapis.com/v1/dynamicconfig/" +
            "suites/{suite-name}/modules/{module}/version/{version}?key=123";

    @Before
    public void setUp() throws Exception {
        mTmpDir = FileUtil.createTempDir("business-logic-unit-tests");
        mMockDevice = Mockito.mock(ITestDevice.class);
        mMockBuildInfo = new BuildInfo();
        mPreparer = new BusinessLogicPreparer() {
            @Override
            String getSuiteName() {
                return "cts";
            }
        };

        OptionSetter setter = new OptionSetter(mPreparer);
        setter.setOptionValue("business-logic-url", serviceUrl);
        setter.setOptionValue("business-logic-api-key", "fakeApiKey");
        setter.setOptionValue("version", "fakeVersion");
    }

    @After
    public void tearDown() throws Exception {
        FileUtil.recursiveDelete(mTmpDir);
    }

    @Test
    public void testBuildRequestString_success() throws Exception {
        Map<String, String> attributes = new HashMap<>();
        // Create a memory device info JSON file for test.
        File jsonPath = createTestDeviceInfoJSONFile("MemoryDeviceInfo", MEMORY_DEVICE_INFO_JSON);
        mMockBuildInfo.setFile(DeviceInfoCollector.DEVICE_INFO_DIR, jsonPath, "v1");
        // Setup BuildInfo attributes.
        mMockBuildInfo.addBuildAttribute(CompatibilityBuildHelper.SUITE_VERSION, "v1");
        testBuildRequestString(16, attributes);
    }

    @Test
    public void testBuildRequestString_deviceDirDoesntExists() throws Exception {
        Map<String, String> attributes = new HashMap<>();
        // Setup BuildInfo attributes.
        attributes.put(CompatibilityBuildHelper.SUITE_VERSION, "v1");
        testBuildRequestString(14, attributes);
    }

    @Test
    public void testBuildRequestString_noDeviceInfoJSONFileExists() throws Exception {
        Map<String, String> attributes = new HashMap<>();
        // Create a memory device info JSON file for test.
        File jsonPath = createTestDeviceInfoTextFile("MemoryDeviceInfo");
        mMockBuildInfo.setFile(DeviceInfoCollector.DEVICE_INFO_DIR, jsonPath, "v1");
        // Setup BuildInfo attributes.
        mMockBuildInfo.addBuildAttribute(CompatibilityBuildHelper.SUITE_VERSION, "v1");
        testBuildRequestString(14, attributes);
    }

    private void testBuildRequestString(int expectedParams, Map<String, String> attributes) throws Exception {
        for (String key: attributes.keySet()) {
            mMockBuildInfo.addBuildAttribute(key, attributes.get(key));
        }
        when(mMockDevice.getProperty(MANUFACTURER_PROPERTY)).thenReturn("MANUFACTURER_NAME");
        // In getBusinessLogicFeatures
        File configFile = createFileFromStr(CONFIG_FILE_CONTENT);
        // BusinessLogicPreparer.getSuiteName() calls TestSuiteInfo.getName()
        // That returns "tests" on local machine.
        // That returns "gts" in presumit.
        mMockBuildInfo.setFile(CONFIG_VERSION + "tests", configFile, CONFIG_VERSION + "tests");
        mMockBuildInfo.setFile(CONFIG_VERSION + "gts", configFile, CONFIG_VERSION + "gts");
        mMockBuildInfo.setFile(CONFIG_VERSION + "cts", configFile, CONFIG_VERSION + "cts");
        mMockBuildInfo.setFile(CONFIG_VERSION + "ats", configFile, CONFIG_VERSION + "ats");
        when(mMockDevice.executeShellCommand(LIST_FEATURE_QUERY)).thenReturn(FEATURES);
        // In getBusinessLogicProperties.
        when(mMockDevice.executeShellCommand(GOOGLE_SETTINGS_QUERY)).thenReturn(PARTNER_CONTENT);
        when(mMockDevice.getProperty(RO_BRAND)).thenReturn("BRAND_NAME");
        when(mMockDevice.getProperty(RO_FIRST_API_LEVEL)).thenReturn("26");
        when(mMockDevice.getProperty(RO_MANIFACTURER)).thenReturn("MANUFACTURER_NAME");
        when(mMockDevice.getProperty(RO_MODEL)).thenReturn("fake_model");
        when(mMockDevice.getProperty(RO_NAME)).thenReturn("fake_name");
        // Extra info
        List<String> pkgNameList = Arrays.asList("com.android.vending", "com.google.android.gms",
            "com.google.android.youtube", "com.google.android.apps.photos");
        mPackages = new HashSet<>(pkgNameList);
        when(mMockDevice.getInstalledPackageNames()).thenReturn(mPackages);
        when(mMockDevice.getTotalMemory()).thenReturn(MEMORY_SIZE);

        ArrayList<String> expectFeatures = new ArrayList<String>(
            Arrays.asList(FEATURE_WATCH, FEATURE_LEANBACK));
        ArrayList<String> expectProperties = new ArrayList<String>(
            Arrays.asList("search_client_id", "client_id", RO_BRAND, RO_FIRST_API_LEVEL,
                RO_MANIFACTURER, RO_MODEL, RO_NAME));
        ArrayList<String> expectPropertyValues = new ArrayList<String>(
            Arrays.asList("ms-android-google", "android-google", "BRAND_NAME", "26", "MANUFACTURER_NAME",
                "fake_model", "fake_name"));
        ArrayList<String> expectPackages = new ArrayList<String>(pkgNameList);
        ArrayList<String> expectDeviceInfos = new ArrayList<String>(
            Arrays.asList("MemoryDeviceInfo%3Atotal_memory%3A1902936064",
            "MemoryDeviceInfo%3Alow_ram_device%3Afalse"));

        String[] params = mPreparer.buildRequestParams(mMockDevice, mMockBuildInfo).split("&");
        assertEquals(expectedParams, params.length);

        for (String param: params){
            String keyVal[] = param.split("=");

            if(keyVal[0].startsWith("features")) {
                if(expectFeatures.contains(keyVal[1])) {
                    expectFeatures.remove(keyVal[1]);
                } else {
                    fail("Found unknown Feature string");
                }
            }
            if (keyVal[0].startsWith("properties")) {
                String property[] = keyVal[1].split("%3A");
                if(expectProperties.contains(property[0])) {
                    assertEquals(expectPropertyValues.get(expectProperties.indexOf(property[0])),
                            property[1]);
                    expectProperties.remove(property[0]);
                    expectPropertyValues.remove(property[1]);
                } else {
                    fail("Found unknown Property string");
                }
            }
            if (keyVal[0].startsWith("package")) {
                if(expectPackages.contains(keyVal[1])) {
                    expectPackages.remove(keyVal[1]);
                } else {
                    fail("Found unknown Package string");
                }
            }
            if (keyVal[0].startsWith("device_info")) {
                if(expectDeviceInfos.contains(keyVal[1])) {
                    expectDeviceInfos.remove(keyVal[1]);
                } else {
                    fail("Found unknown Feature string");
                }
            }
        }
        assertEquals(expectFeatures.size(), 0);
        assertEquals(expectProperties.size(), 0);
        assertEquals(expectPackages.size(), 2);
    }

    private File createTestDeviceInfoJSONFile(String DeviceInfoClassName, String jsonStr)
        throws IOException {
        File file = new File(mTmpDir, DeviceInfoClassName + ".deviceinfo.json");
        FileOutputStream stream = new FileOutputStream(file);
        stream.write(jsonStr.getBytes());
        stream.flush();
        stream.close();
        return mTmpDir;
    }

    private File createTestDeviceInfoTextFile(String DeviceInfoClassName)
        throws IOException {
        new File(mTmpDir, DeviceInfoClassName + ".deviceinfo.text");
        return mTmpDir;
    }

    private File createFileFromStr(String configStr) throws IOException {
        File file = File.createTempFile("test", "dynamic");
        FileOutputStream stream = new FileOutputStream(file);
        stream.write(configStr.getBytes());
        stream.flush();
        stream.close();
        return file;
    }
}
