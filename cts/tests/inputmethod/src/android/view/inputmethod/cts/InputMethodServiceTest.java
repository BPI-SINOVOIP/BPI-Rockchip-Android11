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

import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE;
import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeInvisible;
import static android.view.inputmethod.cts.util.InputMethodVisibilityVerifier.expectImeVisible;
import static android.view.inputmethod.cts.util.TestUtils.getOnMainSync;
import static android.view.inputmethod.cts.util.TestUtils.runOnMainSync;
import static android.view.inputmethod.cts.util.TestUtils.waitOnMainUntil;

import static com.android.cts.mockime.ImeEventStreamTestUtils.EventFilterMode.CHECK_EXIT_EVENT_ONLY;
import static com.android.cts.mockime.ImeEventStreamTestUtils.editorMatcher;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEventWithKeyValue;
import static com.android.cts.mockime.ImeEventStreamTestUtils.notExpectEvent;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.graphics.Matrix;
import android.inputmethodservice.InputMethodService;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.cts.util.EndToEndImeTestBase;
import android.view.inputmethod.cts.util.TestActivity;
import android.view.inputmethod.cts.util.TestUtils;
import android.view.inputmethod.cts.util.UnlockScreenRule;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.mockime.ImeCommand;
import com.android.cts.mockime.ImeEvent;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Predicate;

/**
 * Tests for {@link InputMethodService} methods.
 */
