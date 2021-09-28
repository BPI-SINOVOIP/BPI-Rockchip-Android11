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

package com.android.inputmethod.leanback.service;

import android.content.Intent;
import android.inputmethodservice.InputMethodService;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.util.Log;

import com.android.inputmethod.leanback.LeanbackKeyboardContainer;
import com.android.inputmethod.leanback.LeanbackKeyboardController;
import com.android.inputmethod.leanback.LeanbackKeyboardView;
import com.android.inputmethod.leanback.LeanbackLocales;
import com.android.inputmethod.leanback.LeanbackSuggestionsFactory;
import com.android.inputmethod.leanback.LeanbackUtils;

/**
 * This is a simplified version of GridIme
 */
public class LeanbackImeService extends InputMethodService {

    private static final String TAG = "LbImeService";
    private static final boolean DEBUG = false;

    // use dpad events, with lock axis
    static final int MODE_TRACKPAD_NAVIGATION = 0;
    // track motion directly.
    static final int MODE_FREE_MOVEMENT = 1;

    public static final int MAX_SUGGESTIONS = 10;

    private static final int MSG_SUGGESTIONS_CLEAR = 123;
    private static final int SUGGESTIONS_CLEAR_DELAY = 1000;

    public static final String IME_OPEN = "com.android.inputmethod.leanback.action.IME_OPEN";
    public static final String IME_CLOSE = "com.android.inputmethod.leanback.action.IME_CLOSE";

    private LeanbackKeyboardController.InputListener mInputListener
            = new LeanbackKeyboardController.InputListener() {
        @Override
        public void onEntry(int type, int keyCode, CharSequence result) {
            handleTextEntry(type, keyCode, result);
        }
    };

    private View mInputView;
    private LeanbackKeyboardController mKeyboardController;
    private LeanbackSuggestionsFactory mSuggestionsFactory;

    // IME will auto insert space after clicking on the candidates if next
    // character is alphabet
    private boolean mEnterSpaceBeforeCommitting;

