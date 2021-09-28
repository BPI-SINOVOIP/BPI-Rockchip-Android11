/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.services.telephony.rcs;

import android.annotation.AnyThread;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.AsyncResult;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.util.Log;
import android.util.SparseArray;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.telephony.PhoneConfigurationManager;
import com.android.internal.util.IndentingPrintWriter;

import java.io.FileDescriptor;
import java.io.PrintWriter;

/**
 * Singleton service setup to manage RCS related services that the platform provides such as User
 * Capability Exchange.
 */
@AnyThread
public class TelephonyRcsService {

    private static final String LOG_TAG = "TelephonyRcsService";

    /**
     * Used to inject RcsFeatureController and UserCapabilityExchangeImpl instances for testing.
     */
    @VisibleForTesting
    public interface FeatureFactory {
        /**
         * @return an {@link RcsFeatureController} assoicated with the slot specified.
         */
        RcsFeatureController createController(Context context, int slotId);

        /**
         * @return an instance of {@link UserCapabilityExchangeImpl} associated with the slot
         * specified.
         */
        UserCapabilityExchangeImpl createUserCapabilityExchange(Context context, int slotId,
                int subId);
    }

    private FeatureFactory mFeatureFactory = new FeatureFactory() {
        @Override
        public RcsFeatureController createController(Context context, int slotId) {
            return new RcsFeatureController(context, slotId);
        }

        @Override
        public UserCapabilityExchangeImpl createUserCapabilityExchange(Context context, int slotId,
                int subId) {
            return new UserCapabilityExchangeImpl(context, slotId, subId);
        }
    };

    // Notifies this service that there has been a change in available slots.
    private static final int HANDLER_MSIM_CONFIGURATION_CHANGE = 1;

    private final Context mContext;
    private final Object mLock = new Object();
    private int mNumSlots;

    // Maps slot ID -> RcsFeatureController.
    private SparseArray<RcsFeatureController> mFeatureControllers;

