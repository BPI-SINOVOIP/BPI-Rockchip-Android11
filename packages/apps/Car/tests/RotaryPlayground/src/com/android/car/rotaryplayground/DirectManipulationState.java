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

package com.android.car.rotaryplayground;

import static android.view.ViewGroup.FOCUS_AFTER_DESCENDANTS;

import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.car.ui.utils.DirectManipulationHelper;

/**
 * Keeps track of the state of "direct manipulation" Rotary mode for this application window by
 * tracking a reference to the {@link View} from which the user first enters into "direct
 * manipulation" mode.
 *
 * <p>See {@link DirectManipulationHandler} for a definition of "direct manipulation".
 */
public class DirectManipulationState {

    /** Background color of a view when it's in direct manipulation mode. */
    private static final int BACKGROUND_COLOR_IN_DIRECT_MANIPULATION_MODE = Color.BLUE;

    /** Indicates that the descendant focusability has not been set. */
    private static final int UNKNOWN_DESCENDANT_FOCUSABILITY = -1;

    /** The view that is in direct manipulation mode, or null if none. */
    @Nullable
    private View mViewInDirectManipulationMode;
    /** The original background of the view in direct manipulation mode. */
    @Nullable
    private Drawable mOriginalBackground;
    /** The original descendant focusability value of the view in direct manipulation mode. */
    private int mOriginalDescendantFocusability = UNKNOWN_DESCENDANT_FOCUSABILITY;

    /**
     * Returns true if Direct Manipulation mode is active, false otherwise.
     */
    public boolean isActive() {
        return mViewInDirectManipulationMode != null;
    }

    /**
     * Enables Direct Manipulation mode, and keeps track of {@code view} as the starting point
     * of this transition.
     * <p>
     * We generally want to give some kind of visual indication that this change has happened. In
     * this example we change the background color of {@code view}.
     *
     * @param view the {@link View} from which we entered into Direct Manipulation mode
     */
    public void enable(@NonNull View view) {
        mViewInDirectManipulationMode = view;
        mOriginalBackground = view.getBackground();
        if (mViewInDirectManipulationMode instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) mViewInDirectManipulationMode;
            mOriginalDescendantFocusability = viewGroup.getDescendantFocusability();
            viewGroup.setDescendantFocusability(FOCUS_AFTER_DESCENDANTS);
        }
        view.setBackgroundColor(BACKGROUND_COLOR_IN_DIRECT_MANIPULATION_MODE);
        DirectManipulationHelper.enableDirectManipulationMode(view, /* enable= */ true);
    }

    /**
     * Disables Direct Manipulation mode and restores any visual indicators for the {@link View}
     * from which we entered into Direct Manipulation mode.
     */
    public void disable() {
        mViewInDirectManipulationMode.setBackground(mOriginalBackground);
        DirectManipulationHelper.enableDirectManipulationMode(
                mViewInDirectManipulationMode, /* enable= */ false);
        // For ViewGroup objects, restore descendant focusability to the previous value.
        if (mViewInDirectManipulationMode instanceof ViewGroup
                && mOriginalDescendantFocusability != UNKNOWN_DESCENDANT_FOCUSABILITY) {
            ViewGroup viewGroup = (ViewGroup) mViewInDirectManipulationMode;
            viewGroup.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        }

        mViewInDirectManipulationMode = null;
        mOriginalBackground = null;
        mOriginalDescendantFocusability = UNKNOWN_DESCENDANT_FOCUSABILITY;
    }
}
