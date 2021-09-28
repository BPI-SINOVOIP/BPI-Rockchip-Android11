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
package com.android.helpers;

import android.os.SystemClock;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.By;
import androidx.test.runner.AndroidJUnit4;


import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertEquals;

/**
 * Android Unit tests for {@link com.android.helpers.CrashHelper}.
 *
 * <p>To run: Disable SELinux: adb shell setenforce 0; if this fails with "permission denied", try
 * Build and install Development apk.
 * "adb shell su 0 setenforce 0" atest CollectorsHelperTest:com.android.helpers.CrashHelperTest
 */
@RunWith(AndroidJUnit4.class)
public class CrashHelperTest {

    // Launch Development app BadBehavior activity.
    private static final String START_APP =
            "am start -W -n com.android.development/.BadBehaviorActivity";
    // Kill the Development app
    private static final String KILL_TEST_APP_CMD = "am force-stop com.android.development";
    // Package name of the development app.
    private static final String PKG_NAME = "com.android.development";
    // Key used to store the crash count.
    private static final String TOTAL_CRASHES_KEY = "total_crash";
    // Key used to store the native crash.
    private static final String TOTAL_NATIVE_CRASHES_KEY = "total_native_crash";
    // Key used to store the ANR.
    private static final String TOTAL_ANRS_KEY = "total_anr";
    // Detailed crash key associated with the package name and the foreground status.
    private static final String CRASH_PKG_KEY = "crash_com.android.development_FOREGROUND";
    // Detailed native crash key associated with the package name and the foreground status.
    private static final String NATIVE_CRASH_PKG_KEY =
            "native_crash_com.android.development_FOREGROUND";
    // Detailed event key associated with the ANR: process, reason and foreground status.
    private static final String ANR_DETAIL_KEY = "anr_com.android.development_FOREGROUND";
    // Button id to cause the crash.
    private static final String CRASH_BTN_NAME = "bad_behavior_crash_main";
    // Button id to cause the native crash.
    private static final String NATIVE_CRASH_BTN_NAME = "bad_behavior_crash_native";
    // Button id to cause the an ANR (not all ANR-related buttons work, but this one does).
    private static final String ANR_SERVICE_BTN_NAME = "bad_behavior_anr_service";
    // This delay ensures that an ANR is actually logged.
    // For details, see BadBehaviorActivity.BadService.
    private static final int ANR_DELAY = 40000;

    private CrashHelper mCrashHelper = new CrashHelper();

    @Before
    public void setUp() {
        mCrashHelper = new CrashHelper();
        // Make the apps are starting from the clean state.
        HelperTestUtility.clearApp(KILL_TEST_APP_CMD);
    }

    /**
     * Test successfull crash config.
     */
    @Test
    public void testCrashConfig() throws Exception {
        assertTrue(mCrashHelper.startCollecting());
        assertTrue(mCrashHelper.stopCollecting());
    }

    /**
     * Test no crash metrics.
     */
    @Test
    public void testEmptyCrashMetric() throws Exception {
        assertTrue(mCrashHelper.startCollecting());
        Map<String, Integer> crashMap = mCrashHelper.getMetrics();
        // "crash", "native_crash" and "anr" keys with value 0
        assertEquals(3, crashMap.size());
        assertEquals(0, crashMap.get(TOTAL_CRASHES_KEY).intValue());
        assertEquals(0, crashMap.get(TOTAL_NATIVE_CRASHES_KEY).intValue());
        assertEquals(0, crashMap.get(TOTAL_ANRS_KEY).intValue());
        assertTrue(mCrashHelper.stopCollecting());

    }

    /**
     * Test one crash metric.
     */
    @Test
    public void testCrashMetric() throws Exception {
        assertTrue(mCrashHelper.startCollecting());
        HelperTestUtility.executeShellCommand(START_APP);
        invokeBehavior(CRASH_BTN_NAME);
        Map<String, Integer> crashMap = mCrashHelper.getMetrics();
        // An empty ANR key in addition to the crash keys.
        assertEquals(4, crashMap.size());
        assertEquals(1, crashMap.get(TOTAL_CRASHES_KEY).intValue());
        assertEquals(0, crashMap.get(TOTAL_NATIVE_CRASHES_KEY).intValue());
        assertEquals(1, crashMap.get(CRASH_PKG_KEY).intValue());
        assertEquals(0, crashMap.get(TOTAL_ANRS_KEY).intValue());
        assertTrue(mCrashHelper.stopCollecting());

    }

