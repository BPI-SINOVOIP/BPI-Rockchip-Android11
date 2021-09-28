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

import android.app.ActivityTaskManager;
import android.app.IActivityTaskManager;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;
import android.os.UserManager;
import android.preference.PreferenceActivity;
import android.provider.DocumentsContract;
import android.security.Credentials;
import android.security.KeyChain;
import android.util.Log;
import android.widget.Toast;

import libcore.io.IoUtils;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

/**
 * The main class for installing certificates to the system keystore. It reacts
 * to the public {@link Credentials#INSTALL_ACTION} intent.
 */
public class CertInstallerMain extends PreferenceActivity {
    private static final String TAG = "CertInstaller";

    private static final int REQUEST_INSTALL = 1;
    private static final int REQUEST_OPEN_DOCUMENT = 2;
    private static final int REQUEST_CONFIRM_CREDENTIALS = 3;

    private static final String INSTALL_CERT_AS_USER_CLASS = ".InstallCertAsUser";

    public static final String WIFI_CONFIG = "wifi-config";
    public static final String WIFI_CONFIG_DATA = "wifi-config-data";
    public static final String WIFI_CONFIG_FILE = "wifi-config-file";

    private static Map<String,String> MIME_MAPPINGS = new HashMap<>();

    static {
            MIME_MAPPINGS.put("application/x-x509-ca-cert", KeyChain.EXTRA_CERTIFICATE);
            MIME_MAPPINGS.put("application/x-x509-user-cert", KeyChain.EXTRA_CERTIFICATE);
            MIME_MAPPINGS.put("application/x-x509-server-cert", KeyChain.EXTRA_CERTIFICATE);
            MIME_MAPPINGS.put("application/x-pem-file", KeyChain.EXTRA_CERTIFICATE);
            MIME_MAPPINGS.put("application/pkix-cert", KeyChain.EXTRA_CERTIFICATE);
            MIME_MAPPINGS.put("application/x-pkcs12", KeyChain.EXTRA_PKCS12);
            MIME_MAPPINGS.put("application/x-wifi-config", WIFI_CONFIG);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setResult(RESULT_CANCELED);

        UserManager userManager = (UserManager) getSystemService(Context.USER_SERVICE);
        if (userManager.hasUserRestriction(UserManager.DISALLOW_CONFIG_CREDENTIALS)
                || userManager.isGuestUser()) {
            finish();
            return;
        }

        final Intent intent = getIntent();
        final String action = intent.getAction();

        if (Credentials.INSTALL_ACTION.equals(action)
                || Credentials.INSTALL_AS_USER_ACTION.equals(action)) {
            Bundle bundle = intent.getExtras();

            /*
             * There is a special INSTALL_AS_USER action that this activity is
             * aliased to, but you have to have a permission to call it. If the
             * caller got here any other way, remove the extra that we allow in
             * that INSTALL_AS_USER path.
             */
            String calledClass = intent.getComponent().getClassName();
            String installAsUserClassName = getPackageName() + INSTALL_CERT_AS_USER_CLASS;
            if (bundle != null && !installAsUserClassName.equals(calledClass)) {
                bundle.remove(Credentials.EXTRA_INSTALL_AS_UID);
            }

            // If bundle is empty of any actual credentials, ask user to open.
            // Otherwise, pass extras to CertInstaller to install those credentials.
            // Either way, we use KeyChain.EXTRA_NAME as the default name if available.
            if (nullOrEmptyBundle(bundle) || bundleContainsNameOnly(bundle)
                    || bundleContainsInstallAsUidOnly(bundle)
                    || bundleContainsExtraCertificateUsageOnly(bundle)) {

                // Confirm credentials if there's only a CA certificate
                if (installingCaCertificate(bundle)) {
                    confirmDeviceCredential();
                } else {
                    startOpenDocumentActivity();
                }
            } else {
                startInstallActivity(intent);
            }
        } else if (Intent.ACTION_VIEW.equals(action)) {
            startInstallActivity(intent.getType(), intent.getData());
        }
    }

    private boolean nullOrEmptyBundle(Bundle bundle) {
        return bundle == null || bundle.isEmpty();
    }

    private boolean bundleContainsNameOnly(Bundle bundle) {
        return bundle.size() == 1 && bundle.containsKey(KeyChain.EXTRA_NAME);
    }

    private boolean bundleContainsInstallAsUidOnly(Bundle bundle) {
        return bundle.size() == 1 && bundle.containsKey(Credentials.EXTRA_INSTALL_AS_UID);
    }

    private boolean bundleContainsExtraCertificateUsageOnly(Bundle bundle) {
        return bundle.size() == 1 && bundle.containsKey(Credentials.EXTRA_CERTIFICATE_USAGE);
    }

    private boolean installingCaCertificate(Bundle bundle) {
        return bundle != null && bundle.size() == 1 && Credentials.CERTIFICATE_USAGE_CA.equals(
                bundle.getString(Credentials.EXTRA_CERTIFICATE_USAGE));
    }

