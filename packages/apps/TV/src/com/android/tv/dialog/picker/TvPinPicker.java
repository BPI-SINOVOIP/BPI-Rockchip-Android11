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
package com.android.tv.dialog.picker;

import static android.content.Context.ACCESSIBILITY_SERVICE;

import android.content.Context;
import androidx.leanback.widget.picker.PinPicker;
import android.util.AttributeSet;
import android.view.accessibility.AccessibilityManager;

/** 4 digit picker */
public final class TvPinPicker extends PinPicker {

    private boolean mSkipPerformClick = true;
    private boolean mIsAccessibilityEnabled = false;

    public TvPinPicker(Context context, AttributeSet attributeSet) {
        this(context, attributeSet, 0);
    }

    public TvPinPicker(Context context, AttributeSet attributeSet, int defStyleAttr) {
        super(context, attributeSet, defStyleAttr);
        setActivated(true);
        AccessibilityManager am =
                (AccessibilityManager) context.getSystemService(ACCESSIBILITY_SERVICE);
        mIsAccessibilityEnabled = am.isEnabled();
    }

    @Override
    public boolean performClick() {
        // (b/120096347) Skip first click when talkback is enabled
        if (mSkipPerformClick && mIsAccessibilityEnabled) {
            mSkipPerformClick = false;
            /* Force focus to next value */
            setColumnValue(getSelectedColumn(), 1, true);
            return false;
        }
        return super.performClick();
    }
}
