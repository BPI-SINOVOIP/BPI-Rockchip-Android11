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

package android.accessibilityservice.cts;

import static android.accessibilityservice.cts.utils.AccessibilityEventFilterUtils.AccessibilityEventTypeMatcher;
import static android.accessibilityservice.cts.utils.AccessibilityEventFilterUtils.ContentChangesMatcher;
import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.launchActivityAndWaitForItToBeOnscreen;
import static android.accessibilityservice.cts.utils.AsyncUtils.DEFAULT_TIMEOUT_MS;
import static android.view.accessibility.AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_APPEARED;
import static android.view.accessibility.AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_DISAPPEARED;
import static android.view.accessibility.AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_TITLE;
import static android.view.accessibility.AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED;

import static org.hamcrest.Matchers.both;
import static org.junit.Assert.assertEquals;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibilityservice.cts.activities.AccessibilityEndToEndActivity;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.app.UiAutomation.AccessibilityEventFilter;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

/**
 * Tests reporting of window-like views
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityPaneTest {
    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    private Activity mActivity;
    private View mPaneView;

    private ActivityTestRule<AccessibilityEndToEndActivity> mActivityRule =
            new ActivityTestRule<>(AccessibilityEndToEndActivity.class, false, false);

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
    }

    @Before
    public void setUp() throws Exception {
        mActivity = launchActivityAndWaitForItToBeOnscreen(
                sInstrumentation, sUiAutomation, mActivityRule);
        sInstrumentation.runOnMainSync(() ->  {
            mPaneView = mActivity.findViewById(R.id.button);
        });
    }

    @Test
    public void paneTitleFromXml_reportedToAccessibility() {
        String paneTitle = sInstrumentation.getContext().getString(R.string.paneTitle);
        assertEquals(paneTitle, mPaneView.getAccessibilityPaneTitle());
        AccessibilityNodeInfo paneNode = getPaneNode();
        assertEquals(paneTitle, paneNode.getPaneTitle());
    }

    @Test
    public void windowLikeViewSettersWork_andNewValuesReportedToAccessibility() throws Exception {
        final String newTitle = "Here's a new title";

        sUiAutomation.executeAndWaitForEvent(() -> sInstrumentation.runOnMainSync(() -> {
            mPaneView.setAccessibilityPaneTitle(newTitle);
            assertEquals(newTitle, mPaneView.getAccessibilityPaneTitle());
        }), (new ContentChangesMatcher(CONTENT_CHANGE_TYPE_PANE_TITLE))::matches,
                DEFAULT_TIMEOUT_MS);

        AccessibilityNodeInfo windowLikeNode = getPaneNode();
        assertEquals(newTitle, windowLikeNode.getPaneTitle());
    }

    @Test
    public void windowLikeViewVisibility_reportAsWindowStateChanges() throws Exception {
        final AccessibilityEventFilter paneAppearsFilter =
                both(new AccessibilityEventTypeMatcher(TYPE_WINDOW_STATE_CHANGED)).and(
                        new ContentChangesMatcher(CONTENT_CHANGE_TYPE_PANE_APPEARED))::matches;
        final AccessibilityEventFilter paneDisappearsFilter =
                both(new AccessibilityEventTypeMatcher(TYPE_WINDOW_STATE_CHANGED)).and(
                        new ContentChangesMatcher(CONTENT_CHANGE_TYPE_PANE_DISAPPEARED))::matches;
        sUiAutomation.executeAndWaitForEvent(setPaneViewVisibility(View.GONE),
                paneDisappearsFilter, DEFAULT_TIMEOUT_MS);

        sUiAutomation.executeAndWaitForEvent(setPaneViewVisibility(View.VISIBLE),
                paneAppearsFilter, DEFAULT_TIMEOUT_MS);

        sUiAutomation.executeAndWaitForEvent(setPaneViewParentVisibility(View.GONE),
                paneDisappearsFilter, DEFAULT_TIMEOUT_MS);

        sUiAutomation.executeAndWaitForEvent(setPaneViewParentVisibility(View.VISIBLE),
                paneAppearsFilter, DEFAULT_TIMEOUT_MS);
    }

    private AccessibilityNodeInfo getPaneNode() {
        return sUiAutomation.getRootInActiveWindow().findAccessibilityNodeInfosByText(
                ((TextView) mPaneView).getText().toString()).get(0);
    }

    private Runnable setPaneViewVisibility(int visibility) {
        return () -> sInstrumentation.runOnMainSync(
                () -> mPaneView.setVisibility(visibility));
    }

    private Runnable setPaneViewParentVisibility(int visibility) {
        return () -> sInstrumentation.runOnMainSync(
                () -> ((View) mPaneView.getParent()).setVisibility(visibility));
    }
}
