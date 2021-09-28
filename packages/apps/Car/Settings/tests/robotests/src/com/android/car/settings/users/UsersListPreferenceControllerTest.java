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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.UserInfo;
import android.os.UserManager;

import androidx.lifecycle.Lifecycle;
import androidx.preference.PreferenceGroup;

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
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowUserManager;

import java.util.Collections;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserIconProvider.class, ShadowUserHelper.class})
public class UsersListPreferenceControllerTest {

    private static final UserInfo TEST_CURRENT_USER = new UserInfo(/* id= */ 10,
            "TEST_USER_NAME", FLAG_ADMIN);
    private static final UserInfo TEST_OTHER_USER = new UserInfo(/* id= */ 11,
            "TEST_OTHER_NAME", /* flags= */ 0);

    private PreferenceControllerTestHelper<UsersListPreferenceController> mControllerHelper;
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
                UsersListPreferenceController.class, mPreferenceGroup);

        getShadowUserManager().addUser(TEST_CURRENT_USER.id, TEST_CURRENT_USER.name,
                TEST_CURRENT_USER.flags);
        getShadowUserManager().switchUser(TEST_CURRENT_USER.id);
        when(mUserHelper.getCurrentProcessUserInfo()).thenReturn(TEST_CURRENT_USER);
        when(mUserHelper.isCurrentProcessUser(TEST_CURRENT_USER)).thenReturn(true);
        when(mUserHelper.getAllSwitchableUsers()).thenReturn(
                Collections.singletonList(TEST_OTHER_USER));
        when(mUserHelper.getAllLivingUsers(any())).thenReturn(
                Collections.singletonList(TEST_OTHER_USER));

        mControllerHelper.markState(Lifecycle.State.STARTED);
    }

    @After
    public void tearDown() {
        ShadowUserHelper.reset();
    }

    @Test
    public void testPreferencePerformClick_currentAdminUser_openNewFragment() {
        mPreferenceGroup.getPreference(0).performClick();

        verify(mControllerHelper.getMockFragmentController()).launchFragment(
                any(UserDetailsFragment.class));
    }

    @Test
    public void testPreferencePerformClick_otherNonAdminUser_openNewFragment() {
        mPreferenceGroup.getPreference(1).performClick();

        verify(mControllerHelper.getMockFragmentController()).launchFragment(
                any(UserDetailsPermissionsFragment.class));
    }

    @Test
    public void testPreferencePerformClick_guestUser_noAction() {
        mPreferenceGroup.getPreference(2).performClick();

        verify(mControllerHelper.getMockFragmentController(), never()).launchFragment(any());
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadows.shadowOf(UserManager.get(mContext));
    }
}
