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

package com.android.car.settings.common;

import android.content.Context;
import android.content.res.TypedArray;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import com.android.car.settings.R;
import com.android.car.ui.preference.CarUiPreference;
import com.android.car.ui.utils.DirectManipulationHelper;

/**
 * Car Setting's own version of SeekBarPreference.
 *
 * The code is directly taken from androidx.preference.SeekBarPreference. However it has 1 main
 * functionality difference. There is a new field which can enable continuous updates while the
 * seek bar value is changing. This can be set programmatically by using the {@link
 * #setContinuousUpdate() setContinuousUpdate} method.
 */
public class SeekBarPreference extends CarUiPreference {

    private int mSeekBarValue;
    private int mMin;
    private int mMax;
    private int mSeekBarIncrement;
    private boolean mTrackingTouch;
    private SeekBar mSeekBar;
    private TextView mSeekBarValueTextView;
    private boolean mAdjustable; // whether the seekbar should respond to the left/right keys
    private boolean mShowSeekBarValue; // whether to show the seekbar value TextView next to the bar
    private boolean mContinuousUpdate; // whether scrolling provides continuous calls to listener
    private boolean mInDirectManipulationMode;

    private static final String TAG = "SeekBarPreference";

    /**
     * Listener reacting to the SeekBar changing value by the user
     */
    private final SeekBar.OnSeekBarChangeListener mSeekBarChangeListener =
            new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    if (fromUser && (mContinuousUpdate || !mTrackingTouch)) {
                        syncValueInternal(seekBar);
                    }
                }

                @Override
                public void onStartTrackingTouch(SeekBar seekBar) {
                    mTrackingTouch = true;
                }

                @Override
                public void onStopTrackingTouch(SeekBar seekBar) {
                    mTrackingTouch = false;
                    if (seekBar.getProgress() + mMin != mSeekBarValue) {
                        syncValueInternal(seekBar);
                    }
                }
            };

    /**
     * Listener reacting to the user pressing DPAD left/right keys if {@code
     * adjustable} attribute is set to true; it transfers the key presses to the SeekBar
     * to be handled accordingly. Also handles entering and exiting direct manipulation
     * mode for rotary.
     */
    private final View.OnKeyListener mSeekBarKeyListener = new View.OnKeyListener() {
        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            // Don't allow events through if there is no SeekBar or we're in non-adjustable mode.
            if (mSeekBar == null || !mAdjustable) {
                return false;
            }

            // Consume nudge events in direct manipulation mode.
            if (mInDirectManipulationMode
                    && (keyCode == KeyEvent.KEYCODE_DPAD_LEFT
                    || keyCode == KeyEvent.KEYCODE_DPAD_RIGHT
                    || keyCode == KeyEvent.KEYCODE_DPAD_UP
                    || keyCode == KeyEvent.KEYCODE_DPAD_DOWN)) {
                return true;
            }

            // Handle events to enter or exit direct manipulation mode.
            if (keyCode == KeyEvent.KEYCODE_DPAD_CENTER) {
                if (event.getAction() == KeyEvent.ACTION_DOWN) {
                    setInDirectManipulationMode(v, !mInDirectManipulationMode);
                }
                return true;
            }
            if (keyCode == KeyEvent.KEYCODE_BACK) {
                if (mInDirectManipulationMode) {
                    if (event.getAction() == KeyEvent.ACTION_DOWN) {
                        setInDirectManipulationMode(v, false);
                    }
                    return true;
                }
            }

            // Don't propagate confirm keys to the SeekBar to prevent a ripple effect on the thumb.
            if (KeyEvent.isConfirmKey(keyCode)) {
                return false;
            }

            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                return mSeekBar.onKeyDown(keyCode, event);
            } else {
                return mSeekBar.onKeyUp(keyCode, event);
            }
        }
    };

    /** Listener to exit rotary direct manipulation mode when the user switches to touch. */
    private final View.OnFocusChangeListener mSeekBarFocusChangeListener =
            (v, hasFocus) -> {
                if (!hasFocus && mInDirectManipulationMode && mSeekBar != null) {
                    setInDirectManipulationMode(v, false);
                }
            };

    /** Listener to handle rotate events from the rotary controller in direct manipulation mode. */
    private final View.OnGenericMotionListener mSeekBarScrollListener = (v, event) -> {
        if (!mInDirectManipulationMode || !mAdjustable || mSeekBar == null) {
            return false;
        }
        int adjustment = Math.round(event.getAxisValue(MotionEvent.AXIS_SCROLL));
        if (adjustment == 0) {
            return false;
        }
        int count = Math.abs(adjustment);
        int keyCode = adjustment < 0 ? KeyEvent.KEYCODE_DPAD_LEFT : KeyEvent.KEYCODE_DPAD_RIGHT;
        KeyEvent downEvent = new KeyEvent(event.getDownTime(), event.getEventTime(),
                KeyEvent.ACTION_DOWN, keyCode, /* repeat= */ 0);
        KeyEvent upEvent = new KeyEvent(event.getDownTime(), event.getEventTime(),
                KeyEvent.ACTION_UP, keyCode, /* repeat= */ 0);
        for (int i = 0; i < count; i++) {
            mSeekBar.onKeyDown(keyCode, downEvent);
            mSeekBar.onKeyUp(keyCode, upEvent);
        }
        return true;
    };

    public SeekBarPreference(
            Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);

        TypedArray a = context.obtainStyledAttributes(
                attrs, R.styleable.SeekBarPreference, defStyleAttr, defStyleRes);

        /**
         * The ordering of these two statements are important. If we want to set max first, we need
         * to perform the same steps by changing min/max to max/min as following:
         * mMax = a.getInt(...) and setMin(...).
         */
        mMin = a.getInt(R.styleable.SeekBarPreference_min, 0);
        setMax(a.getInt(R.styleable.SeekBarPreference_android_max, 100));
        setSeekBarIncrement(a.getInt(R.styleable.SeekBarPreference_seekBarIncrement, 0));
        mAdjustable = a.getBoolean(R.styleable.SeekBarPreference_adjustable, true);
        mShowSeekBarValue = a.getBoolean(R.styleable.SeekBarPreference_showSeekBarValue, true);
        a.recycle();
    }

    public SeekBarPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, 0);
    }

    public SeekBarPreference(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.seekBarPreferenceStyle);
    }

    public SeekBarPreference(Context context) {
        this(context, null);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder view) {
        super.onBindViewHolder(view);
        view.itemView.setOnKeyListener(mSeekBarKeyListener);
        view.itemView.setOnFocusChangeListener(mSeekBarFocusChangeListener);
        view.itemView.setOnGenericMotionListener(mSeekBarScrollListener);
        mSeekBar = (SeekBar) view.findViewById(R.id.seekbar);
        mSeekBarValueTextView = (TextView) view.findViewById(R.id.seekbar_value);
        if (mShowSeekBarValue) {
            mSeekBarValueTextView.setVisibility(View.VISIBLE);
        } else {
            mSeekBarValueTextView.setVisibility(View.GONE);
            mSeekBarValueTextView = null;
        }

        if (mSeekBar == null) {
            Log.e(TAG, "SeekBar view is null in onBindViewHolder.");
            return;
        }
        mSeekBar.setOnSeekBarChangeListener(mSeekBarChangeListener);
        mSeekBar.setMax(mMax - mMin);
        // If the increment is not zero, use that. Otherwise, use the default mKeyProgressIncrement
        // in AbsSeekBar when it's zero. This default increment value is set by AbsSeekBar
        // after calling setMax. That's why it's important to call setKeyProgressIncrement after
        // calling setMax() since setMax() can change the increment value.
        if (mSeekBarIncrement != 0) {
            mSeekBar.setKeyProgressIncrement(mSeekBarIncrement);
        } else {
            mSeekBarIncrement = mSeekBar.getKeyProgressIncrement();
        }

        mSeekBar.setProgress(mSeekBarValue - mMin);
        if (mSeekBarValueTextView != null) {
            mSeekBarValueTextView.setText(String.valueOf(mSeekBarValue));
        }
        mSeekBar.setEnabled(isEnabled());
    }

    @Override
    protected void onSetInitialValue(boolean restoreValue, Object defaultValue) {
        setValue(restoreValue ? getPersistedInt(mSeekBarValue)
                : (Integer) defaultValue);
    }

    @Override
    protected Object onGetDefaultValue(TypedArray a, int index) {
        return a.getInt(index, 0);
    }

    /** Setter for the minimum value allowed on seek bar. */
    public void setMin(int min) {
        if (min > mMax) {
            min = mMax;
        }
        if (min != mMin) {
            mMin = min;
            notifyChanged();
        }
    }

    /** Getter for the minimum value allowed on seek bar. */
    public int getMin() {
        return mMin;
    }

    /** Setter for the maximum value allowed on seek bar. */
    public final void setMax(int max) {
        if (max < mMin) {
            max = mMin;
        }
        if (max != mMax) {
            mMax = max;
            notifyChanged();
        }
    }

    /**
     * Returns the amount of increment change via each arrow key click. This value is derived
     * from
     * user's specified increment value if it's not zero. Otherwise, the default value is picked
     * from the default mKeyProgressIncrement value in {@link android.widget.AbsSeekBar}.
     *
     * @return The amount of increment on the SeekBar performed after each user's arrow key press.
     */
    public final int getSeekBarIncrement() {
        return mSeekBarIncrement;
    }

    /**
     * Sets the increment amount on the SeekBar for each arrow key press.
     *
     * @param seekBarIncrement The amount to increment or decrement when the user presses an
     *                         arrow key.
     */
    public final void setSeekBarIncrement(int seekBarIncrement) {
        if (seekBarIncrement != mSeekBarIncrement) {
            mSeekBarIncrement = Math.min(mMax - mMin, Math.abs(seekBarIncrement));
            notifyChanged();
        }
    }

    /** Getter for the maximum value allowed on seek bar. */
    public int getMax() {
        return mMax;
    }

    /** Setter for the functionality which allows for changing the values via keyboard arrows. */
    public void setAdjustable(boolean adjustable) {
        mAdjustable = adjustable;
    }

    /** Getter for the functionality which allows for changing the values via keyboard arrows. */
    public boolean isAdjustable() {
        return mAdjustable;
    }

    /** Setter for the functionality which allows for continuous triggering of listener code. */
    public void setContinuousUpdate(boolean continuousUpdate) {
        mContinuousUpdate = continuousUpdate;
    }

    /** Setter for the whether the text should be visible. */
    public void setShowSeekBarValue(boolean showSeekBarValue) {
        mShowSeekBarValue = showSeekBarValue;
    }

    /** Setter for the current value of the seek bar. */
    public void setValue(int seekBarValue) {
        setValueInternal(seekBarValue, true);
    }

    private void setValueInternal(int seekBarValue, boolean notifyChanged) {
        if (seekBarValue < mMin) {
            seekBarValue = mMin;
        }
        if (seekBarValue > mMax) {
            seekBarValue = mMax;
        }

        if (seekBarValue != mSeekBarValue) {
            mSeekBarValue = seekBarValue;
            if (mSeekBarValueTextView != null) {
                mSeekBarValueTextView.setText(String.valueOf(mSeekBarValue));
            }
            persistInt(seekBarValue);
            if (notifyChanged) {
                notifyChanged();
            }
        }
    }

    /** Getter for the current value of the seek bar. */
    public int getValue() {
        return mSeekBarValue;
    }

    /**
     * Persist the seekBar's seekbar value if callChangeListener
     * returns true, otherwise set the seekBar's value to the stored value
     */
    private void syncValueInternal(SeekBar seekBar) {
        int seekBarValue = mMin + seekBar.getProgress();
        if (seekBarValue != mSeekBarValue) {
            if (callChangeListener(seekBarValue)) {
                setValueInternal(seekBarValue, false);
            } else {
                seekBar.setProgress(mSeekBarValue - mMin);
            }
        }
    }

    private void setInDirectManipulationMode(View view, boolean enable) {
        mInDirectManipulationMode = enable;
        DirectManipulationHelper.enableDirectManipulationMode(mSeekBar, enable);
        // The preference is highlighted when it's focused with one exception. In direct
        // manipulation (DM) mode, the SeekBar's thumb is highlighted instead. In DM mode, the
        // preference and SeekBar are selected. The preference's highlight is drawn when it's
        // focused but not selected, while the SeekBar's thumb highlight is drawn when the SeekBar
        // is selected.
        view.setSelected(enable);
        mSeekBar.setSelected(enable);
    }

    @Override
    protected Parcelable onSaveInstanceState() {
        final Parcelable superState = super.onSaveInstanceState();
        if (isPersistent()) {
            // No need to save instance state since it's persistent
            return superState;
        }

        // Save the instance state
        final SeekBarPreference.SavedState myState = new SeekBarPreference.SavedState(superState);
        myState.mSeekBarValue = mSeekBarValue;
        myState.mMin = mMin;
        myState.mMax = mMax;
        return myState;
    }

    @Override
    protected void onRestoreInstanceState(Parcelable state) {
        if (!state.getClass().equals(SeekBarPreference.SavedState.class)) {
            // Didn't save state for us in onSaveInstanceState
            super.onRestoreInstanceState(state);
            return;
        }

        // Restore the instance state
        SeekBarPreference.SavedState myState = (SeekBarPreference.SavedState) state;
        super.onRestoreInstanceState(myState.getSuperState());
        mSeekBarValue = myState.mSeekBarValue;
        mMin = myState.mMin;
        mMax = myState.mMax;
        notifyChanged();
    }

    /**
     * SavedState, a subclass of {@link BaseSavedState}, will store the state
     * of MyPreference, a subclass of Preference.
     * <p>
     * It is important to always call through to super methods.
     */
    private static class SavedState extends BaseSavedState {
        int mSeekBarValue;
        int mMin;
        int mMax;

        SavedState(Parcel source) {
            super(source);

            // Restore the click counter
            mSeekBarValue = source.readInt();
            mMin = source.readInt();
            mMax = source.readInt();
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            super.writeToParcel(dest, flags);

            // Save the click counter
            dest.writeInt(mSeekBarValue);
            dest.writeInt(mMin);
            dest.writeInt(mMax);
        }

        SavedState(Parcelable superState) {
            super(superState);
        }

        @SuppressWarnings("unused")
        public static final Parcelable.Creator<SeekBarPreference.SavedState> CREATOR =
                new Parcelable.Creator<SeekBarPreference.SavedState>() {
                    @Override
                    public SeekBarPreference.SavedState createFromParcel(Parcel in) {
                        return new SeekBarPreference.SavedState(in);
                    }

                    @Override
                    public SeekBarPreference.SavedState[] newArray(int size) {
                        return new SeekBarPreference
                                .SavedState[size];
                    }
                };
    }
}
