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

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.NetworkErrorException;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;

import java.util.Arrays;

/**
 * Authenticator for cross-user accounts test.
 */
public class MockAuthenticator extends Service {
    private static final String TAG = "MockAuthenticator";

    static final String ACCOUNT_NAME = "TestAccountCts";
    static final String ACCOUNT_TYPE = "com.android.cts.accountmanager";

    private static MockAccountAuthenticator sInstance;

    @Override
    public IBinder onBind(Intent intent) {
        if (sInstance == null) {
            sInstance = new MockAccountAuthenticator(getApplicationContext());

        }
        return sInstance.getIBinder();
    }

    public static Account getTestAccount() {
        return new Account(ACCOUNT_NAME, ACCOUNT_TYPE);
    }

    /**
     * Adds the test account, if it doesn't exist yet.
     */
    public static void addTestAccount(Context context) {
        final Account account = getTestAccount();

        Bundle result = new Bundle();
        result.putString(AccountManager.KEY_ACCOUNT_TYPE, account.type);
        result.putString(AccountManager.KEY_ACCOUNT_NAME, account.name);

        final AccountManager am = context.getSystemService(AccountManager.class);

        if (!Arrays.asList(am.getAccountsByType(account.type)).contains(account) ){
            am.addAccountExplicitly(account, "password", new Bundle());
        }
    }

    public static class MockAccountAuthenticator extends AbstractAccountAuthenticator {

        static final String AUTH_TOKEN = "mockAuthToken";
        private static final String AUTH_TOKEN_LABEL = "mockAuthTokenLabel";

        final private Context mContext;

        private MockAccountAuthenticator(Context context) {
            super(context);
            mContext = context;
        }

        private Bundle createResultBundle() {
            Bundle result = new Bundle();
            result.putString(AccountManager.KEY_ACCOUNT_NAME, ACCOUNT_NAME);
            result.putString(AccountManager.KEY_ACCOUNT_TYPE, ACCOUNT_TYPE);
            result.putString(AccountManager.KEY_AUTHTOKEN, AUTH_TOKEN);
            return result;
        }

        @Override
        public Bundle addAccount(AccountAuthenticatorResponse response, String accountType,
                String authTokenType, String[] requiredFeatures, Bundle options)
                throws NetworkErrorException {

            final AccountManager am = mContext.getSystemService(AccountManager.class);
            am.addAccountExplicitly(getTestAccount(), "fakePassword", null);

            return createResultBundle();
        }

        @Override
        public Bundle editProperties(AccountAuthenticatorResponse response, String accountType) {
            return createResultBundle();
        }

        @Override
        public Bundle updateCredentials(AccountAuthenticatorResponse response, Account account,
                String authTokenType, Bundle options) throws NetworkErrorException {
            return createResultBundle();
        }

        @Override
        public Bundle confirmCredentials(AccountAuthenticatorResponse response, Account account,
                Bundle options) throws NetworkErrorException {

            Bundle result = new Bundle();
            result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, true);
            return result;
        }

        @Override
        public Bundle getAuthToken(AccountAuthenticatorResponse response, Account account,
                String authTokenType, Bundle options) throws NetworkErrorException {
            return createResultBundle();
        }

        @Override
        public String getAuthTokenLabel(String authTokenType) {
            return AUTH_TOKEN_LABEL;
        }

        @Override
        public Bundle hasFeatures(AccountAuthenticatorResponse response, Account account,
                String[] features) throws NetworkErrorException {

            Bundle result = new Bundle();
            result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, true);
            return result;
        }

    }
}
