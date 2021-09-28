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
package com.android.cts.accountmanager;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerFuture;
import android.app.UiAutomation;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Process;
import android.os.UserHandle;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class AccountManagerCrossUserTest {

    private static final Account TEST_ACCOUNT = new Account(MockAuthenticator.ACCOUNT_NAME,
            MockAuthenticator.ACCOUNT_TYPE);

    private Context mContext;
    private UiAutomation uiAutomation;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();

        uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
    }

    @After
    public void tearDown(){
        uiAutomation.dropShellPermissionIdentity();
    }

    private int getTestUser() {
        final Bundle testArguments = InstrumentationRegistry.getArguments();
        if (testArguments.containsKey("testUser")) {
            try {
                return Integer.parseInt(testArguments.getString("testUser"));
            } catch (NumberFormatException ignore) {
            }
        }
        return mContext.getUserId();
    }

    private int getNumAccountsExpected() {
        int numAccountsExpected = 1;
        final Bundle testArguments = InstrumentationRegistry.getArguments();
        if (testArguments.containsKey("numAccountsExpected")) {
            try {
                numAccountsExpected = Integer.parseInt(
                        testArguments.getString("numAccountsExpected"));
            } catch (NumberFormatException ignore) {
            }
        }
        return numAccountsExpected;
    }

    @Test
    public void testAccountManager_addMockAccount() throws Exception {
        UserHandle profileHandle = UserHandle.of(getTestUser());

        uiAutomation.adoptShellPermissionIdentity(
                "android.permission.INTERACT_ACROSS_USERS_FULL");

        AccountManager accountManagerAsUser = (AccountManager) mContext.createContextAsUser(
                profileHandle, 0).getSystemService(Context.ACCOUNT_SERVICE);

        AccountManagerFuture<Bundle> future = accountManagerAsUser.addAccount(
                MockAuthenticator.ACCOUNT_TYPE,
                null, null, null, null, null, null);

        Bundle result = future.getResult();
        assertThat(result.getString(AccountManager.KEY_ACCOUNT_TYPE)).isEqualTo(
                MockAuthenticator.ACCOUNT_TYPE);
        assertThat(result.getString(AccountManager.KEY_ACCOUNT_NAME)).isEqualTo(
                MockAuthenticator.ACCOUNT_NAME);

        Account[] accounts = accountManagerAsUser.getAccounts();
        assertThat(accounts).hasLength(1);
    }

    @Test
    public void testAccountManager_getAccountsForTestUser() throws Exception {
        UserHandle profileHandle = UserHandle.of(getTestUser());

        try {
            uiAutomation.adoptShellPermissionIdentity(
                    "android.permission.INTERACT_ACROSS_USERS_FULL");

            AccountManager accountManagerAsUser = (AccountManager) mContext.createContextAsUser(
                    profileHandle, 0).getSystemService(Context.ACCOUNT_SERVICE);

            assertWithMessage("No accounts for user " + profileHandle
                    + ". Make sure testAccountManager_addAccountExplicitly is executed "
                    + "before this test").that(
                    accountManagerAsUser.getAccountsByType(MockAuthenticator.ACCOUNT_TYPE))
                    .hasLength(1);

            Account[] accounts = accountManagerAsUser.getAccounts();
            assertThat(accounts).hasLength(1);
        } catch (Exception e) {
        }
    }

    @Test
    public void testAccountManager_addAccountExplicitlyForCurrentUser() throws Exception {
        AccountManager accountManager = (AccountManager) mContext.getSystemService(
                Context.ACCOUNT_SERVICE);
        assertThat(accountManager.getAccountsByType(MockAuthenticator.ACCOUNT_TYPE)).isEmpty();
        MockAuthenticator.addTestAccount(mContext);
        assertThat(accountManager.getAccountsByType(MockAuthenticator.ACCOUNT_TYPE)).hasLength(1);
    }

    @Test
    public void testRemoveAccountExplicitly() throws Exception {
        AccountManager accountManager = (AccountManager) mContext.getSystemService(
                Context.ACCOUNT_SERVICE);

        assertThat(accountManager.getAccountsByType(MockAuthenticator.ACCOUNT_TYPE)).hasLength(1);
        accountManager.removeAccountExplicitly(TEST_ACCOUNT);
        assertThat(accountManager.getAccountsByType(MockAuthenticator.ACCOUNT_TYPE)).isEmpty();
    }

    @Test
    public void testAccountManager_getAccountsForCurrentUser() throws Exception {
        AccountManager accountManager = (AccountManager) mContext.getSystemService(
                Context.ACCOUNT_SERVICE);

        Account[] accounts = accountManager.getAccounts();

        final int numAccountsExpected = getNumAccountsExpected();
        assertThat(accounts).hasLength(numAccountsExpected);

        assertThat(accountManager.getAccountsByType(MockAuthenticator.ACCOUNT_TYPE)).hasLength(
                numAccountsExpected);
    }

    @Test
    public void testAccountManager_failCreateContextAsUserRevokePermissions() throws Exception {
        UserHandle profileHandle = UserHandle.of(getTestUser());

        try {
            assertPermissionRevoked("android.permission.INTERACT_ACROSS_USERS");
            AccountManager accountManagerAsUser = (AccountManager) mContext.createContextAsUser(
                    profileHandle, 0).getSystemService(Context.ACCOUNT_SERVICE);

            fail("Should have received a security exception");
        } catch (SecurityException e) {
        }
    }

    @Test
    public void testAccountManager_createContextAsUserForCurrentUserRevokePermissions()
            throws Exception {
        UserHandle profileHandle = UserHandle.of(getTestUser());

        assertPermissionRevoked("android.permission.INTERACT_ACROSS_USERS");

        AccountManager accountManagerAsUser = (AccountManager) mContext.createContextAsUser(
                profileHandle, 0).getSystemService(Context.ACCOUNT_SERVICE);

        assertThat(accountManagerAsUser).isNotNull();
    }

    private void assertPermissionRevoked(String permission) throws Exception {
        assertThat(mContext.getPackageManager().checkPermission(permission,
                mContext.getPackageName())).isEqualTo(PackageManager.PERMISSION_DENIED);
    }

}
