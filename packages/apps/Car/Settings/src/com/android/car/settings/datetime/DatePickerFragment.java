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
 * limitations under the License
 */
package com.android.car.settings.datetime;

import android.app.timedetector.ManualTimeSuggestion;
import android.app.timedetector.TimeDetector;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Intent;
import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.DatePicker;
import android.widget.NumberPicker;

import androidx.annotation.LayoutRes;
import androidx.annotation.StringRes;

import com.android.car.settings.R;
import com.android.car.settings.common.BaseFragment;
import com.android.car.settings.common.rotary.DirectManipulationHandler;
import com.android.car.settings.common.rotary.DirectManipulationState;
import com.android.car.settings.common.rotary.NumberPickerNudgeHandler;
import com.android.car.settings.common.rotary.NumberPickerRotationHandler;
import com.android.car.settings.common.rotary.NumberPickerUtils;
import com.android.car.ui.toolbar.MenuItem;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.List;

/**
 * Sets the system date.
 */
public class DatePickerFragment extends BaseFragment {
    private static final int MILLIS_IN_SECOND = 1000;

    private DirectManipulationState mDirectManipulationMode;
    private DatePicker mDatePicker;
    private List<NumberPicker> mNumberPickers;
    private MenuItem mOkButton;

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mOkButton);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mOkButton = new MenuItem.Builder(getContext())
                .setTitle(android.R.string.ok)
                .setOnClickListener(i -> {
                    Calendar c = Calendar.getInstance();
                    c.set(Calendar.YEAR, mDatePicker.getYear());
                    c.set(Calendar.MONTH, mDatePicker.getMonth());
                    c.set(Calendar.DAY_OF_MONTH, mDatePicker.getDayOfMonth());
                    long when = Math.max(c.getTimeInMillis(), DatetimeSettingsFragment.MIN_DATE);
                    if (when / MILLIS_IN_SECOND < Integer.MAX_VALUE) {
                        TimeDetector timeDetector =
                                getContext().getSystemService(TimeDetector.class);
                        ManualTimeSuggestion manualTimeSuggestion =
                                TimeDetector.createManualTimeSuggestion(when, "Settings: Set date");
                        timeDetector.suggestManualTime(manualTimeSuggestion);
                        getContext().sendBroadcast(new Intent(Intent.ACTION_TIME_CHANGED));
                    }
                    getFragmentHost().goBack();
                })
                .build();
    }

    @Override
    @LayoutRes
    protected int getLayoutId() {
        return R.layout.date_picker;
    }

    @Override
    @StringRes
    protected int getTitleId() {
        return R.string.date_picker_title;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mDirectManipulationMode = new DirectManipulationState();
        mDatePicker = getView().findViewById(R.id.date_picker);
        mDatePicker.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        mNumberPickers = new ArrayList<>();
        NumberPickerUtils.getNumberPickerDescendants(mNumberPickers, mDatePicker);

        DirectManipulationHandler.setDirectManipulationHandler(mDatePicker,
                new DirectManipulationHandler.Builder(mDirectManipulationMode)
                        // Use no-op nudge handler, since we never stay on this view in direct
                        // manipulation mode.
                        .setNudgeHandler((v, keyCode, event) -> true)
                        .setCenterButtonHandler(inDirectManipulationMode -> {
                            if (inDirectManipulationMode) {
                                return true;
                            }

                            NumberPicker picker = mNumberPickers.get(0);
                            if (picker != null) {
                                picker.requestFocus();
                            }
                            return true;
                        })
                        .setBackHandler(inDirectManipulationMode -> {
                            // Only handle back if we weren't previously in direct manipulation
                            // mode.
                            if (!inDirectManipulationMode) {
                                onBackPressed();
                            }
                            return true;
                        })
                        .build());

        DirectManipulationHandler numberPickerListener =
                new DirectManipulationHandler.Builder(mDirectManipulationMode)
                        .setNudgeHandler(new NumberPickerNudgeHandler())
                        .setCenterButtonHandler(inDirectManipulationMode -> {
                            if (!inDirectManipulationMode) {
                                return true;
                            }

                            mDatePicker.requestFocus();
                            return true;
                        })
                        .setBackHandler(inDirectManipulationMode -> {
                            mDatePicker.requestFocus();
                            return true;
                        })
                        .setRotationHandler(new NumberPickerRotationHandler())
                        .build();

        for (int i = 0; i < mNumberPickers.size(); i++) {
            DirectManipulationHandler.setDirectManipulationHandler(mNumberPickers.get(i),
                    numberPickerListener);
        }
    }

    @Override
    public void onUxRestrictionsChanged(CarUxRestrictions restrictionInfo) {
        if (canBeShown(restrictionInfo)) {
            return;
        }
        if (mDirectManipulationMode != null && mDirectManipulationMode.isActive()) {
            mDirectManipulationMode.disable();
        }
    }

    @Override
    public void onDestroy() {
        DirectManipulationHandler.setDirectManipulationHandler(mDatePicker, /* handler= */ null);
        for (int i = 0; i < mNumberPickers.size(); i++) {
            DirectManipulationHandler.setDirectManipulationHandler(mNumberPickers.get(i),
                    /* handler= */ null);
        }

        super.onDestroy();
    }
}
