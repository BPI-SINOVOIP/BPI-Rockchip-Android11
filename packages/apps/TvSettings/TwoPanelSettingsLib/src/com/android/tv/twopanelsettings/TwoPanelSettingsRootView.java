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

package com.android.tv.twopanelsettings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

/**
 * This class provides the functionality to handle the keyEvent correctly for two-panel settings
 * layout.
 */
public class TwoPanelSettingsRootView extends FrameLayout {
    private OnKeyListener mOnBackKeyListener;

    public TwoPanelSettingsRootView(Context context) {
        super(context);
    }

    public TwoPanelSettingsRootView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public TwoPanelSettingsRootView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public void setOnBackKeyListener(OnKeyListener backKeyListener) {
        mOnBackKeyListener = backKeyListener;
    }

    @Override
    public boolean dispatchKeyEvent(@NonNull KeyEvent event) {
        boolean handled = false;
        if ((event.getKeyCode() == KeyEvent.KEYCODE_BACK
                || event.getKeyCode() == KeyEvent.KEYCODE_DPAD_UP
                || event.getKeyCode() == KeyEvent.KEYCODE_DPAD_DOWN
                || event.getKeyCode() == KeyEvent.KEYCODE_DPAD_LEFT
                || event.getKeyCode() == KeyEvent.KEYCODE_DPAD_RIGHT)
                && mOnBackKeyListener != null) {
            handled = mOnBackKeyListener.onKey(this, event.getKeyCode(), event);
        }
        return handled || super.dispatchKeyEvent(event);
    }
}
