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
import android.content.Context;
import android.content.pm.UserInfo;

import androidx.fragment.app.Fragment;

import com.android.car.settings.testutils.BaseTestActivity;
import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowAccountManager;
import com.android.car.settings.testutils.ShadowContentResolver;
import com.android.car.settings.testutils.ShadowUserHelper;
import com.android.car.settings.users.UserHelper;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.ToolbarController;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

/**
 * Tests for the {@link AccountDetailsFragment}.
 */
@Ignore
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowAccountManager.class, ShadowContentResolver.class, ShadowUserHelper.class})
public class AccountDetailsFragmentTest {
    private static final String DIALOG_TAG = "confirmRemoveAccount";
    private final Account mAccount = new Account("Name", "com.acct");
    private final UserInfo mUserInfo = new UserInfo(/* id= */ 0, /* name= */ "name", /* flags= */
            0);
    private final CharSequence mAccountLabel = "Type 1";

    private Context mContext;
    private FragmentController<AccountDetailsFragment> mFragmentController;
    private AccountDetailsFragment mFragment;

    @Mock
    private UserHelper mMockUserHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowUserHelper.setInstance(mMockUserHelper);

        mContext = application;
        // Add the account to the official list of accounts
        getShadowAccountManager().addAccount(mAccount);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();
    }

    @After
    public void tearDown() {
        ShadowContentResolver.reset();
        ShadowUserHelper.reset();
    }

    @Test
    public void onActivityCreated_titleShouldBeSet() {
        initFragment();
        ToolbarController toolbar = requireToolbar(mFragment.requireActivity());
        assertThat(toolbar.getTitle()).isEqualTo(mAccountLabel);
    }

    @Test
    public void cannotModifyUsers_removeAccountButtonShouldNotBeVisible() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts())
                .thenReturn(false);
        initFragment();

        ToolbarController toolbar = requireToolbar(mFragment.requireActivity());
        assertThat(toolbar.getMenuItems()).hasSize(1);
        assertThat(toolbar.getMenuItems().get(0).isVisible()).isFalse();
    }

    @Test
    public void canModifyUsers_removeAccountButtonShouldBeVisible() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts())
                .thenReturn(true);
        initFragment();

        ToolbarController toolbar = requireToolbar(mFragment.requireActivity());
        assertThat(toolbar.getMenuItems()).hasSize(1);
        assertThat(toolbar.getMenuItems().get(0).isVisible()).isTrue();
    }

    @Test
    public void onRemoveAccountButtonClicked_canModifyUsers_shouldShowConfirmRemoveAccountDialog() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts())
                .thenReturn(true);
        initFragment();

        ToolbarController toolbar = requireToolbar(mFragment.requireActivity());
        toolbar.getMenuItems().get(0).performClick();

        Fragment dialogFragment = mFragment.findDialogByTag(DIALOG_TAG);

        assertThat(dialogFragment).isNotNull();
        assertThat(dialogFragment).isInstanceOf(
                AccountDetailsFragment.ConfirmRemoveAccountDialogFragment.class);
    }

    @Test
    public void accountExists_accountStillExists_shouldBeTrue() {
        initFragment();
        // Nothing has happened to the account so this should return true;
        assertThat(mFragment.accountExists()).isTrue();
    }

    @Test
    public void accountExists_accountWasRemoved_shouldBeFalse() {
        initFragment();
        // Clear accounts so that the account being displayed appears to have been removed
        getShadowAccountManager().removeAllAccounts();
        assertThat(mFragment.accountExists()).isFalse();
    }

    @Test
    public void onAccountsUpdate_accountDoesNotExist_shouldGoBack() {
        initFragment();
        // Clear accounts so that the account being displayed appears to have been removed
        getShadowAccountManager().removeAllAccounts();
        mFragment.onAccountsUpdate(null);

        assertThat(((BaseTestActivity) mFragment.requireActivity())
                .getOnBackPressedFlag()).isTrue();
    }

    private void initFragment() {
        mFragment = AccountDetailsFragment.newInstance(mAccount, mAccountLabel, mUserInfo);
        mFragmentController = FragmentController.of(mFragment);
        mFragmentController.setup();
    }

    private ShadowAccountManager getShadowAccountManager() {
        return Shadow.extract(AccountManager.get(mContext));
    }
}
