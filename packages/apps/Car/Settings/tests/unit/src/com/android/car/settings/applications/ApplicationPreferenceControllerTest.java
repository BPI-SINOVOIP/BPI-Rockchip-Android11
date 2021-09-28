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

package com.android.car.settings.applications;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.UserHandle;

import androidx.lifecycle.LifecycleOwner;
import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceControllerTestUtil;
import com.android.car.settings.testutils.TestLifecycleOwner;
import com.android.settingslib.applications.ApplicationsState;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class ApplicationPreferenceControllerTest {

    private static final String PACKAGE_NAME = "Test Package Name";

    private Preference mPreference;
    private CarUxRestrictions mCarUxRestrictions;
    private ApplicationPreferenceController mController;

    @Mock
    private FragmentController mFragmentController;
    @Mock
    private ApplicationsState mMockAppState;
    @Mock
    private ApplicationsState.AppEntry mMockAppEntry;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Context context = ApplicationProvider.getApplicationContext();
        mMockAppEntry.label = PACKAGE_NAME;

        mCarUxRestrictions = new CarUxRestrictions.Builder(/* reqOpt= */ true,
                CarUxRestrictions.UX_RESTRICTIONS_BASELINE, /* timestamp= */ 0).build();

        mPreference = new Preference(context);
        mController = new ApplicationPreferenceController(context,
                /* preferenceKey= */ "key", mFragmentController, mCarUxRestrictions);

        when(mMockAppState.getEntry(PACKAGE_NAME, UserHandle.myUserId())).thenReturn(mMockAppEntry);
    }

    @Test
    public void testCheckInitialized_noAppState_throwException() {
        mController.setAppEntry(mMockAppEntry);
        assertThrows(IllegalStateException.class,
                () -> PreferenceControllerTestUtil.assignPreference(mController, mPreference));
    }

    @Test
    public void testCheckInitialized_noAppEntry_throwException() {
        mController.setAppState(mMockAppState);
        assertThrows(IllegalStateException.class,
                () -> PreferenceControllerTestUtil.assignPreference(mController, mPreference));
    }

    @Test
    public void testRefreshUi_hasResolveInfo_setTitle() {
        LifecycleOwner lifecycleOwner = new TestLifecycleOwner();
        mController.setAppEntry(mMockAppEntry);
        mController.setAppState(mMockAppState);
        PreferenceControllerTestUtil.assignPreference(mController, mPreference);
        mController.onCreate(lifecycleOwner);
        mController.refreshUi();
        assertThat(mPreference.getTitle()).isEqualTo(PACKAGE_NAME);
    }
}
