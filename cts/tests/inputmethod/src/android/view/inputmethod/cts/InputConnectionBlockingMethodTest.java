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

import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE;

import static com.android.compatibility.common.util.SystemUtil.runShellCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.editorMatcher;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectBindInput;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ClipDescription;
import android.net.Uri;
import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputContentInfo;
import android.view.inputmethod.cts.util.EndToEndImeTestBase;
import android.view.inputmethod.cts.util.TestActivity;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.mockime.ImeCommand;
import com.android.cts.mockime.ImeEvent;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Function;

/**
 * Ensures that blocking APIs in {@link InputConnection} are working as expected.
 */
@LargeTest
@RunWith(AndroidJUnit4.class)
public class InputConnectionBlockingMethodTest extends EndToEndImeTestBase {
    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);
    private static final long LONG_TIMEOUT = TimeUnit.SECONDS.toMillis(30);
    private static final long IMMEDIATE_TIMEOUT_NANO = TimeUnit.MILLISECONDS.toNanos(200);

    private static final String TEST_MARKER_PREFIX =
            "android.view.inputmethod.cts.InputConnectionBlockingMethodTest";

    private static String getTestMarker() {
        return TEST_MARKER_PREFIX + "/"  + SystemClock.elapsedRealtimeNanos();
    }

    /**
     * A utility method to verify a method is called within a certain timeout period then block
     * it by {@link BlockingMethodVerifier#close()} is called.
     */
    private static final class BlockingMethodVerifier implements AutoCloseable {
        private final CountDownLatch mWaitUntilMethodCalled = new CountDownLatch(1);
        private final CountDownLatch mWaitUntilTestFinished = new CountDownLatch(1);

        /**
         * Used to notify when a method to be tested is called.
         */
        void onMethodCalled() {
            try {
                mWaitUntilMethodCalled.countDown();
                mWaitUntilTestFinished.await();
            } catch (InterruptedException e) {
            }
        }

        /**
         * Ensures that the method to be tested is called within {@param timeout}.
         *
         * @param message Message to be shown when the method is not called despite the expectation.
         * @param timeout Timeout in milliseconds.
         */
        void expectMethodCalled(@NonNull String message, long timeout) {
            try {
                assertTrue(message, mWaitUntilMethodCalled.await(timeout, TimeUnit.MILLISECONDS));
            } catch (InterruptedException e) {
                fail(message + e);
            }
        }

        /**
         * Unblock the method to be tested to avoid the test from being blocked forever.
         */
        @Override
        public void close() throws Exception {
            mWaitUntilTestFinished.countDown();
        }
    }

    /**
     * A test procedure definition for
     * {@link #testInputConnection(Function, TestProcedure, AutoCloseable)}.
     */
    @FunctionalInterface
    interface TestProcedure {
        /**
         * The test body of {@link #testInputConnection(Function, TestProcedure, AutoCloseable)}
         *
         * @param session {@link MockImeSession} to be used during this test.
         * @param stream {@link ImeEventStream} associated with {@code session}.
         */
        void run(@NonNull MockImeSession session, @NonNull ImeEventStream stream) throws Exception;
    }

    /**
     * Tries to trigger {@link com.android.cts.mockime.MockIme#onUnbindInput()} by showing the
     * Launcher.
     */
    private void triggerUnbindInput() {
        // Note: We hope showing the launcher is sufficient to trigger onUnbindInput() in MockIme,
        // but if it turns out to be not sufficient, consider launching a different Activity in a
        // separate process.
        runShellCommand("input keyevent KEYCODE_HOME");
    }

    /**
     * A utility method to run a unit test for {@link InputConnection}.
     *
     * <p>This utility method enables you to avoid boilerplate code when writing unit tests for
     * {@link InputConnection}.</p>
     *
     * @param inputConnectionWrapperProvider {@link Function} to install custom hooks to the
     *                                       original {@link InputConnection}.
     * @param testProcedure Test body.
     */
    private void testInputConnection(
            Function<InputConnection, InputConnection> inputConnectionWrapperProvider,
            TestProcedure testProcedure) throws Exception {
        testInputConnection(inputConnectionWrapperProvider, testProcedure, null);
    }

    /**
     * A utility method to run a unit test for {@link InputConnection}.
     *
     * <p>This utility method enables you to avoid boilerplate code when writing unit tests for
     * {@link InputConnection}.</p>
     *
     * @param inputConnectionWrapperProvider {@link Function} to install custom hooks to the
     *                                       original {@link InputConnection}.
     * @param testProcedure Test body.
     * @param closeable {@link AutoCloseable} object to be cleaned up after running test.
     */
    private void testInputConnection(
            Function<InputConnection, InputConnection> inputConnectionWrapperProvider,
            TestProcedure testProcedure, @Nullable AutoCloseable closeable) throws Exception {
        try (AutoCloseable closeableHolder = closeable;
             MockImeSession imeSession = MockImeSession.create(
                     InstrumentationRegistry.getInstrumentation().getContext(),
                     InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                     new ImeSettings.Builder())) {
            final AtomicBoolean isTestRunning = new AtomicBoolean(true);
            try {
                final ImeEventStream stream = imeSession.openEventStream();

                final String marker = getTestMarker();
                TestActivity.startSync(activity -> {
                    final LinearLayout layout = new LinearLayout(activity);
                    layout.setOrientation(LinearLayout.VERTICAL);
                    final EditText editText = new EditText(activity) {
                        @Override
                        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
                            final InputConnection ic = super.onCreateInputConnection(outAttrs);
                            // Fall back to the original InputConnection once the test is done.
                            return isTestRunning.get()
                                    ? inputConnectionWrapperProvider.apply(ic) : ic;
                        }
                    };
                    editText.setPrivateImeOptions(marker);
                    editText.setHint("editText");
                    editText.requestFocus();

                    layout.addView(editText);
                    activity.getWindow().setSoftInputMode(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                    return layout;
                });

                // Wait until the MockIme gets bound to the TestActivity.
                expectBindInput(stream, Process.myPid(), TIMEOUT);

                // Wait until "onStartInput" gets called for the EditText.
                expectEvent(stream, editorMatcher("onStartInput", marker), TIMEOUT);

                testProcedure.run(imeSession, stream);
            } finally {
                isTestRunning.set(false);
            }
        }
    }

    /**
     * Ensures that {@code event}'s elapse time is less than the given threshold.
     *
     * @param event {@link ImeEvent} to be tested.
     * @param elapseNanoTimeThreshold threshold in nano sec.
     */
    private static void expectElapseTimeLessThan(@NonNull ImeEvent event,
            long elapseNanoTimeThreshold) {
        final long elapseNanoTime = event.getExitTimestamp() - event.getEnterTimestamp();
        if (elapseNanoTime > elapseNanoTimeThreshold) {
            fail(event.getEventName() + " took " + elapseNanoTime + " nsec,"
                    + " which must be less than" + elapseNanoTimeThreshold + " nsec.");
        }
    }

    /**
     * Test {@link InputConnection#getTextAfterCursor(int, int)} works as expected.
     */
    @Test
    public void testGetTextAfterCursor() throws Exception {
        final int expectedN = 3;
        final int expectedFlags = InputConnection.GET_TEXT_WITH_STYLES;
        final String expectedResult = "89";

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public CharSequence getTextAfterCursor(int n, int flags) {
                assertEquals(expectedN, n);
                assertEquals(expectedFlags, flags);
                return expectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callGetTextAfterCursor(expectedN, expectedFlags);
            final CharSequence result =
                    expectCommand(stream, command, TIMEOUT).getReturnCharSequenceValue();
            assertEquals(expectedResult, result);
        });
    }

    /**
     * Test {@link InputConnection#getTextAfterCursor(int, int)} fails after a system-defined
     * time-out even if the target app does not respond.
     */
    @Test
    public void testGetTextAfterCursorFailWithTimeout() throws Exception {
        final String unexpectedResult = "89";
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public CharSequence getTextAfterCursor(int n, int flags) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callGetTextAfterCursor(
                    unexpectedResult.length(), InputConnection.GET_TEXT_WITH_STYLES);
            blocker.expectMethodCalled("IC#getTextAfterCursor() must be called back", TIMEOUT);
            final ImeEvent result = expectCommand(stream, command, LONG_TIMEOUT);
            assertTrue("When timeout happens, IC#getTextAfterCursor() returns null",
                    result.isNullReturnValue());
        }, blocker);
    }

    /**
     * Test {@link InputConnection#getTextAfterCursor(int, int)} fail-fasts once unbindInput() is
     * issued.
     */
    @Test
    public void testGetTextAfterCursorFailFastAfterUnbindInput() throws Exception {
        final String unexpectedResult = "89";
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public CharSequence getTextAfterCursor(int n, int flags) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            // Schedule to call IC#getTextAfterCursor() twice.
            final ImeCommand command1 = session.callGetTextAfterCursor(
                    unexpectedResult.length(), InputConnection.GET_TEXT_WITH_STYLES);
            final ImeCommand command2 = session.callGetTextAfterCursor(
                    unexpectedResult.length(), InputConnection.GET_TEXT_WITH_STYLES);

            blocker.expectMethodCalled("IC#getTextAfterCursor() must be called back", TIMEOUT);
            triggerUnbindInput();
            final ImeEvent result1 = expectCommand(stream, command1, TIMEOUT);
            assertTrue("When unbindInput() happens, IC#getTextAfterCursor() returns null",
                    result1.isNullReturnValue());

            final ImeEvent result2 = expectCommand(stream, command2, TIMEOUT);
            assertTrue("Once unbindInput() happened, IC#getTextAfterCursor() returns null",
                    result2.isNullReturnValue());
            expectElapseTimeLessThan(result2, IMMEDIATE_TIMEOUT_NANO);
        }, blocker);
    }

    /**
     * Test {@link InputConnection#getTextBeforeCursor(int, int)} works as expected.
     */
    @Test
    public void testGetTextBeforeCursor() throws Exception {
        final int expectedN = 3;
        final int expectedFlags = InputConnection.GET_TEXT_WITH_STYLES;
        final String expectedResult = "123";

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public CharSequence getTextBeforeCursor(int n, int flags) {
                assertEquals(expectedN, n);
                assertEquals(expectedFlags, flags);
                return expectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callGetTextBeforeCursor(expectedN, expectedFlags);
            final CharSequence result =
                    expectCommand(stream, command, TIMEOUT).getReturnCharSequenceValue();
            assertEquals(expectedResult, result);
        });
    }

    /**
     * Test {@link InputConnection#getTextBeforeCursor(int, int)} fails after a system-defined
     * time-out even if the target app does not respond.
     */
    @Test
    public void testGetTextBeforeCursorFailWithTimeout() throws Exception {
        final String unexpectedResult = "123";
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public CharSequence getTextBeforeCursor(int n, int flags) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callGetTextBeforeCursor(
                    unexpectedResult.length(), InputConnection.GET_TEXT_WITH_STYLES);
            blocker.expectMethodCalled("IC#getTextBeforeCursor() must be called back", TIMEOUT);
            final ImeEvent result = expectCommand(stream, command, LONG_TIMEOUT);
            assertTrue("When timeout happens, IC#getTextBeforeCursor() returns null",
                    result.isNullReturnValue());
        }, blocker);
    }

    /**
     * Test {@link InputConnection#getTextBeforeCursor(int, int)} fail-fasts once unbindInput() is
     * issued.
     */
    @Test
    public void testGetTextBeforeCursorFailFastAfterUnbindInput() throws Exception {
        final String unexpectedResult = "123";
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public CharSequence getTextBeforeCursor(int n, int flags) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            // Schedule to call IC#getTextBeforeCursor() twice.
            final ImeCommand command1 = session.callGetTextBeforeCursor(
                    unexpectedResult.length(), InputConnection.GET_TEXT_WITH_STYLES);
            final ImeCommand command2 = session.callGetTextBeforeCursor(
                    unexpectedResult.length(), InputConnection.GET_TEXT_WITH_STYLES);

            blocker.expectMethodCalled("IC#getTextBeforeCursor() must be called back", TIMEOUT);

            triggerUnbindInput();
            final ImeEvent result1 = expectCommand(stream, command1, TIMEOUT);
            assertTrue("When unbindInput() happens, IC#getTextBeforeCursor() returns null",
                    result1.isNullReturnValue());

            final ImeEvent result2 = expectCommand(stream, command2, TIMEOUT);
            assertTrue("Once unbindInput() happened, IC#getTextBeforeCursor() returns null",
                    result2.isNullReturnValue());
            expectElapseTimeLessThan(result2, IMMEDIATE_TIMEOUT_NANO);
        }, blocker);
    }

    /**
     * Test {@link InputConnection#getSelectedText(int)} works as expected.
     */
    @Test
    public void testGetSelectedText() throws Exception {
        final int expectedFlags = InputConnection.GET_TEXT_WITH_STYLES;
        final String expectedResult = "4567";

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public CharSequence getSelectedText(int flags) {
                assertEquals(expectedFlags, flags);
                return expectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callGetSelectedText(expectedFlags);
            final CharSequence result =
                    expectCommand(stream, command, TIMEOUT).getReturnCharSequenceValue();
            assertEquals(expectedResult, result);
        });
    }

    /**
     * Test {@link InputConnection#getSelectedText(int)} fails after a system-defined time-out even
     * if the target app does not respond.
     */
    @Test
    public void testGetSelectedTextFailWithTimeout() throws Exception {
        final String unexpectedResult = "4567";
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public CharSequence getSelectedText(int flags) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command =
                    session.callGetSelectedText(InputConnection.GET_TEXT_WITH_STYLES);
            blocker.expectMethodCalled("IC#getSelectedText() must be called back", TIMEOUT);
            final ImeEvent result = expectCommand(stream, command, LONG_TIMEOUT);
            assertTrue("When timeout happens, IC#getSelectedText() returns null",
                    result.isNullReturnValue());
        }, blocker);
    }

    /**
     * Test {@link InputConnection#getSelectedText(int)} fail-fasts once unbindInput() is issued.
     */
    @Test
    public void testGetSelectedTextFailFastAfterUnbindInput() throws Exception {
        final String unexpectedResult = "4567";
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public CharSequence getSelectedText(int flags) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            // Schedule to call IC#getSelectedText() twice.
            final ImeCommand firstCommand =
                    session.callGetSelectedText(InputConnection.GET_TEXT_WITH_STYLES);
            final ImeCommand secondCommand =
                    session.callGetSelectedText(InputConnection.GET_TEXT_WITH_STYLES);

            blocker.expectMethodCalled("IC#getSelectedText() must be called back", TIMEOUT);
            triggerUnbindInput();
            final ImeEvent firstResult = expectCommand(stream, firstCommand, TIMEOUT);
            assertTrue("When unbindInput() happens, IC#getSelectedText() returns null",
                    firstResult.isNullReturnValue());

            final ImeEvent secondResult = expectCommand(stream, secondCommand, TIMEOUT);
            assertTrue("Once unbindInput() happened, IC#getSelectedText() returns null",
                    secondResult.isNullReturnValue());
            expectElapseTimeLessThan(secondResult, IMMEDIATE_TIMEOUT_NANO);
        }, blocker);
    }

    /**
     * Test {@link InputConnection#getCursorCapsMode(int)} works as expected.
     */
    @Test
    public void testGetCursorCapsMode() throws Exception {
        final int expectedResult = EditorInfo.TYPE_TEXT_FLAG_CAP_SENTENCES;
        final int expectedReqMode = TextUtils.CAP_MODE_SENTENCES | TextUtils.CAP_MODE_CHARACTERS
                | TextUtils.CAP_MODE_WORDS;

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public int getCursorCapsMode(int reqModes) {
                assertEquals(expectedReqMode, reqModes);
                return expectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callGetCursorCapsMode(expectedReqMode);
            final int result = expectCommand(stream, command, TIMEOUT).getReturnIntegerValue();
            assertEquals(expectedResult, result);
        });
    }

    /**
     * Test {@link InputConnection#getCursorCapsMode(int)} fails after a system-defined time-out
     * even if the target app does not respond.
     */
    @Test
    public void testGetCursorCapsModeFailWithTimeout() throws Exception {
        final int unexpectedResult = EditorInfo.TYPE_TEXT_FLAG_CAP_WORDS;
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public int getCursorCapsMode(int reqModes) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callGetCursorCapsMode(TextUtils.CAP_MODE_WORDS);
            blocker.expectMethodCalled("IC#getCursorCapsMode() must be called back", TIMEOUT);
            final ImeEvent result = expectCommand(stream, command, LONG_TIMEOUT);
            assertEquals("When timeout happens, IC#getCursorCapsMode() returns 0",
                    0, result.getReturnIntegerValue());
        }, blocker);
    }

    /**
     * Test {@link InputConnection#getCursorCapsMode(int)} fail-fasts once unbindInput() is issued.
     */
    @Test
    public void testGetCursorCapsModeFailFastAfterUnbindInput() throws Exception {
        final int unexpectedResult = EditorInfo.TYPE_TEXT_FLAG_CAP_WORDS;
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public int getCursorCapsMode(int reqModes) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            // Schedule to call IC#getCursorCapsMode() twice.
            final ImeCommand firstCommand =
                    session.callGetCursorCapsMode(TextUtils.CAP_MODE_WORDS);
            final ImeCommand secondCommand =
                    session.callGetCursorCapsMode(TextUtils.CAP_MODE_WORDS);

            blocker.expectMethodCalled("IC#getCursorCapsMode() must be called back", TIMEOUT);
            triggerUnbindInput();
            final ImeEvent firstResult = expectCommand(stream, firstCommand, TIMEOUT);
            assertEquals("When unbindInput() happens, IC#getCursorCapsMode() returns 0",
                    0, firstResult.getReturnIntegerValue());

            final ImeEvent secondResult = expectCommand(stream, secondCommand, TIMEOUT);
            assertEquals("Once unbindInput() happened, IC#getCursorCapsMode() returns 0",
                    0, secondResult.getReturnIntegerValue());
            expectElapseTimeLessThan(secondResult, IMMEDIATE_TIMEOUT_NANO);
        }, blocker);
    }

    /**
     * Test {@link InputConnection#getExtractedText(ExtractedTextRequest, int)} works as expected.
     */
    @Test
    public void testGetExtractedText() throws Exception {
        final ExtractedTextRequest expectedRequest = ExtractedTextRequestTest.createForTest();
        final int expectedFlags = InputConnection.GET_EXTRACTED_TEXT_MONITOR;
        final ExtractedText expectedResult = ExtractedTextTest.createForTest();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
                assertEquals(expectedFlags, flags);
                ExtractedTextRequestTest.assertTestInstance(request);
                return expectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callGetExtractedText(expectedRequest, expectedFlags);
            final ExtractedText result =
                    expectCommand(stream, command, TIMEOUT).getReturnParcelableValue();
            ExtractedTextTest.assertTestInstance(result);
        });
    }

    /**
     * Test {@link InputConnection#getExtractedText(ExtractedTextRequest, int)} fails after a
     * system-defined time-out even if the target app does not respond.
     */
    @Test
    public void testGetExtractedTextFailWithTimeout() throws Exception {
        final ExtractedText unexpectedResult = ExtractedTextTest.createForTest();
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callGetExtractedText(
                    ExtractedTextRequestTest.createForTest(),
                    InputConnection.GET_EXTRACTED_TEXT_MONITOR);
            blocker.expectMethodCalled("IC#getExtractedText() must be called back", TIMEOUT);
            final ImeEvent result = expectCommand(stream, command, LONG_TIMEOUT);
            assertTrue("When timeout happens, IC#getExtractedText() returns null",
                    result.isNullReturnValue());
        }, blocker);
    }

    /**
     * Test {@link InputConnection#getExtractedText(ExtractedTextRequest, int)} fail-fasts once
     * unbindInput() is issued.
     */
    @Test
    public void testGetExtractedTextFailFastAfterUnbindInput() throws Exception {
        final ExtractedText unexpectedResult = ExtractedTextTest.createForTest();
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            // Schedule to call IC#getExtractedText() twice.
            final ImeCommand firstCommand = session.callGetExtractedText(
                    ExtractedTextRequestTest.createForTest(),
                    InputConnection.GET_EXTRACTED_TEXT_MONITOR);
            final ImeCommand secondCommand = session.callGetExtractedText(
                    ExtractedTextRequestTest.createForTest(),
                    InputConnection.GET_EXTRACTED_TEXT_MONITOR);

            blocker.expectMethodCalled("IC#getExtractedText() must be called back", TIMEOUT);
            triggerUnbindInput();
            final ImeEvent firstResult = expectCommand(stream, firstCommand, TIMEOUT);
            assertTrue("When unbindInput() happens, IC#getExtractedText() returns 0",
                    firstResult.isNullReturnValue());

            final ImeEvent secondResult = expectCommand(stream, secondCommand, TIMEOUT);
            assertTrue("Once unbindInput() happened, IC#getExtractedText() returns 0",
                    firstResult.isNullReturnValue());
            expectElapseTimeLessThan(secondResult, IMMEDIATE_TIMEOUT_NANO);
        }, blocker);
    }

    /**
     * Test {@link InputConnection#requestCursorUpdates(int)} works as expected.
     */
    @Test
    public void testRequestCursorUpdates() throws Exception {
        final int expectedFlags = InputConnection.CURSOR_UPDATE_IMMEDIATE;
        final boolean expectedResult = true;

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public boolean requestCursorUpdates(int cursorUpdateMode) {
                assertEquals(expectedFlags, cursorUpdateMode);
                return expectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callRequestCursorUpdates(expectedFlags);
            assertTrue(expectCommand(stream, command, TIMEOUT).getReturnBooleanValue());
        });
    }

    /**
     * Test {@link InputConnection#requestCursorUpdates(int)} fails after a system-defined time-out
     * even if the target app does not respond.
     */
    @Test
    public void testRequestCursorUpdatesFailWithTimeout() throws Exception {
        final boolean unexpectedResult = true;
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public boolean requestCursorUpdates(int cursorUpdateMode) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callRequestCursorUpdates(
                    InputConnection.CURSOR_UPDATE_IMMEDIATE);
            blocker.expectMethodCalled("IC#requestCursorUpdates() must be called back", TIMEOUT);
            final ImeEvent result = expectCommand(stream, command, LONG_TIMEOUT);
            assertFalse("When timeout happens, IC#requestCursorUpdates() returns false",
                    result.getReturnBooleanValue());
        }, blocker);
    }

    /**
     * Test {@link InputConnection#requestCursorUpdates(int)} fail-fasts once unbindInput() is
     * issued.
     */
    @Test
    public void testRequestCursorUpdatesFailFastAfterUnbindInput() throws Exception {
        final boolean unexpectedResult = true;
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public boolean requestCursorUpdates(int cursorUpdateMode) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            // Schedule to call IC#requestCursorUpdates() twice.
            final ImeCommand firstCommand = session.callRequestCursorUpdates(
                    InputConnection.CURSOR_UPDATE_IMMEDIATE);
            final ImeCommand secondCommand = session.callRequestCursorUpdates(
                    InputConnection.CURSOR_UPDATE_IMMEDIATE);

            blocker.expectMethodCalled("IC#requestCursorUpdates() must be called back", TIMEOUT);
            triggerUnbindInput();
            final ImeEvent firstResult = expectCommand(stream, firstCommand, TIMEOUT);
            assertFalse("When unbindInput() happens, IC#requestCursorUpdates() returns false",
                    firstResult.getReturnBooleanValue());

            final ImeEvent secondResult = expectCommand(stream, secondCommand, TIMEOUT);
            assertFalse("Once unbindInput() happened, IC#requestCursorUpdates() returns false",
                    firstResult.getReturnBooleanValue());
            expectElapseTimeLessThan(secondResult, IMMEDIATE_TIMEOUT_NANO);
        }, blocker);
    }

    /**
     * Test {@link InputConnection#commitContent(InputContentInfo, int, Bundle)} works as expected.
     */
    @Test
    public void testCommitContent() throws Exception {
        final InputContentInfo expectedInputContentInfo = new InputContentInfo(
                Uri.parse("content://com.example/path"),
                new ClipDescription("sample content", new String[]{"image/png"}),
                Uri.parse("https://example.com"));
        final Bundle expectedOpt = new Bundle();
        final String expectedOptKey = "testKey";
        final int expectedOptValue = 42;
        expectedOpt.putInt(expectedOptKey, expectedOptValue);
        final int expectedFlags = InputConnection.INPUT_CONTENT_GRANT_READ_URI_PERMISSION;
        final boolean expectedResult = true;

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public boolean commitContent(InputContentInfo inputContentInfo, int flags,
                    Bundle opts) {
                assertEquals(expectedInputContentInfo.getContentUri(),
                        inputContentInfo.getContentUri());
                assertEquals(expectedFlags, flags);
                assertEquals(expectedOpt.getInt(expectedOptKey), opts.getInt(expectedOptKey));
                return expectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command =
                    session.callCommitContent(expectedInputContentInfo, expectedFlags, expectedOpt);
            assertTrue(expectCommand(stream, command, TIMEOUT).getReturnBooleanValue());
        });
    }

    /**
     * Test {@link InputConnection#commitContent(InputContentInfo, int, Bundle)} fails after a
     * system-defined time-out even if the target app does not respond.
     */
    @Test
    public void testCommitContentFailWithTimeout() throws Exception {
        final boolean unexpectedResult = true;
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public boolean commitContent(InputContentInfo inputContentInfo, int flags,
                    Bundle opts) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            final ImeCommand command = session.callCommitContent(
                    new InputContentInfo(Uri.parse("content://com.example/path"),
                            new ClipDescription("sample content", new String[]{"image/png"}),
                            Uri.parse("https://example.com")), 0, null);
            blocker.expectMethodCalled("IC#commitContent() must be called back", TIMEOUT);
            final ImeEvent result = expectCommand(stream, command, LONG_TIMEOUT);
            assertFalse("When timeout happens, IC#commitContent() returns false",
                    result.getReturnBooleanValue());
        }, blocker);
    }

    /**
     * Test {@link InputConnection#commitContent(InputContentInfo, int, Bundle)} fail-fasts once
     * unbindInput() is issued.
     */
    @Test
    public void testCommitContentFailFastAfterUnbindInput() throws Exception {
        final boolean unexpectedResult = true;
        final BlockingMethodVerifier blocker = new BlockingMethodVerifier();

        final class Wrapper extends InputConnectionWrapper {
            private Wrapper(InputConnection target) {
                super(target, false);
            }

            @Override
            public boolean commitContent(InputContentInfo inputContentInfo, int flags,
                    Bundle opts) {
                blocker.onMethodCalled();
                return unexpectedResult;
            }
        }

        testInputConnection(Wrapper::new, (MockImeSession session, ImeEventStream stream) -> {
            // Schedule to call IC#commitContent() twice.
            final ImeCommand firstCommand = session.callCommitContent(
                    new InputContentInfo(Uri.parse("content://com.example/path"),
                            new ClipDescription("sample content", new String[]{"image/png"}),
                            Uri.parse("https://example.com")), 0, null);
            final ImeCommand secondCommand = session.callCommitContent(
                    new InputContentInfo(Uri.parse("content://com.example/path"),
                            new ClipDescription("sample content", new String[]{"image/png"}),
                            Uri.parse("https://example.com")), 0, null);

            blocker.expectMethodCalled("IC#commitContent() must be called back", TIMEOUT);
            triggerUnbindInput();
            final ImeEvent firstResult = expectCommand(stream, firstCommand, TIMEOUT);
            assertFalse("When unbindInput() happens, IC#commitContent() returns false",
                    firstResult.getReturnBooleanValue());

            final ImeEvent secondResult = expectCommand(stream, secondCommand, TIMEOUT);
            assertFalse("Once unbindInput() happened, IC#commitContent() returns false",
                    firstResult.getReturnBooleanValue());
            expectElapseTimeLessThan(secondResult, IMMEDIATE_TIMEOUT_NANO);
        }, blocker);
    }
}
