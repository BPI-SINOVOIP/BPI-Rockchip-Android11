/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.car.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.endsWith;
import static org.junit.Assume.assumeThat;
import static org.junit.Assume.assumeTrue;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Check Optional Feature related car configs.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class OptionalFeatureHostTest extends BaseHostJUnit4Test {

    private static final String FEATURE_AUTOMOTIVE = "android.hardware.type.automotive";

    private static final String[] MANDATORY_FEATURES = {
            "android.car.input",
            "app_focus",
            "audio",
            "cabin",
            "car_bluetooth",
            "car_bugreport",
            "car_media",
            "car_navigation_service",
            "car_occupant_zone_service",
            "car_user_service",
            "car_watchdog",
            "configuration",
            "drivingstate",
            "hvac",
            "info",
            "package",
            "power",
            "projection",
            "property",
            "sensor",
            "uxrestriction",
            "vendor_extension"
    };

    @Before
    public void setUp() throws Exception {
        assumeTrue(hasDeviceFeature(FEATURE_AUTOMOTIVE));
    }

    /**
     * Partners can use the same system image for multiple product configs with variation in
     * optional feature support. But CTS should run in a config where VHAL
     * DISABLED_OPTIONAL_FEATURES property does not disable any optional features.
     */
    @Test
    public void testNoDisabledFeaturesFromVHAL() throws Exception {
        List<String> features = findFeatureListFromCarServiceDump("mDisabledFeaturesFromVhal");
        assertThat(features).isEmpty();
    }

    /**
     * Experimental features cannot be shipped. There should be no experimental features available
     * in the device.
     */
    @Test
    public void testNoExperimentalFeatures() throws Exception {
        // experimental feature disabled in user build
        assumeUserBuild();
        List<String> features = findFeatureListFromCarServiceDump("mAvailableExperimentalFeatures");
        assertThat(features).isEmpty();
    }

    /**
     * Experimental car service package should not exist in the device.
     */
    @Test
    public void testNoExperimentalCarServicePackage() throws Exception {
        // experimental feature disabled in user build
        assumeUserBuild();
        // Only check for user 0 as experimental car service is launched as user 0.
        String output = getDevice().executeShellCommand(
                "pm list package com.android.experimentalcar").strip();
        assertThat(output).isEmpty();
    }

    /**
     * All optional features declared from {@code mDefaultEnabledFeaturesFromConfig} should be
     * enabled for CTS test.
     */
    @Test
    public void testAllOptionalFeaturesEnabled() throws Exception {
        List<String> enabledFeatures = findFeatureListFromCarServiceDump("mEnabledFeatures");
        List<String> optionalFeaturesFromConfig = findFeatureListFromCarServiceDump(
                "mDefaultEnabledFeaturesFromConfig");
        List<String> missingFeatures = new ArrayList<>();
        for (String optional : optionalFeaturesFromConfig) {
            if (!enabledFeatures.contains(optional)) {
                missingFeatures.add(optional);
            }
        }
        assertThat(missingFeatures).isEmpty();
    }

    /**
     * Confirms that selected mandatory features are enabled.
     */
    @Test
    public void testAllMandatoryFeaturesEnabled() throws Exception {
        List<String> enabledFeatures = findFeatureListFromCarServiceDump("mEnabledFeatures");
        List<String> missingFeatures = new ArrayList<>();
        for (String optional : MANDATORY_FEATURES) {
            if (!enabledFeatures.contains(optional)) {
                missingFeatures.add(optional);
            }
        }
        assertThat(missingFeatures).isEmpty();
    }

    /**
     * Confirms that adb command cannot drop feature.
     */
    @Test
    public void testNoFeatureChangeAfterRebootForAdbCommand() throws Exception {
        List<String> enabledFeaturesOrig = findFeatureListFromCarServiceDump("mEnabledFeatures");
        for (String feature : enabledFeaturesOrig) {
            getDevice().executeShellCommand("cmd car_service disable-feature %s" + feature);
        }
        getDevice().reboot();
        List<String> enabledFeaturesAfterReboot = findFeatureListFromCarServiceDump(
                "mEnabledFeatures");
        for (String feature : enabledFeaturesAfterReboot) {
            enabledFeaturesOrig.remove(feature);
        }
        assertThat(enabledFeaturesOrig).isEmpty();
    }

    private List<String> findFeatureListFromCarServiceDump(String featureDumpName)
            throws Exception {
        String output = getDevice().executeShellCommand(
                "dumpsys car_service --services CarFeatureController");
        Pattern pattern = Pattern.compile(featureDumpName + ":\\[(.*)\\]");
        Matcher m = pattern.matcher(output);
        if (!m.find()) {
            return Collections.EMPTY_LIST;
        }
        String[] features = m.group(1).split(", ");
        ArrayList<String> featureList = new ArrayList<>(features.length);
        for (String feature : features) {
            if (feature.isEmpty()) {
                continue;
            }
            featureList.add(feature);
        }
        return featureList;
    }

    private void assumeUserBuild() throws Exception {
        assumeThat(getDevice().getBuildFlavor(), endsWith("-user"));
    }
}
