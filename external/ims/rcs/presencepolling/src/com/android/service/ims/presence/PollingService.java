/*
 * Copyright (c) 2015, Motorola Mobility LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     - Neither the name of Motorola Mobility nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY LLC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

package com.android.service.ims.presence;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.net.Uri;
import android.provider.Telephony;
import android.telephony.CarrierConfigManager;
import android.os.IBinder;
import android.os.PersistableBundle;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.telephony.SubscriptionManager;
import android.telephony.ims.ImsManager;
import android.telephony.ims.ImsRcsManager;
import android.util.Log;

import com.android.ims.internal.Logger;
import com.android.internal.annotations.VisibleForTesting;

/**
 * Manages the CapabilityPolling class. Starts capability polling when a SIM card is inserted that
 * supports RCS Presence Capability Polling and stops the service otherwise.
 */
public class PollingService extends Service {

    private static final Uri UCE_URI = Uri.withAppendedPath(Telephony.SimInfo.CONTENT_URI,
            Telephony.SimInfo.COLUMN_IMS_RCS_UCE_ENABLED);

    private Logger logger = Logger.getLogger(this.getClass().getName());

    private CapabilityPolling mCapabilityPolling = null;
    private int mDefaultSubId = SubscriptionManager.INVALID_SUBSCRIPTION_ID;

    // not final for testing
    private BroadcastReceiver mReceiver = new BroadcastReceiver() {

        @Override
        public void onReceive(Context context, Intent intent) {
            switch (intent.getAction()) {
                case CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED: {
                    checkAndUpdateCapabilityPollStatus();
                    break;
                }
                case SubscriptionManager.ACTION_DEFAULT_SUBSCRIPTION_CHANGED: {
                    handleDefaultSubChanged();
                }
            }
        }
    };

    private ContentObserver mUceSettingObserver;

    private SubscriptionManager.OnSubscriptionsChangedListener mSubChangedListener =
            new SubscriptionManager.OnSubscriptionsChangedListener() {
                @Override
                public void onSubscriptionsChanged() {
                    handleDefaultSubChanged();
                }
            };

    /**
     * Constructor
     */
    public PollingService() {
        logger.debug("PollingService()");
    }

    @Override
    public void onCreate() {
        logger.debug("onCreate()");
        PresenceSetting.init(this);

        registerBroadcastReceiver();
        SubscriptionManager subscriptionManager = getSystemService(SubscriptionManager.class);
        // This will always call onSubscriptionsChanged when it is added, which will possibly
        // start polling.
        subscriptionManager.addOnSubscriptionsChangedListener(getMainExecutor(),
                mSubChangedListener);
        mUceSettingObserver = new ContentObserver(getMainThreadHandler()) {
            @Override
            public void onChange(boolean selfChange) {
                onChange(selfChange, null /*uri*/);
            }

            @Override
            public void onChange(boolean selfChange, Uri uri) {
                logger.print("UCE setting changed, re-evaluating poll service.");
                checkAndUpdateCapabilityPollStatus();
            }
        };
        getContentResolver().registerContentObserver(UCE_URI, true /*notifyForDescendants*/,
                mUceSettingObserver);
    }

    /**
      * Cleans up when the service is destroyed
      */
    @Override
    public void onDestroy() {
        logger.debug("onDestroy()");

        if (mCapabilityPolling != null) {
            mCapabilityPolling.stop();
            mCapabilityPolling = null;
        }
        SubscriptionManager subscriptionManager = getSystemService(SubscriptionManager.class);
        subscriptionManager.removeOnSubscriptionsChangedListener(mSubChangedListener);
        unregisterReceiver(mReceiver);
        if (mUceSettingObserver != null) {
            getContentResolver().unregisterContentObserver(mUceSettingObserver);
        }

        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        logger.debug("onBind(), intent: " + intent);
        return null;
    }

    @VisibleForTesting
    public void setBroadcastReceiver(BroadcastReceiver r) {
        mReceiver = r;
    }

    private void registerBroadcastReceiver() {
        IntentFilter filter = new IntentFilter(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED);
        filter.addAction(SubscriptionManager.ACTION_DEFAULT_SUBSCRIPTION_CHANGED);
        registerReceiver(mReceiver, filter);
    }

    private void handleDefaultSubChanged() {
        int defaultSubId = PresenceSetting.getDefaultSubscriptionId();
        if (!SubscriptionManager.isValidSubscriptionId(defaultSubId)) {
            return;
        }
        // First, check to see if there is no default set, but there is a saved default sub id.
        // this can happen when the phone is rebooted. If the SIM card stays the same, we do not
        // want to clear the EAB contact cache and requery every time the phone reboots.
        if (!SubscriptionManager.isValidSubscriptionId(mDefaultSubId)) {
            int eabContactSubId = SharedPrefUtil.getLastUsedSubscriptionId(this);
            if (SubscriptionManager.isValidSubscriptionId(mDefaultSubId)) {
                mDefaultSubId = eabContactSubId;
            }
        }
        if (mDefaultSubId == defaultSubId) {
            return;
        }
        mDefaultSubId = defaultSubId;
        logger.print("handleDefaultSubChanged: new default= " + defaultSubId);
        SharedPrefUtil.saveLastUsedSubscriptionId(this, mDefaultSubId);

        if (mCapabilityPolling != null) {
            mCapabilityPolling.handleDefaultSubscriptionChanged(defaultSubId);
        }

        checkAndUpdateCapabilityPollStatus();
    }

    private void checkAndUpdateCapabilityPollStatus() {
        // If the carrier doesn't support RCS Presence, stop polling.
        boolean carrierSupport = isRcsSupportedByCarrier();
        boolean userEnabled = hasUserEnabledUce();
        logger.print("RCS carrier support = " + carrierSupport + ", user enabled = "
                + userEnabled);
        if (!carrierSupport || !userEnabled) {
            logger.print("RCS UCE Not supported, Stopping CapabilityPolling");
            if (mCapabilityPolling != null) {
                mCapabilityPolling.stop();
                mCapabilityPolling = null;
            }
            return;
        }
        // Carrier supports, so start the service if it hasn't already been started.
        if (mCapabilityPolling == null) {
            logger.info("Starting CapabilityPolling...");
            mCapabilityPolling = CapabilityPolling.getInstance(this);
            mCapabilityPolling.start();
        }
    }

    private boolean hasUserEnabledUce() {
        ImsManager manager = getSystemService(ImsManager.class);
        if (manager == null) {
            logger.error("hasUserEnabledUce: manager not available.");
            return false;
        }
        try {
            ImsRcsManager rcsManager = manager.getImsRcsManager(mDefaultSubId);
            return (rcsManager != null) && rcsManager.getUceAdapter().isUceSettingEnabled();
        } catch (Exception e) {
            logger.error("hasUserEnabledUce: exception = " + e.getMessage());
        }
        return false;
    }

    private boolean isRcsSupportedByCarrier() {
        CarrierConfigManager configManager = getSystemService(CarrierConfigManager.class);
        if (configManager != null) {
            PersistableBundle b = configManager.getConfigForSubId(mDefaultSubId);
            if (b != null) {
                return b.getBoolean(CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL, false);
            }
        }
        return true;
    }

    static boolean isRcsSupportedByDevice() {
        String rcsSupported = SystemProperties.get("persist.rcs.supported");
        return "1".equals(rcsSupported);
    }
}
