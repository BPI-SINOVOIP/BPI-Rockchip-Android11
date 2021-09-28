/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.view.inputmethod.cts;

import static android.view.View.SCREEN_STATE_OFF;
import static android.view.View.SCREEN_STATE_ON;
import static android.view.View.VISIBLE;

import static com.android.cts.mockime.ImeEventStreamTestUtils.editorMatcher;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectBindInput;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;
import static com.android.cts.mockime.ImeEventStreamTestUtils.notExpectEvent;

import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.app.Instrumentation;
import android.content.Context;
import android.content.pm.PackageManager;
import android.inputmethodservice.InputMethodService;
import android.os.IBinder;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.cts.util.DisableScreenDozeRule;
import android.view.inputmethod.cts.util.EndToEndImeTestBase;
import android.view.inputmethod.cts.util.TestActivity;
import android.view.inputmethod.cts.util.TestUtils;
import android.view.inputmethod.cts.util.UnlockScreenRule;
import android.view.inputmethod.cts.util.WindowFocusStealer;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CtsTouchUtils;
import com.android.cts.mockime.ImeCommand;
import com.android.cts.mockime.ImeEvent;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Predicate;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class InputMethodStartInputLifecycleTest extends EndToEndImeTestBase {
    @Rule
    public final DisableScreenDozeRule mDisableScreenDozeRule = new DisableScreenDozeRule();
    @Rule
    public final UnlockScreenRule mUnlockScreenRule = new UnlockScreenRule();

    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);

    @Test
    public void testInputConnectionStateWhenScreenStateChanges() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final Context context = instrumentation.getTargetContext();
        final InputMethodManager imManager = context.getSystemService(InputMethodManager.class);
        assumeTrue(context.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_INPUT_METHODS));
        final AtomicReference<EditText> focusedEditTextRef = new AtomicReference<>();

        try (MockImeSession imeSession = MockImeSession.create(
                context, instrumentation.getUiAutomation(), new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = InputMethodManagerTest.class.getName() + "/"
                    + SystemClock.elapsedRealtimeNanos();
            final AtomicInteger screenStateCallbackRef = new AtomicInteger(-1);
            TestActivity.startSync(activity -> {
                final LinearLayout layout = new LinearLayout(activity);
                layout.setOrientation(LinearLayout.VERTICAL);

                final EditText focusedEditText = new EditText(activity) {
                    @Override
                    public void onScreenStateChanged(int screenState) {
                        super.onScreenStateChanged(screenState);
                        screenStateCallbackRef.set(screenState);
                    }
                };
                focusedEditText.setPrivateImeOptions(marker);
                focusedEditText.setHint("editText");
                layout.addView(focusedEditText);
                focusedEditText.requestFocus();
                focusedEditTextRef.set(focusedEditText);

                final EditText nonFocusedEditText = new EditText(activity);
                layout.addView(nonFocusedEditText);

                return layout;
            });

            // Expected onStartInput when TestActivity launched.
            final EditText editText = focusedEditTextRef.get();
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

            // Expected text commit will not work when turnScreenOff.
            TestUtils.turnScreenOff();
            TestUtils.waitOnMainUntil(() -> screenStateCallbackRef.get() == SCREEN_STATE_OFF
                            && editText.getWindowVisibility() != VISIBLE, TIMEOUT);
            expectEvent(stream, onFinishInputMatcher(), TIMEOUT);
            final ImeCommand commit = imeSession.callCommitText("Hi!", 1);
            expectCommand(stream, commit, TIMEOUT);
            TestUtils.waitOnMainUntil(() -> !TextUtils.equals(editText.getText(), "Hi!"), TIMEOUT,
                    "InputMethodService#commitText should not work after screen off");

            // Expected text commit will work when turnScreenOn.
            TestUtils.turnScreenOn();
            TestUtils.unlockScreen();
            TestUtils.waitOnMainUntil(() -> screenStateCallbackRef.get() == SCREEN_STATE_ON
                            && editText.getWindowVisibility() == VISIBLE, TIMEOUT);
            CtsTouchUtils.emulateTapOnViewCenter(instrumentation, null, editText);
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            assertTrue(TestUtils.getOnMainSync(
                    () -> imManager.isActive(editText) && imManager.isAcceptingText()));
            final ImeCommand commit1 = imeSession.callCommitText("Hello!", 1);
            expectCommand(stream, commit1, TIMEOUT);
            TestUtils.waitOnMainUntil(() -> TextUtils.equals(editText.getText(), "Hello!"), TIMEOUT,
                    "InputMethodService#commitText should work after screen on");
        }
    }

    /**
     * Test case for Bug 158624922 and Bug 152373385.
     *
     * Test {@link android.inputmethodservice.InputMethodService#onStartInput(EditorInfo, boolean)}
     * and {@link InputMethodService#onFinishInput()} won't be called and the input connection
     * remains active, even when a non-IME focusable window hosted by a different process
     * temporarily becomes the focused window.
     */
    @Test
    public void testNoStartNewInputWhileOtherProcessHasWindowFocus() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        try (MockImeSession imeSession = MockImeSession.create(
                instrumentation.getContext(),
                instrumentation.getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = InputMethodStartInputLifecycleTest.class.getName() + "/"
                    + SystemClock.elapsedRealtimeNanos();
            final EditText editText = launchTestActivity(marker);
            TestUtils.runOnMainSync(() -> editText.requestFocus());

            // Wait until the MockIme gets bound to the TestActivity.
            expectBindInput(stream, Process.myPid(), TIMEOUT);

            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

            // Get app window token
            final IBinder appWindowToken = TestUtils.getOnMainSync(
                    () -> editText.getApplicationWindowToken());

            try (WindowFocusStealer focusStealer =
                         WindowFocusStealer.connect(instrumentation.getTargetContext(), TIMEOUT)) {

                focusStealer.stealWindowFocus(appWindowToken, TIMEOUT);

                // Wait until the edit text loses window focus.
                TestUtils.waitOnMainUntil(() -> !editText.hasWindowFocus(), TIMEOUT);
            }
            // Wait until the edit text gains window focus again.
            TestUtils.waitOnMainUntil(() -> editText.hasWindowFocus(), TIMEOUT);

            // Not expect the input connection will be started or finished even gaining non-IME
            // focusable window focus.
            notExpectEvent(stream, event -> "onFinishInput".equals(event.getEventName())
                    || "onStartInput".equals(event.getEventName()), TIMEOUT);

            // Verify the input connection of the EditText is still active and can accept text.
            final InputMethodManager imm = editText.getContext().getSystemService(
                    InputMethodManager.class);
            assertTrue(TestUtils.getOnMainSync(() -> imm.isActive(editText)));
            assertTrue(TestUtils.getOnMainSync(() -> imm.isAcceptingText()));
        }
    }

    private EditText launchTestActivity(String marker) {
        final AtomicReference<EditText> editTextRef = new AtomicReference<>();
        TestActivity.startSync(activity-> {
            final LinearLayout layout = new LinearLayout(activity);
            layout.setOrientation(LinearLayout.VERTICAL);

            final EditText editText = new EditText(activity);
            editText.setPrivateImeOptions(marker);
            editText.setHint("editText");
            editText.requestFocus();
            editTextRef.set(editText);

            layout.addView(editText);
            return layout;
        });
        return editTextRef.get();
    }

    private static Predicate<ImeEvent> onFinishInputMatcher() {
        return event -> TextUtils.equals("onFinishInput", event.getEventName());
    }
}
