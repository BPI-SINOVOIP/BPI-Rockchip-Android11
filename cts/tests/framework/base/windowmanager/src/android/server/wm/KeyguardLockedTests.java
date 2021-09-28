/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.server.wm;

import static android.app.WindowConfiguration.ACTIVITY_TYPE_STANDARD;
import static android.app.WindowConfiguration.WINDOWING_MODE_PINNED;
import static android.server.wm.MockImeHelper.createManagedMockImeSession;
import static android.server.wm.UiDeviceUtils.pressBackButton;
import static android.server.wm.app.Components.DISMISS_KEYGUARD_ACTIVITY;
import static android.server.wm.app.Components.DISMISS_KEYGUARD_METHOD_ACTIVITY;
import static android.server.wm.app.Components.PIP_ACTIVITY;
import static android.server.wm.app.Components.PipActivity.ACTION_ENTER_PIP;
import static android.server.wm.app.Components.PipActivity.EXTRA_DISMISS_KEYGUARD;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP;
import static android.server.wm.app.Components.PipActivity.EXTRA_SHOW_OVER_KEYGUARD;
import static android.server.wm.app.Components.SHOW_WHEN_LOCKED_ACTIVITY;
import static android.server.wm.app.Components.SHOW_WHEN_LOCKED_ATTR_IME_ACTIVITY;
import static android.server.wm.app.Components.TURN_SCREEN_ON_ATTR_DISMISS_KEYGUARD_ACTIVITY;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.WindowInsets.Type.ime;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE;

import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.ComponentName;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.test.annotations.Presubmit;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;

import com.android.compatibility.common.util.CtsTouchUtils;
import com.android.compatibility.common.util.PollingCheck;

import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.MockImeSession;

import org.junit.Before;
import org.junit.Test;

import java.util.concurrent.TimeUnit;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:KeyguardLockedTests
 */
@Presubmit
@android.server.wm.annotation.Group2
public class KeyguardLockedTests extends KeyguardTestBase {

