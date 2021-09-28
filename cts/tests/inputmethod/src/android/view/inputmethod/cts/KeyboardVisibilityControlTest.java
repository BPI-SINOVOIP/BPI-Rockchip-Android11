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

package android.view.inputmethod.cts;

import static android.view.View.VISIBLE;
import static android.view.WindowInsets.Type.ime;
import static android.view.WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_UNSPECIFIED;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE;
import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeInvisible;
import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeVisible;
import static android.view.inputmethod.cts.util.TestUtils.getOnMainSync;
import static android.view.inputmethod.cts.util.TestUtils.runOnMainSync;

import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEventWithKeyValue;
import static com.android.cts.mockime.ImeEventStreamTestUtils.notExpectEvent;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.graphics.Color;
import android.os.SystemClock;
import android.support.test.uiautomator.UiObject2;
import android.text.TextUtils;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsetsController;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethod;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.cts.util.EndToEndImeTestBase;
import android.view.inputmethod.cts.util.TestActivity;
import android.view.inputmethod.cts.util.TestUtils;
import android.view.inputmethod.cts.util.TestWebView;
import android.view.inputmethod.cts.util.UnlockScreenRule;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.mockime.ImeEvent;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Predicate;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class KeyboardVisibilityControlTest extends EndToEndImeTestBase {
    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);
    private static final long NOT_EXPECT_TIMEOUT = TimeUnit.SECONDS.toMillis(1);

    @Rule
    public final UnlockScreenRule mUnlockScreenRule = new UnlockScreenRule();

    private static final String TEST_MARKER_PREFIX =
            "android.view.inputmethod.cts.KeyboardVisibilityControlTest";

    private static String getTestMarker() {
        return TEST_MARKER_PREFIX + "/"  + SystemClock.elapsedRealtimeNanos();
    }

    private static Predicate<ImeEvent> editorMatcher(
            @NonNull String eventName, @NonNull String marker) {
        return event -> {
            if (!TextUtils.equals(eventName, event.getEventName())) {
                return false;
            }
            final EditorInfo editorInfo = event.getArguments().getParcelable("editorInfo");
            return TextUtils.equals(marker, editorInfo.privateImeOptions);
        };
    }

    private static Predicate<ImeEvent> showSoftInputMatcher(int requiredFlags) {
        return event -> {
            if (!TextUtils.equals("showSoftInput", event.getEventName())) {
                return false;
            }
            final int flags = event.getArguments().getInt("flags");
            return (flags & requiredFlags) == requiredFlags;
        };
    }

    private static Predicate<ImeEvent> hideSoftInputMatcher() {
        return event -> TextUtils.equals("hideSoftInput", event.getEventName());
    }

    private static Predicate<ImeEvent> onFinishInputViewMatcher(boolean expectedFinishingInput) {
        return event -> {
            if (!TextUtils.equals("onFinishInputView", event.getEventName())) {
                return false;
            }
            final boolean finishingInput = event.getArguments().getBoolean("finishingInput");
            return finishingInput == expectedFinishingInput;
        };
    }

    private Pair<EditText, EditText> launchTestActivity(@NonNull String focusedMarker,
            @NonNull String nonFocusedMarker) {
        final AtomicReference<EditText> focusedEditTextRef = new AtomicReference<>();
        final AtomicReference<EditText> nonFocusedEditTextRef = new AtomicReference<>();
        TestActivity.startSync(activity -> {
            final LinearLayout layout = new LinearLayout(activity);
            layout.setOrientation(LinearLayout.VERTICAL);

            final EditText focusedEditText = new EditText(activity);
            focusedEditText.setHint("focused editText");
            focusedEditText.setPrivateImeOptions(focusedMarker);
            focusedEditText.requestFocus();
            focusedEditTextRef.set(focusedEditText);
            layout.addView(focusedEditText);

            final EditText nonFocusedEditText = new EditText(activity);
            nonFocusedEditText.setPrivateImeOptions(nonFocusedMarker);
            nonFocusedEditText.setHint("target editText");
            nonFocusedEditTextRef.set(nonFocusedEditText);
            layout.addView(nonFocusedEditText);
            return layout;
        });
        return new Pair<>(focusedEditTextRef.get(), nonFocusedEditTextRef.get());
    }

    private EditText launchTestActivity(@NonNull String marker) {
        return launchTestActivity(marker, getTestMarker()).first;
    }

    @Test
    public void testBasicShowHideSoftInput() throws Exception {
        final InputMethodManager imm = InstrumentationRegistry.getInstrumentation()
                .getTargetContext().getSystemService(InputMethodManager.class);

        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final EditText editText = launchTestActivity(marker);

            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            notExpectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectImeInvisible(TIMEOUT);

            assertTrue("isActive() must return true if the View has IME focus",
                    getOnMainSync(() -> imm.isActive(editText)));

            // Test showSoftInput() flow
            assertTrue("showSoftInput must success if the View has IME focus",
                    getOnMainSync(() -> imm.showSoftInput(editText, 0)));

            expectEvent(stream, showSoftInputMatcher(InputMethod.SHOW_EXPLICIT), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.VISIBLE, TIMEOUT);
            expectImeVisible(TIMEOUT);

            // Test hideSoftInputFromWindow() flow
            assertTrue("hideSoftInputFromWindow must success if the View has IME focus",
                    getOnMainSync(() -> imm.hideSoftInputFromWindow(editText.getWindowToken(), 0)));

            expectEvent(stream, hideSoftInputMatcher(), TIMEOUT);
            expectEvent(stream, onFinishInputViewMatcher(false), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.GONE, TIMEOUT);
            expectImeInvisible(TIMEOUT);
        }
    }

    @Test
    public void testShowHideSoftInputShouldBeIgnoredOnNonFocusedView() throws Exception {
        final InputMethodManager imm = InstrumentationRegistry.getInstrumentation()
                .getTargetContext().getSystemService(InputMethodManager.class);

        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String focusedMarker = getTestMarker();
            final String nonFocusedMarker = getTestMarker();
            final Pair<EditText, EditText> editTextPair =
                    launchTestActivity(focusedMarker, nonFocusedMarker);
            final EditText nonFocusedEditText = editTextPair.second;

            expectEvent(stream, editorMatcher("onStartInput", focusedMarker), TIMEOUT);

            expectImeInvisible(TIMEOUT);
            assertFalse("isActive() must return false if the View does not have IME focus",
                    getOnMainSync(() -> imm.isActive(nonFocusedEditText)));
            assertFalse("showSoftInput must fail if the View does not have IME focus",
                    getOnMainSync(() -> imm.showSoftInput(nonFocusedEditText, 0)));
            notExpectEvent(stream, showSoftInputMatcher(InputMethod.SHOW_EXPLICIT), TIMEOUT);

            assertFalse("hideSoftInputFromWindow must fail if the View does not have IME focus",
                    getOnMainSync(() -> imm.hideSoftInputFromWindow(
                            nonFocusedEditText.getWindowToken(), 0)));
            notExpectEvent(stream, hideSoftInputMatcher(), TIMEOUT);
            expectImeInvisible(TIMEOUT);
        }
    }

    @Test
    public void testToggleSoftInput() throws Exception {
        final InputMethodManager imm = InstrumentationRegistry.getInstrumentation()
                .getTargetContext().getSystemService(InputMethodManager.class);

        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final String marker = getTestMarker();
            final EditText editText = launchTestActivity(marker);

            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            notExpectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectImeInvisible(TIMEOUT);

            // Test toggleSoftInputFromWindow() flow
            runOnMainSync(() -> imm.toggleSoftInputFromWindow(editText.getWindowToken(), 0, 0));

            expectEvent(stream.copy(), showSoftInputMatcher(InputMethod.SHOW_EXPLICIT), TIMEOUT);
            expectEvent(stream.copy(), editorMatcher("onStartInputView", marker), TIMEOUT);
            expectImeVisible(TIMEOUT);

            // Calling toggleSoftInputFromWindow() must hide the IME.
            runOnMainSync(() -> imm.toggleSoftInputFromWindow(editText.getWindowToken(), 0, 0));

            expectEvent(stream, hideSoftInputMatcher(), TIMEOUT);
            expectEvent(stream, onFinishInputViewMatcher(false), TIMEOUT);
            expectImeInvisible(TIMEOUT);
        }
    }

    @Test
    public void testShowHideKeyboardOnWebView() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final UiObject2 inputTextField = TestWebView.launchTestWebViewActivity(
                    TimeUnit.SECONDS.toMillis(5));
            assertNotNull("Editor must exists on WebView", inputTextField);

            expectEvent(stream, event -> "onStartInput".equals(event.getEventName()), TIMEOUT);
            notExpectEvent(stream, event -> "onStartInputView".equals(event.getEventName()),
                    TIMEOUT);
            expectImeInvisible(TIMEOUT);

            inputTextField.click();
            expectEvent(stream.copy(), showSoftInputMatcher(InputMethod.SHOW_EXPLICIT), TIMEOUT);
            expectEvent(stream.copy(), event -> "onStartInputView".equals(event.getEventName()),
                    TIMEOUT);
            expectImeVisible(TIMEOUT);
        }
    }

    @Test
    public void testFloatingImeHideKeyboardAfterBackPressed() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final InputMethodManager imm = instrumentation.getTargetContext().getSystemService(
                InputMethodManager.class);

        // Initial MockIme with floating IME settings.
        try (MockImeSession imeSession = MockImeSession.create(
                instrumentation.getContext(), instrumentation.getUiAutomation(),
                getFloatingImeSettings(Color.BLACK))) {
            final ImeEventStream stream = imeSession.openEventStream();
            final String marker = getTestMarker();
            final EditText editText = launchTestActivity(marker);

            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            notExpectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectImeInvisible(TIMEOUT);

            assertTrue("isActive() must return true if the View has IME focus",
                    getOnMainSync(() -> imm.isActive(editText)));

            // Test showSoftInput() flow
            assertTrue("showSoftInput must success if the View has IME focus",
                    getOnMainSync(() -> imm.showSoftInput(editText, 0)));

            expectEvent(stream, showSoftInputMatcher(InputMethod.SHOW_EXPLICIT), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.VISIBLE, TIMEOUT);
            expectImeVisible(TIMEOUT);

            // Pressing back key, expect soft-keyboard will become invisible.
            instrumentation.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
            expectEvent(stream, hideSoftInputMatcher(), TIMEOUT);
            expectEvent(stream, onFinishInputViewMatcher(false), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.GONE, TIMEOUT);
            expectImeInvisible(TIMEOUT);
        }
    }

    @Test
    public void testImeVisibilityWhenDismisingDialogWithImeFocused() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final InputMethodManager imm = instrumentation.getTargetContext().getSystemService(
                InputMethodManager.class);
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            // Launch a simple test activity
            final TestActivity testActivity = TestActivity.startSync(activity -> {
                final LinearLayout layout = new LinearLayout(activity);
                return layout;
            });

            // Launch a dialog
            final String marker = getTestMarker();
            final AtomicReference<EditText> editTextRef = new AtomicReference<>();
            final AtomicReference<AlertDialog> dialogRef = new AtomicReference<>();
            TestUtils.runOnMainSync(() -> {
                final EditText editText = new EditText(testActivity);
                editText.setHint("focused editText");
                editText.setPrivateImeOptions(marker);
                editText.requestFocus();
                final AlertDialog dialog = new AlertDialog.Builder(testActivity)
                        .setView(editText)
                        .create();
                final WindowInsetsController.OnControllableInsetsChangedListener listener =
                        new WindowInsetsController.OnControllableInsetsChangedListener() {
                            @Override
                            public void onControllableInsetsChanged(
                                    @NonNull WindowInsetsController controller, int typeMask) {
                                if ((typeMask & ime()) != 0) {
                                    editText.getWindowInsetsController()
                                            .removeOnControllableInsetsChangedListener(this);
                                    editText.getWindowInsetsController().show(ime());
                                }
                            }
                        };
                dialog.show();
                editText.getWindowInsetsController().addOnControllableInsetsChangedListener(
                        listener);
                editTextRef.set(editText);
                dialogRef.set(dialog);
            });
            TestUtils.waitOnMainUntil(() -> dialogRef.get().isShowing()
                    && editTextRef.get().hasFocus(), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.VISIBLE, TIMEOUT);
            expectImeVisible(TIMEOUT);

            // Hide keyboard and dismiss dialog.
            TestUtils.runOnMainSync(() -> {
                editTextRef.get().getWindowInsetsController().hide(ime());
                dialogRef.get().dismiss();
            });

            // Expect onFinishInput called and keyboard should hide successfully.
            expectEvent(stream, hideSoftInputMatcher(), TIMEOUT);
            expectEvent(stream, onFinishInputViewMatcher(false), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.GONE, TIMEOUT);
            expectImeInvisible(TIMEOUT);

            // Expect fake input connection started and keyboard invisible after activity focused.
            final ImeEvent onStart = expectEvent(stream,
                    event -> "onStartInput".equals(event.getEventName()), TIMEOUT);
            assertTrue(onStart.getEnterState().hasDummyInputConnection());
            TestUtils.waitOnMainUntil(() -> testActivity.hasWindowFocus(), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.GONE, TIMEOUT);
            expectImeInvisible(TIMEOUT);
        }
    }

    @Test
    public void testImeState_EditorDialogLostFocusAfterUnlocked_Unspecified() throws Exception {
        runImeDoesntReshowAfterKeyguardTest(SOFT_INPUT_STATE_UNSPECIFIED);
    }

    @Test
    public void testImeState_EditorDialogLostFocusAfterUnlocked_Visible() throws Exception {
        runImeDoesntReshowAfterKeyguardTest(SOFT_INPUT_STATE_VISIBLE);
    }

    @Test
    public void testImeState_EditorDialogLostFocusAfterUnlocked_AlwaysVisible() throws Exception {
        runImeDoesntReshowAfterKeyguardTest(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
    }

    @Test
    public void testImeState_EditorDialogLostFocusAfterUnlocked_Hidden() throws Exception {
        runImeDoesntReshowAfterKeyguardTest(SOFT_INPUT_STATE_HIDDEN);
    }

    @Test
    public void testImeState_EditorDialogLostFocusAfterUnlocked_AlwaysHidden() throws Exception {
        runImeDoesntReshowAfterKeyguardTest(SOFT_INPUT_STATE_ALWAYS_HIDDEN);
    }

    private void runImeDoesntReshowAfterKeyguardTest(int softInputState) throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            // Launch a simple test activity
            final TestActivity testActivity =
                    TestActivity.startSync(activity -> new LinearLayout(activity));

            // Launch a dialog and show keyboard
            final String marker = getTestMarker();
            final AtomicReference<EditText> editTextRef = new AtomicReference<>();
            final AtomicReference<AlertDialog> dialogRef = new AtomicReference<>();
            TestUtils.runOnMainSync(() -> {
                final EditText editText = new EditText(testActivity);
                editText.setHint("focused editText");
                editText.setPrivateImeOptions(marker);
                editText.requestFocus();
                final AlertDialog dialog = new AlertDialog.Builder(testActivity)
                        .setView(editText)
                        .create();
                dialog.getWindow().setSoftInputMode(softInputState);
                dialog.show();
                editText.getWindowInsetsController().show(ime());
                editTextRef.set(editText);
                dialogRef.set(dialog);
            });

            TestUtils.waitOnMainUntil(() -> dialogRef.get().isShowing()
                    && editTextRef.get().hasFocus(), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.VISIBLE, TIMEOUT);
            expectImeVisible(TIMEOUT);

            // Clear editor focus after screen-off
            TestUtils.turnScreenOff();
            TestUtils.waitOnMainUntil(() -> editTextRef.get().getWindowVisibility() != VISIBLE,
                    TIMEOUT);
            expectEvent(stream, onFinishInputViewMatcher(true), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);
            expectEvent(stream, editorMatcher("onStartInputView", marker), TIMEOUT);
            // Expect showSoftInput comes when system notify InsetsController to apply show IME
            // insets after IME input target updated.
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
            notExpectEvent(stream, hideSoftInputMatcher(), NOT_EXPECT_TIMEOUT);
            TestUtils.runOnMainSync(editTextRef.get()::clearFocus);

            // Verify IME will invisible after device unlocked
            TestUtils.turnScreenOn();
            TestUtils.unlockScreen();
            // Expect hideSoftInput and onFinishInputView will called by IMMS when the same window
            // focused since the editText view focus has been cleared.
            TestUtils.waitOnMainUntil(() -> editTextRef.get().hasWindowFocus()
                    && !editTextRef.get().hasFocus(), TIMEOUT);
            expectEvent(stream, hideSoftInputMatcher(), TIMEOUT);
            expectEvent(stream, onFinishInputViewMatcher(false), TIMEOUT);
            expectImeInvisible(TIMEOUT);
        }
    }

    private static ImeSettings.Builder getFloatingImeSettings(@ColorInt int navigationBarColor) {
        final ImeSettings.Builder builder = new ImeSettings.Builder();
        builder.setWindowFlags(0, FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        // As documented, Window#setNavigationBarColor() is actually ignored when the IME window
        // does not have FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS.  We are calling setNavigationBarColor()
        // to ensure it.
        builder.setNavigationBarColor(navigationBarColor);
        return builder;
    }
}
