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

package com.android.car.settings.datetime;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.widget.TimePicker;

import com.android.car.settings.R;
import com.android.car.settings.testutils.FragmentController;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.shadows.ShadowSettings;

/** Unit test for {@link TimePickerFragment}. */
@RunWith(RobolectricTestRunner.class)
public class TimePickerFragmentTest {

    private TimePickerFragment mFragment;
    private FragmentController<TimePickerFragment> mFragmentController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mFragment = new TimePickerFragment();
        mFragmentController = FragmentController.of(mFragment);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();
    }

    @Test
    public void onActivityCreated_isNot24HourFormat_timePickerShouldShow12HourTimeFormat() {
        ShadowSettings.set24HourTimeFormat(false);
        mFragmentController.create().start();
        TimePicker timePicker = findTimePicker(mFragment.requireActivity());

        assertThat(timePicker.is24HourView()).isFalse();
    }

    @Test
    public void onActivityCreated_is24HourFormat_timePickerShouldShow24HourTimeFormat() {
        ShadowSettings.set24HourTimeFormat(true);
        mFragmentController.create().start();
        TimePicker timePicker = findTimePicker(mFragment.requireActivity());

        assertThat(timePicker.is24HourView()).isTrue();
    }

    private TimePicker findTimePicker(Activity activity) {
        return activity.findViewById(R.id.time_picker);
    }
}
