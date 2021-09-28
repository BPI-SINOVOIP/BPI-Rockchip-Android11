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

package com.android.cts;

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

public class BinderPermissionTestService extends Service {

    private static String TEST_NOT_ALLOWED_MESSAGE = "Test: you're not allowed to do this.";

    private final IBinder mBinder = new IBinderPermissionTestService.Stub() {
        @Override
        public void doEnforceCallingPermission(String permission) {
            enforceCallingPermission(permission, TEST_NOT_ALLOWED_MESSAGE);
        }

        @Override
        public int doCheckCallingPermission(String permission) {
            return checkCallingPermission(permission);
        }

        @Override
        public void doEnforceCallingOrSelfPermission(String permission) {
            enforceCallingOrSelfPermission(permission, TEST_NOT_ALLOWED_MESSAGE);
        }

        @Override
        public int doCheckCallingOrSelfPermission(String permission) {
            return checkCallingOrSelfPermission(permission);
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
