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

package com.android.car.notification.headsup;

import static android.view.accessibility.AccessibilityNodeInfo.ACTION_FOCUS;

import static com.android.car.ui.utils.RotaryConstants.ROTARY_FOCUS_DELEGATING_CONTAINER;

import android.content.Context;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.car.notification.R;
import com.android.car.ui.FocusArea;

/**
 * Container that is used to present heads-up notifications. It is responsible for delegating the
 * focus to the topmost notification and ensuring that new HUNs gains focus automatically when
 * one of the existing HUNs already has focus.
 */
public class HeadsUpContainerView extends FrameLayout {

    private Handler mHandler;
    private int mAnimationDuration;

    public HeadsUpContainerView(@NonNull Context context) {
        super(context);
        init();
    }

    public HeadsUpContainerView(@NonNull Context context,
            @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public HeadsUpContainerView(@NonNull Context context, @Nullable AttributeSet attrs,
            int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    public HeadsUpContainerView(@NonNull Context context, @Nullable AttributeSet attrs,
            int defStyleAttr,
            int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init();
    }

    private void init() {
        mHandler = new Handler(Looper.getMainLooper());
        mAnimationDuration = getResources().getInteger(R.integer.headsup_total_enter_duration_ms);

        // This tag is required to make this container receive the focus request in order to
        // delegate focus to its children, even though the container itself isn't focusable.
        setContentDescription(ROTARY_FOCUS_DELEGATING_CONTAINER);
        setClickable(false);
    }

    @Override
    protected boolean onRequestFocusInDescendants(int direction, Rect previouslyFocusedRect) {
        if (isInTouchMode()) {
            return super.onRequestFocusInDescendants(direction, previouslyFocusedRect);
        }
        return focusTopmostChild();
    }

    @Override
    public void addView(View child) {
        super.addView(child);

        if (!isInTouchMode() && getFocusedChild() != null) {
            // Request focus for the topmost child if one of the children is already focused.
            // Wait for the duration of the heads-up animation for a smoother UI experience.
            mHandler.postDelayed(() -> focusTopmostChild(), mAnimationDuration);
        }
    }

    private boolean focusTopmostChild() {
        int childCount = getChildCount();
        if (childCount <= 0) {
            return false;
        }

        View topmostChild = getChildAt(childCount - 1);
        if (!(topmostChild instanceof FocusArea)) {
            return false;
        }

        FocusArea focusArea = (FocusArea) topmostChild;
        View view = focusArea.findViewById(R.id.action_1);
        if (view != null) {
            focusArea.setDefaultFocus(view);
        }

        return topmostChild.performAccessibilityAction(ACTION_FOCUS, /* arguments= */ null);
    }
}
