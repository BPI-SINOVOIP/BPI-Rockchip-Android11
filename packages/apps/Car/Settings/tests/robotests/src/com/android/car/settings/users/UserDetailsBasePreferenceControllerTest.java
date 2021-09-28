/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.settings.users;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.content.pm.UserInfo;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowUserIconProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.Collections;
import java.util.List;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserIconProvider.class})
public class UserDetailsBasePreferenceControllerTest {

    private static class TestUserDetailsBasePreferenceController extends
            UserDetailsBasePreferenceController<Preference> {

        TestUserDetailsBasePreferenceController(Context context, String preferenceKey,
                FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
            super(context, preferenceKey, fragmentController, uxRestrictions);
        }

        @Override
        protected Class<Preference> getPreferenceType() {
            return Preference.class;
        }
    }

    private static final List<String> LISTENER_ACTIONS =
            Collections.singletonList(Intent.ACTION_USER_INFO_CHANGED);

    private PreferenceControllerTestHelper<TestUserDetailsBasePreferenceController>
            mPreferenceControllerHelper;
    private TestUserDetailsBasePreferenceController mController;
    private Preference mPreference;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Context context = RuntimeEnvironment.application;
        mPreferenceControllerHelper = new PreferenceControllerTestHelper<>(context,
                TestUserDetailsBasePreferenceController.class);
        mController = mPreferenceControllerHelper.getController();
        mPreference = new Preference(context);
    }

    @Test
    public void testCheckInitialized_missingUserInfo() {
        assertThrows(() -> mPreferenceControllerHelper.setPreference(mPreference));
    }

    @Test
    public void testOnCreate_registerListener() {
        mController.setUserInfo(new UserInfo());
        mPreferenceControllerHelper.setPreference(mPreference);
        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        assertThat(BroadcastReceiverHelpers.getRegisteredReceiverWithActions(LISTENER_ACTIONS))
                .isNotNull();
    }

    @Test
    public void testOnDestroy_unregisterListener() {
        mController.setUserInfo(new UserInfo());
        mPreferenceControllerHelper.setPreference(mPreference);
        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_DESTROY);

        assertThat(BroadcastReceiverHelpers.getRegisteredReceiverWithActions(LISTENER_ACTIONS))
                .isNull();
    }
}
