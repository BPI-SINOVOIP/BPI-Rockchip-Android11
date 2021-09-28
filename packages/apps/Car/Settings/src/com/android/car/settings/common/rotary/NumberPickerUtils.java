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

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.DatePicker;
import android.widget.NumberPicker;
import android.widget.TimePicker;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** A helper class to deal with containers holding {@link NumberPicker} instances. */
public final class NumberPickerUtils {

    /** Allowed containers holding {@link NumberPicker} instances. */
    static final Set<Class> POSSIBLE_PARENT_TYPES = new HashSet<>(
            Arrays.asList(TimePicker.class, DatePicker.class));

    /** Focuses the first child {@link NumberPicker} if it exists. */
    public static boolean focusChildNumberPicker(View view) {
        ViewGroup numberPickerParent = (ViewGroup) view;
        List<NumberPicker> numberPickers = new ArrayList<>();
        getNumberPickerDescendants(numberPickers, numberPickerParent);
        if (numberPickers.isEmpty()) {
            return false;
        }
        numberPickerParent.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
        numberPickers.get(0).requestFocus();
        return true;
    }

    /**
     * We don't have API access to the inner views of a {@link TimePicker} or {@link DatePicker}.
     * We do know based on
     * {@code frameworks/base/core/res/res/layout/time_picker_legacy_material.xml} and
     * {@code frameworks/base/core/res/res/layout/date_picker_legacy.xml} that a {@link TimePicker}
     * or {@link DatePicker} that is in spinner mode will be using {@link NumberPicker} instances
     * internally, and that's what we rely on here.
     *
     * @param numberPickers the list to add all the {@link NumberPicker} instances found to
     * @param view the view to search for {@link NumberPicker} instances
     */
    public static void getNumberPickerDescendants(List<NumberPicker> numberPickers,
            ViewGroup view) {
        for (int i = 0; i < view.getChildCount(); i++) {
            View child = view.getChildAt(i);
            if (child instanceof NumberPicker) {
                numberPickers.add((NumberPicker) child);
            } else if (child instanceof ViewGroup) {
                getNumberPickerDescendants(numberPickers, (ViewGroup) child);
            }
        }
    }

    /**
     * Determines whether {@code view1} and {@code view2} have a common ancestor of one of the types
     * defined in {@code POSSIBLE_PARENT_TYPES}.
     */
    static boolean hasCommonNumberPickerParent(@Nullable View view1, @Nullable View view2) {
        if (view1 == null || view2 == null) {
            return false;
        }
        View view1Ancestor = getNumberPickerParent(view1);
        View view2Ancestor = getNumberPickerParent(view2);
        if (view1Ancestor == null || view2Ancestor == null) {
            return false;
        }

        return view1Ancestor == view2Ancestor;
    }

    /**
     * Finds the first parent that is of one of the types defined in {@code POSSIBLE_PARENT_TYPES}.
     * Returns {@code null} if no such parent exists.
     */
    @Nullable
    static View getNumberPickerParent(@Nullable View view) {
        if (view == null) {
            return null;
        }

        if (POSSIBLE_PARENT_TYPES.contains(view.getClass())) {
            return view;
        }

        ViewParent viewParent = view.getParent();
        if (viewParent instanceof View) {
            return getNumberPickerParent((View) viewParent);
        }
        return null;
    }
}
