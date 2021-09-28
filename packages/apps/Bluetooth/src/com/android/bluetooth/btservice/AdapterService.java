/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.bluetooth.btservice;

import static com.android.bluetooth.Utils.addressToBytes;
import static com.android.bluetooth.Utils.callerIsSystemOrActiveOrManagedUser;
import static com.android.bluetooth.Utils.callerIsSystemOrActiveUser;
import static com.android.bluetooth.Utils.enforceBluetoothAdminPermission;
import static com.android.bluetooth.Utils.enforceBluetoothPermission;
import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;
import static com.android.bluetooth.Utils.enforceDumpPermission;
import static com.android.bluetooth.Utils.enforceLocalMacAddressPermission;

import android.annotation.Nullable;
import android.app.ActivityManager;
import android.app.AlarmManager;
import android.app.AppOpsManager;
import android.app.PendingIntent;
import android.app.PropertyInvalidatedCache;
import android.app.Service;
import android.app.admin.DevicePolicyManager;
import android.bluetooth.BluetoothActivityEnergyInfo;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAdapter.ActiveDeviceUse;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothProtoEnums;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetooth;
import android.bluetooth.IBluetoothCallback;
import android.bluetooth.IBluetoothMetadataListener;
import android.bluetooth.IBluetoothSocketManager;
import android.bluetooth.OobData;
import android.bluetooth.UidTraffic;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.BatteryStats;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.PowerManager;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.os.ResultReceiver;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;
import android.util.SparseArray;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.BluetoothStatsLog;
import com.android.bluetooth.Utils;
import com.android.bluetooth.a2dp.A2dpService;
import com.android.bluetooth.a2dpsink.A2dpSinkService;
import com.android.bluetooth.btservice.RemoteDevices.DeviceProperties;
import com.android.bluetooth.btservice.bluetoothkeystore.BluetoothKeystoreService;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.bluetooth.btservice.storage.MetadataDatabase;
import com.android.bluetooth.gatt.GattService;
import com.android.bluetooth.hearingaid.HearingAidService;
import com.android.bluetooth.hfp.HeadsetService;
import com.android.bluetooth.hfpclient.HeadsetClientService;
import com.android.bluetooth.hid.HidDeviceService;
import com.android.bluetooth.hid.HidHostService;
import com.android.bluetooth.map.BluetoothMapService;
import com.android.bluetooth.mapclient.MapClientService;
import com.android.bluetooth.pan.PanService;
import com.android.bluetooth.pbap.BluetoothPbapService;
import com.android.bluetooth.pbapclient.PbapClientService;
import com.android.bluetooth.sap.SapService;
import com.android.bluetooth.sdp.SdpManager;
import com.android.internal.R;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.app.IBatteryStats;
import com.android.internal.util.ArrayUtils;

import com.google.protobuf.InvalidProtocolBufferException;

import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

public class AdapterService extends Service {
    private static final String TAG = "BluetoothAdapterService";
    private static final boolean DBG = true;
    private static final boolean VERBOSE = false;
    private static final int MIN_ADVT_INSTANCES_FOR_MA = 5;
    private static final int MIN_OFFLOADED_FILTERS = 10;
    private static final int MIN_OFFLOADED_SCAN_STORAGE_BYTES = 1024;

    private final Object mEnergyInfoLock = new Object();
    private int mStackReportedState;
    private long mTxTimeTotalMs;
    private long mRxTimeTotalMs;
    private long mIdleTimeTotalMs;
    private long mEnergyUsedTotalVoltAmpSecMicro;
    private final SparseArray<UidTraffic> mUidTraffic = new SparseArray<>();

    private final ArrayList<ProfileService> mRegisteredProfiles = new ArrayList<>();
    private final ArrayList<ProfileService> mRunningProfiles = new ArrayList<>();

    public static final String ACTION_LOAD_ADAPTER_PROPERTIES =
            "com.android.bluetooth.btservice.action.LOAD_ADAPTER_PROPERTIES";
    public static final String ACTION_SERVICE_STATE_CHANGED =
            "com.android.bluetooth.btservice.action.STATE_CHANGED";
    public static final String EXTRA_ACTION = "action";
    public static final int PROFILE_CONN_REJECTED = 2;

    private static final String ACTION_ALARM_WAKEUP =
            "com.android.bluetooth.btservice.action.ALARM_WAKEUP";

    static final String BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY = "persist.bluetooth.btsnooplogmode";
    static final String BLUETOOTH_BTSNOOP_DEFAULT_MODE_PROPERTY =
            "persist.bluetooth.btsnoopdefaultmode";
    private String mSnoopLogSettingAtEnable = "empty";
    private String mDefaultSnoopLogSettingAtEnable = "empty";

    public static final String BLUETOOTH_ADMIN_PERM = android.Manifest.permission.BLUETOOTH_ADMIN;
    public static final String BLUETOOTH_PRIVILEGED =
            android.Manifest.permission.BLUETOOTH_PRIVILEGED;
    static final String BLUETOOTH_PERM = android.Manifest.permission.BLUETOOTH;
    static final String LOCAL_MAC_ADDRESS_PERM = android.Manifest.permission.LOCAL_MAC_ADDRESS;
    static final String RECEIVE_MAP_PERM = android.Manifest.permission.RECEIVE_BLUETOOTH_MAP;

    private static final String PHONEBOOK_ACCESS_PERMISSION_PREFERENCE_FILE =
            "phonebook_access_permission";
    private static final String MESSAGE_ACCESS_PERMISSION_PREFERENCE_FILE =
            "message_access_permission";
    private static final String SIM_ACCESS_PERMISSION_PREFERENCE_FILE = "sim_access_permission";

    private static final int CONTROLLER_ENERGY_UPDATE_TIMEOUT_MILLIS = 30;

    private final ArrayList<DiscoveringPackage> mDiscoveringPackages = new ArrayList<>();

    static {
        classInitNative();
    }

    private static AdapterService sAdapterService;

    public static synchronized AdapterService getAdapterService() {
        Log.d(TAG, "getAdapterService() - returning " + sAdapterService);
        return sAdapterService;
    }

    private static synchronized void setAdapterService(AdapterService instance) {
        Log.d(TAG, "setAdapterService() - trying to set service to " + instance);
        if (instance == null) {
            return;
        }
        sAdapterService = instance;
    }

    private static synchronized void clearAdapterService(AdapterService current) {
        if (sAdapterService == current) {
            sAdapterService = null;
        }
    }

    private AdapterProperties mAdapterProperties;
    private AdapterState mAdapterStateMachine;
    private BondStateMachine mBondStateMachine;
    private JniCallbacks mJniCallbacks;
    private RemoteDevices mRemoteDevices;

    /* TODO: Consider to remove the search API from this class, if changed to use call-back */
    private SdpManager mSdpManager = null;

    private boolean mNativeAvailable;
    private boolean mCleaningUp;
    private final HashMap<BluetoothDevice, ArrayList<IBluetoothMetadataListener>>
            mMetadataListeners = new HashMap<>();
    private final HashMap<String, Integer> mProfileServicesState = new HashMap<String, Integer>();
    //Only BluetoothManagerService should be registered
    private RemoteCallbackList<IBluetoothCallback> mCallbacks;
    private int mCurrentRequestId;
    private boolean mQuietmode = false;

    private AlarmManager mAlarmManager;
    private PendingIntent mPendingAlarm;
    private IBatteryStats mBatteryStats;
    private PowerManager mPowerManager;
    private PowerManager.WakeLock mWakeLock;
    private String mWakeLockName;
    private UserManager mUserManager;

    private PhonePolicy mPhonePolicy;
    private ActiveDeviceManager mActiveDeviceManager;
    private DatabaseManager mDatabaseManager;
    private SilenceDeviceManager mSilenceDeviceManager;
    private AppOpsManager mAppOps;

    private BluetoothSocketManagerBinder mBluetoothSocketManagerBinder;

    private BluetoothKeystoreService mBluetoothKeystoreService;
    private A2dpService mA2dpService;
    private A2dpSinkService mA2dpSinkService;
    private HeadsetService mHeadsetService;
    private HeadsetClientService mHeadsetClientService;
    private BluetoothMapService mMapService;
    private MapClientService mMapClientService;
    private HidDeviceService mHidDeviceService;
    private HidHostService mHidHostService;
    private PanService mPanService;
    private BluetoothPbapService mPbapService;
    private PbapClientService mPbapClientService;
    private HearingAidService mHearingAidService;
    private SapService mSapService;

    /**
     * Register a {@link ProfileService} with AdapterService.
     *
     * @param profile the service being added.
     */
    public void addProfile(ProfileService profile) {
        mHandler.obtainMessage(MESSAGE_PROFILE_SERVICE_REGISTERED, profile).sendToTarget();
    }

    /**
     * Unregister a ProfileService with AdapterService.
     *
     * @param profile the service being removed.
     */
    public void removeProfile(ProfileService profile) {
        mHandler.obtainMessage(MESSAGE_PROFILE_SERVICE_UNREGISTERED, profile).sendToTarget();
    }

    /**
     * Notify AdapterService that a ProfileService has started or stopped.
     *
     * @param profile the service being removed.
     * @param state {@link BluetoothAdapter#STATE_ON} or {@link BluetoothAdapter#STATE_OFF}
     */
    public void onProfileServiceStateChanged(ProfileService profile, int state) {
        if (state != BluetoothAdapter.STATE_ON && state != BluetoothAdapter.STATE_OFF) {
            throw new IllegalArgumentException(BluetoothAdapter.nameForState(state));
        }
        Message m = mHandler.obtainMessage(MESSAGE_PROFILE_SERVICE_STATE_CHANGED);
        m.obj = profile;
        m.arg1 = state;
        mHandler.sendMessage(m);
    }

    private static final int MESSAGE_PROFILE_SERVICE_STATE_CHANGED = 1;
    private static final int MESSAGE_PROFILE_SERVICE_REGISTERED = 2;
    private static final int MESSAGE_PROFILE_SERVICE_UNREGISTERED = 3;

    class AdapterServiceHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            verboseLog("handleMessage() - Message: " + msg.what);

