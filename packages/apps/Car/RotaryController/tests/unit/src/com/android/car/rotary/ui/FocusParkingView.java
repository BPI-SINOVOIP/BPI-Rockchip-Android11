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

package com.android.car.rotary.ui;

import static android.view.accessibility.AccessibilityNodeInfo.ACTION_FOCUS;
import android.content.Context;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import androidx.annotation.Nullable;

/**
 * A light version of FocusParkingView in CarUI Lib. It's used by
 * {@link com.android.car.rotary.RotaryService} to clear the focus highlight in the previous
 * window when moving focus to another window.
 * <p>
 * Unlike FocusParkingView in CarUI Lib, this view shouldn't be placed as the first focusable view
 * in the window. The general recommendation is to keep this as the last view in the layout.
 */
public class FocusParkingView extends View {
    /**
     * This value should not change, even if the actual package containing this class is different.
     */
    private static final String FOCUS_PARKING_VIEW_LITE_CLASS_NAME =
            "com.android.car.rotary.FocusParkingView";
    /** Action performed on this view to hide the IME. */
    private static final int ACTION_HIDE_IME = 0x08000000;
    public FocusParkingView(Context context) {
        super(context);
        init();
    }
    public FocusParkingView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    public FocusParkingView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }
    public FocusParkingView(Context context, @Nullable AttributeSet attrs, int defStyleAttr,
            int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init();
    }
    private void init() {
        // This view is focusable, visible and enabled so it can take focus.
        setFocusable(View.FOCUSABLE);
        setVisibility(VISIBLE);
        setEnabled(true);
        // This view is not clickable so it won't affect the app's behavior when the user clicks on
        // it by accident.
        setClickable(false);
        // This view is always transparent.
        setAlpha(0f);
        // Prevent Android from drawing the default focus highlight for this view when it's focused.
        setDefaultFocusHighlightEnabled(false);
    }
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // This size of the view is always 1 x 1 pixel, no matter what value is set in the layout
        // file (match_parent, wrap_content, 100dp, 0dp, etc). Small size is to ensure it has little
        // impact on the layout, non-zero size is to ensure it can take focus.
        setMeasuredDimension(1, 1);
    }
    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (!hasWindowFocus) {
            // We need to clear the focus highlight(by parking the focus on this view)
            // once the current window goes to background. This can't be done by RotaryService
            // because RotaryService sees the window as removed, thus can't perform any action
            // (such as focus, clear focus) on the nodes in the window. So this view has to
            // grab the focus proactively.
            super.requestFocus(FOCUS_DOWN, null);
        }
        super.onWindowFocusChanged(hasWindowFocus);
    }
    @Override
    public CharSequence getAccessibilityClassName() {
        return FOCUS_PARKING_VIEW_LITE_CLASS_NAME;
    }
    @Override
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        switch (action) {
            case ACTION_HIDE_IME:
                InputMethodManager inputMethodManager =
                        getContext().getSystemService(InputMethodManager.class);
                return inputMethodManager.hideSoftInputFromWindow(getWindowToken(),
                        /* flags= */ 0);
            case ACTION_FOCUS:
                // Don't leave this to View to handle as it will exit touch mode.
                if (!hasFocus()) {
                    return super.requestFocus(FOCUS_DOWN, null);
                }
                return false;
        }
        return super.performAccessibilityAction(action, arguments);
    }
}
