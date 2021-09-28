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

package com.android.car.developeroptions.slices;

import android.content.Context;
import android.util.AttributeSet;

import androidx.slice.Slice;
import androidx.slice.widget.SliceView;

import com.android.car.developeroptions.R;
import com.android.settingslib.widget.LayoutPreference;

/**
 * Preference for {@link SliceView}
 */
public class SlicePreference extends LayoutPreference {
    private SliceView mSliceView;

    public SlicePreference(Context context, AttributeSet attrs) {
        super(context, attrs, R.attr.slicePreferenceStyle);
        init();
    }

    public SlicePreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        mSliceView = findViewById(R.id.slice_view);
        mSliceView.showTitleItems(true);
        mSliceView.setScrollable(false);
    }

    public void onSliceUpdated(Slice slice) {
        mSliceView.onChanged(slice);
        notifyChanged();
    }
}
