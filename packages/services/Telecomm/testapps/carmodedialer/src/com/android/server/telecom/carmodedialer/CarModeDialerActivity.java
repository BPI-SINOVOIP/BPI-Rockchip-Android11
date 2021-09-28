/*
 * Copyright (C) 2019 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.server.telecom.carmodedialer;

import android.app.Activity;
import android.app.UiModeManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.telecom.PhoneAccount;
import android.telecom.TelecomManager;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.EditText;
import android.widget.Toast;

public class CarModeDialerActivity extends Activity {
    private static final int REQUEST_CODE_SET_DEFAULT_DIALER = 1;

    private EditText mNumberView;
    private EditText mPriorityView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.testdialer_main);

        findViewById(R.id.place_call_button).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                placeCall();
            }
        });

        mNumberView = (EditText) findViewById(R.id.number);
        findViewById(R.id.enable_car_mode).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                enableCarMode();
            }
        });
        findViewById(R.id.disable_car_mode).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                disableCarMode();
            }
        });
        findViewById(R.id.toggle_incallservice).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleInCallService();
            }
        });

        mPriorityView = findViewById(R.id.priority);

        updateMutableUi();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_CODE_SET_DEFAULT_DIALER) {
            if (resultCode == RESULT_OK) {
                showToast("User accepted request to become default dialer");
            } else if (resultCode == RESULT_CANCELED) {
                showToast("User declined request to become default dialer");
            }
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        updateMutableUi();
    }

    private void updateMutableUi() {
        Intent intent = getIntent();
        if (intent != null) {
            mNumberView.setText(intent.getDataString());
        }
    }

    private void placeCall() {
        final TelecomManager telecomManager =
                (TelecomManager) getSystemService(Context.TELECOM_SERVICE);
        telecomManager.placeCall(Uri.fromParts(PhoneAccount.SCHEME_TEL,
                mNumberView.getText().toString(), null), createCallIntentExtras());
    }

    private void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }

    private Bundle createCallIntentExtras() {
        Bundle extras = new Bundle();
        extras.putString("com.android.server.telecom.carmodedialer.CALL_EXTRAS", "Tyler was here");

        Bundle intentExtras = new Bundle();
        intentExtras.putBundle(TelecomManager.EXTRA_OUTGOING_CALL_EXTRAS, extras);
        return intentExtras;
    }

    private void enableCarMode() {
        int priority;
        try {
            priority = Integer.parseInt(mPriorityView.getText().toString());
        } catch (NumberFormatException nfe) {
            Toast.makeText(this, "Invalid priority; not enabling car mode.",
                    Toast.LENGTH_LONG).show();
            return;
        }
        UiModeManager uiModeManager = (UiModeManager) getSystemService(UI_MODE_SERVICE);
        uiModeManager.enableCarMode(priority, 0);
        Toast.makeText(this, "Enabling car mode with priority " + priority,
                Toast.LENGTH_LONG).show();
    }

    private void disableCarMode() {
        UiModeManager uiModeManager = (UiModeManager) getSystemService(UI_MODE_SERVICE);
        uiModeManager.disableCarMode(0);
        Toast.makeText(this, "Disabling car mode", Toast.LENGTH_LONG).show();
    }

    private void toggleInCallService() {
        ComponentName uiComponent = new ComponentName(
                com.android.server.telecom.carmodedialer.CarModeInCallServiceImpl.class.getPackage().getName(),
                com.android.server.telecom.carmodedialer.CarModeInCallServiceImpl.class.getName());
        boolean isEnabled = getPackageManager().getComponentEnabledSetting(uiComponent)
                == PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
        getPackageManager().setComponentEnabledSetting(uiComponent,
                isEnabled ? PackageManager.COMPONENT_ENABLED_STATE_DISABLED
                        : PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                PackageManager.DONT_KILL_APP);
        isEnabled = getPackageManager().getComponentEnabledSetting(uiComponent)
                == PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
        Toast.makeText(this, "Is UI enabled? " + isEnabled, Toast.LENGTH_LONG).show();
    }
}
