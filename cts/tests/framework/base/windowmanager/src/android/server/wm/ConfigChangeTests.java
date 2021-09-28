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

package android.server.wm;

import static android.server.wm.StateLogger.log;
import static android.server.wm.StateLogger.logE;
import static android.server.wm.WindowManagerState.STATE_RESUMED;
import static android.server.wm.app.Components.FONT_SCALE_ACTIVITY;
import static android.server.wm.app.Components.FONT_SCALE_NO_RELAUNCH_ACTIVITY;
import static android.server.wm.app.Components.FontScaleActivity.EXTRA_FONT_ACTIVITY_DPI;
import static android.server.wm.app.Components.FontScaleActivity.EXTRA_FONT_PIXEL_SIZE;
import static android.server.wm.app.Components.NO_RELAUNCH_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.server.wm.app.Components.TestActivity.EXTRA_CONFIG_ASSETS_SEQ;
import static android.view.Surface.ROTATION_0;
import static android.view.Surface.ROTATION_180;
import static android.view.Surface.ROTATION_270;
import static android.view.Surface.ROTATION_90;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.content.ComponentName;
import android.os.Bundle;
import android.platform.test.annotations.Presubmit;
import android.provider.Settings;
import android.server.wm.CommandSession.ActivityCallback;
import android.server.wm.TestJournalProvider.TestJournalContainer;
import android.server.wm.settings.SettingsSession;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.Test;

import java.util.Arrays;
import java.util.List;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:ConfigChangeTests
 */
@Presubmit
public class ConfigChangeTests extends ActivityManagerTestBase {

    private static final float EXPECTED_FONT_SIZE_SP = 10.0f;

