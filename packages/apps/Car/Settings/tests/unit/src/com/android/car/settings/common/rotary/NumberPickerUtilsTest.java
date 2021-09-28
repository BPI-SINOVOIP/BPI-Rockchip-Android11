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

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.DatePicker;
import android.widget.NumberPicker;
import android.widget.TimePicker;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.R;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class NumberPickerUtilsTest {

    private Context mContext = ApplicationProvider.getApplicationContext();

    @Test
    public void getNumberPickerDescendants_fromTimePicker_has3NumberPickers() {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        ViewGroup view = (ViewGroup) inflater.inflate(R.layout.time_picker, /* root= */ null);
        TimePicker timePicker = view.findViewById(R.id.time_picker);

        List<NumberPicker> numberPickers = new ArrayList<>();
        NumberPickerUtils.getNumberPickerDescendants(numberPickers, timePicker);

        assertThat(numberPickers).hasSize(3);
    }

    @Test
    public void getNumberPickerDescendants_fromDatePicker_has3NumberPickers() {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        ViewGroup view = (ViewGroup) inflater.inflate(R.layout.date_picker, /* root= */ null);
        DatePicker datePicker = view.findViewById(R.id.date_picker);

        List<NumberPicker> numberPickers = new ArrayList<>();
        NumberPickerUtils.getNumberPickerDescendants(numberPickers, datePicker);

        assertThat(numberPickers).hasSize(3);
    }

    @Test
    public void hasCommonNumberPickerParent_bothNull_returnsFalse() {
        boolean result = NumberPickerUtils.hasCommonNumberPickerParent(null, null);

        assertThat(result).isFalse();
    }

    @Test
    public void hasCommonNumberPickerParent_firstNull_returnsFalse() {
        NumberPicker picker = new NumberPicker(mContext);
        boolean result = NumberPickerUtils.hasCommonNumberPickerParent(null, picker);

        assertThat(result).isFalse();
    }

    @Test
    public void hasCommonNumberPickerParent_secondNull_returnsFalse() {
        NumberPicker picker = new NumberPicker(mContext);
        boolean result = NumberPickerUtils.hasCommonNumberPickerParent(picker, null);

        assertThat(result).isFalse();
    }

    @Test
    public void hasCommonNumberPickerParent_separateNumberPickers_returnsFalse() {
        NumberPicker picker1 = new NumberPicker(mContext);
        NumberPicker picker2 = new NumberPicker(mContext);
        boolean result = NumberPickerUtils.hasCommonNumberPickerParent(picker1, picker2);

        assertThat(result).isFalse();
    }

    @Test
    public void hasCommonNumberPickerParent_fromTimePicker_returnsTrue() {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        ViewGroup view = (ViewGroup) inflater.inflate(R.layout.time_picker, /* root= */ null);
        TimePicker timePicker = view.findViewById(R.id.time_picker);
        List<NumberPicker> numberPickers = new ArrayList<>();
        NumberPickerUtils.getNumberPickerDescendants(numberPickers, timePicker);
        NumberPicker picker1 = numberPickers.get(0);
        NumberPicker picker2 = numberPickers.get(1);

        boolean result = NumberPickerUtils.hasCommonNumberPickerParent(picker1, picker2);

        assertThat(result).isTrue();
    }

    @Test
    public void getNumberPickerParent_noValidParent_returnsNull() {
        NumberPicker picker = new NumberPicker(mContext);
        View result = NumberPickerUtils.getNumberPickerParent(picker);

        assertThat(result).isNull();
    }

    @Test
    public void getNumberPickerParent_validParent_returnsNull() {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        ViewGroup view = (ViewGroup) inflater.inflate(R.layout.time_picker, /* root= */ null);
        TimePicker timePicker = view.findViewById(R.id.time_picker);
        List<NumberPicker> numberPickers = new ArrayList<>();
        NumberPickerUtils.getNumberPickerDescendants(numberPickers, timePicker);
        NumberPicker picker = numberPickers.get(0);

        View result = NumberPickerUtils.getNumberPickerParent(picker);

        assertThat(result).isEqualTo(timePicker);
    }
}
