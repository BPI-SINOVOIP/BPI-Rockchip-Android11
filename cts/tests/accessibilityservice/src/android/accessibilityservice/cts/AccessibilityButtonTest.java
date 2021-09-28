/**
 * Copyright (C) 2017 The Android Open Source Project
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

import static android.accessibilityservice.AccessibilityServiceInfo.FLAG_REQUEST_ACCESSIBILITY_BUTTON;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityButtonController;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.os.Build;
import android.platform.test.annotations.AppModeFull;
import android.view.Display;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

/**
 * Test cases for accessibility service APIs related to the accessibility button within
 * software-rendered navigation bars.
 *
 * TODO: Extend coverage with a more precise signal if a device is compatible with the button
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityButtonTest {

    private InstrumentedAccessibilityServiceTestRule<StubAccessibilityButtonService> mServiceRule =
            new InstrumentedAccessibilityServiceTestRule<>(StubAccessibilityButtonService.class);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mServiceRule)
            .around(mDumpOnFailureRule);

    private StubAccessibilityButtonService mService;
    private AccessibilityButtonController mButtonController;
    private AccessibilityButtonController.AccessibilityButtonCallback mStubCallback =
            new AccessibilityButtonController.AccessibilityButtonCallback() {
        @Override
        public void onClicked(AccessibilityButtonController controller) {
            /* do nothing */
        }

        @Override
        public void onAvailabilityChanged(AccessibilityButtonController controller,
                boolean available) {
            /* do nothing */
        }
    };

    @Before
    public void setUp() {
        mService = mServiceRule.getService();
        mButtonController = mService.getAccessibilityButtonController();
    }

    @Test
    @AppModeFull
    public void testCallbackRegistrationUnregistration_serviceDoesNotCrash() {
        mButtonController.registerAccessibilityButtonCallback(mStubCallback);
        mButtonController.unregisterAccessibilityButtonCallback(mStubCallback);
    }

    @Test
    @AppModeFull
    public void testGetAccessibilityButtonControllerByDisplayId_NotReturnNull() {
        final AccessibilityButtonController buttonController =
                mService.getAccessibilityButtonController(
                        Display.DEFAULT_DISPLAY);
        assertNotNull(buttonController);
    }

    @Test
    @AppModeFull
    public void testUpdateRequestAccessibilityButtonFlag_targetSdkGreaterThanQ_ignoresUpdate() {
        final AccessibilityServiceInfo serviceInfo = mService.getServiceInfo();
        assertTrue((serviceInfo.flags & FLAG_REQUEST_ACCESSIBILITY_BUTTON)
                == FLAG_REQUEST_ACCESSIBILITY_BUTTON);
        assertTrue(mService.getApplicationInfo().targetSdkVersion > Build.VERSION_CODES.Q);

        serviceInfo.flags &= ~FLAG_REQUEST_ACCESSIBILITY_BUTTON;
        mService.setServiceInfo(serviceInfo);
        assertTrue("Update flagRequestAccessibilityButton should fail",
                (mService.getServiceInfo().flags & FLAG_REQUEST_ACCESSIBILITY_BUTTON)
                        == FLAG_REQUEST_ACCESSIBILITY_BUTTON);
    }
}