    private final static long TIMEOUT_IME = TimeUnit.SECONDS.toMillis(5);

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        assumeTrue(supportsSecureLock());
    }

    @Test
    public void testLockAndUnlock() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential().gotoKeyguard();

        assertTrue(mKeyguardManager.isKeyguardLocked());
        assertTrue(mKeyguardManager.isDeviceLocked());
        assertTrue(mKeyguardManager.isDeviceSecure());
        assertTrue(mKeyguardManager.isKeyguardSecure());
        mWmState.assertKeyguardShowingAndNotOccluded();

        lockScreenSession.unlockDevice().enterAndConfirmLockCredential();

        mWmState.waitAndAssertKeyguardGone();
        assertFalse(mKeyguardManager.isDeviceLocked());
        assertFalse(mKeyguardManager.isKeyguardLocked());
    }

    @Test
    public void testDisableKeyguard_thenSettingCredential_reenablesKeyguard_b119322269() {
        final KeyguardManager.KeyguardLock keyguardLock = mContext.getSystemService(
                KeyguardManager.class).newKeyguardLock("KeyguardLockedTests");

        try (final LockScreenSession lockScreenSession = new LockScreenSession()) {
            lockScreenSession.gotoKeyguard();
            keyguardLock.disableKeyguard();

            lockScreenSession.setLockCredential();
            lockScreenSession.gotoKeyguard();

            mWmState.waitForKeyguardShowingAndNotOccluded();
            mWmState.assertKeyguardShowingAndNotOccluded();
            assertTrue(mKeyguardManager.isKeyguardLocked());
            assertTrue(mKeyguardManager.isDeviceLocked());
            assertTrue(mKeyguardManager.isDeviceSecure());
            assertTrue(mKeyguardManager.isKeyguardSecure());
        } finally {
            keyguardLock.reenableKeyguard();
        }
    }

    @Test
    public void testDismissKeyguard() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential().gotoKeyguard();

        mWmState.assertKeyguardShowingAndNotOccluded();
        launchActivity(DISMISS_KEYGUARD_ACTIVITY);
        lockScreenSession.enterAndConfirmLockCredential();

        mWmState.waitAndAssertKeyguardGone();
        mWmState.assertVisibility(DISMISS_KEYGUARD_ACTIVITY, true);
    }

    @Test
    public void testDismissKeyguard_whileOccluded() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential().gotoKeyguard();

        mWmState.assertKeyguardShowingAndNotOccluded();
        launchActivity(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.assertVisibility(SHOW_WHEN_LOCKED_ACTIVITY, true);

        launchActivity(DISMISS_KEYGUARD_ACTIVITY);
        lockScreenSession.enterAndConfirmLockCredential();
        mWmState.waitAndAssertKeyguardGone();
        mWmState.computeState(DISMISS_KEYGUARD_ACTIVITY);

        final boolean isDismissTranslucent = mWmState
                .isActivityTranslucent(DISMISS_KEYGUARD_ACTIVITY);
        mWmState.assertVisibility(DISMISS_KEYGUARD_ACTIVITY, true);
        mWmState.assertVisibility(SHOW_WHEN_LOCKED_ACTIVITY, isDismissTranslucent);
    }

    @Test
    public void testDismissKeyguard_fromShowWhenLocked_notAllowed() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential().gotoKeyguard();

        mWmState.assertKeyguardShowingAndNotOccluded();
        launchActivity(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.assertVisibility(SHOW_WHEN_LOCKED_ACTIVITY, true);
        mBroadcastActionTrigger.dismissKeyguardByFlag();
        lockScreenSession.enterAndConfirmLockCredential();

        // Make sure we stay on Keyguard.
        mWmState.assertKeyguardShowingAndOccluded();
        mWmState.assertVisibility(SHOW_WHEN_LOCKED_ACTIVITY, true);
    }

    @Test
    public void testDismissKeyguardActivity_method() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential();
        separateTestJournal();

        lockScreenSession.gotoKeyguard();
        mWmState.computeState();
        assertTrue(mWmState.getKeyguardControllerState().keyguardShowing);

        launchActivity(DISMISS_KEYGUARD_METHOD_ACTIVITY);
        lockScreenSession.enterAndConfirmLockCredential();
        mWmState.waitForKeyguardGone();
        mWmState.computeState(DISMISS_KEYGUARD_METHOD_ACTIVITY);
        mWmState.assertVisibility(DISMISS_KEYGUARD_METHOD_ACTIVITY, true);
        assertFalse(mWmState.getKeyguardControllerState().keyguardShowing);
        assertOnDismissSucceeded(DISMISS_KEYGUARD_METHOD_ACTIVITY);
    }

    @Test
    public void testDismissKeyguardActivity_method_cancelled() {
        // Pressing the back button does not cancel Keyguard in AAOS.
        assumeFalse(isCar());

        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential();
        separateTestJournal();

        lockScreenSession.gotoKeyguard();
        mWmState.computeState();
        assertTrue(mWmState.getKeyguardControllerState().keyguardShowing);

        launchActivity(DISMISS_KEYGUARD_METHOD_ACTIVITY);
        pressBackButton();
        assertOnDismissCancelled(DISMISS_KEYGUARD_METHOD_ACTIVITY);
        mWmState.computeState();
        mWmState.assertVisibility(DISMISS_KEYGUARD_METHOD_ACTIVITY, false);
        assertTrue(mWmState.getKeyguardControllerState().keyguardShowing);
    }

    @Test
    public void testDismissKeyguardAttrActivity_method_turnScreenOn_withSecureKeyguard() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential().sleepDevice();
        mWmState.computeState();
        assertTrue(mWmState.getKeyguardControllerState().keyguardShowing);

        launchActivity(TURN_SCREEN_ON_ATTR_DISMISS_KEYGUARD_ACTIVITY);
        mWmState.waitForKeyguardShowingAndNotOccluded();
        mWmState.assertVisibility(TURN_SCREEN_ON_ATTR_DISMISS_KEYGUARD_ACTIVITY, false);
        assertTrue(mWmState.getKeyguardControllerState().keyguardShowing);
        assertTrue(isDisplayOn(DEFAULT_DISPLAY));
    }

    @Test
    public void testEnterPipOverKeyguard() {
        assumeTrue(supportsPip());

        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential();

        // Show the PiP activity in fullscreen.
        launchActivity(PIP_ACTIVITY, EXTRA_SHOW_OVER_KEYGUARD, "true");

        // Lock the screen and ensure that the PiP activity showing over the LockScreen.
        lockScreenSession.gotoKeyguard(PIP_ACTIVITY);
        mWmState.waitForKeyguardShowingAndOccluded();
        mWmState.assertKeyguardShowingAndOccluded();

        // Request that the PiP activity enter picture-in-picture mode (ensure it does not).
        mBroadcastActionTrigger.doAction(ACTION_ENTER_PIP);
        waitForEnterPip(PIP_ACTIVITY);
        mWmState.assertDoesNotContainStack("Must not contain pinned stack.",
                WINDOWING_MODE_PINNED, ACTIVITY_TYPE_STANDARD);

        // Enter the credentials and ensure that the activity actually entered picture-in-picture.
        lockScreenSession.enterAndConfirmLockCredential();
        mWmState.waitAndAssertKeyguardGone();
        waitForEnterPip(PIP_ACTIVITY);
        mWmState.assertContainsStack("Must contain pinned stack.", WINDOWING_MODE_PINNED,
                ACTIVITY_TYPE_STANDARD);
    }

    @Test
    public void testShowWhenLockedActivityAndPipActivity() {
        assumeTrue(supportsPip());

        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential();

        // Show an activity in PIP.
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        mWmState.assertContainsStack("Must contain pinned stack.", WINDOWING_MODE_PINNED,
                ACTIVITY_TYPE_STANDARD);
        mWmState.assertVisibility(PIP_ACTIVITY, true);

        // Show an activity that will keep above the keyguard.
        launchActivity(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.assertVisibility(SHOW_WHEN_LOCKED_ACTIVITY, true);

        // Lock the screen and ensure that the fullscreen activity showing over the lockscreen
        // is visible, but not the PiP activity.
        lockScreenSession.gotoKeyguard(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.computeState();
        mWmState.assertKeyguardShowingAndOccluded();
        mWmState.assertVisibility(SHOW_WHEN_LOCKED_ACTIVITY, true);
        mWmState.assertVisibility(PIP_ACTIVITY, false);
    }

    @Test
    public void testShowWhenLockedPipActivity() {
        assumeTrue(supportsPip());

        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential();

        // Show an activity in PIP.
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true", EXTRA_SHOW_OVER_KEYGUARD, "true");
        waitForEnterPip(PIP_ACTIVITY);
        mWmState.assertContainsStack("Must contain pinned stack.", WINDOWING_MODE_PINNED,
                ACTIVITY_TYPE_STANDARD);
        mWmState.assertVisibility(PIP_ACTIVITY, true);

        // Lock the screen and ensure the PiP activity is not visible on the lockscreen even
        // though it's marked as showing over the lockscreen itself.
        lockScreenSession.gotoKeyguard();
        mWmState.assertKeyguardShowingAndNotOccluded();
        mWmState.assertVisibility(PIP_ACTIVITY, false);
    }

    @Test
    public void testDismissKeyguardPipActivity() {
        assumeTrue(supportsPip());

        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        // Show an activity in PIP.
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true", EXTRA_DISMISS_KEYGUARD, "true");
        waitForEnterPip(PIP_ACTIVITY);
        mWmState.assertContainsStack("Must contain pinned stack.", WINDOWING_MODE_PINNED,
                ACTIVITY_TYPE_STANDARD);
        mWmState.assertVisibility(PIP_ACTIVITY, true);

        // Lock the screen and ensure the PiP activity is not visible on the lockscreen even
        // though it's marked as dismiss keyguard.
        lockScreenSession.gotoKeyguard();
        mWmState.computeState();
        mWmState.assertKeyguardShowingAndNotOccluded();
        mWmState.assertVisibility(PIP_ACTIVITY, false);
    }

    @Test
    public void testShowWhenLockedAttrImeActivityAndShowSoftInput() throws Exception {
        assumeTrue(MSG_NO_MOCK_IME, supportsInstallableIme());

        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        final MockImeSession mockImeSession = createManagedMockImeSession(this);

        lockScreenSession.setLockCredential().gotoKeyguard();
        mWmState.assertKeyguardShowingAndNotOccluded();
        launchActivity(SHOW_WHEN_LOCKED_ATTR_IME_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_ATTR_IME_ACTIVITY);
        mWmState.assertVisibility(SHOW_WHEN_LOCKED_ATTR_IME_ACTIVITY, true);

        // Make sure the activity has been called showSoftInput & IME window is visible.
        final ImeEventStream stream = mockImeSession.openEventStream();
        expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()),
                TIMEOUT_IME);
        // Assert the IME is shown on the expected display.
        mWmState.waitAndAssertImeWindowShownOnDisplay(DEFAULT_DISPLAY);
    }

    @Test
    public void testShowWhenLockedImeActivityAndShowSoftInput() throws Exception {
        assumeTrue(MSG_NO_MOCK_IME, supportsInstallableIme());

        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        final MockImeSession mockImeSession = createManagedMockImeSession(this);
        final TestActivitySession<ShowWhenLockedImeActivity> imeTestActivitySession =
                createManagedTestActivitySession();

        lockScreenSession.setLockCredential().gotoKeyguard();
        mWmState.assertKeyguardShowingAndNotOccluded();
        imeTestActivitySession.launchTestActivityOnDisplaySync(ShowWhenLockedImeActivity.class,
                DEFAULT_DISPLAY);

        // Make sure the activity has been called showSoftInput & IME window is visible.
        final ImeEventStream stream = mockImeSession.openEventStream();
        expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()),
                TIMEOUT_IME);
        // Assert the IME is shown on the expected display.
        mWmState.waitAndAssertImeWindowShownOnDisplay(DEFAULT_DISPLAY);

    }

    @Test
    public void testImeShowsAfterLockScreenOnEditorTap() throws Exception {
        assumeTrue(MSG_NO_MOCK_IME, supportsInstallableIme());

        final MockImeSession mockImeSession = createManagedMockImeSession(this);
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        final TestActivitySession<ShowImeAfterLockscreenActivity> imeTestActivitySession =
                createManagedTestActivitySession();
        imeTestActivitySession.launchTestActivityOnDisplaySync(ShowImeAfterLockscreenActivity.class,
                DEFAULT_DISPLAY);

        final ShowImeAfterLockscreenActivity activity = imeTestActivitySession.getActivity();
        final View rootView = activity.getWindow().getDecorView();

        CtsTouchUtils.emulateTapOnViewCenter(getInstrumentation(), null, activity.mEditor);
        PollingCheck.waitFor(
                TIMEOUT_IME,
                () -> rootView.getRootWindowInsets().isVisible(ime()));

        lockScreenSession.setLockCredential().gotoKeyguard();
        assertTrue("Keyguard is showing", mWmState.getKeyguardControllerState().keyguardShowing);
        lockScreenSession.enterAndConfirmLockCredential();
        mWmState.waitAndAssertKeyguardGone();

        final ImeEventStream stream = mockImeSession.openEventStream();

        CtsTouchUtils.emulateTapOnViewCenter(getInstrumentation(), null, activity.mEditor);

        // Make sure the activity has been called showSoftInput & IME window is visible.
        expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()),
                TimeUnit.SECONDS.toMillis(5) /* eventTimeout */);
        // Assert the IME is shown event on the expected display.
        mWmState.waitAndAssertImeWindowShownOnDisplay(DEFAULT_DISPLAY);
        // Check if IME is actually visible.
        PollingCheck.waitFor(
                TIMEOUT_IME,
                () -> rootView.getRootWindowInsets().isVisible(ime()));
    }

    public static class ShowImeAfterLockscreenActivity extends Activity {

        EditText mEditor;

        @Override
        protected void onCreate(Bundle icicle) {
            super.onCreate(icicle);
            mEditor = createViews(this, false /* showWhenLocked */);
        }
    }

    public static class ShowWhenLockedImeActivity extends Activity {

        @Override
        protected void onCreate(Bundle icicle) {
            super.onCreate(icicle);
            createViews(this, true /* showWhenLocked */);
        }
    }

    private static EditText createViews(
            Activity activity, boolean showWhenLocked /* showWhenLocked */) {
        EditText editor = new EditText(activity);
        // Set private IME option for editorMatcher to identify which TextView received
        // onStartInput event.
        editor.setPrivateImeOptions(
                activity.getClass().getName()
                        + "/" + Long.toString(SystemClock.elapsedRealtimeNanos()));
        final LinearLayout layout = new LinearLayout(activity);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.addView(editor);
        activity.setContentView(layout);

        if (showWhenLocked) {
            // Set showWhenLocked as true & request focus for showing soft input.
            activity.setShowWhenLocked(true);
            activity.getWindow().setSoftInputMode(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
        }
        editor.requestFocus();
        return editor;
    }

    /**
     * Waits until the given activity has entered picture-in-picture mode (allowing for the
     * subsequent animation to start).
     */
    private void waitForEnterPip(ComponentName activityName) {
        mWmState.waitForValidState(new WaitForValidActivityState.Builder(activityName)
                .setWindowingMode(WINDOWING_MODE_PINNED)
                .setActivityType(ACTIVITY_TYPE_STANDARD)
                .build());
    }
}
