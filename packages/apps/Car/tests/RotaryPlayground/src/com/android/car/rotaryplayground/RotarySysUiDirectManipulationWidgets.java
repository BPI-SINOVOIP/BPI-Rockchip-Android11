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

import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.android.car.ui.utils.DirectManipulationHelper;

public class RotarySysUiDirectManipulationWidgets extends Fragment {

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.rotary_sys_ui_direct_manipulation, container, false);

        View directManipulationSupportedSeekBar = view.findViewById(
                R.id.direct_manipulation_supported_seek_bar);
        DirectManipulationHelper.setSupportsDirectManipulation(
                directManipulationSupportedSeekBar, true);

        View directManipulationUnsupportedSeekBar = view.findViewById(
                R.id.direct_manipulation_unsupported_seek_bar);
        directManipulationUnsupportedSeekBar.setOnClickListener(
                new ChangeBackgroundColorOnClickListener());

        View directManipulationSupportedRadialTimePickerView = view.findViewById(
                R.id.direct_manipulation_supported_radial_time_picker);
        DirectManipulationHelper.setSupportsDirectManipulation(
                directManipulationSupportedRadialTimePickerView, true);

        View directManipulationUnsupportedRadialTimePickerView = view.findViewById(
                R.id.direct_manipulation_unsupported_radial_time_picker);
        directManipulationUnsupportedRadialTimePickerView.setOnClickListener(
                new ChangeBackgroundColorOnClickListener());

        return view;
    }

    /**
     * An {@link android.view.View.OnClickListener} object that changes the background of the
     * view it is registered on to {@link Color#BLUE} and back to whatever it was before on
     * subsequent calls. This is just to demo doing something visually obvious in the UI.
     */
    private static class ChangeBackgroundColorOnClickListener implements View.OnClickListener {

        /** Just some random color to switch the background to for demo purposes. */
        private static final int ALTERNATIVE_BACKGROUND_COLOR = Color.BLUE;

        /**
         * The purpose of this boolean is to change color every other time {@link #onClick} is
         * called.
         */
        private boolean mFlipFlop = true;

        @Override
        public void onClick(View v) {
            Log.d(L.TAG,
                    "Handling onClick for " + v + " and going " + (mFlipFlop ? "flip" : "flop"));
            if (mFlipFlop) {
                Log.d(L.TAG, "Background about to be overwritten: " + v.getBackground());
                v.setTag(R.id.saved_background_tag, v.getBackground());
                v.setBackgroundColor(ALTERNATIVE_BACKGROUND_COLOR);
            } else {
                Object savedBackground = v.getTag(R.id.saved_background_tag);
                Log.d(L.TAG, "Restoring background: " + savedBackground);
                v.setBackground((Drawable) savedBackground);
            }
            // Flip mode bit.
            mFlipFlop = !mFlipFlop;
        }
    }
}