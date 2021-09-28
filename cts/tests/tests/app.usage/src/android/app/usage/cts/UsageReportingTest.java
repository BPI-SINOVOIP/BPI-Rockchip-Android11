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
 * limitations under the License
 */
package android.app.usage.cts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.app.usage.UsageStatsManager;
import android.content.ComponentName;
import android.content.Context;
import android.platform.test.annotations.AppModeFull;
import android.server.wm.ActivityManagerTestBase;
import android.support.test.uiautomator.UiDevice;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.TestUtils;

import org.junit.Before;
import org.junit.Test;

/**
 * Test the UsageStats API around usage reporting against tokens
 * Run test: atest CtsUsageStatsTestCases:UsageReportingTest
 */
@AppModeFull
public class UsageReportingTest extends ActivityManagerTestBase {

    private UiDevice mUiDevice;
    private UsageStatsManager mUsageStatsManager;
    private String mTargetPackage;

    private String mFullToken0;
    private String mFullToken1;

    private static final String TOKEN_0 = "SuperSecretToken";
    private static final String TOKEN_1 = "AnotherSecretToken";

    private static final int ASSERT_TIMEOUT_SECONDS = 5; // 5 seconds

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mUsageStatsManager = (UsageStatsManager) InstrumentationRegistry.getInstrumentation()
                .getContext().getSystemService(Context.USAGE_STATS_SERVICE);
        mTargetPackage = InstrumentationRegistry.getTargetContext().getPackageName();

