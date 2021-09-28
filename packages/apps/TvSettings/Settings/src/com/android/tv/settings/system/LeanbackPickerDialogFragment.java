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
 * limitations under the License.
 */

package com.android.tv.settings.system;

import android.app.timedetector.ManualTimeSuggestion;
import android.app.timedetector.TimeDetector;
import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.leanback.preference.LeanbackPreferenceDialogFragment;
import androidx.leanback.preference.LeanbackSettingsFragment;
import androidx.leanback.widget.picker.DatePicker;
import androidx.leanback.widget.picker.Picker;
import androidx.leanback.widget.picker.TimePicker;
import androidx.preference.DialogPreference;

import com.android.tv.settings.R;
import com.android.tv.twopanelsettings.TwoPanelSettingsFragment;

import java.util.Calendar;

/**
 * A DialogFragment started for either setting date or setting time purposes. The type of
 * fragment launched is controlled by the type of {@link LeanbackPickerDialogPreference}
 * that's clicked. Launching of these two fragments is done inside
 * {@link com.android.tv.settings.BaseSettingsFragment#onPreferenceDisplayDialog}.
 */
public class LeanbackPickerDialogFragment extends LeanbackPreferenceDialogFragment implements
        TwoPanelSettingsFragment.NavigationCallback {

    private static final String EXTRA_PICKER_TYPE = "LeanbackPickerDialogFragment.PickerType";
    private static final String TYPE_DATE = "date";
    private static final String TYPE_TIME = "time";
    private static final String SAVE_STATE_TITLE = "LeanbackPickerDialogFragment.title";

    private CharSequence mDialogTitle;
    private Calendar mCalendar;
    private Picker mPicker;

    /**
     * Generated a new DialogFragment displaying a Leanback DatePicker widget.
     *
     * @param key The preference key starting this DialogFragment.
     * @return The fragment to be started displaying a DatePicker widget for setting date.
     */
    public static LeanbackPickerDialogFragment newDatePickerInstance(String key) {
        final Bundle args = new Bundle(1);
        args.putString(ARG_KEY, key);
        args.putString(EXTRA_PICKER_TYPE, TYPE_DATE);

        final LeanbackPickerDialogFragment fragment = new LeanbackPickerDialogFragment();
        fragment.setArguments(args);
        return fragment;
    }

    /**
     * Generated a new DialogFragment displaying a Leanback TimePicker widget.
     *
     * @param key The preference key starting this DialogFragment.
     * @return The fragment to be started displaying a TimePicker widget for setting time.
     */
    public static LeanbackPickerDialogFragment newTimePickerInstance(String key) {
        final Bundle args = new Bundle(1);
        args.putString(ARG_KEY, key);
        args.putString(EXTRA_PICKER_TYPE, TYPE_TIME);

        final LeanbackPickerDialogFragment fragment = new LeanbackPickerDialogFragment();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (savedInstanceState == null) {
            final DialogPreference preference = getPreference();
            mDialogTitle = preference.getDialogTitle();
        } else {
            mDialogTitle = savedInstanceState.getCharSequence(SAVE_STATE_TITLE);
        }
        mCalendar = Calendar.getInstance();
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putCharSequence(SAVE_STATE_TITLE, mDialogTitle);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        final String pickerType = getArguments().getString(EXTRA_PICKER_TYPE);

        final TypedValue tv = new TypedValue();
        getActivity().getTheme().resolveAttribute(R.attr.preferenceTheme, tv, true);
        int theme = tv.resourceId;
        if (theme == 0) {
            // Fallback to default theme.
            theme = R.style.PreferenceThemeOverlayLeanback;
        }
        Context styledContext = new ContextThemeWrapper(getActivity(), theme);
        LayoutInflater styledInflater = inflater.cloneInContext(styledContext);
        final View view = styledInflater.inflate(R.layout.picker_dialog_fragment, container, false);
        ViewGroup pickerContainer = view.findViewById(R.id.picker_container);
        if (pickerType.equals(TYPE_DATE)) {
            styledInflater.inflate(R.layout.date_picker_widget, pickerContainer, true);
            DatePicker datePicker = pickerContainer.findViewById(R.id.date_picker);
            mPicker = datePicker;
            if (getParentFragment() instanceof LeanbackSettingsFragment) {
                datePicker.setActivated(true);
            }
            datePicker.setOnClickListener(v -> {
                // Setting the new system date
                long whenMillis = datePicker.getDate();
                TimeDetector timeDetector = getContext().getSystemService(TimeDetector.class);
                ManualTimeSuggestion manualTimeSuggestion = TimeDetector.createManualTimeSuggestion(
                        whenMillis, "Settings: Set date");
                timeDetector.suggestManualTime(manualTimeSuggestion);
                // a) Two Panel: Navigate back, setActivated(false) in the callback
                // b) Side Panel: Finish the fragment/activity when clicked.
                if (getParentFragment() instanceof TwoPanelSettingsFragment) {
                    ((TwoPanelSettingsFragment) getParentFragment()).navigateBack();
                } else if (!getFragmentManager().popBackStackImmediate()) {
                    getActivity().finish();
                }
            });

        } else {
            styledInflater.inflate(R.layout.time_picker_widget, pickerContainer, true);
            TimePicker timePicker = pickerContainer.findViewById(R.id.time_picker);
            if (getParentFragment() instanceof LeanbackSettingsFragment) {
                timePicker.setActivated(true);
            }
            mPicker = timePicker;
            timePicker.setOnClickListener(v -> {
                // Setting the new system time
                mCalendar.set(Calendar.HOUR_OF_DAY, timePicker.getHour());
                mCalendar.set(Calendar.MINUTE, timePicker.getMinute());
                mCalendar.set(Calendar.SECOND, 0);
                mCalendar.set(Calendar.MILLISECOND, 0);
                long whenMillis = mCalendar.getTimeInMillis();

                TimeDetector timeDetector = getContext().getSystemService(TimeDetector.class);
                ManualTimeSuggestion manualTimeSuggestion = TimeDetector.createManualTimeSuggestion(
                        whenMillis, "Settings: Set time");
                timeDetector.suggestManualTime(manualTimeSuggestion);
                // a) Two Panel: Navigate back, setActivated(false) in the callback.
                // b) Side Panel: Finish the fragment/activity when clicked.
                if (getParentFragment() instanceof TwoPanelSettingsFragment) {
                    ((TwoPanelSettingsFragment) getParentFragment()).navigateBack();
                } else if (!getFragmentManager().popBackStackImmediate()) {
                    getActivity().finish();
                }
            });
        }

        final CharSequence title = mDialogTitle;
        if (!TextUtils.isEmpty(title)) {
            final TextView titleView = view.findViewById(R.id.decor_title);
            titleView.setText(title);
        }
        return view;
    }

    @Override
    public boolean canNavigateBackOnDPAD() {
        if (mPicker.isActivated() && mPicker.hasFocus()) {
            if (mPicker.getSelectedColumn() == 0) {
                return true;
            }
            return false;
        }
        return true;
    }

    @Override
    public void onNavigateToPreview() {
        mPicker.setActivated(true);
        mPicker.requestFocus();
    }

    @Override
    public void onNavigateBack() {
        mPicker.setActivated(false);
    }
}
