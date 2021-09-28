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

package android.server.wm.jetpack;

import static android.server.wm.jetpack.ExtensionUtils.assertEqualsState;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeFalse;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.os.IBinder;
import android.server.wm.jetpack.wrapper.TestDeviceState;
import android.server.wm.jetpack.wrapper.TestDisplayFeature;
import android.server.wm.jetpack.wrapper.TestInterfaceCompat;
import android.server.wm.jetpack.wrapper.TestWindowLayoutInfo;

import androidx.annotation.NonNull;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;

import com.google.common.collect.BoundType;
import com.google.common.collect.Range;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import java.util.List;

/**
 * Tests for the {@link androidx.window.extensions} implementation provided on the device (and only
 * if one is available).
 *
 * Build/Install/Run:
 *     atest CtsWindowManagerJetpackTestCases:ExtensionTest
 */
// TODO(b/155343832) add a foldable presubmit target.
@LargeTest
@RunWith(AndroidJUnit4.class)
public class ExtensionTest extends JetpackExtensionTestBase {
    private ActivityTestRule<TestActivity> mActivityTestRule = new ActivityTestRule<>(
            TestActivity.class, false /* initialTouchMode */, false /* launchActivity */);
    private ActivityTestRule<TestConfigChangeHandlingActivity> mConfigHandlingActivityTestRule =
            new ActivityTestRule<>(TestConfigChangeHandlingActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);
    private ActivityTestRule<TestGetWindowLayoutInfoActivity> mGetWindowLayoutInfoActivityTestRule =
            new ActivityTestRule<>(TestGetWindowLayoutInfoActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    /**
     * This chain rule will launch TestActivity before each test starts, and cleanup all activities
     * after each test finishes.
     */
    @Rule
    public TestRule chain = RuleChain.outerRule(mActivityTestRule)
            .around(mConfigHandlingActivityTestRule)
            .around(mGetWindowLayoutInfoActivityTestRule);

    private TestActivity mActivity;
    private TestInterfaceCompat mExtension;
    private IBinder mWindowToken;

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        // Launch activity after the ActivityManagerTestBase clean all package states.
        mActivity = mActivityTestRule.launchActivity(new Intent());
        ExtensionUtils.assumeSupportedDevice(mActivity);

        mExtension = ExtensionUtils.getInterfaceCompat(mActivity);
        assertThat(mExtension).isNotNull();
        mWindowToken = getActivityWindowToken(mActivity);
        assertThat(mWindowToken).isNotNull();
    }

    @Test
    public void testExtensionProvider_hasValidVersion() {
        ExtensionUtils.assertValidVersion();
    }

    @Test
    public void testExtensionInterface_setExtensionCallback() {
        // Make sure that the method can be called without exception.
        mExtension.setExtensionCallback(new TestInterfaceCompat.TestInterfaceCallback() {
            @Override
            public void onDeviceStateChanged(@NonNull TestDeviceState newDeviceState) {}

            @Override
            public void onWindowLayoutChanged(@NonNull IBinder windowToken,
                    @NonNull TestWindowLayoutInfo newLayout) {}
        });
    }

    @Test
    public void testExtensionInterface_getWindowLayoutInfo() {
        // No display feature to compare, finish test early.
        assumeHasDisplayFeatures();

        // Layout must happen after launch
        assertThat(mActivity.waitForLayout()).isTrue();
        TestWindowLayoutInfo windowLayoutInfo = mExtension.getWindowLayoutInfo(mWindowToken);
        assertThat(windowLayoutInfo).isNotNull();

        for (TestDisplayFeature displayFeature : windowLayoutInfo.getDisplayFeatures()) {
            int featureType = displayFeature.getType();
            assertThat(featureType).isAtLeast(TestDisplayFeature.TYPE_FOLD);
            assertThat(featureType).isAtMost(TestDisplayFeature.TYPE_HINGE);

            Rect featureRect = displayFeature.getBounds();
            assertThat(featureRect.width() == 0 && featureRect.height() == 0).isFalse();
            assertThat(featureRect.left).isAtLeast(0);
            assertThat(featureRect.top).isAtLeast(0);
            assertThat(featureRect.right).isAtLeast(0);
            assertThat(featureRect.bottom).isAtLeast(0);
            assertThat(featureRect.right).isAtMost(mActivity.getWidth());
            assertThat(featureRect.bottom).isAtMost(mActivity.getHeight());
        }
    }

    @Test
    public void testExtensionInterface_onWindowLayoutChangeListenerAdded() {
        // Make sure that the method can be called without exception.
        mExtension.onWindowLayoutChangeListenerAdded(mWindowToken);
    }

