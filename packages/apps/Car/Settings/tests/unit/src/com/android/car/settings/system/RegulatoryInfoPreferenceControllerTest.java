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

package com.android.car.settings.system;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class RegulatoryInfoPreferenceControllerTest {

    private Context mContext = ApplicationProvider.getApplicationContext();
    private RegulatoryInfoPreferenceController mPreferenceController;
    private CarUxRestrictions mCarUxRestrictions;

    @Mock
    private FragmentController mFragmentController;
    @Mock
    private PackageManager mPm;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mCarUxRestrictions = new CarUxRestrictions.Builder(/* reqOpt= */ true,
                CarUxRestrictions.UX_RESTRICTIONS_BASELINE, /* timestamp= */ 0).build();

        mPreferenceController = new RegulatoryInfoPreferenceController(mContext,
                /* preferenceKey= */ "key", mFragmentController, mCarUxRestrictions);
        mPreferenceController.setPackageManager(mPm);
    }

    @Test
    public void hasIntent_isAvailable() {
        List<ResolveInfo> activities = new ArrayList<>();
        activities.add(new ResolveInfo());
        when(mPm.queryIntentActivities(any(Intent.class), eq(0)))
                .thenReturn(activities);

        assertThat(mPreferenceController.getAvailabilityStatus()).isEqualTo(
                PreferenceController.AVAILABLE);
    }

    @Test
    public void hasNoIntent_isNotAvailable() {
        List<ResolveInfo> activities = new ArrayList<>();
        when(mPm.queryIntentActivities(any(Intent.class), eq(0)))
                .thenReturn(activities);

        assertThat(mPreferenceController.getAvailabilityStatus()).isEqualTo(
                PreferenceController.UNSUPPORTED_ON_DEVICE);
    }
}
