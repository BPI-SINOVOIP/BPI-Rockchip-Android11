

package com.android.tv.settings.bluetooth;

import android.content.Context;
import android.net.wifi.WifiConfiguration;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.provider.Settings;
import androidx.preference.SwitchPreference;
import androidx.leanback.preference.LeanbackPreferenceFragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.TwoStatePreference;

import com.android.settingslib.wifi.AccessPoint;
import com.android.settingslib.wifi.AccessPointPreference;

import java.util.List;

import com.android.internal.net.VpnProfile;
import com.android.tv.settings.R;

import java.util.Collection;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

import android.util.Log;
import android.security.Credentials;
import android.security.KeyStore;
import android.net.IConnectivityManager;
import android.os.ServiceManager;
import android.util.ArraySet;

import java.util.Map;

import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Message;
import android.provider.Settings;
import android.util.Log;
import android.widget.ListView;
import android.widget.Switch;
import android.widget.Toast;

import com.android.internal.logging.MetricsLogger;

import com.android.tv.settings.R;
//import com.android.tv.settings.search.Index;
import com.android.settingslib.WirelessUtils;
import com.android.settingslib.bluetooth.LocalBluetoothAdapter;
import com.android.settingslib.bluetooth.LocalBluetoothManager;
//import com.android.tv.settings.data.ConstData;
//import com.android.tv.settings.vpn.*;

import android.annotation.UiThread;
import android.annotation.WorkerThread;
import android.app.AppOpsManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.ConnectivityManager;
import android.net.IConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.UserHandle;
import android.os.UserManager;
import android.security.Credentials;
import android.security.KeyStore;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toolbar;

//import com.android.internal.logging.MetricsProto.MetricsEvent;
import com.android.internal.net.LegacyVpnInfo;
import com.android.internal.net.VpnConfig;
import com.android.internal.net.VpnProfile;
import com.android.internal.util.ArrayUtils;
import com.android.settingslib.RestrictedLockUtils;
import com.google.android.collect.Lists;

import java.util.ArrayList;
import java.util.Collections;

import static android.app.AppOpsManager.OP_ACTIVATE_VPN;

import android.os.SystemProperties;
import androidx.annotation.Keep;
import androidx.annotation.VisibleForTesting;
import android.text.BidiFormatter;
import android.text.TextUtils;

import com.android.settingslib.bluetooth.BluetoothCallback;
import com.android.settingslib.bluetooth.BluetoothDeviceFilter;
import com.android.settingslib.bluetooth.CachedBluetoothDevice;

import java.util.WeakHashMap;

import static android.os.UserManager.DISALLOW_CONFIG_BLUETOOTH;

import android.bluetooth.BluetoothDevice;

@Keep
public class BluetoothFragment extends LeanbackPreferenceFragment implements Preference.OnPreferenceClickListener, BluetoothCallback {
    private static final String TAG = "BluetoothFragment";
    private static final String BTOPP_ACTION_OPEN_RECEIVED_FILES =
            "android.btopp.intent.action.OPEN_RECEIVED_FILES";
    private static final String KEY_BLUETOOTH_ENABLE = "bluetooth_enable";
    private static final String KEY_BLUETOOTH_RENAME = "bluetooth_rename";
    private static final String KEY_BLUETOOTH_PAIRED = "bluetooth_paried";
    private static final String KEY_BLUETOOTH_AVAILABLE = "bluetooth_avaliable";
    private static final String KEY_BLUETOOTH_REFRESH = "bluetooth_refresh";
    private static final String KEY_BLUETOOTH_RECEIVED = "bluetooth_received";

    @VisibleForTesting
    static final String ACTION_OPEN_FILES = "com.android.bluetooth.action.TransferHistory";
    @VisibleForTesting
    static final String EXTRA_SHOW_ALL_FILES = "android.btopp.intent.extra.SHOW_ALL";
    @VisibleForTesting
    static final String EXTRA_DIRECTION = "direction";

