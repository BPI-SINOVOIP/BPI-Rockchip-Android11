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

package com.android.car.settings.security;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Process;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.security.Credentials;
import android.security.KeyChain;
import android.security.KeyStore;
import android.text.TextUtils;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import com.android.car.settings.R;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.Logger;
import com.android.internal.widget.LockPatternUtils;

import java.lang.ref.WeakReference;

/**
 * Handles resetting and installing keys into {@link KeyStore}. Only Settings and the CertInstaller
 * application are permitted to perform the install action.
 */
public class CredentialStorageActivity extends FragmentActivity {

    private static final Logger LOG = new Logger(CredentialStorageActivity.class);

    private static final String ACTION_INSTALL = "com.android.credentials.INSTALL";
    private static final String ACTION_RESET = "com.android.credentials.RESET";

    private static final String DIALOG_TAG = "com.android.car.settings.security.RESET_CREDENTIALS";
    private static final String CERT_INSTALLER_PKG = "com.android.certinstaller";

    private static final int CONFIRM_CLEAR_SYSTEM_CREDENTIAL_REQUEST = 1;

    private UserManager mUserManager;
    private LockPatternUtils mUtils;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mUserManager = UserManager.get(getApplicationContext());
        mUtils = new LockPatternUtils(this);

        if (mUserManager.hasUserRestriction(UserManager.DISALLOW_CONFIG_CREDENTIALS)) {
            finish();
            return;
        }

