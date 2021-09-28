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

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.car.drivingstate.CarUxRestrictions;
import android.car.user.CarUserManager;
import android.car.user.UserCreationResult;
import android.content.Context;
import android.content.pm.UserInfo;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.car.settings.R;
import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowUserHelper;
import com.android.car.settings.testutils.ShadowUserIconProvider;
import com.android.car.settings.testutils.ShadowUserManager;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ToolbarController;
import com.android.car.ui.utils.CarUxRestrictionsUtil;
import com.android.internal.infra.AndroidFuture;

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

import java.util.ArrayList;

/**
 * Tests for UserDetailsFragment.
 */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserIconProvider.class, ShadowUserHelper.class, ShadowUserManager.class})
public class UsersListFragmentTest {

    private Context mContext;
    private UsersListFragment mFragment;
    private MenuItem mActionButton;
    private FragmentController<UsersListFragment> mFragmentController;

    @Mock
    private CarUserManager mCarUserManager;

    @Mock
    private UserHelper mUserHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowUserHelper.setInstance(mUserHelper);
        mContext = RuntimeEnvironment.application;

        mFragment = new UsersListFragment();
        mFragmentController = FragmentController.of(mFragment);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();
    }

    @After
    public void tearDown() {
        ShadowUserHelper.reset();
        ShadowUserManager.reset();
    }

    @Test
    public void onCreate_userInDemoMode_showsExitRetailModeButton() {
        createUsersListFragment(UserInfo.FLAG_DEMO, /* disallowAddUser= */ false);

        assertThat(mActionButton.isVisible()).isTrue();
        assertThat(mActionButton.getTitle().toString())
                .isEqualTo(mContext.getString(R.string.exit_retail_button_text));
    }

    @Test
    public void onCreate_userCanAddNewUser_showsAddUserButton() {
        createUsersListFragment(/* flags= */ 0, /* disallowAddUser= */ false);

        assertThat(mActionButton.isVisible()).isTrue();
        assertThat(mActionButton.getTitle().toString())
                .isEqualTo(mContext.getString(R.string.user_add_user_menu));
    }

    @Test
    public void onCreate_userRestrictedFromAddingNewUserAndNotInDemo_doesNotShowActionButton() {
        createUsersListFragment(/* flags= */ 0, /*disallowAddUser= */ true);

        assertThat(mActionButton.isVisible()).isFalse();
    }

    /* Test that onCreateNewUserConfirmed invokes a creation of a new non-admin. */
    @Test
    public void testOnCreateNewUserConfirmedInvokesCreateNewUser() {
        createUsersListFragment(/* flags= */ 0, /* disallowAddUser= */ false);
        mFragment.setCarUserManager(mCarUserManager);

        AndroidFuture<UserCreationResult> future = new AndroidFuture<>();
        future.complete(new UserCreationResult(UserCreationResult.STATUS_SUCCESSFUL,
                /* user= */ null, /* errorMessage= */ null));
        when(mCarUserManager.createUser(anyString(), anyInt())).thenReturn(future);

        mFragment.mConfirmCreateNewUserListener.onConfirm(/* arguments= */ null);
        verify(mCarUserManager).createUser(mContext.getString(R.string.user_new_user_name),
                /* flags= */ 0);
    }

    /* Test that if we're in demo user, click on the button starts exit out of the retail mode. */
    @Test
    public void testCallOnClick_demoUser_exitRetailMode() {
        createUsersListFragment(UserInfo.FLAG_DEMO, /* disallowAddUser= */ false);
        mActionButton.performClick();
        assertThat(isDialogShown(UsersListFragment.CONFIRM_EXIT_RETAIL_MODE_DIALOG_TAG)).isTrue();
    }

    /* Test that if the max num of users is reached, click on the button informs user of that. */
    @Test
    public void testCallOnClick_userLimitReached_showErrorDialog() {
        ShadowUserManager.setCanAddMoreUsers(false);
        createUsersListFragment(/* flags= */ 0, /* disallowAddUser= */ false);
        mActionButton.performClick();
        assertThat(isDialogShown(UsersListFragment.MAX_USERS_LIMIT_REACHED_DIALOG_TAG)).isTrue();
    }

    /* Test that if user can add other users, click on the button creates a dialog to confirm. */
    @Test
    public void testCallOnClick_showAddUserDialog() {
        createUsersListFragment(/* flags= */ 0, /* disallowAddUser= */ false);
        mActionButton.performClick();
        assertThat(isDialogShown(UsersListFragment.CONFIRM_CREATE_NEW_USER_DIALOG_TAG)).isTrue();
    }

    private void createUsersListFragment(int flags, boolean disallowAddUser) {
        Shadows.shadowOf(UserManager.get(mContext)).addUser(UserHandle.myUserId(),
                "User Name", flags);
        UserInfo testUser = UserManager.get(mContext).getUserInfo(UserHandle.myUserId());
        Shadows.shadowOf(UserManager.get(mContext)).setUserRestriction(
                testUser.getUserHandle(), UserManager.DISALLOW_ADD_USER, disallowAddUser);
        when(mUserHelper.getCurrentProcessUserInfo()).thenReturn(testUser);
        when(mUserHelper.getAllSwitchableUsers()).thenReturn(new ArrayList<>());
        mFragmentController.setup();

        ToolbarController toolbar = (ToolbarController) requireToolbar(mFragment.requireActivity());
        CarUxRestrictionsUtil.getInstance(mContext).setUxRestrictions(new CarUxRestrictions.Builder(
                true, CarUxRestrictions.UX_RESTRICTIONS_BASELINE, 0)
                .build());
        mActionButton = toolbar.getMenuItems().get(0);
    }

    private boolean isDialogShown(String tag) {
        return mFragment.findDialogByTag(tag) != null;
    }
}
