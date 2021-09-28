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
package android.cts.backup.syncadaptersettingsapp;

import static com.android.compatibility.common.util.BackupUtils.LOCAL_TRANSPORT_TOKEN;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Instrumentation;
import android.content.ContentResolver;
import android.content.Context;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.platform.test.annotations.AppModeFull;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.BackupUtils;
import com.android.compatibility.common.util.TestUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.InputStream;
import java.util.Arrays;
import java.util.stream.Collectors;

/**
 * Device side routines to be invoked by the host side SyncAdapterSettingsHostSideTest. These
 * are not designed to be called in any other way, as they rely on state set up by the host side
 * test.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull
public class SyncAdapterSettingsTest {

    /** The name of the package for backup */
    private static final String ANDROID_PACKAGE = "android";

    private static final String ACCOUNT_NAME = "SyncAdapterSettings";
    private static final String ACCOUNT_TYPE = "android.cts.backup.syncadaptersettingsapp";
    private static final int AUTHENTICATOR_POLL_TIMEOUT_SECS = 30;

    private Context mContext;
    private Account mAccount;
    private BackupUtils mBackupUtils =
            new BackupUtils() {
                @Override
                protected InputStream executeShellCommand(String command) {
                    Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
                    ParcelFileDescriptor pfd =
                            instrumentation.getUiAutomation().executeShellCommand(command);
                    return new ParcelFileDescriptor.AutoCloseInputStream(pfd);
                }
            };

    @Before
    public void setUp() {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = instrumentation.getTargetContext();
        mAccount = getAccount();
    }

    /**
     * Test backup and restore of MasterSyncAutomatically=true.
     *
     * Test logic:
     * 1. Enable MasterSyncAutomatically.
     * 2. Backup android package, disable MasterSyncAutomatically and restore android package.
     * 3. Check restored MasterSyncAutomatically=true is the same with backup value.
     */
    @Test
    public void testMasterSyncAutomatically_whenOn_isRestored() throws Exception {
        ContentResolver.setMasterSyncAutomatically(true);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        ContentResolver.setMasterSyncAutomatically(false);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        assertTrue(ContentResolver.getMasterSyncAutomatically());
    }

    /**
     * Test backup and restore of MasterSyncAutomatically=false.
     *
     * Test logic:
     * 1. Disable MasterSyncAutomatically.
     * 2. Backup android package, enable MasterSyncAutomatically and restore android package.
     * 3. Check restored MasterSyncAutomatically=false is the same with backup value.
     */
    @Test
    public void testMasterSyncAutomatically_whenOff_isRestored() throws Exception {
        ContentResolver.setMasterSyncAutomatically(false);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        ContentResolver.setMasterSyncAutomatically(true);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        assertFalse(ContentResolver.getMasterSyncAutomatically());
    }

    /**
     * Test that if syncEnabled=true and isSyncable=1 in restore data, syncEnabled will be true
     * and isSyncable will be left as the current value in the device.
     *
     * Test logic:
     * 1. Add an account if account does not exist and set syncEnabled=true, isSyncable=1.
     * 2. Backup android package, set syncEnabled=false and set isSyncable=0. Then restore
     * android package.
     * 3. Check restored syncEnabled=true and isSyncable=0. Then remove account.
     *
     * @see AccountSyncSettingsBackupHelper#restoreExistingAccountSyncSettingsFromJSON(JSONObject)
     */
    @Test
    public void testIsSyncableChanged_ifTurnOnSyncEnabled() throws Exception {
        addAccount(mContext);
        initSettings(/* masterSyncAutomatically= */true, /* syncAutomatically= */
                true, /* isSyncable= */1);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        setSyncAutomaticallyAndIsSyncable(false, 0);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        assertSettings(/* syncAutomatically= */true, /* isSyncable= */0);
        removeAccount(mContext);
    }

    /**
     * Test that if syncEnabled=false and isSyncable=0 in restore data, syncEnabled will be false
     * and isSyncable will be set to 0.
     *
     * Test logic:
     * 1. Add an account if account does not exist and set syncEnabled=false, isSyncable=0.
     * 2. Backup android package, set syncEnabled=true and set isSyncable=1. Then restore android
     * package.
     * 3. Check restored syncEnabled=false and isSyncable=0. Then remove account.
     *
     * @see AccountSyncSettingsBackupHelper#restoreExistingAccountSyncSettingsFromJSON(JSONObject)
     */
    @Test
    public void testIsSyncableIsZero_ifTurnOffSyncEnabled() throws Exception {
        addAccount(mContext);
        initSettings(/* masterSyncAutomatically= */true, /* syncAutomatically= */
                false, /* isSyncable= */0);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        setSyncAutomaticallyAndIsSyncable(true, 1);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        assertSettings(/* syncAutomatically= */false, /* isSyncable= */0);
        removeAccount(mContext);
    }

    /**
     * Test that if syncEnabled=false and isSyncable=1 in restore data, syncEnabled will be false
     * and isSyncable will be set to 1.
     * According to
     * {@link AccountSyncSettingsBackupHelper#restoreExistingAccountSyncSettingsFromJSON(JSONObject)}
     * isSyncable will be set to 2, but function
     * {@link ContentService#setIsSyncable(Account, String, int)} would call
     * {@link ContentService#normalizeSyncable(int)} and set isSyncable to 1.
     *
     * Test logic:
     * 1. Add an account if account does not exist and set syncEnabled=false, isSyncable=1.
     * 2. Backup android package, set syncEnabled=true and set isSyncable=0. Then restore android
     * package.
     * 3. Check restored syncEnabled=false and isSyncable=1. Then remove account.
     */
    @Test
    public void testIsSyncableIsOne_ifTurnOffSyncEnabled() throws Exception {
        addAccount(mContext);
        initSettings(/* masterSyncAutomatically= */true, /* syncAutomatically= */
                false, /* isSyncable= */1);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        setSyncAutomaticallyAndIsSyncable(true, 0);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        assertSettings(/* syncAutomatically= */false, /* isSyncable= */1);
        removeAccount(mContext);
    }

    private void initSettings(boolean masterSyncAutomatically, boolean syncAutomatically,
            int isSyncable) throws Exception {
        ContentResolver.setMasterSyncAutomatically(masterSyncAutomatically);
        setSyncAutomaticallyAndIsSyncable(syncAutomatically, isSyncable);
    }

    private void setSyncAutomaticallyAndIsSyncable(boolean syncAutomatically, int isSyncable)
            throws Exception {
        ContentResolver.setSyncAutomatically(mAccount, SyncAdapterSettingsProvider.AUTHORITY,
                syncAutomatically);

        ContentResolver.setIsSyncable(mAccount, SyncAdapterSettingsProvider.AUTHORITY, isSyncable);
    }

    private void assertSettings(boolean syncAutomatically, int isSyncable) throws Exception {
        assertEquals(syncAutomatically, ContentResolver.getSyncAutomatically(mAccount,
                SyncAdapterSettingsProvider.AUTHORITY));
        assertEquals(isSyncable,
                ContentResolver.getIsSyncable(mAccount, SyncAdapterSettingsProvider.AUTHORITY));
    }

    /**
     * Get the account.
     */
    private Account getAccount() {
        return new Account(ACCOUNT_NAME, ACCOUNT_TYPE);
    }

    /**
     * Add an account, if it doesn't exist yet.
     */
    private void addAccount(Context context) throws Exception {
        final String password = "password";

        final AccountManager am = context.getSystemService(AccountManager.class);

        if (!Arrays.asList(am.getAccountsByType(mAccount.type)).contains(mAccount)) {
            waitUntilAuthenticatorRegistered();
            am.addAccountExplicitly(mAccount, password, new Bundle());
        }
    }

    private boolean isAuthenticatorRegistered() {
        final AccountManager am = mContext.getSystemService(AccountManager.class);
        return Arrays.stream(am.getAuthenticatorTypes())
                .map(a -> a.type)
                .collect(Collectors.toList())
                .contains(ACCOUNT_TYPE);
    }

    private void waitUntilAuthenticatorRegistered() throws Exception {
        TestUtils.waitUntil("Timeout waiting for authenticator to be registered",
                AUTHENTICATOR_POLL_TIMEOUT_SECS, this::isAuthenticatorRegistered);
    }

    /**
     * Remove the account.
     */
    private void removeAccount(Context context) {
        final AccountManager am = context.getSystemService(AccountManager.class);
        final Account[] accounts = am.getAccountsByType(ACCOUNT_TYPE);

        for (int i = 0, size = accounts.length; i < size; i++) {
            Account account = accounts[i];
            am.removeAccountExplicitly(account);
        }
    }
}