    private BroadcastReceiver mCarrierConfigChangedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent == null) {
                return;
            }
            if (CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED.equals(intent.getAction())) {
                Bundle bundle = intent.getExtras();
                if (bundle == null) {
                    return;
                }
                int slotId = bundle.getInt(CarrierConfigManager.EXTRA_SLOT_INDEX,
                        SubscriptionManager.INVALID_PHONE_INDEX);
                int subId = bundle.getInt(CarrierConfigManager.EXTRA_SUBSCRIPTION_INDEX,
                        SubscriptionManager.INVALID_SUBSCRIPTION_ID);
                updateFeatureControllerSubscription(slotId, subId);
            }
        }
    };

    private Handler mHandler = new Handler(Looper.getMainLooper(), (msg) -> {
        switch (msg.what) {
            case HANDLER_MSIM_CONFIGURATION_CHANGE: {
                AsyncResult result = (AsyncResult) msg.obj;
                Integer numSlots = (Integer) result.result;
                if (numSlots == null) {
                    Log.w(LOG_TAG, "msim config change with null num slots.");
                    break;
                }
                updateFeatureControllerSize(numSlots);
                break;
            }
            default:
                return false;
        }
        return true;
    });

    public TelephonyRcsService(Context context, int numSlots) {
        mContext = context;
        mNumSlots = numSlots;
        mFeatureControllers = new SparseArray<>(numSlots);
    }

    /**
     * @return the {@link RcsFeatureController} associated with the given slot.
     */
    public RcsFeatureController getFeatureController(int slotId) {
        synchronized (mLock) {
            return mFeatureControllers.get(slotId);
        }
    }

    /**
     * Called after instance creation to initialize internal structures as well as register for
     * system callbacks.
     */
    public void initialize() {
        updateFeatureControllerSize(mNumSlots);

        PhoneConfigurationManager.registerForMultiSimConfigChange(mHandler,
                HANDLER_MSIM_CONFIGURATION_CHANGE, null);
        mContext.registerReceiver(mCarrierConfigChangedReceiver, new IntentFilter(
                CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));
    }

    @VisibleForTesting
    public void setFeatureFactory(FeatureFactory f) {
        mFeatureFactory = f;
    }

    /**
     * Update the number of {@link RcsFeatureController}s that are created based on the number of
     * active slots on the device.
     */
    @VisibleForTesting
    public void updateFeatureControllerSize(int newNumSlots) {
        synchronized (mLock) {
            int oldNumSlots = mFeatureControllers.size();
            if (oldNumSlots == newNumSlots) {
                return;
            }
            Log.i(LOG_TAG, "updateFeatureControllers: oldSlots=" + oldNumSlots + ", newNumSlots="
                    + newNumSlots);
            mNumSlots = newNumSlots;
            if (oldNumSlots < newNumSlots) {
                for (int i = oldNumSlots; i < newNumSlots; i++) {
                    RcsFeatureController c = constructFeatureController(i);
                    // Do not add feature controllers for inactive subscriptions
                    if (c.hasActiveFeatures()) {
                        mFeatureControllers.put(i, c);
                    }
                }
            } else {
                for (int i = (oldNumSlots - 1); i > (newNumSlots - 1); i--) {
                    RcsFeatureController c = mFeatureControllers.get(i);
                    if (c != null) {
                        mFeatureControllers.remove(i);
                        c.destroy();
                    }
                }
            }
        }
    }

    private void updateFeatureControllerSubscription(int slotId, int newSubId) {
        synchronized (mLock) {
            RcsFeatureController f = mFeatureControllers.get(slotId);
            Log.i(LOG_TAG, "updateFeatureControllerSubscription: slotId=" + slotId + " newSubId="
                    + newSubId + ", existing feature=" + (f != null));
            if (SubscriptionManager.isValidSubscriptionId(newSubId)) {
                if (f == null) {
                    // A controller doesn't exist for this slot yet.
                    f = mFeatureFactory.createController(mContext, slotId);
                    updateSupportedFeatures(f, slotId, newSubId);
                    if (f.hasActiveFeatures()) mFeatureControllers.put(slotId, f);
                } else {
                    updateSupportedFeatures(f, slotId, newSubId);
                    // Do not keep an empty container around.
                    if (!f.hasActiveFeatures()) {
                        f.destroy();
                        mFeatureControllers.remove(slotId);
                    }
                }
            }
            if (f != null) f.updateAssociatedSubscription(newSubId);
        }
    }

    private RcsFeatureController constructFeatureController(int slotId) {
        RcsFeatureController c = mFeatureFactory.createController(mContext, slotId);
        int subId = getSubscriptionFromSlot(slotId);
        updateSupportedFeatures(c, slotId, subId);
        return c;
    }

    private void updateSupportedFeatures(RcsFeatureController c, int slotId, int subId) {
        if (doesSubscriptionSupportPresence(subId)) {
            if (c.getFeature(UserCapabilityExchangeImpl.class) == null) {
                c.addFeature(mFeatureFactory.createUserCapabilityExchange(mContext, slotId, subId),
                        UserCapabilityExchangeImpl.class);
            }
        } else {
            if (c.getFeature(UserCapabilityExchangeImpl.class) != null) {
                c.removeFeature(UserCapabilityExchangeImpl.class);
            }
        }
        // Only start the connection procedure if we have active features.
        if (c.hasActiveFeatures()) c.connect();
    }

    private boolean doesSubscriptionSupportPresence(int subId) {
        if (!SubscriptionManager.isValidSubscriptionId(subId)) return false;
        CarrierConfigManager carrierConfigManager =
                mContext.getSystemService(CarrierConfigManager.class);
        if (carrierConfigManager == null) return false;
        boolean supportsUce = carrierConfigManager.getConfigForSubId(subId).getBoolean(
                CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL);
        supportsUce |= carrierConfigManager.getConfigForSubId(subId).getBoolean(
                CarrierConfigManager.KEY_USE_RCS_SIP_OPTIONS_BOOL);
        return supportsUce;
    }


    private int getSubscriptionFromSlot(int slotId) {
        SubscriptionManager manager = mContext.getSystemService(SubscriptionManager.class);
        if (manager == null) {
            Log.w(LOG_TAG, "Couldn't find SubscriptionManager for slotId=" + slotId);
            return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        }
        int[] subIds = manager.getSubscriptionIds(slotId);
        if (subIds != null && subIds.length > 0) {
            return subIds[0];
        }
        return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
    }

    /**
     * Dump this instance into a readable format for dumpsys usage.
     */
    public void dump(FileDescriptor fd, PrintWriter printWriter, String[] args) {
        IndentingPrintWriter pw = new IndentingPrintWriter(printWriter, "  ");
        pw.println("RcsFeatureControllers:");
        pw.increaseIndent();
        synchronized (mLock) {
            for (int i = 0; i < mNumSlots; i++) {
                RcsFeatureController f = mFeatureControllers.get(i);
                pw.increaseIndent();
                f.dump(fd, printWriter, args);
                pw.decreaseIndent();
            }
        }
        pw.decreaseIndent();
    }
}