        mFullToken0 = mTargetPackage + "/" + TOKEN_0;
        mFullToken1 = mTargetPackage + "/" + TOKEN_1;
    }

    @Test
    public void testUsageStartAndStopReporting() throws Exception {
        launchActivity(new ComponentName(mTargetPackage, Activities.ActivityOne.class.getName()));

        Activity activity;
        synchronized ( Activities.startedActivities) {
            activity = Activities.startedActivities.valueAt(0);
        }

        mUsageStatsManager.reportUsageStart(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        mUsageStatsManager.reportUsageStop(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, false);


        mUsageStatsManager.reportUsageStart(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        mUsageStatsManager.reportUsageStop(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, false);
    }

    @Test
    public void testUsagePastReporting() throws Exception {
        launchActivity(new ComponentName(mTargetPackage, Activities.ActivityOne.class.getName()));

        Activity activity;
        synchronized ( Activities.startedActivities) {
            activity = Activities.startedActivities.valueAt(0);
        }

        mUsageStatsManager.reportUsageStart(activity, TOKEN_0, 100);
        assertAppOrTokenUsed(mFullToken0, true);

        mUsageStatsManager.reportUsageStop(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, false);
    }

    @Test
    public void testUsageReportingMissingStop() throws Exception {
        launchActivity(new ComponentName(mTargetPackage, Activities.ActivityOne.class.getName()));

        Activity activity;
        synchronized ( Activities.startedActivities) {
            activity = Activities.startedActivities.valueAt(0);
        }

        mUsageStatsManager.reportUsageStart(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        // Send the device to sleep to get onStop called for the token reporting activities.
        mUiDevice.sleep();
        Thread.sleep(1000);

        assertAppOrTokenUsed(mFullToken0, false);
    }

    @Test
    public void testExceptionOnRepeatReport() throws Exception {
        launchActivity(new ComponentName(mTargetPackage, Activities.ActivityOne.class.getName()));

        Activity activity;
        synchronized ( Activities.startedActivities) {
            activity = Activities.startedActivities.valueAt(0);
        }

        mUsageStatsManager.reportUsageStart(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        try {
            mUsageStatsManager.reportUsageStart(activity, TOKEN_0);
            fail("Should have thrown an IllegalArgumentException for double reporting start");
        } catch (IllegalArgumentException iae) {
            //Expected exception
        }
        assertAppOrTokenUsed(mFullToken0, true);

        mUsageStatsManager.reportUsageStop(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, false);


        try {
            mUsageStatsManager.reportUsageStop(activity, TOKEN_0);
            fail("Should have thrown an IllegalArgumentException for double reporting stop");
        } catch (IllegalArgumentException iae) {
            //Expected exception
        }

        // One more cycle of reporting just to make sure there was no underflow
        mUsageStatsManager.reportUsageStart(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        mUsageStatsManager.reportUsageStop(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, false);
    }

    @Test
    public void testMultipleTokenUsageReporting() throws Exception {
        launchActivity(new ComponentName(mTargetPackage, Activities.ActivityOne.class.getName()));

        Activity activity;
        synchronized ( Activities.startedActivities) {
            activity = Activities.startedActivities.valueAt(0);
        }

        mUsageStatsManager.reportUsageStart(activity, TOKEN_0);
        mUsageStatsManager.reportUsageStart(activity, TOKEN_1);
        assertAppOrTokenUsed(mFullToken0, true);
        assertAppOrTokenUsed(mFullToken1, true);

        mUsageStatsManager.reportUsageStop(activity, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, false);
        assertAppOrTokenUsed(mFullToken1, true);

        mUsageStatsManager.reportUsageStop(activity, TOKEN_1);
        assertAppOrTokenUsed(mFullToken0, false);
        assertAppOrTokenUsed(mFullToken1, false);
    }

    @Test
    public void testMultipleTokenMissingStop() throws Exception {
        launchActivity(new ComponentName(mTargetPackage, Activities.ActivityOne.class.getName()));

        Activity activity;
        synchronized ( Activities.startedActivities) {
            activity = Activities.startedActivities.valueAt(0);
        }

        mUsageStatsManager.reportUsageStart(activity, TOKEN_0);
        mUsageStatsManager.reportUsageStart(activity, TOKEN_1);
        assertAppOrTokenUsed(mFullToken0, true);
        assertAppOrTokenUsed(mFullToken1, true);


        // Send the device to sleep to get onStop called for the token reporting activities.
        mUiDevice.sleep();
        Thread.sleep(1000);

        assertAppOrTokenUsed(mFullToken0, false);
        assertAppOrTokenUsed(mFullToken1, false);
    }

    @Test
    public void testSplitscreenUsageReporting() throws Exception {
        if (!supportsSplitScreenMultiWindow()) {
            // Skipping test: no multi-window support
            return;
        }

        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(
                        new ComponentName(mTargetPackage,
                                Activities.ActivityOne.class.getName())),
                getLaunchActivityBuilder().setTargetActivity(
                        new ComponentName(mTargetPackage,
                                Activities.ActivityTwo.class.getName())));
        Thread.sleep(500);

        Activity activity0;
        Activity activity1;
        synchronized ( Activities.startedActivities) {
            activity0 = Activities.startedActivities.valueAt(0);
            activity1 = Activities.startedActivities.valueAt(1);
        }

        mUsageStatsManager.reportUsageStart(activity0, TOKEN_0);
        mUsageStatsManager.reportUsageStart(activity1, TOKEN_1);
        assertAppOrTokenUsed(mFullToken0, true);
        assertAppOrTokenUsed(mFullToken1, true);

        mUsageStatsManager.reportUsageStop(activity0, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, false);
        assertAppOrTokenUsed(mFullToken1, true);

        mUsageStatsManager.reportUsageStop(activity1, TOKEN_1);
        assertAppOrTokenUsed(mFullToken0, false);
        assertAppOrTokenUsed(mFullToken1, false);
    }

    @Test
    public void testSplitscreenSameToken() throws Exception {
        if (!supportsSplitScreenMultiWindow()) {
            // Skipping test: no multi-window support
            return;
        }

        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(
                        new ComponentName(mTargetPackage,
                                Activities.ActivityOne.class.getName())),
                getLaunchActivityBuilder().setTargetActivity(
                        new ComponentName(mTargetPackage,
                                Activities.ActivityTwo.class.getName())));
        Thread.sleep(500);

        Activity activity0;
        Activity activity1;
        synchronized ( Activities.startedActivities) {
            activity0 = Activities.startedActivities.valueAt(0);
            activity1 = Activities.startedActivities.valueAt(1);
        }

        mUsageStatsManager.reportUsageStart(activity0, TOKEN_0);
        mUsageStatsManager.reportUsageStart(activity1, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        mUsageStatsManager.reportUsageStop(activity0, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        mUsageStatsManager.reportUsageStop(activity1, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, false);
    }

    @Test
    public void testSplitscreenSameTokenOneMissedStop() throws Exception {
        if (!supportsSplitScreenMultiWindow()) {
            // Skipping test: no multi-window support
            return;
        }

        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(
                        new ComponentName(mTargetPackage,
                                Activities.ActivityOne.class.getName())),
                getLaunchActivityBuilder().setTargetActivity(
                        new ComponentName(mTargetPackage,
                                Activities.ActivityTwo.class.getName())));
        Thread.sleep(500);

        Activity activity0;
        Activity activity1;
        synchronized ( Activities.startedActivities) {
            activity0 = Activities.startedActivities.valueAt(0);
            activity1 = Activities.startedActivities.valueAt(1);
        }

        mUsageStatsManager.reportUsageStart(activity0, TOKEN_0);
        mUsageStatsManager.reportUsageStart(activity1, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        mUsageStatsManager.reportUsageStop(activity0, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        // Send the device to sleep to get onStop called for the token reporting activities.
        mUiDevice.sleep();
        Thread.sleep(1000);
        assertAppOrTokenUsed(mFullToken0, false);
    }

    @Test
    public void testSplitscreenSameTokenTwoMissedStop() throws Exception {
        if (!supportsSplitScreenMultiWindow()) {
            // Skipping test: no multi-window support
            return;
        }

        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(
                        new ComponentName(mTargetPackage,
                                Activities.ActivityOne.class.getName())),
                getLaunchActivityBuilder().setTargetActivity(
                        new ComponentName(mTargetPackage,
                                Activities.ActivityTwo.class.getName())));
        Thread.sleep(500);

        Activity activity0;
        Activity activity1;
        synchronized ( Activities.startedActivities) {
            activity0 = Activities.startedActivities.valueAt(0);
            activity1 = Activities.startedActivities.valueAt(1);
        }

        mUsageStatsManager.reportUsageStart(activity0, TOKEN_0);
        mUsageStatsManager.reportUsageStart(activity1, TOKEN_0);
        assertAppOrTokenUsed(mFullToken0, true);

        // Send the device to keyguard to get onStop called for the token reporting activities.
        try (final LockScreenSession lockScreenSession = new LockScreenSession()) {
            lockScreenSession.gotoKeyguard();
            Thread.sleep(1000);
            assertAppOrTokenUsed(mFullToken0, false);
        }
    }

    private void assertAppOrTokenUsed(String entity, boolean expected) throws Exception {
        final String failMessage;
        if (expected) {
            failMessage = entity + " not found in list of active activities and tokens";
        } else {
            failMessage = entity + " found in list of active activities and tokens";
        }

        TestUtils.waitUntil(failMessage, ASSERT_TIMEOUT_SECONDS, () -> {
            final String activeUsages =
                    mUiDevice.executeShellCommand("dumpsys usagestats apptimelimit actives");
            final String[] actives = activeUsages.split("\n");
            boolean found = false;

            for (String active: actives) {
                if (active.equals(entity)) {
                    found = true;
                    break;
                }
            }
            return found == expected;
        });
    }
}
