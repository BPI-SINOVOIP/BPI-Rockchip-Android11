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

import static com.android.car.ui.core.CarUi.requireToolbar;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.os.UserManager;

import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowUserIconProvider;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.ToolbarController;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserIconProvider.class})
public class UserDetailsFragmentTest {

    private static final String TEST_NAME = "test_name";
    private static final String TEST_UPDATED_NAME = "test_updated_name";
    private static final int TEST_USER_ID = 10;

    private Context mContext;
    private UserManager mUserManager;
    private UserDetailsFragment mUserDetailsFragment;

    private ToolbarController mToolbar;

    private FragmentController<UserDetailsFragment> mFragmentController;

    @Before
    public void setUpTestActivity() {
        MockitoAnnotations.initMocks(this);

        mContext = RuntimeEnvironment.application;
        mUserManager = UserManager.get(mContext);
        Shadows.shadowOf(mUserManager).addUser(TEST_USER_ID, TEST_NAME, /* flags= */ 0);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();
    }

    @Test
    public void testCarUserManagerHelperUpdateListener_showsCorrectText() {
        createUserDetailsFragment();
        mUserDetailsFragment.mUserUpdateReceiver.onReceive(/* context= */ null, /* intent= */ null);
        assertThat(mToolbar.getTitle()).isEqualTo(
                UserUtils.getUserDisplayName(mContext, mUserManager.getUserInfo(TEST_USER_ID)));
    }

    @Test
    public void testCarUserManagerHelperUpdateListener_textChangesWithUserUpdate() {
        createUserDetailsFragment();
        mUserDetailsFragment.mUserUpdateReceiver.onReceive(/* context= */ null, /* intent= */ null);
        assertThat(mToolbar.getTitle()).isEqualTo(
                UserUtils.getUserDisplayName(mContext, mUserManager.getUserInfo(TEST_USER_ID)));

        mUserManager.removeUser(TEST_USER_ID);
        Shadows.shadowOf(mUserManager).addUser(TEST_USER_ID, TEST_UPDATED_NAME, /* flags= */ 0);

        mUserDetailsFragment.mUserUpdateReceiver.onReceive(/* context= */ null, /* intent= */ null);
        assertThat(mToolbar.getTitle()).isEqualTo(
                UserUtils.getUserDisplayName(mContext, mUserManager.getUserInfo(TEST_USER_ID)));
    }

    private void createUserDetailsFragment() {
        mUserDetailsFragment = UserDetailsFragment.newInstance(TEST_USER_ID);
        mFragmentController = FragmentController.of(mUserDetailsFragment);
        mFragmentController.setup();

        mToolbar = ((ToolbarController) requireToolbar(mUserDetailsFragment.requireActivity()));
    }
}
