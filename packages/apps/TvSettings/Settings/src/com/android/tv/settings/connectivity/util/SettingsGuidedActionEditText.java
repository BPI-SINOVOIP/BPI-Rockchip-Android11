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

package com.android.tv.settings.connectivity.util;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.EditText;
import android.widget.TextView;

import androidx.leanback.widget.ImeKeyMonitor;

/**
 * Mostly copied from {@GuidedActionEditText}, remove some code to satisfies TvSettings need.
 */
public class SettingsGuidedActionEditText extends EditText implements ImeKeyMonitor {
    private ImeKeyMonitor.ImeKeyListener mKeyListener;

    public SettingsGuidedActionEditText(Context context) {
        super(context);
    }

    public SettingsGuidedActionEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SettingsGuidedActionEditText(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    public void setImeKeyListener(ImeKeyMonitor.ImeKeyListener listener) {
        mKeyListener = listener;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        boolean result = false;
        if (mKeyListener != null) {
            result = mKeyListener.onKeyPreIme(this, keyCode, event);
        }
        if (!result) {
            result = super.onKeyPreIme(keyCode, event);
        }
        return result;
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        info.setClassName(isFocused() ? EditText.class.getName() : TextView.class.getName());
    }

    @Override
    protected void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(focused, direction, previouslyFocusedRect);
        // Make the TextView focusable during editing, avoid the TextView gets accessibility focus
        // before editing started. see also GuidedActionAdapterGroup where setFocusable(true).
        if (!focused) {
            setFocusable(false);
        }
    }
}
