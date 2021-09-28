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

package com.android.car.settings.accounts;

import static com.android.car.ui.core.CarUi.requireToolbar;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;
import static org.robolectric.RuntimeEnvironment.application;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AuthenticatorDescription;
import android.content.Intent;
import android.content.pm.UserInfo;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.car.settings.R;
import com.android.car.settings.common.CarSettingActivities;
import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowAccountManager;
import com.android.car.settings.testutils.ShadowContentResolver;
import com.android.car.settings.testutils.ShadowUserHelper;
import com.android.car.settings.users.UserHelper;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ToolbarController;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowIntent;
import org.robolectric.shadows.ShadowUserManager;

/**
 * Tests for AccountSettingsFragment class.
 */
@Ignore
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowAccountManager.class, ShadowContentResolver.class, ShadowUserHelper.class})
public class AccountSettingsFragmentTest {
    private final int mUserId = UserHandle.myUserId();

    private FragmentController<AccountSettingsFragment> mFragmentController;
    private AccountSettingsFragment mFragment;

    @Mock
    private UserHelper mMockUserHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowUserHelper.setInstance(mMockUserHelper);
        // Set up user info
        when(mMockUserHelper.getCurrentProcessUserInfo())
                .thenReturn(new UserInfo(mUserId, "USER", /* flags= */ 0));

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();
    }

    @After
    public void tearDown() {
        ShadowUserHelper.reset();
    }

    @Test
    public void cannotModifyUsers_addAccountButtonShouldNotBeVisible() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts()).thenReturn(false);
        initFragment();

        assertThat(getToolbar().getMenuItems()).hasSize(1);
        assertThat(getToolbar().getMenuItems().get(0).isVisible()).isFalse();
    }

    @Test
    public void canModifyUsers_addAccountButtonShouldBeVisible() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts()).thenReturn(true);
        initFragment();

        assertThat(getToolbar().getMenuItems()).hasSize(1);
        assertThat(getToolbar().getMenuItems().get(0).isVisible()).isTrue();
    }

    @Test
    public void clickAddAccountButton_shouldOpenChooseAccountFragment() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts()).thenReturn(true);
        initFragment();

        MenuItem addAccountButton = getToolbar().getMenuItems().get(0);
        addAccountButton.performClick();

        Intent intent = Shadows.shadowOf(mFragment.getActivity()).getNextStartedActivity();
        ShadowIntent shadowIntent = Shadows.shadowOf(intent);
        assertThat(shadowIntent.getIntentClass()).isEqualTo(
                CarSettingActivities.ChooseAccountActivity.class);
    }

    @Test
    public void clickAddAccountButton_shouldNotOpenChooseAccountFragmentWhenOneType() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts()).thenReturn(true);
        getShadowUserManager().addProfile(mUserId, mUserId,
                String.valueOf(mUserId), /* profileFlags= */ 0);
        addAccountAndDescription(mUserId, "accountName", R.string.account_type1_label);
        initFragment();

        MenuItem addAccountButton = getToolbar().getMenuItems().get(0);
        addAccountButton.performClick();

        Intent intent = Shadows.shadowOf(mFragment.getActivity()).getNextStartedActivity();
        ShadowIntent shadowIntent = Shadows.shadowOf(intent);
        assertThat(shadowIntent.getIntentClass()).isEqualTo(AddAccountActivity.class);
    }

    @Test
    public void clickAddAccountButton_shouldOpenChooseAccountFragmentWhenTwoTypes() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts()).thenReturn(true);
        getShadowUserManager().addProfile(mUserId, mUserId,
                String.valueOf(mUserId), /* profileFlags= */ 0);
        addAccountAndDescription(mUserId, "accountName1", R.string.account_type1_label);
        addAccountAndDescription(mUserId, "accountName2", R.string.account_type2_label);
        initFragment();

        getToolbar().getMenuItems().get(0).performClick();

        Intent intent = Shadows.shadowOf(mFragment.getActivity()).getNextStartedActivity();
        ShadowIntent shadowIntent = Shadows.shadowOf(intent);
        assertThat(shadowIntent.getIntentClass()).isEqualTo(
                CarSettingActivities.ChooseAccountActivity.class);
    }

    private void initFragment() {
        mFragment = new AccountSettingsFragment();
        mFragmentController = FragmentController.of(mFragment);
        mFragmentController.setup();
    }

    private void addAccountAndDescription(int profileId, String accountName, int labelId) {
        String type = accountName + "_type";
        getShadowAccountManager().addAccountAsUser(profileId, new Account(accountName, type));
        getShadowAccountManager().addAuthenticatorAsUser(profileId,
                new AuthenticatorDescription(type, "com.android.car.settings",
                        labelId, /* iconId= */ R.drawable.ic_add, /* smallIconId= */
                        0, /* prefId= */ 0));
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadows.shadowOf(UserManager.get(application));
    }

    private ShadowAccountManager getShadowAccountManager() {
        return Shadow.extract(AccountManager.get(application));
    }

    private ToolbarController getToolbar() {
        return requireToolbar(mFragment.requireActivity());
    }
}
