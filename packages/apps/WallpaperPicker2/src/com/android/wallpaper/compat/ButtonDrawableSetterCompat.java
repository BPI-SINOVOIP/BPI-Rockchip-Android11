/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.wallpaper.compat;

import android.graphics.drawable.Drawable;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.widget.Button;

/**
 * Compatibility class for adding Drawables to Buttons.
 */
public class ButtonDrawableSetterCompat {

    /**
     * Sets a Drawable to the start of a Button on API 17+ and to the left of a Button on older
     * versions where RTL is not supported.
     */
    public static void setDrawableToButtonStart(Button button, Drawable icon) {
        // RTL method support was added in API 17.
        if (VERSION.SDK_INT >= VERSION_CODES.JELLY_BEAN_MR1) {
            button.setCompoundDrawablesRelativeWithIntrinsicBounds(icon, null, null, null);
        } else {
            // Fall back to "left" icon placement if RTL is not supported.
            button.setCompoundDrawablesWithIntrinsicBounds(icon, null, null, null);
        }
    }
}
