/*
 * Copyright (C) 2009 The Android Open Source Project
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

package com.android.certinstaller;

import static android.view.WindowManager.LayoutParams.SYSTEM_FLAG_HIDE_NON_SYSTEM_OVERLAY_WINDOWS;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.security.Credentials;
import android.security.KeyChain;
import android.security.KeyChain.KeyChainConnection;
import android.text.TextUtils;
import android.util.Log;
import android.util.Slog;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.Toast;

import java.io.Serializable;

/**
 * Installs certificates to the system keystore.
 */
public class CertInstaller extends Activity {
    private static final String TAG = "CertInstaller";

    private static final int STATE_INIT = 1;
    private static final int STATE_RUNNING = 2;
    private static final int STATE_PAUSED = 3;

    private static final int NAME_CREDENTIAL_DIALOG = 1;
    private static final int PKCS12_PASSWORD_DIALOG = 2;
    private static final int PROGRESS_BAR_DIALOG = 3;
    private static final int REDIRECT_CA_CERTIFICATE_DIALOG = 4;
    private static final int SELECT_CERTIFICATE_USAGE_DIALOG = 5;
    private static final int INVALID_CERTIFICATE_DIALOG = 6;

    private static final int REQUEST_SYSTEM_INSTALL_CODE = 1;

    // key to states Bundle
    private static final String NEXT_ACTION_KEY = "na";

    private final ViewHelper mView = new ViewHelper();

    private int mState;
    private CredentialHelper mCredentials;
    private MyAction mNextAction;

    private CredentialHelper createCredentialHelper(Intent intent) {
        try {
            return new CredentialHelper(intent);
        } catch (Throwable t) {
            Log.w(TAG, "createCredentialHelper", t);
            toastErrorAndFinish(R.string.invalid_cert);
            return new CredentialHelper();
        }
    }

    @Override
    protected void onCreate(Bundle savedStates) {
        super.onCreate(savedStates);
        getWindow().addSystemFlags(SYSTEM_FLAG_HIDE_NON_SYSTEM_OVERLAY_WINDOWS);

        mCredentials = createCredentialHelper(getIntent());

        mState = (savedStates == null) ? STATE_INIT : STATE_RUNNING;

        if (mState == STATE_INIT) {
            if (!mCredentials.containsAnyRawData()) {
                toastErrorAndFinish(R.string.no_cert_to_saved);
                finish();
            } else {
                if (installingCaCertificate()) {
                    extractPkcs12OrInstall();
                } else {
                    if (mCredentials.hasUserCertificate() && !mCredentials.hasPrivateKey()) {
                        toastErrorAndFinish(R.string.action_missing_private_key);
                    } else if (mCredentials.hasPrivateKey() && !mCredentials.hasUserCertificate()) {
                        toastErrorAndFinish(R.string.action_missing_user_cert);
                    } else {
                        extractPkcs12OrInstall();
                    }
                }
            }
        } else {
            mCredentials.onRestoreStates(savedStates);
            mNextAction = (MyAction)
                    savedStates.getSerializable(NEXT_ACTION_KEY);
        }
    }

    private boolean installingCaCertificate() {
        return mCredentials.hasCaCerts() && !mCredentials.hasPrivateKey() &&
                !mCredentials.hasUserCertificate();
    }

