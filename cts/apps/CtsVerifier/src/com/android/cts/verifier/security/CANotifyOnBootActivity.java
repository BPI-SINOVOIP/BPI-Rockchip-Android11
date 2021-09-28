package com.android.cts.verifier.security;

import android.app.admin.DevicePolicyManager;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.Toast;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

public class CANotifyOnBootActivity extends PassFailButtons.Activity {

    private static final String TAG = CANotifyOnBootActivity.class.getSimpleName();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        View view = getLayoutInflater().inflate(R.layout.ca_boot_notify, null);
        Button checkCredsButton = (Button) view.findViewById(R.id.check_creds);
        Button installButton = (Button) view.findViewById(R.id.install);
        checkCredsButton.setOnClickListener(new OpenTrustedCredentials());
        installButton.setOnClickListener(new InstallCert());
        Button removeScreenLockButton = (Button) view.findViewById(R.id.remove_screen_lock);
        removeScreenLockButton.setOnClickListener(
                v -> startActivity(new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD)));

        setContentView(view);

        setPassFailButtonClickListeners();
        setInfoResources(R.string.caboot_test, R.string.caboot_info, -1);

        getPassButton().setEnabled(true);
    }

    class OpenTrustedCredentials implements OnClickListener {
        @Override
        public void onClick(View v) {
            try {
                startActivity(new Intent("com.android.settings.TRUSTED_CREDENTIALS_USER"));
            } catch (ActivityNotFoundException e) {
                // do nothing
            }
        }
    }

    class InstallCert implements OnClickListener {
        @Override
        public void onClick(View v) {
            if (!CACertWriter.extractCertToDownloads(getApplicationContext(), getAssets())) {
                return;
            }

            try {
                startActivity(new Intent(Settings.ACTION_SECURITY_SETTINGS));
            } catch (ActivityNotFoundException e) {
                // do nothing
            }
        }
    }


}