    @Test
    public void testExtensionInterface_onWindowLayoutChangeListenerRemoved() {
        // Make sure that the method can be called without exception.
        mExtension.onWindowLayoutChangeListenerRemoved(mWindowToken);
    }

    @Test
    public void testExtensionInterface_getDeviceState() {
        TestDeviceState deviceState = mExtension.getDeviceState();
        assertThat(deviceState).isNotNull();

        assertThat(deviceState.getPosture()).isIn(Range.range(
                TestDeviceState.POSTURE_UNKNOWN, BoundType.CLOSED,
                TestDeviceState.POSTURE_FLIPPED, BoundType.CLOSED));
    }

    @Test
    public void testExtensionInterface_onDeviceStateListenersChanged() {
        TestDeviceState deviceState1 = mExtension.getDeviceState();
        mExtension.onDeviceStateListenersChanged(false /* isEmpty */);
        TestDeviceState deviceState2 = mExtension.getDeviceState();
        mExtension.onDeviceStateListenersChanged(true /* isEmpty */);
        TestDeviceState deviceState3 = mExtension.getDeviceState();

        assertEqualsState(deviceState1, deviceState2);
        assertEqualsState(deviceState1, deviceState3);
    }

    @Test
    public void testGetWindowLayoutInfo_activityNotAttached_notReturnIncorrectValue() {
        // No display feature to compare, finish test early.
        assumeHasDisplayFeatures();

        // The value is verified inside TestGetWindowLayoutInfoActivity
        TestGetWindowLayoutInfoActivity.resetResumeCounter();
        TestGetWindowLayoutInfoActivity testGetWindowLayoutInfoActivity =
                mGetWindowLayoutInfoActivityTestRule.launchActivity(new Intent());

        // Make sure the activity has gone through all states.
        assertThat(TestGetWindowLayoutInfoActivity.waitForOnResume()).isTrue();
        assertThat(testGetWindowLayoutInfoActivity.waitForLayout()).isTrue();
    }

    @Test
    public void testGetWindowLayoutInfo_configChanged_windowLayoutUpdates() {
        // No display feature to compare, finish test early.
        assumeHasDisplayFeatures();

        TestConfigChangeHandlingActivity configHandlingActivity =
                mConfigHandlingActivityTestRule.launchActivity(new Intent());
        TestInterfaceCompat extension =
                ExtensionUtils.getInterfaceCompat(configHandlingActivity);
        assertThat(extension).isNotNull();
        IBinder configHandlingActivityWindowToken = getActivityWindowToken(configHandlingActivity);
        assertThat(configHandlingActivityWindowToken).isNotNull();

        configHandlingActivity.resetLayoutCounter();
        configHandlingActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        configHandlingActivity.waitForLayout();
        TestWindowLayoutInfo portraitWindowLayoutInfo =
                extension.getWindowLayoutInfo(configHandlingActivityWindowToken);

        configHandlingActivity.resetLayoutCounter();
        configHandlingActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        // Layout must happen after orientation change.
        assertThat(configHandlingActivity.waitForLayout()).isTrue();
        TestWindowLayoutInfo landscapeWindowLayoutInfo =
                extension.getWindowLayoutInfo(configHandlingActivityWindowToken);

        assertThat(portraitWindowLayoutInfo).isNotEqualTo(landscapeWindowLayoutInfo);
    }

    @Test
    public void testGetWindowLayoutInfo_windowRecreated_windowLayoutUpdates() {
        // No display feature to compare, finish test early.
        assumeHasDisplayFeatures();

        mActivity.resetLayoutCounter();
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        mActivity.waitForLayout();
        TestWindowLayoutInfo portraitWindowLayoutInfo =
                mExtension.getWindowLayoutInfo(mWindowToken);

        TestActivity.resetResumeCounter();
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        TestActivity.waitForOnResume();

        mWindowToken = getActivityWindowToken(mActivity);
        assertThat(mWindowToken).isNotNull();

        mActivity.waitForLayout();
        TestWindowLayoutInfo landscapeWindowLayoutInfo =
                mExtension.getWindowLayoutInfo(mWindowToken);

        assertThat(portraitWindowLayoutInfo).isNotEqualTo(landscapeWindowLayoutInfo);
    }

    /** Skips devices that have no display feature to compare. */
    private void assumeHasDisplayFeatures() {
        TestWindowLayoutInfo windowLayoutInfo = mExtension.getWindowLayoutInfo(mWindowToken);
        assertThat(windowLayoutInfo).isNotNull();
        List<TestDisplayFeature> displayFeatures = windowLayoutInfo.getDisplayFeatures();
        assumeFalse(displayFeatures == null || displayFeatures.isEmpty());
    }
}