            switch (msg.what) {
                case MESSAGE_PROFILE_SERVICE_STATE_CHANGED:
                    verboseLog("handleMessage() - MESSAGE_PROFILE_SERVICE_STATE_CHANGED");
                    processProfileServiceStateChanged((ProfileService) msg.obj, msg.arg1);
                    break;
                case MESSAGE_PROFILE_SERVICE_REGISTERED:
                    verboseLog("handleMessage() - MESSAGE_PROFILE_SERVICE_REGISTERED");
                    registerProfileService((ProfileService) msg.obj);
                    break;
                case MESSAGE_PROFILE_SERVICE_UNREGISTERED:
                    verboseLog("handleMessage() - MESSAGE_PROFILE_SERVICE_UNREGISTERED");
                    unregisterProfileService((ProfileService) msg.obj);
                    break;
            }
        }

        private void registerProfileService(ProfileService profile) {
            if (mRegisteredProfiles.contains(profile)) {
                Log.e(TAG, profile.getName() + " already registered.");
                return;
            }
            mRegisteredProfiles.add(profile);
        }

        private void unregisterProfileService(ProfileService profile) {
            if (!mRegisteredProfiles.contains(profile)) {
                Log.e(TAG, profile.getName() + " not registered (UNREGISTER).");
                return;
            }
            mRegisteredProfiles.remove(profile);
        }

        private void processProfileServiceStateChanged(ProfileService profile, int state) {
            switch (state) {
                case BluetoothAdapter.STATE_ON:
                    if (!mRegisteredProfiles.contains(profile)) {
                        Log.e(TAG, profile.getName() + " not registered (STATE_ON).");
                        return;
                    }
                    if (mRunningProfiles.contains(profile)) {
                        Log.e(TAG, profile.getName() + " already running.");
                        return;
                    }
                    mRunningProfiles.add(profile);
                    if (GattService.class.getSimpleName().equals(profile.getName())) {
                        enableNative();
                    } else if (mRegisteredProfiles.size() == Config.getSupportedProfiles().length
                            && mRegisteredProfiles.size() == mRunningProfiles.size()) {
                        mAdapterProperties.onBluetoothReady();
                        updateUuids();
                        setBluetoothClassFromConfig();
                        initProfileServices();
                        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_LOCAL_IO_CAPS);
                        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_LOCAL_IO_CAPS_BLE);
                        mAdapterStateMachine.sendMessage(AdapterState.BREDR_STARTED);
                    }
                    break;
                case BluetoothAdapter.STATE_OFF:
                    if (!mRegisteredProfiles.contains(profile)) {
                        Log.e(TAG, profile.getName() + " not registered (STATE_OFF).");
                        return;
                    }
                    if (!mRunningProfiles.contains(profile)) {
                        Log.e(TAG, profile.getName() + " not running.");
                        return;
                    }
                    mRunningProfiles.remove(profile);
                    // If only GATT is left, send BREDR_STOPPED.
                    if ((mRunningProfiles.size() == 1 && (GattService.class.getSimpleName()
                            .equals(mRunningProfiles.get(0).getName())))) {
                        mAdapterStateMachine.sendMessage(AdapterState.BREDR_STOPPED);
                    } else if (mRunningProfiles.size() == 0) {
                        disableNative();
                    }
                    break;
                default:
                    Log.e(TAG, "Unhandled profile state: " + state);
            }
        }
    }

    private final AdapterServiceHandler mHandler = new AdapterServiceHandler();

    private void updateInteropDatabase() {
        interopDatabaseClearNative();

        String interopString = Settings.Global.getString(getContentResolver(),
                Settings.Global.BLUETOOTH_INTEROPERABILITY_LIST);
        if (interopString == null) {
            return;
        }
        Log.d(TAG, "updateInteropDatabase: [" + interopString + "]");

        String[] entries = interopString.split(";");
        for (String entry : entries) {
            String[] tokens = entry.split(",");
            if (tokens.length != 2) {
                continue;
            }

            // Get feature
            int feature = 0;
            try {
                feature = Integer.parseInt(tokens[1]);
            } catch (NumberFormatException e) {
                Log.e(TAG, "updateInteropDatabase: Invalid feature '" + tokens[1] + "'");
                continue;
            }

            // Get address bytes and length
            int length = (tokens[0].length() + 1) / 3;
            if (length < 1 || length > 6) {
                Log.e(TAG, "updateInteropDatabase: Malformed address string '" + tokens[0] + "'");
                continue;
            }

            byte[] addr = new byte[6];
            int offset = 0;
            for (int i = 0; i < tokens[0].length(); ) {
                if (tokens[0].charAt(i) == ':') {
                    i += 1;
                } else {
                    try {
                        addr[offset++] = (byte) Integer.parseInt(tokens[0].substring(i, i + 2), 16);
                    } catch (NumberFormatException e) {
                        offset = 0;
                        break;
                    }
                    i += 2;
                }
            }

            // Check if address was parsed ok, otherwise, move on...
            if (offset == 0) {
                continue;
            }

            // Add entry
            interopDatabaseAddNative(feature, addr, length);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        debugLog("onCreate()");
        mRemoteDevices = new RemoteDevices(this, Looper.getMainLooper());
        mRemoteDevices.init();
        clearDiscoveringPackages();
        mBinder = new AdapterServiceBinder(this);
        mAdapterProperties = new AdapterProperties(this);
        mAdapterStateMachine = AdapterState.make(this);
        mJniCallbacks = new JniCallbacks(this, mAdapterProperties);
        mBluetoothKeystoreService = new BluetoothKeystoreService(isNiapMode());
        mBluetoothKeystoreService.start();
        int configCompareResult = mBluetoothKeystoreService.getCompareResult();

        // Android TV doesn't show consent dialogs for just works and encryption only le pairing
        boolean isAtvDevice = getApplicationContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_LEANBACK_ONLY);
        initNative(isGuest(), isNiapMode(), configCompareResult, isAtvDevice);
        mNativeAvailable = true;
        mCallbacks = new RemoteCallbackList<IBluetoothCallback>();
        mAppOps = getSystemService(AppOpsManager.class);
        //Load the name and address
        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_BDADDR);
        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_BDNAME);
        getAdapterPropertyNative(AbstractionLayer.BT_PROPERTY_CLASS_OF_DEVICE);
        mAlarmManager = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
        mPowerManager = (PowerManager) getSystemService(Context.POWER_SERVICE);
        mUserManager = (UserManager) getSystemService(Context.USER_SERVICE);
        mBatteryStats = IBatteryStats.Stub.asInterface(
                ServiceManager.getService(BatteryStats.SERVICE_NAME));

        mBluetoothKeystoreService.initJni();

        mSdpManager = SdpManager.init(this);
        registerReceiver(mAlarmBroadcastReceiver, new IntentFilter(ACTION_ALARM_WAKEUP));

        // Phone policy is specific to phone implementations and hence if a device wants to exclude
        // it out then it can be disabled by using the flag below.
        if (getResources().getBoolean(com.android.bluetooth.R.bool.enable_phone_policy)) {
            Log.i(TAG, "Phone policy enabled");
            mPhonePolicy = new PhonePolicy(this, new ServiceFactory());
            mPhonePolicy.start();
        } else {
            Log.i(TAG, "Phone policy disabled");
        }

        mActiveDeviceManager = new ActiveDeviceManager(this, new ServiceFactory());
        mActiveDeviceManager.start();

        mDatabaseManager = new DatabaseManager(this);
        mDatabaseManager.start(MetadataDatabase.createDatabase(this));

        mSilenceDeviceManager = new SilenceDeviceManager(this, new ServiceFactory(),
                Looper.getMainLooper());
        mSilenceDeviceManager.start();

        mBluetoothSocketManagerBinder = new BluetoothSocketManagerBinder(this);

        setAdapterService(this);

        invalidateBluetoothCaches();

        // First call to getSharedPreferences will result in a file read into
        // memory cache. Call it here asynchronously to avoid potential ANR
        // in the future
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                getSharedPreferences(PHONEBOOK_ACCESS_PERMISSION_PREFERENCE_FILE,
                        Context.MODE_PRIVATE);
                getSharedPreferences(MESSAGE_ACCESS_PERMISSION_PREFERENCE_FILE,
                        Context.MODE_PRIVATE);
                getSharedPreferences(SIM_ACCESS_PERMISSION_PREFERENCE_FILE, Context.MODE_PRIVATE);
                return null;
            }
        }.execute();

        try {
            int systemUiUid = getApplicationContext().getPackageManager().getPackageUid(
                    "com.android.systemui", PackageManager.MATCH_SYSTEM_ONLY);

            Utils.setSystemUiUid(systemUiUid);
        } catch (PackageManager.NameNotFoundException e) {
            // Some platforms, such as wearables do not have a system ui.
            Log.w(TAG, "Unable to resolve SystemUI's UID.", e);
        }

        IntentFilter filter = new IntentFilter(Intent.ACTION_USER_SWITCHED);
        getApplicationContext().registerReceiverForAllUsers(sUserSwitchedReceiver, filter, null, null);
        int fuid = ActivityManager.getCurrentUser();
        Utils.setForegroundUserId(fuid);
    }

    @Override
    public IBinder onBind(Intent intent) {
        debugLog("onBind()");
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        debugLog("onUnbind() - calling cleanup");
        cleanup();
        return super.onUnbind(intent);
    }

    @Override
    public void onDestroy() {
        debugLog("onDestroy()");
        if (!isMock()) {
            // TODO(b/27859763)
            Log.i(TAG, "Force exit to cleanup internal state in Bluetooth stack");
            System.exit(0);
        }
    }

    public static final BroadcastReceiver sUserSwitchedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_USER_SWITCHED.equals(intent.getAction())) {
                int fuid = intent.getIntExtra(Intent.EXTRA_USER_HANDLE, 0);
                Utils.setForegroundUserId(fuid);
            }
        }
    };

    void bringUpBle() {
        debugLog("bleOnProcessStart()");

        if (getResources().getBoolean(
                R.bool.config_bluetooth_reload_supported_profiles_when_enabled)) {
            Config.init(getApplicationContext());
        }

        // Reset |mRemoteDevices| whenever BLE is turned off then on
        // This is to replace the fact that |mRemoteDevices| was
        // reinitialized in previous code.
        //
        // TODO(apanicke): The reason is unclear but
        // I believe it is to clear the variable every time BLE was
        // turned off then on. The same effect can be achieved by
        // calling cleanup but this may not be necessary at all
        // We should figure out why this is needed later
        mRemoteDevices.reset();
        mAdapterProperties.init(mRemoteDevices);

        debugLog("bleOnProcessStart() - Make Bond State Machine");
        mBondStateMachine = BondStateMachine.make(this, mAdapterProperties, mRemoteDevices);

        mJniCallbacks.init(mBondStateMachine, mRemoteDevices);

        try {
            mBatteryStats.noteResetBleScan();
        } catch (RemoteException e) {
            Log.w(TAG, "RemoteException trying to send a reset to BatteryStats");
        }
        BluetoothStatsLog.write_non_chained(BluetoothStatsLog.BLE_SCAN_STATE_CHANGED, -1, null,
                BluetoothStatsLog.BLE_SCAN_STATE_CHANGED__STATE__RESET, false, false, false);

        //Start Gatt service
        setProfileServiceState(GattService.class, BluetoothAdapter.STATE_ON);
    }

    void bringDownBle() {
        stopGattProfileService();
    }

    void stateChangeCallback(int status) {
        if (status == AbstractionLayer.BT_STATE_OFF) {
            debugLog("stateChangeCallback: disableNative() completed");
            mAdapterStateMachine.sendMessage(AdapterState.BLE_STOPPED);
        } else if (status == AbstractionLayer.BT_STATE_ON) {
            mAdapterStateMachine.sendMessage(AdapterState.BLE_STARTED);
        } else {
            Log.e(TAG, "Incorrect status " + status + " in stateChangeCallback");
        }
    }

    /**
     * Sets the Bluetooth CoD value of the local adapter if there exists a config value for it.
     */
    void setBluetoothClassFromConfig() {
        int bluetoothClassConfig = retrieveBluetoothClassConfig();
        if (bluetoothClassConfig != 0) {
            mAdapterProperties.setBluetoothClass(new BluetoothClass(bluetoothClassConfig));
        }
    }

    private int retrieveBluetoothClassConfig() {
        return Settings.Global.getInt(
                getContentResolver(), Settings.Global.BLUETOOTH_CLASS_OF_DEVICE, 0);
    }

    void startProfileServices() {
        debugLog("startCoreServices()");
        Class[] supportedProfileServices = Config.getSupportedProfiles();
        if (supportedProfileServices.length == 1 && GattService.class.getSimpleName()
                .equals(supportedProfileServices[0].getSimpleName())) {
            mAdapterProperties.onBluetoothReady();
            updateUuids();
            setBluetoothClassFromConfig();
            mAdapterStateMachine.sendMessage(AdapterState.BREDR_STARTED);
        } else {
            setAllProfileServiceStates(supportedProfileServices, BluetoothAdapter.STATE_ON);
        }
    }

    void stopProfileServices() {
        // Make sure to stop classic background tasks now
        cancelDiscoveryNative();
        mAdapterProperties.setScanMode(AbstractionLayer.BT_SCAN_MODE_NONE);

        Class[] supportedProfileServices = Config.getSupportedProfiles();
        if (supportedProfileServices.length == 1 && (mRunningProfiles.size() == 1
                && GattService.class.getSimpleName().equals(mRunningProfiles.get(0).getName()))) {
            debugLog("stopProfileServices() - No profiles services to stop or already stopped.");
            mAdapterStateMachine.sendMessage(AdapterState.BREDR_STOPPED);
        } else {
            setAllProfileServiceStates(supportedProfileServices, BluetoothAdapter.STATE_OFF);
        }
    }

    private void stopGattProfileService() {
        mAdapterProperties.onBleDisable();
        if (mRunningProfiles.size() == 0) {
            debugLog("stopGattProfileService() - No profiles services to stop.");
            mAdapterStateMachine.sendMessage(AdapterState.BLE_STOPPED);
        }
        setProfileServiceState(GattService.class, BluetoothAdapter.STATE_OFF);
    }

    private void invalidateBluetoothGetStateCache() {
        BluetoothAdapter.invalidateBluetoothGetStateCache();
    }

    void updateAdapterState(int prevState, int newState) {
        mAdapterProperties.setState(newState);
        invalidateBluetoothGetStateCache();
        if (mCallbacks != null) {
            int n = mCallbacks.beginBroadcast();
            debugLog("updateAdapterState() - Broadcasting state " + BluetoothAdapter.nameForState(
                    newState) + " to " + n + " receivers.");
            for (int i = 0; i < n; i++) {
                try {
                    mCallbacks.getBroadcastItem(i).onBluetoothStateChange(prevState, newState);
                } catch (RemoteException e) {
                    debugLog("updateAdapterState() - Callback #" + i + " failed (" + e + ")");
                }
            }
            mCallbacks.finishBroadcast();
        }

        // Turn the Adapter all the way off if we are disabling and the snoop log setting changed.
        if (newState == BluetoothAdapter.STATE_BLE_TURNING_ON) {
            mSnoopLogSettingAtEnable =
                    SystemProperties.get(BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY, "empty");
            mDefaultSnoopLogSettingAtEnable =
                    Settings.Global.getString(getContentResolver(),
                            Settings.Global.BLUETOOTH_BTSNOOP_DEFAULT_MODE);
            SystemProperties.set(BLUETOOTH_BTSNOOP_DEFAULT_MODE_PROPERTY,
                    mDefaultSnoopLogSettingAtEnable);
        } else if (newState == BluetoothAdapter.STATE_BLE_ON
                   && prevState != BluetoothAdapter.STATE_OFF) {
            String snoopLogSetting =
                    SystemProperties.get(BLUETOOTH_BTSNOOP_LOG_MODE_PROPERTY, "empty");
            String snoopDefaultModeSetting =
                    Settings.Global.getString(getContentResolver(),
                            Settings.Global.BLUETOOTH_BTSNOOP_DEFAULT_MODE);

            if (!TextUtils.equals(mSnoopLogSettingAtEnable, snoopLogSetting)
                    || !TextUtils.equals(mDefaultSnoopLogSettingAtEnable,
                            snoopDefaultModeSetting)) {
                mAdapterStateMachine.sendMessage(AdapterState.BLE_TURN_OFF);
            }
        }
    }

    void cleanup() {
        debugLog("cleanup()");
        if (mCleaningUp) {
            errorLog("cleanup() - Service already starting to cleanup, ignoring request...");
            return;
        }

        clearAdapterService(this);

        mCleaningUp = true;
        invalidateBluetoothCaches();

        unregisterReceiver(mAlarmBroadcastReceiver);

        if (mPendingAlarm != null) {
            mAlarmManager.cancel(mPendingAlarm);
            mPendingAlarm = null;
        }

        // This wake lock release may also be called concurrently by
        // {@link #releaseWakeLock(String lockName)}, so a synchronization is needed here.
        synchronized (this) {
            if (mWakeLock != null) {
                if (mWakeLock.isHeld()) {
                    mWakeLock.release();
                }
                mWakeLock = null;
            }
        }

        if (mDatabaseManager != null) {
            mDatabaseManager.cleanup();
        }

        if (mAdapterStateMachine != null) {
            mAdapterStateMachine.doQuit();
        }

        if (mBondStateMachine != null) {
            mBondStateMachine.doQuit();
        }

        if (mRemoteDevices != null) {
            mRemoteDevices.cleanup();
        }

        if (mSdpManager != null) {
            mSdpManager.cleanup();
            mSdpManager = null;
        }

        if (mBluetoothKeystoreService != null) {
            mBluetoothKeystoreService.cleanup();
        }

        if (mNativeAvailable) {
            debugLog("cleanup() - Cleaning up adapter native");
            cleanupNative();
            mNativeAvailable = false;
        }

        if (mAdapterProperties != null) {
            mAdapterProperties.cleanup();
        }

        if (mJniCallbacks != null) {
            mJniCallbacks.cleanup();
        }

        if (mPhonePolicy != null) {
            mPhonePolicy.cleanup();
        }

        if (mSilenceDeviceManager != null) {
            mSilenceDeviceManager.cleanup();
        }

        if (mActiveDeviceManager != null) {
            mActiveDeviceManager.cleanup();
        }

        if (mProfileServicesState != null) {
            mProfileServicesState.clear();
        }

        if (mBluetoothSocketManagerBinder != null) {
            mBluetoothSocketManagerBinder.cleanUp();
            mBluetoothSocketManagerBinder = null;
        }

        if (mBinder != null) {
            mBinder.cleanup();
            mBinder = null;  //Do not remove. Otherwise Binder leak!
        }

        if (mCallbacks != null) {
            mCallbacks.kill();
        }
    }

    private void invalidateBluetoothCaches() {
        BluetoothAdapter.invalidateGetProfileConnectionStateCache();
        BluetoothAdapter.invalidateIsOffloadedFilteringSupportedCache();
        BluetoothDevice.invalidateBluetoothGetBondStateCache();
        BluetoothAdapter.invalidateBluetoothGetStateCache();
    }

    private void setProfileServiceState(Class service, int state) {
        Intent intent = new Intent(this, service);
        intent.putExtra(EXTRA_ACTION, ACTION_SERVICE_STATE_CHANGED);
        intent.putExtra(BluetoothAdapter.EXTRA_STATE, state);
        startService(intent);
    }

    private void setAllProfileServiceStates(Class[] services, int state) {
        for (Class service : services) {
            if (GattService.class.getSimpleName().equals(service.getSimpleName())) {
                continue;
            }
            setProfileServiceState(service, state);
        }
    }

    /**
     * Verifies whether the profile is supported by the local bluetooth adapter by checking a
     * bitmask of its supported profiles
     *
     * @param remoteDeviceUuids is an array of all supported profiles by the remote device
     * @param localDeviceUuids  is an array of all supported profiles by the local device
     * @param profile           is the profile we are checking for support
     * @param device            is the remote device we wish to connect to
     * @return true if the profile is supported by both the local and remote device, false otherwise
     */
    private boolean isSupported(ParcelUuid[] localDeviceUuids, ParcelUuid[] remoteDeviceUuids,
            int profile, BluetoothDevice device) {
        if (remoteDeviceUuids == null || remoteDeviceUuids.length == 0) {
            Log.e(TAG, "isSupported: Remote Device Uuids Empty");
        }

        if (profile == BluetoothProfile.HEADSET) {
            return (ArrayUtils.contains(localDeviceUuids, BluetoothUuid.HSP_AG)
                    && ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.HSP))
                    || (ArrayUtils.contains(localDeviceUuids, BluetoothUuid.HFP_AG)
                    && ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.HFP));
        }
        if (profile == BluetoothProfile.HEADSET_CLIENT) {
            return ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.HFP_AG)
                    && ArrayUtils.contains(localDeviceUuids, BluetoothUuid.HFP);
        }
        if (profile == BluetoothProfile.A2DP) {
            return ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.ADV_AUDIO_DIST)
                    || ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.A2DP_SINK);
        }
        if (profile == BluetoothProfile.A2DP_SINK) {
            return ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.ADV_AUDIO_DIST)
                    || ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.A2DP_SOURCE);
        }
        if (profile == BluetoothProfile.OPP) {
            return ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.OBEX_OBJECT_PUSH);
        }
        if (profile == BluetoothProfile.HID_HOST) {
            return ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.HID)
                    || ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.HOGP);
        }
        if (profile == BluetoothProfile.HID_DEVICE) {
            return mHidDeviceService.getConnectionState(device)
                    == BluetoothProfile.STATE_DISCONNECTED;
        }
        if (profile == BluetoothProfile.PAN) {
            return ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.NAP);
        }
        if (profile == BluetoothProfile.MAP) {
            return mMapService.getConnectionState(device) == BluetoothProfile.STATE_CONNECTED;
        }
        if (profile == BluetoothProfile.PBAP) {
            return mPbapService.getConnectionState(device) == BluetoothProfile.STATE_CONNECTED;
        }
        if (profile == BluetoothProfile.MAP_CLIENT) {
            return true;
        }
        if (profile == BluetoothProfile.PBAP_CLIENT) {
            return ArrayUtils.contains(localDeviceUuids, BluetoothUuid.PBAP_PCE)
                    && ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.PBAP_PSE);
        }
        if (profile == BluetoothProfile.HEARING_AID) {
            return ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.HEARING_AID);
        }
        if (profile == BluetoothProfile.SAP) {
            return ArrayUtils.contains(remoteDeviceUuids, BluetoothUuid.SAP);
        }

        Log.e(TAG, "isSupported: Unexpected profile passed in to function: " + profile);
        return false;
    }

    /**
     * Checks if any profile is enabled for the given device
     *
     * @param device is the device for which we are checking if any profiles are enabled
     * @return true if any profile is enabled, false otherwise
     */
    private boolean isAnyProfileEnabled(BluetoothDevice device) {

        if (mA2dpService != null && mA2dpService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return true;
        }
        if (mA2dpSinkService != null && mA2dpSinkService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return true;
        }
        if (mHeadsetService != null && mHeadsetService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return true;
        }
        if (mHeadsetClientService != null && mHeadsetClientService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return true;
        }
        if (mMapClientService != null && mMapClientService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return true;
        }
        if (mHidHostService != null && mHidHostService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return true;
        }
        if (mPanService != null && mPanService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return true;
        }
        if (mPbapClientService != null && mPbapClientService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return true;
        }
        if (mHearingAidService != null && mHearingAidService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return true;
        }

        return false;
    }

    /**
     * Connects only available profiles
     * (those with {@link BluetoothProfile.CONNECTION_POLICY_ALLOWED})
     *
     * @param device is the device with which we are connecting the profiles
     * @return true
     */
    private boolean connectEnabledProfiles(BluetoothDevice device) {
        ParcelUuid[] remoteDeviceUuids = getRemoteUuids(device);
        ParcelUuid[] localDeviceUuids = mAdapterProperties.getUuids();

        if (mA2dpService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.A2DP, device) && mA2dpService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.i(TAG, "connectEnabledProfiles: Connecting A2dp");
            mA2dpService.connect(device);
        }
        if (mA2dpSinkService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.A2DP_SINK, device) && mA2dpSinkService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.i(TAG, "connectEnabledProfiles: Connecting A2dp Sink");
            mA2dpSinkService.connect(device);
        }
        if (mHeadsetService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.HEADSET, device) && mHeadsetService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.i(TAG, "connectEnabledProfiles: Connecting Headset Profile");
            mHeadsetService.connect(device);
        }
        if (mHeadsetClientService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.HEADSET_CLIENT, device)
                && mHeadsetClientService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.i(TAG, "connectEnabledProfiles: Connecting HFP");
            mHeadsetClientService.connect(device);
        }
        if (mMapClientService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.MAP_CLIENT, device)
                && mMapClientService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.i(TAG, "connectEnabledProfiles: Connecting MAP");
            mMapClientService.connect(device);
        }
        if (mHidHostService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.HID_HOST, device) && mHidHostService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.i(TAG, "connectEnabledProfiles: Connecting Hid Host Profile");
            mHidHostService.connect(device);
        }
        if (mPanService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.PAN, device) && mPanService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.i(TAG, "connectEnabledProfiles: Connecting Pan Profile");
            mPanService.connect(device);
        }
        if (mPbapClientService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.PBAP_CLIENT, device)
                && mPbapClientService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.i(TAG, "connectEnabledProfiles: Connecting Pbap");
            mPbapClientService.connect(device);
        }
        if (mHearingAidService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.HEARING_AID, device)
                && mHearingAidService.getConnectionPolicy(device)
                > BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.i(TAG, "connectEnabledProfiles: Connecting Hearing Aid Profile");
            mHearingAidService.connect(device);
        }

        return true;
    }

    /**
     * Verifies that all bluetooth profile services are running
     *
     * @return true if all bluetooth profile services running, false otherwise
     */
    private boolean profileServicesRunning() {
        if (mRegisteredProfiles.size() == Config.getSupportedProfiles().length
                && mRegisteredProfiles.size() == mRunningProfiles.size()) {
            return true;
        }

        Log.e(TAG, "profileServicesRunning: One or more supported services not running");
        return false;
    }

    /**
     * Initializes all the profile services fields
     */
    private void initProfileServices() {
        Log.i(TAG, "initProfileServices: Initializing all bluetooth profile services");
        mA2dpService = A2dpService.getA2dpService();
        mA2dpSinkService = A2dpSinkService.getA2dpSinkService();
        mHeadsetService = HeadsetService.getHeadsetService();
        mHeadsetClientService = HeadsetClientService.getHeadsetClientService();
        mMapService = BluetoothMapService.getBluetoothMapService();
        mMapClientService = MapClientService.getMapClientService();
        mHidDeviceService = HidDeviceService.getHidDeviceService();
        mHidHostService = HidHostService.getHidHostService();
        mPanService = PanService.getPanService();
        mPbapService = BluetoothPbapService.getBluetoothPbapService();
        mPbapClientService = PbapClientService.getPbapClientService();
        mHearingAidService = HearingAidService.getHearingAidService();
        mSapService = SapService.getSapService();
    }

    private boolean isAvailable() {
        return !mCleaningUp;
    }

    /**
     * Handlers for incoming service calls
     */
    private AdapterServiceBinder mBinder;

    /**
     * The Binder implementation must be declared to be a static class, with
     * the AdapterService instance passed in the constructor. Furthermore,
     * when the AdapterService shuts down, the reference to the AdapterService
     * must be explicitly removed.
     *
     * Otherwise, a memory leak can occur from repeated starting/stopping the
     * service...Please refer to android.os.Binder for further details on
     * why an inner instance class should be avoided.
     *
     */
    @VisibleForTesting
    public static class AdapterServiceBinder extends IBluetooth.Stub {
        private AdapterService mService;

        AdapterServiceBinder(AdapterService svc) {
            mService = svc;
            mService.invalidateBluetoothGetStateCache();
            BluetoothAdapter.getDefaultAdapter().disableBluetoothGetStateCache();
        }

        public void cleanup() {
            mService = null;
        }

        public AdapterService getService() {
            if (mService != null && mService.isAvailable()) {
                return mService;
            }
            return null;
        }

        @Override
        public int getState() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return BluetoothAdapter.STATE_OFF;
            }

            enforceBluetoothPermission(service);

            return service.getState();
        }

        @Override
        public boolean enable(boolean quietMode) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "enable")) {
                return false;
            }

            enforceBluetoothAdminPermission(service);

            return service.enable(quietMode);
        }

        @Override
        public boolean disable() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "disable")) {
                return false;
            }

            enforceBluetoothAdminPermission(service);

            return service.disable();
        }

        @Override
        public String getAddress() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "getAddress")) {
                return null;
            }

            enforceBluetoothPermission(service);
            enforceLocalMacAddressPermission(service);

            return Utils.getAddressStringFromByte(service.mAdapterProperties.getAddress());
        }

        @Override
        public ParcelUuid[] getUuids() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getUuids")) {
                return new ParcelUuid[0];
            }

            enforceBluetoothPermission(service);

            return service.mAdapterProperties.getUuids();
        }

        @Override
        public String getName() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getName")) {
                return null;
            }

            enforceBluetoothPermission(service);

            return service.getName();
        }

        @Override
        public boolean setName(String name) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setName")) {
                return false;
            }

            enforceBluetoothAdminPermission(service);

            return service.mAdapterProperties.setName(name);
        }

        @Override
        public BluetoothClass getBluetoothClass() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getBluetoothClass")) {
                return null;
            }

            enforceBluetoothAdminPermission(service);

            return service.mAdapterProperties.getBluetoothClass();
        }

        @Override
        public boolean setBluetoothClass(BluetoothClass bluetoothClass) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setBluetoothClass")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            if (!service.mAdapterProperties.setBluetoothClass(bluetoothClass)) {
              return false;
            }

            return Settings.Global.putInt(
                    service.getContentResolver(),
                    Settings.Global.BLUETOOTH_CLASS_OF_DEVICE,
                    bluetoothClass.getClassOfDevice());
        }

        @Override
        public int getIoCapability() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getIoCapability")) {
                return BluetoothAdapter.IO_CAPABILITY_UNKNOWN;
            }

            enforceBluetoothAdminPermission(service);

            return service.mAdapterProperties.getIoCapability();
        }

        @Override
        public boolean setIoCapability(int capability) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setIoCapability")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            if (!isValidIoCapability(capability)) {
              return false;
            }

            return service.mAdapterProperties.setIoCapability(capability);
        }

        @Override
        public int getLeIoCapability() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getLeIoCapability")) {
                return BluetoothAdapter.IO_CAPABILITY_UNKNOWN;
            }

            enforceBluetoothAdminPermission(service);

            return service.mAdapterProperties.getLeIoCapability();
        }

        @Override
        public boolean setLeIoCapability(int capability) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setLeIoCapability")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            if (!isValidIoCapability(capability)) {
              return false;
            }

            return service.mAdapterProperties.setLeIoCapability(capability);
        }

        @Override
        public int getScanMode() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "getScanMode")) {
                return BluetoothAdapter.SCAN_MODE_NONE;
            }

            enforceBluetoothPermission(service);

            return service.mAdapterProperties.getScanMode();
        }

        @Override
        public boolean setScanMode(int mode, int duration) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setScanMode")) {
                return false;
            }

            enforceBluetoothPermission(service);

            service.mAdapterProperties.setDiscoverableTimeout(duration);
            return service.mAdapterProperties.setScanMode(convertScanModeToHal(mode));
        }

        @Override
        public int getDiscoverableTimeout() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getDiscoverableTimeout")) {
                return 0;
            }

            enforceBluetoothPermission(service);

            return service.mAdapterProperties.getDiscoverableTimeout();
        }

        @Override
        public boolean setDiscoverableTimeout(int timeout) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setDiscoverableTimeout")) {
                return false;
            }

            enforceBluetoothPermission(service);

            return service.mAdapterProperties.setDiscoverableTimeout(timeout);
        }

        @Override
        public boolean startDiscovery(String callingPackage, String callingFeatureId) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "startDiscovery")) {
                return false;
            }

            enforceBluetoothAdminPermission(service);

            return service.startDiscovery(callingPackage, callingFeatureId);
        }

        @Override
        public boolean cancelDiscovery() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "cancelDiscovery")) {
                return false;
            }

            enforceBluetoothAdminPermission(service);

            service.debugLog("cancelDiscovery");
            return service.cancelDiscoveryNative();
        }

        @Override
        public boolean isDiscovering() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "isDiscovering")) {
                return false;
            }

            enforceBluetoothPermission(service);

            return service.mAdapterProperties.isDiscovering();
        }

        @Override
        public long getDiscoveryEndMillis() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getDiscoveryEndMillis")) {
                return -1;
            }

            enforceBluetoothPrivilegedPermission(service);

            return service.mAdapterProperties.discoveryEndMillis();
        }

        @Override
        public List<BluetoothDevice> getMostRecentlyConnectedDevices() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return new ArrayList<>();
            }

            enforceBluetoothAdminPermission(service);

            return service.mDatabaseManager.getMostRecentlyConnectedDevices();
        }

        @Override
        public BluetoothDevice[] getBondedDevices() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return new BluetoothDevice[0];
            }

            enforceBluetoothPermission(service);

            return service.getBondedDevices();
        }

        @Override
        public int getAdapterConnectionState() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return BluetoothAdapter.STATE_DISCONNECTED;
            }

            enforceBluetoothPermission(service);

            return service.mAdapterProperties.getConnectionState();
        }

        /**
         * This method has an associated binder cache.  The invalidation
         * methods must be changed if the logic behind this method changes.
         */
        @Override
        public int getProfileConnectionState(int profile) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "getProfileConnectionState")) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }

            enforceBluetoothPermission(service);

            return service.mAdapterProperties.getProfileConnectionState(profile);
        }

        @Override
        public boolean createBond(BluetoothDevice device, int transport, OobData oobData) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "createBond")) {
                return false;
            }

            enforceBluetoothAdminPermission(service);

            return service.createBond(device, transport, oobData);
        }

        @Override
        public boolean cancelBondProcess(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "cancelBondProcess")) {
                return false;
            }

            enforceBluetoothAdminPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            if (deviceProp != null) {
                deviceProp.setBondingInitiatedLocally(false);
            }

            return service.cancelBondNative(addressToBytes(device.getAddress()));
        }

        @Override
        public boolean removeBond(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "removeBond")) {
                return false;
            }

            enforceBluetoothAdminPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            if (deviceProp == null || deviceProp.getBondState() != BluetoothDevice.BOND_BONDED) {
                return false;
            }
            deviceProp.setBondingInitiatedLocally(false);

            Message msg = service.mBondStateMachine.obtainMessage(BondStateMachine.REMOVE_BOND);
            msg.obj = device;
            service.mBondStateMachine.sendMessage(msg);
            return true;
        }

        @Override
        public int getBondState(BluetoothDevice device) {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return BluetoothDevice.BOND_NONE;
            }

            enforceBluetoothPermission(service);

            return service.getBondState(device);
        }

        @Override
        public boolean isBondingInitiatedLocally(BluetoothDevice device) {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            return deviceProp != null && deviceProp.isBondingInitiatedLocally();
        }

        @Override
        public long getSupportedProfiles() {
            AdapterService service = getService();
            if (service == null) {
                return 0;
            }
            return Config.getSupportedProfilesBitMask();
        }

        @Override
        public int getConnectionState(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null) {
                return 0;
            }

            enforceBluetoothPermission(service);

            return service.getConnectionState(device);
        }

        @Override
        public boolean removeActiveDevice(@ActiveDeviceUse int profiles) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "removeActiveDevice() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setActiveDevice(null, profiles);
        }

        @Override
        public boolean setActiveDevice(BluetoothDevice device, @ActiveDeviceUse int profiles) {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "setActiveDevice() - Not allowed for non-active user");
                return false;
            }

            AdapterService service = getService();
            if (service == null) {
                return false;
            }
            return service.setActiveDevice(device, profiles);
        }

        @Override
        public boolean connectAllEnabledProfiles(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "connectAllEnabledProfiles")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            return service.connectAllEnabledProfiles(device);
        }

        @Override
        public boolean disconnectAllEnabledProfiles(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null | !callerIsSystemOrActiveUser(TAG, "disconnectAllEnabledProfiles")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            return service.disconnectAllEnabledProfiles(device);
        }

        @Override
        public String getRemoteName(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "getRemoteName")) {
                return null;
            }

            enforceBluetoothPermission(service);

            return service.getRemoteName(device);
        }

        @Override
        public int getRemoteType(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "getRemoteType")) {
                return BluetoothDevice.DEVICE_TYPE_UNKNOWN;
            }

            enforceBluetoothPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            return deviceProp != null ? deviceProp.getDeviceType() : BluetoothDevice.DEVICE_TYPE_UNKNOWN;
        }

        @Override
        public String getRemoteAlias(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "getRemoteAlias")) {
                return null;
            }

            enforceBluetoothPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            return deviceProp != null ? deviceProp.getAlias() : null;
        }

        @Override
        public boolean setRemoteAlias(BluetoothDevice device, String name) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setRemoteAlias")) {
                return false;
            }

            enforceBluetoothPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            if (deviceProp == null) {
                return false;
            }
            deviceProp.setAlias(device, name);
            return true;
        }

        @Override
        public int getRemoteClass(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "getRemoteClass")) {
                return 0;
            }

            enforceBluetoothPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            return deviceProp != null ? deviceProp.getBluetoothClass() : 0;
        }

        @Override
        public ParcelUuid[] getRemoteUuids(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "getRemoteUuids")) {
                return new ParcelUuid[0];
            }

            enforceBluetoothPermission(service);

            return service.getRemoteUuids(device);
        }

        @Override
        public boolean fetchRemoteUuids(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveOrManagedUser(service, TAG, "fetchRemoteUuids")) {
                return false;
            }

            enforceBluetoothPermission(service);

            service.mRemoteDevices.fetchUuids(device);
            return true;
        }


        @Override
        public boolean setPin(BluetoothDevice device, boolean accept, int len, byte[] pinCode) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setPin")) {
                return false;
            }

            enforceBluetoothAdminPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            // Only allow setting a pin in bonding state, or bonded state in case of security upgrade.
            if (deviceProp == null || !deviceProp.isBondingOrBonded()) {
                return false;
            }
            if (pinCode.length != len) {
                android.util.EventLog.writeEvent(0x534e4554, "139287605", -1,
                        "PIN code length mismatch");
                return false;
            }
            service.logUserBondResponse(device, accept, BluetoothProtoEnums.BOND_SUB_STATE_LOCAL_PIN_REPLIED);
            return service.pinReplyNative(addressToBytes(device.getAddress()), accept, len, pinCode);
        }

        @Override
        public boolean setPasskey(BluetoothDevice device, boolean accept, int len, byte[] passkey) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setPasskey")) {
                return false;
            }

            enforceBluetoothPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            if (deviceProp == null || !deviceProp.isBonding()) {
                return false;
            }
            if (passkey.length != len) {
                android.util.EventLog.writeEvent(0x534e4554, "139287605", -1,
                        "Passkey length mismatch");
                return false;
            }
            service.logUserBondResponse(device, accept, BluetoothProtoEnums.BOND_SUB_STATE_LOCAL_SSP_REPLIED);
            return service.sspReplyNative(
                    addressToBytes(device.getAddress()),
                    AbstractionLayer.BT_SSP_VARIANT_PASSKEY_ENTRY,
                    accept,
                    Utils.byteArrayToInt(passkey));
        }

        @Override
        public boolean setPairingConfirmation(BluetoothDevice device, boolean accept) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setPairingConfirmation")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            if (deviceProp == null || !deviceProp.isBonding()) {
                return false;
            }
            service.logUserBondResponse(device, accept, BluetoothProtoEnums.BOND_SUB_STATE_LOCAL_SSP_REPLIED);
            return service.sspReplyNative(
                    addressToBytes(device.getAddress()),
                    AbstractionLayer.BT_SSP_VARIANT_PASSKEY_CONFIRMATION,
                    accept,
                    0);
        }

        @Override
        public boolean getSilenceMode(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getSilenceMode")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            return service.mSilenceDeviceManager.getSilenceMode(device);
        }


        @Override
        public boolean setSilenceMode(BluetoothDevice device, boolean silence) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setSilenceMode")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            service.mSilenceDeviceManager.setSilenceMode(device, silence);
            return true;
        }

        @Override
        public int getPhonebookAccessPermission(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getPhonebookAccessPermission")) {
                return BluetoothDevice.ACCESS_UNKNOWN;
            }

            enforceBluetoothPermission(service);

            return service.getDeviceAccessFromPrefs(device, PHONEBOOK_ACCESS_PERMISSION_PREFERENCE_FILE);
        }

        @Override
        public boolean setPhonebookAccessPermission(BluetoothDevice device, int value) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setPhonebookAccessPermission")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            service.setPhonebookAccessPermission(device, value);
            return true;
        }

        @Override
        public int getMessageAccessPermission(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getMessageAccessPermission")) {
                return BluetoothDevice.ACCESS_UNKNOWN;
            }

            enforceBluetoothPermission(service);

            return service.getDeviceAccessFromPrefs(device, MESSAGE_ACCESS_PERMISSION_PREFERENCE_FILE);
        }

        @Override
        public boolean setMessageAccessPermission(BluetoothDevice device, int value) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setMessageAccessPermission")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            service.setMessageAccessPermission(device, value);
            return true;
        }

        @Override
        public int getSimAccessPermission(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getSimAccessPermission")) {
                return BluetoothDevice.ACCESS_UNKNOWN;
            }

            enforceBluetoothPermission(service);

            return service.getDeviceAccessFromPrefs(device, SIM_ACCESS_PERMISSION_PREFERENCE_FILE);
        }

        @Override
        public boolean setSimAccessPermission(BluetoothDevice device, int value) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setSimAccessPermission")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            service.setSimAccessPermission(device, value);
            return true;
        }

        @Override
        public IBluetoothSocketManager getSocketManager() {
            AdapterService service = getService();
            if (service == null) {
                return null;
            }

            return IBluetoothSocketManager.Stub.asInterface(service.mBluetoothSocketManagerBinder);
        }

        @Override
        public boolean sdpSearch(BluetoothDevice device, ParcelUuid uuid) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "sdpSearch")) {
                return false;
            }

            enforceBluetoothPermission(service);

            if (service.mSdpManager == null) {
                return false;
            }
            service.mSdpManager.sdpSearch(device, uuid);
            return true;
        }

        @Override
        public int getBatteryLevel(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getBatteryLevel")) {
                return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
            }

            enforceBluetoothPermission(service);

            DeviceProperties deviceProp = service.mRemoteDevices.getDeviceProperties(device);
            if (deviceProp == null) {
                return BluetoothDevice.BATTERY_LEVEL_UNKNOWN;
            }
            return deviceProp.getBatteryLevel();
        }

        @Override
        public int getMaxConnectedAudioDevices() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return AdapterProperties.MAX_CONNECTED_AUDIO_DEVICES_LOWER_BOND;
            }

            enforceBluetoothPermission(service);

            return service.getMaxConnectedAudioDevices();
        }

        //@Override
        public boolean isA2dpOffloadEnabled() {
            // don't check caller, may be called from system UI
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPermission(service);

            return service.isA2dpOffloadEnabled();
        }

        @Override
        public boolean factoryReset() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            if (service.mDatabaseManager != null) {
                service.mDatabaseManager.factoryReset();
            }

            if (service.mBluetoothKeystoreService != null) {
                service.mBluetoothKeystoreService.factoryReset();
            }

            return service.factoryResetNative();
        }

        @Override
        public void registerCallback(IBluetoothCallback callback) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "registerCallback")) {
                return;
            }

            enforceBluetoothPrivilegedPermission(service);

            service.mCallbacks.register(callback);
        }

        @Override
        public void unregisterCallback(IBluetoothCallback callback) {
            AdapterService service = getService();
            if (service == null || service.mCallbacks == null
                    || !callerIsSystemOrActiveUser(TAG, "unregisterCallback")) {
                return;
            }

            enforceBluetoothPrivilegedPermission(service);

            service.mCallbacks.unregister(callback);
        }

        @Override
        public boolean isMultiAdvertisementSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPermission(service);

            int val = service.mAdapterProperties.getNumOfAdvertisementInstancesSupported();
            return val >= MIN_ADVT_INSTANCES_FOR_MA;
        }

        /**
         * This method has an associated binder cache.  The invalidation
         * methods must be changed if the logic behind this method changes.
         */
        @Override
        public boolean isOffloadedFilteringSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPermission(service);

            int val = service.getNumOfOffloadedScanFilterSupported();
            return val >= MIN_OFFLOADED_FILTERS;
        }

        @Override
        public boolean isOffloadedScanBatchingSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPermission(service);

            int val = service.getOffloadedScanResultStorage();
            return val >= MIN_OFFLOADED_SCAN_STORAGE_BYTES;
        }

        @Override
        public boolean isLe2MPhySupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPermission(service);

            return service.isLe2MPhySupported();
        }

        @Override
        public boolean isLeCodedPhySupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPermission(service);

            return service.isLeCodedPhySupported();
        }

        @Override
        public boolean isLeExtendedAdvertisingSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPermission(service);

            return service.isLeExtendedAdvertisingSupported();
        }

        @Override
        public boolean isLePeriodicAdvertisingSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPermission(service);

            return service.isLePeriodicAdvertisingSupported();
        }

        @Override
        public int getLeMaximumAdvertisingDataLength() {
            AdapterService service = getService();
            if (service == null) {
                return 0;
            }

            enforceBluetoothPermission(service);

            return service.getLeMaximumAdvertisingDataLength();
        }

        @Override
        public boolean isActivityAndEnergyReportingSupported() {
            AdapterService service = getService();
            if (service == null) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            return service.mAdapterProperties.isActivityAndEnergyReportingSupported();
        }

        @Override
        public BluetoothActivityEnergyInfo reportActivityInfo() {
            AdapterService service = getService();
            if (service == null) {
                return null;
            }

            enforceBluetoothPrivilegedPermission(service);

            return service.reportActivityInfo();
        }

        @Override
        public boolean registerMetadataListener(IBluetoothMetadataListener listener,
                BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "registerMetadataListener")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            if (service.mMetadataListeners == null) {
                return false;
            }
            ArrayList<IBluetoothMetadataListener> list = service.mMetadataListeners.get(device);
            if (list == null) {
                list = new ArrayList<>();
            } else if (list.contains(listener)) {
                // The device is already registered with this listener
                return true;
            }
            list.add(listener);
            service.mMetadataListeners.put(device, list);
            return true;
        }

        @Override
        public boolean unregisterMetadataListener(BluetoothDevice device) {
            AdapterService service = getService();
            if (service == null
                    || !callerIsSystemOrActiveUser(TAG, "unregisterMetadataListener")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            if (service.mMetadataListeners == null) {
                return false;
            }
            if (service.mMetadataListeners.containsKey(device)) {
                service.mMetadataListeners.remove(device);
            }
            return true;
        }

        @Override
        public boolean setMetadata(BluetoothDevice device, int key, byte[] value) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "setMetadata")) {
                return false;
            }

            enforceBluetoothPrivilegedPermission(service);

            if (value.length > BluetoothDevice.METADATA_MAX_LENGTH) {
                return false;
            }
            return service.mDatabaseManager.setCustomMeta(device, key, value);
        }

        @Override
        public byte[] getMetadata(BluetoothDevice device, int key) {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "getMetadata")) {
                return null;
            }

            enforceBluetoothPrivilegedPermission(service);

            return service.mDatabaseManager.getCustomMeta(device, key);
        }

        @Override
        public void requestActivityInfo(ResultReceiver result) {
            Bundle bundle = new Bundle();
            bundle.putParcelable(BatteryStats.RESULT_RECEIVER_CONTROLLER_KEY, reportActivityInfo());
            result.send(0, bundle);
        }

        @Override
        public void onLeServiceUp() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "onLeServiceUp")) {
                return;
            }

            enforceBluetoothPrivilegedPermission(service);

            service.mAdapterStateMachine.sendMessage(AdapterState.USER_TURN_ON);
        }

        @Override
        public void onBrEdrDown() {
            AdapterService service = getService();
            if (service == null || !callerIsSystemOrActiveUser(TAG, "onBrEdrDown")) {
                return;
            }

            enforceBluetoothPrivilegedPermission(service);

            service.mAdapterStateMachine.sendMessage(AdapterState.BLE_TURN_OFF);
        }

        @Override
        public void dump(FileDescriptor fd, String[] args) {
            PrintWriter writer = new PrintWriter(new FileOutputStream(fd));
            AdapterService service = getService();
            if (service == null) {
                return;
            }

            enforceDumpPermission(service);

            service.dump(fd, writer, args);
            writer.close();
        }
    }

    // ----API Methods--------

    public int getState() {
        if (mAdapterProperties != null) {
            return mAdapterProperties.getState();
        }
        return BluetoothAdapter.STATE_OFF;
    }

    public synchronized boolean enable(boolean quietMode) {
        // Enforce the user restriction for disallowing Bluetooth if it was set.
        if (mUserManager.hasUserRestriction(UserManager.DISALLOW_BLUETOOTH, UserHandle.SYSTEM)) {
            debugLog("enable() called when Bluetooth was disallowed");
            return false;
        }

        debugLog("enable() - Enable called with quiet mode status =  " + quietMode);
        mQuietmode = quietMode;
        mAdapterStateMachine.sendMessage(AdapterState.BLE_TURN_ON);
        return true;
    }

    boolean disable() {
        debugLog("disable() called with mRunningProfiles.size() = " + mRunningProfiles.size());
        mAdapterStateMachine.sendMessage(AdapterState.USER_TURN_OFF);
        return true;
    }

    public String getName() {
        return mAdapterProperties.getName();
    }

    private static boolean isValidIoCapability(int capability) {
        if (capability < 0 || capability >= BluetoothAdapter.IO_CAPABILITY_MAX) {
            Log.e(TAG, "Invalid IO capability value - " + capability);
            return false;
        }

        return true;
    }

    ArrayList<DiscoveringPackage> getDiscoveringPackages() {
        return mDiscoveringPackages;
    }

    void clearDiscoveringPackages() {
        synchronized (mDiscoveringPackages) {
            mDiscoveringPackages.clear();
        }
    }

    boolean startDiscovery(String callingPackage, @Nullable String callingFeatureId) {
        UserHandle callingUser = UserHandle.of(UserHandle.getCallingUserId());
        debugLog("startDiscovery");
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        boolean isQApp = Utils.isQApp(this, callingPackage);
        String permission = null;
        if (Utils.checkCallerHasNetworkSettingsPermission(this)) {
            permission = android.Manifest.permission.NETWORK_SETTINGS;
        } else if (Utils.checkCallerHasNetworkSetupWizardPermission(this)) {
            permission = android.Manifest.permission.NETWORK_SETUP_WIZARD;
        } else if (isQApp) {
            if (!Utils.checkCallerHasFineLocation(this, mAppOps, callingPackage, callingFeatureId,
                    callingUser)) {
                return false;
            }
            permission = android.Manifest.permission.ACCESS_FINE_LOCATION;
        } else {
            if (!Utils.checkCallerHasCoarseLocation(this, mAppOps, callingPackage, callingFeatureId,
                    callingUser)) {
                return false;
            }
            permission = android.Manifest.permission.ACCESS_COARSE_LOCATION;
        }

        synchronized (mDiscoveringPackages) {
            mDiscoveringPackages.add(new DiscoveringPackage(callingPackage, permission));
        }
        return startDiscoveryNative();
    }

    /**
     * Same as API method {@link BluetoothAdapter#getBondedDevices()}
     *
     * @return array of bonded {@link BluetoothDevice} or null on error
     */
    public BluetoothDevice[] getBondedDevices() {
        return mAdapterProperties.getBondedDevices();
    }

    /**
     * Get the database manager to access Bluetooth storage
     *
     * @return {@link DatabaseManager} or null on error
     */
    @VisibleForTesting
    public DatabaseManager getDatabase() {
        return mDatabaseManager;
    }

    boolean createBond(BluetoothDevice device, int transport, OobData oobData) {
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp != null && deviceProp.getBondState() != BluetoothDevice.BOND_NONE) {
            return false;
        }

        mRemoteDevices.setBondingInitiatedLocally(Utils.getByteAddress(device));

        // Pairing is unreliable while scanning, so cancel discovery
        // Note, remove this when native stack improves
        cancelDiscoveryNative();

        Message msg = mBondStateMachine.obtainMessage(BondStateMachine.CREATE_BOND);
        msg.obj = device;
        msg.arg1 = transport;

        if (oobData != null) {
            Bundle oobDataBundle = new Bundle();
            oobDataBundle.putParcelable(BondStateMachine.OOBDATA, oobData);
            msg.setData(oobDataBundle);
        }
        mBondStateMachine.sendMessage(msg);
        return true;
    }

    public boolean isQuietModeEnabled() {
        debugLog("isQuetModeEnabled() - Enabled = " + mQuietmode);
        return mQuietmode;
    }

    public void updateUuids() {
        debugLog("updateUuids() - Updating UUIDs for bonded devices");
        BluetoothDevice[] bondedDevices = getBondedDevices();
        if (bondedDevices == null) {
            return;
        }

        for (BluetoothDevice device : bondedDevices) {
            mRemoteDevices.updateUuids(device);
        }
    }

    /**
     * Update device UUID changed to {@link BondStateMachine}
     *
     * @param device remote device of interest
     */
    public void deviceUuidUpdated(BluetoothDevice device) {
        // Notify BondStateMachine for SDP complete / UUID changed.
        Message msg = mBondStateMachine.obtainMessage(BondStateMachine.UUID_UPDATE);
        msg.obj = device;
        mBondStateMachine.sendMessage(msg);
    }

    /**
     * Get the bond state of a particular {@link BluetoothDevice}
     *
     * @param device remote device of interest
     * @return bond state <p>Possible values are
     * {@link BluetoothDevice#BOND_NONE},
     * {@link BluetoothDevice#BOND_BONDING},
     * {@link BluetoothDevice#BOND_BONDED}.
     */
    @VisibleForTesting
    public int getBondState(BluetoothDevice device) {
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return BluetoothDevice.BOND_NONE;
        }
        return deviceProp.getBondState();
    }

    int getConnectionState(BluetoothDevice device) {
        return getConnectionStateNative(addressToBytes(device.getAddress()));
    }

    /**
     * Sets device as the active devices for the profiles passed into the function
     *
     * @param device is the remote bluetooth device
     * @param profiles is a constant that references for which profiles we'll be setting the remote
     *                 device as our active device. One of the following:
     *                 {@link BluetoothAdapter#ACTIVE_DEVICE_AUDIO},
     *                 {@link BluetoothAdapter#ACTIVE_DEVICE_PHONE_CALL}
     *                 {@link BluetoothAdapter#ACTIVE_DEVICE_ALL}
     * @return false if profiles value is not one of the constants we accept, true otherwise
     */
    public boolean setActiveDevice(BluetoothDevice device, @ActiveDeviceUse int profiles) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH_PRIVILEGED permission");

        boolean setA2dp = false;
        boolean setHeadset = false;

        // Determine for which profiles we want to set device as our active device
        switch(profiles) {
            case BluetoothAdapter.ACTIVE_DEVICE_AUDIO:
                setA2dp = true;
                break;
            case BluetoothAdapter.ACTIVE_DEVICE_PHONE_CALL:
                setHeadset = true;
                break;
            case BluetoothAdapter.ACTIVE_DEVICE_ALL:
                setA2dp = true;
                setHeadset = true;
                break;
            default:
                return false;
        }

        if (setA2dp && mA2dpService != null && (device == null
                || mA2dpService.getConnectionPolicy(device)
                == BluetoothProfile.CONNECTION_POLICY_ALLOWED)) {
            Log.i(TAG, "setActiveDevice: Setting active A2dp device " + device);
            mA2dpService.setActiveDevice(device);
        }

        if (mHearingAidService != null && (device == null
                || mHearingAidService.getConnectionPolicy(device)
                == BluetoothProfile.CONNECTION_POLICY_ALLOWED)) {
            Log.i(TAG, "setActiveDevice: Setting active Hearing Aid " + device);
            mHearingAidService.setActiveDevice(device);
        }

        if (setHeadset && mHeadsetService != null && (device == null
                || mHeadsetService.getConnectionPolicy(device)
                == BluetoothProfile.CONNECTION_POLICY_ALLOWED)) {
            Log.i(TAG, "setActiveDevice: Setting active Headset " + device);
            mHeadsetService.setActiveDevice(device);
        }

        return true;
    }

    /**
     * Connects all enabled and supported bluetooth profiles between the local and remote device
     *
     * @param device is the remote device with which to connect these profiles
     * @return true if all profiles successfully connected, false if an error occurred
     */
    public boolean connectAllEnabledProfiles(BluetoothDevice device) {
        if (!profileServicesRunning()) {
            Log.e(TAG, "connectAllEnabledProfiles: Not all profile services running");
            return false;
        }

        // Checks if any profiles are enabled and if so, only connect enabled profiles
        if (isAnyProfileEnabled(device)) {
            return connectEnabledProfiles(device);
        }

        int numProfilesConnected = 0;
        ParcelUuid[] remoteDeviceUuids = getRemoteUuids(device);
        ParcelUuid[] localDeviceUuids = mAdapterProperties.getUuids();

        // All profile toggles disabled, so connects all supported profiles
        if (mA2dpService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.A2DP, device)) {
            Log.i(TAG, "connectAllEnabledProfiles: Connecting A2dp");
            // Set connection policy also connects the profile with CONNECTION_POLICY_ALLOWED
            mA2dpService.setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            numProfilesConnected++;
        }
        if (mA2dpSinkService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.A2DP_SINK, device)) {
            Log.i(TAG, "connectAllEnabledProfiles: Connecting A2dp Sink");
            mA2dpSinkService.setConnectionPolicy(device,
                    BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            numProfilesConnected++;
        }
        if (mHeadsetService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.HEADSET, device)) {
            Log.i(TAG, "connectAllEnabledProfiles: Connecting Headset Profile");
            mHeadsetService.setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            numProfilesConnected++;
        }
        if (mHeadsetClientService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.HEADSET_CLIENT, device)) {
            Log.i(TAG, "connectAllEnabledProfiles: Connecting HFP");
            mHeadsetClientService.setConnectionPolicy(device,
                    BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            numProfilesConnected++;
        }
        if (mMapClientService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.MAP_CLIENT, device)) {
            Log.i(TAG, "connectAllEnabledProfiles: Connecting MAP");
            mMapClientService.setConnectionPolicy(device,
                    BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            numProfilesConnected++;
        }
        if (mHidHostService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.HID_HOST, device)) {
            Log.i(TAG, "connectAllEnabledProfiles: Connecting Hid Host Profile");
            mHidHostService.setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            numProfilesConnected++;
        }
        if (mPanService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.PAN, device)) {
            Log.i(TAG, "connectAllEnabledProfiles: Connecting Pan Profile");
            mPanService.setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            numProfilesConnected++;
        }
        if (mPbapClientService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.PBAP_CLIENT, device)) {
            Log.i(TAG, "connectAllEnabledProfiles: Connecting Pbap");
            mPbapClientService.setConnectionPolicy(device,
                    BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            numProfilesConnected++;
        }
        if (mHearingAidService != null && isSupported(localDeviceUuids, remoteDeviceUuids,
                BluetoothProfile.HEARING_AID, device)) {
            Log.i(TAG, "connectAllEnabledProfiles: Connecting Hearing Aid Profile");
            mHearingAidService.setConnectionPolicy(device,
                    BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            numProfilesConnected++;
        }

        Log.i(TAG, "connectAllEnabledProfiles: Number of Profiles Connected: "
                + numProfilesConnected);

        return true;
    }

    /**
     * Disconnects all enabled and supported bluetooth profiles between the local and remote device
     *
     * @param device is the remote device with which to disconnect these profiles
     * @return true if all profiles successfully disconnected, false if an error occurred
     */
    public boolean disconnectAllEnabledProfiles(BluetoothDevice device) {
        if (!profileServicesRunning()) {
            Log.e(TAG, "disconnectAllEnabledProfiles: Not all profile services bound");
            return false;
        }

        if (mA2dpService != null && mA2dpService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting A2dp");
            mA2dpService.disconnect(device);
        }
        if (mA2dpSinkService != null && mA2dpSinkService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting A2dp Sink");
            mA2dpSinkService.disconnect(device);
        }
        if (mHeadsetService != null && mHeadsetService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG,
                    "disconnectAllEnabledProfiles: Disconnecting Headset Profile");
            mHeadsetService.disconnect(device);
        }
        if (mHeadsetClientService != null && mHeadsetClientService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting HFP");
            mHeadsetClientService.disconnect(device);
        }
        if (mMapClientService != null && mMapClientService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting MAP Client");
            mMapClientService.disconnect(device);
        }
        if (mMapService != null && mMapService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting MAP");
            mMapService.disconnect(device);
        }
        if (mHidDeviceService != null && mHidDeviceService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting Hid Device Profile");
            mHidDeviceService.disconnect(device);
        }
        if (mHidHostService != null && mHidHostService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting Hid Host Profile");
            mHidHostService.disconnect(device);
        }
        if (mPanService != null && mPanService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting Pan Profile");
            mPanService.disconnect(device);
        }
        if (mPbapClientService != null && mPbapClientService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting Pbap Client");
            mPbapClientService.disconnect(device);
        }
        if (mPbapService != null && mPbapService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting Pbap Server");
            mPbapService.disconnect(device);
        }
        if (mHearingAidService != null && mHearingAidService.getConnectionState(device)
                == BluetoothProfile.STATE_CONNECTED) {
            Log.i(TAG, "disconnectAllEnabledProfiles: Disconnecting Hearing Aid Profile");
            mHearingAidService.disconnect(device);
        }

        return true;
    }

    /**
     * Same as API method {@link BluetoothDevice#getName()}
     *
     * @param device remote device of interest
     * @return remote device name
     */
    public String getRemoteName(BluetoothDevice device) {
        if (mRemoteDevices == null) {
            return null;
        }
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return null;
        }
        return deviceProp.getName();
    }

    /**
     * Get UUIDs for service supported by a remote device
     *
     * @param device the remote device that we want to get UUIDs from
     * @return
     */
    @VisibleForTesting
    public ParcelUuid[] getRemoteUuids(BluetoothDevice device) {
        DeviceProperties deviceProp = mRemoteDevices.getDeviceProperties(device);
        if (deviceProp == null) {
            return null;
        }
        return deviceProp.getUuids();
    }

    void logUserBondResponse(BluetoothDevice device, boolean accepted, int event) {
        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_BOND_STATE_CHANGED,
                obfuscateAddress(device), 0, device.getType(),
                BluetoothDevice.BOND_BONDING,
                event,
                accepted ? 0 : BluetoothDevice.UNBOND_REASON_AUTH_REJECTED);
    }

    int getDeviceAccessFromPrefs(BluetoothDevice device, String prefFile) {
        SharedPreferences prefs = getSharedPreferences(prefFile, Context.MODE_PRIVATE);
        if (!prefs.contains(device.getAddress())) {
            return BluetoothDevice.ACCESS_UNKNOWN;
        }
        return prefs.getBoolean(device.getAddress(), false)
                ? BluetoothDevice.ACCESS_ALLOWED
                : BluetoothDevice.ACCESS_REJECTED;
    }

    void setDeviceAccessFromPrefs(BluetoothDevice device, int value, String prefFile) {
        SharedPreferences pref = getSharedPreferences(prefFile, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        if (value == BluetoothDevice.ACCESS_UNKNOWN) {
            editor.remove(device.getAddress());
        } else {
            editor.putBoolean(device.getAddress(), value == BluetoothDevice.ACCESS_ALLOWED);
        }
        editor.apply();
    }

    void setPhonebookAccessPermission(BluetoothDevice device, int value) {
        setDeviceAccessFromPrefs(device, value, PHONEBOOK_ACCESS_PERMISSION_PREFERENCE_FILE);
    }

    void setMessageAccessPermission(BluetoothDevice device, int value) {
        setDeviceAccessFromPrefs(device, value, MESSAGE_ACCESS_PERMISSION_PREFERENCE_FILE);
    }

    void setSimAccessPermission(BluetoothDevice device, int value) {
        setDeviceAccessFromPrefs(device, value, SIM_ACCESS_PERMISSION_PREFERENCE_FILE);
    }

    public boolean isRpaOffloadSupported() {
        enforceBluetoothPermission(this);
        return mAdapterProperties.isRpaOffloadSupported();
    }

    public int getNumOfOffloadedIrkSupported() {
        enforceBluetoothPermission(this);
        return mAdapterProperties.getNumOfOffloadedIrkSupported();
    }

    public int getNumOfOffloadedScanFilterSupported() {
        return mAdapterProperties.getNumOfOffloadedScanFilterSupported();
    }

    public int getOffloadedScanResultStorage() {
        return mAdapterProperties.getOffloadedScanResultStorage();
    }

    public boolean isLe2MPhySupported() {
        return mAdapterProperties.isLe2MPhySupported();
    }

    public boolean isLeCodedPhySupported() {
        return mAdapterProperties.isLeCodedPhySupported();
    }

    public boolean isLeExtendedAdvertisingSupported() {
        return mAdapterProperties.isLeExtendedAdvertisingSupported();
    }

    public boolean isLePeriodicAdvertisingSupported() {
        return mAdapterProperties.isLePeriodicAdvertisingSupported();
    }

    public int getLeMaximumAdvertisingDataLength() {
        return mAdapterProperties.getLeMaximumAdvertisingDataLength();
    }

    /**
     * Get the maximum number of connected audio devices.
     *
     * @return the maximum number of connected audio devices
     */
    public int getMaxConnectedAudioDevices() {
        return mAdapterProperties.getMaxConnectedAudioDevices();
    }

    /**
     * Check whether A2DP offload is enabled.
     *
     * @return true if A2DP offload is enabled
     */
    public boolean isA2dpOffloadEnabled() {
        return mAdapterProperties.isA2dpOffloadEnabled();
    }

    private BluetoothActivityEnergyInfo reportActivityInfo() {
        if (mAdapterProperties.getState() != BluetoothAdapter.STATE_ON
                || !mAdapterProperties.isActivityAndEnergyReportingSupported()) {
            return null;
        }

        // Pull the data. The callback will notify mEnergyInfoLock.
        readEnergyInfo();

        synchronized (mEnergyInfoLock) {
            try {
                mEnergyInfoLock.wait(CONTROLLER_ENERGY_UPDATE_TIMEOUT_MILLIS);
            } catch (InterruptedException e) {
                // Just continue, the energy data may be stale but we won't miss anything next time
                // we query.
            }

            final BluetoothActivityEnergyInfo info =
                    new BluetoothActivityEnergyInfo(SystemClock.elapsedRealtime(),
                            mStackReportedState, mTxTimeTotalMs, mRxTimeTotalMs, mIdleTimeTotalMs,
                            mEnergyUsedTotalVoltAmpSecMicro);

            // Count the number of entries that have byte counts > 0
            int arrayLen = 0;
            for (int i = 0; i < mUidTraffic.size(); i++) {
                final UidTraffic traffic = mUidTraffic.valueAt(i);
                if (traffic.getTxBytes() != 0 || traffic.getRxBytes() != 0) {
                    arrayLen++;
                }
            }

            // Copy the traffic objects whose byte counts are > 0
            final UidTraffic[] result = arrayLen > 0 ? new UidTraffic[arrayLen] : null;
            int putIdx = 0;
            for (int i = 0; i < mUidTraffic.size(); i++) {
                final UidTraffic traffic = mUidTraffic.valueAt(i);
                if (traffic.getTxBytes() != 0 || traffic.getRxBytes() != 0) {
                    result[putIdx++] = traffic.clone();
                }
            }

            info.setUidTraffic(result);

            return info;
        }
    }

    public int getTotalNumOfTrackableAdvertisements() {
        enforceBluetoothPermission(this);
        return mAdapterProperties.getTotalNumOfTrackableAdvertisements();
    }

    private static int convertScanModeToHal(int mode) {
        switch (mode) {
            case BluetoothAdapter.SCAN_MODE_NONE:
                return AbstractionLayer.BT_SCAN_MODE_NONE;
            case BluetoothAdapter.SCAN_MODE_CONNECTABLE:
                return AbstractionLayer.BT_SCAN_MODE_CONNECTABLE;
            case BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE:
                return AbstractionLayer.BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE;
        }
        // errorLog("Incorrect scan mode in convertScanModeToHal");
        return -1;
    }

    static int convertScanModeFromHal(int mode) {
        switch (mode) {
            case AbstractionLayer.BT_SCAN_MODE_NONE:
                return BluetoothAdapter.SCAN_MODE_NONE;
            case AbstractionLayer.BT_SCAN_MODE_CONNECTABLE:
                return BluetoothAdapter.SCAN_MODE_CONNECTABLE;
            case AbstractionLayer.BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE:
                return BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE;
        }
        //errorLog("Incorrect scan mode in convertScanModeFromHal");
        return -1;
    }

    // This function is called from JNI. It allows native code to set a single wake
    // alarm. If an alarm is already pending and a new request comes in, the alarm
    // will be rescheduled (i.e. the previously set alarm will be cancelled).
    private boolean setWakeAlarm(long delayMillis, boolean shouldWake) {
        synchronized (this) {
            if (mPendingAlarm != null) {
                mAlarmManager.cancel(mPendingAlarm);
            }

            long wakeupTime = SystemClock.elapsedRealtime() + delayMillis;
            int type = shouldWake ? AlarmManager.ELAPSED_REALTIME_WAKEUP
                    : AlarmManager.ELAPSED_REALTIME;

            Intent intent = new Intent(ACTION_ALARM_WAKEUP);
            mPendingAlarm =
                    PendingIntent.getBroadcast(this, 0, intent, PendingIntent.FLAG_ONE_SHOT);
            mAlarmManager.setExact(type, wakeupTime, mPendingAlarm);
            return true;
        }
    }

    // This function is called from JNI. It allows native code to acquire a single wake lock.
    // If the wake lock is already held, this function returns success. Although this function
    // only supports acquiring a single wake lock at a time right now, it will eventually be
    // extended to allow acquiring an arbitrary number of wake locks. The current interface
    // takes |lockName| as a parameter in anticipation of that implementation.
    private boolean acquireWakeLock(String lockName) {
        synchronized (this) {
            if (mWakeLock == null) {
                mWakeLockName = lockName;
                mWakeLock = mPowerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, lockName);
            }

            if (!mWakeLock.isHeld()) {
                mWakeLock.acquire();
            }
        }
        return true;
    }

    // This function is called from JNI. It allows native code to release a wake lock acquired
    // by |acquireWakeLock|. If the wake lock is not held, this function returns failure.
    // Note that the release() call is also invoked by {@link #cleanup()} so a synchronization is
    // needed here. See the comment for |acquireWakeLock| for an explanation of the interface.
    private boolean releaseWakeLock(String lockName) {
        synchronized (this) {
            if (mWakeLock == null) {
                errorLog("Repeated wake lock release; aborting release: " + lockName);
                return false;
            }

            if (mWakeLock.isHeld()) {
                mWakeLock.release();
            }
        }
        return true;
    }

    private void energyInfoCallback(int status, int ctrlState, long txTime, long rxTime,
            long idleTime, long energyUsed, UidTraffic[] data) throws RemoteException {
        if (ctrlState >= BluetoothActivityEnergyInfo.BT_STACK_STATE_INVALID
                && ctrlState <= BluetoothActivityEnergyInfo.BT_STACK_STATE_STATE_IDLE) {
            // Energy is product of mA, V and ms. If the chipset doesn't
            // report it, we have to compute it from time
            if (energyUsed == 0) {
                try {
                    final long txMah = Math.multiplyExact(txTime, getTxCurrentMa());
                    final long rxMah = Math.multiplyExact(rxTime, getRxCurrentMa());
                    final long idleMah = Math.multiplyExact(idleTime, getIdleCurrentMa());
                    energyUsed = (long) (Math.addExact(Math.addExact(txMah, rxMah), idleMah)
                            * getOperatingVolt());
                } catch (ArithmeticException e) {
                    Log.wtf(TAG, "overflow in bluetooth energy callback", e);
                    // Energy is already 0 if the exception was thrown.
                }
            }

            synchronized (mEnergyInfoLock) {
                mStackReportedState = ctrlState;
                long totalTxTimeMs;
                long totalRxTimeMs;
                long totalIdleTimeMs;
                long totalEnergy;
                try {
                    totalTxTimeMs = Math.addExact(mTxTimeTotalMs, txTime);
                    totalRxTimeMs = Math.addExact(mRxTimeTotalMs, rxTime);
                    totalIdleTimeMs = Math.addExact(mIdleTimeTotalMs, idleTime);
                    totalEnergy = Math.addExact(mEnergyUsedTotalVoltAmpSecMicro, energyUsed);
                } catch (ArithmeticException e) {
                    // This could be because we accumulated a lot of time, or we got a very strange
                    // value from the controller (more likely). Discard this data.
                    Log.wtf(TAG, "overflow in bluetooth energy callback", e);
                    totalTxTimeMs = mTxTimeTotalMs;
                    totalRxTimeMs = mRxTimeTotalMs;
                    totalIdleTimeMs = mIdleTimeTotalMs;
                    totalEnergy = mEnergyUsedTotalVoltAmpSecMicro;
                }

                mTxTimeTotalMs = totalTxTimeMs;
                mRxTimeTotalMs = totalRxTimeMs;
                mIdleTimeTotalMs = totalIdleTimeMs;
                mEnergyUsedTotalVoltAmpSecMicro = totalEnergy;

                for (UidTraffic traffic : data) {
                    UidTraffic existingTraffic = mUidTraffic.get(traffic.getUid());
                    if (existingTraffic == null) {
                        mUidTraffic.put(traffic.getUid(), traffic);
                    } else {
                        existingTraffic.addRxBytes(traffic.getRxBytes());
                        existingTraffic.addTxBytes(traffic.getTxBytes());
                    }
                }
                mEnergyInfoLock.notifyAll();
            }
        }

        verboseLog("energyInfoCallback() status = " + status + "txTime = " + txTime + "rxTime = "
                + rxTime + "idleTime = " + idleTime + "energyUsed = " + energyUsed + "ctrlState = "
                + ctrlState + "traffic = " + Arrays.toString(data));
    }

    /**
     * Update metadata change to registered listeners
     */
    @VisibleForTesting
    public void metadataChanged(String address, int key, byte[] value) {
        BluetoothDevice device = mRemoteDevices.getDevice(Utils.getBytesFromAddress(address));
        if (mMetadataListeners.containsKey(device)) {
            ArrayList<IBluetoothMetadataListener> list = mMetadataListeners.get(device);
            for (IBluetoothMetadataListener listener : list) {
                try {
                    listener.onMetadataChanged(device, key, value);
                } catch (RemoteException e) {
                    Log.w(TAG, "RemoteException when onMetadataChanged");
                }
            }
        }
    }

    private int getIdleCurrentMa() {
        return getResources().getInteger(R.integer.config_bluetooth_idle_cur_ma);
    }

    private int getTxCurrentMa() {
        return getResources().getInteger(R.integer.config_bluetooth_tx_cur_ma);
    }

    private int getRxCurrentMa() {
        return getResources().getInteger(R.integer.config_bluetooth_rx_cur_ma);
    }

    private double getOperatingVolt() {
        return getResources().getInteger(R.integer.config_bluetooth_operating_voltage_mv) / 1000.0;
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        if (args.length == 0) {
            writer.println("Skipping dump in APP SERVICES, see bluetooth_manager section.");
            writer.println("Use --print argument for dumpsys direct from AdapterService.");
            return;
        }

        verboseLog("dumpsys arguments, check for protobuf output: " + TextUtils.join(" ", args));
        if (args[0].equals("--proto-bin")) {
            dumpMetrics(fd);
            return;
        }

        writer.println();
        mAdapterProperties.dump(fd, writer, args);
        writer.println("mSnoopLogSettingAtEnable = " + mSnoopLogSettingAtEnable);
        writer.println("mDefaultSnoopLogSettingAtEnable = " + mDefaultSnoopLogSettingAtEnable);

        writer.println();
        mAdapterStateMachine.dump(fd, writer, args);

        StringBuilder sb = new StringBuilder();
        for (ProfileService profile : mRegisteredProfiles) {
            profile.dump(sb);
        }
        mSilenceDeviceManager.dump(fd, writer, args);
        mDatabaseManager.dump(writer);

        writer.write(sb.toString());
        writer.flush();

        dumpNative(fd, args);
    }

    private void dumpMetrics(FileDescriptor fd) {
        BluetoothMetricsProto.BluetoothLog.Builder metricsBuilder =
                BluetoothMetricsProto.BluetoothLog.newBuilder();
        byte[] nativeMetricsBytes = dumpMetricsNative();
        debugLog("dumpMetrics: native metrics size is " + nativeMetricsBytes.length);
        if (nativeMetricsBytes.length > 0) {
            try {
                metricsBuilder.mergeFrom(nativeMetricsBytes);
            } catch (InvalidProtocolBufferException ex) {
                Log.w(TAG, "dumpMetrics: problem parsing metrics protobuf, " + ex.getMessage());
                return;
            }
        }
        metricsBuilder.setNumBondedDevices(getBondedDevices().length);
        MetricsLogger.dumpProto(metricsBuilder);
        for (ProfileService profile : mRegisteredProfiles) {
            profile.dumpProto(metricsBuilder);
        }
        byte[] metricsBytes = Base64.encode(metricsBuilder.build().toByteArray(), Base64.DEFAULT);
        debugLog("dumpMetrics: combined metrics size is " + metricsBytes.length);
        try (FileOutputStream protoOut = new FileOutputStream(fd)) {
            protoOut.write(metricsBytes);
        } catch (IOException e) {
            errorLog("dumpMetrics: error writing combined protobuf to fd, " + e.getMessage());
        }
    }

    private void debugLog(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }

    private void verboseLog(String msg) {
        if (VERBOSE) {
            Log.v(TAG, msg);
        }
    }

    private void errorLog(String msg) {
        Log.e(TAG, msg);
    }

    private final BroadcastReceiver mAlarmBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (AdapterService.this) {
                mPendingAlarm = null;
                alarmFiredNative();
            }
        }
    };

    private boolean isGuest() {
        return UserManager.get(this).isGuestUser();
    }

    private boolean isNiapMode() {
        return ((DevicePolicyManager) getSystemService(Context.DEVICE_POLICY_SERVICE))
                .isCommonCriteriaModeEnabled(null);
    }

    /**
     *  Obfuscate Bluetooth MAC address into a PII free ID string
     *
     *  @param device Bluetooth device whose MAC address will be obfuscated
     *  @return a byte array that is unique to this MAC address on this device,
     *          or empty byte array when either device is null or obfuscateAddressNative fails
     */
    public byte[] obfuscateAddress(BluetoothDevice device) {
        if (device == null) {
            return new byte[0];
        }
        return obfuscateAddressNative(Utils.getByteAddress(device));
    }

    /**
     *  Get an incremental id of Bluetooth metrics and log
     *
     *  @param device Bluetooth device
     *  @return int of id for Bluetooth metrics and logging, 0 if the device is invalid
     */
    public int getMetricId(BluetoothDevice device) {
        if (device == null) {
            return 0;
        }
        return getMetricIdNative(Utils.getByteAddress(device));
    }

    static native void classInitNative();

    native boolean initNative(boolean startRestricted, boolean isNiapMode,
            int configCompareResult, boolean isAtvDevice);

    native void cleanupNative();

    /*package*/
    native boolean enableNative();

    /*package*/
    native boolean disableNative();

    /*package*/
    native boolean setAdapterPropertyNative(int type, byte[] val);

    /*package*/
    native boolean getAdapterPropertiesNative();

    /*package*/
    native boolean getAdapterPropertyNative(int type);

    /*package*/
    native boolean setAdapterPropertyNative(int type);

    /*package*/
    native boolean setDevicePropertyNative(byte[] address, int type, byte[] val);

    /*package*/
    native boolean getDevicePropertyNative(byte[] address, int type);

    /*package*/
    native boolean createBondNative(byte[] address, int transport);

    /*package*/
    native boolean createBondOutOfBandNative(byte[] address, int transport, OobData oobData);

    /*package*/
    native boolean removeBondNative(byte[] address);

    /*package*/
    native boolean cancelBondNative(byte[] address);

    /*package*/
    native boolean sdpSearchNative(byte[] address, byte[] uuid);

    /*package*/
    native int getConnectionStateNative(byte[] address);

    private native boolean startDiscoveryNative();

    private native boolean cancelDiscoveryNative();

    private native boolean pinReplyNative(byte[] address, boolean accept, int len, byte[] pin);

    private native boolean sspReplyNative(byte[] address, int type, boolean accept, int passkey);

    /*package*/
    native boolean getRemoteServicesNative(byte[] address);

    /*package*/
    native boolean getRemoteMasInstancesNative(byte[] address);

    private native int readEnergyInfo();

    /*package*/
    native boolean factoryResetNative();

    private native void alarmFiredNative();

    private native void dumpNative(FileDescriptor fd, String[] arguments);

    private native byte[] dumpMetricsNative();

    private native void interopDatabaseClearNative();

    private native void interopDatabaseAddNative(int feature, byte[] address, int length);

    private native byte[] obfuscateAddressNative(byte[] address);

    private native int getMetricIdNative(byte[] address);

    /*package*/ native int connectSocketNative(
            byte[] address, int type, byte[] uuid, int port, int flag, int callingUid);

    /*package*/ native int createSocketChannelNative(
            int type, String serviceName, byte[] uuid, int port, int flag, int callingUid);

    /*package*/ native void requestMaximumTxDataLengthNative(byte[] address);

    // Returns if this is a mock object. This is currently used in testing so that we may not call
    // System.exit() while finalizing the object. Otherwise GC of mock objects unfortunately ends up
    // calling finalize() which in turn calls System.exit() and the process crashes.
    //
    // Mock this in your testing framework to return true to avoid the mentioned behavior. In
    // production this has no effect.
    public boolean isMock() {
        return false;
    }
}
