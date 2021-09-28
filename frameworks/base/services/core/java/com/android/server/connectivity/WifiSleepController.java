/*
 * Copyright (C) 2020 Rock-Chips Corporation, All Rights Reserved.
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

package com.android.server.connectivity;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.WifiManager;
import android.os.BatteryManager;
import android.os.PowerManager;
import android.os.SystemProperties;
import android.provider.Settings;

import android.util.Log;

public class WifiSleepController {
    private static final String LOG_TAG = "WifiSleepController";
    private static final boolean DBG = true;

    private static final String ACTION_WIFI_SLEEP_TIMEOUT_ALARM =
			"WifiSleepController.TimeoutForWifiSleep";

    private static final String PERSISTENT_PROPERTY_WIFI_SLEEP_DELAY = "persist.wifi.sleep.delay.ms";
    private static final String PERSISTENT_PROPERTY_WIFI_SLEEP_FLAG = "persist.wifi.sleep.flag";
    private static final int DEFAULT_WIFI_SLEEP_DELAY_MS = 15 * 60 * 1000;
    private static final String PERSISTENT_PROPERTY_BT_SLEEP_FLAG = "persist.bt.sleep.flag";


    static final int WIFI_DISABLED = 0;
    static final int WIFI_ENABLED = 1;
    static final int WIFI_ENABLED_AIRPLANE_OVERRIDE = 2;
    static final int BT_DISABLED = 0;
    static final int BT_ENABLED = 1;
    static final int BT_ENABLED_AIRPLANE_OVERRIDE = 2;
  
    private Context mContext;
    private AlarmManager mAlarmManager;
    private WifiManager mWifiManager;
    private PowerManager.WakeLock mWifiWakeLock;
    private WifiReceiver mWifiReceiver = new WifiReceiver();

    private PendingIntent mWifiSleepIntent = null;
    private boolean mCharging = false;
    private boolean mIsScreenON = true;

    private boolean mBtPowerDownSetting = SystemProperties.getBoolean("persist.bt.power.down", false);
    private boolean mBtIsOpened = false;
    private boolean mWifiIsOpened = false;

    public WifiSleepController(Context context) {
        mContext = context;
	PowerManager pm = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
	mWifiWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "WifiSleepPowerDown");
	mWifiManager = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        mAlarmManager = (AlarmManager)mContext.getSystemService(Context.ALARM_SERVICE);
        IntentFilter filter = new IntentFilter();
	filter.addAction(Intent.ACTION_BOOT_COMPLETED);
        filter.addAction(Intent.ACTION_BATTERY_CHANGED);
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
	filter.addAction(ACTION_WIFI_SLEEP_TIMEOUT_ALARM);
        mContext.registerReceiver(mWifiReceiver, filter);
    }

    private class WifiReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            log("onReceive, action=" + action);
            if (action.equals(Intent.ACTION_BATTERY_CHANGED)) {
                updateChargingState(intent);
            } else if (action.equals(Intent.ACTION_SCREEN_ON)) {
                mIsScreenON = true;
                exitSleepState();
                SystemProperties.set(PERSISTENT_PROPERTY_WIFI_SLEEP_FLAG, "false");
                SystemProperties.set(PERSISTENT_PROPERTY_BT_SLEEP_FLAG, "false");
            } else if (action.equals(Intent.ACTION_SCREEN_OFF)) {
                mIsScreenON = false;
                log("isScanAlwaysAvailable = " + mWifiManager.isScanAlwaysAvailable());
                boolean mWifiSleepConfig = SystemProperties.getBoolean("ro.wifi.sleep.power.down", false);
                boolean cts_running = SystemProperties.getBoolean("cts_gts.status", false);
                if(!cts_running && mWifiSleepConfig && mWifiManager.isScanAlwaysAvailable()) {
                    mWifiManager.setScanAlwaysAvailable(false);
                }
                mBtIsOpened = getBtIsEnabled();
                mWifiIsOpened = getWifiIsEnabled();
                if (shouldStartWifiSleep()) {
                    setWifiSleepAlarm();
                }else if(shouldStartBtSleep()){//check if only bt
                    setWifiSleepAlarm();
                }
            } else if (action.contains(ACTION_WIFI_SLEEP_TIMEOUT_ALARM)) {
                //Sometimes we receive this action after SCREEN_ON.
                //Turn off wifi should only happen when SCREEN_OFF.
                if(!mIsScreenON) {
                    if(mWifiIsOpened) {
                        setWifiEnabled(false);
                        SystemProperties.set(PERSISTENT_PROPERTY_WIFI_SLEEP_FLAG, "true");
                    }
                    if(mBtIsOpened && mBtPowerDownSetting){
                        setBtEnabled(false);
                        SystemProperties.set(PERSISTENT_PROPERTY_BT_SLEEP_FLAG, "true");;
                    }
                }
            } else if (action.equals(Intent.ACTION_BOOT_COMPLETED)) {
                boolean mWifiSleepFlag = SystemProperties.getBoolean(PERSISTENT_PROPERTY_WIFI_SLEEP_FLAG, false);
                log("mWifiSleepFlag is " + mWifiSleepFlag);
                if(mWifiSleepFlag && !getWifiIsEnabled()) {
                    setWifiEnabled(true);
                }
                boolean mBtSleepFlag = SystemProperties.getBoolean(PERSISTENT_PROPERTY_BT_SLEEP_FLAG, false);
                log("mBtSleepFlag is " + mBtSleepFlag);
                if(mBtSleepFlag && !getBtIsEnabled()) {
                    setBtEnabled(true);
                }
            }
        }
    }

    private void updateChargingState(Intent batteryChangedIntent) {
        final int status = batteryChangedIntent.getIntExtra(BatteryManager.EXTRA_STATUS,
                BatteryManager.BATTERY_STATUS_UNKNOWN);
        boolean charging = status == BatteryManager.BATTERY_STATUS_FULL ||
                    status == BatteryManager.BATTERY_STATUS_CHARGING;
	log("updateChargingState, mCharging: " + charging);
        if (mCharging != charging) {
            log("updateChargingState, mCharging: " + charging);
            //no need update sleep state right now, because charge
            //state changed will cause SCREEN_ON
            mCharging = charging;
        }
    }

    private int wifiSleepPolicyMode() {
	return Settings.Global.getInt(mContext.getContentResolver(),
			Settings.Global.WIFI_SLEEP_POLICY,
			Settings.Global.WIFI_SLEEP_POLICY_NEVER);
    }
    private boolean shouldStartWifiSleep() {
	log("shouldStartWifiSleep: isWifiOpen = " + mWifiIsOpened);
	if(mWifiIsOpened) {
		if(wifiSleepPolicyMode() == Settings.Global.WIFI_SLEEP_POLICY_DEFAULT) {
			return true;
		} else if(wifiSleepPolicyMode() == Settings.Global.WIFI_SLEEP_POLICY_NEVER_WHILE_PLUGGED) {
			if(!mCharging){
				return true;
			}
		}
	}
        return false;
    }

    private boolean getWifiIsEnabled() {
	final ContentResolver cr = mContext.getContentResolver();
	return Settings.Global.getInt(cr, Settings.Global.WIFI_ON, WIFI_DISABLED) == WIFI_ENABLED
	       || Settings.Global.getInt(cr, Settings.Global.WIFI_ON, WIFI_DISABLED) == WIFI_ENABLED_AIRPLANE_OVERRIDE;
    }

    private void setWifiSleepAlarm() {
	long delayMs = SystemProperties.getInt(PERSISTENT_PROPERTY_WIFI_SLEEP_DELAY, DEFAULT_WIFI_SLEEP_DELAY_MS);
        Intent intent = new Intent(ACTION_WIFI_SLEEP_TIMEOUT_ALARM);
        mWifiSleepIntent = PendingIntent.getBroadcast(mContext, 0, intent,
                PendingIntent.FLAG_UPDATE_CURRENT);
        mAlarmManager.set(AlarmManager.RTC_WAKEUP,
                System.currentTimeMillis() + delayMs, mWifiSleepIntent);
        log("setWifiSleepAlarm: set alarm :" + delayMs + "ms");
    }

    private void setWifiEnabled(boolean enable) {
	log("setWifiEnabled " + enable);
	if (!mWifiWakeLock.isHeld()) {
		log("---- mWifiWakeLock.acquire ----");
		mWifiWakeLock.acquire();
	}
	mWifiManager.setWifiEnabled(enable);
	if (mWifiWakeLock.isHeld()) {
		try {
			Thread.sleep(2000);
		} catch (InterruptedException ignore) {
		}
		log("---- mWifiWakeLock.release ----");
		mWifiWakeLock.release();
	}
    }

    private void exitSleepState() {
        log("exitSleepState()");
        if (mWifiSleepIntent != null) {
            if(mWifiIsOpened && !getWifiIsEnabled()){
                setWifiEnabled(true);
            }
            if(mBtIsOpened && !getBtIsEnabled()){
                setBtEnabled(true);
            }
            removeWifiSleepAlarm();
        }
    }

    private void removeWifiSleepAlarm() {
        log("removeWifiSleepAlarm...");
        if (mWifiSleepIntent != null) {
            mAlarmManager.cancel(mWifiSleepIntent);
            mWifiSleepIntent = null;
        }
    }


    private boolean shouldStartBtSleep() {
        log("shouldStartBtSleep: isBtOpen = " + mBtIsOpened);
        if(mBtIsOpened && mBtPowerDownSetting) {
            if(wifiSleepPolicyMode() == Settings.Global.WIFI_SLEEP_POLICY_DEFAULT) {
                return true;
            } else if(wifiSleepPolicyMode() == Settings.Global.WIFI_SLEEP_POLICY_NEVER_WHILE_PLUGGED) {
                if(!mCharging){
                    return true;
                }
            }
        }
        return false;
    }

    private boolean getBtIsEnabled() {
        final ContentResolver cr = mContext.getContentResolver();
        return Settings.Global.getInt(cr, Settings.Global.BLUETOOTH_ON, 0) == BT_ENABLED
	       || Settings.Global.getInt(cr, Settings.Global.BLUETOOTH_ON, 0) == BT_ENABLED_AIRPLANE_OVERRIDE;
    }

    private void setBtEnabled(boolean enable){
		log("setBtEnabled " + enable);
        BluetoothAdapter mAdapter= BluetoothAdapter.getDefaultAdapter();
        if(enable)
            mAdapter.enable();
        else
            mAdapter.disable();
    }


    private static void log(String s) {
        Log.d(LOG_TAG, s);
    }
}

