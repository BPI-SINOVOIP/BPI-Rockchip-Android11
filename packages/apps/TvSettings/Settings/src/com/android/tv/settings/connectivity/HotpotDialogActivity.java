package com.android.tv.settings.connectivity;

import com.android.tv.settings.BaseInputActivity;
import com.android.tv.settings.R;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import java.io.*;

import android.widget.Button;
import android.view.View.OnClickListener;
import android.net.ConnectivityManager;
import androidx.preference.Preference;
import android.os.Handler;

import android.content.Intent;
import android.content.BroadcastReceiver;

import static android.net.ConnectivityManager.TETHERING_WIFI;

import java.lang.ref.WeakReference;
import java.util.UUID;

import java.nio.charset.Charset;

public class HotpotDialogActivity extends BaseInputActivity implements View.OnClickListener,
        TextWatcher, AdapterView.OnItemSelectedListener {

    static final int BUTTON_SUBMIT = DialogInterface.BUTTON_POSITIVE;
    private static final String DATA_SAVER_FOOTER = "disabled_on_data_saver";
    static final String DEFAULT_SSID = "AndroidAP";

//  private final DialogInterface.OnClickListener mListener;

    public static final int WPA2_INDEX = 0;
    public static final int OPEN_INDEX = 1;
    public static final int INDEX_2GHZ = 0;
    public static final int INDEX_5GHZ = 1;

    private String[] mSecurityType;

    private boolean mDataSaverEnabled;


    private ConnectivityManager mCm;
    private OnStartTetheringCallback mStartTetheringCallback;
    private Handler mHandler = new Handler();

    private Button my_save_button;
    private Button my_cancel_button;
    private Preference mCreateNetwork;
    private Spinner security;

    private EditText mSsid;
    private int mSecurityTypeIndex = WPA2_INDEX;
    private EditText mPassword;
    private int mBandIndex = INDEX_2GHZ; //SoftApConfiguration.BAND_2GHZ;

    SoftApConfiguration mWifiConfig;
    WifiManager mWifiManager;
    private Context mContext;

    private static final String TAG = "HotpotDialogActivity";
    private static final String WIFI_AP_SSID_AND_SECURITY = "wifi_ap_ssid_and_security";

    @Override
    public void init() {
        // TODO Auto-generated method stub

    }

    @Override
    public int getContentLayoutRes() {
        // TODO Auto-generated method stub
        return R.layout.wifi_ap_dialog_my;
    }

    @Override
    public String getInputTitle() {
        // TODO Auto-generated method stub
        return "Hotpot Setup";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        security = (Spinner) findViewById(R.id.security);

        Spinner mSecurity = ((Spinner) findViewById(R.id.security));
        final Spinner mChannel = (Spinner) findViewById(R.id.choose_channel);

        mWifiManager = (WifiManager) getSystemService(Context.WIFI_SERVICE);
        mWifiConfig = mWifiManager.getSoftApConfiguration();


        mStartTetheringCallback = new OnStartTetheringCallback(this);

        mSsid = (EditText) findViewById(R.id.ssid);
        mSsid.requestFocus();
        mPassword = (EditText) findViewById(R.id.password);
        my_save_button = (Button) findViewById(R.id.OK);
        my_cancel_button = (Button) findViewById(R.id.NO);

        my_save_button.setOnClickListener(new myListener());
        my_cancel_button.setOnClickListener(new myListener());

        if (mWifiConfig != null) {
            mSsid.setText(mWifiConfig.getSsid());
            if (is5GhzBandSupported()) {
                Log.d(TAG, "current band = " + mWifiConfig.getBand());
                mBandIndex = validateSelection(mWifiConfig.getBand());
                Log.d(TAG, "Updating band index to " + mBandIndex);
            } else {
                mBandIndex = 0;//SoftApConfiguration.BAND_2GHZ;
                Log.d(TAG, "5Ghz not supported, updating band index to 2GHz");
            }

            security.setSelection(getSecurityTypeIndex(mWifiConfig));
            if (getSecurityTypeIndex(mWifiConfig) == WPA2_INDEX) {
                mPassword.setText(mWifiConfig.getPassphrase());
            }
        } else {
            Log.i(TAG, "mWifiConfig = null");
            mSsid.setText(DEFAULT_SSID);
            mBandIndex = INDEX_2GHZ;//SoftApConfiguration.BAND_2GHZ;
            security.setSelection(getSecurityTypeIndex(null));
        }

        mChannel.setOnItemSelectedListener(
                new AdapterView.OnItemSelectedListener() {
                    boolean mInit = true;

                    @Override
                    public void onItemSelected(AdapterView<?> adapterView, View view, int position,
                                               long id) {
                        if (!mInit) {
                            mBandIndex = position;;
                            mWifiManager.setSoftApConfiguration(new SoftApConfiguration.Builder(mWifiConfig).setBand(mBandIndex == INDEX_5GHZ ? SoftApConfiguration.BAND_5GHZ : SoftApConfiguration.BAND_2GHZ).build());
                            Log.i(TAG, "config on channelIndex : " + mBandIndex + " Band: " +
                                    mWifiConfig.getBand());
                        } else {
                            mInit = false;
                            mChannel.setSelection(mBandIndex);
                        }

                    }

                    @Override
                    public void onNothingSelected(AdapterView<?> adapterView) {

                    }
                }
        );

        mSsid.addTextChangedListener(this);
        mPassword.addTextChangedListener(this);
        ((CheckBox) findViewById(R.id.show_password)).setOnClickListener(this);
        mSecurity.setOnItemSelectedListener(this);

        validate();

    }

    class myListener implements OnClickListener {
        @Override
        public void onClick(View view) {
            switch (view.getId()) {
                case R.id.OK: {
                    mWifiConfig = getConfig();
                    if (mWifiConfig != null) {
                        if (mWifiManager.getWifiApState() == WifiManager.WIFI_AP_STATE_ENABLED) {
                            Log.d("TetheringSettings", "Wifi AP config changed while enabled, stop and restart");
                        }
                        mWifiManager.setSoftApConfiguration(mWifiConfig);
                        HotPotFragment.mRestartWifiApAfterConfigChange = true;
                        Intent intent = new Intent();
                        intent.setAction(ConnectivityManager.ACTION_TETHER_STATE_CHANGED);
                        sendBroadcast(intent);
                        finish();
                    }
                }
                break;
                case R.id.NO:
                    finish();
                    break;
            }
        }
    }

    private void validate() {
        String mSsidString = mSsid.getText().toString();
        if ((mSsid != null && mSsid.length() == 0)
                || ((mSecurityTypeIndex == WPA2_INDEX) && mPassword.length() < 8)
                || (mSsid != null &&
                Charset.forName("UTF-8").encode(mSsidString).limit() > 32)) {
            my_save_button.setEnabled(false);
        } else {
            my_save_button.setEnabled(true);
        }
    }

    public static int getSecurityTypeIndex(SoftApConfiguration wifiConfig) {
        if(wifiConfig != null && wifiConfig.getSecurityType() == SoftApConfiguration.SECURITY_TYPE_OPEN) {
            return OPEN_INDEX;
        }
        return WPA2_INDEX;
    }

    public SoftApConfiguration getConfig() {
        SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setSsid(mSsid.getText().toString());
        configBuilder.setBand(mBandIndex == INDEX_5GHZ ? SoftApConfiguration.BAND_5GHZ : SoftApConfiguration.BAND_2GHZ);
        if(mSecurityTypeIndex == WPA2_INDEX) {
            configBuilder.setPassphrase(getPasswordValidated(SoftApConfiguration.SECURITY_TYPE_WPA2_PSK), SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        }
        return configBuilder.build();
    }

    public void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        mPassword.setInputType(
                InputType.TYPE_CLASS_TEXT |
                        (((CheckBox) findViewById(R.id.show_password)).isChecked() ?
                                InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD :
                                InputType.TYPE_TEXT_VARIATION_PASSWORD));
    }

    public void onClick(View view) {
        mPassword.setInputType(
                InputType.TYPE_CLASS_TEXT | (((CheckBox) view).isChecked() ?
                        InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD :
                        InputType.TYPE_TEXT_VARIATION_PASSWORD));
    }

    public void onTextChanged(CharSequence s, int start, int before, int count) {
    }

    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
    }

    public void afterTextChanged(Editable editable) {
        validate();
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        mSecurityTypeIndex = position;
        showSecurityFields();
        validate();
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
    }

    private void showSecurityFields() {
        if (mSecurityTypeIndex == OPEN_INDEX) {
            findViewById(R.id.fields).setVisibility(View.GONE);
            return;
        }
        findViewById(R.id.fields).setVisibility(View.VISIBLE);
    }

    private static final class OnStartTetheringCallback extends
            ConnectivityManager.OnStartTetheringCallback {
        final WeakReference<HotpotDialogActivity> mTetherSettings;

        OnStartTetheringCallback(HotpotDialogActivity settings) {
            mTetherSettings = new WeakReference<HotpotDialogActivity>(settings);
        }

        @Override
        public void onTetheringStarted() {
            update();
        }

        @Override
        public void onTetheringFailed() {
            update();
        }

        private void update() {

        }
    }

    //默认关闭5G选项
    public boolean is5GhzBandSupported() {
        String countryCode = mWifiManager.getCountryCode();
        if (!mWifiManager.is5GHzBandSupported() || countryCode == null) {
            return false;
        }
        return true;
    }

    public int validateSelection(int band) {
        // unsupported states:
        // 1: BAND_5GHZ only - include 2GHZ since some of countries doesn't support 5G hotspot
        // 2: no 5 GHZ support means we can't have BAND_5GHZ - default to 2GHZ
        if (SoftApConfiguration.BAND_5GHZ <= band) {
            if (!is5GhzBandSupported()) {
                return INDEX_2GHZ;//SoftApConfiguration.BAND_2GHZ;
            }
            return INDEX_5GHZ;//SoftApConfiguration.BAND_5GHZ | SoftApConfiguration.BAND_2GHZ;
        }

        return band == SoftApConfiguration.BAND_5GHZ ? INDEX_5GHZ : INDEX_2GHZ;
    }

    public static String generateRandomPassword() {
        String randomUUID = UUID.randomUUID().toString();
        //first 12 chars from xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        return randomUUID.substring(0, 8) + randomUUID.substring(9, 13);
    }

    public String getPasswordValidated(int securityType) {
        // don't actually overwrite unless we get a new config in case it was accidentally toggled.
        if (securityType == SoftApConfiguration.SECURITY_TYPE_OPEN) {
            return "";
        } else if (!isTextValid(mPassword.getText().toString())) {
            mPassword.setText(generateRandomPassword());
        }
        return mPassword.getText().toString();
    }

    public boolean isTextValid(String value) {
        final SoftApConfiguration.Builder configBuilder = new SoftApConfiguration.Builder();
        try {
            configBuilder.setPassphrase(value, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK);
        } catch (IllegalArgumentException e) {
            return false;
        }
        return true;
    }
}
