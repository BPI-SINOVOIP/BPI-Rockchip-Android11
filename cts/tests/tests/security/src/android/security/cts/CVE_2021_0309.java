/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.security.cts;

import static org.junit.Assert.assertFalse;

import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.GrantCredentialsPermissionActivity;
import android.accounts.IAccountAuthenticatorResponse;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.RemoteException;
import android.platform.test.annotations.SecurityTest;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class CVE_2021_0309 {
    private final Context mContext = InstrumentationRegistry.getContext();
    boolean isVulnerable = true;

    /**
     * b/159145361
     */
    @SecurityTest(minPatchLevel = "2021-01")
    @Test
    public void testPocCVE_2021_0309() {
        /**
         * Output of adb shell pm list packages --user 0 -U com.android.providers.media
         * package:com.android.providers.media uid:10008
         */
        final int REQUESTED_UID = 10008;
        Intent intent = new Intent();
        intent.setClassName("android",
                            "android.accounts.GrantCredentialsPermissionActivity");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(GrantCredentialsPermissionActivity.EXTRAS_ACCOUNT,
                        new Account("abc@xyz.org", "com.my.auth"));
        intent.putExtra(GrantCredentialsPermissionActivity.EXTRAS_AUTH_TOKEN_TYPE,
                        AccountManager.ACCOUNT_ACCESS_TOKEN_TYPE);
        intent.putExtra(GrantCredentialsPermissionActivity.EXTRAS_RESPONSE,
            new AccountAuthenticatorResponse(new IAccountAuthenticatorResponse.Stub() {
                @Override
                public void onResult(Bundle value) throws RemoteException {
                }

                @Override
                public void onRequestContinued() {
                }

                @Override
                public void onError(int errorCode, String errorMessage) throws RemoteException {
                    /**
                     * GrantCredentialsPermissionActivity's onCreate() should not execute and
                     * should return error when the requested UID does not match the process's UID
                     */
                    isVulnerable = false;
                }
            }));

        intent.putExtra(GrantCredentialsPermissionActivity.EXTRAS_REQUESTING_UID,
                        REQUESTED_UID);
        mContext.startActivity(intent);
        /**
         * Sleep for 5 seconds to ensure that the AccountAuthenticatorResponse callback gets
         * triggered if vulnerability is fixed.
         */
        try {
            Thread.sleep(5000);
        } catch (Exception e) {
        }
        assertFalse(isVulnerable);
    }
}
