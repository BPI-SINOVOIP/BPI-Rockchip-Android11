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

package com.android.tv.twopanelsettings.slices;

import android.content.Context;
import android.util.AttributeSet;

/**
 * An embedded slice preference which would be embedded in common TvSettings preference
 * items, but will automatically update its status and communicates with external apps through
 * slice api.
 */
public class EmbeddedSlicePreference extends SlicePreference {
    private static final String TAG = "EmbeddedSlicePreference";
    private final EmbeddedSlicePreferenceHelper mHelper;

    public EmbeddedSlicePreference(Context context, String uri) {
        super(context);
        mHelper = new EmbeddedSlicePreferenceHelper(this, getUri());
    }

    public EmbeddedSlicePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mHelper = new EmbeddedSlicePreferenceHelper(this, getUri());
    }

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
        if (mHelper.mNewPref instanceof HasSliceAction) {
            setIntent(((HasSliceAction) mHelper.mNewPref).getSliceAction().getAction().getIntent());
        }
    }
}
