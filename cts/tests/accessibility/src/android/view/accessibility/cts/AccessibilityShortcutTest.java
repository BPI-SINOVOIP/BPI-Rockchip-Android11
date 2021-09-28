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
 * limitations under the License.
 */

package android.view.accessibility.cts;

import static android.accessibility.cts.common.AccessibilityShortcutSettingsRule.ACCESSIBILITY_BUTTON;
import static android.accessibility.cts.common.InstrumentedAccessibilityService.TIMEOUT_SERVICE_ENABLE;
import static android.app.UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.AccessibilityShortcutSettingsRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityButtonController;
import android.accessibilityservice.AccessibilityButtonController.AccessibilityButtonCallback;
import android.accessibilityservice.AccessibilityService;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Service;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.platform.test.annotations.AppModeFull;
import android.view.accessibility.AccessibilityManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.TestUtils;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.Collections;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests accessibility shortcut related functionality
 */
@AppModeFull
@RunWith(AndroidJUnit4.class)
public class AccessibilityShortcutTest {
    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    private final InstrumentedAccessibilityServiceTestRule<SpeakingAccessibilityService>
            mServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
                    SpeakingAccessibilityService.class, false);

    private final InstrumentedAccessibilityServiceTestRule<AccessibilityButtonService>
            mA11yButtonServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
            AccessibilityButtonService.class, false);

    private final AccessibilityShortcutSettingsRule mShortcutSettingsRule =
            new AccessibilityShortcutSettingsRule();

    private final AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mServiceRule)
            .around(mA11yButtonServiceRule)
            .around(mShortcutSettingsRule)
            .around(mDumpOnFailureRule);

    private AccessibilityManager mAccessibilityManager;
    private ActivityMonitor mActivityMonitor;
    private Activity mShortcutTargetActivity;

    private String mSpeakingA11yServiceName;
    private String mShortcutTargetActivityName;
    private String mA11yButtonServiceName;

    @BeforeClass
    public static void oneTimeSetup() {
        sInstrumentation = InstrumentationRegistry.getInstrumentation();
        sUiAutomation = sInstrumentation.getUiAutomation(FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
    }

    @AfterClass
    public static void postTestTearDown() {
        sUiAutomation.destroy();
    }

    @Before
    public void setUp() {
        final Context context = sInstrumentation.getTargetContext();
        mAccessibilityManager = (AccessibilityManager) context.getSystemService(
                Service.ACCESSIBILITY_SERVICE);
        mSpeakingA11yServiceName = new ComponentName(context,
                SpeakingAccessibilityService.class).flattenToString();
        mShortcutTargetActivityName = new ComponentName(context,
                AccessibilityShortcutTargetActivity.class).flattenToString();
        mA11yButtonServiceName = new ComponentName(context,
                AccessibilityButtonService.class).flattenToString();
        mActivityMonitor = new ActivityMonitor(
                AccessibilityShortcutTargetActivity.class.getName(), null, false);
        sInstrumentation.addMonitor(mActivityMonitor);
    }

    @After
    public void tearDown() {
        if (mActivityMonitor != null) {
            sInstrumentation.removeMonitor(mActivityMonitor);
        }
        if (mShortcutTargetActivity != null) {
            sInstrumentation.runOnMainSync(() -> mShortcutTargetActivity.finish());
        }
    }

    @Test
    public void performAccessibilityShortcut_withoutPermission_throwsSecurityException() {
        try {
            mAccessibilityManager.performAccessibilityShortcut();
            fail("No security exception thrown when performing shortcut without permission");
        } catch (SecurityException e) {
            // Expected
        }
    }

    @Test
    public void performAccessibilityShortcut_launchAccessibilityService() {
        mShortcutSettingsRule.configureAccessibilityShortcut(
                sUiAutomation, mSpeakingA11yServiceName);
        mShortcutSettingsRule.waitForAccessibilityShortcutStateChange(
                sUiAutomation, Arrays.asList(mSpeakingA11yServiceName));

        runWithShellPermissionIdentity(sUiAutomation,
                () -> mAccessibilityManager.performAccessibilityShortcut());

        // Make sure the service starts up
        final SpeakingAccessibilityService service = mServiceRule.getService();
        assertTrue("Speaking accessibility service starts up", service != null);
    }

    @Test
    public void performAccessibilityShortcut_launchShortcutTargetActivity() {
        mShortcutSettingsRule.configureAccessibilityShortcut(
                sUiAutomation, mShortcutTargetActivityName);
        mShortcutSettingsRule.waitForAccessibilityShortcutStateChange(
                sUiAutomation, Arrays.asList(mShortcutTargetActivityName));

        runWithShellPermissionIdentity(sUiAutomation,
                () -> mAccessibilityManager.performAccessibilityShortcut());

        // Make sure the activity starts up
        mShortcutTargetActivity = mActivityMonitor.waitForActivityWithTimeout(
                TIMEOUT_SERVICE_ENABLE);
        assertTrue("Accessibility shortcut target starts up",
                mShortcutTargetActivity != null);
    }

    @Test
    public void performAccessibilityShortcut_withReqA11yButtonService_a11yButtonCallback() {
        mA11yButtonServiceRule.enableService();
        mShortcutSettingsRule.configureAccessibilityShortcut(
                sUiAutomation, mA11yButtonServiceName);
        mShortcutSettingsRule.waitForAccessibilityShortcutStateChange(
                sUiAutomation, Arrays.asList(mA11yButtonServiceName));

        performShortcutAndWaitForA11yButtonClicked(mA11yButtonServiceRule.getService());
    }

    @Test
    public void getAccessibilityShortcut_withoutPermission_throwsSecurityException() {
        try {
            mAccessibilityManager.getAccessibilityShortcutTargets(ACCESSIBILITY_BUTTON);
            fail("No security exception thrown when get shortcut without permission");
        } catch (SecurityException e) {
            // Expected
        }
    }

    @Test
    public void getAccessibilityButton_assignedTarget_returnAssignedTarget() {
        mShortcutSettingsRule.configureAccessibilityButton(
                sUiAutomation, mSpeakingA11yServiceName);
        mShortcutSettingsRule.waitForAccessibilityButtonStateChange(
                sUiAutomation, Arrays.asList(mSpeakingA11yServiceName));
    }

    @Test
    public void getAccessibilityButton_assignedMultipleTargets_returnMultipleTargets() {
        mShortcutSettingsRule.configureAccessibilityButton(sUiAutomation,
                mSpeakingA11yServiceName, mShortcutTargetActivityName);
        mShortcutSettingsRule.waitForAccessibilityButtonStateChange(sUiAutomation,
                Arrays.asList(mSpeakingA11yServiceName, mShortcutTargetActivityName));
    }

    @Test
    public void testAccessibilityButtonService_disableSelf_buttonRemoved() {
        mA11yButtonServiceRule.enableService();
        mShortcutSettingsRule.configureAccessibilityButton(
                sUiAutomation, mA11yButtonServiceName);
        mShortcutSettingsRule.waitForAccessibilityButtonStateChange(
                sUiAutomation, Arrays.asList(mA11yButtonServiceName));

        mA11yButtonServiceRule.getService().disableSelfAndRemove();
        mShortcutSettingsRule.waitForAccessibilityButtonStateChange(sUiAutomation,
                Collections.emptyList());
    }

    @Test
    public void testAccessibilityButtonService_disableSelf_shortcutRemoved() {
        mA11yButtonServiceRule.enableService();
        mShortcutSettingsRule.configureAccessibilityShortcut(
                sUiAutomation, mA11yButtonServiceName);
        mShortcutSettingsRule.waitForAccessibilityShortcutStateChange(
                sUiAutomation, Arrays.asList(mA11yButtonServiceName));

        mA11yButtonServiceRule.getService().disableSelfAndRemove();
        mShortcutSettingsRule.waitForAccessibilityShortcutStateChange(sUiAutomation,
                Collections.emptyList());
    }

    /**
     * Perform shortcut and wait for accessibility button clicked call back.
     *
     * @param service The accessibility service
     */
    private void performShortcutAndWaitForA11yButtonClicked(AccessibilityService service) {
        final AtomicBoolean clicked = new AtomicBoolean();
        final AccessibilityButtonCallback callback = new AccessibilityButtonCallback() {
            @Override
            public void onClicked(AccessibilityButtonController controller) {
                synchronized (clicked) {
                    clicked.set(true);
                    clicked.notifyAll();
                }
            }

            @Override
            public void onAvailabilityChanged(AccessibilityButtonController controller,
                    boolean available) {
                /* do nothing */
            }
        };
        try {
            service.getAccessibilityButtonController()
                    .registerAccessibilityButtonCallback(callback);
            runWithShellPermissionIdentity(sUiAutomation,
                    () -> mAccessibilityManager.performAccessibilityShortcut());
            TestUtils.waitOn(clicked, () -> clicked.get(), TIMEOUT_SERVICE_ENABLE,
                    "Wait for a11y button clicked");
        } finally {
            service.getAccessibilityButtonController()
                    .unregisterAccessibilityButtonCallback(callback);
        }
    }
}
