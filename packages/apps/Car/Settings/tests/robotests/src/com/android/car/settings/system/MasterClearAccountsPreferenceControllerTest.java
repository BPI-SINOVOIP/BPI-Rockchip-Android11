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

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AuthenticatorDescription;
import android.content.Context;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.lifecycle.Lifecycle;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.R;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowAccountManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowUserManager;

/** Unit test for {@link MasterClearAccountsPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowAccountManager.class})
public class MasterClearAccountsPreferenceControllerTest {

    private final int mUserId = UserHandle.myUserId();

    private Context mContext;
    private PreferenceGroup mPreferenceGroup;
    private PreferenceControllerTestHelper<MasterClearAccountsPreferenceController>
            mControllerHelper;
    private MasterClearAccountsPreferenceController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        getShadowUserManager().addProfile(mUserId, mUserId,
                String.valueOf(mUserId), /* profileFlags= */ 0);

        mPreferenceGroup = new PreferenceCategory(mContext);
        mControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                MasterClearAccountsPreferenceController.class, mPreferenceGroup);
        mController = mControllerHelper.getController();
    }

    @Test
    public void onCreate_addsTitlePreference() {
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(1);
        assertThat(mPreferenceGroup.getPreference(0).getTitle()).isEqualTo(
                mContext.getString(R.string.master_clear_accounts));
    }

    @Test
    public void refreshUi_accountsPresent_showsGroup() {
        mControllerHelper.markState(Lifecycle.State.STARTED);
        addAccountAndDescription(mUserId, "accountName");

        mController.refreshUi();

        assertThat(mPreferenceGroup.isVisible()).isTrue();
    }

    @Test
    public void refreshUi_noAccountsPresent_hidesGroup() {
        mControllerHelper.markState(Lifecycle.State.STARTED);

        mController.refreshUi();

        assertThat(mPreferenceGroup.isVisible()).isFalse();
    }

    @Test
    public void refreshUi_multipleProfiles_showsAllAccounts() {
        mControllerHelper.markState(Lifecycle.State.STARTED);

        int profileId1 = 112;
        getShadowUserManager().addProfile(mUserId, profileId1,
                String.valueOf(profileId1), /* profileFlags= */ 0);
        String accountName1 = "accountName1";
        addAccountAndDescription(profileId1, accountName1);

        int profileId2 = 113;
        getShadowUserManager().addProfile(mUserId, profileId2,
                String.valueOf(profileId2), /* profileFlags= */ 0);
        String accountName2 = "accountName2";
        addAccountAndDescription(profileId2, accountName2);

        mController.refreshUi();

        // Title + two profiles with one account each.
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(3);
        assertThat(mPreferenceGroup.getPreference(1).getTitle()).isEqualTo(accountName1);
        assertThat(mPreferenceGroup.getPreference(2).getTitle()).isEqualTo(accountName2);
    }

    @Test
    public void refreshUi_missingAccountDescription_skipsAccount() {
        mControllerHelper.markState(Lifecycle.State.STARTED);
        addAccountAndDescription(mUserId, "account name with desc");
        String accountNameNoDesc = "account name no desc";
        getShadowAccountManager().addAccountAsUser(mUserId,
                new Account(accountNameNoDesc, accountNameNoDesc + "_type"));

        mController.refreshUi();

        // Title + one account with valid description.
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(2);
        assertThat(mPreferenceGroup.getPreference(1).getTitle()).isNotEqualTo(accountNameNoDesc);
    }

    @Test
    public void refreshUi_accountAdded_addsPreferenceToGroup() {
        addAccountAndDescription(mUserId, "accountName");
        mControllerHelper.markState(Lifecycle.State.STARTED);
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(2);

        String addedAccountName = "added account name";
        addAccountAndDescription(mUserId, addedAccountName);
        mController.refreshUi();

        // Title + one already present account + one newly added account.
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(3);
        assertThat(mPreferenceGroup.getPreference(2).getTitle()).isEqualTo(addedAccountName);
    }

    @Test
    public void refreshUi_accountRemoved_removesPreferenceFromGroup() {
        String accountNameToRemove = "account name to remove";
        addAccountAndDescription(mUserId, accountNameToRemove);
        mControllerHelper.markState(Lifecycle.State.STARTED);
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(2);

        getShadowAccountManager().removeAllAccounts();
        mController.refreshUi();

        // Title only, all accounts removed.
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(1);
        assertThat(mPreferenceGroup.getPreference(0).getTitle()).isNotEqualTo(accountNameToRemove);
    }

    private void addAccountAndDescription(int profileId, String accountName) {
        String type = accountName + "_type";
        getShadowAccountManager().addAccountAsUser(profileId, new Account(accountName, type));
        getShadowAccountManager().addAuthenticatorAsUser(profileId,
                new AuthenticatorDescription(type, "packageName", /* labelId= */ 0, /* iconId= */
                        0, /* smallIconId= */ 0, /* prefId= */ 0));
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadows.shadowOf(UserManager.get(mContext));
    }

    private ShadowAccountManager getShadowAccountManager() {
        return Shadow.extract(AccountManager.get(mContext));
    }
}
