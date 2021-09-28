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

package com.android.cts.mockime;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.app.UiAutomation;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputContentInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.AssumptionViolatedException;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

/**
 * Represents an active Mock IME session, which provides basic primitives to write end-to-end tests
 * for IME APIs.
 *
 * <p>To use {@link MockIme} via {@link MockImeSession}, you need to </p>
 * <p>Public methods are not thread-safe.</p>
 */
public class MockImeSession implements AutoCloseable {
    private final String mImeEventActionName =
            "com.android.cts.mockime.action.IME_EVENT." + SystemClock.elapsedRealtimeNanos();

    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(10);

    @NonNull
    private final Context mContext;
    @NonNull
    private final UiAutomation mUiAutomation;

    private final HandlerThread mHandlerThread = new HandlerThread("EventReceiver");

    private static final class EventStore {
        private static final int INITIAL_ARRAY_SIZE = 32;

        @NonNull
        public final ImeEvent[] mArray;
        public int mLength;

        EventStore() {
            mArray = new ImeEvent[INITIAL_ARRAY_SIZE];
            mLength = 0;
        }

        EventStore(EventStore src, int newLength) {
            mArray = new ImeEvent[newLength];
            mLength = src.mLength;
            System.arraycopy(src.mArray, 0, mArray, 0, src.mLength);
        }

        public EventStore add(ImeEvent event) {
            if (mLength + 1 <= mArray.length) {
                mArray[mLength] = event;
                ++mLength;
                return this;
            } else {
                return new EventStore(this, mLength * 2).add(event);
            }
        }

        public ImeEventStream.ImeEventArray takeSnapshot() {
            return new ImeEventStream.ImeEventArray(mArray, mLength);
        }
    }

    private static final class MockImeEventReceiver extends BroadcastReceiver {
        private final Object mLock = new Object();

        @GuardedBy("mLock")
        @NonNull
        private EventStore mCurrentEventStore = new EventStore();

        @NonNull
        private final String mActionName;

        MockImeEventReceiver(@NonNull String actionName) {
            mActionName = actionName;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (TextUtils.equals(mActionName, intent.getAction())) {
                synchronized (mLock) {
                    mCurrentEventStore =
                            mCurrentEventStore.add(ImeEvent.fromBundle(intent.getExtras()));
                }
            }
        }

        public ImeEventStream.ImeEventArray takeEventSnapshot() {
            synchronized (mLock) {
                return mCurrentEventStore.takeSnapshot();
            }
        }
    }
    private final MockImeEventReceiver mEventReceiver =
            new MockImeEventReceiver(mImeEventActionName);

    private final ImeEventStream mEventStream =
            new ImeEventStream(mEventReceiver::takeEventSnapshot);

    private static String executeShellCommand(
            @NonNull UiAutomation uiAutomation, @NonNull String command) throws IOException {
        try (ParcelFileDescriptor.AutoCloseInputStream in =
                     new ParcelFileDescriptor.AutoCloseInputStream(
                             uiAutomation.executeShellCommand(command))) {
            final StringBuilder sb = new StringBuilder();
            final byte[] buffer = new byte[4096];
            while (true) {
                final int numRead = in.read(buffer);
                if (numRead <= 0) {
                    break;
                }
                sb.append(new String(buffer, 0, numRead));
            }
            return sb.toString();
        }
    }

    @Nullable
    private String getCurrentInputMethodId() {
        // TODO: Replace this with IMM#getCurrentInputMethodIdForTesting()
        return Settings.Secure.getString(mContext.getContentResolver(),
                Settings.Secure.DEFAULT_INPUT_METHOD);
    }

    @Nullable
    private static void writeMockImeSettings(@NonNull Context context,
            @NonNull String imeEventActionName,
            @Nullable ImeSettings.Builder imeSettings) throws Exception {
        final Bundle bundle = ImeSettings.serializeToBundle(imeEventActionName, imeSettings);
        context.getContentResolver().call(SettingsProvider.AUTHORITY, "write", null, bundle);
    }

