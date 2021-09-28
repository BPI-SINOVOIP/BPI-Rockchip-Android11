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

package com.android.car.media;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.constraintlayout.widget.ConstraintLayout;

import com.android.car.ui.baselayout.Insets;
import com.android.car.ui.baselayout.InsetsChangedListener;

import java.util.HashSet;
import java.util.Set;

/**
 * Applies the insets computed by the car-ui-lib to the spacer views in ui_guides.xml. This allows
 * the Media app to have different instances of the application bar.
 */
class GuidelinesUpdater implements InsetsChangedListener {

    private final View mGuidedView;
    private final Set<InsetsChangedListener> mListeners = new HashSet<>();

    public GuidelinesUpdater(View guidedView) {
        mGuidedView = guidedView;
    }

    public void addListener(InsetsChangedListener listener) {
        mListeners.add(listener);
    }

    // Read the results of the base layout measurements and adjust the guidelines to match
    public void onCarUiInsetsChanged(@NonNull Insets insets) {
        View startPad = mGuidedView.findViewById(R.id.ui_content_start_guideline);
        ConstraintLayout.LayoutParams start =
                (ConstraintLayout.LayoutParams)startPad.getLayoutParams();
        start.setMargins(insets.getLeft(), 0,0, 0);
        startPad.setLayoutParams(start);

        View endPad = mGuidedView.findViewById(R.id.ui_content_end_guideline);
        ConstraintLayout.LayoutParams end = (ConstraintLayout.LayoutParams)endPad.getLayoutParams();
        end.setMargins(0, 0, insets.getRight(), 0);
        endPad.setLayoutParams(end);

        View topPad = mGuidedView.findViewById(R.id.ui_content_top_guideline);
        ConstraintLayout.LayoutParams top = (ConstraintLayout.LayoutParams)topPad.getLayoutParams();
        top.setMargins(0, insets.getTop(), 0, 0);
        topPad.setLayoutParams(top);

        View bottomPad = mGuidedView.findViewById(R.id.ui_content_bottom_guideline);
        ConstraintLayout.LayoutParams bottom =
                (ConstraintLayout.LayoutParams)bottomPad.getLayoutParams();
        bottom.setMargins(0, 0, 0, insets.getBottom());
        bottomPad.setLayoutParams(bottom);

        for (InsetsChangedListener listener : mListeners) {
            listener.onCarUiInsetsChanged(insets);
        }
    }
}