    @Test
    public void testRotation90Relaunch() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        // Should relaunch on every rotation and receive no onConfigurationChanged()
        testRotation(TEST_ACTIVITY, 1, 1, 0);
    }

    @Test
    public void testRotation90NoRelaunch() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        // Should receive onConfigurationChanged() on every rotation and no relaunch
        testRotation(NO_RELAUNCH_ACTIVITY, 1, 0, 1);
    }

    @Test
    public void testRotation180_RegularActivity() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());
        assumeFalse("Skipping test: display cutout present, can't predict exact lifecycle",
                hasDisplayCutout());

        // Should receive nothing
        testRotation(TEST_ACTIVITY, 2, 0, 0);
    }

    @Test
    public void testRotation180_NoRelaunchActivity() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());
        assumeFalse("Skipping test: display cutout present, can't predict exact lifecycle",
                hasDisplayCutout());

        // Should receive nothing
        testRotation(NO_RELAUNCH_ACTIVITY, 2, 0, 0);
    }

    /**
     * Test activity configuration changes for devices with cutout(s). Landscape and
     * reverse-landscape rotations should result in same screen space available for apps.
     */
    @Test
    public void testRotation180RelaunchWithCutout() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());
        assumeTrue("Skipping test: no display cutout", hasDisplayCutout());

        testRotation180WithCutout(TEST_ACTIVITY, false /* canHandleConfigChange */);
    }

    @Test
    public void testRotation180NoRelaunchWithCutout() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());
        assumeTrue("Skipping test: no display cutout", hasDisplayCutout());

        testRotation180WithCutout(NO_RELAUNCH_ACTIVITY, true /* canHandleConfigChange */);
    }

    private void testRotation180WithCutout(ComponentName activityName,
            boolean canHandleConfigChange) {
        launchActivity(activityName);
        mWmState.computeState(activityName);

        final RotationSession rotationSession = createManagedRotationSession();
        final ActivityLifecycleCounts count1 = getLifecycleCountsForRotation(activityName,
                rotationSession, ROTATION_0 /* before */, ROTATION_180 /* after */,
                canHandleConfigChange);
        final int configChangeCount1 = count1.getCount(ActivityCallback.ON_CONFIGURATION_CHANGED);
        final int relaunchCount1 = count1.getCount(ActivityCallback.ON_CREATE);

        final ActivityLifecycleCounts count2 = getLifecycleCountsForRotation(activityName,
                rotationSession, ROTATION_90 /* before */, ROTATION_270 /* after */,
                canHandleConfigChange);
        final int configChangeCount2 = count2.getCount(ActivityCallback.ON_CONFIGURATION_CHANGED);
        final int relaunchCount2 = count2.getCount(ActivityCallback.ON_CREATE);

        final int configChange = configChangeCount1 + configChangeCount2;
        final int relaunch = relaunchCount1 + relaunchCount2;
        if (canHandleConfigChange) {
            assertWithMessage("There must be at most one 180 degree rotation that results in the"
                    + " same configuration.").that(configChange).isLessThan(2);
            assertEquals("There must be no relaunch during test", 0, relaunch);
            return;
        }

        // If the size change does not cross the threshold, the activity will receive
        // onConfigurationChanged instead of relaunching.
        assertWithMessage("There must be at most one 180 degree rotation that results in relaunch"
                + " or a configuration change.").that(relaunch + configChange).isLessThan(2);

        final boolean resize1 = configChangeCount1 + relaunchCount1 > 0;
        final boolean resize2 = configChangeCount2 + relaunchCount2 > 0;
        // There should at least one 180 rotation without resize.
        final boolean sameSize = !resize1 || !resize2;

        assertTrue("A device with cutout should have the same available screen space"
                + " in landscape and reverse-landscape", sameSize);
    }

    private void prepareRotation(ComponentName activityName, RotationSession session,
            int currentRotation, int initialRotation, boolean canHandleConfigChange) {
        final boolean is90DegreeDelta = Math.abs(currentRotation - initialRotation) % 2 != 0;
        if (is90DegreeDelta) {
            separateTestJournal();
        }
        session.set(initialRotation);
        if (is90DegreeDelta) {
            // Consume the changes of "before" rotation to make sure the activity is in a stable
            // state to apply "after" rotation.
            final ActivityCallback expectedCallback = canHandleConfigChange
                    ? ActivityCallback.ON_CONFIGURATION_CHANGED
                    : ActivityCallback.ON_CREATE;
            Condition.waitFor(new ActivityLifecycleCounts(activityName)
                    .countWithRetry("activity rotated with 90 degree delta",
                            countSpec(expectedCallback, CountSpec.GREATER_THAN, 0)));
        }
    }

    private ActivityLifecycleCounts getLifecycleCountsForRotation(ComponentName activityName,
            RotationSession session, int before, int after, boolean canHandleConfigChange)  {
        final int currentRotation = mWmState.getRotation();
        // The test verifies the events from "before" rotation to "after" rotation. So when
        // preparing "before" rotation, the changes should be consumed to avoid being mixed into
        // the result to verify.
        prepareRotation(activityName, session, currentRotation, before, canHandleConfigChange);
        separateTestJournal();
        session.set(after);
        mWmState.computeState(activityName);
        return new ActivityLifecycleCounts(activityName);
    }

    @Test
    public void testChangeFontScaleRelaunch() {
        // Should relaunch and receive no onConfigurationChanged()
        testChangeFontScale(FONT_SCALE_ACTIVITY, true /* relaunch */);
    }

    @Test
    public void testChangeFontScaleNoRelaunch() {
        // Should receive onConfigurationChanged() and no relaunch
        testChangeFontScale(FONT_SCALE_NO_RELAUNCH_ACTIVITY, false /* relaunch */);
    }

    private void testRotation(ComponentName activityName, int rotationStep, int numRelaunch,
            int numConfigChange) {
        launchActivity(activityName);
        mWmState.computeState(activityName);

        final int initialRotation = 4 - rotationStep;
        final RotationSession rotationSession = createManagedRotationSession();
        prepareRotation(activityName, rotationSession, mWmState.getRotation(), initialRotation,
                numConfigChange > 0);
        final int actualStackId =
                mWmState.getTaskByActivity(activityName).mRootTaskId;
        final int displayId = mWmState.getRootTask(actualStackId).mDisplayId;
        final int newDeviceRotation = getDeviceRotation(displayId);
        if (newDeviceRotation == INVALID_DEVICE_ROTATION) {
            logE("Got an invalid device rotation value. "
                    + "Continuing the test despite of that, but it is likely to fail.");
        } else if (newDeviceRotation != initialRotation) {
            log("This device doesn't support user rotation "
                    + "mode. Not continuing the rotation checks.");
            return;
        }

        for (int rotation = 0; rotation < 4; rotation += rotationStep) {
            separateTestJournal();
            rotationSession.set(rotation);
            mWmState.computeState(activityName);
            assertRelaunchOrConfigChanged(activityName, numRelaunch, numConfigChange);
        }
    }

    /** Helper class to save, set, and restore font_scale preferences. */
    private static class FontScaleSession extends SettingsSession<Float> {
        FontScaleSession() {
            super(Settings.System.getUriFor(Settings.System.FONT_SCALE),
                    Settings.System::getFloat,
                    Settings.System::putFloat);
        }
    }

    private void testChangeFontScale(ComponentName activityName, boolean relaunch) {
        final FontScaleSession fontScaleSession = mObjectTracker.manage(new FontScaleSession());
        fontScaleSession.set(1.0f);
        separateTestJournal();
        launchActivity(activityName);
        mWmState.computeState(activityName);

        final Bundle extras = TestJournalContainer.get(activityName).extras;
        if (!extras.containsKey(EXTRA_FONT_ACTIVITY_DPI)) {
            fail("No fontActivityDpi reported from activity " + activityName);
        }
        final int densityDpi = extras.getInt(EXTRA_FONT_ACTIVITY_DPI);

        for (float fontScale = 0.85f; fontScale <= 1.3f; fontScale += 0.15f) {
            separateTestJournal();
            fontScaleSession.set(fontScale);
            mWmState.computeState(activityName);
            assertRelaunchOrConfigChanged(activityName, relaunch ? 1 : 0, relaunch ? 0 : 1);

            // Verify that the display metrics are updated, and therefore the text size is also
            // updated accordingly.
            final Bundle changedExtras = TestJournalContainer.get(activityName).extras;
            waitForOrFail("reported fontPixelSize from " + activityName,
                    () -> changedExtras.containsKey(EXTRA_FONT_PIXEL_SIZE));
            final int expectedFontPixelSize =
                    scaledPixelsToPixels(EXPECTED_FONT_SIZE_SP, fontScale, densityDpi);
            assertEquals("Expected font pixel size should match", expectedFontPixelSize,
                    changedExtras.getInt(EXTRA_FONT_PIXEL_SIZE));
        }
    }

    /**
     * Test updating application info when app is running. An activity with matching package name
     * must be recreated and its asset sequence number must be incremented.
     */
    @Test
    public void testUpdateApplicationInfo() throws Exception {
        separateTestJournal();

        // Launch an activity that prints applied config.
        launchActivity(TEST_ACTIVITY);
        final int assetSeq = getAssetSeqNumber(TEST_ACTIVITY);

        separateTestJournal();
        // Update package info.
        updateApplicationInfo(Arrays.asList(TEST_ACTIVITY.getPackageName()));
        mWmState.waitForWithAmState((amState) -> {
            // Wait for activity to be resumed and asset seq number to be updated.
            try {
                return getAssetSeqNumber(TEST_ACTIVITY) == assetSeq + 1
                        && amState.hasActivityState(TEST_ACTIVITY, STATE_RESUMED);
            } catch (Exception e) {
                logE("Error waiting for valid state: " + e.getMessage());
                return false;
            }
        }, "asset sequence number to be updated and for activity to be resumed.");

        // Check if activity is relaunched and asset seq is updated.
        assertRelaunchOrConfigChanged(TEST_ACTIVITY, 1 /* numRelaunch */,
                0 /* numConfigChange */);
        final int newAssetSeq = getAssetSeqNumber(TEST_ACTIVITY);
        assertEquals("Asset sequence number must be incremented.", assetSeq + 1, newAssetSeq);
    }

    private static int getAssetSeqNumber(ComponentName activityName) {
        return TestJournalContainer.get(activityName).extras.getInt(EXTRA_CONFIG_ASSETS_SEQ);
    }

    // Calculate the scaled pixel size just like the device is supposed to.
    private static int scaledPixelsToPixels(float sp, float fontScale, int densityDpi) {
        final int DEFAULT_DENSITY = 160;
        float f = densityDpi * (1.0f / DEFAULT_DENSITY) * fontScale * sp;
        return (int) ((f >= 0) ? (f + 0.5f) : (f - 0.5f));
    }

    private void updateApplicationInfo(List<String> packages) {
        SystemUtil.runWithShellPermissionIdentity(
                () -> mAm.scheduleApplicationInfoChanged(packages,
                        android.os.Process.myUserHandle().getIdentifier())
        );
    }
}