    private ComponentName getMockImeComponentName() {
        return MockIme.getComponentName();
    }

    private String getMockImeId() {
        return MockIme.getImeId();
    }

    private MockImeSession(@NonNull Context context, @NonNull UiAutomation uiAutomation) {
        mContext = context;
        mUiAutomation = uiAutomation;
    }

    private void initialize(@Nullable ImeSettings.Builder imeSettings) throws Exception {
        // Make sure that MockIME is not selected.
        if (mContext.getSystemService(InputMethodManager.class)
                .getInputMethodList()
                .stream()
                .anyMatch(info -> getMockImeComponentName().equals(info.getComponent()))) {
            executeShellCommand(mUiAutomation, "ime reset");
        }
        if (mContext.getSystemService(InputMethodManager.class)
                .getEnabledInputMethodList()
                .stream()
                .anyMatch(info -> getMockImeComponentName().equals(info.getComponent()))) {
            throw new IllegalStateException();
        }

        writeMockImeSettings(mContext, mImeEventActionName, imeSettings);

        mHandlerThread.start();
        mContext.registerReceiver(mEventReceiver,
                new IntentFilter(mImeEventActionName), null /* broadcastPermission */,
                new Handler(mHandlerThread.getLooper()));

        executeShellCommand(mUiAutomation, "ime enable " + getMockImeId());
        executeShellCommand(mUiAutomation, "ime set " + getMockImeId());

        PollingCheck.check("Make sure that MockIME becomes available", TIMEOUT,
                () -> getMockImeId().equals(getCurrentInputMethodId()));
    }

    /** @see #create(Context, UiAutomation, ImeSettings.Builder) */
    @NonNull
    public static MockImeSession create(@NonNull Context context) throws Exception {
        return create(context, getInstrumentation().getUiAutomation(), new ImeSettings.Builder());
    }

    /**
     * Creates a new Mock IME session. During this session, you can receive various events from
     * {@link MockIme}.
     *
     * @param context {@link Context} to be used to receive inter-process events from the
     *                {@link MockIme} (e.g. via {@link BroadcastReceiver}
     * @param uiAutomation {@link UiAutomation} object to change the device state that are typically
     *                     guarded by permissions.
     * @param imeSettings Key-value pairs to be passed to the {@link MockIme}.
     * @return A session object, with which you can retrieve event logs from the {@link MockIme} and
     *         can clean up the session.
     */
    @NonNull
    public static MockImeSession create(
            @NonNull Context context,
            @NonNull UiAutomation uiAutomation,
            @Nullable ImeSettings.Builder imeSettings) throws Exception {
        final String unavailabilityReason = getUnavailabilityReason(context);
        if (unavailabilityReason != null) {
            throw new AssumptionViolatedException(unavailabilityReason);
        }

        final MockImeSession client = new MockImeSession(context, uiAutomation);
        client.initialize(imeSettings);
        return client;
    }

