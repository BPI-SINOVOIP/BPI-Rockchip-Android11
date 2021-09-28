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

package com.android.car.settings.common.rotary;

import static android.view.ViewGroup.FOCUS_AFTER_DESCENDANTS;
import static android.view.ViewGroup.FOCUS_BLOCK_DESCENDANTS;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.NumberPicker;
import android.widget.TimePicker;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.R;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class DirectManipulationStateTest {

    private Context mContext = ApplicationProvider.getApplicationContext();
    private DirectManipulationState mDirectManipulationState;
    private TimePicker mTimePicker;
    private NumberPicker mNumberPicker;

    @Before
    public void setUp() {
        mDirectManipulationState = new DirectManipulationState();

        LayoutInflater inflater = LayoutInflater.from(mContext);
        ViewGroup view = (ViewGroup) inflater.inflate(R.layout.time_picker, /* root= */ null);
        mTimePicker = view.findViewById(R.id.time_picker);
        mTimePicker.setDescendantFocusability(FOCUS_AFTER_DESCENDANTS);

        List<NumberPicker> numberPickers = new ArrayList<>();
        NumberPickerUtils.getNumberPickerDescendants(numberPickers, mTimePicker);
        mNumberPicker = numberPickers.get(0);
        mNumberPicker.setDescendantFocusability(FOCUS_BLOCK_DESCENDANTS);
    }

    @Test
    public void enable_timePicker_storesDescendantFocusability() {
        mDirectManipulationState.enable(mTimePicker);

        assertThat(mDirectManipulationState.getOriginalDescendantFocusability())
                .isEqualTo(FOCUS_AFTER_DESCENDANTS);
    }

    @Test
    public void enable_timePicker_storesViewInDirectManipulation() {
        mDirectManipulationState.enable(mTimePicker);

        assertThat(mDirectManipulationState.getViewInDirectManipulationMode())
                .isEqualTo(mTimePicker);
    }

    @Test
    public void enable_timePickerChild_descendantFocusabilityUpdates() {
        mDirectManipulationState.enable(mTimePicker);
        mDirectManipulationState.enable(mNumberPicker);

        assertThat(mDirectManipulationState.getOriginalDescendantFocusability())
                .isEqualTo(FOCUS_BLOCK_DESCENDANTS);
    }

    @Test
    public void enable_timePickerChild_viewInDirectManipulationUpdates() {
        mDirectManipulationState.enable(mTimePicker);
        mDirectManipulationState.enable(mNumberPicker);

        assertThat(mDirectManipulationState.getViewInDirectManipulationMode())
                .isEqualTo(mNumberPicker);
    }

    @Test
    public void disable_viewInDirectManipulationReset() {
        mDirectManipulationState.enable(mTimePicker);
        mDirectManipulationState.disable();

        assertThat(mDirectManipulationState.getViewInDirectManipulationMode()).isNull();
    }

    @Test
    public void disable_descendantFocusabilityReset() {
        mDirectManipulationState.enable(mTimePicker);
        mDirectManipulationState.disable();

        assertThat(mDirectManipulationState.getOriginalDescendantFocusability())
                .isEqualTo(DirectManipulationState.UNKNOWN_DESCENDANT_FOCUSABILITY);
    }
}