    private LocalBluetoothAdapter mLocalAdapter;
    private LocalBluetoothManager mLocalManager;
    ;
    private Map<String, Preference> mPreferenceCache;
    private BluetoothDeviceFilter.Filter mFilter = BluetoothDeviceFilter.ALL_FILTER;
    private boolean mInitiateDiscoverable;
    final WeakHashMap<CachedBluetoothDevice, BluetoothDevicePreference> mDevicePreferenceMap =
            new WeakHashMap<CachedBluetoothDevice, BluetoothDevicePreference>();
    BluetoothDevice mSelectedDevice;
    private boolean mAvailableDevicesCategoryIsPresent;
    private boolean mInitialScanStarted;
    private PreferenceGroup mDeviceListGroup;
    private Preference mPreferenceBluetoothRename;
    private Preference mPreferenceBluetoothRefresh;
    private Preference mPreferenceBluetoothReceived;
    private SwitchPreference mPreferenceBluetoothEnable;
    private PreferenceCategory mCategoryBluetoothPaired;
    private BluetoothProgressCategory mCategoryBluetoothAvailable;
    private PreferenceScreen mPreferenceScreen;
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            final int state =
                    intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
            Log.i(TAG,"Broadcast state = " + state + ",action = " + action);

            if (action.equals(BluetoothAdapter.ACTION_LOCAL_NAME_CHANGED)) {
                updateDeviceName(context);
            }