    private void confirmDeviceCredential() {
        KeyguardManager keyguardManager = getSystemService(KeyguardManager.class);
        Intent intent = keyguardManager.createConfirmDeviceCredentialIntent(null,
                null);
        if (intent == null) { // No screenlock
            startOpenDocumentActivity();
        } else {
            startActivityForResult(intent, REQUEST_CONFIRM_CREDENTIALS);
        }
    }

    // The maximum amount of data to read into memory before aborting.
    // Without a limit, a sufficiently-large file will run us out of memory.  A
    // typical certificate or WiFi config is under 10k, so 10MiB should be more
    // than sufficient.  See b/32320490.
    private static final int READ_LIMIT = 10 * 1024 * 1024;

    /**
     * Reads the given InputStream until EOF or more than READ_LIMIT bytes have
     * been read, whichever happens first.  If the maximum limit is reached, throws
     * IOException.
     */
    private static byte[] readWithLimit(InputStream in) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        byte[] buffer = new byte[1024];
        int bytesRead = 0;
        int count;
        while ((count = in.read(buffer)) != -1) {
            bytes.write(buffer, 0, count);
            bytesRead += count;
            if (bytesRead > READ_LIMIT) {
                throw new IOException("Data file exceeded maximum size.");
            }
        }
        return bytes.toByteArray();
    }

    private void startInstallActivity(Intent intent) {
        final Intent installIntent = new Intent(this, CertInstaller.class);
        if (intent.getExtras() != null && intent.getExtras().getString(Intent.EXTRA_REFERRER)
                != null) {
            Log.v(TAG, String.format(
                    "Removing referrer extra with value %s which was not meant to be included",
                    intent.getBundleExtra(Intent.EXTRA_REFERRER)));
            intent.removeExtra(Intent.EXTRA_REFERRER);
        }
        installIntent.putExtras(intent);

        try {
            // The referrer is passed as an extra because the launched-from package needs to be
            // obtained here and not in the CertInstaller.
            // It is also safe to add the referrer as an extra because the CertInstaller activity
            // is not exported, which means it cannot be called from other apps.
            IActivityTaskManager activityTaskManager = ActivityTaskManager.getService();
            installIntent.putExtra(Intent.EXTRA_REFERRER,
                    activityTaskManager.getLaunchedFromPackage(getActivityToken()));
            startActivityForResult(installIntent, REQUEST_INSTALL);
        } catch (RemoteException e) {
            Log.v(TAG, "Could not talk to activity manager.", e);
            Toast.makeText(this, R.string.cert_temp_error, Toast.LENGTH_LONG).show();
            finish();
        }
    }

    private void startInstallActivity(String mimeType, Uri uri) {
        if (mimeType == null) {
            mimeType = getContentResolver().getType(uri);
        }

        String target = MIME_MAPPINGS.get(mimeType);
        if (target == null) {
            throw new IllegalArgumentException("Unknown MIME type: " + mimeType);
        }

        if (WIFI_CONFIG.equals(target)) {
            startWifiInstallActivity(mimeType, uri);
        }
        else {
            InputStream in = null;
            try {
                in = getContentResolver().openInputStream(uri);

                final byte[] raw = readWithLimit(in);

                Intent intent = getIntent();
                intent.putExtra(target, raw);
                startInstallActivity(intent);
            } catch (IOException e) {
                Log.e(TAG, "Failed to read certificate: " + e);
                Toast.makeText(this, R.string.cert_read_error, Toast.LENGTH_LONG).show();
            } finally {
                IoUtils.closeQuietly(in);
            }
        }
    }

    private void startWifiInstallActivity(String mimeType, Uri uri) {
        Intent intent = new Intent(this, WiFiInstaller.class);
        try (BufferedInputStream in =
                     new BufferedInputStream(getContentResolver().openInputStream(uri))) {
            byte[] data = readWithLimit(in);
            intent.putExtra(WIFI_CONFIG_FILE, uri.toString());
            intent.putExtra(WIFI_CONFIG_DATA, data);
            intent.putExtra(WIFI_CONFIG, mimeType);
            startActivityForResult(intent, REQUEST_INSTALL);
        } catch (IOException e) {
            Log.e(TAG, "Failed to read wifi config: " + e);
            Toast.makeText(this, R.string.cert_read_error, Toast.LENGTH_LONG).show();
        }
    }

    private void startOpenDocumentActivity() {
        final String[] mimeTypes = MIME_MAPPINGS.keySet().toArray(new String[0]);
        final Intent openIntent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        openIntent.setType("*/*");
        openIntent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
        openIntent.putExtra(DocumentsContract.EXTRA_SHOW_ADVANCED, true);
        startActivityForResult(openIntent, REQUEST_OPEN_DOCUMENT);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case REQUEST_INSTALL:
                setResult(resultCode);
                finish();
                break;
            case REQUEST_OPEN_DOCUMENT:
                if (resultCode == RESULT_OK) {
                    startInstallActivity(null, data.getData());
                } else {
                    finish();
                }
                break;
            case REQUEST_CONFIRM_CREDENTIALS:
                if (resultCode == RESULT_OK) {
                    startOpenDocumentActivity();
                    return;
                }
                // Failed to confirm credentials, do nothing.
                finish();
                break;
            default:
                Log.w(TAG, "unknown request code: " + requestCode);
                break;
        }
    }
}
