package com.android.server.telecom.testapps;

import static android.content.res.Configuration.UI_MODE_TYPE_CAR;

import android.app.Activity;
import android.app.UiModeManager;
import android.content.ComponentName;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.provider.CallLog.Calls;
import android.provider.Settings;
import android.telecom.PhoneAccount;
import android.telecom.TelecomManager;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.telephony.ims.ImsRcsManager;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Toast;

public class TestDialerActivity extends Activity {
    private static final int REQUEST_CODE_SET_DEFAULT_DIALER = 1;

    private EditText mNumberView;
    private CheckBox mRttCheckbox;
    private EditText mPriorityView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.testdialer_main);
        findViewById(R.id.set_default_button).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                setDefault();
            }
        });

        findViewById(R.id.place_call_button).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                placeCall();
            }
        });

        findViewById(R.id.test_voicemail_button).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                testVoicemail();
            }
        });

        findViewById(R.id.cancel_missed_button).setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                cancelMissedCallNotification();
            }
        });

        mNumberView = (EditText) findViewById(R.id.number);
        mRttCheckbox = (CheckBox) findViewById(R.id.call_with_rtt_checkbox);
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

        Button discoveryButton = findViewById(R.id.send_contact_discovery_button);
        discoveryButton.setOnClickListener(v -> sendContactDiscoveryIntent());

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
            mRttCheckbox.setChecked(
                    intent.getBooleanExtra(TelecomManager.EXTRA_START_CALL_WITH_RTT, false));
        }
    }

    private void setDefault() {
        final Intent intent = new Intent(TelecomManager.ACTION_CHANGE_DEFAULT_DIALER);
        intent.putExtra(TelecomManager.EXTRA_CHANGE_DEFAULT_DIALER_PACKAGE_NAME, getPackageName());
        startActivityForResult(intent, REQUEST_CODE_SET_DEFAULT_DIALER);
    }

    private void placeCall() {
        final TelecomManager telecomManager =
                (TelecomManager) getSystemService(Context.TELECOM_SERVICE);
        telecomManager.placeCall(Uri.fromParts(PhoneAccount.SCHEME_TEL,
                mNumberView.getText().toString(), null), createCallIntentExtras());
    }

    private void testVoicemail() {
        try {
            // Test read
            getContentResolver().query(Calls.CONTENT_URI_WITH_VOICEMAIL, null, null, null, null);
            // Test write
            final ContentValues values = new ContentValues();
            values.put(Calls.CACHED_NAME, "hello world");
            getContentResolver().update(Calls.CONTENT_URI_WITH_VOICEMAIL, values, "1=0", null);
        } catch (SecurityException e) {
            showToast("Permission check failed");
            return;
        }
        showToast("Permission check succeeded");
    }

    private void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }

    private void cancelMissedCallNotification() {
        try {
            final TelecomManager tm = (TelecomManager) getSystemService(Context.TELECOM_SERVICE);
            tm.cancelMissedCallsNotification();
        } catch (SecurityException e) {
            Toast.makeText(this, "Privileged dialer operation failed", Toast.LENGTH_SHORT).show();
            return;
        }
        Toast.makeText(this, "Privileged dialer operation succeeded", Toast.LENGTH_SHORT).show();
    }

    private Bundle createCallIntentExtras() {
        Bundle extras = new Bundle();
        extras.putString("com.android.server.telecom.testapps.CALL_EXTRAS", "Hall was here");
        if (mRttCheckbox.isChecked()) {
            extras.putBoolean(TelecomManager.EXTRA_START_CALL_WITH_RTT, true);
        }

        Bundle intentExtras = new Bundle();
        intentExtras.putBundle(TelecomManager.EXTRA_OUTGOING_CALL_EXTRAS, extras);
        Log.i("Santos xtr", intentExtras.toString());
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
                TestInCallServiceImpl.class.getPackage().getName(),
                TestInCallServiceImpl.class.getName());
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

    private void sendContactDiscoveryIntent() {
        Intent intent = new Intent(ImsRcsManager.ACTION_SHOW_CAPABILITY_DISCOVERY_OPT_IN);
        intent.putExtra(Settings.EXTRA_SUB_ID, SubscriptionManager.getDefaultSubscriptionId());
        startActivity(intent);
    }
}
