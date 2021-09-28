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
 * limitations under the License.
 */

package com.android.tv.twopanelsettings.slices;

import static android.app.slice.Slice.EXTRA_TOGGLE_STATE;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.preference.TwoStatePreference;

import com.android.tv.twopanelsettings.R;

/**
 * An embedded slice switch preference which would be embedded in common TvSettings preference
 * items, but will automatically update its status and communicates with external apps through
 * slice api.
 */
public class EmbeddedSliceSwitchPreference extends SliceSwitchPreference {
    private static final String TAG = "EmbeddedSliceSwitchPreference";
    private final EmbeddedSlicePreferenceHelper mHelper;
    private String mUri;

    @Override
    public void onAttached() {
        super.onAttached();
        mHelper.onAttached();
    }

    @Override
    public void onDetached() {
        super.onDetached();
        mHelper.onDetached();
    }

    public EmbeddedSliceSwitchPreference(Context context) {
        super(context);
        init(null);
        mHelper = new EmbeddedSlicePreferenceHelper(this, mUri);
    }

    public EmbeddedSliceSwitchPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(attrs);
        mHelper = new EmbeddedSlicePreferenceHelper(this, mUri);
    }

    private void init(@Nullable AttributeSet attrs) {
        if (attrs != null) {
            initStyleAttributes(attrs);
        }
    }

    private void initStyleAttributes(AttributeSet attrs) {
        final TypedArray a = getContext().obtainStyledAttributes(
                attrs, R.styleable.SlicePreference);
        for (int i = a.getIndexCount() - 1; i >= 0; i--) {
            int attr = a.getIndex(i);
            if (attr == R.styleable.SlicePreference_uri) {
                mUri = a.getString(attr);
                break;
            }
        }
    }

    public void addListener(EmbeddedSlicePreferenceHelper.SlicePreferenceListener listener) {
        mHelper.mListener = listener;
    }

    public void removeListener(EmbeddedSlicePreferenceHelper.SlicePreferenceListener listener) {
        mHelper.mListener = null;
    }

    void update() {
        setTitle(mHelper.mNewPref.getTitle());
        setSummary(mHelper.mNewPref.getSummary());
        setIcon(mHelper.mNewPref.getIcon());
        if (mHelper.mNewPref instanceof TwoStatePreference) {
            setChecked(((TwoStatePreference) mHelper.mNewPref).isChecked());
        }
        if (mHelper.mNewPref instanceof HasSliceAction) {
            setSliceAction(((HasSliceAction) mHelper.mNewPref).getSliceAction());
        }
        setVisible(true);
    }

    @Override
    public void onClick() {
        boolean newValue = !isChecked();
        try {
            if (mAction == null) {
                return;
            }
            if (mAction.isToggle()) {
                // Update the intent extra state
                Intent i = new Intent().putExtra(EXTRA_TOGGLE_STATE, newValue);
                mAction.getActionItem().fireAction(getContext(), i);
            } else {
                mAction.getActionItem().fireAction(null, null);
            }
        } catch (PendingIntent.CanceledException e) {
            newValue = !newValue;
            Log.e(TAG, "PendingIntent for slice cannot be sent", e);
        }
        if (callChangeListener(newValue)) {
            setChecked(newValue);
        }
    }
}
