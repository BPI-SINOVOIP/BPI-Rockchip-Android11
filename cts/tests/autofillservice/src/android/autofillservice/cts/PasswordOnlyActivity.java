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
package android.autofillservice.cts;

import android.os.Bundle;
import android.util.Log;
import android.view.autofill.AutofillId;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

public final class PasswordOnlyActivity extends AbstractAutoFillActivity {

    private static final String TAG = "PasswordOnlyActivity";

    static final String EXTRA_USERNAME = "username";
    static final String EXTRA_PASSWORD_AUTOFILL_ID = "password_autofill_id";

    private TextView mWelcomeLabel;
    private EditText mPasswordEditText;
    private Button mLoginButton;
    private String mUsername;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(getContentView());

        mWelcomeLabel = findViewById(R.id.welcome);
        mPasswordEditText = findViewById(R.id.password);
        mLoginButton = findViewById(R.id.login);
        mLoginButton.setOnClickListener((v) -> login());

        mUsername = getIntent().getStringExtra(EXTRA_USERNAME);
        final String welcomeMsg = "Welcome to the jungle, " + mUsername;
        Log.v(TAG, welcomeMsg);
        mWelcomeLabel.setText(welcomeMsg);
        final AutofillId id = getIntent().getParcelableExtra(EXTRA_PASSWORD_AUTOFILL_ID);
        if (id != null) {
            Log.v(TAG, "Setting autofill id to " + id);
            mPasswordEditText.setAutofillId(id);
        }
    }

    protected int getContentView() {
        return R.layout.password_only_activity;
    }

    public void focusOnPassword() {
        syncRunOnUiThread(() -> mPasswordEditText.requestFocus());
    }

    void setPassword(String password) {
        syncRunOnUiThread(() -> mPasswordEditText.setText(password));
    }

    void login() {
        final String password = mPasswordEditText.getText().toString();
        Log.i(TAG, "Login as " + mUsername + "/" + password);
        finish();
    }
}
