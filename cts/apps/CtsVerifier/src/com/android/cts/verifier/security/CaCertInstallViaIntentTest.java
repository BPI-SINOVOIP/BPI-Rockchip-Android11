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

package com.android.cts.verifier.security;

import android.content.Intent;
import android.os.Bundle;
import android.security.KeyChain;
import android.util.Log;
import android.view.View;
import android.widget.Button;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import java.io.IOException;
import java.io.InputStream;

public class CaCertInstallViaIntentTest extends PassFailButtons.Activity implements
        View.OnClickListener {
    private static final String TAG = CaCertInstallViaIntentTest.class.getSimpleName();
    private static final String CERT_ASSET_NAME = "myCA.cer";
    private static final int REQUEST_CA_CERT_INSTALL = 1;

    private Button mRunButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        View root = getLayoutInflater().inflate(R.layout.ca_install_via_intent, null);
        setContentView(root);

        setInfoResources(R.string.cacert_install_via_intent_info,
                R.string.cacert_install_via_intent_title, -1);
        setPassFailButtonClickListeners();

        mRunButton = root.findViewById(R.id.run_test_button);
        mRunButton.setOnClickListener(this);

        getPassButton().setEnabled(false);
    }

    @Override
    public void onClick(View v) {
        Intent installIntent = KeyChain.createInstallIntent();
        installIntent.putExtra(KeyChain.EXTRA_NAME, "My CA");
        try {
            InputStream is = getAssets().open(CERT_ASSET_NAME);
            byte[] cert = new byte[is.available()];
            is.read(cert);
            installIntent.putExtra(KeyChain.EXTRA_CERTIFICATE, cert);
        } catch (IOException e) {
            Log.d(TAG, "Failed reading certificate", e);
            return;
        }
        startActivityForResult(installIntent, REQUEST_CA_CERT_INSTALL);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case REQUEST_CA_CERT_INSTALL: {
                getPassButton().setEnabled(true);
                Log.d(TAG, "Result: " + resultCode);
                break;
            }
            default:
                throw new IllegalStateException("requestCode == " + requestCode);
        }
    }
}