    /**
     * Test native crash metric.
     */
    @Test
    public void testNativeCrashMetric() throws Exception {
        assertTrue(mCrashHelper.startCollecting());
        HelperTestUtility.executeShellCommand(START_APP);
        invokeBehavior(NATIVE_CRASH_BTN_NAME);
        Map<String, Integer> crashMap = mCrashHelper.getMetrics();
        // An empty ANR key in addition to the crash keys.
        assertEquals(4, crashMap.size());
        assertEquals(0, crashMap.get(TOTAL_CRASHES_KEY).intValue());
        assertEquals(1, crashMap.get(TOTAL_NATIVE_CRASHES_KEY).intValue());
        assertEquals(1, crashMap.get(NATIVE_CRASH_PKG_KEY).intValue());
        assertEquals(0, crashMap.get(TOTAL_ANRS_KEY).intValue());
        assertTrue(mCrashHelper.stopCollecting());

    }

    /**
     * Test ANR metric.
     */
    @Test
    public void testAnrMetric() throws Exception {
        assertTrue(mCrashHelper.startCollecting());
        HelperTestUtility.executeShellCommand(START_APP);
        invokeBehavior(ANR_SERVICE_BTN_NAME, ANR_DELAY);
        Map<String, Integer> crashMap = mCrashHelper.getMetrics();
        // Two ANR keys and two empty crash keys.
        assertEquals(4, crashMap.size());
        assertEquals(1, crashMap.get(TOTAL_ANRS_KEY).intValue());
        assertEquals(1, crashMap.get(ANR_DETAIL_KEY).intValue());
        assertEquals(0, crashMap.get(TOTAL_CRASHES_KEY).intValue());
        assertEquals(0, crashMap.get(TOTAL_NATIVE_CRASHES_KEY).intValue());
        assertTrue(mCrashHelper.stopCollecting());
    }

    /**
     * Test both crash and native crash.
     */
    @Test
    public void testMultipleCrashMetric() throws Exception {
        assertTrue(mCrashHelper.startCollecting());

        // Invoke the crash
        HelperTestUtility.executeShellCommand(START_APP);
        invokeBehavior(CRASH_BTN_NAME);

        // Invoke the native crash
        HelperTestUtility.executeShellCommand(START_APP);
        invokeBehavior(NATIVE_CRASH_BTN_NAME);

        // Invoke the ANR.
        HelperTestUtility.executeShellCommand(START_APP);
        invokeBehavior(ANR_SERVICE_BTN_NAME, ANR_DELAY);

        Map<String, Integer> crashMap = mCrashHelper.getMetrics();
        // Two keys for each crash metric, totaling 6.
        assertEquals(6, crashMap.size());
        assertEquals(1, crashMap.get(TOTAL_CRASHES_KEY).intValue());
        assertEquals(1, crashMap.get(CRASH_PKG_KEY).intValue());
        assertEquals(1, crashMap.get(TOTAL_NATIVE_CRASHES_KEY).intValue());
        assertEquals(1, crashMap.get(NATIVE_CRASH_PKG_KEY).intValue());
        assertEquals(1, crashMap.get(TOTAL_ANRS_KEY).intValue());
        assertEquals(1, crashMap.get(ANR_DETAIL_KEY).intValue());
        assertTrue(mCrashHelper.stopCollecting());
    }

    /**
     * Cause the behavior by clicking on the button in bad behaviour activity.
     */
    private void invokeBehavior(String resourceName, int delayAfterAction) {
        UiObject2 behaviorButton = HelperTestUtility.getUiDevice().findObject(
                By.res(PKG_NAME, resourceName));
        behaviorButton.click();
        // Some actions, e.g. service ANR, requires a delay to register.
        SystemClock.sleep(delayAfterAction);

        // Dismiss the crash dialog which sometimes appear.
        UiObject2 closeButton = HelperTestUtility.getUiDevice().findObject(
                By.res("android", "aerr_close"));
        if (closeButton != null) {
            closeButton.click();
        }
        SystemClock.sleep(HelperTestUtility.ACTION_DELAY);
    }

    /**
     * Convenience function for "immediate" actions such as crashes.
     */
    private void invokeBehavior(String resourceName) {
        invokeBehavior(resourceName, 0);
    }
}
