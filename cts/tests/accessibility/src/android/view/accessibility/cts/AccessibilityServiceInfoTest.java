/*
 * Copyright (C) 2013 The Android Open Source Project
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

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.pm.PackageManager;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.List;

/**
 * Tests whether accessibility service infos are properly reported. Logically,
 * this test belongs to cts/test/test/accessibilityservice but the tests there
 * are using the new UiAutomation API which disables all currently active
 * accessibility service and the fake service used for implementing the UI
 * automation is not reported through the APIs.
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityServiceInfoTest {
    private static final int FLAGS_MASK = AccessibilityServiceInfo.DEFAULT
            | AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
            | AccessibilityServiceInfo.FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY
            | AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS
            | AccessibilityServiceInfo.FLAG_ENABLE_ACCESSIBILITY_VOLUME
            | AccessibilityServiceInfo.FLAG_REQUEST_ACCESSIBILITY_BUTTON
            | AccessibilityServiceInfo.FLAG_REQUEST_FINGERPRINT_GESTURES
            | AccessibilityServiceInfo.FLAG_SERVICE_HANDLES_DOUBLE_TAP
            | AccessibilityServiceInfo.FLAG_REQUEST_MULTI_FINGER_GESTURES
            | AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE
            | AccessibilityServiceInfo.FLAG_REQUEST_FILTER_KEY_EVENTS
            | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
            | AccessibilityServiceInfo.FLAG_REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK;

    private AccessibilityManager mAccessibilityManager;
    private PackageManager mPackageManager;

    private final InstrumentedAccessibilityServiceTestRule<SpeakingAccessibilityService>
            mSpeakingAccessibilityServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
            SpeakingAccessibilityService.class);

    private final InstrumentedAccessibilityServiceTestRule<VibratingAccessibilityService>
            mVibratingAccessibilityServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
            VibratingAccessibilityService.class);

    private final InstrumentedAccessibilityServiceTestRule<SpeakingAndVibratingAccessibilityService>
            mSpeakingAndVibratingAccessibilityServiceRule =
            new InstrumentedAccessibilityServiceTestRule<>(
                    SpeakingAndVibratingAccessibilityService.class, /* enableService= */ false);

    private final InstrumentedAccessibilityServiceTestRule<AccessibilityButtonService>
            mA11yButtonServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
            AccessibilityButtonService.class, /* enableService= */ false);

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mVibratingAccessibilityServiceRule)
            .around(mSpeakingAccessibilityServiceRule)
            .around(mSpeakingAndVibratingAccessibilityServiceRule)
            .around(mA11yButtonServiceRule)
            // Inner rule capture failure and dump data before finishing a11y service
            .around(new AccessibilityDumpOnFailureRule());

    @Before
    public void setUp() throws Exception {
        mAccessibilityManager = getInstrumentation().getContext().getSystemService(
                AccessibilityManager.class);
        mPackageManager = getInstrumentation().getContext().getPackageManager();
    }

    /**
     * Tests whether a service can that requested it can retrieve
     * window content.
     */
    @MediumTest
    @SuppressWarnings("deprecation")
    @Test
    public void testAccessibilityServiceInfoForEnabledService() {
        final List<AccessibilityServiceInfo> enabledServices =
                mAccessibilityManager.getEnabledAccessibilityServiceList(
                        AccessibilityServiceInfo.FEEDBACK_SPOKEN);
        assertSame(/* message= */ "There should be one speaking service.",
                /* expected= */ 1, enabledServices.size());
        final AccessibilityServiceInfo speakingService = enabledServices.get(0);
        assertSame(AccessibilityEvent.TYPES_ALL_MASK, speakingService.eventTypes);
        assertSame(AccessibilityServiceInfo.FEEDBACK_SPOKEN, speakingService.feedbackType);

        int serviceFlags = AccessibilityServiceInfo.DEFAULT
                | AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
                | AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE
                | AccessibilityServiceInfo.FLAG_REQUEST_FILTER_KEY_EVENTS
                | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
                | AccessibilityServiceInfo.FLAG_REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK;

        assertEquals(speakingService.flags & FLAGS_MASK, serviceFlags);

        assertSame(/* expected= */ 0l, speakingService.notificationTimeout);
        assertEquals(/* expected= */ "Some description", speakingService.getDescription());
        assertNull(speakingService.packageNames /*all packages*/);
        assertNotNull(speakingService.getId());
        assertSame(speakingService.getCapabilities(),
                AccessibilityServiceInfo.CAPABILITY_CAN_REQUEST_FILTER_KEY_EVENTS
                | AccessibilityServiceInfo.CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION
                | AccessibilityServiceInfo.CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT);
        assertEquals("foo.bar.Activity", speakingService.getSettingsActivityName());
        assertEquals(/* expected= */ "Some description",
                speakingService.loadDescription(mPackageManager));
        assertEquals(/* expected= */ "Some summary",
                speakingService.loadSummary(mPackageManager));
        assertNotNull(speakingService.getResolveInfo());
        assertEquals(/* expected= */ 6000,
                speakingService.getInteractiveUiTimeoutMillis());
        assertEquals(/* expected= */ 1000,
                speakingService.getNonInteractiveUiTimeoutMillis());
    }
}
