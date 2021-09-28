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

package android.view.accessibility.cts;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.LinearLayout;

import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

/**
 * Tests AccessibilityDelegate functionality
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityDelegateTest {

    private LinearLayout mParentView;
    private View mChildView;

    private ActivityTestRule<DummyActivity> mActivityRule =
            new ActivityTestRule<>(DummyActivity.class, false, false);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mActivityRule)
            // Inner rule capture failure and dump data before finishing activity
            .around(mDumpOnFailureRule);

    @Before
    public void setUp() throws Exception {
        Activity activity = mActivityRule.launchActivity(null);
        LinearLayout grandparent = new LinearLayout(activity);
        mParentView = new LinearLayout(activity);
        mChildView = new View(activity);
        grandparent.addView(mParentView);
        mParentView.addView(mChildView);
    }

    @Test
    public void testAccessibilityDelegateGetAndSet() {
        AccessibilityDelegate delegate = new AccessibilityDelegate();
        mParentView.setAccessibilityDelegate(delegate);
        assertThat(mParentView.getAccessibilityDelegate(), is(equalTo(delegate)));
    }

    @Test
    public void testViewDelegatesToAccessibilityDelegate() {
        AccessibilityDelegate mockDelegate = mock(AccessibilityDelegate.class);
        mParentView.setAccessibilityDelegate(mockDelegate);
        final AccessibilityEvent event = AccessibilityEvent.obtain();

        mParentView.sendAccessibilityEvent(AccessibilityEvent.TYPE_ANNOUNCEMENT);
        verify(mockDelegate).sendAccessibilityEvent(
                mParentView, AccessibilityEvent.TYPE_ANNOUNCEMENT);

        mParentView.sendAccessibilityEventUnchecked(event);
        verify(mockDelegate).sendAccessibilityEventUnchecked(mParentView, event);

        mParentView.dispatchPopulateAccessibilityEvent(event);
        verify(mockDelegate).dispatchPopulateAccessibilityEvent(mParentView, event);

        mParentView.onPopulateAccessibilityEvent(event);
        verify(mockDelegate).onPopulateAccessibilityEvent(mParentView, event);

        mParentView.onInitializeAccessibilityEvent(event);
        verify(mockDelegate).onInitializeAccessibilityEvent(mParentView, event);

        final AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mParentView.onInitializeAccessibilityNodeInfo(info);
        verify(mockDelegate).onInitializeAccessibilityNodeInfo(mParentView, info);

        mParentView.requestSendAccessibilityEvent(mChildView, event);
        verify(mockDelegate).onRequestSendAccessibilityEvent(mParentView, mChildView, event);

        mParentView.getAccessibilityNodeProvider();
        verify(mockDelegate).getAccessibilityNodeProvider(mParentView);

        final Bundle bundle = new Bundle();
        mParentView.performAccessibilityAction(
                    AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, bundle);
        verify(mockDelegate).performAccessibilityAction(
                mParentView, AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, bundle);
    }
}
