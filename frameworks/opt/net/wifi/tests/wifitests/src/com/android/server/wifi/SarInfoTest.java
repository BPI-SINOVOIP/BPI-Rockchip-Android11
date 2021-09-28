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

package com.android.server.wifi;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/**
 * unit tests for {@link com.android.server.wifi.SarInfo}.
 */
@SmallTest
public class SarInfoTest extends WifiBaseTest {
    private static final String TAG = "WifiSarInfoTest";

    private SarInfo mSarInfo;

    private static final int SAR_SCENARIO_1 = 1;
    private static final int SAR_SCENARIO_2 = 2;

    @Before
    public void setUp() throws Exception {
        mSarInfo = new SarInfo();
    }

    @After
    public void cleanUp() throws Exception {
    }

    /**
     * Test that at start, resetSarScenarioNeeded returns true,
     * to allow for initial setting of normal scenario.
     */
    @Test
    public void testSarInfo_resetSarScenarioNeed_atStart() throws Exception {
        assertTrue(mSarInfo.resetSarScenarioNeeded());
    }

    /**
     * Test that at start, setSarScenarioNeeded returns true.
     */
    @Test
    public void testSarInfo_setSarScenarioNeeded_atStart() throws Exception {
        assertTrue(mSarInfo.setSarScenarioNeeded(SAR_SCENARIO_1));
    }

    /**
     * Test performing two successive reset of SAR scenario.
     * The first should succeed, while the second should fail, since it is redundant.
     */
    @Test
    public void testSarInfo_repeat_reset_scenario() throws Exception {
        /* Initial reset is allowed */
        assertTrue(mSarInfo.resetSarScenarioNeeded());
        mSarInfo.reportingSuccessful();

        /* Now resetting again should not be allowed */
        assertFalse(mSarInfo.resetSarScenarioNeeded());
    }

    /**
     * Test performing set SAR scenario after reset.
     * The two attempts should succeed.
     */
    @Test
    public void testSarInfo_set_after_reset_scenario() throws Exception {
        assertTrue(mSarInfo.resetSarScenarioNeeded());
        mSarInfo.reportingSuccessful();

        /* Setting scenario should be allowed, since last call was for a reset */
        assertTrue(mSarInfo.setSarScenarioNeeded(SAR_SCENARIO_1));
    }

    /**
     * Test performing setting SAR scenario twice with same value.
     * The second attempt should fail.
     */
    @Test
    public void testSarInfo_set_twice_same_value_scenario() throws Exception {
        assertTrue(mSarInfo.setSarScenarioNeeded(SAR_SCENARIO_1));
        mSarInfo.reportingSuccessful();

        /* Second attempt should fail */
        assertFalse(mSarInfo.setSarScenarioNeeded(SAR_SCENARIO_1));
    }

    /**
     * Test performing setting SAR scenario twice with different values.
     * Both attempts should succeed.
     */
    @Test
    public void testSarInfo_set_twice_different_values_scenario() throws Exception {
        assertTrue(mSarInfo.setSarScenarioNeeded(SAR_SCENARIO_1));
        mSarInfo.reportingSuccessful();

        /* Setting scenario should be allowed */
        assertTrue(mSarInfo.setSarScenarioNeeded(SAR_SCENARIO_2));
    }

    /**
     * Test performing reset of SAR scenario after setting it.
     * Both attempts should succeed.
     */
    @Test
    public void testSarInfo_reset_after_set_scenario() throws Exception {
        assertTrue(mSarInfo.setSarScenarioNeeded(SAR_SCENARIO_1));
        mSarInfo.reportingSuccessful();

        /* Resetting scenario should be allowed */
        assertTrue(mSarInfo.resetSarScenarioNeeded());
    }

    /**
     * Test that at start, shouldReport returns false (wifi modes still disabled).
     */
    @Test
    public void testSarInfo_shouldReport_all_wifi_disabled() throws Exception {
        assertFalse(mSarInfo.shouldReport());
    }

    /**
     * Test that once Wifi (any mode) is enabled, shouldReport returns true.
     */
    @Test
    public void testSarInfo_shouldReport_wifi_enabled() throws Exception {
        mSarInfo.isWifiClientEnabled = true;
        assertTrue(mSarInfo.shouldReport());
    }

    /**
     * Test having a voice call, shouldReport should return true
     */
    @Test
    public void testSarInfo_voice_call_wifi_enabled() throws Exception {
        mSarInfo.isWifiClientEnabled = true;
        assertTrue(mSarInfo.shouldReport());
        mSarInfo.reportingSuccessful();

        mSarInfo.isVoiceCall = true;
        assertTrue(mSarInfo.shouldReport());
    }

    /**
     * Test a change in earpiece status, shouldReport should return true
     */
    @Test
    public void testSarInfo_earpiece_wifi_enabled() throws Exception {
        mSarInfo.isWifiClientEnabled = true;
        assertTrue(mSarInfo.shouldReport());
        mSarInfo.reportingSuccessful();

        mSarInfo.isEarPieceActive = true;
        assertTrue(mSarInfo.shouldReport());
    }

    /**
     * Test starting SAP, shouldReport should return true
     */
    @Test
    public void testSarInfo_sap_wifi_enabled() throws Exception {
        mSarInfo.isWifiClientEnabled = true;
        assertTrue(mSarInfo.shouldReport());
        mSarInfo.reportingSuccessful();

        mSarInfo.isWifiSapEnabled = true;
        assertTrue(mSarInfo.shouldReport());
    }
}
