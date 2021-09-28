/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.tv.settings.dialog;

import android.animation.Animator;
import android.animation.AnimatorInflater;
import android.app.Dialog;
import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.OverScroller;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.IntDef;

import com.android.tv.settings.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Consumer;

public abstract class PinDialogFragment extends SafeDismissDialogFragment {
    private static final String TAG = PinDialogFragment.class.getSimpleName();
    private static final boolean DEBUG = false;

    protected static final String ARG_TYPE = "type";

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PIN_DIALOG_TYPE_UNLOCK_CHANNEL,
            PIN_DIALOG_TYPE_UNLOCK_PROGRAM,
            PIN_DIALOG_TYPE_ENTER_PIN,
            PIN_DIALOG_TYPE_NEW_PIN,
            PIN_DIALOG_TYPE_OLD_PIN,
            PIN_DIALOG_TYPE_DELETE_PIN})
    public @interface PinDialogType {}
    /**
     * PIN code dialog for unlock channel
     */
    public static final int PIN_DIALOG_TYPE_UNLOCK_CHANNEL = 0;

    /**
     * PIN code dialog for unlock content.
     * Only difference between {@code PIN_DIALOG_TYPE_UNLOCK_CHANNEL} is it's title.
     */
    public static final int PIN_DIALOG_TYPE_UNLOCK_PROGRAM = 1;

    /**
     * PIN code dialog for change parental control settings
     */
    public static final int PIN_DIALOG_TYPE_ENTER_PIN = 2;

    /**
     * PIN code dialog for set new PIN
     */
    public static final int PIN_DIALOG_TYPE_NEW_PIN = 3;

    // PIN code dialog for checking old PIN. This is intenal only.
    private static final int PIN_DIALOG_TYPE_OLD_PIN = 4;

    /**
     * PIN code dialog for deleting the PIN
     */
    public static final int PIN_DIALOG_TYPE_DELETE_PIN = 5;

    private static final int PIN_DIALOG_RESULT_SUCCESS = 0;
    private static final int PIN_DIALOG_RESULT_FAIL = 1;

    private static final int MAX_WRONG_PIN_COUNT = 5;
    private static final int WRONG_PIN_REFRESH_DELAY = 1000;
    private static final int DISABLE_PIN_DURATION_MILLIS = 60 * 1000; // 1 minute

    public interface ResultListener {
        void pinFragmentDone(int requestCode, boolean success);
    }

    public static final String DIALOG_TAG = PinDialogFragment.class.getName();

    private static final int NUMBER_PICKERS_RES_ID[] = {
            R.id.first, R.id.second, R.id.third, R.id.fourth };

    private int mType;
    private int mRetCode;

    private TextView mWrongPinView;
    private View mEnterPinView;
    private TextView mTitleView;
    private PinNumberPicker[] mPickers;
    private String mOriginalPin;
    private String mPrevPin;
    private int mWrongPinCount;
    private long mDisablePinUntil;
    private boolean mIsPinSet;
    private boolean mIsDispatched;

    private final Handler mHandler = new Handler();

    /**
     * Get the bad PIN retry time
     * @return Retry time
     */
    public abstract long getPinDisabledUntil();

    /**
     * Set the bad PIN retry time
     * @param retryDisableTimeout Retry time
     */
    public abstract void setPinDisabledUntil(long retryDisableTimeout);

    /**
     * Set PIN password for the profile
     * @param pin New PIN password
     * @param consumer Will be called with the success result from setting the pin
     */
    public abstract void setPin(String pin, String originalPin, Consumer<Boolean> consumer);

    /**
     * Delete PIN password for the profile
     * @param oldPin Old PIN password (required)
     * @param consumer Will be called with the success result from deleting the pin
     */
    public abstract void deletePin(String oldPin, Consumer<Boolean> consumer);

    /**
     * Validate PIN password for the profile
     * @param pin Password to check
     * @param consumer Will be called with the result of the check
     */
    public abstract void isPinCorrect(String pin, Consumer<Boolean> consumer);

    /**
     * Check if there is a PIN password set on the profile
     * @param consumer Will be called with the result of the check
     */
    public abstract void isPinSet(Consumer<Boolean> consumer);

    public PinDialogFragment() {
        mRetCode = PIN_DIALOG_RESULT_FAIL;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setStyle(STYLE_NO_TITLE, 0);
        mDisablePinUntil = getPinDisabledUntil();
        final Bundle args = getArguments();
        if (!args.containsKey(ARG_TYPE)) {
            throw new IllegalStateException("Fragment arguments must specify type");
        }
        mType = getArguments().getInt(ARG_TYPE);
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        Dialog dlg = super.onCreateDialog(savedInstanceState);
        dlg.getWindow().getAttributes().windowAnimations = R.style.pin_dialog_animation;
        PinNumberPicker.loadResources(dlg.getContext());
        return dlg;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        final View v = inflater.inflate(R.layout.pin_dialog, container, false);

        mWrongPinView = v.findViewById(R.id.wrong_pin);
        mEnterPinView = v.findViewById(R.id.enter_pin);
        if (mEnterPinView == null) {
            throw new IllegalStateException("R.id.enter_pin missing!");
        }
        mTitleView = mEnterPinView.findViewById(R.id.title);
        isPinSet(result -> dispatchOnIsPinSet(result, savedInstanceState, v));

        return v;
    }

    private void updateWrongPin() {
        if (getActivity() == null) {
            // The activity is already detached. No need to update.
            mHandler.removeCallbacks(null);
            return;
        }

        final long secondsLeft = (mDisablePinUntil - System.currentTimeMillis()) / 1000;
        final boolean enabled = secondsLeft < 1;
        if (enabled) {
            mWrongPinView.setVisibility(View.GONE);
            mEnterPinView.setVisibility(View.VISIBLE);
            mWrongPinCount = 0;
        } else {
            mEnterPinView.setVisibility(View.GONE);
            mWrongPinView.setVisibility(View.VISIBLE);
            mWrongPinView.setText(getResources().getString(R.string.pin_enter_wrong_seconds,
                    secondsLeft));
            mHandler.postDelayed(this::updateWrongPin, WRONG_PIN_REFRESH_DELAY);
        }
    }

    private void exit(int retCode) {
        mRetCode = retCode;
        mIsDispatched = false;
        dismiss();
    }

    private void handleWrongPin() {
        if (++mWrongPinCount >= MAX_WRONG_PIN_COUNT) {
            mDisablePinUntil = System.currentTimeMillis() + DISABLE_PIN_DURATION_MILLIS;
            setPinDisabledUntil(mDisablePinUntil);
            updateWrongPin();
        } else {
            showToast(R.string.pin_toast_wrong);
        }
    }

    private void showToast(int resId) {
        Toast.makeText(getActivity(), resId, Toast.LENGTH_SHORT).show();
    }

    private void done(String pin) {
        if (DEBUG) Log.d(TAG, "done: mType=" + mType + " pin=" + pin);
        if (mIsDispatched) {
            // avoid re-triggering any of the dispatch methods if the user
            // double clicks in the pin dialog
            return;
        }
        switch (mType) {
            case PIN_DIALOG_TYPE_UNLOCK_CHANNEL:
            case PIN_DIALOG_TYPE_UNLOCK_PROGRAM:
            case PIN_DIALOG_TYPE_ENTER_PIN:
                dispatchOnPinEntered(pin);
                break;
            case PIN_DIALOG_TYPE_DELETE_PIN:
                dispatchOnDeletePin(pin);
                break;
            case PIN_DIALOG_TYPE_NEW_PIN:
                dispatchOnNewPinTyped(pin);
                break;
            case PIN_DIALOG_TYPE_OLD_PIN:
                dispatchOnOldPinTyped(pin);
                break;
        }
    }

    private void dispatchOnPinEntered(String pin) {
        isPinCorrect(pin, pinCorrect -> {
            if (!mIsPinSet || pinCorrect) {
                exit(PIN_DIALOG_RESULT_SUCCESS);
            } else {
                resetPinInput();
                handleWrongPin();
            }
        });
    }

    private void dispatchOnDeletePin(String pin) {
        isPinCorrect(pin, pinIsCorrect -> {
            if (pinIsCorrect) {
                mIsDispatched = true;
                deletePin(pin, success -> {
                    exit(success ? PIN_DIALOG_RESULT_SUCCESS : PIN_DIALOG_RESULT_FAIL);
                });
            } else {
                resetPinInput();
                handleWrongPin();
            }
        });
    }

    private void dispatchOnNewPinTyped(String pin) {
        if (mPrevPin == null) {
            resetPinInput();
            mPrevPin = pin;
            mTitleView.setText(R.string.pin_enter_again);
        } else {
            if (pin.equals(mPrevPin)) {
                mIsDispatched = true;
                setPin(pin, mOriginalPin, success -> {
                    exit(PIN_DIALOG_RESULT_SUCCESS);
                });
            } else {
                resetPinInput();
                mTitleView.setText(R.string.pin_enter_new_pin);
                mPrevPin = null;
                showToast(R.string.pin_toast_not_match);
            }
        }
    }

    private void dispatchOnOldPinTyped(String pin) {
        resetPinInput();
        isPinCorrect(pin, pinIsCorrect -> {
            if (isAdded()) {
                if (pinIsCorrect) {
                    mOriginalPin = pin;
                    mType = PIN_DIALOG_TYPE_NEW_PIN;
                    mTitleView.setText(R.string.pin_enter_new_pin);
                } else {
                    handleWrongPin();
                }
            }
        });
    }

    public int getType() {
        return mType;
    }

    private void dispatchOnIsPinSet(Boolean result, Bundle savedInstanceState, View v) {
        mIsPinSet = result;
        if (!mIsPinSet) {
            // If PIN isn't set, user should set a PIN.
            // Successfully setting a new set is considered as entering correct PIN.
            mType = PIN_DIALOG_TYPE_NEW_PIN;
        }

        mEnterPinView.setVisibility(View.VISIBLE);
        setDialogTitle();
        setUpPinNumberPicker(savedInstanceState, v);
    }

    private void setDialogTitle() {
        switch (mType) {
            case PIN_DIALOG_TYPE_UNLOCK_CHANNEL:
                mTitleView.setText(R.string.pin_enter_unlock_channel);
                break;
            case PIN_DIALOG_TYPE_UNLOCK_PROGRAM:
                mTitleView.setText(R.string.pin_enter_unlock_program);
                break;
            case PIN_DIALOG_TYPE_ENTER_PIN:
            case PIN_DIALOG_TYPE_DELETE_PIN:
                mTitleView.setText(R.string.pin_enter_pin);
                break;
            case PIN_DIALOG_TYPE_NEW_PIN:
                if (!mIsPinSet) {
                    mTitleView.setText(R.string.pin_enter_new_pin);
                } else {
                    mTitleView.setText(R.string.pin_enter_old_pin);
                    mType = PIN_DIALOG_TYPE_OLD_PIN;
                }
        }
    }

    private void setUpPinNumberPicker(Bundle savedInstanceState, View v) {
        if (mType != PIN_DIALOG_TYPE_NEW_PIN) {
            updateWrongPin();
        }

        mPickers = new PinNumberPicker[NUMBER_PICKERS_RES_ID.length];
        for (int i = 0; i < NUMBER_PICKERS_RES_ID.length; i++) {
            mPickers[i] = v.findViewById(NUMBER_PICKERS_RES_ID[i]);
            mPickers[i].setValueRange(0, 9);
            mPickers[i].setPinDialogFragment(this);
            mPickers[i].updateFocus();
        }
        for (int i = 0; i < NUMBER_PICKERS_RES_ID.length - 1; i++) {
            mPickers[i].setNextNumberPicker(mPickers[i + 1]);
        }

        if (savedInstanceState == null) {
            mPickers[0].requestFocus();
        }
    }

    private String getPinInput() {
        String result = "";
        try {
            for (PinNumberPicker pnp : mPickers) {
                pnp.updateText();
                result += pnp.getValue();
            }
        } catch (IllegalStateException e) {
            result = "";
        }
        return result;
    }

    private void resetPinInput() {
        for (PinNumberPicker pnp : mPickers) {
            pnp.setValueRange(0, 9);
        }
        mPickers[0].requestFocus();
    }

    public static final class PinNumberPicker extends FrameLayout {
        private static final int NUMBER_VIEWS_RES_ID[] = {
            R.id.previous2_number,
            R.id.previous_number,
            R.id.current_number,
            R.id.next_number,
            R.id.next2_number };
        private static final int CURRENT_NUMBER_VIEW_INDEX = 2;

        private static Animator sFocusedNumberEnterAnimator;
        private static Animator sFocusedNumberExitAnimator;
        private static Animator sAdjacentNumberEnterAnimator;
        private static Animator sAdjacentNumberExitAnimator;

        private static float sAlphaForFocusedNumber;
        private static float sAlphaForAdjacentNumber;

        private int mMinValue;
        private int mMaxValue;
        private int mCurrentValue;
        private int mNextValue;
        private final int mNumberViewHeight;
        private PinDialogFragment mDialog;
        private PinNumberPicker mNextNumberPicker;
        private boolean mCancelAnimation;

        private final View mNumberViewHolder;
        private final View mBackgroundView;
        private final TextView[] mNumberViews;
        private final OverScroller mScroller;

        public PinNumberPicker(Context context) {
            this(context, null);
        }

        public PinNumberPicker(Context context, AttributeSet attrs) {
            this(context, attrs, 0);
        }

        public PinNumberPicker(Context context, AttributeSet attrs, int defStyleAttr) {
            this(context, attrs, defStyleAttr, 0);
        }

        public PinNumberPicker(Context context, AttributeSet attrs, int defStyleAttr,
                int defStyleRes) {
            super(context, attrs, defStyleAttr, defStyleRes);
            View view = inflate(context, R.layout.pin_number_picker, this);
            mNumberViewHolder = view.findViewById(R.id.number_view_holder);
            if (mNumberViewHolder == null) {
                throw new IllegalStateException("R.id.number_view_holder missing!");
            }
            mBackgroundView = view.findViewById(R.id.focused_background);
            mNumberViews = new TextView[NUMBER_VIEWS_RES_ID.length];
            for (int i = 0; i < NUMBER_VIEWS_RES_ID.length; ++i) {
                mNumberViews[i] = view.findViewById(NUMBER_VIEWS_RES_ID[i]);
            }
            Resources resources = context.getResources();
            mNumberViewHeight = resources.getDimensionPixelOffset(
                    R.dimen.pin_number_picker_text_view_height);

            mScroller = new OverScroller(context);

            mNumberViewHolder.setOnFocusChangeListener((v, hasFocus) -> updateFocus());

            mNumberViewHolder.setOnKeyListener((v, keyCode, event) -> {
                if (event.getAction() == KeyEvent.ACTION_DOWN) {
                    switch (keyCode) {
                        case KeyEvent.KEYCODE_DPAD_UP:
                        case KeyEvent.KEYCODE_DPAD_DOWN: {
                            if (!mScroller.isFinished() || mCancelAnimation) {
                                endScrollAnimation();
                            }
                            if (mScroller.isFinished() || mCancelAnimation) {
                                mCancelAnimation = false;
                                if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) {
                                    mNextValue = adjustValueInValidRange(mCurrentValue + 1);
                                    startScrollAnimation(true);
                                    mScroller.startScroll(0, 0, 0, mNumberViewHeight,
                                            getResources().getInteger(
                                                    R.integer.pin_number_scroll_duration));
                                } else {
                                    mNextValue = adjustValueInValidRange(mCurrentValue - 1);
                                    startScrollAnimation(false);
                                    mScroller.startScroll(0, 0, 0, -mNumberViewHeight,
                                            getResources().getInteger(
                                                    R.integer.pin_number_scroll_duration));
                                }
                                updateText();
                                invalidate();
                            }
                            return true;
                        }
                    }
                } else if (event.getAction() == KeyEvent.ACTION_UP) {
                    switch (keyCode) {
                        case KeyEvent.KEYCODE_DPAD_UP:
                        case KeyEvent.KEYCODE_DPAD_DOWN: {
                            mCancelAnimation = true;
                            return true;
                        }
                    }
                }
                return false;
            });
            mNumberViewHolder.setScrollY(mNumberViewHeight);
        }

        static void loadResources(Context context) {
            if (sFocusedNumberEnterAnimator == null) {
                TypedValue outValue = new TypedValue();
                context.getResources().getValue(
                        R.dimen.pin_alpha_for_focused_number, outValue, true);
                sAlphaForFocusedNumber = outValue.getFloat();
                context.getResources().getValue(
                        R.dimen.pin_alpha_for_adjacent_number, outValue, true);
                sAlphaForAdjacentNumber = outValue.getFloat();

                sFocusedNumberEnterAnimator = AnimatorInflater.loadAnimator(context,
                        R.animator.pin_focused_number_enter);
                sFocusedNumberExitAnimator = AnimatorInflater.loadAnimator(context,
                        R.animator.pin_focused_number_exit);
                sAdjacentNumberEnterAnimator = AnimatorInflater.loadAnimator(context,
                        R.animator.pin_adjacent_number_enter);
                sAdjacentNumberExitAnimator = AnimatorInflater.loadAnimator(context,
                        R.animator.pin_adjacent_number_exit);
            }
        }

        @Override
        public void computeScroll() {
            super.computeScroll();
            if (mScroller.computeScrollOffset()) {
                mNumberViewHolder.setScrollY(mScroller.getCurrY() + mNumberViewHeight);
                updateText();
                invalidate();
            } else if (mCurrentValue != mNextValue) {
                mCurrentValue = mNextValue;
            }
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_UP) {
                int keyCode = event.getKeyCode();
                if (keyCode >= KeyEvent.KEYCODE_0 && keyCode <= KeyEvent.KEYCODE_9) {
                    jumpNextValue(keyCode - KeyEvent.KEYCODE_0);
                } else if (keyCode != KeyEvent.KEYCODE_DPAD_CENTER
                        && keyCode != KeyEvent.KEYCODE_ENTER) {
                    return super.dispatchKeyEvent(event);
                }
                if (mNextNumberPicker == null) {
                    String pin = mDialog.getPinInput();
                    if (!TextUtils.isEmpty(pin)) {
                        mDialog.done(pin);
                    }
                } else {
                    mNextNumberPicker.requestFocus();
                }
                return true;
            }
            return super.dispatchKeyEvent(event);
        }

        @Override
        public void setEnabled(boolean enabled) {
            super.setEnabled(enabled);
            mNumberViewHolder.setFocusable(enabled);
            for (int i = 0; i < NUMBER_VIEWS_RES_ID.length; ++i) {
                mNumberViews[i].setEnabled(enabled);
            }
        }

        void startScrollAnimation(boolean scrollUp) {
            if (scrollUp) {
                sAdjacentNumberExitAnimator.setTarget(mNumberViews[1]);
                sFocusedNumberExitAnimator.setTarget(mNumberViews[2]);
                sFocusedNumberEnterAnimator.setTarget(mNumberViews[3]);
                sAdjacentNumberEnterAnimator.setTarget(mNumberViews[4]);
            } else {
                sAdjacentNumberEnterAnimator.setTarget(mNumberViews[0]);
                sFocusedNumberEnterAnimator.setTarget(mNumberViews[1]);
                sFocusedNumberExitAnimator.setTarget(mNumberViews[2]);
                sAdjacentNumberExitAnimator.setTarget(mNumberViews[3]);
            }
            sAdjacentNumberExitAnimator.start();
            sFocusedNumberExitAnimator.start();
            sFocusedNumberEnterAnimator.start();
            sAdjacentNumberEnterAnimator.start();
        }

        void endScrollAnimation() {
            sAdjacentNumberExitAnimator.end();
            sFocusedNumberExitAnimator.end();
            sFocusedNumberEnterAnimator.end();
            sAdjacentNumberEnterAnimator.end();
            mCurrentValue = mNextValue;
            mNumberViews[1].setAlpha(sAlphaForAdjacentNumber);
            mNumberViews[2].setAlpha(sAlphaForFocusedNumber);
            mNumberViews[3].setAlpha(sAlphaForAdjacentNumber);
        }

        void setValueRange(int min, int max) {
            if (min > max) {
                throw new IllegalArgumentException(
                        "The min value should be greater than or equal to the max value");
            }
            mMinValue = min;
            mMaxValue = max;
            mNextValue = mCurrentValue = mMinValue - 1;
            clearText();
            mNumberViews[CURRENT_NUMBER_VIEW_INDEX].setText("—");
        }

        void setPinDialogFragment(PinDialogFragment dlg) {
            mDialog = dlg;
        }

        void setNextNumberPicker(PinNumberPicker picker) {
            mNextNumberPicker = picker;
        }

        int getValue() {
            if (mCurrentValue < mMinValue || mCurrentValue > mMaxValue) {
                throw new IllegalStateException("Value is not set");
            }
            return mCurrentValue;
        }

        void jumpNextValue(int value) {
            if (value < mMinValue || value > mMaxValue) {
                throw new IllegalStateException("Value is not set");
            }
            mNextValue = mCurrentValue = adjustValueInValidRange(value);
            updateText();
        }

        void updateFocus() {
            endScrollAnimation();
            if (mNumberViewHolder.isFocused()) {
                mBackgroundView.setVisibility(View.VISIBLE);
                updateText();
            } else {
                mBackgroundView.setVisibility(View.GONE);
                if (!mScroller.isFinished()) {
                    mCurrentValue = mNextValue;
                    mScroller.abortAnimation();
                }
                clearText();
                mNumberViewHolder.setScrollY(mNumberViewHeight);
            }
        }

        private void clearText() {
            for (int i = 0; i < NUMBER_VIEWS_RES_ID.length; ++i) {
                if (i != CURRENT_NUMBER_VIEW_INDEX) {
                    mNumberViews[i].setText("");
                } else if (mCurrentValue >= mMinValue && mCurrentValue <= mMaxValue) {
                    // Bullet
                    mNumberViews[i].setText("\u2022");
                }
            }
        }

        private void updateText() {
            if (mNumberViewHolder.isFocused()) {
                if (mCurrentValue < mMinValue || mCurrentValue > mMaxValue) {
                    mNextValue = mCurrentValue = mMinValue;
                }
                int value = adjustValueInValidRange(mCurrentValue - CURRENT_NUMBER_VIEW_INDEX);
                for (int i = 0; i < NUMBER_VIEWS_RES_ID.length; ++i) {
                    mNumberViews[i].setText(String.valueOf(adjustValueInValidRange(value)));
                    value = adjustValueInValidRange(value + 1);
                }
            }
        }

        private int adjustValueInValidRange(int value) {
            int interval = mMaxValue - mMinValue + 1;
            if (value < mMinValue - interval || value > mMaxValue + interval) {
                throw new IllegalArgumentException("The value( " + value
                        + ") is too small or too big to adjust");
            }
            return (value < mMinValue) ? value + interval
                    : (value > mMaxValue) ? value - interval : value;
        }
    }
}