    private boolean mShouldClearSuggestions = true;
    private final Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (msg.what == MSG_SUGGESTIONS_CLEAR) {
                if (mShouldClearSuggestions) {
                    mSuggestionsFactory.clearSuggestions();
                    mKeyboardController.updateSuggestions(mSuggestionsFactory.getSuggestions());
                    mShouldClearSuggestions = false;
                }
            }
        }
    };

    public LeanbackImeService() {
        if (!enableHardwareAcceleration()) {
            Log.w(TAG, "Could not enable hardware acceleration");
        }
    }

    private void clearSuggestionsDelayed() {
        // if suggestions amend, we should keep clearing them
        if (!mSuggestionsFactory.shouldSuggestionsAmend()) {
            mHandler.removeMessages(MSG_SUGGESTIONS_CLEAR);
            mShouldClearSuggestions = true;
            mHandler.sendEmptyMessageDelayed(MSG_SUGGESTIONS_CLEAR, SUGGESTIONS_CLEAR_DELAY);
        }
    }

    @Override
    public void onInitializeInterface() {
        mKeyboardController = new LeanbackKeyboardController(this, mInputListener);
        mEnterSpaceBeforeCommitting = false;
        mSuggestionsFactory = new LeanbackSuggestionsFactory(this, MAX_SUGGESTIONS);
    }

    @Override
    public View onCreateInputView() {
        mInputView = mKeyboardController.getView();
        mInputView.requestFocus();
        return mInputView;
    }

    /**
     * {@inheritDoc} This function gets called whenever we start the input
     * window
     */
    @Override
    public void onStartInputView(EditorInfo info, boolean restarting) {
        super.onStartInputView(info, restarting);
        mKeyboardController.onStartInputView();
        sendBroadcast(new Intent(IME_OPEN));

        if (mKeyboardController.areSuggestionsEnabled()) {
            mSuggestionsFactory.createSuggestions();
            mKeyboardController.updateSuggestions(mSuggestionsFactory.getSuggestions());

            // repost text to get completions
            InputConnection ic = getCurrentInputConnection();
            if (ic != null) {
                String c = getEditorText(ic);
                ic.deleteSurroundingText(getCharLengthBeforeCursor(ic),
                        getCharLengthAfterCursor(ic));
                ic.commitText(c, 1);
            }
        }
    }


    @Override
    public void onFinishInputView(boolean finishingInput) {
        super.onFinishInputView(finishingInput);
        sendBroadcast(new Intent(IME_CLOSE));
        mSuggestionsFactory.clearSuggestions();
    }

    /**
     * {@inheritDoc} This function doesn't get called when we dismiss the
     * keyboard, and reopen it on the same input field
     */
    @Override
    public void onStartInput(EditorInfo attribute, boolean restarting) {
        super.onStartInput(attribute, restarting);
        mEnterSpaceBeforeCommitting = false;
        mSuggestionsFactory.onStartInput(attribute);
        mKeyboardController.onStartInput(attribute);
    }

    /**
     * {@inheritDoc} Always return true to show GridIme when editText calls
     * requestFocus
     */
    @Override
    public boolean onShowInputRequested(int flags, boolean configChange) {
        return true;
    }

    /**
     * {@inheritDoc} Always enable soft keyboard. If we return the super method,
     * the IME will not be shown if there is a hardware keyboard connected
     */
    @Override
    public boolean onEvaluateInputViewShown() {
        return true;
    }

    @Override
    public boolean onEvaluateFullscreenMode() {
        // Superclass always returns true in landscape mode.
        // Assume we're on TV with lots of display area.
        return false;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (isInputViewShown()
                && mKeyboardController.onKeyUp(keyCode, event)) {
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (isInputViewShown()
                && mKeyboardController.onKeyDown(keyCode, event)) {
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (isInputViewShown() && (event.getSource() & InputDevice.SOURCE_TOUCH_NAVIGATION)
                == InputDevice.SOURCE_TOUCH_NAVIGATION) {
            if (mKeyboardController.onGenericMotionEvent(event)) {
                return true;
            }
        }
        return super.onGenericMotionEvent(event);
    }

    @Override
    public void onDisplayCompletions(CompletionInfo[] completions) {
        if (mKeyboardController.areSuggestionsEnabled()) {
            mShouldClearSuggestions = false;
            mHandler.removeMessages(MSG_SUGGESTIONS_CLEAR);
            mSuggestionsFactory.onDisplayCompletions(completions);
            mKeyboardController.updateSuggestions(mSuggestionsFactory.getSuggestions());
        }
    }

    private String getEditorText(InputConnection ic) {
        StringBuilder editorText = new StringBuilder();
        CharSequence textBeforeCursor = ic.getTextBeforeCursor(1000, 0);
        CharSequence textAfterCursor = ic.getTextAfterCursor(1000, 0);
        if (textBeforeCursor != null) {
            editorText.append(textBeforeCursor);
        }
        if (textAfterCursor != null) {
            editorText.append(textAfterCursor);
        }
        return editorText.toString();
    }

    private int getAmpersandLocation(InputConnection ic) {
        String editorText = getEditorText(ic);
        int indexOf = editorText.indexOf('@');
        if (indexOf < 0) {
            indexOf = editorText.length();
        }

        return indexOf;
    }

    private int getCharLengthBeforeCursor(InputConnection ic) {
        final CharSequence textLeft = ic.getTextBeforeCursor(1000, 0);
        return textLeft != null ? textLeft.length() : 0;
    }

    private int getCharLengthAfterCursor(InputConnection ic ) {
        final CharSequence textRight = ic.getTextAfterCursor(1000, 0);
        return textRight != null ? textRight.length() : 0;
    }

    private void handleTextEntry(int type, int keyCode, CharSequence c) {
        InputConnection ic = getCurrentInputConnection();
        boolean updateSuggestions = true;

        if (ic == null) {
            return;
        }

        switch (type) {
            case LeanbackKeyboardController.InputListener.ENTRY_TYPE_BACKSPACE:
                clearSuggestionsDelayed();
                ic.deleteSurroundingText(1, 0);
                mEnterSpaceBeforeCommitting = false;
                break;
            case LeanbackKeyboardController.InputListener.ENTRY_TYPE_LEFT:
            case LeanbackKeyboardController.InputListener.ENTRY_TYPE_RIGHT:
                CharSequence textBeforeCursor = ic.getTextBeforeCursor(1000, 0);
                int newCursorPosition = textBeforeCursor == null ? 0 : textBeforeCursor.length();

                if (type == LeanbackKeyboardController.InputListener.ENTRY_TYPE_LEFT) {
                    if (newCursorPosition > 0) {
                        newCursorPosition--;
                    }
                } else {
                    CharSequence textAfterCursor = ic.getTextAfterCursor(1000, 0);
                    if (textAfterCursor != null && textAfterCursor.length() > 0) {
                        newCursorPosition++;
                    }
                }

                ic.setSelection(newCursorPosition, newCursorPosition);
                break;
            case LeanbackKeyboardController.InputListener.ENTRY_TYPE_STRING:
                clearSuggestionsDelayed();
                if (mEnterSpaceBeforeCommitting
                        && mKeyboardController.enableAutoEnterSpace()) {
                    if (LeanbackUtils.isAlphabet(keyCode)) {
                        ic.commitText(" ", 1);
                    }
                    mEnterSpaceBeforeCommitting = false;
                }
                ic.commitText(c, 1);
                if (keyCode == LeanbackKeyboardView.ASCII_PERIOD) {
                    mEnterSpaceBeforeCommitting = true;
                }
                break;
            case LeanbackKeyboardController.InputListener.ENTRY_TYPE_SUGGESTION:
            case LeanbackKeyboardController.InputListener.ENTRY_TYPE_VOICE:
                clearSuggestionsDelayed();
                if (!mSuggestionsFactory.shouldSuggestionsAmend()) {
                    ic.deleteSurroundingText(getCharLengthBeforeCursor(ic),
                            getCharLengthAfterCursor(ic));
                } else {
                    int location = getAmpersandLocation(ic);
                    ic.setSelection(location, location);
                    ic.deleteSurroundingText(0, getCharLengthAfterCursor(ic));
                }
                ic.commitText(c, 1);
                mEnterSpaceBeforeCommitting = true;
                // go straight into action (skip updating suggestions)
            case LeanbackKeyboardController.InputListener.ENTRY_TYPE_ACTION:
                sendDefaultEditorAction(false);
                updateSuggestions = false;
                break;
            case LeanbackKeyboardController.InputListener.ENTRY_TYPE_DISMISS:
                ic.performEditorAction(EditorInfo.IME_ACTION_NONE);
                updateSuggestions = false;
                break;
            case LeanbackKeyboardController.InputListener.ENTRY_TYPE_VOICE_DISMISS:
                ic.performEditorAction(EditorInfo.IME_ACTION_GO);
                updateSuggestions = false;
                break;
        }

        if (mKeyboardController.areSuggestionsEnabled() && updateSuggestions) {
            mKeyboardController.updateSuggestions(mSuggestionsFactory.getSuggestions());
        }
    }

    public void onHideIme() {
        requestHideSelf(0);
    }
}