    /**
     * Checks if the {@link MockIme} can be used in this device.
     *
     * @return {@code null} if it can be used, or message describing why if it cannot.
     */
    @Nullable
    public static String getUnavailabilityReason(@NonNull Context context) {
        if (!context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_INPUT_METHODS)) {
            return "Device must support installable IMEs that implement InputMethodService API";
        }
        return null;
    }

    /**
     * @return {@link ImeEventStream} object that stores events sent from {@link MockIme} since the
     *         session is created.
     */
    public ImeEventStream openEventStream() {
        return mEventStream.copy();
    }

    /**
     * Closes the active session and de-selects {@link MockIme}. Currently which IME will be
     * selected next is up to the system.
     */
    public void close() throws Exception {
        executeShellCommand(mUiAutomation, "ime reset");

        PollingCheck.check("Make sure that MockIME becomes unavailable", TIMEOUT, () ->
                mContext.getSystemService(InputMethodManager.class)
                        .getEnabledInputMethodList()
                        .stream()
                        .noneMatch(info -> getMockImeComponentName().equals(info.getComponent())));

        mContext.unregisterReceiver(mEventReceiver);
        mHandlerThread.quitSafely();
        mContext.getContentResolver().call(SettingsProvider.AUTHORITY, "delete", null, null);
    }

    /**
     * Common logic to send a special command to {@link MockIme}.
     *
     * @param commandName command to be passed to {@link MockIme}
     * @param params {@link Bundle} to be passed to {@link MockIme} as a parameter set of
     *               {@code commandName}
     * @return {@link ImeCommand} that is sent to {@link MockIme}.  It can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    private ImeCommand callCommandInternal(@NonNull String commandName, @NonNull Bundle params) {
        final ImeCommand command = new ImeCommand(
                commandName, SystemClock.elapsedRealtimeNanos(), true, params);
        final Intent intent = new Intent();
        intent.setPackage(MockIme.getComponentName().getPackageName());
        intent.setAction(MockIme.getCommandActionName(mImeEventActionName));
        intent.putExtras(command.toBundle());
        mContext.sendBroadcast(intent);
        return command;
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#getTextBeforeCursor(int, int)} with the
     * given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().getTextBeforeCursor(n, flag)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnCharSequenceValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param n to be passed as the {@code n} parameter.
     * @param flag to be passed as the {@code flag} parameter.
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callGetTextBeforeCursor(int n, int flag) {
        final Bundle params = new Bundle();
        params.putInt("n", n);
        params.putInt("flag", flag);
        return callCommandInternal("getTextBeforeCursor", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#getTextAfterCursor(int, int)} with the
     * given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().getTextAfterCursor(n, flag)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnCharSequenceValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param n to be passed as the {@code n} parameter.
     * @param flag to be passed as the {@code flag} parameter.
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callGetTextAfterCursor(int n, int flag) {
        final Bundle params = new Bundle();
        params.putInt("n", n);
        params.putInt("flag", flag);
        return callCommandInternal("getTextAfterCursor", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#getSelectedText(int)} with the
     * given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().getSelectedText(flag)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnCharSequenceValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param flag to be passed as the {@code flag} parameter.
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callGetSelectedText(int flag) {
        final Bundle params = new Bundle();
        params.putInt("flag", flag);
        return callCommandInternal("getSelectedText", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#getCursorCapsMode(int)} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().getCursorCapsMode(reqModes)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnIntegerValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param reqModes to be passed as the {@code reqModes} parameter.
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callGetCursorCapsMode(int reqModes) {
        final Bundle params = new Bundle();
        params.putInt("reqModes", reqModes);
        return callCommandInternal("getCursorCapsMode", params);
    }

    /**
     * Lets {@link MockIme} to call
     * {@link InputConnection#getExtractedText(ExtractedTextRequest, int)} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().getExtractedText(request, flags)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnParcelableValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param request to be passed as the {@code request} parameter
     * @param flags to be passed as the {@code flags} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callGetExtractedText(@Nullable ExtractedTextRequest request, int flags) {
        final Bundle params = new Bundle();
        params.putParcelable("request", request);
        params.putInt("flags", flags);
        return callCommandInternal("getExtractedText", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#deleteSurroundingText(int, int)} with the
     * given parameters.
     *
     * <p>This triggers
     * {@code getCurrentInputConnection().deleteSurroundingText(beforeLength, afterLength)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param beforeLength to be passed as the {@code beforeLength} parameter
     * @param afterLength to be passed as the {@code afterLength} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callDeleteSurroundingText(int beforeLength, int afterLength) {
        final Bundle params = new Bundle();
        params.putInt("beforeLength", beforeLength);
        params.putInt("afterLength", afterLength);
        return callCommandInternal("deleteSurroundingText", params);
    }

    /**
     * Lets {@link MockIme} to call
     * {@link InputConnection#deleteSurroundingTextInCodePoints(int, int)} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().deleteSurroundingTextInCodePoints(
     * beforeLength, afterLength)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param beforeLength to be passed as the {@code beforeLength} parameter
     * @param afterLength to be passed as the {@code afterLength} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callDeleteSurroundingTextInCodePoints(int beforeLength, int afterLength) {
        final Bundle params = new Bundle();
        params.putInt("beforeLength", beforeLength);
        params.putInt("afterLength", afterLength);
        return callCommandInternal("deleteSurroundingTextInCodePoints", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#setComposingText(CharSequence, int)} with
     * the given parameters.
     *
     * <p>This triggers
     * {@code getCurrentInputConnection().setComposingText(text, newCursorPosition)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param text to be passed as the {@code text} parameter
     * @param newCursorPosition to be passed as the {@code newCursorPosition} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callSetComposingText(@Nullable CharSequence text, int newCursorPosition) {
        final Bundle params = new Bundle();
        params.putCharSequence("text", text);
        params.putInt("newCursorPosition", newCursorPosition);
        return callCommandInternal("setComposingText", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#setComposingRegion(int, int)} with the
     * given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().setComposingRegion(start, end)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param start to be passed as the {@code start} parameter
     * @param end to be passed as the {@code end} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callSetComposingRegion(int start, int end) {
        final Bundle params = new Bundle();
        params.putInt("start", start);
        params.putInt("end", end);
        return callCommandInternal("setComposingRegion", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#finishComposingText()} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().finishComposingText()}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callFinishComposingText() {
        final Bundle params = new Bundle();
        return callCommandInternal("finishComposingText", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#commitText(CharSequence, int)} with the
     * given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().commitText(text, newCursorPosition)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param text to be passed as the {@code text} parameter
     * @param newCursorPosition to be passed as the {@code newCursorPosition} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callCommitText(@Nullable CharSequence text, int newCursorPosition) {
        final Bundle params = new Bundle();
        params.putCharSequence("text", text);
        params.putInt("newCursorPosition", newCursorPosition);
        return callCommandInternal("commitText", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#commitCompletion(CompletionInfo)} with
     * the given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().commitCompletion(text)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param text to be passed as the {@code text} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callCommitCompletion(@Nullable CompletionInfo text) {
        final Bundle params = new Bundle();
        params.putParcelable("text", text);
        return callCommandInternal("commitCompletion", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#commitCorrection(CorrectionInfo)} with
     * the given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().commitCorrection(correctionInfo)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param correctionInfo to be passed as the {@code correctionInfo} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callCommitCorrection(@Nullable CorrectionInfo correctionInfo) {
        final Bundle params = new Bundle();
        params.putParcelable("correctionInfo", correctionInfo);
        return callCommandInternal("commitCorrection", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#setSelection(int, int)} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().setSelection(start, end)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param start to be passed as the {@code start} parameter
     * @param end to be passed as the {@code end} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callSetSelection(int start, int end) {
        final Bundle params = new Bundle();
        params.putInt("start", start);
        params.putInt("end", end);
        return callCommandInternal("setSelection", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#performEditorAction(int)} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().performEditorAction(editorAction)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param editorAction to be passed as the {@code editorAction} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callPerformEditorAction(int editorAction) {
        final Bundle params = new Bundle();
        params.putInt("editorAction", editorAction);
        return callCommandInternal("performEditorAction", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#performContextMenuAction(int)} with the
     * given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().performContextMenuAction(id)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param id to be passed as the {@code id} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callPerformContextMenuAction(int id) {
        final Bundle params = new Bundle();
        params.putInt("id", id);
        return callCommandInternal("performContextMenuAction", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#beginBatchEdit()} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().beginBatchEdit()}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callBeginBatchEdit() {
        final Bundle params = new Bundle();
        return callCommandInternal("beginBatchEdit", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#endBatchEdit()} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().endBatchEdit()}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callEndBatchEdit() {
        final Bundle params = new Bundle();
        return callCommandInternal("endBatchEdit", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#sendKeyEvent(KeyEvent)} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().sendKeyEvent(event)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param event to be passed as the {@code event} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callSendKeyEvent(@Nullable KeyEvent event) {
        final Bundle params = new Bundle();
        params.putParcelable("event", event);
        return callCommandInternal("sendKeyEvent", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#clearMetaKeyStates(int)} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().sendKeyEvent(event)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param states to be passed as the {@code states} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callClearMetaKeyStates(int states) {
        final Bundle params = new Bundle();
        params.putInt("states", states);
        return callCommandInternal("clearMetaKeyStates", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#reportFullscreenMode(boolean)} with the
     * given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().reportFullscreenMode(enabled)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param enabled to be passed as the {@code enabled} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callReportFullscreenMode(boolean enabled) {
        final Bundle params = new Bundle();
        params.putBoolean("enabled", enabled);
        return callCommandInternal("reportFullscreenMode", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#performPrivateCommand(String, Bundle)}
     * with the given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().performPrivateCommand(action, data)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param action to be passed as the {@code action} parameter
     * @param data to be passed as the {@code data} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callPerformPrivateCommand(@Nullable String action, Bundle data) {
        final Bundle params = new Bundle();
        params.putString("action", action);
        params.putBundle("data", data);
        return callCommandInternal("performPrivateCommand", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#requestCursorUpdates(int)} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().requestCursorUpdates(cursorUpdateMode)}.
     * </p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param cursorUpdateMode to be passed as the {@code cursorUpdateMode} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callRequestCursorUpdates(int cursorUpdateMode) {
        final Bundle params = new Bundle();
        params.putInt("cursorUpdateMode", cursorUpdateMode);
        return callCommandInternal("requestCursorUpdates", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#getHandler()} with the given parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().getHandler()}.</p>
     *
     * <p>Use {@link ImeEvent#isNullReturnValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API was {@code null} or not.</p>
     *
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callGetHandler() {
        final Bundle params = new Bundle();
        return callCommandInternal("getHandler", params);
    }

    /**
     * Lets {@link MockIme} to call {@link InputConnection#closeConnection()} with the given
     * parameters.
     *
     * <p>This triggers {@code getCurrentInputConnection().closeConnection()}.</p>
     *
     * <p>Return value information is not available for this command.</p>
     *
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callCloseConnection() {
        final Bundle params = new Bundle();
        return callCommandInternal("closeConnection", params);
    }

    /**
     * Lets {@link MockIme} to call
     * {@link InputConnection#commitContent(InputContentInfo, int, Bundle)} with the given
     * parameters.
     *
     * <p>This triggers
     * {@code getCurrentInputConnection().commitContent(inputContentInfo, flags, opts)}.</p>
     *
     * <p>Use {@link ImeEvent#getReturnBooleanValue()} for {@link ImeEvent} returned from
     * {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to see the
     * value returned from the API.</p>
     *
     * @param inputContentInfo to be passed as the {@code inputContentInfo} parameter
     * @param flags to be passed as the {@code flags} parameter
     * @param opts to be passed as the {@code opts} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callCommitContent(@NonNull InputContentInfo inputContentInfo, int flags,
            @Nullable Bundle opts) {
        final Bundle params = new Bundle();
        params.putParcelable("inputContentInfo", inputContentInfo);
        params.putInt("flags", flags);
        params.putBundle("opts", opts);
        return callCommandInternal("commitContent", params);
    }

    /**
     * Lets {@link MockIme} to call
     * {@link android.inputmethodservice.InputMethodService#setBackDisposition(int)} with the given
     * parameters.
     *
     * <p>This triggers {@code setBackDisposition(backDisposition)}.</p>
     *
     * @param backDisposition to be passed as the {@code backDisposition} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callSetBackDisposition(int backDisposition) {
        final Bundle params = new Bundle();
        params.putInt("backDisposition", backDisposition);
        return callCommandInternal("setBackDisposition", params);
    }

    /**
     * Lets {@link MockIme} to call
     * {@link android.inputmethodservice.InputMethodService#requestHideSelf(int)} with the given
     * parameters.
     *
     * <p>This triggers {@code requestHideSelf(flags)}.</p>
     *
     * @param flags to be passed as the {@code flags} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callRequestHideSelf(int flags) {
        final Bundle params = new Bundle();
        params.putInt("flags", flags);
        return callCommandInternal("requestHideSelf", params);
    }

    /**
     * Lets {@link MockIme} to call
     * {@link android.inputmethodservice.InputMethodService#requestShowSelf(int)} with the given
     * parameters.
     *
     * <p>This triggers {@code requestShowSelf(flags)}.</p>
     *
     * @param flags to be passed as the {@code flags} parameter
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callRequestShowSelf(int flags) {
        final Bundle params = new Bundle();
        params.putInt("flags", flags);
        return callCommandInternal("requestShowSelf", params);
    }

    /**
     * Lets {@link MockIme} call
     * {@link android.inputmethodservice.InputMethodService#sendDownUpKeyEvents(int)} with the given
     * {@code keyEventCode}.
     *
     * @param keyEventCode to be passed as the {@code keyEventCode} parameter.
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand callSendDownUpKeyEvents(int keyEventCode) {
        final Bundle params = new Bundle();
        params.putInt("keyEventCode", keyEventCode);
        return callCommandInternal("sendDownUpKeyEvents", params);
    }

    /**
     * Lets {@link MockIme} call
     * {@link android.content.pm.PackageManager#getApplicationInfo(String, int)} with the given
     * {@code packageName} and {@code flags}.
     *
     * @param packageName the package name to be passed to
     *                    {@link android.content.pm.PackageManager#getApplicationInfo(String, int)}.
     * @param flags the flags to be passed to
     *                    {@link android.content.pm.PackageManager#getApplicationInfo(String, int)}.
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}.
     */
    @NonNull
    public ImeCommand callGetApplicationInfo(@NonNull String packageName, int flags) {
        final Bundle params = new Bundle();
        params.putString("packageName", packageName);
        params.putInt("flags", flags);
        return callCommandInternal("getApplicationInfo", params);
    }

    @NonNull
    public ImeCommand callGetDisplayId() {
        final Bundle params = new Bundle();
        return callCommandInternal("getDisplayId", params);
    }

    /**
     * Verifies {@code InputMethodService.getLayoutInflater().getContext()} is equal to
     * {@code InputMethodService.this}.
     *
     * @return {@link ImeCommand} object that can be passed to
     *         {@link ImeEventStreamTestUtils#expectCommand(ImeEventStream, ImeCommand, long)} to
     *         wait until this event is handled by {@link MockIme}
     */
    @NonNull
    public ImeCommand verifyLayoutInflaterContext() {
        final Bundle params = new Bundle();
        return callCommandInternal("verifyLayoutInflaterContext", params);
    }

    @NonNull
    public ImeCommand callSetHeight(int height) {
        final Bundle params = new Bundle();
        params.putInt("height", height);
        return callCommandInternal("setHeight", params);
    }

    @NonNull
    public ImeCommand callSetInlineSuggestionsExtras(@NonNull Bundle bundle) {
        return callCommandInternal("setInlineSuggestionsExtras", bundle);
    }

    @NonNull
    public ImeCommand callVerifyGetDisplay() {
        return callCommandInternal("verifyGetDisplay", new Bundle());
    }

    @NonNull
    public ImeCommand callVerifyGetWindowManager() {
        return callCommandInternal("verifyGetWindowManager", new Bundle());
    }

    @NonNull
    public ImeCommand callVerifyGetViewConfiguration() {
        return callCommandInternal("verifyGetViewConfiguration", new Bundle());
    }
}
