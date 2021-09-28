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

package android.server.wm;

import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.server.wm.CommandSession.ActivityCallback.ON_CONFIGURATION_CHANGED;
import static android.server.wm.CommandSession.ActivityCallback.ON_RESUME;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.Display.INVALID_DISPLAY;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.android.cts.mockime.ImeEventStreamTestUtils.expectCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.platform.test.annotations.Presubmit;
import android.server.wm.WindowManagerState.DisplayContent;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;

import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.MockImeSession;

import org.junit.Before;
import org.junit.Test;

import java.util.concurrent.TimeUnit;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:MultiDisplayClientTests
 */
@Presubmit
@MediumTest
@android.server.wm.annotation.Group3
public class MultiDisplayClientTests extends MultiDisplayTestBase {

    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(10); // 10 seconds
    private static final String EXTRA_SHOW_IME = "show_ime";

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        assumeTrue(supportsMultiDisplay());
    }

    @Test
    public void testDisplayIdUpdateOnMove_RelaunchActivity() throws Exception {
        testDisplayIdUpdateOnMove(ClientTestActivity.class, false /* handlesConfigChange */);
    }

    @Test
    public void testDisplayIdUpdateOnMove_NoRelaunchActivity() throws Exception {
        testDisplayIdUpdateOnMove(NoRelaunchActivity.class, true /* handlesConfigChange */);
    }

    private <T extends Activity> void testDisplayIdUpdateOnMove(Class<T> activityClass,
            boolean handlesConfigChange) throws Exception {
        final ActivityTestRule<T> activityTestRule = new ActivityTestRule<>(
                activityClass, true /* initialTouchMode */, false /* launchActivity */);

        // Launch activity display.
        separateTestJournal();
        Activity activity = activityTestRule.launchActivity(new Intent());
        final ComponentName activityName = activity.getComponentName();
        waitAndAssertResume(activityName);

        // Create new simulated display
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        // Move the activity to the new secondary display.
        separateTestJournal();
        final ActivityOptions launchOptions = ActivityOptions.makeBasic();
        final int displayId = newDisplay.mId;
        launchOptions.setLaunchDisplayId(displayId);
        final Intent newDisplayIntent = new Intent(mContext, activityClass);
        newDisplayIntent.setFlags(FLAG_ACTIVITY_NEW_TASK);
        getInstrumentation().getTargetContext().startActivity(newDisplayIntent,
                launchOptions.toBundle());
        waitAndAssertTopResumedActivity(activityName, displayId,
                "Activity moved to secondary display must be focused");

        if (handlesConfigChange) {
            // Wait for activity to receive the configuration change after move
            waitAndAssertConfigurationChange(activityName);
        } else {
            // Activity will be re-created, wait for resumed state
            waitAndAssertResume(activityName);
            activity = activityTestRule.getActivity();
        }

        final String suffix = " must be updated.";
        assertEquals("Activity#getDisplayId()" + suffix, displayId, activity.getDisplayId());
        assertEquals("Activity#getDisplay" + suffix,
                displayId, activity.getDisplay().getDisplayId());

        final WindowManager wm = activity.getWindowManager();
        assertEquals("WM#getDefaultDisplay()" + suffix,
                displayId, wm.getDefaultDisplay().getDisplayId());

        final View view = activity.getWindow().getDecorView();
        assertEquals("View#getDisplay()" + suffix,
                displayId, view.getDisplay().getDisplayId());
    }

    @Test
    public void testDisplayIdUpdateWhenImeMove_RelaunchActivity() throws Exception {
        testDisplayIdUpdateWhenImeMove(ClientTestActivity.class);
    }

    @Test
    public void testDisplayIdUpdateWhenImeMove_NoRelaunchActivity() throws Exception {
        testDisplayIdUpdateWhenImeMove(NoRelaunchActivity.class);
    }

    private void testDisplayIdUpdateWhenImeMove(Class<? extends ImeTestActivity> activityClass)
            throws Exception {
        assumeTrue(MSG_NO_MOCK_IME, supportsInstallableIme());

        final VirtualDisplaySession virtualDisplaySession = createManagedVirtualDisplaySession();
        final MockImeSession mockImeSession = MockImeHelper.createManagedMockImeSession(this);

        assertImeShownAndMatchesDisplayId(
                activityClass, mockImeSession, DEFAULT_DISPLAY);

        final DisplayContent newDisplay = virtualDisplaySession
                .setSimulateDisplay(true)
                .setShowSystemDecorations(true)
                .setRequestShowIme(true)
                .createDisplay();

        // Launch activity on the secondary display and make IME show.
        assertImeShownAndMatchesDisplayId(
                activityClass, mockImeSession, newDisplay.mId);
    }

    private  void assertImeShownAndMatchesDisplayId(Class<? extends ImeTestActivity> activityClass,
            MockImeSession imeSession, int targetDisplayId) throws Exception {
        final ImeEventStream stream = imeSession.openEventStream();

        final Intent intent = new Intent(mContext, activityClass)
                .putExtra(EXTRA_SHOW_IME, true).setFlags(FLAG_ACTIVITY_NEW_TASK);
        separateTestJournal();
        final ActivityOptions launchOptions = ActivityOptions.makeBasic();
        launchOptions.setLaunchDisplayId(targetDisplayId);
        getInstrumentation().getTargetContext().startActivity(intent, launchOptions.toBundle());


        // Verify if IME is showed on the target display.
        expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
        mWmState.waitAndAssertImeWindowShownOnDisplay(targetDisplayId);

        final int imeDisplayId = expectCommand(stream, imeSession.callGetDisplayId(), TIMEOUT)
                .getReturnIntegerValue();
        assertEquals("IME#getDisplayId() must match when IME move.",
                targetDisplayId, imeDisplayId);
    }

    @Test
    public void testInputMethodManagerDisplayId() {
        // Create a simulated display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        final Display display = mDm.getDisplay(newDisplay.mId);
        final Context newDisplayContext = mContext.createDisplayContext(display);
        final InputMethodManager imm = newDisplayContext.getSystemService(InputMethodManager.class);

        assertEquals("IMM#getDisplayId() must match.", newDisplay.mId, imm.getDisplayId());
    }

    @Test
    public void testViewGetDisplayOnPrimaryDisplay() {
        testViewGetDisplay(true /* isPrimary */);
    }

    @Test
    public void testViewGetDisplayOnSecondaryDisplay() {
        testViewGetDisplay(false /* isPrimary */);
    }

    private void testViewGetDisplay(boolean isPrimary) {
        final TestActivitySession<ClientTestActivity> activitySession =
                createManagedTestActivitySession();
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();
        final int displayId = isPrimary ? DEFAULT_DISPLAY : newDisplay.mId;

        separateTestJournal();
        activitySession.launchTestActivityOnDisplaySync(ClientTestActivity.class, displayId);

        final Activity activity = activitySession.getActivity();
        final ComponentName activityName = activity.getComponentName();

        waitAndAssertTopResumedActivity(activityName, displayId,
                "Activity launched on display:" + displayId + " must be focused");

        // Test View#getdisplay() from activity
        final View view = activity.getWindow().getDecorView();
        assertEquals("View#getDisplay() must match.", displayId, view.getDisplay().getDisplayId());

        final int[] resultDisplayId = { INVALID_DISPLAY };
        activitySession.runOnMainAndAssertWithTimeout(
                () -> {
                    // Test View#getdisplay() from WM#addView()
                    final WindowManager wm = activity.getWindowManager();
                    final View addedView = new View(activity);
                    wm.addView(addedView, new WindowManager.LayoutParams());

                    // Get display ID from callback in case the added view has not be attached.
                    addedView.addOnAttachStateChangeListener(
                            new View.OnAttachStateChangeListener() {
                                @Override
                                public void onViewAttachedToWindow(View view) {
                                    resultDisplayId[0] = view.getDisplay().getDisplayId();
                                }

                                @Override
                                public void onViewDetachedFromWindow(View view) {}
                            });

                    return displayId == resultDisplayId[0];
                }, TIMEOUT, "Display from added view must match. "
                        + "Should be display:" + displayId
                        + ", but was display:" + resultDisplayId[0]
        );
    }

    private void waitAndAssertConfigurationChange(ComponentName activityName) {
        assertTrue("Must receive a single configuration change",
                mWmState.waitForWithAmState(
                        state -> getCallbackCount(activityName, ON_CONFIGURATION_CHANGED) == 1,
                        activityName + " receives configuration change"));
    }

    private void waitAndAssertResume(ComponentName activityName) {
        assertTrue("Must be resumed once",
                mWmState.waitForWithAmState(
                        state -> getCallbackCount(activityName, ON_RESUME) == 1,
                        activityName + " performs resume"));
    }

    private static int getCallbackCount(ComponentName activityName,
            CommandSession.ActivityCallback callback) {
        final ActivityLifecycleCounts lifecycles = new ActivityLifecycleCounts(activityName);
        return lifecycles.getCount(callback);
    }

    public static class ClientTestActivity extends ImeTestActivity { }

    public static class NoRelaunchActivity extends ImeTestActivity { }

    public static class ImeTestActivity extends CommandSession.BasicTestActivity {
        private EditText mEditText;
        private boolean mShouldShowIme;

        @Override
        protected void onCreate(Bundle icicle) {
            super.onCreate(icicle);
            mShouldShowIme = getIntent().hasExtra(EXTRA_SHOW_IME);
            if (mShouldShowIme) {
                mEditText = new EditText(this);
                final LinearLayout layout = new LinearLayout(this);
                layout.setOrientation(LinearLayout.VERTICAL);
                layout.addView(mEditText);
                setContentView(layout);
            }
        }

        @Override
        protected void onResume() {
            super.onResume();
            if (mShouldShowIme) {
                getWindow().setSoftInputMode(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                mEditText.requestFocus();
            }
        }
    }
}