        Intent intent = getIntent();
        String action = intent.getAction();
        if (ACTION_RESET.equals(action)) {
            showResetConfirmationDialog();
        } else if (ACTION_INSTALL.equals(action) && checkCallerIsCertInstallerOrSelfInProfile()) {
            Bundle installBundle = intent.getExtras();
            boolean allTasksComplete = installIfAvailable(installBundle);
            if (allTasksComplete) {
                finish();
            }
        }
    }

    private void showResetConfirmationDialog() {
        ConfirmationDialogFragment dialog = new ConfirmationDialogFragment.Builder(this)
                .setTitle(R.string.credentials_reset)
                .setMessage(R.string.credentials_reset_hint)
                .setPositiveButton(android.R.string.ok, arguments -> onResetConfirmed())
                .setNegativeButton(android.R.string.cancel, arguments -> finish())
                .build();
        dialog.show(getSupportFragmentManager(), DIALOG_TAG);
    }

    private void onResetConfirmed() {
        if (!mUtils.isSecure(UserHandle.myUserId())) {
            new ResetKeyStoreAndKeyChain(this).execute();
        } else {
            startActivityForResult(new Intent(this, CheckLockActivity.class),
                    CONFIRM_CLEAR_SYSTEM_CREDENTIAL_REQUEST);
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == CONFIRM_CLEAR_SYSTEM_CREDENTIAL_REQUEST) {
            if (resultCode == Activity.RESULT_OK) {
                new ResetKeyStoreAndKeyChain(this).execute();
            } else {
                finish();
            }
        }
    }

    /**
     * Check that the caller is either CertInstaller or Settings running in a profile of this user.
     */
    private boolean checkCallerIsCertInstallerOrSelfInProfile() {
        if (TextUtils.equals(CERT_INSTALLER_PKG, getCallingPackage())) {
            // CertInstaller is allowed to install credentials if it has the same signature as
            // Settings package.
            return getPackageManager().checkSignatures(getCallingPackage(), getPackageName())
                    == PackageManager.SIGNATURE_MATCH;
        }

        int launchedFromUserId;
        try {
            int launchedFromUid = android.app.ActivityManager.getService().getLaunchedFromUid(
                    getActivityToken());
            if (launchedFromUid == -1) {
                LOG.e(ACTION_INSTALL + " must be started with startActivityForResult");
                return false;
            }
            if (!UserHandle.isSameApp(launchedFromUid, Process.myUid())) {
                return false;
            }
            launchedFromUserId = UserHandle.getUserId(launchedFromUid);
        } catch (RemoteException e) {
            LOG.w("Unable to verify calling identity. ActivityManager is down.", e);
            return false;
        }

        UserManager userManager = (UserManager) getSystemService(Context.USER_SERVICE);
        UserInfo parentInfo = userManager.getProfileParent(launchedFromUserId);
        // Caller is running in a profile of this user
        return ((parentInfo != null) && (parentInfo.id == UserHandle.myUserId()));
    }

    /**
     * Install credentials if available, otherwise do nothing.
     *
     * @return {@code true} if the installation is done and the activity should be finished, {@code
     * false} if an asynchronous task is pending and will finish the activity when it's done.
     */
    private boolean installIfAvailable(Bundle installBundle) {
        if (installBundle == null || installBundle.isEmpty()) {
            return true;
        }

        int uid = installBundle.getInt(Credentials.EXTRA_INSTALL_AS_UID, KeyStore.UID_SELF);

        // Checks that the provided uid is none of the following:
        // 1. KeyStore.UID_SELF
        // 2. Current uid process
        // 3. uid running as the system process (if in headless system user mode)
        if (uid != KeyStore.UID_SELF && !UserHandle.isSameUser(uid, Process.myUid())
                && !(mUserManager.isHeadlessSystemUserMode()
                && UserHandle.getUserId(uid) == UserHandle.USER_SYSTEM)) {
            int dstUserId = UserHandle.getUserId(uid);

            // Restrict install target to the wifi uid.
            if (uid != Process.WIFI_UID) {
                LOG.e("Failed to install credentials as uid " + uid
                        + ": cross-user installs may only target wifi uids");
                return true;
            }

            Intent installIntent = new Intent(ACTION_INSTALL)
                    .setFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT)
                    .putExtras(installBundle);
            startActivityAsUser(installIntent, new UserHandle(dstUserId));
            return true;
        }

        String alias = installBundle.getString(Credentials.EXTRA_USER_KEY_ALIAS, null);
        if (TextUtils.isEmpty(alias)) {
            LOG.e("Cannot install key without an alias");
            return true;
        }

        final byte[] privateKeyData = installBundle.getByteArray(
                Credentials.EXTRA_USER_PRIVATE_KEY_DATA);
        final byte[] certData = installBundle.getByteArray(Credentials.EXTRA_USER_CERTIFICATE_DATA);
        final byte[] caListData = installBundle.getByteArray(
                Credentials.EXTRA_CA_CERTIFICATES_DATA);
        new InstallKeyInKeyChain(alias, privateKeyData, certData, caListData, uid).execute();

        return false;
    }

    /**
     * Background task to handle reset of both {@link KeyStore} and user installed CAs.
     */
    private static class ResetKeyStoreAndKeyChain extends AsyncTask<Void, Void, Boolean> {

        private final WeakReference<CredentialStorageActivity> mCredentialStorage;

        ResetKeyStoreAndKeyChain(CredentialStorageActivity credentialStorage) {
            mCredentialStorage = new WeakReference<>(credentialStorage);
        }

        @Override
        protected Boolean doInBackground(Void... unused) {
            CredentialStorageActivity credentialStorage = mCredentialStorage.get();
            if (credentialStorage == null || credentialStorage.isFinishing()
                    || credentialStorage.isDestroyed()) {
                return false;
            }

            UserHandle user = getUserHandleToUse(mCredentialStorage.get().mUserManager);
            credentialStorage.mUtils.resetKeyStore(user.getIdentifier());

            try {
                KeyChain.KeyChainConnection keyChainConnection = KeyChain.bindAsUser(
                        credentialStorage, user);
                try {
                    return keyChainConnection.getService().reset();
                } catch (RemoteException e) {
                    LOG.w("Failed to reset KeyChain", e);
                    return false;
                } finally {
                    keyChainConnection.close();
                }
            } catch (InterruptedException e) {
                LOG.w("Failed to reset KeyChain", e);
                Thread.currentThread().interrupt();
                return false;
            }
        }

        @Override
        protected void onPostExecute(Boolean success) {
            CredentialStorageActivity credentialStorage = mCredentialStorage.get();
            if (credentialStorage == null || credentialStorage.isFinishing()
                    || credentialStorage.isDestroyed()) {
                return;
            }
            if (success) {
                Toast.makeText(credentialStorage, R.string.credentials_erased,
                        Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(credentialStorage, R.string.credentials_not_erased,
                        Toast.LENGTH_SHORT).show();
            }
            credentialStorage.finish();
        }
    }

    /**
     * Background task to install a certificate into KeyChain.
     */
    private class InstallKeyInKeyChain extends AsyncTask<Void, Void, Boolean> {
        final String mAlias;
        private final byte[] mKeyData;
        private final byte[] mCertData;
        private final byte[] mCaListData;
        private final int mUid;

        InstallKeyInKeyChain(String alias, byte[] keyData, byte[] certData, byte[] caListData,
                int uid) {
            mAlias = alias;
            mKeyData = keyData;
            mCertData = certData;
            mCaListData = caListData;
            mUid = uid;
        }

        @Override
        protected Boolean doInBackground(Void... unused) {
            try (KeyChain.KeyChainConnection keyChainConnection = KeyChain.bindAsUser(
                    CredentialStorageActivity.this,
                    getUserHandleToUse(CredentialStorageActivity.this.mUserManager))) {
                return keyChainConnection.getService()
                        .installKeyPair(mKeyData, mCertData, mCaListData, mAlias, mUid);
            } catch (RemoteException e) {
                LOG.w(String.format("Failed to install key %s to uid %d", mAlias, mUid), e);
                return false;
            } catch (InterruptedException e) {
                LOG.w(String.format("Interrupted while installing key %s", mAlias), e);
                Thread.currentThread().interrupt();
                return false;
            }
        }

        @Override
        protected void onPostExecute(Boolean result) {
            LOG.i(String.format("Marked alias %s as selectable, success? %s",
                    mAlias, result));
            CredentialStorageActivity.this.onKeyInstalled(mAlias, mUid, result);
        }
    }

    private void onKeyInstalled(String alias, int uid, boolean result) {
        if (!result) {
            LOG.w(String.format("Error installing alias %s for uid %d", alias, uid));
            finish();
            return;
        }

        // Send the broadcast.
        final Intent broadcast = new Intent(KeyChain.ACTION_KEYCHAIN_CHANGED);
        sendBroadcast(broadcast);
        setResult(RESULT_OK);

        if (uid == Process.SYSTEM_UID || uid == KeyStore.UID_SELF) {
            new MarkKeyAsUserSelectable(this, alias).execute();
        } else {
            finish();
        }
    }

    /**
     * Background task to mark a given key alias as user-selectable so that it can be selected by
     * users from the Certificate Selection prompt.
     */
    private static class MarkKeyAsUserSelectable extends AsyncTask<Void, Void, Boolean> {

        private final WeakReference<CredentialStorageActivity> mCredentialStorage;
        private final String mAlias;

        MarkKeyAsUserSelectable(CredentialStorageActivity credentialStorage, String alias) {
            mCredentialStorage = new WeakReference<>(credentialStorage);
            mAlias = alias;
        }

        @Override
        protected Boolean doInBackground(Void... unused) {
            CredentialStorageActivity credentialStorage = mCredentialStorage.get();
            if (credentialStorage == null || credentialStorage.isFinishing()
                    || credentialStorage.isDestroyed()) {
                return false;
            }
            try (KeyChain.KeyChainConnection keyChainConnection = KeyChain.bindAsUser(
                    credentialStorage, getUserHandleToUse(credentialStorage.mUserManager))) {
                keyChainConnection.getService().setUserSelectable(mAlias, true);
                return true;
            } catch (RemoteException e) {
                LOG.w("Failed to mark key " + mAlias + " as user-selectable.", e);
                return false;
            } catch (InterruptedException e) {
                LOG.w("Failed to mark key " + mAlias + " as user-selectable.", e);
                Thread.currentThread().interrupt();
                return false;
            }
        }

        @Override
        protected void onPostExecute(Boolean result) {
            LOG.i(String.format("Marked alias %s as selectable, success? %s", mAlias, result));
            CredentialStorageActivity credentialStorage = mCredentialStorage.get();
            if (credentialStorage == null || credentialStorage.isFinishing()
                    || credentialStorage.isDestroyed()) {
                return;
            }
            credentialStorage.finish();
        }
    }

    private static UserHandle getUserHandleToUse(UserManager userManager) {
        return userManager.isHeadlessSystemUserMode()
                ? UserHandle.SYSTEM : UserHandle.of(ActivityManager.getCurrentUser());
    }
}
