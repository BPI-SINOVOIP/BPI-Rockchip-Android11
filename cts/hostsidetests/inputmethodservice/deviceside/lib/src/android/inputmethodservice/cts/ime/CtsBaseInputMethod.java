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

package android.inputmethodservice.cts.ime;

import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_BIND_INPUT;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_CREATE;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_DESTROY;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_FINISH_INPUT;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_FINISH_INPUT_VIEW;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_START_INPUT;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_START_INPUT_VIEW;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_UNBIND_INPUT;
import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.content.Intent;
import android.graphics.Bitmap;
import android.inputmethodservice.InputMethodService;
import android.inputmethodservice.cts.DeviceEvent;
import android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType;
import android.inputmethodservice.cts.ime.ImeCommandReceiver.ImeCommandCallbacks;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.function.Consumer;

/**
 * Base class to create test {@link InputMethodService}.
 */
public abstract class CtsBaseInputMethod extends InputMethodService implements ImeCommandCallbacks {

    protected static final boolean DEBUG = false;

    public static final String EDITOR_INFO_KEY_REPLY_USER_HANDLE_SESSION_ID =
            "android.inputmethodservice.cts.ime.ReplyUserHandleSessionId";

    public static final String ACTION_KEY_REPLY_USER_HANDLE =
            "android.inputmethodservice.cts.ime.ReplyUserHandle";

    public static final String BUNDLE_KEY_REPLY_USER_HANDLE =
            "android.inputmethodservice.cts.ime.ReplyUserHandle";

    public static final String BUNDLE_KEY_REPLY_USER_HANDLE_SESSION_ID =
            "android.inputmethodservice.cts.ime.ReplyUserHandleSessionId";

    private final ImeCommandReceiver<CtsBaseInputMethod> mImeCommandReceiver =
            new ImeCommandReceiver<>();
    private String mLogTag;

    @Override
    public void onCreate() {
        mLogTag = getClass().getSimpleName();
        if (DEBUG) {
            Log.d(mLogTag, "onCreate:");
        }
        sendEvent(ON_CREATE);

        super.onCreate();

        mImeCommandReceiver.register(this /* ime */);
    }

    protected View createInputViewInternal(Watermark watermark) {
        final FrameLayout layout = new FrameLayout(this);
        layout.setForegroundGravity(Gravity.CENTER);

        final TextView textView = new TextView(this);
        textView.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        textView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
        textView.setGravity(Gravity.CENTER);
        textView.setText(getClass().getName());
        textView.setBackgroundColor(0xffffaa99);
        layout.addView(textView);

        final ImageView imageView = new ImageView(this);
        final Bitmap bitmap = watermark.toBitmap();
        imageView.setImageBitmap(bitmap);
        layout.addView(imageView, new FrameLayout.LayoutParams(
                bitmap.getWidth(), bitmap.getHeight(), Gravity.CENTER));
        return layout;
    }

    @Override
    public void onBindInput() {
        if (DEBUG) {
            Log.d(mLogTag, "onBindInput");
        }
        sendEvent(ON_BIND_INPUT);
        super.onBindInput();
    }

    @Override
    public boolean onEvaluateFullscreenMode() {
        // Opt-out the fullscreen mode regardless of the existence of the hardware keyboard
        // and screen rotation.  Otherwise test scenarios become unpredictable.
        return false;
    }

    @Override
    public boolean onEvaluateInputViewShown() {
        // Always returns true regardless of the existence of the hardware keyboard.
        // Otherwise test scenarios become unpredictable.
        return true;
    }

    @Override
    public boolean onShowInputRequested(int flags, boolean configChange) {
        // Always returns true regardless of the existence of the hardware keyboard.
        // Otherwise test scenarios become unpredictable.
        return true;
    }

    @Override
    public void onStartInput(EditorInfo editorInfo, boolean restarting) {
        if (DEBUG) {
            Log.d(mLogTag, "onStartInput:"
                    + " editorInfo=" + editorInfo
                    + " restarting=" + restarting);
        }
        sendEvent(ON_START_INPUT, editorInfo, restarting);

        super.onStartInput(editorInfo, restarting);

        if (editorInfo.extras != null) {
            final String sessionKey =
                    editorInfo.extras.getString(EDITOR_INFO_KEY_REPLY_USER_HANDLE_SESSION_ID, null);
            if (sessionKey != null) {
                final Bundle bundle = new Bundle();
                bundle.putString(BUNDLE_KEY_REPLY_USER_HANDLE_SESSION_ID, sessionKey);
                bundle.putParcelable(BUNDLE_KEY_REPLY_USER_HANDLE, Process.myUserHandle());
                getCurrentInputConnection().performPrivateCommand(
                        ACTION_KEY_REPLY_USER_HANDLE, bundle);
            }
        }
    }

    @Override
    public void onStartInputView(EditorInfo editorInfo, boolean restarting) {
        if (DEBUG) {
            Log.d(mLogTag, "onStartInputView:"
                    + " editorInfo=" + editorInfo
                    + " restarting=" + restarting);
        }
        sendEvent(ON_START_INPUT_VIEW, editorInfo, restarting);

        super.onStartInputView(editorInfo, restarting);
    }

    @Override
    public void onUnbindInput() {
        super.onUnbindInput();
        if (DEBUG) {
            Log.d(mLogTag, "onUnbindInput");
        }
        sendEvent(ON_UNBIND_INPUT);
    }

    @Override
    public void onFinishInputView(boolean finishingInput) {
        if (DEBUG) {
            Log.d(mLogTag, "onFinishInputView: finishingInput=" + finishingInput);
        }
        sendEvent(ON_FINISH_INPUT_VIEW, finishingInput);

        super.onFinishInputView(finishingInput);
    }

    @Override
    public void onFinishInput() {
        if (DEBUG) {
            Log.d(mLogTag, "onFinishInput:");
        }
        sendEvent(ON_FINISH_INPUT);

        super.onFinishInput();
    }

    @Override
    public void onDestroy() {
        if (DEBUG) {
            Log.d(mLogTag, "onDestroy:");
        }
        sendEvent(ON_DESTROY);

        super.onDestroy();

        unregisterReceiver(mImeCommandReceiver);
    }

    //
    // Implementations of {@link ImeCommandCallbacks}.
    //

    @Override
    public void commandCommitText(CharSequence text, int newCursorPosition) {
        executeOnInputConnection(ic -> {
            // TODO: Log the return value of {@link InputConnection#commitText(CharSequence,int)}.
            ic.commitText(text, newCursorPosition);
        });
    }

    @Override
    public void commandSwitchInputMethod(String imeId) {
        switchInputMethod(imeId);
    }

    @Override
    public void commandRequestHideSelf(int flags) {
        requestHideSelf(flags);
    }

    private void executeOnInputConnection(Consumer<InputConnection> consumer) {
        final InputConnection ic = getCurrentInputConnection();
        // TODO: Check and log whether {@code ic} is null or equals to
        // {@link #getCurrentInputBindin().getConnection()}.
        if (ic != null) {
            consumer.accept(ic);
        }
    }

    private void sendEvent(DeviceEventType type, Object... args) {
        final String sender = getClass().getName();
        final Intent intent = DeviceEvent.newDeviceEventIntent(sender, type);
        // TODO: Send arbitrary {@code args} in {@code intent}.
        sendBroadcast(intent);
    }
}
