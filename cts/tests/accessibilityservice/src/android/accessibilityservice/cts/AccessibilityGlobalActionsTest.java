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
 * limitations under the License.
 */

package android.accessibilityservice.cts;

import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.homeScreenOrBust;

import static org.junit.Assert.assertTrue;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.Presubmit;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeoutException;

/**
 * Test global actions
 */
@Presubmit
@AppModeFull
@RunWith(AndroidJUnit4.class)
public class AccessibilityGlobalActionsTest {
    /**
     * Timeout required for pending Binder calls or event processing to
     * complete.
     */
    private static final long TIMEOUT_ASYNC_PROCESSING = 5000;

    /**
     * The timeout since the last accessibility event to consider the device idle.
     */
    private static final long TIMEOUT_ACCESSIBILITY_STATE_IDLE = 500;

    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    @Rule
    public final AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @BeforeClass
    public static void oneTimeSetup() {
        sInstrumentation = InstrumentationRegistry.getInstrumentation();
        sUiAutomation = sInstrumentation.getUiAutomation();
        AccessibilityServiceInfo info = sUiAutomation.getServiceInfo();
        info.flags |= AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;
        sUiAutomation.setServiceInfo(info);
    }

    @AfterClass
    public static void postTestTearDown() {
        sUiAutomation.destroy();
    }

    @Before
    public void setUp() throws Exception {
        // Make sure we start test on home screen so we can know if clean up is successful
        // or not in the end.
        homeScreenOrBust(sInstrumentation.getContext(), sUiAutomation);
    }

    @After
    public void tearDown() throws Exception {
        // Make sure we clean up and back to home screen again, or let test fail...
        homeScreenOrBust(sInstrumentation.getContext(), sUiAutomation);
    }

    @MediumTest
    @Test
    public void testPerformGlobalActionBack() throws Exception {
        assertTrue(sUiAutomation.performGlobalAction(AccessibilityService.GLOBAL_ACTION_BACK));

        // Sleep a bit so the UI is settled.
        waitForIdle();
    }

    @MediumTest
    @Test
    public void testPerformGlobalActionHome() throws Exception {
        assertTrue(sUiAutomation.performGlobalAction(AccessibilityService.GLOBAL_ACTION_HOME));

        // Sleep a bit so the UI is settled.
        waitForIdle();
    }

    @MediumTest
    @Test
    public void testPerformGlobalActionRecents() throws Exception {
        // Perform the action.
        boolean actionWasPerformed = sUiAutomation.performGlobalAction(
                AccessibilityService.GLOBAL_ACTION_RECENTS);

        // Sleep a bit so the UI is settled.
        waitForIdle();

        if (actionWasPerformed) {
            // This is a necessary since the global action does not
            // dismiss recents until they stop animating. Sigh...
            SystemClock.sleep(5000);
        }
    }

    @MediumTest
    @Test
    public void testPerformGlobalActionNotifications() throws Exception {
        // Perform the action under test
        assertTrue(sUiAutomation.performGlobalAction(
                AccessibilityService.GLOBAL_ACTION_NOTIFICATIONS));

        // Sleep a bit so the UI is settled.
        waitForIdle();
    }

    @MediumTest
    @Test
    public void testPerformGlobalActionQuickSettings() throws Exception {
        // Check whether the action succeeded.
        assertTrue(sUiAutomation.performGlobalAction(
                AccessibilityService.GLOBAL_ACTION_QUICK_SETTINGS));

        // Sleep a bit so the UI is settled.
        waitForIdle();
    }

    @MediumTest
    @Test
    public void testPerformGlobalActionPowerDialog() throws Exception {
        // Check whether the action succeeded.
        assertTrue(sUiAutomation.performGlobalAction(
                AccessibilityService.GLOBAL_ACTION_POWER_DIALOG));

        // Sleep a bit so the UI is settled.
        waitForIdle();
    }

    @MediumTest
    @Test
    public void testPerformActionScreenshot() throws Exception {
        // Action should succeed
        assertTrue(sUiAutomation.performGlobalAction(
                AccessibilityService.GLOBAL_ACTION_TAKE_SCREENSHOT));
        // Ideally should verify that we actually have a screenshot, but it's also possible
        // for the screenshot to fail
        waitForIdle();
    }

    private void waitForIdle() throws TimeoutException {
        sUiAutomation.waitForIdle(TIMEOUT_ACCESSIBILITY_STATE_IDLE, TIMEOUT_ASYNC_PROCESSING);
    }
}
