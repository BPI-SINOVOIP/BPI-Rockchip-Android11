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

package com.android.inputmethod.leanback.voice;

import com.android.inputmethod.leanback.LeanbackUtils;
import com.android.inputmethod.leanback.R;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.RelativeLayout;

/**
 * Displays the recognizer status.
 * This mView includes a {@link BitmapSoundLevelsView} to display the recording value.
 */
public class RecognizerView extends RelativeLayout {
    private static final boolean DEBUG = false;
    private static final String TAG = "RecognizerView";

    private BitmapSoundLevelView mSoundLevels;
    protected ImageView mMicButton;

    private Callback mCallback;

    private State mState;

    private boolean mEnabled;

    private enum State {
        NOT_LISTENING,
        MIC_INITIALIZING,
        LISTENING,
        RECORDING,
        RECOGNIZING,
    }

    public RecognizerView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    public RecognizerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public RecognizerView(Context context) {
        super(context);
    }

    @Override
    public void onFinishInflate() {
        LayoutInflater inflater = LayoutInflater.from(getContext());
        inflater.inflate(R.layout.recognizer_view, this, true);

        mSoundLevels = (BitmapSoundLevelView) findViewById(R.id.microphone);
        mMicButton = (ImageView) findViewById(R.id.recognizer_mic_button);

        mState = State.NOT_LISTENING;
    }

    public View getMicButton() {
        return mMicButton;
    }

    public void onClick() {
        if (DEBUG) Log.v(TAG, "onClick " + mState);
        switch (mState) {
            case MIC_INITIALIZING:
                if (DEBUG)
                    Log.d(TAG, "Ignore #onClick as mic is initializing");
                return;
            case LISTENING:
                mCallback.onCancelRecordingClicked();
                break;
            case RECORDING:
                mCallback.onStopRecordingClicked();
                break;
            case RECOGNIZING:
                mCallback.onCancelRecordingClicked();
                break;
            case NOT_LISTENING:
                mCallback.onStartRecordingClicked();
                break;
            default:
                return;
        }
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        // When the mView is attached to a window, a callback has to be already set.
        //
        // This isn't true when this mView is used in the intent API layout. When the user hits
        // retry, we reattach the layout to the window, which is being shown. onAttachedToWindow( )
        // will be called from inside setContentView( ) and before we can attach a callback to it.
        // We ensure that the callback is not used when not set.
        //
        // Preconditions.checkNotNull(mCallback);

        // The callbacks from the microphone happen before the mView is attached to the window.
        // I need to investigate how to change the code to avoid it.
        refreshUi();
    }

    public void showRecording() {
        updateState(State.RECORDING);
    }

    public void showListening() {
        updateState(State.LISTENING);
    }

    public void showNotListening() {
        updateState(State.NOT_LISTENING);
    }

    public void showRecognizing() {
        updateState(State.RECOGNIZING);
    }

    public void setCallback(final Callback callback) {
        mCallback = callback;
    }

    private void updateState(State newState) {
        if (DEBUG) Log.d(TAG, mState + " -> " + newState);
        mState = newState;
        refreshUi();
    }

    public void setSpeechLevelSource(SpeechLevelSource source) {
        mSoundLevels.setLevelSource(source);
    }

    @Override
    public Parcelable onSaveInstanceState() {
        SavedState ss = new SavedState(super.onSaveInstanceState());
        ss.mState = mState;
        return ss;
    }

    @Override
    public void onRestoreInstanceState(Parcelable state) {
        if (!(state instanceof SavedState)) {
            super.onRestoreInstanceState(state);
            return;
        }

        SavedState ss = (SavedState) state;
        super.onRestoreInstanceState(ss.getSuperState());

        mState = ss.mState;
    }

    protected void refreshUi() {
        if (!mEnabled) {
            return;
        }
        switch (mState) {
            case MIC_INITIALIZING:
                mMicButton.setImageResource(R.drawable.vs_micbtn_on_selector);
                mSoundLevels.setEnabled(false);
                break;
            case LISTENING:
                mMicButton.setImageResource(R.drawable.vs_micbtn_on_selector);
                mSoundLevels.setEnabled(true);
                break;
            case RECORDING:
                mMicButton.setImageResource(R.drawable.vs_micbtn_rec_selector);
                mSoundLevels.setEnabled(true);
                break;
            case NOT_LISTENING:
                mMicButton.setImageResource(R.drawable.vs_micbtn_off_selector);
                mSoundLevels.setEnabled(false);
                break;
            case RECOGNIZING:
                mMicButton.setImageResource(R.drawable.vs_micbtn_off_selector);
                mSoundLevels.setEnabled(false);
                break;
        }
    }

    public void setMicFocused(boolean hasFocus) {
        if (mEnabled) {
            if (hasFocus) {
                mMicButton.setImageResource(R.drawable.ic_voice_focus);
            } else {
                mMicButton.setImageResource(R.drawable.ic_voice_available);
            }

            LeanbackUtils.sendAccessibilityEvent(mMicButton, hasFocus);
        }
    }

    public void setMicEnabled(boolean enabled) {
        mEnabled = enabled;
        if (enabled) {
            mMicButton.setAlpha(1.0f);
            mMicButton.setImageResource(R.drawable.ic_voice_available);
        } else {
            mMicButton.setAlpha(0.1f);
            mMicButton.setImageResource(R.drawable.ic_voice_off);
        }
    }

    public void showInitializingMic() {
        updateState(State.MIC_INITIALIZING);
    }

    public interface Callback {
        void onStartRecordingClicked();
        void onStopRecordingClicked();
        void onCancelRecordingClicked();
    }

    public static class SavedState extends View.BaseSavedState {
        State mState;

        public SavedState(Parcelable superState) {
            super(superState);
        }

        @Override
        public void writeToParcel(Parcel out, int flags) {
            super.writeToParcel(out, flags);
            out.writeString(mState.toString());
        }

        @SuppressWarnings("hiding")
        public static final Parcelable.Creator<SavedState> CREATOR
        = new Parcelable.Creator<SavedState>() {

            @Override
            public SavedState createFromParcel(Parcel in) {
                return new SavedState(in);
            }

            @Override
            public SavedState[] newArray(int size) {
                return new SavedState[size];
            }
        };

        private SavedState(Parcel in) {
            super(in);
            mState = State.valueOf(in.readString());
        }
    }
}
