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
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.util.Log;
import android.view.Gravity;

import androidx.core.content.ContextCompat;

/**
 * Create two panel settings style icon
 */
public class IconUtil {
    private static final int INSET = 12;
    private static final String TAG = "IconUtil";

    /**
     * Add the border and return the compound icon.
     */
    public static Drawable getCompoundIcon(Context context, Drawable icon) {
        Drawable container =
                ContextCompat.getDrawable(context, R.drawable.compound_icon_background);

        Resources res = context.getResources();
        try {
            ColorStateList colorStateList = ColorStateList
                    .createFromXml(res, res.getXml(R.color.two_panel_preference_icon_color));
            icon.setTintList(colorStateList);
        } catch (Exception e) {
            Log.e(TAG, "Cannot set tint", e);

        }

        LayerDrawable compoundDrawable = new LayerDrawable(new Drawable[] {container, icon});
        compoundDrawable.setLayerGravity(0, Gravity.CENTER);
        compoundDrawable.setLayerGravity(1, Gravity.CENTER);
        compoundDrawable.setLayerInset(1, INSET, INSET, INSET, INSET);
        return compoundDrawable;
    }
}
