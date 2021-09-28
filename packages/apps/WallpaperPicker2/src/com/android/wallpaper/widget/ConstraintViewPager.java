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

package com.android.wallpaper.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.viewpager.widget.ViewPager;

/**
 * When ConstraintViewPager is being measured, it will get all height of pages and makes itself
 * height as the same as the maximum height.
 */
public class ConstraintViewPager extends ViewPager {

    public ConstraintViewPager(@NonNull Context context) {
        this(context, null /* attrs */);
    }

    public ConstraintViewPager(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Visit all child views first and then determine the maximum height for ViewPager.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int maxChildHeight = 0;
        for (int i = 0; i < getChildCount(); i++) {
            View view = getChildAt(i);
            view.measure(widthMeasureSpec,
                    MeasureSpec.makeMeasureSpec(0 /* size */, MeasureSpec.UNSPECIFIED));
            int childHeight = view.getMeasuredHeight();
            if (childHeight > maxChildHeight) {
                maxChildHeight = childHeight;
            }
        }

        if (maxChildHeight != 0) {
            heightMeasureSpec = MeasureSpec.makeMeasureSpec(maxChildHeight, MeasureSpec.EXACTLY);
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }
}
