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

package com.android.car.developeroptions.wifi.tether;

import android.app.Activity;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.app.Service;
import android.app.usage.UsageStatsManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothPan;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothProfile.ServiceListener;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.ConnectivityManager;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.os.SystemClock;
import android.telephony.SubscriptionManager;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

public class TetherService extends Service {
    private static final String TAG = "TetherService";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    @VisibleForTesting
    public static final String EXTRA_RESULT = "EntitlementResult";
    @VisibleForTesting
    public static final String EXTRA_TETHER_PROVISIONING_RESPONSE =
            "android.net.extra.TETHER_PROVISIONING_RESPONSE";
    @VisibleForTesting
    public static final String EXTRA_TETHER_SILENT_PROVISIONING_ACTION =
            "android.net.extra.TETHER_SILENT_PROVISIONING_ACTION";

    // Activity results to match the activity provision protocol.
    // Default to something not ok.
    private static final int RESULT_DEFAULT = Activity.RESULT_CANCELED;
    private static final int RESULT_OK = Activity.RESULT_OK;

    private static final String TETHER_CHOICE = "TETHER_TYPE";
    private static final int MS_PER_HOUR = 60 * 60 * 1000;

    private static final String PREFS = "tetherPrefs";
    private static final String KEY_TETHERS = "currentTethers";

