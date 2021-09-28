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
 * limitations under the License.
 */

package com.android.phone.settings;

import static android.net.ConnectivityManager.NetworkCallback;
import static android.provider.Settings.Global.PREFERRED_NETWORK_MODE;

import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Typeface;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.TrafficStats;
import android.net.Uri;
import android.os.AsyncResult;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemProperties;
import android.provider.Settings;
import android.telephony.AccessNetworkConstants;
import android.telephony.CarrierConfigManager;
import android.telephony.CellIdentityCdma;
import android.telephony.CellIdentityGsm;
import android.telephony.CellIdentityLte;
import android.telephony.CellIdentityWcdma;
import android.telephony.CellInfo;
import android.telephony.CellInfoCdma;
import android.telephony.CellInfoGsm;
import android.telephony.CellInfoLte;
import android.telephony.CellInfoWcdma;
import android.telephony.CellLocation;
import android.telephony.CellSignalStrengthCdma;
import android.telephony.CellSignalStrengthGsm;
import android.telephony.CellSignalStrengthLte;
import android.telephony.CellSignalStrengthWcdma;
import android.telephony.DataSpecificRegistrationInfo;
import android.telephony.NetworkRegistrationInfo;
import android.telephony.PhoneStateListener;
import android.telephony.PhysicalChannelConfig;
import android.telephony.PreciseCallState;
import android.telephony.ServiceState;
import android.telephony.SignalStrength;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.cdma.CdmaCellLocation;
import android.telephony.gsm.GsmCellLocation;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AlertDialog.Builder;
import androidx.appcompat.app.AppCompatActivity;

import com.android.ims.ImsConfig;
import com.android.ims.ImsException;
import com.android.ims.ImsManager;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneFactory;
import com.android.phone.R;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.List;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * Radio Information Class
 *
 * Allows user to read and alter some of the radio related information.
 *
 */
public class RadioInfo extends AppCompatActivity {
    private static final String TAG = "RadioInfo";

    private static final boolean IS_USER_BUILD = "user".equals(Build.TYPE);

    private static final String[] PREFERRED_NETWORK_LABELS = {
            "GSM/WCDMA preferred",
            "GSM only",
            "WCDMA only",
            "GSM/WCDMA auto (PRL)",
            "CDMA/EvDo auto (PRL)",
            "CDMA only",
            "EvDo only",
            "CDMA/EvDo/GSM/WCDMA (PRL)",
            "CDMA + LTE/EvDo (PRL)",
            "GSM/WCDMA/LTE (PRL)",
            "LTE/CDMA/EvDo/GSM/WCDMA (PRL)",
            "LTE only",
            "LTE/WCDMA",
            "TDSCDMA only",
            "TDSCDMA/WCDMA",
            "LTE/TDSCDMA",
            "TDSCDMA/GSM",
            "LTE/TDSCDMA/GSM",
            "TDSCDMA/GSM/WCDMA",
            "LTE/TDSCDMA/WCDMA",
            "LTE/TDSCDMA/GSM/WCDMA",
            "TDSCDMA/CDMA/EvDo/GSM/WCDMA ",
            "LTE/TDSCDMA/CDMA/EvDo/GSM/WCDMA",
            "NR only",
            "NR/LTE",
            "NR/LTE/CDMA/EvDo",
            "NR/LTE/GSM/WCDMA",
            "NR/LTE/CDMA/EvDo/GSM/WCDMA",
            "NR/LTE/WCDMA",
            "NR/LTE/TDSCDMA",
            "NR/LTE/TDSCDMA/GSM",
            "NR/LTE/TDSCDMA/WCDMA",
            "NR/LTE/TDSCDMA/GSM/WCDMA",
            "NR/LTE/TDSCDMA/CDMA/EvDo/GSM/WCDMA",
            "Unknown"
    };

    private static String[] sPhoneIndexLabels;

    private static final int sCellInfoListRateDisabled = Integer.MAX_VALUE;
    private static final int sCellInfoListRateMax = 0;

    private static final String OEM_RADIO_INFO_INTENT =
            "com.android.phone.settings.OEM_RADIO_INFO";

    private static final String DSDS_MODE_PROPERTY = "ro.boot.hardware.dsds";

    /**
     * A value indicates the device is always on dsds mode.
     * @see {@link #DSDS_MODE_PROPERTY}
     */
    private static final int ALWAYS_ON_DSDS_MODE = 1;

    private static final int IMS_VOLTE_PROVISIONED_CONFIG_ID =
            ImsConfig.ConfigConstants.VLT_SETTING_ENABLED;

    private static final int IMS_VT_PROVISIONED_CONFIG_ID =
            ImsConfig.ConfigConstants.LVC_SETTING_ENABLED;

    private static final int IMS_WFC_PROVISIONED_CONFIG_ID =
            ImsConfig.ConfigConstants.VOICE_OVER_WIFI_SETTING_ENABLED;

    private static final int EAB_PROVISIONED_CONFIG_ID =
            ImsConfig.ConfigConstants.EAB_SETTING_ENABLED;

    //Values in must match CELL_INFO_REFRESH_RATES
    private static final String[] CELL_INFO_REFRESH_RATE_LABELS = {
            "Disabled",
            "Immediate",
            "Min 5s",
            "Min 10s",
            "Min 60s"
    };

    //Values in seconds, must match CELL_INFO_REFRESH_RATE_LABELS
    private static final int [] CELL_INFO_REFRESH_RATES = {
        sCellInfoListRateDisabled,
        sCellInfoListRateMax,
        5000,
        10000,
        60000
    };

    private static void log(String s) {
        Log.d(TAG, s);
    }

    private static final int EVENT_CFI_CHANGED = 302;

    private static final int EVENT_QUERY_PREFERRED_TYPE_DONE = 1000;
    private static final int EVENT_SET_PREFERRED_TYPE_DONE = 1001;
    private static final int EVENT_QUERY_SMSC_DONE = 1005;
    private static final int EVENT_UPDATE_SMSC_DONE = 1006;
    private static final int EVENT_PHYSICAL_CHANNEL_CONFIG_CHANGED = 1007;

    private static final int MENU_ITEM_SELECT_BAND         = 0;
    private static final int MENU_ITEM_VIEW_ADN            = 1;
    private static final int MENU_ITEM_VIEW_FDN            = 2;
    private static final int MENU_ITEM_VIEW_SDN            = 3;
    private static final int MENU_ITEM_GET_IMS_STATUS      = 4;
    private static final int MENU_ITEM_TOGGLE_DATA         = 5;

    private TextView mDeviceId; //DeviceId is the IMEI in GSM and the MEID in CDMA
    private TextView mLine1Number;
    private TextView mSubscriptionId;
    private TextView mDds;
    private TextView mSubscriberId;
    private TextView mCallState;
    private TextView mOperatorName;
    private TextView mRoamingState;
    private TextView mGsmState;
    private TextView mGprsState;
    private TextView mVoiceNetwork;
    private TextView mDataNetwork;
    private TextView mDBm;
    private TextView mMwi;
    private TextView mCfi;
    private TextView mLocation;
    private TextView mCellInfo;
    private TextView mSent;
    private TextView mReceived;
    private TextView mPingHostnameV4;
    private TextView mPingHostnameV6;
    private TextView mHttpClientTest;
    private TextView mPhyChanConfig;
    private TextView mDnsCheckState;
    private TextView mDownlinkKbps;
    private TextView mUplinkKbps;
    private TextView mEndcAvailable;
    private TextView mDcnrRestricted;
    private TextView mNrAvailable;
    private TextView mNrState;
    private TextView mNrFrequency;
    private EditText mSmsc;
    private Switch mRadioPowerOnSwitch;
    private Button mCellInfoRefreshRateButton;
    private Button mDnsCheckToggleButton;
    private Button mPingTestButton;
    private Button mUpdateSmscButton;
    private Button mRefreshSmscButton;
    private Button mOemInfoButton;
    private Button mCarrierProvisioningButton;
    private Button mTriggerCarrierProvisioningButton;
    private Switch mImsVolteProvisionedSwitch;
    private Switch mImsVtProvisionedSwitch;
    private Switch mImsWfcProvisionedSwitch;
    private Switch mEabProvisionedSwitch;
    private Switch mCbrsDataSwitch;
    private Switch mDsdsSwitch;
    private Spinner mPreferredNetworkType;
    private Spinner mSelectPhoneIndex;
    private Spinner mCellInfoRefreshRateSpinner;

    private static final long RUNNABLE_TIMEOUT_MS = 5 * 60 * 1000L;

    private ThreadPoolExecutor mQueuedWork;

    private ConnectivityManager mConnectivityManager;
    private TelephonyManager mTelephonyManager;
    private ImsManager mImsManager = null;
    private Phone mPhone = null;

    private String mPingHostnameResultV4;
    private String mPingHostnameResultV6;
    private String mHttpClientTestResult;
    private boolean mMwiValue = false;
    private boolean mCfiValue = false;