    @Override
    protected void onResume() {
        super.onResume();

        if (mState == STATE_INIT) {
            mState = STATE_RUNNING;
        } else {
            if (mNextAction != null) {
                mNextAction.run(this);
            }
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        mState = STATE_PAUSED;
    }

    @Override
    protected void onSaveInstanceState(Bundle outStates) {
        super.onSaveInstanceState(outStates);
        mCredentials.onSaveStates(outStates);
        if (mNextAction != null) {
            outStates.putSerializable(NEXT_ACTION_KEY, mNextAction);
        }
    }

    @Override
    protected Dialog onCreateDialog (int dialogId) {
        switch (dialogId) {
            case PKCS12_PASSWORD_DIALOG:
                return createPkcs12PasswordDialog();

            case NAME_CREDENTIAL_DIALOG:
                return createNameCertificateDialog();

            case PROGRESS_BAR_DIALOG:
                ProgressDialog dialog = new ProgressDialog(this);
                dialog.setMessage(getString(R.string.extracting_pkcs12));
                dialog.setIndeterminate(true);
                dialog.setCancelable(false);
                return dialog;

            case REDIRECT_CA_CERTIFICATE_DIALOG:
                return createRedirectCaCertificateDialog();

            case SELECT_CERTIFICATE_USAGE_DIALOG:
                return createSelectCertificateUsageDialog();

            case INVALID_CERTIFICATE_DIALOG:
                return createInvalidCertificateDialog();

            default:
                return null;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case REQUEST_SYSTEM_INSTALL_CODE:
                if (resultCode != RESULT_OK) {
                    Log.d(TAG, "credential not saved, err: " + resultCode);
                    toastErrorAndFinish(R.string.cert_not_saved);
                    return;
                }

                Log.d(TAG, "credential is added: " + mCredentials.getName());
                if (mCredentials.getCertUsageSelected().equals(Credentials.CERTIFICATE_USAGE_WIFI)) {
                    Toast.makeText(this, R.string.wifi_cert_is_added, Toast.LENGTH_LONG).show();
                } else {
                    Toast.makeText(this, R.string.user_cert_is_added, Toast.LENGTH_LONG).show();
                }
                setResult(RESULT_OK);
                finish();
                break;
            default:
                Log.w(TAG, "unknown request code: " + requestCode);
                finish();
                break;
        }
    }

    private void extractPkcs12OrInstall() {
        if (mCredentials.hasPkcs12KeyStore()) {
            if (mCredentials.hasPassword()) {
                showDialog(PKCS12_PASSWORD_DIALOG);
            } else {
                new Pkcs12ExtractAction("").run(this);
            }
        } else {
            if (mCredentials.calledBySettings()) {
                MyAction action = new InstallOthersAction();
                action.run(this);
            } else {
                createRedirectOrSelectUsageDialog();
            }
        }
    }

    private class InstallVpnAndAppsTrustAnchorsTask extends AsyncTask<Void, Void, Boolean> {

        @Override
        protected Boolean doInBackground(Void... unused) {
            try {
                try (KeyChainConnection keyChainConnection = KeyChain.bind(CertInstaller.this)) {
                    return mCredentials.installVpnAndAppsTrustAnchors(CertInstaller.this,
                            keyChainConnection.getService());
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return false;
            }
        }

        @Override
        protected void onPostExecute(Boolean success) {
            if (success) {
                Toast.makeText(getApplicationContext(), R.string.ca_cert_is_added,
                        Toast.LENGTH_LONG).show();
                setResult(RESULT_OK);
            }
            finish();
        }
    }

    private void installOthers() {
        // Sanity check: Check that there's either:
        // * A private key AND a user certificate, or
        // * A CA cert.
        boolean hasPrivateKeyAndUserCertificate =
                mCredentials.hasPrivateKey() && mCredentials.hasUserCertificate();
        boolean hasCaCertificate = mCredentials.hasCaCerts();
        Log.d(TAG,
                String.format(
                        "Attempting credentials installation, has ca cert? %b, has user cert? %b",
                        hasCaCertificate, hasPrivateKeyAndUserCertificate));
        if (!(hasPrivateKeyAndUserCertificate || hasCaCertificate)) {
            finish();
            return;
        }

        if (validCertificateSelected()) {
            installCertificateOrShowNameDialog();
        } else {
            showDialog(INVALID_CERTIFICATE_DIALOG);
        }
    }

    private boolean validCertificateSelected() {
        switch (mCredentials.getCertUsageSelected()) {
            case Credentials.CERTIFICATE_USAGE_CA:
                return mCredentials.hasOnlyVpnAndAppsTrustAnchors();
            case Credentials.CERTIFICATE_USAGE_USER:
                return mCredentials.hasUserCertificate()
                        && !mCredentials.hasOnlyVpnAndAppsTrustAnchors();
            case Credentials.CERTIFICATE_USAGE_WIFI:
                return true;
            default:
                return false;
        }
    }

    private void installCertificateOrShowNameDialog() {
        if (!mCredentials.hasAnyForSystemInstall()) {
            toastErrorAndFinish(R.string.no_cert_to_saved);
        } else if (mCredentials.hasOnlyVpnAndAppsTrustAnchors()) {
            // If there's only a CA certificate to install, then it's going to be used
            // as a trust anchor. Install it and skip importing to Keystore.

            // more work to do, don't finish just yet
            new InstallVpnAndAppsTrustAnchorsTask().execute();
        } else {
            // Name is required if installing User certificate
            showDialog(NAME_CREDENTIAL_DIALOG);
        }
    }

    private void extractPkcs12InBackground(final String password) {
        // show progress bar and extract certs in a background thread
        showDialog(PROGRESS_BAR_DIALOG);

        new AsyncTask<Void,Void,Boolean>() {
            @Override protected Boolean doInBackground(Void... unused) {
                return mCredentials.extractPkcs12(password);
            }
            @Override protected void onPostExecute(Boolean success) {
                MyAction action = new OnExtractionDoneAction(success);
                if (mState == STATE_PAUSED) {
                    // activity is paused; run it in next onResume()
                    mNextAction = action;
                } else {
                    action.run(CertInstaller.this);
                }
            }
        }.execute();
    }

    private void onExtractionDone(boolean success) {
        mNextAction = null;
        removeDialog(PROGRESS_BAR_DIALOG);
        if (success) {
            removeDialog(PKCS12_PASSWORD_DIALOG);
            if (mCredentials.calledBySettings()) {
                if (validCertificateSelected()) {
                    installCertificateOrShowNameDialog();
                } else {
                    showDialog(INVALID_CERTIFICATE_DIALOG);
                }
            } else {
                createRedirectOrSelectUsageDialog();
            }
        } else {
            showDialog(PKCS12_PASSWORD_DIALOG);
            mView.setText(R.id.credential_password, "");
            mView.showError(R.string.password_error);
        }
    }

    private void createRedirectOrSelectUsageDialog() {
        if (mCredentials.hasOnlyVpnAndAppsTrustAnchors()) {
            showDialog(REDIRECT_CA_CERTIFICATE_DIALOG);
        } else {
            showDialog(SELECT_CERTIFICATE_USAGE_DIALOG);
        }
    }

    public CharSequence getCallingAppLabel() {
        final String callingPkg = mCredentials.getReferrer();
        if (callingPkg == null) {
            Log.e(TAG, "Cannot get calling calling AppPackage");
            return null;
        }

        final PackageManager pm = getPackageManager();
        final ApplicationInfo appInfo;
        try {
            appInfo = pm.getApplicationInfo(callingPkg, PackageManager.MATCH_DISABLED_COMPONENTS);
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Unable to find info for package: " + callingPkg);
            return null;
        }

        return appInfo.loadLabel(pm);
    }

    private Dialog createRedirectCaCertificateDialog() {
        final String message = getString(
                R.string.redirect_ca_certificate_with_app_info_message, getCallingAppLabel());
        Dialog d = new AlertDialog.Builder(this)
                .setTitle(R.string.redirect_ca_certificate_title)
                .setMessage(message)
                .setPositiveButton(R.string.redirect_ca_certificate_close_button,
                        (dialog, id) -> toastErrorAndFinish(R.string.cert_not_saved))
                .create();
        d.setOnCancelListener(dialog -> toastErrorAndFinish(R.string.cert_not_saved));
        return d;
    }

    private Dialog createSelectCertificateUsageDialog() {
        ViewGroup view = (ViewGroup) View.inflate(this, R.layout.select_certificate_usage_dialog,
                null);
        mView.setView(view);

        RadioGroup radioGroup = view.findViewById(R.id.certificate_usage);
        radioGroup.setOnCheckedChangeListener((group, checkedId) -> {
            switch (checkedId) {
                case R.id.user_certificate:
                    mCredentials.setCertUsageSelectedAndUid(Credentials.CERTIFICATE_USAGE_USER);
                    break;
                case R.id.wifi_certificate:
                    mCredentials.setCertUsageSelectedAndUid(Credentials.CERTIFICATE_USAGE_WIFI);
                default:
                    Slog.i(TAG, "Unknown selection for scope");
            }
        });


        final Context appContext = getApplicationContext();
        Dialog d = new AlertDialog.Builder(this)
                .setView(view)
                .setPositiveButton(android.R.string.ok, (dialog, id) -> {
                    showDialog(NAME_CREDENTIAL_DIALOG);
                })
                .setNegativeButton(android.R.string.cancel,
                        (dialog, id) -> toastErrorAndFinish(R.string.cert_not_saved))
                .create();
        d.setOnCancelListener(dialog -> toastErrorAndFinish(R.string.cert_not_saved));
        return d;
    }

    private Dialog createInvalidCertificateDialog() {
        Dialog d = new AlertDialog.Builder(this)
                .setTitle(R.string.invalid_certificate_title)
                .setMessage(getString(R.string.invalid_certificate_message,
                        getCertificateUsageName()))
                .setPositiveButton(R.string.invalid_certificate_close_button,
                        (dialog, id) -> toastErrorAndFinish(R.string.cert_not_saved))
                .create();
        d.setOnCancelListener(dialog -> finish());
        return d;
    }

    String getCertificateUsageName() {
        switch (mCredentials.getCertUsageSelected()) {
            case Credentials.CERTIFICATE_USAGE_CA:
                return getString(R.string.ca_certificate);
            case Credentials.CERTIFICATE_USAGE_USER:
                return getString(R.string.user_certificate);
            case Credentials.CERTIFICATE_USAGE_WIFI:
                return getString(R.string.wifi_certificate);
            default:
                return getString(R.string.certificate);
        }
    }

    private Dialog createPkcs12PasswordDialog() {
        View view = View.inflate(this, R.layout.password_dialog, null);
        mView.setView(view);
        if (mView.getHasEmptyError()) {
            mView.showError(R.string.password_empty_error);
            mView.setHasEmptyError(false);
        }

        String title = mCredentials.getName();
        title = TextUtils.isEmpty(title)
                ? getString(R.string.pkcs12_password_dialog_title)
                : getString(R.string.pkcs12_file_password_dialog_title, title);
        Dialog d = new AlertDialog.Builder(this)
                .setView(view)
                .setTitle(title)
                .setPositiveButton(android.R.string.ok, (dialog, id) -> {
                    String password = mView.getText(R.id.credential_password);
                    mNextAction = new Pkcs12ExtractAction(password);
                    mNextAction.run(CertInstaller.this);
                 })
                .setNegativeButton(android.R.string.cancel,
                        (dialog, id) -> toastErrorAndFinish(R.string.cert_not_saved))
                .create();
        d.setOnCancelListener(dialog -> toastErrorAndFinish(R.string.cert_not_saved));
        return d;
    }

    private Dialog createNameCertificateDialog() {
        ViewGroup view = (ViewGroup) View.inflate(this, R.layout.name_certificate_dialog, null);
        mView.setView(view);
        if (mView.getHasEmptyError()) {
            mView.showError(R.string.name_empty_error);
            mView.setHasEmptyError(false);
        }
        final EditText nameInput = view.findViewById(R.id.certificate_name);
        nameInput.setText(getDefaultName());
        nameInput.selectAll();
        final Context appContext = getApplicationContext();

        Dialog d = new AlertDialog.Builder(this)
                .setView(view)
                .setTitle(R.string.name_credential_dialog_title)
                .setPositiveButton(android.R.string.ok, (dialog, id) -> {
                    String name = mView.getText(R.id.certificate_name);
                    if (TextUtils.isEmpty(name)) {
                        mView.setHasEmptyError(true);
                        removeDialog(NAME_CREDENTIAL_DIALOG);
                        showDialog(NAME_CREDENTIAL_DIALOG);
                    } else {
                        removeDialog(NAME_CREDENTIAL_DIALOG);
                        mCredentials.setName(name);
                        installCertificateToKeystore(appContext);
                    }
                })
                .setNegativeButton(android.R.string.cancel,
                        (dialog, id) -> toastErrorAndFinish(R.string.cert_not_saved))
                .create();
        d.setOnCancelListener(dialog -> toastErrorAndFinish(R.string.cert_not_saved));
        return d;
    }

    private void installCertificateToKeystore(Context context) {
        try {
            startActivityForResult(
                    mCredentials.createSystemInstallIntent(context),
                    REQUEST_SYSTEM_INSTALL_CODE);
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "installCertificateToKeystore(): ", e);
            toastErrorAndFinish(R.string.cert_not_saved);
        }
    }

    private String getDefaultName() {
        String name = mCredentials.getName();
        if (TextUtils.isEmpty(name)) {
            return null;
        } else {
            // remove the extension from the file name
            int index = name.lastIndexOf(".");
            if (index > 0) name = name.substring(0, index);
            return name;
        }
    }

    private void toastErrorAndFinish(int msgId) {
        Toast.makeText(this, msgId, Toast.LENGTH_SHORT).show();
        finish();
    }

    private interface MyAction extends Serializable {
        void run(CertInstaller host);
    }

    private static class Pkcs12ExtractAction implements MyAction {
        private final String mPassword;
        private transient boolean hasRun;

        Pkcs12ExtractAction(String password) {
            mPassword = password;
        }

        public void run(CertInstaller host) {
            if (hasRun) {
                return;
            }
            hasRun = true;
            host.extractPkcs12InBackground(mPassword);
        }
    }

    private static class InstallOthersAction implements MyAction {
        public void run(CertInstaller host) {
            host.mNextAction = null;
            host.installOthers();
        }
    }

    private static class OnExtractionDoneAction implements MyAction {
        private final boolean mSuccess;

        OnExtractionDoneAction(boolean success) {
            mSuccess = success;
        }

        public void run(CertInstaller host) {
            host.onExtractionDone(mSuccess);
        }
    }
}
