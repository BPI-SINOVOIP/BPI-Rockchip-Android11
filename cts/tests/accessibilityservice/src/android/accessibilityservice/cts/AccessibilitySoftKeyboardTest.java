/**
 * Copyright (C) 2016 The Android Open Source Project
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

import static android.accessibility.cts.common.InstrumentedAccessibilityService.enableService;
import static android.accessibilityservice.AccessibilityService.SHOW_MODE_AUTO;
import static android.accessibilityservice.AccessibilityService.SHOW_MODE_HIDDEN;
import static android.accessibilityservice.AccessibilityService.SHOW_MODE_IGNORE_HARD_KEYBOARD;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityService;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibility.cts.common.ShellCommandBuilder;
import android.accessibilityservice.AccessibilityService.SoftKeyboardController;
import android.accessibilityservice.AccessibilityService.SoftKeyboardController.OnShowModeChangedListener;
import android.accessibilityservice.cts.activities.AccessibilityTestActivity;
import android.accessibilityservice.cts.utils.AsyncUtils;
import android.app.Instrumentation;
import android.inputmethodservice.cts.common.Ime1Constants;
import android.inputmethodservice.cts.common.test.ShellCommandUtils;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.provider.Settings;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

/**
 * Test cases for {@code SoftKeyboardController}. It tests the accessibility APIs for interacting
 * with the soft keyboard show mode.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull
public class AccessibilitySoftKeyboardTest {
    private Instrumentation mInstrumentation;
    private int mLastCallbackValue;

    private InstrumentedAccessibilityService mService;
    private final Object mLock = new Object();
    private final OnShowModeChangedListener mListener = (c, showMode) -> {
        synchronized (mLock) {
            mLastCallbackValue = showMode;
            mLock.notifyAll();
        }
    };

    private InstrumentedAccessibilityServiceTestRule<InstrumentedAccessibilityService>
            mServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
                    InstrumentedAccessibilityService.class);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mServiceRule)
            .around(mDumpOnFailureRule);

    @Before
    public void setUp() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mService = mServiceRule.getService();
    }

    @Test
    public void testApiReturnValues_shouldChangeValueOnRequestAndSendCallback() throws Exception {
        final SoftKeyboardController controller = mService.getSoftKeyboardController();

        // Confirm that we start in the default state
        assertEquals(SHOW_MODE_AUTO, controller.getShowMode());

        controller.addOnShowModeChangedListener(mListener);
        assertCanSetAndGetShowModeAndCallbackHappens(SHOW_MODE_HIDDEN, mService);
        assertCanSetAndGetShowModeAndCallbackHappens(SHOW_MODE_IGNORE_HARD_KEYBOARD, mService);
        assertCanSetAndGetShowModeAndCallbackHappens(SHOW_MODE_AUTO, mService);

        // Make sure we can remove our listener.
        assertTrue(controller.removeOnShowModeChangedListener(mListener));
    }

    @Test
    public void secondServiceChangingTheShowMode_updatesModeAndNotifiesFirstService()
            throws Exception {

        final SoftKeyboardController controller = mService.getSoftKeyboardController();
        // Confirm that we start in the default state
        assertEquals(SHOW_MODE_AUTO, controller.getShowMode());

        final InstrumentedAccessibilityService secondService =
                enableService(StubAccessibilityButtonService.class);
        try {
            // Listen on the first service
            controller.addOnShowModeChangedListener(mListener);
            assertCanSetAndGetShowModeAndCallbackHappens(SHOW_MODE_HIDDEN, mService);

            // Change the mode on the second service
            assertCanSetAndGetShowModeAndCallbackHappens(SHOW_MODE_IGNORE_HARD_KEYBOARD,
                    secondService);
        } finally {
            secondService.runOnServiceSync(() -> secondService.disableSelf());
        }

        // Shutting down the second service, which was controlling the mode, should put us back
        // to the default
        waitForCallbackValueWithLock(SHOW_MODE_AUTO);
        final int showMode = mService.getOnService(() -> controller.getShowMode());
        assertEquals(SHOW_MODE_AUTO, showMode);
    }

    @Test
    public void testSwitchToInputMethod() throws Exception {
        final SoftKeyboardController controller = mService.getSoftKeyboardController();
        String currentIME = Settings.Secure.getString(
                mService.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
        assertNotEquals(Ime1Constants.IME_ID, currentIME);
        // Enable a dummy IME for this test.
        try (TestImeSession imeSession = new TestImeSession(Ime1Constants.IME_ID)) {
            // Switch to the dummy IME.
            final boolean success = controller.switchToInputMethod(Ime1Constants.IME_ID);
            currentIME = Settings.Secure.getString(
                    mService.getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);

            // The current IME should be set to the dummy IME successfully.
            assertTrue(success);
            assertEquals(Ime1Constants.IME_ID, currentIME);
        }
    }

    private void assertCanSetAndGetShowModeAndCallbackHappens(
            int mode, InstrumentedAccessibilityService service)
            throws Exception  {
        final SoftKeyboardController controller = service.getSoftKeyboardController();
        mLastCallbackValue = -1;
        final boolean setShowModeReturns =
                service.getOnService(() -> controller.setShowMode(mode));
        assertTrue(setShowModeReturns);
        waitForCallbackValueWithLock(mode);
        assertEquals(mode, controller.getShowMode());
    }

    private void waitForCallbackValueWithLock(int expectedValue) throws Exception {
        long timeoutTimeMillis = SystemClock.uptimeMillis() + AsyncUtils.DEFAULT_TIMEOUT_MS;

        while (SystemClock.uptimeMillis() < timeoutTimeMillis) {
            synchronized(mLock) {
                if (mLastCallbackValue == expectedValue) {
                    return;
                }
                try {
                    mLock.wait(timeoutTimeMillis - SystemClock.uptimeMillis());
                } catch (InterruptedException e) {
                    // Wait until timeout.
                }
            }
        }

        throw new IllegalStateException("last callback value <" + mLastCallbackValue
                + "> does not match expected value < " + expectedValue + ">");
    }

    /**
     * Activity for testing the AccessibilityService API for hiding and showing the soft keyboard.
     */
    public static class SoftKeyboardModesActivity extends AccessibilityTestActivity {
        public SoftKeyboardModesActivity() {
            super();
        }

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.accessibility_soft_keyboard_modes_test);
        }
    }

    private class TestImeSession implements AutoCloseable {
        TestImeSession(String imeId) {
            // Enable the dummy IME by shell command.
            final String enableImeCommand = ShellCommandUtils.enableIme(imeId);
            ShellCommandBuilder.create(mInstrumentation)
                    .addCommand(enableImeCommand)
                    .run();
        }

        @Override
        public void close() throws Exception {
            // Reset IMEs by shell command.
            ShellCommandBuilder.create(mInstrumentation)
                    .addCommand(ShellCommandUtils.resetImes())
                    .run();
        }
    }
}
