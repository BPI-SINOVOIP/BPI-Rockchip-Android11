/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.mms.service;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.telephony.CarrierConfigManager;
import android.telephony.SmsManager;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.util.ArrayMap;

import java.util.List;
import java.util.Map;

/**
 * This class manages cached copies of all the MMS configuration for each subscription ID.
 * A subscription ID loosely corresponds to a particular SIM. See the
 * {@link android.telephony.SubscriptionManager} for more details.
 *
 */
public class MmsConfigManager {
    private static volatile MmsConfigManager sInstance = new MmsConfigManager();

    public static MmsConfigManager getInstance() {
        return sInstance;
    }

    // Map the various subIds to their corresponding MmsConfigs.
    private final Map<Integer, Bundle> mSubIdConfigMap = new ArrayMap<Integer, Bundle>();
    private Context mContext;
    private SubscriptionManager mSubscriptionManager;

    /** This receiver listens to ACTION_CARRIER_CONFIG_CHANGED to load the MMS config. */
    private final BroadcastReceiver mReceiver =
            new BroadcastReceiver() {
                public void onReceive(Context context, Intent intent) {
                    LogUtil.i("MmsConfigManager receives ACTION_CARRIER_CONFIG_CHANGED");
                    loadInBackground();
                }
            };

    public void init(final Context context) {
        mContext = context;
        mSubscriptionManager = SubscriptionManager.from(context);
        context.registerReceiver(
                mReceiver, new IntentFilter(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));
        LogUtil.i("MmsConfigManager loads mms config in init()");
        load(context);
    }

    private void loadInBackground() {
        // TODO (ywen) - AsyncTask to avoid creating a new thread?
        new Thread() {
            @Override
            public void run() {
                load(mContext);
            }
        }.start();
    }

    /**
     * Find and return the MMS config for a particular subscription id.
     *
     * @param subId Subscription Id of the desired MMS config bundle
     * @return MMS config bundle for the particular subscription Id. This function can return null
     *         if the MMS config is not loaded yet.
     */
    public Bundle getMmsConfigBySubId(int subId) {
        Bundle mmsConfig;
        synchronized(mSubIdConfigMap) {
            mmsConfig = mSubIdConfigMap.get(subId);
        }
        LogUtil.i("mms config for sub " + subId + ": " + mmsConfig);
        // Return a copy so that callers can mutate it.
        if (mmsConfig != null) {
          return new Bundle(mmsConfig);
        }
        return null;
    }

    /**
     * This loads the MMS config for each active subscription.
     *
     * MMS config is fetched from SmsManager#getCarrierConfigValues(). The resulting bundles are
     * stored in mSubIdConfigMap.
     */
    private void load(Context context) {
        List<SubscriptionInfo> subs = mSubscriptionManager.getActiveSubscriptionInfoList();
        if (subs == null || subs.size() < 1) {
            LogUtil.e(" Failed to load mms config: empty getActiveSubInfoList");
            return;
        }
        // Load all the config bundles into a new map and then swap it with the real map to avoid
        // blocking.
        final Map<Integer, Bundle> newConfigMap = new ArrayMap<Integer, Bundle>();
        for (SubscriptionInfo sub : subs) {
            final int subId = sub.getSubscriptionId();
            LogUtil.i("MmsConfigManager loads mms config for "
                    + sub.getMccString() + "/" +  sub.getMncString()
                    + ", CarrierId " + sub.getCarrierId());
            newConfigMap.put(
                    subId,
                    SmsManager.getSmsManagerForSubscriptionId(subId).getCarrierConfigValues());
        }
        synchronized(mSubIdConfigMap) {
            mSubIdConfigMap.clear();
            mSubIdConfigMap.putAll(newConfigMap);
        }
    }

}
