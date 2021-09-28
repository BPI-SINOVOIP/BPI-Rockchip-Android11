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

package com.android.car.settings.common;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;

import androidx.lifecycle.LifecycleOwner;
import androidx.preference.Preference;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.ui.preference.CarUiPreference;
import com.android.car.ui.preference.UxRestrictablePreference;
import com.android.settingslib.core.lifecycle.Lifecycle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class PreferenceControllerTest {

    private static final CarUxRestrictions NO_SETUP_UX_RESTRICTIONS =
            new CarUxRestrictions.Builder(/* reqOpt= */ true,
                    CarUxRestrictions.UX_RESTRICTIONS_NO_SETUP, /* timestamp= */ 0).build();

    private static final CarUxRestrictions BASELINE_UX_RESTRICTIONS =
            new CarUxRestrictions.Builder(/* reqOpt= */ true,
                    CarUxRestrictions.UX_RESTRICTIONS_BASELINE, /* timestamp= */ 0).build();

    private LifecycleOwner mLifecycleOwner;
    private Lifecycle mLifecycle;

    private Context mContext = ApplicationProvider.getApplicationContext();
    private FakePreferenceController mPreferenceController;

    @Mock
    private FragmentController mFragmentController;
    @Mock
    private CarUiPreference mPreference;

    @Before
    @UiThreadTest
    public void setUp() {
        mLifecycleOwner = () -> mLifecycle;
        mLifecycle = new Lifecycle(mLifecycleOwner);

        MockitoAnnotations.initMocks(this);

        mPreferenceController = new FakePreferenceController(mContext, /* preferenceKey= */ "key",
                mFragmentController, BASELINE_UX_RESTRICTIONS);
    }

    @Test
    public void onUxRestrictionsChanged_restricted_preferenceRestricted() {
        mPreferenceController.setPreference(mPreference);
        mPreferenceController.onCreate(mLifecycleOwner);

        Mockito.reset(mPreference);
        mPreferenceController.onUxRestrictionsChanged(NO_SETUP_UX_RESTRICTIONS);

        verify((UxRestrictablePreference) mPreference).setUxRestricted(true);
    }

    @Test
    public void onUxRestrictionsChanged_restricted_viewOnly_preferenceUnrestricted() {
        mPreferenceController.setPreference(mPreference);
        mPreferenceController.setAvailabilityStatus(PreferenceController.AVAILABLE_FOR_VIEWING);
        mPreferenceController.onCreate(mLifecycleOwner);

        Mockito.reset(mPreference);
        mPreferenceController.onUxRestrictionsChanged(NO_SETUP_UX_RESTRICTIONS);

        verify((UxRestrictablePreference) mPreference).setUxRestricted(false);
    }

    @Test
    @UiThreadTest
    public void onUxRestrictionsChanged_restricted_preferenceGroup_preferencesRestricted() {
        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);
        CarUiPreference preference1 = mock(CarUiPreference.class);
        CarUiPreference preference2 = mock(CarUiPreference.class);
        preferenceScreen.addPreference(preference1);
        preferenceScreen.addPreference(preference2);

        mPreferenceController.setPreference(preferenceScreen);
        mPreferenceController.onCreate(mLifecycleOwner);

        Mockito.reset(preference1);
        Mockito.reset(preference2);
        mPreferenceController.onUxRestrictionsChanged(NO_SETUP_UX_RESTRICTIONS);

        verify((UxRestrictablePreference) preference1).setUxRestricted(true);
        verify((UxRestrictablePreference) preference2).setUxRestricted(true);
    }

    @Test
    public void onCreate_unrestricted_disabled_preferenceUnrestricted() {
        mPreference.setEnabled(false);
        mPreferenceController.setPreference(mPreference);
        mPreferenceController.onCreate(mLifecycleOwner);

        verify((UxRestrictablePreference) mPreference).setUxRestricted(false);
    }

    private static class FakePreferenceController extends
            PreferenceController<Preference> {

        private int mAvailabilityStatus;

        FakePreferenceController(Context context, String preferenceKey,
                FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
            super(context, preferenceKey, fragmentController, uxRestrictions);
            mAvailabilityStatus = AVAILABLE;
        }

        @Override
        protected Class<Preference> getPreferenceType() {
            return Preference.class;
        }

        @Override
        protected int getAvailabilityStatus() {
            return mAvailabilityStatus;
        }

        public void setAvailabilityStatus(int availabilityStatus) {
            mAvailabilityStatus = availabilityStatus;
        }
    }
}
