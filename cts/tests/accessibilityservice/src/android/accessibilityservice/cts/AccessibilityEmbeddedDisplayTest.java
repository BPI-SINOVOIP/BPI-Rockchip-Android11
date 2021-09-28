/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.accessibilityservice.cts;

import static android.accessibilityservice.cts.utils.AccessibilityEventFilterUtils.filterWindowsChangeTypesAndWindowTitle;
import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.findWindowByTitle;
import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.launchActivityAndWaitForItToBeOnscreen;
import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.supportsMultiDisplay;
import static android.accessibilityservice.cts.utils.AsyncUtils.DEFAULT_TIMEOUT_MS;
import static android.content.pm.PackageManager.FEATURE_ACTIVITIES_ON_SECONDARY_DISPLAYS;
import static android.view.accessibility.AccessibilityEvent.WINDOWS_CHANGE_ADDED;
import static android.view.accessibility.AccessibilityEvent.WINDOWS_CHANGE_BOUNDS;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.accessibilityservice.cts.activities.AccessibilityTestActivity;
import android.app.Activity;
import android.app.ActivityView;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.platform.test.annotations.Presubmit;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

/**
 * Tests that AccessibilityWindowInfos and AccessibilityNodeInfos from a window on an embedded
 * display that is re-parented to another window are properly populated.
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityEmbeddedDisplayTest {
    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    private EmbeddedDisplayParentActivity mActivity;
    private EmbeddedDisplayActivity mEmbeddedDisplayActivity;
    private ActivityView mActivityView;
    private Context mContext;

    private String mParentActivityTitle;
    private String mActivityTitle;

    private final ActivityTestRule<EmbeddedDisplayParentActivity> mActivityRule =
            new ActivityTestRule<>(EmbeddedDisplayParentActivity.class, false, false);

    private final AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mActivityRule)
            .around(mDumpOnFailureRule);

    @BeforeClass
    public static void oneTimeSetup() {
        sInstrumentation = InstrumentationRegistry.getInstrumentation();
        sUiAutomation = sInstrumentation.getUiAutomation();
    }

    @AfterClass
    public static void postTestTearDown() {
        sUiAutomation.destroy();
    }

    @Before
    public void setUp() throws Exception {
        mContext = sInstrumentation.getContext();
        assumeTrue(supportsMultiDisplay(mContext));

        mParentActivityTitle = mContext.getString(
                R.string.accessibility_embedded_display_test_parent_activity);
        mActivityTitle = mContext.getString(R.string.accessibility_embedded_display_test_activity);

        SystemUtil.runWithShellPermissionIdentity(() -> {
            mActivity = launchActivityAndWaitForItToBeOnscreen(
                    sInstrumentation, sUiAutomation, mActivityRule);
            mActivityView = mActivity.getActivityView();
        });

        launchActivityInActivityView();
    }

    @After
    public void tearDown() throws Exception {
        mEmbeddedDisplayActivity = null;
        if (mActivityView != null) {
            SystemUtil.runWithShellPermissionIdentity(() -> mActivityView.release());
        }
    }

    @Presubmit
    @Test
    public void testA11yWindowInfoHasCorrectLayer() {
        final AccessibilityWindowInfo parentActivityWindow =
                findWindowByTitle(sUiAutomation, mParentActivityTitle);
        final AccessibilityWindowInfo activityWindow =
                findWindowByTitle(sUiAutomation, mActivityTitle);

        assertNotNull(parentActivityWindow);
        assertNotNull(activityWindow);
        assertTrue(parentActivityWindow.getLayer() > activityWindow.getLayer());
    }

    @Presubmit
    @Test
    public void testA11yWindowInfoAndA11yNodeInfoHasCorrectBoundsInScreen() {
        final AccessibilityWindowInfo parentActivityWindow =
                findWindowByTitle(sUiAutomation, mParentActivityTitle);
        final AccessibilityWindowInfo activityWindow =
                findWindowByTitle(sUiAutomation, mActivityTitle);
        final AccessibilityNodeInfo button = findWindowByTitle(sUiAutomation,
                mActivityTitle).getRoot().findAccessibilityNodeInfosByViewId(
                "android.accessibilityservice.cts:id/button").get(0);

        assertNotNull(parentActivityWindow);
        assertNotNull(activityWindow);
        assertNotNull(button);

        final Rect parentActivityBound = new Rect();
        final Rect activityBound = new Rect();
        final Rect buttonBound = new Rect();
        parentActivityWindow.getBoundsInScreen(parentActivityBound);
        activityWindow.getBoundsInScreen(activityBound);
        button.getBoundsInScreen(buttonBound);

        assertTrue("parentActivityBound" + parentActivityBound.toShortString()
                        + " doesn't contain activityBound" + activityBound.toShortString(),
                parentActivityBound.contains(activityBound));
        assertTrue("parentActivityBound" + parentActivityBound.toShortString()
                        + " doesn't contain buttonBound" + buttonBound.toShortString(),
                parentActivityBound.contains(buttonBound));
    }

    @Test
    public void testA11yWindowNotifyWhenResizeWindowInActivityView() throws Exception {
        testA11yWindowNotifyAfterResizeWindowInActivityView();
    }

    @Test
    public void testA11yWindowNotifyWhenResizeWindowInActivityViewAfterServiceOffAndOn()
            throws Exception {
        // Clears window access flag to disable the A11y window tracking.
        AccessibilityServiceInfo info = sUiAutomation.getServiceInfo();
        info.flags &= ~AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;
        sUiAutomation.setServiceInfo(info);

        // Only needs to make sure the windows cannot be accessed for UiAutomation service
        // because other A11y services had been disabled when calling the method, getUiAutomation().
        assertTrue(sUiAutomation.getWindows().isEmpty());

        // Sets window access flag to enable the A11y window tracking.
        sUiAutomation.executeAndWaitForEvent(
                () -> sInstrumentation.runOnMainSync(() -> {
                    // Make sure we get window events, so we'll know when the window appears
                    info.flags |= AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;
                    sUiAutomation.setServiceInfo(info);
                }),
                filterWindowsChangeTypesAndWindowTitle(sUiAutomation, WINDOWS_CHANGE_ADDED,
                        mActivityTitle),
                DEFAULT_TIMEOUT_MS);

        testA11yWindowNotifyAfterResizeWindowInActivityView();
    }

    private void testA11yWindowNotifyAfterResizeWindowInActivityView() throws Exception {
        final AccessibilityWindowInfo oldActivityWindow =
                findWindowByTitle(sUiAutomation, mActivityTitle);
        final Rect activityBound = new Rect();
        oldActivityWindow.getBoundsInScreen(activityBound);

        final int width = activityBound.width() / 2;
        final int height = activityBound.height() / 2;
        sUiAutomation.executeAndWaitForEvent(() -> sInstrumentation.runOnMainSync(
                () -> mEmbeddedDisplayActivity.getWindow().setLayout(width, height)),
                filterWindowsChangeTypesAndWindowTitle(sUiAutomation, WINDOWS_CHANGE_BOUNDS,
                        mActivityTitle),
                DEFAULT_TIMEOUT_MS);

        final AccessibilityWindowInfo newActivityWindow =
                findWindowByTitle(sUiAutomation, mActivityTitle);
        newActivityWindow.getBoundsInScreen(activityBound);

        assertEquals(height, activityBound.height());
        assertEquals(width, activityBound.width());
    }

    private void launchActivityInActivityView() throws Exception {
        final Instrumentation.ActivityMonitor am = sInstrumentation.addMonitor(
                EmbeddedDisplayActivity.class.getName(), null, false);
        final Rect bounds = new Rect();
        sUiAutomation.executeAndWaitForEvent(
                () -> sInstrumentation.runOnMainSync(() -> {
                    Intent intent = new Intent(mContext, EmbeddedDisplayActivity.class);
                    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    SystemUtil.runWithShellPermissionIdentity(
                            () -> mActivityView.startActivity(intent));
                }),
                (event) -> {
                    // Ensure the target activity is shown
                    final AccessibilityWindowInfo window =
                            findWindowByTitle(sUiAutomation, mActivityTitle);
                    if (window == null) {
                        return false;
                    }
                    window.getBoundsInScreen(bounds);
                    return !bounds.isEmpty();
                }, DEFAULT_TIMEOUT_MS);
        mEmbeddedDisplayActivity = (EmbeddedDisplayActivity)
                am.waitForActivityWithTimeout(DEFAULT_TIMEOUT_MS);
        assertNotNull(mEmbeddedDisplayActivity);
    }

    public static class EmbeddedDisplayParentActivity extends Activity {
        private ActivityView mActivityView;

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            mActivityView = new ActivityView(this, null, 0, false, true);
            setContentView(mActivityView);
        }

        ActivityView getActivityView() {
            return mActivityView;
        }
    }

    public static class EmbeddedDisplayActivity extends AccessibilityTestActivity {

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.accessibility_embedded_display_test);
        }
    }
}
