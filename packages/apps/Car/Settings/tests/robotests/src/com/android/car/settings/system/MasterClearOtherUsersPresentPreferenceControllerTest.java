/*
 * Copyright 2019 The Android Open Source Project
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

import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.UserInfo;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowUserHelper;
import com.android.car.settings.users.UserHelper;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.Collections;

/** Unit test for {@link MasterClearOtherUsersPresentPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserHelper.class})
public class MasterClearOtherUsersPresentPreferenceControllerTest {

    @Mock
    private UserHelper mUserHelper;
    private Preference mPreference;
    private MasterClearOtherUsersPresentPreferenceController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowUserHelper.setInstance(mUserHelper);

        Context context = RuntimeEnvironment.application;
        mPreference = new Preference(context);
        PreferenceControllerTestHelper<MasterClearOtherUsersPresentPreferenceController>
                controllerHelper = new PreferenceControllerTestHelper<>(context,
                MasterClearOtherUsersPresentPreferenceController.class, mPreference);
        controllerHelper.markState(Lifecycle.State.STARTED);
        mController = controllerHelper.getController();
    }

    @After
    public void tearDown() {
        ShadowUserHelper.reset();
    }

    @Test
    public void refreshUi_noSwitchableUsers_hidesPreference() {
        when(mUserHelper.getAllSwitchableUsers()).thenReturn(Collections.emptyList());

        mController.refreshUi();

        assertThat(mPreference.isVisible()).isFalse();
    }

    @Test
    public void refreshUi_switchableUsers_showsPreference() {
        when(mUserHelper.getAllSwitchableUsers()).thenReturn(
                Collections.singletonList(new UserInfo()));

        mController.refreshUi();

        assertThat(mPreference.isVisible()).isTrue();
    }
}