    private List<CellInfo> mCellInfoResult = null;
    private CellLocation mCellLocationResult = null;

    private int mPreferredNetworkTypeResult;
    private int mCellInfoRefreshRateIndex;
    private int mSelectedPhoneIndex;

    private final NetworkRequest mDefaultNetworkRequest = new NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR)
            .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .build();

    private final NetworkCallback mNetworkCallback = new NetworkCallback() {
        public void onCapabilitiesChanged(Network n, NetworkCapabilities nc) {
            int dlbw = nc.getLinkDownstreamBandwidthKbps();
            int ulbw = nc.getLinkUpstreamBandwidthKbps();
            updateBandwidths(dlbw, ulbw);
        }
    };

    // not final because we need to recreate this object to register on a new subId (b/117555407)
    private PhoneStateListener mPhoneStateListener = new RadioInfoPhoneStateListener();
    private class RadioInfoPhoneStateListener extends PhoneStateListener {
        @Override
        public void onDataConnectionStateChanged(int state) {
            updateDataState();
            updateNetworkType();
        }

        @Override
        public void onDataActivity(int direction) {
            updateDataStats2();
        }

        @Override
        public void onCallStateChanged(int state, String incomingNumber) {
            updateNetworkType();
            updatePhoneState(state);
        }

        @Override
        public void onPreciseCallStateChanged(PreciseCallState preciseState) {
            updateNetworkType();
        }

        @Override
        public void onCellLocationChanged(CellLocation location) {
            updateLocation(location);
        }

        @Override
        public void onMessageWaitingIndicatorChanged(boolean mwi) {
            mMwiValue = mwi;
            updateMessageWaiting();
        }

        @Override
        public void onCallForwardingIndicatorChanged(boolean cfi) {
            mCfiValue = cfi;
            updateCallRedirect();
        }

        @Override
        public void onCellInfoChanged(List<CellInfo> arrayCi) {
            log("onCellInfoChanged: arrayCi=" + arrayCi);
            mCellInfoResult = arrayCi;
            updateCellInfo(mCellInfoResult);
        }

        @Override
        public void onSignalStrengthsChanged(SignalStrength signalStrength) {
            log("onSignalStrengthChanged: SignalStrength=" + signalStrength);
            updateSignalStrength(signalStrength);
        }

        @Override
        public void onServiceStateChanged(ServiceState serviceState) {
            log("onServiceStateChanged: ServiceState=" + serviceState);
            updateServiceState(serviceState);
            updateRadioPowerState();
            updateNetworkType();
            updateImsProvisionedState();
            updateNrStats(serviceState);
        }

    }

    private void updatePhysicalChannelConfiguration(List<PhysicalChannelConfig> configs) {
        StringBuilder sb = new StringBuilder();
        String div = "";
        sb.append("{");
        if (configs != null) {
            for (PhysicalChannelConfig c : configs) {
                sb.append(div).append(c);
                div = ",";
            }
        }
        sb.append("}");
        mPhyChanConfig.setText(sb.toString());
    }

    private void updatePreferredNetworkType(int type) {
        if (type >= PREFERRED_NETWORK_LABELS.length || type < 0) {
            log("EVENT_QUERY_PREFERRED_TYPE_DONE: unknown "
                    + "type=" + type);
            type = PREFERRED_NETWORK_LABELS.length - 1; //set to Unknown
        }
        mPreferredNetworkTypeResult = type;

        mPreferredNetworkType.setSelection(mPreferredNetworkTypeResult, true);
    }

    private void updatePhoneIndex(int phoneIndex, int subId) {
        // unregister listeners on the old subId
        unregisterPhoneStateListener();
        mTelephonyManager.setCellInfoListRate(sCellInfoListRateDisabled);

        // update the subId
        mTelephonyManager = mTelephonyManager.createForSubscriptionId(subId);

        // update the phoneId
        mImsManager = ImsManager.getInstance(getApplicationContext(), phoneIndex);
        mPhone = PhoneFactory.getPhone(phoneIndex);

        updateAllFields();
    }

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            AsyncResult ar;
            switch (msg.what) {
                case EVENT_QUERY_PREFERRED_TYPE_DONE:
                    ar = (AsyncResult) msg.obj;
                    if (ar.exception == null && ar.result != null) {
                        updatePreferredNetworkType(((int []) ar.result)[0]);
                    } else {
                        //In case of an exception, we will set this to unknown
                        updatePreferredNetworkType(PREFERRED_NETWORK_LABELS.length - 1);
                    }
                    break;
                case EVENT_SET_PREFERRED_TYPE_DONE:
                    ar = (AsyncResult) msg.obj;
                    if (ar.exception != null) {
                        log("Set preferred network type failed.");
                    }
                    break;
                case EVENT_QUERY_SMSC_DONE:
                    ar = (AsyncResult) msg.obj;
                    if (ar.exception != null) {
                        mSmsc.setText("refresh error");
                    } else {
                        mSmsc.setText((String) ar.result);
                    }
                    break;
                case EVENT_UPDATE_SMSC_DONE:
                    mUpdateSmscButton.setEnabled(true);
                    ar = (AsyncResult) msg.obj;
                    if (ar.exception != null) {
                        mSmsc.setText("update error");
                    }
                    break;
                case EVENT_PHYSICAL_CHANNEL_CONFIG_CHANGED:
                    ar = (AsyncResult) msg.obj;
                    if (ar.exception != null) {
                        mPhyChanConfig.setText(("update error"));
                    }
                    updatePhysicalChannelConfiguration((List<PhysicalChannelConfig>) ar.result);
                    break;
                default:
                    super.handleMessage(msg);
                    break;

            }
        }
    };

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        if (!android.os.Process.myUserHandle().isSystem()) {
            Log.e(TAG, "Not run from system user, don't do anything.");
            finish();
            return;
        }

        setContentView(R.layout.radio_info);

        log("Started onCreate");

        mQueuedWork = new ThreadPoolExecutor(1, 1, RUNNABLE_TIMEOUT_MS, TimeUnit.MICROSECONDS,
                new LinkedBlockingDeque<Runnable>());
        mTelephonyManager = (TelephonyManager) getSystemService(TELEPHONY_SERVICE);
        mConnectivityManager = (ConnectivityManager) getSystemService(CONNECTIVITY_SERVICE);
        mPhone = PhoneFactory.getDefaultPhone();

        mImsManager = ImsManager.getInstance(getApplicationContext(),
                SubscriptionManager.getDefaultVoicePhoneId());

        sPhoneIndexLabels = getPhoneIndexLabels(mTelephonyManager);

        mDeviceId = (TextView) findViewById(R.id.imei);
        mLine1Number = (TextView) findViewById(R.id.number);
        mSubscriptionId = (TextView) findViewById(R.id.subid);
        mDds = (TextView) findViewById(R.id.dds);
        mSubscriberId = (TextView) findViewById(R.id.imsi);
        mCallState = (TextView) findViewById(R.id.call);
        mOperatorName = (TextView) findViewById(R.id.operator);
        mRoamingState = (TextView) findViewById(R.id.roaming);
        mGsmState = (TextView) findViewById(R.id.gsm);
        mGprsState = (TextView) findViewById(R.id.gprs);
        mVoiceNetwork = (TextView) findViewById(R.id.voice_network);
        mDataNetwork = (TextView) findViewById(R.id.data_network);
        mDBm = (TextView) findViewById(R.id.dbm);
        mMwi = (TextView) findViewById(R.id.mwi);
        mCfi = (TextView) findViewById(R.id.cfi);
        mLocation = (TextView) findViewById(R.id.location);
        mCellInfo = (TextView) findViewById(R.id.cellinfo);
        mCellInfo.setTypeface(Typeface.MONOSPACE);

        mSent = (TextView) findViewById(R.id.sent);
        mReceived = (TextView) findViewById(R.id.received);
        mSmsc = (EditText) findViewById(R.id.smsc);
        mDnsCheckState = (TextView) findViewById(R.id.dnsCheckState);
        mPingHostnameV4 = (TextView) findViewById(R.id.pingHostnameV4);
        mPingHostnameV6 = (TextView) findViewById(R.id.pingHostnameV6);
        mHttpClientTest = (TextView) findViewById(R.id.httpClientTest);
        mEndcAvailable = (TextView) findViewById(R.id.endc_available);
        mDcnrRestricted = (TextView) findViewById(R.id.dcnr_restricted);
        mNrAvailable = (TextView) findViewById(R.id.nr_available);
        mNrState = (TextView) findViewById(R.id.nr_state);
        mNrFrequency = (TextView) findViewById(R.id.nr_frequency);
        mPhyChanConfig = (TextView) findViewById(R.id.phy_chan_config);

        // hide 5G stats on devices that don't support 5G
        if ((mTelephonyManager.getSupportedRadioAccessFamily()
                & TelephonyManager.NETWORK_TYPE_BITMASK_NR) == 0) {
            ((TextView) findViewById(R.id.endc_available_label)).setVisibility(View.GONE);
            mEndcAvailable.setVisibility(View.GONE);
            ((TextView) findViewById(R.id.dcnr_restricted_label)).setVisibility(View.GONE);
            mDcnrRestricted.setVisibility(View.GONE);
            ((TextView) findViewById(R.id.nr_available_label)).setVisibility(View.GONE);
            mNrAvailable.setVisibility(View.GONE);
            ((TextView) findViewById(R.id.nr_state_label)).setVisibility(View.GONE);
            mNrState.setVisibility(View.GONE);
            ((TextView) findViewById(R.id.nr_frequency_label)).setVisibility(View.GONE);
            mNrFrequency.setVisibility(View.GONE);
        }

        mPreferredNetworkType = (Spinner) findViewById(R.id.preferredNetworkType);
        ArrayAdapter<String> mPreferredNetworkTypeAdapter = new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_item, PREFERRED_NETWORK_LABELS);
        mPreferredNetworkTypeAdapter
                .setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mPreferredNetworkType.setAdapter(mPreferredNetworkTypeAdapter);

        mSelectPhoneIndex = (Spinner) findViewById(R.id.phoneIndex);
        ArrayAdapter<String> phoneIndexAdapter = new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_item, sPhoneIndexLabels);
        phoneIndexAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mSelectPhoneIndex.setAdapter(phoneIndexAdapter);

        mCellInfoRefreshRateSpinner = (Spinner) findViewById(R.id.cell_info_rate_select);
        ArrayAdapter<String> cellInfoAdapter = new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_item, CELL_INFO_REFRESH_RATE_LABELS);
        cellInfoAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mCellInfoRefreshRateSpinner.setAdapter(cellInfoAdapter);

        mImsVolteProvisionedSwitch = (Switch) findViewById(R.id.volte_provisioned_switch);
        mImsVtProvisionedSwitch = (Switch) findViewById(R.id.vt_provisioned_switch);
        mImsWfcProvisionedSwitch = (Switch) findViewById(R.id.wfc_provisioned_switch);
        mEabProvisionedSwitch = (Switch) findViewById(R.id.eab_provisioned_switch);

        if (!ImsManager.isImsSupportedOnDevice(mPhone.getContext())) {
            mImsVolteProvisionedSwitch.setVisibility(View.GONE);
            mImsVtProvisionedSwitch.setVisibility(View.GONE);
            mImsWfcProvisionedSwitch.setVisibility(View.GONE);
            mEabProvisionedSwitch.setVisibility(View.GONE);
        }

        mCbrsDataSwitch = (Switch) findViewById(R.id.cbrs_data_switch);
        mCbrsDataSwitch.setVisibility(isCbrsSupported() ? View.VISIBLE : View.GONE);

        mDsdsSwitch = findViewById(R.id.dsds_switch);
        if (isDsdsSupported() && !dsdsModeOnly()) {
            mDsdsSwitch.setVisibility(View.VISIBLE);
            mDsdsSwitch.setOnClickListener(v -> {
                if (mTelephonyManager.doesSwitchMultiSimConfigTriggerReboot()) {
                    // Undo the click action until user clicks the confirm dialog.
                    mDsdsSwitch.toggle();
                    showDsdsChangeDialog();
                } else {
                    performDsdsSwitch();
                }
            });
            mDsdsSwitch.setChecked(isDsdsEnabled());
        } else {
            mDsdsSwitch.setVisibility(View.GONE);
        }

        mRadioPowerOnSwitch = (Switch) findViewById(R.id.radio_power);

        mDownlinkKbps = (TextView) findViewById(R.id.dl_kbps);
        mUplinkKbps = (TextView) findViewById(R.id.ul_kbps);
        updateBandwidths(0, 0);

        mPingTestButton = (Button) findViewById(R.id.ping_test);
        mPingTestButton.setOnClickListener(mPingButtonHandler);
        mUpdateSmscButton = (Button) findViewById(R.id.update_smsc);
        mUpdateSmscButton.setOnClickListener(mUpdateSmscButtonHandler);
        mRefreshSmscButton = (Button) findViewById(R.id.refresh_smsc);
        mRefreshSmscButton.setOnClickListener(mRefreshSmscButtonHandler);
        mDnsCheckToggleButton = (Button) findViewById(R.id.dns_check_toggle);
        mDnsCheckToggleButton.setOnClickListener(mDnsCheckButtonHandler);
        mCarrierProvisioningButton = (Button) findViewById(R.id.carrier_provisioning);
        mCarrierProvisioningButton.setOnClickListener(mCarrierProvisioningButtonHandler);
        mTriggerCarrierProvisioningButton = (Button) findViewById(
                R.id.trigger_carrier_provisioning);
        mTriggerCarrierProvisioningButton.setOnClickListener(
                mTriggerCarrierProvisioningButtonHandler);

        mOemInfoButton = (Button) findViewById(R.id.oem_info);
        mOemInfoButton.setOnClickListener(mOemInfoButtonHandler);
        PackageManager pm = getPackageManager();
        Intent oemInfoIntent = new Intent(OEM_RADIO_INFO_INTENT);
        List<ResolveInfo> oemInfoIntentList = pm.queryIntentActivities(oemInfoIntent, 0);
        if (oemInfoIntentList.size() == 0) {
            mOemInfoButton.setEnabled(false);
        }

        mCellInfoRefreshRateIndex = 0; //disabled
        mPreferredNetworkTypeResult = PREFERRED_NETWORK_LABELS.length - 1; //Unknown
        mSelectedPhoneIndex = 0; //phone 0

        //FIXME: Replace with TelephonyManager call
        mPhone.getPreferredNetworkType(
                mHandler.obtainMessage(EVENT_QUERY_PREFERRED_TYPE_DONE));

        restoreFromBundle(icicle);
    }

    @Override
    public Intent getParentActivityIntent() {
        Intent parentActivity = super.getParentActivityIntent();
        if (parentActivity == null) {
            parentActivity = (new Intent()).setClassName("com.android.settings",
                    "com.android.settings.Settings$TestingSettingsActivity");
        }
        return parentActivity;
    }

    @Override
    protected void onResume() {
        super.onResume();

        log("Started onResume");

        updateAllFields();
    }

    private void updateAllFields() {
        updateMessageWaiting();
        updateCallRedirect();
        updateDataState();
        updateDataStats2();
        updateRadioPowerState();
        updateImsProvisionedState();
        updateProperties();
        updateDnsCheckState();
        updateNetworkType();
        updateNrStats(null);

        updateLocation(mCellLocationResult);
        updateCellInfo(mCellInfoResult);
        updateSubscriptionIds();

        mPingHostnameV4.setText(mPingHostnameResultV4);
        mPingHostnameV6.setText(mPingHostnameResultV6);
        mHttpClientTest.setText(mHttpClientTestResult);

        mCellInfoRefreshRateSpinner.setOnItemSelectedListener(mCellInfoRefreshRateHandler);
        //set selection after registering listener to force update
        mCellInfoRefreshRateSpinner.setSelection(mCellInfoRefreshRateIndex);

        //set selection before registering to prevent update
        mPreferredNetworkType.setSelection(mPreferredNetworkTypeResult, true);
        mPreferredNetworkType.setOnItemSelectedListener(mPreferredNetworkHandler);

        // set phone index
        mSelectPhoneIndex.setSelection(mSelectedPhoneIndex, true);
        mSelectPhoneIndex.setOnItemSelectedListener(mSelectPhoneIndexHandler);

        mRadioPowerOnSwitch.setOnCheckedChangeListener(mRadioPowerOnChangeListener);
        mImsVolteProvisionedSwitch.setOnCheckedChangeListener(mImsVolteCheckedChangeListener);
        mImsVtProvisionedSwitch.setOnCheckedChangeListener(mImsVtCheckedChangeListener);
        mImsWfcProvisionedSwitch.setOnCheckedChangeListener(mImsWfcCheckedChangeListener);
        mEabProvisionedSwitch.setOnCheckedChangeListener(mEabCheckedChangeListener);

        if (isCbrsSupported()) {
            mCbrsDataSwitch.setChecked(getCbrsDataState());
            mCbrsDataSwitch.setOnCheckedChangeListener(mCbrsDataSwitchChangeListener);
        }

        unregisterPhoneStateListener();
        registerPhoneStateListener();
        mPhone.registerForPhysicalChannelConfig(mHandler,
            EVENT_PHYSICAL_CHANNEL_CONFIG_CHANGED, null);

        mConnectivityManager.registerNetworkCallback(
                mDefaultNetworkRequest, mNetworkCallback, mHandler);

        mSmsc.clearFocus();
    }

    @Override
    protected void onPause() {
        super.onPause();

        log("onPause: unregister phone & data intents");

        mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
        mTelephonyManager.setCellInfoListRate(sCellInfoListRateDisabled);
        mConnectivityManager.unregisterNetworkCallback(mNetworkCallback);

    }

    private void restoreFromBundle(Bundle b) {
        if (b == null) {
            return;
        }

        mPingHostnameResultV4 = b.getString("mPingHostnameResultV4", "");
        mPingHostnameResultV6 = b.getString("mPingHostnameResultV6", "");
        mHttpClientTestResult = b.getString("mHttpClientTestResult", "");

        mPingHostnameV4.setText(mPingHostnameResultV4);
        mPingHostnameV6.setText(mPingHostnameResultV6);
        mHttpClientTest.setText(mHttpClientTestResult);

        mPreferredNetworkTypeResult = b.getInt("mPreferredNetworkTypeResult",
                PREFERRED_NETWORK_LABELS.length - 1);

        mSelectedPhoneIndex = b.getInt("mSelectedPhoneIndex", 0);

        mCellInfoRefreshRateIndex = b.getInt("mCellInfoRefreshRateIndex", 0);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        outState.putString("mPingHostnameResultV4", mPingHostnameResultV4);
        outState.putString("mPingHostnameResultV6", mPingHostnameResultV6);
        outState.putString("mHttpClientTestResult", mHttpClientTestResult);

        outState.putInt("mPreferredNetworkTypeResult", mPreferredNetworkTypeResult);
        outState.putInt("mSelectedPhoneIndex", mSelectedPhoneIndex);
        outState.putInt("mCellInfoRefreshRateIndex", mCellInfoRefreshRateIndex);

    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(0, MENU_ITEM_SELECT_BAND, 0, R.string.radio_info_band_mode_label)
                .setOnMenuItemClickListener(mSelectBandCallback)
                .setAlphabeticShortcut('b');
        menu.add(1, MENU_ITEM_VIEW_ADN, 0,
                R.string.radioInfo_menu_viewADN).setOnMenuItemClickListener(mViewADNCallback);
        menu.add(1, MENU_ITEM_VIEW_FDN, 0,
                R.string.radioInfo_menu_viewFDN).setOnMenuItemClickListener(mViewFDNCallback);
        menu.add(1, MENU_ITEM_VIEW_SDN, 0,
                R.string.radioInfo_menu_viewSDN).setOnMenuItemClickListener(mViewSDNCallback);
        if (ImsManager.isImsSupportedOnDevice(mPhone.getContext())) {
            menu.add(1, MENU_ITEM_GET_IMS_STATUS,
                    0, R.string.radioInfo_menu_getIMS).setOnMenuItemClickListener(mGetImsStatus);
        }
        menu.add(1, MENU_ITEM_TOGGLE_DATA,
                0, R.string.radio_info_data_connection_disable)
                .setOnMenuItemClickListener(mToggleData);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        // Get the TOGGLE DATA menu item in the right state.
        MenuItem item = menu.findItem(MENU_ITEM_TOGGLE_DATA);
        int state = mTelephonyManager.getDataState();
        boolean visible = true;

        switch (state) {
            case TelephonyManager.DATA_CONNECTED:
            case TelephonyManager.DATA_SUSPENDED:
                item.setTitle(R.string.radio_info_data_connection_disable);
                break;
            case TelephonyManager.DATA_DISCONNECTED:
                item.setTitle(R.string.radio_info_data_connection_enable);
                break;
            default:
                visible = false;
                break;
        }
        item.setVisible(visible);
        return true;
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mQueuedWork.shutdown();
    }

    // returns array of string labels for each phone index. The array index is equal to the phone
    // index.
    private static String[] getPhoneIndexLabels(TelephonyManager tm) {
        int phones = tm.getPhoneCount();
        String[] labels = new String[phones];
        for (int i = 0; i < phones; i++) {
            labels[i] = "Phone " + i;
        }
        return labels;
    }

    private void unregisterPhoneStateListener() {
        mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
        mPhone.unregisterForPhysicalChannelConfig(mHandler);

        // clear all fields so they are blank until the next listener event occurs
        mOperatorName.setText("");
        mGprsState.setText("");
        mDataNetwork.setText("");
        mVoiceNetwork.setText("");
        mSent.setText("");
        mReceived.setText("");
        mCallState.setText("");
        mLocation.setText("");
        mMwiValue = false;
        mMwi.setText("");
        mCfiValue = false;
        mCfi.setText("");
        mCellInfo.setText("");
        mDBm.setText("");
        mGsmState.setText("");
        mRoamingState.setText("");
        mPhyChanConfig.setText("");
    }

    // register mPhoneStateListener for relevant fields using the current TelephonyManager
    private void registerPhoneStateListener() {
        mPhoneStateListener = new RadioInfoPhoneStateListener();
        mTelephonyManager.listen(mPhoneStateListener,
                  PhoneStateListener.LISTEN_CALL_STATE
        //b/27803938 - RadioInfo currently cannot read PRECISE_CALL_STATE
        //      | PhoneStateListener.LISTEN_PRECISE_CALL_STATE
                | PhoneStateListener.LISTEN_DATA_CONNECTION_STATE
                | PhoneStateListener.LISTEN_DATA_ACTIVITY
                | PhoneStateListener.LISTEN_CELL_LOCATION
                | PhoneStateListener.LISTEN_MESSAGE_WAITING_INDICATOR
                | PhoneStateListener.LISTEN_CALL_FORWARDING_INDICATOR
                | PhoneStateListener.LISTEN_CELL_INFO
                | PhoneStateListener.LISTEN_SERVICE_STATE
                | PhoneStateListener.LISTEN_SIGNAL_STRENGTHS);
    }

    private void updateDnsCheckState() {
        //FIXME: Replace with a TelephonyManager call
        mDnsCheckState.setText(mPhone.isDnsCheckDisabled()
                ? "0.0.0.0 allowed" : "0.0.0.0 not allowed");
    }

    private void updateBandwidths(int dlbw, int ulbw) {
        dlbw = (dlbw < 0 || dlbw == Integer.MAX_VALUE) ? -1 : dlbw;
        ulbw = (ulbw < 0 || ulbw == Integer.MAX_VALUE) ? -1 : ulbw;
        mDownlinkKbps.setText(String.format("%-5d", dlbw));
        mUplinkKbps.setText(String.format("%-5d", ulbw));
    }


    private void updateSignalStrength(SignalStrength signalStrength) {
        Resources r = getResources();

        int signalDbm = signalStrength.getDbm();

        int signalAsu = signalStrength.getAsuLevel();

        if (-1 == signalAsu) signalAsu = 0;

        mDBm.setText(String.valueOf(signalDbm) + " "
                + r.getString(R.string.radioInfo_display_dbm) + "   "
                + String.valueOf(signalAsu) + " "
                + r.getString(R.string.radioInfo_display_asu));
    }

    private void updateLocation(CellLocation location) {
        Resources r = getResources();
        if (location instanceof GsmCellLocation) {
            GsmCellLocation loc = (GsmCellLocation) location;
            int lac = loc.getLac();
            int cid = loc.getCid();
            mLocation.setText(r.getString(R.string.radioInfo_lac) + " = "
                    + ((lac == -1) ? "unknown" : Integer.toHexString(lac))
                    + "   "
                    + r.getString(R.string.radioInfo_cid) + " = "
                    + ((cid == -1) ? "unknown" : Integer.toHexString(cid)));
        } else if (location instanceof CdmaCellLocation) {
            CdmaCellLocation loc = (CdmaCellLocation) location;
            int bid = loc.getBaseStationId();
            int sid = loc.getSystemId();
            int nid = loc.getNetworkId();
            int lat = loc.getBaseStationLatitude();
            int lon = loc.getBaseStationLongitude();
            mLocation.setText("BID = "
                    + ((bid == -1) ? "unknown" : Integer.toHexString(bid))
                    + "   "
                    + "SID = "
                    + ((sid == -1) ? "unknown" : Integer.toHexString(sid))
                    + "   "
                    + "NID = "
                    + ((nid == -1) ? "unknown" : Integer.toHexString(nid))
                    + "\n"
                    + "LAT = "
                    + ((lat == -1) ? "unknown" : Integer.toHexString(lat))
                    + "   "
                    + "LONG = "
                    + ((lon == -1) ? "unknown" : Integer.toHexString(lon)));
        } else {
            mLocation.setText("unknown");
        }


    }

    private String getCellInfoDisplayString(int i) {
        return (i != Integer.MAX_VALUE) ? Integer.toString(i) : "";
    }

    private String getCellInfoDisplayString(long i) {
        return (i != Long.MAX_VALUE) ? Long.toString(i) : "";
    }

    private String getConnectionStatusString(CellInfo ci) {
        String regStr = "";
        String connStatStr = "";
        String connector = "";

        if (ci.isRegistered()) {
            regStr = "R";
        }
        switch (ci.getCellConnectionStatus()) {
            case CellInfo.CONNECTION_PRIMARY_SERVING: connStatStr = "P"; break;
            case CellInfo.CONNECTION_SECONDARY_SERVING: connStatStr = "S"; break;
            case CellInfo.CONNECTION_NONE: connStatStr = "N"; break;
            case CellInfo.CONNECTION_UNKNOWN: /* Field is unsupported */ break;
            default: break;
        }
        if (!TextUtils.isEmpty(regStr) && !TextUtils.isEmpty(connStatStr)) {
            connector = "+";
        }

        return regStr + connector + connStatStr;
    }

    private String buildCdmaInfoString(CellInfoCdma ci) {
        CellIdentityCdma cidCdma = ci.getCellIdentity();
        CellSignalStrengthCdma ssCdma = ci.getCellSignalStrength();

        return String.format("%-3.3s %-5.5s %-5.5s %-5.5s %-6.6s %-6.6s %-6.6s %-6.6s %-5.5s",
                getConnectionStatusString(ci),
                getCellInfoDisplayString(cidCdma.getSystemId()),
                getCellInfoDisplayString(cidCdma.getNetworkId()),
                getCellInfoDisplayString(cidCdma.getBasestationId()),
                getCellInfoDisplayString(ssCdma.getCdmaDbm()),
                getCellInfoDisplayString(ssCdma.getCdmaEcio()),
                getCellInfoDisplayString(ssCdma.getEvdoDbm()),
                getCellInfoDisplayString(ssCdma.getEvdoEcio()),
                getCellInfoDisplayString(ssCdma.getEvdoSnr()));
    }

    private String buildGsmInfoString(CellInfoGsm ci) {
        CellIdentityGsm cidGsm = ci.getCellIdentity();
        CellSignalStrengthGsm ssGsm = ci.getCellSignalStrength();

        return String.format("%-3.3s %-3.3s %-3.3s %-5.5s %-5.5s %-6.6s %-4.4s %-4.4s\n",
                getConnectionStatusString(ci),
                getCellInfoDisplayString(cidGsm.getMcc()),
                getCellInfoDisplayString(cidGsm.getMnc()),
                getCellInfoDisplayString(cidGsm.getLac()),
                getCellInfoDisplayString(cidGsm.getCid()),
                getCellInfoDisplayString(cidGsm.getArfcn()),
                getCellInfoDisplayString(cidGsm.getBsic()),
                getCellInfoDisplayString(ssGsm.getDbm()));
    }

    private String buildLteInfoString(CellInfoLte ci) {
        CellIdentityLte cidLte = ci.getCellIdentity();
        CellSignalStrengthLte ssLte = ci.getCellSignalStrength();

        return String.format(
                "%-3.3s %-3.3s %-3.3s %-5.5s %-5.5s %-3.3s %-6.6s %-2.2s %-4.4s %-4.4s %-2.2s\n",
                getConnectionStatusString(ci),
                getCellInfoDisplayString(cidLte.getMcc()),
                getCellInfoDisplayString(cidLte.getMnc()),
                getCellInfoDisplayString(cidLte.getTac()),
                getCellInfoDisplayString(cidLte.getCi()),
                getCellInfoDisplayString(cidLte.getPci()),
                getCellInfoDisplayString(cidLte.getEarfcn()),
                getCellInfoDisplayString(cidLte.getBandwidth()),
                getCellInfoDisplayString(ssLte.getDbm()),
                getCellInfoDisplayString(ssLte.getRsrq()),
                getCellInfoDisplayString(ssLte.getTimingAdvance()));
    }

    private String buildWcdmaInfoString(CellInfoWcdma ci) {
        CellIdentityWcdma cidWcdma = ci.getCellIdentity();
        CellSignalStrengthWcdma ssWcdma = ci.getCellSignalStrength();

        return String.format("%-3.3s %-3.3s %-3.3s %-5.5s %-5.5s %-6.6s %-3.3s %-4.4s\n",
                getConnectionStatusString(ci),
                getCellInfoDisplayString(cidWcdma.getMcc()),
                getCellInfoDisplayString(cidWcdma.getMnc()),
                getCellInfoDisplayString(cidWcdma.getLac()),
                getCellInfoDisplayString(cidWcdma.getCid()),
                getCellInfoDisplayString(cidWcdma.getUarfcn()),
                getCellInfoDisplayString(cidWcdma.getPsc()),
                getCellInfoDisplayString(ssWcdma.getDbm()));
    }

    private String buildCellInfoString(List<CellInfo> arrayCi) {
        String value = new String();
        StringBuilder cdmaCells = new StringBuilder(),
                gsmCells = new StringBuilder(),
                lteCells = new StringBuilder(),
                wcdmaCells = new StringBuilder();

        if (arrayCi != null) {
            for (CellInfo ci : arrayCi) {

                if (ci instanceof CellInfoLte) {
                    lteCells.append(buildLteInfoString((CellInfoLte) ci));
                } else if (ci instanceof CellInfoWcdma) {
                    wcdmaCells.append(buildWcdmaInfoString((CellInfoWcdma) ci));
                } else if (ci instanceof CellInfoGsm) {
                    gsmCells.append(buildGsmInfoString((CellInfoGsm) ci));
                } else if (ci instanceof CellInfoCdma) {
                    cdmaCells.append(buildCdmaInfoString((CellInfoCdma) ci));
                }
            }
            if (lteCells.length() != 0) {
                value += String.format(
                        "LTE\n%-3.3s %-3.3s %-3.3s %-5.5s %-5.5s %-3.3s"
                                + " %-6.6s %-2.2s %-4.4s %-4.4s %-2.2s\n",
                        "SRV", "MCC", "MNC", "TAC", "CID", "PCI",
                        "EARFCN", "BW", "RSRP", "RSRQ", "TA");
                value += lteCells.toString();
            }
            if (wcdmaCells.length() != 0) {
                value += String.format(
                        "WCDMA\n%-3.3s %-3.3s %-3.3s %-5.5s %-5.5s %-6.6s %-3.3s %-4.4s\n",
                        "SRV", "MCC", "MNC", "LAC", "CID", "UARFCN", "PSC", "RSCP");
                value += wcdmaCells.toString();
            }
            if (gsmCells.length() != 0) {
                value += String.format(
                        "GSM\n%-3.3s %-3.3s %-3.3s %-5.5s %-5.5s %-6.6s %-4.4s %-4.4s\n",
                        "SRV", "MCC", "MNC", "LAC", "CID", "ARFCN", "BSIC", "RSSI");
                value += gsmCells.toString();
            }
            if (cdmaCells.length() != 0) {
                value += String.format(
                        "CDMA/EVDO\n%-3.3s %-5.5s %-5.5s %-5.5s"
                                + " %-6.6s %-6.6s %-6.6s %-6.6s %-5.5s\n",
                        "SRV", "SID", "NID", "BSID",
                        "C-RSSI", "C-ECIO", "E-RSSI", "E-ECIO", "E-SNR");
                value += cdmaCells.toString();
            }
        } else {
            value = "unknown";
        }

        return value.toString();
    }

    private void updateCellInfo(List<CellInfo> arrayCi) {
        mCellInfo.setText(buildCellInfoString(arrayCi));
    }

    private void updateSubscriptionIds() {
        mSubscriptionId.setText(Integer.toString(mPhone.getSubId()));
        mDds.setText(Integer.toString(SubscriptionManager.getDefaultDataSubscriptionId()));
    }

    private void updateMessageWaiting() {
        mMwi.setText(String.valueOf(mMwiValue));
    }

    private void updateCallRedirect() {
        mCfi.setText(String.valueOf(mCfiValue));
    }


    private void updateServiceState(ServiceState serviceState) {
        int state = serviceState.getState();
        Resources r = getResources();
        String display = r.getString(R.string.radioInfo_unknown);

        switch (state) {
            case ServiceState.STATE_IN_SERVICE:
                display = r.getString(R.string.radioInfo_service_in);
                break;
            case ServiceState.STATE_OUT_OF_SERVICE:
            case ServiceState.STATE_EMERGENCY_ONLY:
                display = r.getString(R.string.radioInfo_service_emergency);
                break;
            case ServiceState.STATE_POWER_OFF:
                display = r.getString(R.string.radioInfo_service_off);
                break;
        }

        mGsmState.setText(display);

        if (serviceState.getRoaming()) {
            mRoamingState.setText(R.string.radioInfo_roaming_in);
        } else {
            mRoamingState.setText(R.string.radioInfo_roaming_not);
        }

        mOperatorName.setText(serviceState.getOperatorAlphaLong());
    }

    private void updatePhoneState(int state) {
        Resources r = getResources();
        String display = r.getString(R.string.radioInfo_unknown);

        switch (state) {
            case TelephonyManager.CALL_STATE_IDLE:
                display = r.getString(R.string.radioInfo_phone_idle);
                break;
            case TelephonyManager.CALL_STATE_RINGING:
                display = r.getString(R.string.radioInfo_phone_ringing);
                break;
            case TelephonyManager.CALL_STATE_OFFHOOK:
                display = r.getString(R.string.radioInfo_phone_offhook);
                break;
        }

        mCallState.setText(display);
    }

    private void updateDataState() {
        int state = mTelephonyManager.getDataState();
        Resources r = getResources();
        String display = r.getString(R.string.radioInfo_unknown);

        switch (state) {
            case TelephonyManager.DATA_CONNECTED:
                display = r.getString(R.string.radioInfo_data_connected);
                break;
            case TelephonyManager.DATA_CONNECTING:
                display = r.getString(R.string.radioInfo_data_connecting);
                break;
            case TelephonyManager.DATA_DISCONNECTED:
                display = r.getString(R.string.radioInfo_data_disconnected);
                break;
            case TelephonyManager.DATA_SUSPENDED:
                display = r.getString(R.string.radioInfo_data_suspended);
                break;
        }

        mGprsState.setText(display);
    }

    private void updateNetworkType() {
        if (mPhone != null) {
            ServiceState ss = mPhone.getServiceState();
            mDataNetwork.setText(ServiceState.rilRadioTechnologyToString(
                    mPhone.getServiceState().getRilDataRadioTechnology()));
            mVoiceNetwork.setText(ServiceState.rilRadioTechnologyToString(
                    mPhone.getServiceState().getRilVoiceRadioTechnology()));
        }
    }

    private void updateNrStats(ServiceState serviceState) {
        if ((mTelephonyManager.getSupportedRadioAccessFamily()
                & TelephonyManager.NETWORK_TYPE_BITMASK_NR) == 0) {
            return;
        }

        ServiceState ss = serviceState;
        if (ss == null && mPhone != null) {
            ss = mPhone.getServiceState();
        }
        if (ss != null) {
            NetworkRegistrationInfo nri = ss.getNetworkRegistrationInfo(
                    NetworkRegistrationInfo.DOMAIN_PS, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
            if (nri != null) {
                DataSpecificRegistrationInfo dsri = nri.getDataSpecificInfo();
                if (dsri != null) {
                    mEndcAvailable.setText(dsri.isEnDcAvailable ? "True" : "False");
                    mDcnrRestricted.setText(dsri.isDcNrRestricted ? "True" : "False");
                    mNrAvailable.setText(dsri.isNrAvailable ? "True" : "False");
                }
            }
            mNrState.setText(NetworkRegistrationInfo.nrStateToString(ss.getNrState()));
            mNrFrequency.setText(ServiceState.frequencyRangeToString(ss.getNrFrequencyRange()));
        }
    }

    private void updateProperties() {
        String s;
        Resources r = getResources();

        s = mPhone.getDeviceId();
        if (s == null) s = r.getString(R.string.radioInfo_unknown);
        mDeviceId.setText(s);

        s = mPhone.getSubscriberId();
        if (s == null) s = r.getString(R.string.radioInfo_unknown);
        mSubscriberId.setText(s);

        //FIXME: Replace with a TelephonyManager call
        s = mPhone.getLine1Number();
        if (s == null) s = r.getString(R.string.radioInfo_unknown);
        mLine1Number.setText(s);
    }

    private void updateDataStats2() {
        Resources r = getResources();

        long txPackets = TrafficStats.getMobileTxPackets();
        long rxPackets = TrafficStats.getMobileRxPackets();
        long txBytes   = TrafficStats.getMobileTxBytes();
        long rxBytes   = TrafficStats.getMobileRxBytes();

        String packets = r.getString(R.string.radioInfo_display_packets);
        String bytes   = r.getString(R.string.radioInfo_display_bytes);

        mSent.setText(txPackets + " " + packets + ", " + txBytes + " " + bytes);
        mReceived.setText(rxPackets + " " + packets + ", " + rxBytes + " " + bytes);
    }

    /**
     *  Ping a host name
     */
    private void pingHostname() {
        try {
            try {
                Process p4 = Runtime.getRuntime().exec("ping -c 1 www.google.com");
                int status4 = p4.waitFor();
                if (status4 == 0) {
                    mPingHostnameResultV4 = "Pass";
                } else {
                    mPingHostnameResultV4 = String.format("Fail(%d)", status4);
                }
            } catch (IOException e) {
                mPingHostnameResultV4 = "Fail: IOException";
            }
            try {
                Process p6 = Runtime.getRuntime().exec("ping6 -c 1 www.google.com");
                int status6 = p6.waitFor();
                if (status6 == 0) {
                    mPingHostnameResultV6 = "Pass";
                } else {
                    mPingHostnameResultV6 = String.format("Fail(%d)", status6);
                }
            } catch (IOException e) {
                mPingHostnameResultV6 = "Fail: IOException";
            }
        } catch (InterruptedException e) {
            mPingHostnameResultV4 = mPingHostnameResultV6 = "Fail: InterruptedException";
        }
    }

    /**
     * This function checks for basic functionality of HTTP Client.
     */
    private void httpClientTest() {
        HttpURLConnection urlConnection = null;
        try {
            // TODO: Hardcoded for now, make it UI configurable
            URL url = new URL("https://www.google.com");
            urlConnection = (HttpURLConnection) url.openConnection();
            if (urlConnection.getResponseCode() == 200) {
                mHttpClientTestResult = "Pass";
            } else {
                mHttpClientTestResult = "Fail: Code: " + urlConnection.getResponseMessage();
            }
        } catch (IOException e) {
            mHttpClientTestResult = "Fail: IOException";
        } finally {
            if (urlConnection != null) {
                urlConnection.disconnect();
            }
        }
    }

    private void refreshSmsc() {
        mQueuedWork.execute(new Runnable() {
            public void run() {
                //FIXME: Replace with a TelephonyManager call
                mPhone.getSmscAddress(mHandler.obtainMessage(EVENT_QUERY_SMSC_DONE));
            }
        });
    }

    private void updateAllCellInfo() {

        mCellInfo.setText("");
        mLocation.setText("");

        final Runnable updateAllCellInfoResults = new Runnable() {
            public void run() {
                updateLocation(mCellLocationResult);
                updateCellInfo(mCellInfoResult);
            }
        };

        mQueuedWork.execute(new Runnable() {
            @Override
            public void run() {
                mCellInfoResult = mTelephonyManager.getAllCellInfo();
                mCellLocationResult = mTelephonyManager.getCellLocation();

                mHandler.post(updateAllCellInfoResults);
            }
        });
    }

    private void updatePingState() {
        // Set all to unknown since the threads will take a few secs to update.
        mPingHostnameResultV4 = getResources().getString(R.string.radioInfo_unknown);
        mPingHostnameResultV6 = getResources().getString(R.string.radioInfo_unknown);
        mHttpClientTestResult = getResources().getString(R.string.radioInfo_unknown);

        mPingHostnameV4.setText(mPingHostnameResultV4);
        mPingHostnameV6.setText(mPingHostnameResultV6);
        mHttpClientTest.setText(mHttpClientTestResult);

        final Runnable updatePingResults = new Runnable() {
            public void run() {
                mPingHostnameV4.setText(mPingHostnameResultV4);
                mPingHostnameV6.setText(mPingHostnameResultV6);
                mHttpClientTest.setText(mHttpClientTestResult);
            }
        };

        Thread hostname = new Thread() {
            @Override
            public void run() {
                pingHostname();
                mHandler.post(updatePingResults);
            }
        };
        hostname.start();

        Thread httpClient = new Thread() {
            @Override
            public void run() {
                httpClientTest();
                mHandler.post(updatePingResults);
            }
        };
        httpClient.start();
    }

    private MenuItem.OnMenuItemClickListener mViewADNCallback =
            new MenuItem.OnMenuItemClickListener() {
        public boolean onMenuItemClick(MenuItem item) {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            // XXX We need to specify the component here because if we don't
            // the activity manager will try to resolve the type by calling
            // the content provider, which causes it to be loaded in a process
            // other than the Dialer process, which causes a lot of stuff to
            // break.
            intent.setClassName("com.android.phone", "com.android.phone.SimContacts");
            startActivity(intent);
            return true;
        }
    };

    private MenuItem.OnMenuItemClickListener mViewFDNCallback =
            new MenuItem.OnMenuItemClickListener() {
        public boolean onMenuItemClick(MenuItem item) {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            // XXX We need to specify the component here because if we don't
            // the activity manager will try to resolve the type by calling
            // the content provider, which causes it to be loaded in a process
            // other than the Dialer process, which causes a lot of stuff to
            // break.
            intent.setClassName("com.android.phone", "com.android.phone.settings.fdn.FdnList");
            startActivity(intent);
            return true;
        }
    };

    private MenuItem.OnMenuItemClickListener mViewSDNCallback =
            new MenuItem.OnMenuItemClickListener() {
        public boolean onMenuItemClick(MenuItem item) {
            Intent intent = new Intent(
                    Intent.ACTION_VIEW, Uri.parse("content://icc/sdn"));
            // XXX We need to specify the component here because if we don't
            // the activity manager will try to resolve the type by calling
            // the content provider, which causes it to be loaded in a process
            // other than the Dialer process, which causes a lot of stuff to
            // break.
            intent.setClassName("com.android.phone", "com.android.phone.ADNList");
            startActivity(intent);
            return true;
        }
    };

    private MenuItem.OnMenuItemClickListener mGetImsStatus =
            new MenuItem.OnMenuItemClickListener() {
        public boolean onMenuItemClick(MenuItem item) {
            boolean isImsRegistered = mPhone.isImsRegistered();
            boolean availableVolte = mPhone.isVolteEnabled();
            boolean availableWfc = mPhone.isWifiCallingEnabled();
            boolean availableVt = mPhone.isVideoEnabled();
            boolean availableUt = mPhone.isUtEnabled();

            final String imsRegString = isImsRegistered
                    ? getString(R.string.radio_info_ims_reg_status_registered)
                    : getString(R.string.radio_info_ims_reg_status_not_registered);

            final String available = getString(R.string.radio_info_ims_feature_status_available);
            final String unavailable = getString(
                    R.string.radio_info_ims_feature_status_unavailable);

            String imsStatus = getString(R.string.radio_info_ims_reg_status,
                    imsRegString,
                    availableVolte ? available : unavailable,
                    availableWfc ? available : unavailable,
                    availableVt ? available : unavailable,
                    availableUt ? available : unavailable);

            AlertDialog imsDialog = new AlertDialog.Builder(RadioInfo.this)
                    .setMessage(imsStatus)
                    .setTitle(getString(R.string.radio_info_ims_reg_status_title))
                    .create();

            imsDialog.show();

            return true;
        }
    };

    private MenuItem.OnMenuItemClickListener mSelectBandCallback =
            new MenuItem.OnMenuItemClickListener() {
        public boolean onMenuItemClick(MenuItem item) {
            Intent intent = new Intent();
            intent.setClass(RadioInfo.this, BandMode.class);
            startActivity(intent);
            return true;
        }
    };

    private MenuItem.OnMenuItemClickListener mToggleData =
            new MenuItem.OnMenuItemClickListener() {
        public boolean onMenuItemClick(MenuItem item) {
            int state = mTelephonyManager.getDataState();
            switch (state) {
                case TelephonyManager.DATA_CONNECTED:
                    mTelephonyManager.setDataEnabled(false);
                    break;
                case TelephonyManager.DATA_DISCONNECTED:
                    mTelephonyManager.setDataEnabled(true);
                    break;
                default:
                    // do nothing
                    break;
            }
            return true;
        }
    };

    private boolean isRadioOn() {
        //FIXME: Replace with a TelephonyManager call
        return mPhone.getServiceState().getState() != ServiceState.STATE_POWER_OFF;
    }

    private void updateRadioPowerState() {
        //delightful hack to prevent on-checked-changed calls from
        //actually forcing the radio preference to its transient/current value.
        mRadioPowerOnSwitch.setOnCheckedChangeListener(null);
        mRadioPowerOnSwitch.setChecked(isRadioOn());
        mRadioPowerOnSwitch.setOnCheckedChangeListener(mRadioPowerOnChangeListener);
    }

    void setImsVolteProvisionedState(boolean state) {
        Log.d(TAG, "setImsVolteProvisioned state: " + ((state) ? "on" : "off"));
        setImsConfigProvisionedState(IMS_VOLTE_PROVISIONED_CONFIG_ID, state);
    }

    void setImsVtProvisionedState(boolean state) {
        Log.d(TAG, "setImsVtProvisioned() state: " + ((state) ? "on" : "off"));
        setImsConfigProvisionedState(IMS_VT_PROVISIONED_CONFIG_ID, state);
    }

    void setImsWfcProvisionedState(boolean state) {
        Log.d(TAG, "setImsWfcProvisioned() state: " + ((state) ? "on" : "off"));
        setImsConfigProvisionedState(IMS_WFC_PROVISIONED_CONFIG_ID, state);
    }

    void setEabProvisionedState(boolean state) {
        Log.d(TAG, "setEabProvisioned() state: " + ((state) ? "on" : "off"));
        setImsConfigProvisionedState(EAB_PROVISIONED_CONFIG_ID, state);
    }

    void setImsConfigProvisionedState(int configItem, boolean state) {
        if (mPhone != null && mImsManager != null) {
            mQueuedWork.execute(new Runnable() {
                public void run() {
                    try {
                        mImsManager.getConfigInterface().setProvisionedValue(
                                configItem, state ? 1 : 0);
                    } catch (ImsException e) {
                        Log.e(TAG, "setImsConfigProvisioned() exception:", e);
                    }
                }
            });
        }
    }

    OnCheckedChangeListener mRadioPowerOnChangeListener = new OnCheckedChangeListener() {
        @Override
        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
            // TODO: b/145681511. Within current design, radio power on all of the phones need
            // to be controlled at the same time.
            Phone[] phones = PhoneFactory.getPhones();
            if (phones == null) {
                return;
            }
            log("toggle radio power: phone*" + phones.length + " " + (isRadioOn() ? "on" : "off"));
            for (int phoneIndex = 0; phoneIndex < phones.length; phoneIndex++) {
                if (phones[phoneIndex] != null) {
                    phones[phoneIndex].setRadioPower(isChecked);
                }
            }
        }
    };

    private boolean isImsVolteProvisioned() {
        if (mPhone != null && mImsManager != null) {
            return mImsManager.isVolteEnabledByPlatform(mPhone.getContext())
                && mImsManager.isVolteProvisionedOnDevice(mPhone.getContext());
        }
        return false;
    }

    OnCheckedChangeListener mImsVolteCheckedChangeListener = new OnCheckedChangeListener() {
        @Override
        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
            setImsVolteProvisionedState(isChecked);
        }
    };

    private boolean isImsVtProvisioned() {
        if (mPhone != null && mImsManager != null) {
            return mImsManager.isVtEnabledByPlatform(mPhone.getContext())
                && mImsManager.isVtProvisionedOnDevice(mPhone.getContext());
        }
        return false;
    }

    OnCheckedChangeListener mImsVtCheckedChangeListener = new OnCheckedChangeListener() {
        @Override
        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
            setImsVtProvisionedState(isChecked);
        }
    };

    private boolean isImsWfcProvisioned() {
        if (mPhone != null && mImsManager != null) {
            return mImsManager.isWfcEnabledByPlatform(mPhone.getContext())
                && mImsManager.isWfcProvisionedOnDevice(mPhone.getContext());
        }
        return false;
    }

    OnCheckedChangeListener mImsWfcCheckedChangeListener = new OnCheckedChangeListener() {
        @Override
        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
            setImsWfcProvisionedState(isChecked);
        }
    };

    private boolean isEabProvisioned() {
        return isFeatureProvisioned(EAB_PROVISIONED_CONFIG_ID, false);
    }

    OnCheckedChangeListener mEabCheckedChangeListener = new OnCheckedChangeListener() {
        @Override
        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
            setEabProvisionedState(isChecked);
        }
    };

    private boolean isFeatureProvisioned(int featureId, boolean defaultValue) {
        boolean provisioned = defaultValue;
        if (mImsManager != null) {
            try {
                ImsConfig imsConfig = mImsManager.getConfigInterface();
                if (imsConfig != null) {
                    provisioned =
                            (imsConfig.getProvisionedValue(featureId)
                                    == ImsConfig.FeatureValueConstants.ON);
                }
            } catch (ImsException ex) {
                Log.e(TAG, "isFeatureProvisioned() exception:", ex);
            }
        }

        log("isFeatureProvisioned() featureId=" + featureId + " provisioned=" + provisioned);
        return provisioned;
    }

    private static boolean isEabEnabledByPlatform(Context context) {
        if (context != null) {
            CarrierConfigManager configManager = (CarrierConfigManager)
                    context.getSystemService(Context.CARRIER_CONFIG_SERVICE);
            if (configManager != null && configManager.getConfig().getBoolean(
                        CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL)) {
                return true;
            }
        }
        return false;
    }

    private void updateImsProvisionedState() {
        if (!ImsManager.isImsSupportedOnDevice(mPhone.getContext())) {
            return;
        }
        log("updateImsProvisionedState isImsVolteProvisioned()=" + isImsVolteProvisioned());
        //delightful hack to prevent on-checked-changed calls from
        //actually forcing the ims provisioning to its transient/current value.
        mImsVolteProvisionedSwitch.setOnCheckedChangeListener(null);
        mImsVolteProvisionedSwitch.setChecked(isImsVolteProvisioned());
        mImsVolteProvisionedSwitch.setOnCheckedChangeListener(mImsVolteCheckedChangeListener);
        mImsVolteProvisionedSwitch.setEnabled(!IS_USER_BUILD
                && mImsManager.isVolteEnabledByPlatform(mPhone.getContext()));

        mImsVtProvisionedSwitch.setOnCheckedChangeListener(null);
        mImsVtProvisionedSwitch.setChecked(isImsVtProvisioned());
        mImsVtProvisionedSwitch.setOnCheckedChangeListener(mImsVtCheckedChangeListener);
        mImsVtProvisionedSwitch.setEnabled(!IS_USER_BUILD
                && mImsManager.isVtEnabledByPlatform(mPhone.getContext()));

        mImsWfcProvisionedSwitch.setOnCheckedChangeListener(null);
        mImsWfcProvisionedSwitch.setChecked(isImsWfcProvisioned());
        mImsWfcProvisionedSwitch.setOnCheckedChangeListener(mImsWfcCheckedChangeListener);
        mImsWfcProvisionedSwitch.setEnabled(!IS_USER_BUILD
                && mImsManager.isWfcEnabledByPlatform(mPhone.getContext()));

        mEabProvisionedSwitch.setOnCheckedChangeListener(null);
        mEabProvisionedSwitch.setChecked(isEabProvisioned());
        mEabProvisionedSwitch.setOnCheckedChangeListener(mEabCheckedChangeListener);
        mEabProvisionedSwitch.setEnabled(!IS_USER_BUILD
                && isEabEnabledByPlatform(mPhone.getContext()));
    }

    OnClickListener mDnsCheckButtonHandler = new OnClickListener() {
        public void onClick(View v) {
            //FIXME: Replace with a TelephonyManager call
            mPhone.disableDnsCheck(!mPhone.isDnsCheckDisabled());
            updateDnsCheckState();
        }
    };

    OnClickListener mOemInfoButtonHandler = new OnClickListener() {
        public void onClick(View v) {
            Intent intent = new Intent(OEM_RADIO_INFO_INTENT);
            try {
                startActivity(intent);
            } catch (android.content.ActivityNotFoundException ex) {
                log("OEM-specific Info/Settings Activity Not Found : " + ex);
                // If the activity does not exist, there are no OEM
                // settings, and so we can just do nothing...
            }
        }
    };

    OnClickListener mPingButtonHandler = new OnClickListener() {
        public void onClick(View v) {
            updatePingState();
        }
    };

    OnClickListener mUpdateSmscButtonHandler = new OnClickListener() {
        public void onClick(View v) {
            mUpdateSmscButton.setEnabled(false);
            mQueuedWork.execute(new Runnable() {
                public void run() {
                    mPhone.setSmscAddress(mSmsc.getText().toString(),
                            mHandler.obtainMessage(EVENT_UPDATE_SMSC_DONE));
                }
            });
        }
    };

    OnClickListener mRefreshSmscButtonHandler = new OnClickListener() {
        public void onClick(View v) {
            refreshSmsc();
        }
    };

    OnClickListener mCarrierProvisioningButtonHandler = new OnClickListener() {
        public void onClick(View v) {
            final Intent intent = new Intent("com.android.settings.CARRIER_PROVISIONING");
            final ComponentName serviceComponent = ComponentName.unflattenFromString(
                    "com.android.omadm.service/.DMIntentReceiver");
            intent.setComponent(serviceComponent);
            sendBroadcast(intent);
        }
    };

    OnClickListener mTriggerCarrierProvisioningButtonHandler = new OnClickListener() {
        public void onClick(View v) {
            final Intent intent = new Intent("com.android.settings.TRIGGER_CARRIER_PROVISIONING");
            final ComponentName serviceComponent = ComponentName.unflattenFromString(
                    "com.android.omadm.service/.DMIntentReceiver");
            intent.setComponent(serviceComponent);
            sendBroadcast(intent);
        }
    };

    AdapterView.OnItemSelectedListener mPreferredNetworkHandler =
            new AdapterView.OnItemSelectedListener() {

        public void onItemSelected(AdapterView parent, View v, int pos, long id) {
            if (mPreferredNetworkTypeResult != pos && pos >= 0
                    && pos <= PREFERRED_NETWORK_LABELS.length - 2) {
                mPreferredNetworkTypeResult = pos;

                // TODO: Possibly migrate this to TelephonyManager.setPreferredNetworkType()
                // which today still has some issues (mostly that the "set" is conditional
                // on a successful modem call, which is not what we want). Instead we always
                // want this setting to be set, so that if the radio hiccups and this setting
                // is for some reason unsuccessful, future calls to the radio will reflect
                // the users's preference which is set here.
                final int subId = mPhone.getSubId();
                if (SubscriptionManager.isUsableSubIdValue(subId)) {
                    Settings.Global.putInt(mPhone.getContext().getContentResolver(),
                            PREFERRED_NETWORK_MODE + subId, mPreferredNetworkTypeResult);
                }
                log("Calling setPreferredNetworkType(" + mPreferredNetworkTypeResult + ")");
                Message msg = mHandler.obtainMessage(EVENT_SET_PREFERRED_TYPE_DONE);
                mPhone.setPreferredNetworkType(mPreferredNetworkTypeResult, msg);
            }
        }

        public void onNothingSelected(AdapterView parent) {
        }
    };

    AdapterView.OnItemSelectedListener mSelectPhoneIndexHandler =
            new AdapterView.OnItemSelectedListener() {

        public void onItemSelected(AdapterView parent, View v, int pos, long id) {
            if (pos >= 0 && pos <= sPhoneIndexLabels.length - 1) {
                // the array position is equal to the phone index
                int phoneIndex = pos;
                Phone[] phones = PhoneFactory.getPhones();
                if (phones == null || phones.length <= phoneIndex) {
                    return;
                }
                // getSubId says it takes a slotIndex, but it actually takes a phone index
                int subId = SubscriptionManager.INVALID_SUBSCRIPTION_ID;
                int[] subIds = SubscriptionManager.getSubId(phoneIndex);
                if (subIds != null && subIds.length > 0) {
                    subId = subIds[0];
                }
                mSelectedPhoneIndex = phoneIndex;

                updatePhoneIndex(phoneIndex, subId);
            }
        }

        public void onNothingSelected(AdapterView parent) {
        }
    };

    AdapterView.OnItemSelectedListener mCellInfoRefreshRateHandler  =
            new AdapterView.OnItemSelectedListener() {

        public void onItemSelected(AdapterView parent, View v, int pos, long id) {
            mCellInfoRefreshRateIndex = pos;
            mTelephonyManager.setCellInfoListRate(CELL_INFO_REFRESH_RATES[pos]);
            updateAllCellInfo();
        }

        public void onNothingSelected(AdapterView parent) {
        }
    };

    boolean isCbrsSupported() {
        return getResources().getBoolean(
              com.android.internal.R.bool.config_cbrs_supported);
    }

    void updateCbrsDataState(boolean state) {
        Log.d(TAG, "setCbrsDataSwitchState() state:" + ((state) ? "on" : "off"));
        if (mTelephonyManager != null) {
            mQueuedWork.execute(new Runnable() {
                public void run() {
                    mTelephonyManager.setOpportunisticNetworkState(state);
                    mHandler.post(() -> mCbrsDataSwitch.setChecked(getCbrsDataState()));
                }
            });
        }
    }

    boolean getCbrsDataState() {
        boolean state = false;
        if (mTelephonyManager != null) {
            state = mTelephonyManager.isOpportunisticNetworkEnabled();
        }
        Log.d(TAG, "getCbrsDataState() state:" + ((state) ? "on" : "off"));
        return state;
    }

    OnCheckedChangeListener mCbrsDataSwitchChangeListener = new OnCheckedChangeListener() {
        @Override
        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
            updateCbrsDataState(isChecked);
        }
    };

    private void showDsdsChangeDialog() {
        final AlertDialog confirmDialog = new Builder(RadioInfo.this)
                .setTitle(R.string.dsds_dialog_title)
                .setMessage(R.string.dsds_dialog_message)
                .setPositiveButton(R.string.dsds_dialog_confirm, mOnDsdsDialogConfirmedListener)
                .setNegativeButton(R.string.dsds_dialog_cancel, mOnDsdsDialogConfirmedListener)
                .create();
        confirmDialog.show();
    }

    private static boolean isDsdsSupported() {
        return (TelephonyManager.getDefault().isMultiSimSupported()
            == TelephonyManager.MULTISIM_ALLOWED);
    }

    private static boolean isDsdsEnabled() {
        return TelephonyManager.getDefault().getPhoneCount() > 1;
    }

    private void performDsdsSwitch() {
        mTelephonyManager.switchMultiSimConfig(mDsdsSwitch.isChecked() ? 2 : 1);
    }

    /**
     * @return {@code True} if the device is only supported dsds mode.
     */
    private boolean dsdsModeOnly() {
        String dsdsMode = SystemProperties.get(DSDS_MODE_PROPERTY);
        return !TextUtils.isEmpty(dsdsMode) && Integer.parseInt(dsdsMode) == ALWAYS_ON_DSDS_MODE;
    }

    DialogInterface.OnClickListener mOnDsdsDialogConfirmedListener =
            new DialogInterface.OnClickListener() {
        @Override
        public void onClick(DialogInterface dialog, int which) {
            if (which == DialogInterface.BUTTON_POSITIVE) {
                mDsdsSwitch.toggle();
                performDsdsSwitch();
            }
        }
    };
}