            if (state == BluetoothAdapter.STATE_ON) {
                mInitiateDiscoverable = true;
            }
        }

        private void updateDeviceName(Context context) {
            if (mLocalAdapter.isEnabled() && mPreferenceBluetoothRename != null) {
                final Resources res = context.getResources();
                final Locale locale = res.getConfiguration().getLocales().get(0);
                final BidiFormatter bidiFormatter = BidiFormatter.getInstance(locale);
                mPreferenceBluetoothRename.setSummary(bidiFormatter.unicodeWrap(mLocalAdapter.getName()));
            }
        }
    };

    private final BroadcastReceiver mRenameReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            // Broadcast receiver is always running on the UI thread here,
            // so we don't need consider thread synchronization.
            int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
            handleStateChanged(state);
        }
    };

    public static BluetoothFragment newInstance() {
        return new BluetoothFragment();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onStart() {
        super.onStart();
    }

    @Override
    public void onResume() {
        IntentFilter bluetoothChangeFilter = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        IntentFilter bluetoothRenameFilter = new IntentFilter(BluetoothAdapter.ACTION_LOCAL_NAME_CHANGED);
        getActivity().registerReceiver(mReceiver, bluetoothRenameFilter);
        getActivity().registerReceiver(mRenameReceiver, bluetoothChangeFilter);
        super.onResume();
        if (mLocalManager == null /*|| isUiRestricted()*/)
            return;
        mLocalManager.setForegroundActivity(getActivity());
        mLocalManager.getEventManager().registerCallback(this);
        updateProgressUi(mLocalAdapter.isDiscovering());
        updateBluetooth();
        mInitiateDiscoverable = true;
    }

    @Override
    public void onPause() {
        getActivity().unregisterReceiver(mReceiver);
        getActivity().unregisterReceiver(mRenameReceiver);
        super.onPause();
        if (mLocalManager == null /*|| isUiRestricted()*/) {
            return;
        }
        removeAllDevices();
        mLocalManager.setForegroundActivity(null);
        mLocalManager.getEventManager().unregisterCallback(this);
        mLocalAdapter.setScanMode(BluetoothAdapter.SCAN_MODE_CONNECTABLE);
    }

    @Override
    public void onStop() {
        super.onStop();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.bluetooth, null);
        mLocalManager = Utils.getLocalBtManager(getContext());
        if (mLocalManager == null) {
            Log.i(TAG,"mLocalManager = null");
            // Bluetooth is not supported
            mLocalAdapter = null;
            mPreferenceBluetoothEnable.setEnabled(false);
        } else {
            Log.i(TAG,"mLocalManager != null");
            mLocalAdapter = mLocalManager.getBluetoothAdapter();
        }
        mInitiateDiscoverable = true;
        mPreferenceScreen = getPreferenceScreen();
        mPreferenceBluetoothEnable = (SwitchPreference) findPreference(KEY_BLUETOOTH_ENABLE);
        mPreferenceBluetoothRename = (Preference) findPreference(KEY_BLUETOOTH_RENAME);
        mCategoryBluetoothAvailable = (BluetoothProgressCategory) findPreference(KEY_BLUETOOTH_AVAILABLE);
        mCategoryBluetoothPaired = (PreferenceCategory) findPreference(KEY_BLUETOOTH_PAIRED);
        mPreferenceBluetoothRefresh = findPreference(KEY_BLUETOOTH_REFRESH);
        mPreferenceBluetoothReceived = findPreference(KEY_BLUETOOTH_RECEIVED);
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        /*if (preference.getKey() == null) {
            return super.onPreferenceTreeClick(preference);
        }*/
        if (preference instanceof BluetoothDevicePreference) {
            BluetoothDevicePreference btPreference = (BluetoothDevicePreference) preference;
            CachedBluetoothDevice device = btPreference.getCachedDevice();
            mSelectedDevice = device.getDevice();
            onDevicePreferenceClick(btPreference);
            return true;
        }
        if (preference == mPreferenceBluetoothRename) {
            // MetricsLogger.action(getActivity(),
            // MetricsEvent.ACTION_BLUETOOTH_RENAME);
            new BluetoothNameDialogFragment().show(getFragmentManager(),
                    "rename device");
            return true;
        } else if (preference == mPreferenceBluetoothEnable) {
            refreshSwitchView();
            return true;
        } else if (preference == mPreferenceBluetoothRefresh) {
            startScanning();
            return true;
        } else if (preference == mPreferenceBluetoothReceived) {
            Intent intent = new Intent(ACTION_OPEN_FILES);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            intent.putExtra(EXTRA_DIRECTION, 1 /* DIRECTION_INBOUND */);
            intent.putExtra(EXTRA_SHOW_ALL_FILES, true);
            getActivity().startActivity(intent);
        }
        return super.onPreferenceTreeClick(preference);

    }


    @Override
    public boolean onPreferenceClick(Preference preference) {
        return false;
    }

    private void updateBluetooth() {
        if (mLocalAdapter == null)
            return;
        handleStateChanged(mLocalAdapter.getBluetoothState());
        if (mLocalAdapter != null) {
            updateContent(mLocalAdapter.getBluetoothState());
        }
    }

    private void refreshSwitchView() {
        boolean isChecked = mPreferenceBluetoothEnable.isChecked();
        Log.i(TAG,"refreshSwitchView isChecked = " + isChecked);
        if (isChecked && !WirelessUtils.isRadioAllowed(getContext(), Settings.Global.RADIO_BLUETOOTH)) {
            Toast.makeText(getContext(), R.string.wifi_in_airplane_mode, Toast.LENGTH_SHORT).show();
            mPreferenceBluetoothEnable.setChecked(false);
        }

        //MetricsLogger.action(mContext, MetricsEvent.ACTION_BLUETOOTH_TOGGLE, isChecked);
        Log.i(TAG,"refreshSwitchView mLocalAdapter = " + mLocalAdapter);
        if (mLocalAdapter != null) {
            boolean status = mLocalAdapter.setBluetoothEnabled(isChecked);
            if (isChecked && !status) {
                mPreferenceBluetoothEnable.setChecked(false);
                mPreferenceBluetoothEnable.setEnabled(true);
                // mSwitchBar.setTextViewLabel(false);
                return;
            }
        }
        mPreferenceBluetoothEnable.setEnabled(false);
    }

    private void handleStateChanged(int state) {
        Log.i(TAG,"handleStateChanged state = " + state);
        switch (state) {
            case BluetoothAdapter.STATE_TURNING_ON:
                mPreferenceBluetoothEnable.setEnabled(false);
                break;
            case BluetoothAdapter.STATE_ON:
                mPreferenceBluetoothEnable.setEnabled(true);
                mPreferenceBluetoothEnable.setChecked(true);
                //updateSearchIndex(true);
                break;
            case BluetoothAdapter.STATE_TURNING_OFF:
                mPreferenceBluetoothEnable.setEnabled(false);
                break;
            case BluetoothAdapter.STATE_OFF:
                mPreferenceBluetoothEnable.setEnabled(true);
                mPreferenceBluetoothEnable.setChecked(false);
                //updateSearchIndex(false);
                break;
            default:
                mPreferenceBluetoothEnable.setEnabled(true);
                mPreferenceBluetoothEnable.setChecked(false);
                //updateSearchIndex(false);
        }
    }

    private void updateContent(int bluetoothState) {
        Log.i(TAG,"updateContent bluetoothState = " + bluetoothState);
        final PreferenceScreen preferenceScreen = getPreferenceScreen();
        int messageId = 0;

        switch (bluetoothState) {
            case BluetoothAdapter.STATE_ON:
                mDevicePreferenceMap.clear();

              /*  if (isUiRestricted()) {
                    messageId = R.string.bluetooth_empty_list_user_restricted;
                    break;
                }*/
                mCategoryBluetoothPaired.removeAll();
                mCategoryBluetoothAvailable.removeAll();
                mPreferenceScreen.removePreference(mCategoryBluetoothPaired);
                mPreferenceScreen.removePreference(mCategoryBluetoothAvailable);
                mPreferenceScreen.addPreference(mCategoryBluetoothPaired);
                mPreferenceScreen.addPreference(mCategoryBluetoothAvailable);
                /*getPreferenceScreen().removePreference(mPairedDevicesCategory);
                getPreferenceScreen().removePreference(mAvailableDevicesCategory);
                getPreferenceScreen().addPreference(mPairedDevicesCategory);
                getPreferenceScreen().addPreference(mAvailableDevicesCategory);*/
                //getPreferenceScreen().addPreference(mMyDevicePreference);

                // Paired devices category
                addDeviceCategory(mCategoryBluetoothPaired,
                        R.string.bluetooth_preference_paired_devices,
                        BluetoothDeviceFilter.BONDED_DEVICE_FILTER, true);
                int numberOfPairedDevices = mCategoryBluetoothPaired.getPreferenceCount();

                if (/*isUiRestricted() ||*/ numberOfPairedDevices <= 0) {
                    if (mCategoryBluetoothPaired != null) {
                        preferenceScreen.removePreference(mCategoryBluetoothPaired);
                    }
                } else {
                    if (preferenceScreen.findPreference(KEY_BLUETOOTH_PAIRED) == null) {
                        preferenceScreen.addPreference(mCategoryBluetoothPaired);
                    }
                }

                addDeviceCategory(mCategoryBluetoothAvailable,
                        R.string.bluetooth_preference_found_devices,
                        BluetoothDeviceFilter.UNBONDED_DEVICE_FILTER, mInitialScanStarted);

                if (!mInitialScanStarted) {
                    startScanning();
                }

                final Resources res = getResources();
                final Locale locale = res.getConfiguration().getLocales().get(0);
                final BidiFormatter bidiFormatter = BidiFormatter.getInstance(locale);
                mPreferenceBluetoothRename.setSummary(bidiFormatter.unicodeWrap(mLocalAdapter.getName()));

                //getActivity().invalidateOptionsMenu();

                // mLocalAdapter.setScanMode is internally synchronized so it is okay for multiple
                // threads to execute.
                updateOtherOpration();
                if (mInitiateDiscoverable) {
                    // Make the device visible to other devices.
                    mLocalAdapter.setScanMode(BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE);
                    mInitiateDiscoverable = false;
                }
                return; // not break

            case BluetoothAdapter.STATE_TURNING_OFF:
                messageId = R.string.bluetooth_turning_off;
                break;

            case BluetoothAdapter.STATE_OFF:
                setOffMessage();
              /*  if (isUiRestricted()) {
                    messageId = R.string.bluetooth_empty_list_user_restricted;
                }*/
                break;

            case BluetoothAdapter.STATE_TURNING_ON:
                messageId = R.string.bluetooth_turning_on;
                mInitialScanStarted = false;
                break;
        }

        setDeviceListGroup(preferenceScreen);
        removeAllDevices();
        updateOtherOpration();
        /*if (messageId != 0) {
            getEmptyTextView().setText(messageId);
        }
        if (!isUiRestricted()) {
            getActivity().invalidateOptionsMenu();
        }*/
    }

    private void addDeviceCategory(PreferenceGroup preferenceGroup, int titleId,
                                   BluetoothDeviceFilter.Filter filter, boolean addCachedDevices) {
        cacheRemoveAllPrefs(preferenceGroup);
        preferenceGroup.setTitle(titleId);
        setFilter(filter);
        setDeviceListGroup(preferenceGroup);
        if (addCachedDevices) {
            addCachedDevices();
        }
        preferenceGroup.setEnabled(true);
        removeCachedPrefs(preferenceGroup);
    }

    private void cacheRemoveAllPrefs(PreferenceGroup group) {
        mPreferenceCache = new ArrayMap<String, Preference>();
        final int N = group.getPreferenceCount();
        for (int i = 0; i < N; i++) {
            Preference p = group.getPreference(i);
            if (TextUtils.isEmpty(p.getKey())) {
                continue;
            }
            mPreferenceCache.put(p.getKey(), p);
        }
    }

    private void setFilter(BluetoothDeviceFilter.Filter filter) {
        mFilter = filter;
    }

    private void setFilter(int filterType) {
        mFilter = BluetoothDeviceFilter.getFilter(filterType);
    }

    private void setDeviceListGroup(PreferenceGroup preferenceGroup) {
        mDeviceListGroup = preferenceGroup;
    }

    protected void removeCachedPrefs(PreferenceGroup group) {
        for (Preference p : mPreferenceCache.values()) {
            group.removePreference(p);
        }
        mPreferenceCache = null;
    }

    private void removeAllDevices() {
        mLocalAdapter.stopScanning();
        if (mDeviceListGroup != mPreferenceScreen) {
            mDevicePreferenceMap.clear();
            mDeviceListGroup.removeAll();
        } else {
            mCategoryBluetoothPaired.removeAll();
            mCategoryBluetoothAvailable.removeAll();
            mPreferenceScreen.removePreference(mCategoryBluetoothPaired);
            mPreferenceScreen.removePreference(mCategoryBluetoothAvailable);
        }
    }

    private void startScanning() {
       /* if (isUiRestricted()) {
            return;
        }*/
        Log.i(TAG, "startScanning");
        if (!mAvailableDevicesCategoryIsPresent) {
            getPreferenceScreen().addPreference(mCategoryBluetoothAvailable);
            mAvailableDevicesCategoryIsPresent = true;
        }

        if (mCategoryBluetoothAvailable != null) {
            setDeviceListGroup(mCategoryBluetoothAvailable);
            removeAllDevices();
        }

        mLocalManager.getCachedDeviceManager().clearNonBondedDevices();
        mCategoryBluetoothAvailable.removeAll();
        mInitialScanStarted = true;
        mLocalAdapter.startScanning(true);
    }

    @Override
    public void onBluetoothStateChanged(int bluetoothState) {
        if (bluetoothState == BluetoothAdapter.STATE_OFF) {
            updateProgressUi(false);
        }
        Log.i(TAG, "onBluetoothStateChanged bluetoothState = " + bluetoothState);
        //super.onBluetoothStateChanged(bluetoothState);

        // If BT is turned off/on staying in the same BT Settings screen
        // discoverability to be set again
        if (BluetoothAdapter.STATE_ON == bluetoothState)
            mInitiateDiscoverable = true;
        updateContent(bluetoothState);
    }

    @Override
    public void onScanningStateChanged(boolean started) {
        Log.i(TAG, "onScanningStateChanged started = " + started);
        updateProgressUi(started);
        updateOtherOpration();
        //super.onScanningStateChanged(started);
        // Update options' enabled state
        /*if (getActivity() != null) {
            getActivity().invalidateOptionsMenu();
        }*/
    }

    public void onDeviceAdded(CachedBluetoothDevice cachedDevice) {
        Log.i(TAG, "onDeviceAdded");
        if (mDevicePreferenceMap.get(cachedDevice) != null) {
            return;
        }

        // Prevent updates while the list shows one of the state messages
        if (mLocalAdapter.getBluetoothState() != BluetoothAdapter.STATE_ON) return;

        if (mFilter.matches(cachedDevice.getDevice())) {
            createDevicePreference(cachedDevice);
        }
    }

    public void onDeviceDeleted(CachedBluetoothDevice cachedDevice) {
        Log.i(TAG, "onDeviceDeleted");
        BluetoothDevicePreference preference = mDevicePreferenceMap.remove(cachedDevice);
        if (preference != null) {
            mDeviceListGroup.removePreference(preference);
        }
    }

    @Override
    public void onDeviceBondStateChanged(CachedBluetoothDevice cachedDevice, int bondState) {
        Log.i(TAG, "setDeviceListGroup");
        setDeviceListGroup(getPreferenceScreen());
        removeAllDevices();
        updateContent(mLocalAdapter.getBluetoothState());
    }

    @Override
    public void onAudioModeChanged(){
        //do nothing
        Log.i(TAG, "onAudioModeChanged");
    }

    @Override
    public void onActiveDeviceChanged(CachedBluetoothDevice activeDevice, int bluetoothProfile){
        //do nothing
        Log.i(TAG, "onActiveDeviceChanged bluetoothProfile = " + bluetoothProfile);
    }

    @Override
    public void onConnectionStateChanged(CachedBluetoothDevice cachedDevice, int state) {
        Log.i(TAG, "onConnectionStateChanged state = " + state);
    }

    void createDevicePreference(CachedBluetoothDevice cachedDevice) {
        if (mDeviceListGroup == null) {
            Log.w(TAG, "Trying to create a device preference before the list group/category "
                    + "exists!");
            return;
        }

        String key = cachedDevice.getDevice().getAddress();
        BluetoothDevicePreference preference = (BluetoothDevicePreference) getCachedPreference(key);

        if (preference == null) {
            preference = new BluetoothDevicePreference(getPreferenceManager().getContext(), cachedDevice);
            preference.setKey(key);
            mDeviceListGroup.addPreference(preference);
            Log.i(TAG,"preference == null,preference.key = " + preference.getKey() + ",preference.Title = " + preference.getTitle());
        } else {
            // Tell the preference it is being re-used in case there is new info in the
            // cached device.
            preference.rebind();
            Log.i(TAG,"preferece ! = null");
        }

        mDevicePreferenceMap.put(cachedDevice, preference);
    }

    protected Preference getCachedPreference(String key) {
        return mPreferenceCache != null ? mPreferenceCache.remove(key) : null;
    }

    void onDevicePreferenceClick(BluetoothDevicePreference btPreference) {
        mLocalAdapter.stopScanning();
        btPreference.setFragmentManager(getFragmentManager());
        btPreference.onClicked();
    }

    void addCachedDevices() {
        Collection<CachedBluetoothDevice> cachedDevices =
                mLocalManager.getCachedDeviceManager().getCachedDevicesCopy();
        for (CachedBluetoothDevice cachedDevice : cachedDevices) {
            onDeviceAdded(cachedDevice);
        }
    }

    private void updateProgressUi(boolean start) {
        if (mDeviceListGroup instanceof BluetoothProgressCategory) {
            ((BluetoothProgressCategory) mDeviceListGroup).setProgress(start);
        }
    }

    private void setOffMessage() {
        mCategoryBluetoothPaired.removeAll();
        mCategoryBluetoothAvailable.removeAll();
        mPreferenceScreen.removePreference(mCategoryBluetoothPaired);
        mPreferenceScreen.removePreference(mCategoryBluetoothAvailable);
    }

    private void updateOtherOpration() {
        boolean bluetoothIsEnabled = mLocalAdapter.getBluetoothState() == BluetoothAdapter.STATE_ON;
        boolean isDiscovering = mLocalAdapter.isDiscovering();
        mPreferenceBluetoothRefresh.setEnabled(bluetoothIsEnabled && !isDiscovering);
        mPreferenceBluetoothRename.setEnabled(bluetoothIsEnabled);
    }
}