    private int mCurrentTypeIndex;
    private boolean mInProvisionCheck;
    /** Intent action received from the provisioning app when entitlement check completes. */
    private String mExpectedProvisionResponseAction = null;
    /** Intent action sent to the provisioning app to request an entitlement check. */
    private String mProvisionAction;
    private TetherServiceWrapper mWrapper;
    private ArrayList<Integer> mCurrentTethers;
    private ArrayMap<Integer, List<ResultReceiver>> mPendingCallbacks;
    private HotspotOffReceiver mHotspotReceiver;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (DEBUG) Log.d(TAG, "Creating TetherService");
        SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        mCurrentTethers = stringToTethers(prefs.getString(KEY_TETHERS, ""));
        mCurrentTypeIndex = 0;
        mPendingCallbacks = new ArrayMap<>(3);
        mPendingCallbacks.put(ConnectivityManager.TETHERING_WIFI, new ArrayList<ResultReceiver>());
        mPendingCallbacks.put(ConnectivityManager.TETHERING_USB, new ArrayList<ResultReceiver>());
        mPendingCallbacks.put(
                ConnectivityManager.TETHERING_BLUETOOTH, new ArrayList<ResultReceiver>());
        mHotspotReceiver = new HotspotOffReceiver(this);
    }

    // Registers the broadcast receiver for the specified response action, first unregistering
    // the receiver if it was registered for a different response action.
    private void maybeRegisterReceiver(final String responseAction) {
        if (Objects.equals(responseAction, mExpectedProvisionResponseAction)) return;

        if (mExpectedProvisionResponseAction != null) unregisterReceiver(mReceiver);

        registerReceiver(mReceiver, new IntentFilter(responseAction),
                android.Manifest.permission.TETHER_PRIVILEGED, null /* handler */);
        mExpectedProvisionResponseAction = responseAction;
        if (DEBUG) Log.d(TAG, "registerReceiver " + responseAction);
    }

    private int stopSelfAndStartNotSticky() {
        stopSelf();
        return START_NOT_STICKY;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent.hasExtra(ConnectivityManager.EXTRA_ADD_TETHER_TYPE)) {
            int type = intent.getIntExtra(ConnectivityManager.EXTRA_ADD_TETHER_TYPE,
                    ConnectivityManager.TETHERING_INVALID);
            ResultReceiver callback =
                    intent.getParcelableExtra(ConnectivityManager.EXTRA_PROVISION_CALLBACK);
            if (callback != null) {
                List<ResultReceiver> callbacksForType = mPendingCallbacks.get(type);
                if (callbacksForType != null) {
                    callbacksForType.add(callback);
                } else {
                    // Invalid tether type. Just ignore this request and report failure.
                    Log.e(TAG, "Invalid tethering type " + type + ", stopping");
                    callback.send(ConnectivityManager.TETHER_ERROR_UNKNOWN_IFACE, null);
                    return stopSelfAndStartNotSticky();
                }
            }

            if (!mCurrentTethers.contains(type)) {
                if (DEBUG) Log.d(TAG, "Adding tether " + type);
                mCurrentTethers.add(type);
            }
        }

        mProvisionAction = intent.getStringExtra(EXTRA_TETHER_SILENT_PROVISIONING_ACTION);
        if (mProvisionAction == null) {
            Log.e(TAG, "null provisioning action, stop ");
            return stopSelfAndStartNotSticky();
        }

        final String response = intent.getStringExtra(EXTRA_TETHER_PROVISIONING_RESPONSE);
        if (response == null) {
            Log.e(TAG, "null provisioning response, stop ");
            return stopSelfAndStartNotSticky();
        }
        maybeRegisterReceiver(response);

        if (intent.hasExtra(ConnectivityManager.EXTRA_REM_TETHER_TYPE)) {
            if (!mInProvisionCheck) {
                int type = intent.getIntExtra(ConnectivityManager.EXTRA_REM_TETHER_TYPE,
                        ConnectivityManager.TETHERING_INVALID);
                int index = mCurrentTethers.indexOf(type);
                if (DEBUG) Log.d(TAG, "Removing tether " + type + ", index " + index);
                if (index >= 0) {
                    removeTypeAtIndex(index);
                }
            } else {
                if (DEBUG) Log.d(TAG, "Don't cancel alarm during provisioning");
            }
        }

        if (intent.getBooleanExtra(ConnectivityManager.EXTRA_RUN_PROVISION, false)) {
            startProvisioning(mCurrentTypeIndex);
        } else if (!mInProvisionCheck) {
            // If we aren't running any provisioning, no reason to stay alive.
            if (DEBUG) Log.d(TAG, "Stopping self.  startid: " + startId);
            return stopSelfAndStartNotSticky();
        }
        // We want to be started if we are killed accidently, so that we can be sure we finish
        // the check.
        return START_REDELIVER_INTENT;
    }

    @Override
    public void onDestroy() {
        if (mInProvisionCheck) {
            Log.e(TAG, "TetherService getting destroyed while mid-provisioning"
                    + mCurrentTethers.get(mCurrentTypeIndex));
        }
        SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        prefs.edit().putString(KEY_TETHERS, tethersToString(mCurrentTethers)).commit();

        if (mExpectedProvisionResponseAction != null) {
            unregisterReceiver(mReceiver);
            mExpectedProvisionResponseAction = null;
        }
        mHotspotReceiver.unregister();
        if (DEBUG) Log.d(TAG, "Destroying TetherService");
        super.onDestroy();
    }

    private void removeTypeAtIndex(int index) {
        mCurrentTethers.remove(index);
        // If we are currently in the middle of a check, we may need to adjust the
        // index accordingly.
        if (DEBUG) Log.d(TAG, "mCurrentTypeIndex: " + mCurrentTypeIndex);
        if (index <= mCurrentTypeIndex && mCurrentTypeIndex > 0) {
            mCurrentTypeIndex--;
        }
    }

    @VisibleForTesting
    void setHotspotOffReceiver(HotspotOffReceiver receiver) {
        mHotspotReceiver = receiver;
    }

    private ArrayList<Integer> stringToTethers(String tethersStr) {
        ArrayList<Integer> ret = new ArrayList<Integer>();
        if (TextUtils.isEmpty(tethersStr)) return ret;

        String[] tethersSplit = tethersStr.split(",");
        for (int i = 0; i < tethersSplit.length; i++) {
            ret.add(Integer.parseInt(tethersSplit[i]));
        }
        return ret;
    }

    private String tethersToString(ArrayList<Integer> tethers) {
        final StringBuffer buffer = new StringBuffer();
        final int N = tethers.size();
        for (int i = 0; i < N; i++) {
            if (i != 0) {
                buffer.append(',');
            }
            buffer.append(tethers.get(i));
        }

        return buffer.toString();
    }

    private void disableWifiTethering() {
        ConnectivityManager cm =
                (ConnectivityManager)getSystemService(Context.CONNECTIVITY_SERVICE);
        cm.stopTethering(ConnectivityManager.TETHERING_WIFI);
    }

    private void disableUsbTethering() {
        ConnectivityManager cm =
                (ConnectivityManager)getSystemService(Context.CONNECTIVITY_SERVICE);
        cm.setUsbTethering(false);
    }

    private void disableBtTethering() {
        final BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter != null) {
            adapter.getProfileProxy(this, new ServiceListener() {
                @Override
                public void onServiceDisconnected(int profile) { }

                @Override
                public void onServiceConnected(int profile, BluetoothProfile proxy) {
                    ((BluetoothPan) proxy).setBluetoothTethering(false);
                    adapter.closeProfileProxy(BluetoothProfile.PAN, proxy);
                }
            }, BluetoothProfile.PAN);
        }
    }

    private void startProvisioning(int index) {
        if (index >= mCurrentTethers.size()) return;
        Intent intent = getProvisionBroadcastIntent(index);
        setEntitlementAppActive(index);

        if (DEBUG) {
            Log.d(TAG, "Sending provisioning broadcast: " + intent.getAction()
                    + " type: " + mCurrentTethers.get(index));
        }

        sendBroadcast(intent);
        mInProvisionCheck = true;
    }

    private Intent getProvisionBroadcastIntent(int index) {
        if (mProvisionAction == null) Log.wtf(TAG, "null provisioning action");
        Intent intent = new Intent(mProvisionAction);
        int type = mCurrentTethers.get(index);
        intent.putExtra(TETHER_CHOICE, type);
        intent.setFlags(Intent.FLAG_RECEIVER_FOREGROUND
                | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);

        return intent;
    }

    private void setEntitlementAppActive(int index) {
        final PackageManager packageManager = getPackageManager();
        Intent intent = getProvisionBroadcastIntent(index);
        List<ResolveInfo> resolvers =
                packageManager.queryBroadcastReceivers(intent, PackageManager.MATCH_ALL);
        if (resolvers.isEmpty()) {
            Log.e(TAG, "No found BroadcastReceivers for provision intent.");
            return;
        }

        for (ResolveInfo resolver : resolvers) {
            if (resolver.activityInfo.applicationInfo.isSystemApp()) {
                String packageName = resolver.activityInfo.packageName;
                getTetherServiceWrapper().setAppInactive(packageName, false);
            }
        }
    }

    @VisibleForTesting
    void scheduleAlarm() {
        Intent intent = new Intent(this, TetherService.class);
        intent.putExtra(ConnectivityManager.EXTRA_RUN_PROVISION, true);

        PendingIntent pendingIntent = PendingIntent.getService(this, 0, intent, 0);
        AlarmManager alarmManager = (AlarmManager) getSystemService(ALARM_SERVICE);
        long periodMs = 24 * MS_PER_HOUR;
        long firstTime = SystemClock.elapsedRealtime() + periodMs;
        if (DEBUG) Log.d(TAG, "Scheduling alarm at interval " + periodMs);
        alarmManager.setRepeating(AlarmManager.ELAPSED_REALTIME, firstTime, periodMs,
                pendingIntent);
        mHotspotReceiver.register();
    }

    /**
     * Cancels the recheck alarm only if no tethering is currently active.
     *
     * Runs in the background, to get access to bluetooth service that takes time to bind.
     */
    public static void cancelRecheckAlarmIfNecessary(final Context context, int type) {
        Intent intent = new Intent(context, TetherService.class);
        intent.putExtra(ConnectivityManager.EXTRA_REM_TETHER_TYPE, type);
        context.startService(intent);
    }

    @VisibleForTesting
    void cancelAlarmIfNecessary() {
        if (mCurrentTethers.size() != 0) {
            if (DEBUG) Log.d(TAG, "Tethering still active, not cancelling alarm");
            return;
        }
        Intent intent = new Intent(this, TetherService.class);
        PendingIntent pendingIntent = PendingIntent.getService(this, 0, intent, 0);
        AlarmManager alarmManager = (AlarmManager) getSystemService(ALARM_SERVICE);
        alarmManager.cancel(pendingIntent);
        if (DEBUG) Log.d(TAG, "Tethering no longer active, canceling recheck");
        mHotspotReceiver.unregister();
    }

    private void fireCallbacksForType(int type, int result) {
        List<ResultReceiver> callbacksForType = mPendingCallbacks.get(type);
        if (callbacksForType == null) {
            return;
        }
        int errorCode = result == RESULT_OK ? ConnectivityManager.TETHER_ERROR_NO_ERROR :
                ConnectivityManager.TETHER_ERROR_PROVISION_FAILED;
        for (ResultReceiver callback : callbacksForType) {
          if (DEBUG) Log.d(TAG, "Firing result: " + errorCode + " to callback");
          callback.send(errorCode, null);
        }
        callbacksForType.clear();
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (DEBUG) Log.d(TAG, "Got provision result " + intent);
            if (!intent.getAction().equals(mExpectedProvisionResponseAction)) {
                Log.e(TAG, "Received provisioning response for unexpected action="
                        + intent.getAction() + ", expected=" + mExpectedProvisionResponseAction);
                return;
            }

            if (!mInProvisionCheck) {
                Log.e(TAG, "Unexpected provisioning response when not in provisioning check"
                        + intent);
                return;
            }


            if (!mInProvisionCheck) {
                Log.e(TAG, "Unexpected provision response " + intent);
                return;
            }
            int checkType = mCurrentTethers.get(mCurrentTypeIndex);
            mInProvisionCheck = false;
            int result = intent.getIntExtra(EXTRA_RESULT, RESULT_DEFAULT);
            if (result != RESULT_OK) {
                switch (checkType) {
                    case ConnectivityManager.TETHERING_WIFI:
                        disableWifiTethering();
                        break;
                    case ConnectivityManager.TETHERING_BLUETOOTH:
                        disableBtTethering();
                        break;
                    case ConnectivityManager.TETHERING_USB:
                        disableUsbTethering();
                        break;
                }
            }
            fireCallbacksForType(checkType, result);

            if (++mCurrentTypeIndex >= mCurrentTethers.size()) {
                // We are done with all checks, time to die.
                stopSelf();
            } else {
                // Start the next check in our list.
                startProvisioning(mCurrentTypeIndex);
            }
        }
    };

    @VisibleForTesting
    void setTetherServiceWrapper(TetherServiceWrapper wrapper) {
        mWrapper = wrapper;
    }

    private TetherServiceWrapper getTetherServiceWrapper() {
        if (mWrapper == null) {
            mWrapper = new TetherServiceWrapper(this);
        }
        return mWrapper;
    }

    /**
     * A static helper class used for tests. UsageStatsManager cannot be mocked out because
     * it's marked final. This class can be mocked out instead.
     */
    @VisibleForTesting
    public static class TetherServiceWrapper {
        private final UsageStatsManager mUsageStatsManager;

        TetherServiceWrapper(Context context) {
            mUsageStatsManager = (UsageStatsManager)
                    context.getSystemService(Context.USAGE_STATS_SERVICE);
        }

        void setAppInactive(String packageName, boolean isInactive) {
            mUsageStatsManager.setAppInactive(packageName, isInactive);
        }

        int getDefaultDataSubscriptionId() {
            return SubscriptionManager.getDefaultDataSubscriptionId();
        }
    }
}