@MediumTest
@RunWith(AndroidJUnit4.class)
public class InputMethodServiceTest extends EndToEndImeTestBase {
    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);
    private static final long EXPECTED_TIMEOUT = TimeUnit.SECONDS.toMillis(2);

    @Rule
    public final UnlockScreenRule mUnlockScreenRule = new UnlockScreenRule();

    private Instrumentation mInstrumentation;

    private static Predicate<ImeEvent> backKeyDownMatcher(boolean expectedReturnValue) {
        return event -> {
            if (!TextUtils.equals("onKeyDown", event.getEventName())) {
                return false;
            }
            final int keyCode = event.getArguments().getInt("keyCode");
            if (keyCode != KeyEvent.KEYCODE_BACK) {
                return false;
            }
            return event.getReturnBooleanValue() == expectedReturnValue;
        };
    }

    @Before
    public void setup() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
    }

    private TestActivity createTestActivity(final int windowFlags) {
        return TestActivity.startSync(activity -> {
            final LinearLayout layout = new LinearLayout(activity);
            layout.setOrientation(LinearLayout.VERTICAL);

            final EditText editText = new EditText(activity);
            editText.setText("Editable");
            layout.addView(editText);
            editText.requestFocus();

            activity.getWindow().setSoftInputMode(windowFlags);
            return layout;
        });
    }

    @Test
    public void verifyLayoutInflaterContext() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            createTestActivity(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
            expectEvent(stream, event -> "onStartInputView".equals(event.getEventName()), TIMEOUT);

            final ImeCommand command = imeSession.verifyLayoutInflaterContext();
            assertTrue("InputMethodService.getLayoutInflater().getContext() must be equal to"
                    + " InputMethodService.this",
                    expectCommand(stream, command, TIMEOUT).getReturnBooleanValue());
        }
    }

    private void verifyImeConsumesBackButton(int backDisposition) throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final TestActivity testActivity = createTestActivity(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
            expectEvent(stream, event -> "onStartInputView".equals(event.getEventName()), TIMEOUT);

            final ImeCommand command = imeSession.callSetBackDisposition(backDisposition);
            expectCommand(stream, command, TIMEOUT);

            testActivity.setIgnoreBackKey(true);
            assertEquals(0,
                    (long) getOnMainSync(() -> testActivity.getOnBackPressedCallCount()));
            mInstrumentation.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);

            expectEvent(stream, backKeyDownMatcher(true), CHECK_EXIT_EVENT_ONLY, TIMEOUT);

            // Make sure TestActivity#onBackPressed() is NOT called.
            try {
                waitOnMainUntil(() -> testActivity.getOnBackPressedCallCount() > 0,
                        EXPECTED_TIMEOUT);
                fail("Activity#onBackPressed() should not be called");
            } catch (TimeoutException e) {
                // This is fine.  We actually expect timeout.
            }
        }
    }

    @Test
    public void testSetBackDispositionDefault() throws Exception {
        verifyImeConsumesBackButton(InputMethodService.BACK_DISPOSITION_DEFAULT);
    }

    @Test
    public void testSetBackDispositionWillNotDismiss() throws Exception {
        verifyImeConsumesBackButton(InputMethodService.BACK_DISPOSITION_WILL_NOT_DISMISS);
    }

    @Test
    public void testSetBackDispositionWillDismiss() throws Exception {
        verifyImeConsumesBackButton(InputMethodService.BACK_DISPOSITION_WILL_DISMISS);
    }

    @Test
    public void testSetBackDispositionAdjustNothing() throws Exception {
        verifyImeConsumesBackButton(InputMethodService.BACK_DISPOSITION_ADJUST_NOTHING);
    }

    @Test
    public void testRequestHideSelf() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            createTestActivity(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
            expectEvent(stream, event -> "onStartInputView".equals(event.getEventName()), TIMEOUT);

            expectImeVisible(TIMEOUT);

            imeSession.callRequestHideSelf(0);
            expectEvent(stream, event -> "hideSoftInput".equals(event.getEventName()), TIMEOUT);
            expectEvent(stream, event -> "onFinishInputView".equals(event.getEventName()), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.GONE, TIMEOUT);

            expectImeInvisible(TIMEOUT);
        }
    }

    @Test
    public void testRequestShowSelf() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            createTestActivity(SOFT_INPUT_STATE_ALWAYS_HIDDEN);
            notExpectEvent(
                    stream, event -> "onStartInputView".equals(event.getEventName()), TIMEOUT);

            expectImeInvisible(TIMEOUT);

            imeSession.callRequestShowSelf(0);
            expectEvent(stream, event -> "showSoftInput".equals(event.getEventName()), TIMEOUT);
            expectEvent(stream, event -> "onStartInputView".equals(event.getEventName()), TIMEOUT);
            expectEventWithKeyValue(stream, "onWindowVisibilityChanged", "visible",
                    View.VISIBLE, TIMEOUT);

            expectImeVisible(TIMEOUT);
        }
    }

    private static void assertSynthesizedSoftwareKeyEvent(KeyEvent keyEvent, int expectedAction,
            int expectedKeyCode, long expectedEventTimeBefore, long expectedEventTimeAfter) {
        if (keyEvent.getEventTime() < expectedEventTimeBefore
                || expectedEventTimeAfter < keyEvent.getEventTime()) {
            fail(String.format("EventTime must be within [%d, %d],"
                            + " which was %d", expectedEventTimeBefore, expectedEventTimeAfter,
                    keyEvent.getEventTime()));
        }
        assertEquals(expectedAction, keyEvent.getAction());
        assertEquals(expectedKeyCode, keyEvent.getKeyCode());
        assertEquals(KeyCharacterMap.VIRTUAL_KEYBOARD, keyEvent.getDeviceId());
        assertEquals(0, keyEvent.getScanCode());
        assertEquals(0, keyEvent.getRepeatCount());
        assertEquals(0, keyEvent.getRepeatCount());
        final int mustHaveFlags = KeyEvent.FLAG_SOFT_KEYBOARD | KeyEvent.FLAG_KEEP_TOUCH_MODE;
        final int mustNotHaveFlags = KeyEvent.FLAG_FROM_SYSTEM;
        if ((keyEvent.getFlags() & mustHaveFlags) == 0
                || (keyEvent.getFlags() & mustNotHaveFlags) != 0) {
            fail(String.format("Flags must have FLAG_SOFT_KEYBOARD|"
                    + "FLAG_KEEP_TOUCH_MODE and must not have FLAG_FROM_SYSTEM, "
                    + "which was 0x%08X", keyEvent.getFlags()));
        }
    }

    /**
     * Test compatibility requirements of {@link InputMethodService#sendDownUpKeyEvents(int)}.
     */
    @Test
    public void testSendDownUpKeyEvents() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final ImeEventStream stream = imeSession.openEventStream();

            final AtomicReference<ArrayList<KeyEvent>> keyEventsRef = new AtomicReference<>();
            final String marker = "testSendDownUpKeyEvents/" + SystemClock.elapsedRealtimeNanos();

            TestActivity.startSync(activity -> {
                final LinearLayout layout = new LinearLayout(activity);
                layout.setOrientation(LinearLayout.VERTICAL);

                final ArrayList<KeyEvent> keyEvents = new ArrayList<>();
                keyEventsRef.set(keyEvents);
                final EditText editText = new EditText(activity) {
                    @Override
                    public InputConnection onCreateInputConnection(EditorInfo editorInfo) {
                        return new InputConnectionWrapper(
                                super.onCreateInputConnection(editorInfo), false) {
                            /**
                             * {@inheritDoc}
                             */
                            @Override
                            public boolean sendKeyEvent(KeyEvent event) {
                                keyEvents.add(event);
                                return super.sendKeyEvent(event);
                            }
                        };
                    }
                };
                editText.setPrivateImeOptions(marker);
                layout.addView(editText);
                editText.requestFocus();
                return layout;
            });

            // Wait until "onStartInput" gets called for the EditText.
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

            // Make sure that InputConnection#sendKeyEvent() has never been called yet.
            assertTrue(TestUtils.getOnMainSync(
                    () -> new ArrayList<>(keyEventsRef.get())).isEmpty());

            final int expectedKeyCode = KeyEvent.KEYCODE_0;
            final long uptimeStart = SystemClock.uptimeMillis();
            expectCommand(stream, imeSession.callSendDownUpKeyEvents(expectedKeyCode), TIMEOUT);
            final long uptimeEnd = SystemClock.uptimeMillis();

            final ArrayList<KeyEvent> keyEvents = TestUtils.getOnMainSync(
                    () -> new ArrayList<>(keyEventsRef.get()));

            // Check KeyEvent objects.
            assertNotNull(keyEvents);
            assertEquals(2, keyEvents.size());
            assertSynthesizedSoftwareKeyEvent(keyEvents.get(0), KeyEvent.ACTION_DOWN,
                    expectedKeyCode, uptimeStart, uptimeEnd);
            assertSynthesizedSoftwareKeyEvent(keyEvents.get(1), KeyEvent.ACTION_UP,
                    expectedKeyCode, uptimeStart, uptimeEnd);
        }
    }

    /**
     * Ensure that {@link InputConnection#requestCursorUpdates(int)} works for the built-in
     * {@link EditText} and {@link InputMethodService#onUpdateCursorAnchorInfo(CursorAnchorInfo)}
     * will be called back.
     */
    @Test
    public void testOnUpdateCursorAnchorInfo() throws Exception {
        try (MockImeSession imeSession = MockImeSession.create(
                InstrumentationRegistry.getInstrumentation().getContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder())) {
            final String marker =
                    "testOnUpdateCursorAnchorInfo()/" + SystemClock.elapsedRealtimeNanos();

            final AtomicReference<EditText> editTextRef = new AtomicReference<>();
            final AtomicInteger requestCursorUpdatesCallCount = new AtomicInteger();
            TestActivity.startSync(activity -> {
                final LinearLayout layout = new LinearLayout(activity);
                layout.setOrientation(LinearLayout.VERTICAL);

                final EditText editText = new EditText(activity) {
                    @Override
                    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
                        final InputConnection original = super.onCreateInputConnection(outAttrs);
                        return new InputConnectionWrapper(original, false) {
                            @Override
                            public boolean requestCursorUpdates(int cursorUpdateMode) {
                                if (cursorUpdateMode == InputConnection.CURSOR_UPDATE_IMMEDIATE) {
                                    requestCursorUpdatesCallCount.incrementAndGet();
                                    return true;
                                }
                                return false;
                            }
                        };
                    }
                };
                editTextRef.set(editText);
                editText.setPrivateImeOptions(marker);
                layout.addView(editText);
                editText.requestFocus();
                return layout;
            });
            final EditText editText = editTextRef.get();

            final ImeEventStream stream = imeSession.openEventStream();
            expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

            // Make sure that InputConnection#requestCursorUpdates() returns true.
            assertTrue(expectCommand(stream,
                    imeSession.callRequestCursorUpdates(InputConnection.CURSOR_UPDATE_IMMEDIATE),
                    TIMEOUT).getReturnBooleanValue());

            // Also make sure that requestCursorUpdates() actually gets called only once.
            assertEquals(1, requestCursorUpdatesCallCount.get());

            final CursorAnchorInfo originalCursorAnchorInfo = new CursorAnchorInfo.Builder()
                    .setMatrix(new Matrix())
                    .setInsertionMarkerLocation(3.0f, 4.0f, 5.0f, 6.0f, 0)
                    .setSelectionRange(7, 8)
                    .build();

            runOnMainSync(() -> editText.getContext().getSystemService(InputMethodManager.class)
                    .updateCursorAnchorInfo(editText, originalCursorAnchorInfo));

            final CursorAnchorInfo receivedCursorAnchorInfo = expectEvent(stream,
                    event -> "onUpdateCursorAnchorInfo".equals(event.getEventName()),
                    TIMEOUT).getArguments().getParcelable("cursorAnchorInfo");
            assertNotNull(receivedCursorAnchorInfo);
            assertEquals(receivedCursorAnchorInfo, originalCursorAnchorInfo);
        }
    }
}
