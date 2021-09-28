/**
 * Copyright (C) 2019 The Android Open Source Project
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

import static android.accessibility.cts.common.AccessibilityShortcutSettingsRule.ACCESSIBILITY_BUTTON;
import static android.accessibilityservice.AccessibilityServiceInfo.FLAG_REQUEST_ACCESSIBILITY_BUTTON;
import static android.app.UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static org.junit.Assert.assertTrue;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Instrumentation;
import android.app.Service;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.os.Build;
import android.platform.test.annotations.AppModeFull;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.List;

/**
 * Test to verify accessibility button targeting sdk 29 APIs.
 */
@AppModeFull
@RunWith(AndroidJUnit4.class)
public class AccessibilityButtonSdk29Test {
    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    private InstrumentedAccessibilityServiceTestRule<StubAccessibilityButtonSdk29Service>
            mServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
                    StubAccessibilityButtonSdk29Service.class);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mServiceRule)
            .around(mDumpOnFailureRule);

    private StubAccessibilityButtonSdk29Service mService;
    private AccessibilityServiceInfo mServiceInfo;
    private ComponentName mServiceComponentName;
    private AccessibilityManager mAccessibilityManager;

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
        mAccessibilityManager = (AccessibilityManager) sInstrumentation.getContext()
                .getSystemService(Service.ACCESSIBILITY_SERVICE);
        mService = mServiceRule.getService();
        mServiceInfo = mService.getServiceInfo();
        mServiceComponentName = new ComponentName(
                mServiceInfo.getResolveInfo().serviceInfo.packageName,
                mServiceInfo.getResolveInfo().serviceInfo.name);

        mServiceInfo.flags |= FLAG_REQUEST_ACCESSIBILITY_BUTTON;
        mService.setServiceInfo(mServiceInfo);
        mServiceInfo = mService.getServiceInfo();
        assertTrue(mService.getApplicationInfo().targetSdkVersion == Build.VERSION_CODES.Q);
        assertTrue((mServiceInfo.flags & FLAG_REQUEST_ACCESSIBILITY_BUTTON)
                == FLAG_REQUEST_ACCESSIBILITY_BUTTON);
    }

    @Test
    public void testUpdateRequestAccessibilityButtonFlag_succeeds() {
        mServiceInfo.flags &= ~FLAG_REQUEST_ACCESSIBILITY_BUTTON;
        mService.setServiceInfo(mServiceInfo);
        assertTrue("Update flagRequestAccessibilityButton should succeed",
                mService.getServiceInfo().flags == mServiceInfo.flags);
    }

    @Test
    public void getA11yBtn_withLegacyReqA11yBtnService_succeeds() {
        runWithShellPermissionIdentity(sUiAutomation, () -> {
            final List<String> targets = mAccessibilityManager.getAccessibilityShortcutTargets(
                    ACCESSIBILITY_BUTTON);
            assertTrue("Single Tap should contain legacy accessibility service.",
                    shortcutTargetListContains(targets, mServiceComponentName));
        });
    }

    /**
     * Return true if the {@code shortcutTargets} contains the given component name.
     *
     * @param shortcutTargets A list of shortcut targets.
     * @param name The component name.
     * @return {@code true} if the {@code shortcutTargets} contains the given component name.
     */
    private boolean shortcutTargetListContains(List<String> shortcutTargets, ComponentName name) {
        for (int i = 0; i < shortcutTargets.size(); i++) {
            final String stringName = shortcutTargets.get(i);
            if (TextUtils.isEmpty(stringName)) {
                continue;
            }
            if (name.equals(ComponentName.unflattenFromString(stringName))) return true;
        }
        return false;
    }
}
