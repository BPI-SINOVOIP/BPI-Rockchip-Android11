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

import static android.content.pm.UserInfo.FLAG_ADMIN;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.content.pm.UserInfo;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.LogicalPreferenceGroup;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowUserHelper;
import com.android.car.settings.testutils.ShadowUserIconProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserIconProvider.class, ShadowUserHelper.class})
public class UsersBasePreferenceControllerTest {

    private static class TestUsersBasePreferenceController extends UsersBasePreferenceController {

        TestUsersBasePreferenceController(Context context, String preferenceKey,
                FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
            super(context, preferenceKey, fragmentController, uxRestrictions);
        }

        @Override
        protected void userClicked(UserInfo userInfo) {
        }
    }

    private static final UserInfo TEST_CURRENT_USER = new UserInfo(/* id= */ 10,
            "TEST_USER_NAME", /* flags= */ 0);
    private static final UserInfo TEST_OTHER_USER = new UserInfo(/* id= */ 11,
            "TEST_OTHER_NAME", /* flags= */ 0);

    private static final List<String> LISTENER_ACTIONS =
            Collections.singletonList(Intent.ACTION_USER_INFO_CHANGED);

    private PreferenceControllerTestHelper<TestUsersBasePreferenceController> mControllerHelper;
    private TestUsersBasePreferenceController mController;
    private PreferenceGroup mPreferenceGroup;
    private Context mContext;
    @Mock
    private UserHelper mUserHelper;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        MockitoAnnotations.initMocks(this);
        ShadowUserHelper.setInstance(mUserHelper);
        mPreferenceGroup = new LogicalPreferenceGroup(mContext);
        mControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                TestUsersBasePreferenceController.class, mPreferenceGroup);
        mController = mControllerHelper.getController();
        when(mUserHelper.getCurrentProcessUserInfo()).thenReturn(TEST_CURRENT_USER);
        when(mUserHelper.isCurrentProcessUser(TEST_CURRENT_USER)).thenReturn(true);
        when(mUserHelper.getAllSwitchableUsers()).thenReturn(
                Collections.singletonList(TEST_OTHER_USER));
        when(mUserHelper.getAllLivingUsers(any())).thenReturn(
                Collections.singletonList(TEST_OTHER_USER));
    }

    @After
    public void tearDown() {
        ShadowUserHelper.reset();
    }

    @Test
    public void onCreate_registersOnUsersUpdateListener() {
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        assertThat(BroadcastReceiverHelpers.getRegisteredReceiverWithActions(LISTENER_ACTIONS))
                .isNotNull();
    }

    @Test
    public void onCreate_populatesUsers() {
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        // Three users. Current user, other user, guest user.
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(3);
    }

    @Test
    public void onDestroy_unregistersOnUsersUpdateListener() {
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
        mControllerHelper.markState(Lifecycle.State.STARTED);
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_DESTROY);
        assertThat(BroadcastReceiverHelpers.getRegisteredReceiverWithActions(LISTENER_ACTIONS))
                .isNull();
    }

    @Test
    public void refreshUi_userChange_updatesGroup() {
        mControllerHelper.markState(Lifecycle.State.STARTED);

        // Store the list of previous Preferences.
        List<Preference> currentPreferences = new ArrayList<>();
        for (int i = 0; i < mPreferenceGroup.getPreferenceCount(); i++) {
            currentPreferences.add(mPreferenceGroup.getPreference(i));
        }

        // Mock a change so that other user becomes an admin.
        UserInfo adminOtherUser = new UserInfo(/* id= */ 11, "TEST_OTHER_NAME", FLAG_ADMIN);
        when(mUserHelper.getAllSwitchableUsers()).thenReturn(
                Collections.singletonList(adminOtherUser));
        when(mUserHelper.getAllLivingUsers(any())).thenReturn(
                Arrays.asList(TEST_OTHER_USER, adminOtherUser));

        mController.refreshUi();

        List<Preference> newPreferences = new ArrayList<>();
        for (int i = 0; i < mPreferenceGroup.getPreferenceCount(); i++) {
            newPreferences.add(mPreferenceGroup.getPreference(i));
        }

        assertThat(newPreferences).containsNoneIn(currentPreferences);
    }

    @Test
    public void refreshUi_noChange_doesNotUpdateGroup() {
        mControllerHelper.markState(Lifecycle.State.STARTED);

        // Store the list of previous Preferences.
        List<Preference> currentPreferences = new ArrayList<>();
        for (int i = 0; i < mPreferenceGroup.getPreferenceCount(); i++) {
            currentPreferences.add(mPreferenceGroup.getPreference(i));
        }

        mController.refreshUi();

        List<Preference> newPreferences = new ArrayList<>();
        for (int i = 0; i < mPreferenceGroup.getPreferenceCount(); i++) {
            newPreferences.add(mPreferenceGroup.getPreference(i));
        }

        assertThat(newPreferences).containsExactlyElementsIn(currentPreferences);
    }

    @Test
    public void onUsersUpdated_updatesGroup() {
        mControllerHelper.markState(Lifecycle.State.STARTED);

        // Store the list of previous Preferences.
        List<Preference> currentPreferences = new ArrayList<>();
        for (int i = 0; i < mPreferenceGroup.getPreferenceCount(); i++) {
            currentPreferences.add(mPreferenceGroup.getPreference(i));
        }

        // Mock a change so that other user becomes an admin.
        UserInfo adminOtherUser = new UserInfo(/* id= */ 11, "TEST_OTHER_NAME", FLAG_ADMIN);
        when(mUserHelper.getAllSwitchableUsers()).thenReturn(
                Collections.singletonList(adminOtherUser));
        when(mUserHelper.getAllLivingUsers(any())).thenReturn(
                Arrays.asList(TEST_OTHER_USER, adminOtherUser));

        mContext.sendBroadcast(new Intent(LISTENER_ACTIONS.get(0)));

        List<Preference> newPreferences = new ArrayList<>();
        for (int i = 0; i < mPreferenceGroup.getPreferenceCount(); i++) {
            newPreferences.add(mPreferenceGroup.getPreference(i));
        }

        assertThat(newPreferences).containsNoneIn(currentPreferences);
    }
}
