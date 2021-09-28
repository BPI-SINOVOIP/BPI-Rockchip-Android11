/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.robolectric.RuntimeEnvironment.application;

import android.accounts.AccountManager;
import android.accounts.AuthenticatorDescription;
import android.car.userlib.CarUserManagerHelper;
import android.content.SyncAdapterType;

import com.android.car.settings.R;
import com.android.car.settings.testutils.ShadowAccountManager;
import com.android.car.settings.testutils.ShadowContentResolver;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link AccountTypesHelper}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowContentResolver.class, ShadowAccountManager.class})
public class AccountTypesHelperTest {
    private static final String ACCOUNT_TYPE_1 = "com.acct1";
    private static final String ACCOUNT_TYPE_2 = "com.acct2";
    private static final String ACCOUNT_TYPE_3 = "com.acct3";

    private AccountTypesHelper mHelper;

    @Mock
    private CarUserManagerHelper mMockCarUserManagerHelper;

    private AccountManager mAccountManager = AccountManager.get(application);

    private int mOnChangeListenerInvocations;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Add authenticated account types.
        addAuthenticator(ACCOUNT_TYPE_1, /* label= */ R.string.account_type1_label);
        addAuthenticator(ACCOUNT_TYPE_2, /* label= */ R.string.account_type2_label);

        mHelper = new AccountTypesHelper(application);
        mHelper.setOnChangeListener(() -> mOnChangeListenerInvocations++);
    }

    @After
    public void tearDown() {
        ShadowContentResolver.reset();
    }

    @Test
    public void forceUpdate_authorizedAccountTypesShouldBeSet() {
        mHelper.forceUpdate();

        assertThat(mHelper.getAuthorizedAccountTypes()).containsExactly(
                ACCOUNT_TYPE_1, ACCOUNT_TYPE_2);
        assertThat(mOnChangeListenerInvocations).isEqualTo(1);
    }

    @Test
    public void forceUpdate_hasAccountTypeFilter_shouldFilterAccounts() {
        // Add a filter that should filter out the second account type (com.acct2).
        Set<String> accountTypesFilter = new HashSet<>();
        accountTypesFilter.add(ACCOUNT_TYPE_1);
        mHelper.setAccountTypesFilter(accountTypesFilter);

        mHelper.forceUpdate();

        assertThat(mHelper.getAuthorizedAccountTypes()).containsExactly(ACCOUNT_TYPE_1);
        assertThat(mOnChangeListenerInvocations).isEqualTo(1);
    }

    @Test
    public void forceUpdate_hasAccountExclusionFilter_shouldFilterAccounts() {
        // Add a filter that should filter out the first account type (com.acct1).
        Set<String> accountExclusionTypesFilter = new HashSet<>();
        accountExclusionTypesFilter.add(ACCOUNT_TYPE_1);
        mHelper.setAccountTypesExclusionFilter(accountExclusionTypesFilter);

        mHelper.forceUpdate();

        assertThat(mHelper.getAuthorizedAccountTypes()).containsExactly(ACCOUNT_TYPE_2);
        assertThat(mOnChangeListenerInvocations).isEqualTo(1);
    }

    @Test
    public void forceUpdate_doesNotHaveAuthoritiesInFilter_notAuthorized() {
        // Add a sync adapter type for the com.acct1 account type that does not have the same
        // authority as the one passed to someAuthority.
        SyncAdapterType syncAdapterType = new SyncAdapterType("someAuthority",
                ACCOUNT_TYPE_1, /* userVisible= */ true, /* supportsUploading= */ true);
        SyncAdapterType[] syncAdapters = {syncAdapterType};
        ShadowContentResolver.setSyncAdapterTypes(syncAdapters);

        mHelper.setAuthorities(Collections.singletonList("someOtherAuthority"));

        // Force an authenticator refresh so the authorities are refreshed.
        mHelper.getAuthenticatorHelper().onReceive(application, null);
        mHelper.forceUpdate();

        assertThat(mHelper.getAuthorizedAccountTypes()).containsExactly(ACCOUNT_TYPE_2);
        assertThat(mOnChangeListenerInvocations).isEqualTo(1);
    }

    @Test
    public void refreshUi_hasAuthoritiesInFilter_notAuthorized() {
        // Add a sync adapter type for the com.acct1 account type that has the same authority as
        // the one passed to someAuthority.
        SyncAdapterType syncAdapterType = new SyncAdapterType("someAuthority",
                ACCOUNT_TYPE_1, /* userVisible= */ true, /* supportsUploading= */ true);
        SyncAdapterType[] syncAdapters = {syncAdapterType};
        ShadowContentResolver.setSyncAdapterTypes(syncAdapters);

        mHelper.setAuthorities(Collections.singletonList("someAuthority"));

        // Force an authenticator refresh so the authorities are refreshed.
        mHelper.getAuthenticatorHelper().onReceive(application, null);
        mHelper.forceUpdate();

        assertThat(mHelper.getAuthorizedAccountTypes()).containsExactly(
                ACCOUNT_TYPE_1, ACCOUNT_TYPE_2);
        assertThat(mOnChangeListenerInvocations).isEqualTo(1);
    }

    @Test
    public void onAccountsUpdate_currentUserUpdated_shouldForceUpdate() {
        assertThat(mHelper.getAuthorizedAccountTypes().size()).isEqualTo(2);

        addAuthenticator(ACCOUNT_TYPE_3, /* label= */ R.string.account_type3_label);

        // Trigger an account update via the authenticator helper while listening for account
        // updates.
        mHelper.listenToAccountUpdates();
        mHelper.getAuthenticatorHelper().onReceive(application, null);
        mHelper.stopListeningToAccountUpdates();

        assertThat(mHelper.getAuthorizedAccountTypes()).containsExactly(
                ACCOUNT_TYPE_1, ACCOUNT_TYPE_2, ACCOUNT_TYPE_3);
        assertWithMessage("listener should be invoked twice")
                .that(mOnChangeListenerInvocations).isEqualTo(2);
    }

    private void addAuthenticator(String type, int labelRes) {
        getShadowAccountManager().addAuthenticator(
                new AuthenticatorDescription(type, "com.android.car.settings",
                        labelRes, 0, 0, 0, false));
    }

    private ShadowAccountManager getShadowAccountManager() {
        return Shadow.extract(mAccountManager);
    }
}
