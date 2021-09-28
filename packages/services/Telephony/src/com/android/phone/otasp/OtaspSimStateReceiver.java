/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.phone.otasp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncResult;
import android.os.Handler;
import android.os.Message;
import android.os.PersistableBundle;
import android.telephony.CarrierConfigManager;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.internal.telephony.Phone;
import com.android.phone.PhoneGlobals;

public class OtaspSimStateReceiver extends BroadcastReceiver {
    private static final String TAG = OtaspSimStateReceiver.class.getSimpleName();
    private static final boolean DBG = true;
    private Context mContext;

    private static final int EVENT_OTASP_CHANGED = 1;

    private Handler mOtaspHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            AsyncResult ar;
            switch (msg.what) {
                case EVENT_OTASP_CHANGED:
                    ar = (AsyncResult) msg.obj;
                    if (ar.exception == null && ar.result != null) {
                        int otaspMode = (Integer) ar.result;
                        logd("EVENT_OTASP_CHANGED: otaspMode=" + otaspMode);
                        if (otaspMode == TelephonyManager.OTASP_NEEDED) {
                            logd("otasp activation required, start otaspActivationService");
                            mContext.startService(
                                    new Intent(mContext, OtaspActivationService.class));
                        } else if (otaspMode == TelephonyManager.OTASP_NOT_NEEDED) {
                            OtaspActivationService.updateActivationState(mContext, true);
                        }
                    } else {
                        logd("EVENT_OTASP_CHANGED: exception=" + ar.exception);
                    }
                    break;
                default:
                    super.handleMessage(msg);
                    break;
            }
        }
    };

    /**
     * check if OTA service provisioning activation is supported by the current carrier
     * @return true if otasp activation is needed, false otherwise
     */
    private static boolean isCarrierSupported() {
        final Phone phone = PhoneGlobals.getPhone();
        final Context context = phone.getContext();
        if (context != null) {
            PersistableBundle b = null;
            final CarrierConfigManager configManager = (CarrierConfigManager) context
                    .getSystemService(Context.CARRIER_CONFIG_SERVICE);
            if (configManager != null) {
                b = configManager.getConfig();
            }
            if (b != null && b.getBoolean(
                    CarrierConfigManager.KEY_USE_OTASP_FOR_PROVISIONING_BOOL)) {
                return true;
            }
        }
        logd("otasp activation not needed: no supported carrier");
        return false;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        mContext = context;
        if(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED.equals(intent.getAction())) {
            if (DBG) logd("Received intent: " + intent.getAction());
            if (PhoneGlobals.getPhone().getIccRecordsLoaded() && isCarrierSupported()) {
                registerOtaspChangedHandler();
            }
        }
    }

    // It's fine to call mutiple times, as the registrants are de-duped by Handler object.
    private void registerOtaspChangedHandler() {
        final Phone phone = PhoneGlobals.getPhone();
        phone.registerForOtaspChange(mOtaspHandler, EVENT_OTASP_CHANGED, null);
    }

    private static void logd(String s) {
        Log.d(TAG, s);
    }
}

