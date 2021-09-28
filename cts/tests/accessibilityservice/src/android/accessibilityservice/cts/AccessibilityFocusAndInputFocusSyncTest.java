/**
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.accessibilityservice.cts;

import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.launchActivityAndWaitForItToBeOnscreen;
import static android.accessibilityservice.cts.utils.AsyncUtils.DEFAULT_TIMEOUT_MS;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS;
import static android.view.accessibility.AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.accessibilityservice.cts.activities.AccessibilityFocusAndInputFocusSyncActivity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.platform.test.annotations.Presubmit;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test cases for testing the accessibility focus APIs exposed to accessibility
 * services. These APIs allow moving accessibility focus in the view tree from
 * an AccessiiblityService. Specifically, this activity is for verifying the the
 * sync between accessibility and input focus.
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityFocusAndInputFocusSyncTest {
    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    private AccessibilityFocusAndInputFocusSyncActivity mActivity;

    private ActivityTestRule<AccessibilityFocusAndInputFocusSyncActivity> mActivityRule =
            new ActivityTestRule<>(AccessibilityFocusAndInputFocusSyncActivity.class, false, false);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mActivityRule)
            .around(mDumpOnFailureRule);

    @BeforeClass
    public static void oneTimeSetup() throws Exception {
        sInstrumentation = InstrumentationRegistry.getInstrumentation();
        sUiAutomation = sInstrumentation.getUiAutomation();
        AccessibilityServiceInfo info = sUiAutomation.getServiceInfo();
        info.flags |= AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE;
        info.flags &= ~AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS;
        sUiAutomation.setServiceInfo(info);
    }

    @AfterClass
    public static void postTestTearDown() {
        sUiAutomation.destroy();
    }

    @Before
    public void setUp() throws Exception {
        mActivity = launchActivityAndWaitForItToBeOnscreen(
                sInstrumentation, sUiAutomation, mActivityRule);
    }

    @MediumTest
    @Presubmit
    @Test
    public void testFindAccessibilityFocus() throws Exception {
        sInstrumentation.runOnMainSync(() -> {
            mActivity.findViewById(R.id.firstEditText).requestFocus();
        });
        // Get the view that has input and accessibility focus.
        final AccessibilityNodeInfo expected = sUiAutomation
                .getRootInActiveWindow().findAccessibilityNodeInfosByText(
                        sInstrumentation.getContext().getString(R.string.firstEditText)).get(0);
        assertNotNull(expected);
        assertFalse(expected.isAccessibilityFocused());
        assertTrue(expected.isFocused());

        sUiAutomation.executeAndWaitForEvent(
                () -> assertTrue(expected.performAction(ACTION_ACCESSIBILITY_FOCUS)),
                (event) ->
                        event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                DEFAULT_TIMEOUT_MS);

        // Get the second expected node info.
        AccessibilityNodeInfo received = sUiAutomation
                .getRootInActiveWindow().findFocus(AccessibilityNodeInfo.FOCUS_ACCESSIBILITY);
        assertNotNull(received);
        assertTrue(received.isAccessibilityFocused());

        // Make sure we got the expected focusable.
        assertEquals(expected, received);
    }

    @MediumTest
    @Presubmit
    @Test
    public void testInitialStateNoAccessibilityFocus() throws Exception {
        // Get the root which is only accessibility focused.
        AccessibilityNodeInfo focused = sUiAutomation
                .getRootInActiveWindow().findFocus(AccessibilityNodeInfo.FOCUS_ACCESSIBILITY);
        assertNull(focused);
    }

    @MediumTest
    @Test
    public void testActionAccessibilityFocus() throws Exception {
        // Get the root linear layout info.
        final AccessibilityNodeInfo rootLinearLayout = sUiAutomation
                .getRootInActiveWindow().findAccessibilityNodeInfosByText(
                        sInstrumentation.getContext().getString(R.string.rootLinearLayout)).get(0);
        assertNotNull(rootLinearLayout);
        assertFalse(rootLinearLayout.isAccessibilityFocused());

        sUiAutomation.executeAndWaitForEvent(
                () -> assertTrue(rootLinearLayout.performAction(ACTION_ACCESSIBILITY_FOCUS)),
                (event) ->
                        event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                DEFAULT_TIMEOUT_MS);

        // Get the node info again.
        rootLinearLayout.refresh();

        // Check if the node info is focused.
        assertTrue(rootLinearLayout.isAccessibilityFocused());
    }

    @MediumTest
    @Presubmit
    @Test
    public void testActionClearAccessibilityFocus() throws Exception {
        // Get the root linear layout info.
        final AccessibilityNodeInfo rootLinearLayout = sUiAutomation
                .getRootInActiveWindow().findAccessibilityNodeInfosByText(
                        sInstrumentation.getContext().getString(R.string.rootLinearLayout)).get(0);
        assertNotNull(rootLinearLayout);

        sUiAutomation.executeAndWaitForEvent(
                () -> assertTrue(rootLinearLayout.performAction(ACTION_ACCESSIBILITY_FOCUS)),
                (event) ->
                        event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                DEFAULT_TIMEOUT_MS);

        // Refresh the node info.
        rootLinearLayout.refresh();

        // Check if the node info is focused.
        assertTrue(rootLinearLayout.isAccessibilityFocused());

        sUiAutomation.executeAndWaitForEvent(
                () -> assertTrue(rootLinearLayout.performAction(ACTION_CLEAR_ACCESSIBILITY_FOCUS)),
                (event) -> event.getEventType()
                        == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED,
                DEFAULT_TIMEOUT_MS);

        // Refresh the node info.
        rootLinearLayout.refresh();

        // Check if the node info is not focused.
        assertFalse(rootLinearLayout.isAccessibilityFocused());
    }

    @MediumTest
    @Presubmit
    @Test
    public void testOnlyOneNodeHasAccessibilityFocus() throws Exception {
        // Get the first not focused edit text.
        final AccessibilityNodeInfo firstEditText = sUiAutomation
                .getRootInActiveWindow().findAccessibilityNodeInfosByText(
                        sInstrumentation.getContext().getString(R.string.firstEditText)).get(0);
        assertNotNull(firstEditText);
        assertTrue(firstEditText.isFocusable());
        assertFalse(firstEditText.isAccessibilityFocused());

        sUiAutomation.executeAndWaitForEvent(
                () -> assertTrue(firstEditText.performAction(ACTION_ACCESSIBILITY_FOCUS)),
                (event) ->
                        event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                DEFAULT_TIMEOUT_MS);

        // Get the second not focused edit text.
        final AccessibilityNodeInfo secondEditText = sUiAutomation
                .getRootInActiveWindow().findAccessibilityNodeInfosByText(
                        sInstrumentation.getContext().getString(R.string.secondEditText)).get(0);
        assertNotNull(secondEditText);
        assertTrue(secondEditText.isFocusable());
        assertFalse(secondEditText.isFocused());
        assertFalse(secondEditText.isAccessibilityFocused());

        sUiAutomation.executeAndWaitForEvent(
                () -> assertTrue(secondEditText.performAction(ACTION_ACCESSIBILITY_FOCUS)),
                (event) ->
                        event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                DEFAULT_TIMEOUT_MS);

        // Get the node info again.
        secondEditText.refresh();

        // Make sure no other node has accessibility focus.
        AccessibilityNodeInfo root = sUiAutomation.getRootInActiveWindow();
        Queue<AccessibilityNodeInfo> workQueue = new LinkedList<AccessibilityNodeInfo>();
        workQueue.add(root);
        while (!workQueue.isEmpty()) {
            AccessibilityNodeInfo current = workQueue.poll();
            if (current.isAccessibilityFocused() && !current.equals(secondEditText)) {
                fail();
            }
            final int childCount = current.getChildCount();
            for (int i = 0; i < childCount; i++) {
                AccessibilityNodeInfo child = current.getChild(i);
                if (child != null) {
                    workQueue.offer(child);
                }
            }
        }
    }

    @Presubmit
    @Test
    public void testScreenReaderFocusableAttribute_reportedToAccessibility() {
        final AccessibilityNodeInfo secondButton = sUiAutomation.getRootInActiveWindow()
                .findAccessibilityNodeInfosByText(
                        sInstrumentation.getContext().getString(R.string.secondButton)).get(0);
        assertTrue("Screen reader focusability not propagated from xml to accessibility",
                secondButton.isScreenReaderFocusable());

        // Verify the setter and getter work
        final AtomicBoolean isScreenReaderFocusableAtomic = new AtomicBoolean(false);
        sInstrumentation.runOnMainSync(() -> {
            View secondButtonView = mActivity.findViewById(R.id.secondButton);
            secondButtonView.setScreenReaderFocusable(false);
            isScreenReaderFocusableAtomic.set(secondButtonView.isScreenReaderFocusable());
        });

        assertFalse("isScreenReaderFocusable did not change after value set",
                isScreenReaderFocusableAtomic.get());

        secondButton.refresh();
        assertFalse(
                "Screen reader focusability not propagated to accessibility after calling setter",
                secondButton.isScreenReaderFocusable());
    }
}
