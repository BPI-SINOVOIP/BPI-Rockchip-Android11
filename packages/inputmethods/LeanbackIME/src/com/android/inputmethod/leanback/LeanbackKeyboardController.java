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

package com.android.inputmethod.leanback;

import android.graphics.PointF;
import android.inputmethodservice.InputMethodService;
import android.inputmethodservice.Keyboard;
import android.inputmethodservice.Keyboard.Key;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import com.android.inputmethod.leanback.LeanbackKeyboardContainer.KeyFocus;

import java.util.ArrayList;
/**
 * Holds logic for the keyboard views. This includes things like when to
 * snap, when to switch keyboards, etc. It provides callbacks for when actions
 * that need to be handled at the IME level occur (when text is entered, when
 * the action should be performed).
 */
public class LeanbackKeyboardController implements LeanbackKeyboardContainer.VoiceListener,
        LeanbackKeyboardContainer.DismissListener {
    private static final String TAG = "LbKbController";
    private static final boolean DEBUG = false;

    /**
     * The amount of time to block movement after a button down was detected.
     */
    public static final int CLICK_MOVEMENT_BLOCK_DURATION_MS = 500;

    /**
     * The minimum distance in pixels before the view will transition to the
     * move state.
     */
    public float mResizeSquareDistance;

    // keep track of the most recent key changes and their times so we can
    // revert motion caused by clicking
    private static final int KEY_CHANGE_HISTORY_SIZE = 10;
    private static final long KEY_CHANGE_REVERT_TIME_MS = 100;

    /**
     * This listener reports high level actions that have occurred, such as
     * text entry (from keys or voice) or the action button being pressed.
     */
    public interface InputListener {
        public static final int ENTRY_TYPE_STRING = 0;
        public static final int ENTRY_TYPE_BACKSPACE = 1;
        public static final int ENTRY_TYPE_SUGGESTION = 2;
        public static final int ENTRY_TYPE_LEFT = 3;
        public static final int ENTRY_TYPE_RIGHT = 4;
        public static final int ENTRY_TYPE_ACTION = 5;
        public static final int ENTRY_TYPE_VOICE = 6;
        public static final int ENTRY_TYPE_DISMISS = 7;
        public static final int ENTRY_TYPE_VOICE_DISMISS = 8;

        /**
         * Sent when the user has selected something that should affect the text
         * field, such as entering a character, selecting the action, or
         * finishing a voice action.
         *
         * @param type The type of key selected
         * @param keyCode the key code of the key if applicable
         * @param result The text entered if applicable
         */
        public void onEntry(int type, int keyCode, CharSequence result);
    }

    private static final class KeyChange {
        public long time;
        public PointF position;

        public KeyChange(long time, PointF position) {
            this.time = time;
            this.position = position;
        }
    }

    private class DoubleClickDetector {
        final long DOUBLE_CLICK_TIMEOUT_MS = 200;
        long mFirstClickTime = 0;
        boolean mFirstClickShiftLocked;

        public void reset() {
            mFirstClickTime = 0;
        }

        public void addEvent(long currTime) {
            if (currTime - mFirstClickTime > DOUBLE_CLICK_TIMEOUT_MS) {
                mFirstClickTime = currTime;
                mFirstClickShiftLocked = mContainer.isCapsLockOn();
                commitKey();
            } else {
                mContainer.onShiftDoubleClick(mFirstClickShiftLocked);
                reset();
            }
        }
    }

    private DoubleClickDetector mDoubleClickDetector = new DoubleClickDetector();

    private View.OnLayoutChangeListener mOnLayoutChangeListener
            = new View.OnLayoutChangeListener() {

                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    int w = right - left;
                    int h = bottom - top;
                    int oldW = oldRight - oldLeft;
                    int oldH = oldBottom - oldTop;
                    if (w > 0 && h > 0) {
                        if (w != oldW || h != oldH) {
                            initInputView();
                        }
                    }
                }
            };

    private InputMethodService mContext;
    private InputListener mInputListener;
    private LeanbackKeyboardContainer mContainer;

    private LeanbackKeyboardContainer.KeyFocus mDownFocus =
            new LeanbackKeyboardContainer.KeyFocus();
    private LeanbackKeyboardContainer.KeyFocus mTempFocus =
            new LeanbackKeyboardContainer.KeyFocus();

    ArrayList<KeyChange> mKeyChangeHistory = new ArrayList<KeyChange>(KEY_CHANGE_HISTORY_SIZE + 1);
    private PointF mTempPoint = new PointF();

    private boolean mKeyDownReceived = false;
    private boolean mLongPressHandled = false;
    private KeyFocus mKeyDownKeyFocus;
    private int mMoveCount;

    public LeanbackKeyboardController(InputMethodService context, InputListener listener) {
        this(context, listener, new LeanbackKeyboardContainer(context));
    }

    LeanbackKeyboardController(InputMethodService context, InputListener listener,
            LeanbackKeyboardContainer container) {
        mContext = context;
        mResizeSquareDistance = context.getResources().getDimension(R.dimen.resize_move_distance);
        mResizeSquareDistance *= mResizeSquareDistance;
        mInputListener = listener;
        setKeyboardContainer(container);
        mContainer.setVoiceListener(this);
        mContainer.setDismissListener(this);
    }

    /**
     * This method is called when we start the input at a NEW input field.
     */
    public void onStartInput(EditorInfo attribute) {
        if (mContainer != null) {
            mContainer.onStartInput(attribute);
            initInputView();
        }
    }

    /**
     * This method is called by whenever we bring up the IME at an input field.
     */
    public void onStartInputView() {
        mKeyDownReceived = false;
        if (mContainer != null) {
            mContainer.onStartInputView();
        }
        mDoubleClickDetector.reset();
    }

    /**
     * This method sets the pixel positions in mSpaceTracker to match the
     * current KeyFocus in {@link LeanbackKeyboardContainer} This method is called
     * when the keyboard layout is complete, after
     * {@link LeanbackKeyboardContainer.onInitInputView}, to initialize the starting
     * position of mSpaceTracker; and in onUp to reset the pixel position in
     * mSpaceTracker.
     */
    private void updatePositionToCurrentFocus() {
        PointF currPosition = getCurrentKeyPosition();
        if (currPosition != null) {
        }
    }

    private void initInputView() {
        mContainer.onInitInputView();
        updatePositionToCurrentFocus();
    }

    private PointF getCurrentKeyPosition() {
        if (mContainer != null) {
            LeanbackKeyboardContainer.KeyFocus initialKeyInfo = mContainer.getCurrFocus();
            return new PointF(initialKeyInfo.rect.centerX(), initialKeyInfo.rect.centerY());
        }
        return null;
    }

    private void performBestSnap(long time) {
        KeyFocus focus = mContainer.getCurrFocus();
        mTempPoint.x = focus.rect.centerX();
        mTempPoint.y = focus.rect.centerY();
        PointF bestSnap = getBestSnapPosition(mTempPoint, time);
        mContainer.getBestFocus(bestSnap.x, bestSnap.y, mTempFocus);
        mContainer.setFocus(mTempFocus);
        updatePositionToCurrentFocus();
    }

    private PointF getBestSnapPosition(PointF currPoint, long currTime) {
        if (mKeyChangeHistory.size() <= 1) {
            return currPoint;
        }
        for (int i = 0; i < mKeyChangeHistory.size() - 1; i++) {
            KeyChange change = mKeyChangeHistory.get(i);
            KeyChange nextChange = mKeyChangeHistory.get(i + 1);
            if (currTime - nextChange.time < KEY_CHANGE_REVERT_TIME_MS) {
                if (DEBUG) {
                    Log.d(TAG, "Reverting keychange to " + change.position.toString());
                }
                // Return the oldest key change within the revert window and
                // clear all key changes
                currPoint = change.position;
                // on a revert, clear the history and add the reverting point.
                // This way the reverted point will be preferred if there's
                // another fast change before the next call.
                mKeyChangeHistory.clear();
                mKeyChangeHistory.add(new KeyChange(currTime, currPoint));
                break;
            }
        }
        return currPoint;
    }

    public void setKeyboardContainer(LeanbackKeyboardContainer container) {
        mContainer = container;
        container.getView().addOnLayoutChangeListener(mOnLayoutChangeListener);
    }

    public View getView() {
        if (mContainer != null) {
            return mContainer.getView();
        }
        return null;
    }

    public boolean areSuggestionsEnabled() {
        if (mContainer != null) {
            return mContainer.areSuggestionsEnabled();
        }
        return false;
    }

    public boolean enableAutoEnterSpace() {
        if (mContainer != null) {
            return mContainer.enableAutoEnterSpace();
        }
        return false;
    }

    public boolean onKeyDown(int keyCode, KeyEvent event) {
        mDownFocus.set(mContainer.getCurrFocus());
        // this will handle other events, e.g. hardware keyboard
        if (isEnterKey(keyCode)) {
            mKeyDownReceived = true;
            // first keyDown
            if (event.getRepeatCount() == 0) {
                mContainer.setTouchState(LeanbackKeyboardContainer.TOUCH_STATE_CLICK);
            }
        }

        return handleKeyDownEvent(keyCode, event.getRepeatCount());
    }

    public boolean onKeyUp(int keyCode, KeyEvent event) {
        // this only handles InputDevice.SOURCE_TOUCH_NAVIGATION events
        if (isEnterKey(keyCode)) {
            if (!mKeyDownReceived || mLongPressHandled) {
                mLongPressHandled = false;
                return true;
            }
            mKeyDownReceived = false;
            if (mContainer.getTouchState() == LeanbackKeyboardContainer.TOUCH_STATE_CLICK) {
                mContainer.setTouchState(LeanbackKeyboardContainer.TOUCH_STATE_TOUCH_SNAP);
            }
        }
        return handleKeyUpEvent(keyCode, event.getEventTime());
    }

    public boolean onGenericMotionEvent(MotionEvent event) {
        return false;
    }

    private boolean onDirectionalMove(int dir) {
        if (mContainer.getNextFocusInDirection(dir, mDownFocus, mTempFocus)) {
            mContainer.setFocus(mTempFocus);
            mDownFocus.set(mTempFocus);

            clearKeyIfNecessary();
        }

        return true;
    }

    private void clearKeyIfNecessary() {
        mMoveCount++;
        if (mMoveCount >= 3) {
            mMoveCount = 0;
            mKeyDownKeyFocus = null;
        }
    }

    private void commitKey() {
        commitKey(mContainer.getCurrFocus());
    }

    private void commitKey(LeanbackKeyboardContainer.KeyFocus keyFocus) {
        if (mContainer == null || keyFocus == null) {
            return;
        }

        switch (keyFocus.type) {
            case KeyFocus.TYPE_VOICE:
                // voice doesn't have to go through the IME
                mContainer.onVoiceClick();
                break;
            case KeyFocus.TYPE_ACTION:
                mInputListener.onEntry(InputListener.ENTRY_TYPE_ACTION, 0, null);
                break;
            case KeyFocus.TYPE_SUGGESTION:
                mInputListener.onEntry(InputListener.ENTRY_TYPE_SUGGESTION, 0,
                        mContainer.getSuggestionText(keyFocus.index));
                break;
            default:
                Key key = mContainer.getKey(keyFocus.type, keyFocus.index);
                if (key != null) {
                    int code = key.codes[0];
                    CharSequence label = key.label;
                    handleCommitKeyboardKey(code, label);
                }
                break;

        }
    }

    private void handleCommitKeyboardKey(int code, CharSequence label) {
        switch (code) {
            case Keyboard.KEYCODE_MODE_CHANGE:
                if (Log.isLoggable(TAG, Log.VERBOSE)) {
                    Log.d(TAG, "mode change");
                }
                mContainer.onModeChangeClick();
                break;
            case LeanbackKeyboardView.KEYCODE_CAPS_LOCK:
                mContainer.onShiftDoubleClick(mContainer.isCapsLockOn());
                break;
            case Keyboard.KEYCODE_SHIFT:
                // TODO invalidate and draw a different shift
                // key in the function keyboard
                if (Log.isLoggable(TAG, Log.VERBOSE)) {
                    Log.d(TAG, "shift");
                }
                mContainer.onShiftClick();
                break;
            case LeanbackKeyboardView.KEYCODE_DISMISS_MINI_KEYBOARD:
                mContainer.dismissMiniKeyboard();
                break;
            case LeanbackKeyboardView.KEYCODE_LEFT:
                mInputListener.onEntry(InputListener.ENTRY_TYPE_LEFT, 0, null);
                break;
            case LeanbackKeyboardView.KEYCODE_RIGHT:
                mInputListener.onEntry(InputListener.ENTRY_TYPE_RIGHT, 0, null);
                break;
            case Keyboard.KEYCODE_DELETE:
                mInputListener.onEntry(InputListener.ENTRY_TYPE_BACKSPACE, 0, null);
                break;
            case LeanbackKeyboardView.ASCII_SPACE:
                mInputListener.onEntry(InputListener.ENTRY_TYPE_STRING, code, " ");
                mContainer.onSpaceEntry();
                break;
            case LeanbackKeyboardView.ASCII_PERIOD:
                mInputListener.onEntry(InputListener.ENTRY_TYPE_STRING, code, label);
                mContainer.onPeriodEntry();
                break;
            case LeanbackKeyboardView.KEYCODE_VOICE:
                mContainer.startVoiceRecording();
                break;
        // fall through to default with this label
            default:
                mInputListener.onEntry(InputListener.ENTRY_TYPE_STRING, code, label);
                mContainer.onTextEntry();

                if (mContainer.isMiniKeyboardOnScreen()) {
                    mContainer.dismissMiniKeyboard();
                }
                break;
        }
    }

    private boolean handleKeyDownEvent(int keyCode, int eventRepeatCount) {
        keyCode = getSimplifiedKey(keyCode);

        // never trap back
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            mContainer.cancelVoiceRecording();
            return false;
        }

        // capture all key downs when voice is visible
        if (mContainer.isVoiceVisible()) {
            if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT || keyCode == KeyEvent.KEYCODE_DPAD_CENTER) {
                mContainer.cancelVoiceRecording();
            }
            return true;
        }

        boolean handled = true;
        switch(keyCode) {
            // Direction keys are handled on down to allow repeated movement
            case KeyEvent.KEYCODE_DPAD_LEFT:
                handled = onDirectionalMove(LeanbackKeyboardContainer.DIRECTION_LEFT);
                break;
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                handled = onDirectionalMove(LeanbackKeyboardContainer.DIRECTION_RIGHT);
                break;
            case KeyEvent.KEYCODE_DPAD_UP:
                handled = onDirectionalMove(LeanbackKeyboardContainer.DIRECTION_UP);
                break;
            case KeyEvent.KEYCODE_DPAD_DOWN:
                handled = onDirectionalMove(LeanbackKeyboardContainer.DIRECTION_DOWN);
                break;
            case KeyEvent.KEYCODE_BUTTON_X:
                handleCommitKeyboardKey(Keyboard.KEYCODE_DELETE, null);
                break;
            case KeyEvent.KEYCODE_BUTTON_Y:
                handleCommitKeyboardKey(LeanbackKeyboardView.ASCII_SPACE, null);
                break;
            case KeyEvent.KEYCODE_BUTTON_L1:
                handleCommitKeyboardKey(LeanbackKeyboardView.KEYCODE_LEFT, null);
                break;
            case KeyEvent.KEYCODE_BUTTON_R1:
                handleCommitKeyboardKey(LeanbackKeyboardView.KEYCODE_RIGHT, null);
                break;
            // these are handled on up
            case KeyEvent.KEYCODE_DPAD_CENTER:
                if (eventRepeatCount == 0) {
                    mMoveCount = 0;
                    mKeyDownKeyFocus = new KeyFocus();
                    mKeyDownKeyFocus.set(mContainer.getCurrFocus());
                } else if (eventRepeatCount == 1) {
                    if (handleKeyLongPress(keyCode)) {
                        mKeyDownKeyFocus = null;
                    }
                }

                if (isKeyHandledOnKeyDown(mContainer.getCurrKeyCode())) {
                    commitKey();
                }
                break;
            // also handled on up
            case KeyEvent.KEYCODE_BUTTON_THUMBL:
            case KeyEvent.KEYCODE_BUTTON_THUMBR:
            case KeyEvent.KEYCODE_ENTER:
                break;
            default:
                handled = false;
                break;
        }
        return handled;
    }

    private boolean handleKeyLongPress(int keyCode) {
        mLongPressHandled = isEnterKey(keyCode) && mContainer.onKeyLongPress();
        if (mContainer.isMiniKeyboardOnScreen()) {
            Log.d(TAG, "mini keyboard shown after long press");
        }
        return mLongPressHandled;
    }

    private boolean isKeyHandledOnKeyDown(int currKeyCode) {
        return currKeyCode == Keyboard.KEYCODE_DELETE
                || currKeyCode == LeanbackKeyboardView.KEYCODE_LEFT
                || currKeyCode == LeanbackKeyboardView.KEYCODE_RIGHT;
    }

    /**
     * This handles all key events from an input device
     * @param keyCode
     * @return true if the key was handled, false otherwise
     */
    private boolean handleKeyUpEvent(int keyCode, long currTime) {
        keyCode = getSimplifiedKey(keyCode);

        // never trap back
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            return false;
        }

        // capture all key ups when voice is visible
        if (mContainer.isVoiceVisible()) {
            return true;
        }

        boolean handled = true;
        switch(keyCode) {
            // Some keys are handled on down to allow repeats
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
                clearKeyIfNecessary();
                break;
            case KeyEvent.KEYCODE_BUTTON_X:
            case KeyEvent.KEYCODE_BUTTON_Y:
            case KeyEvent.KEYCODE_BUTTON_L1:
            case KeyEvent.KEYCODE_BUTTON_R1:
                break;
            case KeyEvent.KEYCODE_DPAD_CENTER:
                if (mContainer.getCurrKeyCode() == Keyboard.KEYCODE_SHIFT) {
                    mDoubleClickDetector.addEvent(currTime);
                } else if (!isKeyHandledOnKeyDown(mContainer.getCurrKeyCode())) {
                    commitKey(mKeyDownKeyFocus);
                }
                break;
            case KeyEvent.KEYCODE_BUTTON_THUMBL:
                handleCommitKeyboardKey(Keyboard.KEYCODE_MODE_CHANGE, null);
                break;
            case KeyEvent.KEYCODE_BUTTON_THUMBR:
                handleCommitKeyboardKey(LeanbackKeyboardView.KEYCODE_CAPS_LOCK, null);
                break;
            case KeyEvent.KEYCODE_ENTER:
                if (mContainer != null) {
                    KeyFocus keyFocus = mContainer.getCurrFocus();
                    if (keyFocus != null && keyFocus.type ==  KeyFocus.TYPE_SUGGESTION) {
                        mInputListener.onEntry(InputListener.ENTRY_TYPE_SUGGESTION, 0,
                                mContainer.getSuggestionText(keyFocus.index));
                    }
                }
                mInputListener.onEntry(InputListener.ENTRY_TYPE_DISMISS, 0, null);
                break;
            default:
                handled = false;
                break;
        }
        return handled;
    }

    public void updateSuggestions(ArrayList<String> suggestions) {
        if (mContainer != null) {
            mContainer.updateSuggestions(suggestions);
        }
    }

    @Override
    public void onVoiceResult(String result) {
        mInputListener.onEntry(InputListener.ENTRY_TYPE_VOICE, 0, result);
    }

    @Override
    public void onDismiss(boolean fromVoice) {
        if (fromVoice) {
            mInputListener.onEntry(InputListener.ENTRY_TYPE_VOICE_DISMISS, 0, null);
        } else {
            mInputListener.onEntry(InputListener.ENTRY_TYPE_DISMISS, 0, null);
        }
    }

    private boolean isEnterKey(int keyCode) {
        return getSimplifiedKey(keyCode) == KeyEvent.KEYCODE_DPAD_CENTER;
    }

    private int getSimplifiedKey(int keyCode) {
        // simplify for dpad center
        keyCode = (keyCode == KeyEvent.KEYCODE_DPAD_CENTER ||
                keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER ||
                keyCode == KeyEvent.KEYCODE_BUTTON_A) ? KeyEvent.KEYCODE_DPAD_CENTER : keyCode;

        // simply for back
        keyCode = (keyCode == KeyEvent.KEYCODE_BUTTON_B ? KeyEvent.KEYCODE_BACK : keyCode);

        return keyCode;
    }
}
