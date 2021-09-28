/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static android.view.WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE;
import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeInvisible;
import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeVisible;
import static android.view.inputmethod.cts.util.TestUtils.runOnMainSync;
import static android.widget.PopupWindow.INPUT_METHOD_NEEDED;
import static android.widget.PopupWindow.INPUT_METHOD_NOT_NEEDED;

import static com.android.cts.mockime.ImeEventStreamTestUtils.editorMatcher;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectBindInput;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;
import static com.android.cts.mockime.ImeEventStreamTestUtils.notExpectEvent;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.cts.util.EndToEndImeTestBase;
import android.view.inputmethod.cts.util.TestActivity;
import android.view.inputmethod.cts.util.TestUtils;
import android.view.inputmethod.cts.util.UnlockScreenRule;
import android.view.inputmethod.cts.util.WindowFocusHandleService;
import android.view.inputmethod.cts.util.WindowFocusStealer;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.test.filters.FlakyTest;
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
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class FocusHandlingTest extends EndToEndImeTestBase {
    static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);
    static final long NOT_EXPECT_TIMEOUT = TimeUnit.SECONDS.toMillis(1);

    @Rule
    public final UnlockScreenRule mUnlockScreenRule = new UnlockScreenRule();

    private static final String TEST_MARKER_PREFIX =
            "android.view.inputmethod.cts.FocusHandlingTest";

    public EditText launchTestActivity(String marker) {
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

    public EditText launchTestActivity(String marker,
            @NonNull AtomicBoolean outEditHasWindowFocusRef) {
        final EditText editText = launchTestActivity(marker);
        editText.post(() -> {
            final ViewTreeObserver observerForEditText = editText.getViewTreeObserver();
            observerForEditText.addOnWindowFocusChangeListener((hasFocus) ->
                    outEditHasWindowFocusRef.set(editText.hasWindowFocus()));
            outEditHasWindowFocusRef.set(editText.hasWindowFocus());
        });
        return editText;
    }

    private static String getTestMarker() {
        return TEST_MARKER_PREFIX + "/"  + SystemClock.elapsedRealtimeNanos();
    }

    @FlakyTest(bugId = 149246840)
    @Test
    public void testOnStartInputCalledOnceIme() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final EditText editText = launchTestActivity(marker);

            // Wait until the MockIme gets bound to the TestActivity.
            expectBindInput(stream, Process.myPid(), TIMEOUT);

            // Emulate tap event
            CtsTouchUtils.emulateTapOnViewCenter(
                    InstrumentationRegistry.getInstrumentation(), null, editText);

            // Wait until "onStartInput" gets called for the EditText.
            final ImeEvent onStart =
                    expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

            assertFalse(stream.dump(), onStart.getEnterState().hasDummyInputConnection());
            assertFalse(stream.dump(), onStart.getArguments().getBoolean("restarting"));

            // There shouldn't be onStartInput any more.
            notExpectEvent(stream, editorMatcher("onStartInput", marker), NOT_EXPECT_TIMEOUT);
        }
    }

    @Test
    public void testSoftInputStateAlwaysVisibleWithoutFocusedEditorView() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final TestActivity testActivity = TestActivity.startSync(activity -> {
                final LinearLayout layout = new LinearLayout(activity);
                layout.setOrientation(LinearLayout.VERTICAL);

                final TextView textView = new TextView(activity) {
                    @Override
                    public boolean onCheckIsTextEditor() {
                        return false;
                    }
                };
                textView.setText("textView");
                textView.setPrivateImeOptions(marker);
                textView.requestFocus();

                activity.getWindow().setSoftInputMode(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                layout.addView(textView);
                return layout;
            });

            if (testActivity.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P) {
                // Input shouldn't start
                notExpectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
                // There shouldn't be onStartInput because the focused view is not an editor.
                notExpectEvent(stream, event -> "showSoftInput".equals(event.getEventName()),
                        TIMEOUT);
            } else {
                // Wait until the MockIme gets bound to the TestActivity.
                expectBindInput(stream, Process.myPid(), TIMEOUT);
                // For apps that target pre-P devices, onStartInput() should be called.
                expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
            }
        }
    }

    @Test
    public void testEditorStartsInput() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            TestActivity.startSync(activity -> {
                final LinearLayout layout = new LinearLayout(activity);
                layout.setOrientation(LinearLayout.VERTICAL);

                final EditText editText = new EditText(activity);
                editText.setPrivateImeOptions(marker);
                editText.setText("Editable");
                editText.requestFocus();
                layout.addView(editText);
                return layout;
            });

            // Input should start
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
        }
    }

    @Test
    public void testSoftInputStateAlwaysVisibleFocusedEditorView() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            TestActivity.startSync(activity -> {
                final LinearLayout layout = new LinearLayout(activity);
                layout.setOrientation(LinearLayout.VERTICAL);

                final EditText editText = new EditText(activity);
                editText.setText("editText");
                editText.requestFocus();

                activity.getWindow().setSoftInputMode(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                layout.addView(editText);
                return layout;
            });

            // Wait until the MockIme gets bound to the TestActivity.
            expectBindInput(stream, Process.myPid(), TIMEOUT);

            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
        }
    }

    /**
     * Makes sure that an existing {@link android.view.inputmethod.InputConnection} will not be
     * invalidated by showing a focusable {@link PopupWindow} with
     * {@link PopupWindow#INPUT_METHOD_NOT_NEEDED}.
     *
     * <p>If {@link android.view.WindowManager.LayoutParams#FLAG_ALT_FOCUSABLE_IM} is set and
     * {@link android.view.WindowManager.LayoutParams#FLAG_NOT_FOCUSABLE} is not set to a
     * {@link android.view.Window}, showing that window must not invalidate an existing valid
     * {@link android.view.inputmethod.InputConnection}.</p>
     *
     * @see android.view.WindowManager.LayoutParams#mayUseInputMethod(int)
     */
    @Test
    public void testFocusableWindowDoesNotInvalidateExistingInputConnection() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        try (MockImeSession imeSession = MockImeSession.create(
                instrumentation.getContext(),
                instrumentation.getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final EditText editText = launchTestActivity(marker);
            instrumentation.runOnMainSync(() -> editText.requestFocus());

            // Wait until the MockIme gets bound to the TestActivity.
            expectBindInput(stream, Process.myPid(), TIMEOUT);

            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

            // Make sure that InputConnection#commitText() works.
            final ImeCommand commit1 = imeSession.callCommitText("test commit", 1);
            expectCommand(stream, commit1, TIMEOUT);
            TestUtils.waitOnMainUntil(
                    () -> TextUtils.equals(editText.getText(), "test commit"), TIMEOUT);
            instrumentation.runOnMainSync(() -> editText.setText(""));

            // Create a popup window that cannot be the IME target.
            final PopupWindow popupWindow = TestUtils.getOnMainSync(() -> {
                final Context context = instrumentation.getTargetContext();
                final PopupWindow popup = new PopupWindow(context);
                popup.setFocusable(true);
                popup.setInputMethodMode(INPUT_METHOD_NOT_NEEDED);
                final TextView textView = new TextView(context);
                textView.setText("Test Text");
                popup.setContentView(textView);
                return popup;
            });

            // Show the popup window.
            instrumentation.runOnMainSync(() -> popupWindow.showAsDropDown(editText));
            instrumentation.waitForIdleSync();

            // Make sure that the EditText no longer has window-focus
            TestUtils.waitOnMainUntil(() -> !editText.hasWindowFocus(), TIMEOUT);

            // Make sure that InputConnection#commitText() works.
            final ImeCommand commit2 = imeSession.callCommitText("Hello!", 1);
            expectCommand(stream, commit2, TIMEOUT);
            TestUtils.waitOnMainUntil(
                    () -> TextUtils.equals(editText.getText(), "Hello!"), TIMEOUT);
            instrumentation.runOnMainSync(() -> editText.setText(""));

            stream.skipAll();

            final String marker2 = getTestMarker();
            // Call InputMethodManager#restartInput()
            instrumentation.runOnMainSync(() -> {
                editText.setPrivateImeOptions(marker2);
                editText.getContext()
                        .getSystemService(InputMethodManager.class)
                        .restartInput(editText);
            });

            // Make sure that onStartInput() is called with restarting == true.
            expectEvent(stream, event -> {
                if (!TextUtils.equals("onStartInput", event.getEventName())) {
                    return false;
                }
                if (!event.getArguments().getBoolean("restarting")) {
                    return false;
                }
                final EditorInfo editorInfo = event.getArguments().getParcelable("editorInfo");
                return TextUtils.equals(marker2, editorInfo.privateImeOptions);
            }, TIMEOUT);

            // Make sure that InputConnection#commitText() works.
            final ImeCommand commit3 = imeSession.callCommitText("World!", 1);
            expectCommand(stream, commit3, TIMEOUT);
            TestUtils.waitOnMainUntil(
                    () -> TextUtils.equals(editText.getText(), "World!"), TIMEOUT);
            instrumentation.runOnMainSync(() -> editText.setText(""));

            // Dismiss the popup window.
            instrumentation.runOnMainSync(() -> popupWindow.dismiss());
            instrumentation.waitForIdleSync();

            // Make sure that the EditText now has window-focus again.
            TestUtils.waitOnMainUntil(() -> editText.hasWindowFocus(), TIMEOUT);

            // Make sure that InputConnection#commitText() works.
            final ImeCommand commit4 = imeSession.callCommitText("Done!", 1);
            expectCommand(stream, commit4, TIMEOUT);
            TestUtils.waitOnMainUntil(
                    () -> TextUtils.equals(editText.getText(), "Done!"), TIMEOUT);
            instrumentation.runOnMainSync(() -> editText.setText(""));
        }
    }

    /**
     * Test case for Bug 152698568.
     *
     * <p>This test ensures that showing a non-focusable {@link PopupWindow} with
     * {@link PopupWindow#INPUT_METHOD_NEEDED} does not affect IME visibility.</p>
     */
    @Test
    public void testNonFocusablePopupWindowDoesNotAffectImeVisibility() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        try (MockImeSession imeSession = MockImeSession.create(
                instrumentation.getContext(),
                instrumentation.getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final EditText editText = launchTestActivity(marker);

            // Wait until the MockIme is connected to the edit text.
            runOnMainSync(editText::requestFocus);
            expectBindInput(stream, Process.myPid(), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

            expectImeInvisible(TIMEOUT);

            // Show IME.
            runOnMainSync(() -> editText.getContext().getSystemService(InputMethodManager.class)
                    .showSoftInput(editText, 0));

            expectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectImeVisible(TIMEOUT);

            // Create a non-focusable PopupWindow with INPUT_METHOD_NEEDED.
            final PopupWindow popupWindow = TestUtils.getOnMainSync(() -> {
                final Context context = instrumentation.getTargetContext();
                final PopupWindow popup = new PopupWindow(context);
                popup.setFocusable(false);
                popup.setInputMethodMode(INPUT_METHOD_NEEDED);
                final TextView textView = new TextView(context);
                textView.setText("Popup");
                popup.setContentView(textView);
                return popup;
            });

            // Show the popup window.
            runOnMainSync(() -> popupWindow.showAsDropDown(editText));
            instrumentation.waitForIdleSync();

            // Make sure that the IME remains to be visible.
            expectImeVisible(TIMEOUT);

            SystemClock.sleep(NOT_EXPECT_TIMEOUT);

            // Make sure that the IME remains to be visible.
            expectImeVisible(TIMEOUT);
        }
    }

    /**
     * Test case for Bug 70629102.
     *
     * {@link InputMethodManager#restartInput(View)} can be called even when another process
     * temporarily owns focused window. {@link InputMethodManager} should continue to work after
     * the IME target application gains window focus again.
     */
    @Test
    public void testRestartInputWhileOtherProcessHasWindowFocus() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        try (MockImeSession imeSession = MockImeSession.create(
                instrumentation.getContext(),
                instrumentation.getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final EditText editText = launchTestActivity(marker);
            instrumentation.runOnMainSync(() -> editText.requestFocus());

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

                // Call InputMethodManager#restartInput()
                instrumentation.runOnMainSync(() -> {
                    editText.getContext()
                            .getSystemService(InputMethodManager.class)
                            .restartInput(editText);
                });
            }

            // Wait until the edit text gains window focus again.
            TestUtils.waitOnMainUntil(() -> editText.hasWindowFocus(), TIMEOUT);

            // Make sure that InputConnection#commitText() still works.
            final ImeCommand command = imeSession.callCommitText("test commit", 1);
            expectCommand(stream, command, TIMEOUT);

            TestUtils.waitOnMainUntil(
                    () -> TextUtils.equals(editText.getText(), "test commit"), TIMEOUT);
        }
    }

    /**
     * Test {@link EditText#setShowSoftInputOnFocus(boolean)}.
     */
    @Test
    public void testSetShowInputOnFocus() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final EditText editText = launchTestActivity(marker);
            runOnMainSync(() -> editText.setShowSoftInputOnFocus(false));

            // Wait until "onStartInput" gets called for the EditText.
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

            // Emulate tap event
            CtsTouchUtils.emulateTapOnViewCenter(
                    InstrumentationRegistry.getInstrumentation(), null, editText);

            // "showSoftInput" must not happen when setShowSoftInputOnFocus(false) is called.
            notExpectEvent(stream, event -> "showSoftInput".equals(event.getEventName()),
                    NOT_EXPECT_TIMEOUT);
        }
    }

    @AppModeFull(reason = "Instant apps cannot hold android.permission.SYSTEM_ALERT_WINDOW")
    @Test
    public void testMultiWindowFocusHandleOnDifferentUiThread() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        try (CloseOnce session = CloseOnce.of(new ServiceSession(instrumentation.getContext()));
             MockImeSession imeSession = MockImeSession.create(
                     instrumentation.getContext(), instrumentation.getUiAutomation(),
                     new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();
            final AtomicBoolean popupTextHasWindowFocus = new AtomicBoolean(false);
            final AtomicBoolean popupTextHasViewFocus = new AtomicBoolean(false);
            final AtomicBoolean editTextHasWindowFocus = new AtomicBoolean(false);

            // Start a TestActivity and verify the edit text will receive focus and keyboard shown.
            final String marker = getTestMarker();
            final EditText editText = launchTestActivity(marker, editTextHasWindowFocus);

            // Wait until the MockIme gets bound to the TestActivity.
            expectBindInput(stream, Process.myPid(), TIMEOUT);

            // Emulate tap event
            CtsTouchUtils.emulateTapOnViewCenter(instrumentation, null, editText);
            TestUtils.waitOnMainUntil(() -> editTextHasWindowFocus.get(), TIMEOUT);

            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);

            // Create a popupTextView which from Service with different UI thread.
            final ServiceSession serviceSession = (ServiceSession) session.mAutoCloseable;
            final EditText popupTextView = serviceSession.getService().getPopupTextView(
                    popupTextHasWindowFocus);
            assertTrue(popupTextView.getHandler().getLooper()
                    != serviceSession.getService().getMainLooper());

            // Verify popupTextView will also receive window focus change and soft keyboard shown
            // after tapping the view.
            final String marker1 = getTestMarker();
            popupTextView.post(() -> {
                popupTextView.setPrivateImeOptions(marker1);
                popupTextHasViewFocus.set(popupTextView.requestFocus());
            });
            TestUtils.waitOnMainUntil(() -> popupTextHasViewFocus.get(), TIMEOUT);

            CtsTouchUtils.emulateTapOnViewCenter(instrumentation, null, popupTextView);
            TestUtils.waitOnMainUntil(() -> popupTextHasWindowFocus.get()
                            && !editTextHasWindowFocus.get(), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInput", marker1), TIMEOUT);
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);

            // Emulate tap event for editText again, verify soft keyboard and window focus will
            // come back.
            CtsTouchUtils.emulateTapOnViewCenter(instrumentation, null, editText);
            TestUtils.waitOnMainUntil(() -> editTextHasWindowFocus.get()
                    && !popupTextHasWindowFocus.get(), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);

            // Remove the popTextView window and back to test activity, and then verify if
            // commitText is still workable.
            session.close();
            TestUtils.waitOnMainUntil(() -> editText.hasWindowFocus(), TIMEOUT);
            final ImeCommand commit = imeSession.callCommitText("test commit", 1);
            expectCommand(stream, commit, TIMEOUT);
            TestUtils.waitOnMainUntil(
                    () -> TextUtils.equals(editText.getText(), "test commit"), TIMEOUT);
        }
    }

    @Test
    public void testKeyboardStateAfterImeFocusableFlagChanged() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        try (MockImeSession imeSession = MockImeSession.create(
                     instrumentation.getContext(), instrumentation.getUiAutomation(),
                     new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();
            final AtomicReference<EditText> editTextRef = new AtomicReference<>();
            final String marker = getTestMarker();
            final TestActivity testActivity = TestActivity.startSync(activity-> {
                // Initially set activity window to not IME focusable.
                activity.getWindow().addFlags(FLAG_ALT_FOCUSABLE_IM);

                final LinearLayout layout = new LinearLayout(activity);
                layout.setOrientation(LinearLayout.VERTICAL);

                final EditText editText = new EditText(activity);
                editText.setPrivateImeOptions(marker);
                editText.setHint("editText");
                editTextRef.set(editText);
                editText.requestFocus();

                layout.addView(editText);
                return layout;
            });

            // Emulate tap event, expect there is no "onStartInput", and "showSoftInput" happened.
            final EditText editText = editTextRef.get();
            CtsTouchUtils.emulateTapOnViewCenter(instrumentation, null, editText);
            notExpectEvent(stream, editorMatcher("onStartInput", marker), NOT_EXPECT_TIMEOUT);
            notExpectEvent(stream, event -> "showSoftInput".equals(event.getEventName()),
                    NOT_EXPECT_TIMEOUT);

            // Set testActivity window to be IME focusable.
            testActivity.getWindow().getDecorView().post(() -> {
                final WindowManager.LayoutParams params = testActivity.getWindow().getAttributes();
                testActivity.getWindow().clearFlags(FLAG_ALT_FOCUSABLE_IM);
                editTextRef.get().requestFocus();
            });

            // Make sure test activity's window has changed to be IME focusable.
            TestUtils.waitOnMainUntil(() -> WindowManager.LayoutParams.mayUseInputMethod(
                    testActivity.getWindow().getAttributes().flags), TIMEOUT);

            // Emulate tap event again.
            CtsTouchUtils.emulateTapOnViewCenter(instrumentation, null, editText);
            assertTrue(TestUtils.getOnMainSync(() -> editText.hasFocus()
                    && editText.hasWindowFocus()));

            // "onStartInput", and "showSoftInput" must happen when editText became IME focusable.
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
        }
    }

    private static class ServiceSession implements ServiceConnection, AutoCloseable {
        private final Context mContext;

        ServiceSession(Context context) {
            mContext = context;
            Intent service = new Intent(mContext, WindowFocusHandleService.class);
            mContext.bindService(service, this, Context.BIND_AUTO_CREATE);

            // Wait for service bound.
            try {
                TestUtils.waitOnMainUntil(() -> WindowFocusHandleService.getInstance() != null,
                        TIMEOUT, "WindowFocusHandleService should be bound");
            } catch (TimeoutException e) {
                fail("WindowFocusHandleService should be bound");
            }
        }

        @Override
        public void close() throws Exception {
            mContext.unbindService(this);
        }

        WindowFocusHandleService getService() {
            return WindowFocusHandleService.getInstance();
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
        }
    }

    private static final class CloseOnce implements AutoCloseable {
        final AtomicBoolean mClosed = new AtomicBoolean(false);
        final AutoCloseable mAutoCloseable;
        private CloseOnce(@NonNull AutoCloseable autoCloseable) {
            mAutoCloseable = autoCloseable;
        }
        @Override
        public void close() throws Exception {
            if (!mClosed.getAndSet(true)) {
                mAutoCloseable.close();
            }
        }
        @NonNull
        static CloseOnce of(@NonNull AutoCloseable autoCloseable) {
            return new CloseOnce(autoCloseable);
        }
    }
}
