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
import android.content.Context;
import android.net.Uri;
import android.telephony.ims.ImsException;
import android.telephony.ims.ImsRcsManager;
import android.telephony.ims.ImsReasonInfo;
import android.telephony.ims.RegistrationManager;
import android.telephony.ims.aidl.IImsCapabilityCallback;
import android.telephony.ims.aidl.IImsRegistrationCallback;
import android.telephony.ims.stub.ImsRegistrationImplBase;
import android.util.ArrayMap;
import android.util.Log;

import com.android.ims.FeatureConnector;
import com.android.ims.IFeatureConnector;
import com.android.ims.RcsFeatureManager;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.telephony.imsphone.ImsRegistrationCallbackHelper;
import com.android.internal.util.IndentingPrintWriter;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.function.Consumer;

/**
 * Contains the RCS feature implementations that are associated with this slot's RcsFeature.
 */
@AnyThread
public class RcsFeatureController {
    private static final String LOG_TAG = "RcsFeatureController";

    /**
     * Interface used by RCS features that need to listen for when the associated service has been
     * connected.
     */
    public interface Feature {
        /**
         * The RcsFeature has been connected to the framework and is ready.
         */
        void onRcsConnected(RcsFeatureManager manager);

        /**
         * The framework has lost the binding to the RcsFeature or it is in the process of changing.
         */
        void onRcsDisconnected();

        /**
         * The subscription associated with the slot this controller is bound to has changed or its
         * carrier configuration has changed.
         */
        void onAssociatedSubscriptionUpdated(int subId);

        /**
         * Called when the feature should be destroyed.
         */
        void onDestroy();
    }

    /**
     * Used to inject FeatureConnector instances for testing.
     */
    @VisibleForTesting
    public interface FeatureConnectorFactory<T extends IFeatureConnector> {
        /**
         * @return a {@link FeatureConnector} associated for the given {@link IFeatureConnector}
         * and slot id.
         */
        FeatureConnector<T> create(Context context, int slotId,
                FeatureConnector.Listener<T> listener, Executor executor, String tag);
    }

    /**
     * Used to inject ImsRegistrationCallbackHelper instances for testing.
     */
    @VisibleForTesting
    public interface RegistrationHelperFactory {
        /**
         * @return an {@link ImsRegistrationCallbackHelper}, which helps manage IMS registration
         * state.
         */
        ImsRegistrationCallbackHelper create(
                ImsRegistrationCallbackHelper.ImsRegistrationUpdate cb, Executor executor);
    }

    private FeatureConnectorFactory<RcsFeatureManager> mFeatureFactory = FeatureConnector::new;
    private RegistrationHelperFactory mRegistrationHelperFactory =
            ImsRegistrationCallbackHelper::new;

    private final Map<Class<?>, Feature> mFeatures = new ArrayMap<>();
    private final Context mContext;
    private final ImsRegistrationCallbackHelper mImsRcsRegistrationHelper;
    private final int mSlotId;
    private final Object mLock = new Object();
    private FeatureConnector<RcsFeatureManager> mFeatureConnector;
    private RcsFeatureManager mFeatureManager;

    private FeatureConnector.Listener<RcsFeatureManager> mFeatureConnectorListener =
            new FeatureConnector.Listener<RcsFeatureManager>() {
                @Override
                public RcsFeatureManager getFeatureManager() {
                    return new RcsFeatureManager(mContext, mSlotId);
                }

                @Override
                public void connectionReady(RcsFeatureManager manager)
                        throws com.android.ims.ImsException {
                    if (manager == null) {
                        logw("connectionReady returned null RcsFeatureManager");
                        return;
                    }
                    try {
                        // May throw ImsException if for some reason the connection to the
                        // ImsService is gone.
                        setupConnectionToService(manager);
                    } catch (ImsException e) {
                        // Use deprecated Exception for compatibility.
                        throw new com.android.ims.ImsException(e.getMessage(),
                                ImsReasonInfo.CODE_LOCAL_IMS_SERVICE_DOWN);
                    }
                    updateConnectionStatus(manager);
                }

                @Override
                public void connectionUnavailable() {
                    // Call before disabling connection to manager.
                    removeConnectionToService();
                    updateConnectionStatus(null /*manager*/);
                }
            };

    private ImsRegistrationCallbackHelper.ImsRegistrationUpdate mRcsRegistrationUpdate = new
            ImsRegistrationCallbackHelper.ImsRegistrationUpdate() {
                @Override
                public void handleImsRegistered(int imsRadioTech) {
                }

                @Override
                public void handleImsRegistering(int imsRadioTech) {
                }

                @Override
                public void handleImsUnregistered(ImsReasonInfo imsReasonInfo) {
                }

                @Override
                public void handleImsSubscriberAssociatedUriChanged(Uri[] uris) {
                }
            };

    public RcsFeatureController(Context context, int slotId) {
        mContext = context;
        mSlotId = slotId;
        mImsRcsRegistrationHelper = mRegistrationHelperFactory.create(mRcsRegistrationUpdate,
                mContext.getMainExecutor());
    }

    /**
     * Should only be used to inject registration helpers for testing.
     */
    @VisibleForTesting
    public RcsFeatureController(Context context, int slotId, RegistrationHelperFactory f) {
        mContext = context;
        mSlotId = slotId;
        mRegistrationHelperFactory = f;
        mImsRcsRegistrationHelper = mRegistrationHelperFactory.create(mRcsRegistrationUpdate,
                mContext.getMainExecutor());
    }

    /**
     * This method should be called after constructing an instance of this class to start the
     * connection process to the associated RcsFeature.
     */
    public void connect() {
        synchronized (mLock) {
            if (mFeatureConnector != null) return;
            mFeatureConnector = mFeatureFactory.create(mContext, mSlotId, mFeatureConnectorListener,
                    mContext.getMainExecutor(), LOG_TAG);
            mFeatureConnector.connect();
        }
    }

    /**
     * Adds a {@link Feature} to be tracked by this FeatureController.
     */
    public <T extends Feature> void addFeature(T connector, Class<T> clazz) {
        synchronized (mLock) {
            mFeatures.put(clazz, connector);
        }
        RcsFeatureManager manager = getFeatureManager();
        if (manager != null) {
            connector.onRcsConnected(manager);
        } else {
            connector.onRcsDisconnected();
        }
    }

    /**
     * @return The RCS feature implementation tracked by this controller.
     */
    @SuppressWarnings("unchecked")
    public <T> T getFeature(Class<T> clazz) {
        synchronized (mLock) {
            return (T) mFeatures.get(clazz);
        }
    }

    /**
     * Removes the feature associated with this class.
     */
    public <T> void removeFeature(Class<T> clazz) {
        synchronized (mLock) {
            RcsFeatureController.Feature feature = mFeatures.remove(clazz);
            feature.onDestroy();
        }
    }

    /**
     * @return true if this controller has features it is actively tracking.
     */
    public boolean hasActiveFeatures() {
        synchronized (mLock) {
            return mFeatures.size() > 0;
        }
    }

    /**
     * Update the subscription associated with this controller.
     */
    public void updateAssociatedSubscription(int newSubId) {
        RcsFeatureManager manager = getFeatureManager();
        if (manager != null) {
            try {
                manager.updateCapabilities();
            } catch (ImsException e) {
                Log.w(LOG_TAG, "associatedSubscriptionChanged failed:" + e);
            }
        }
        synchronized (mLock) {
            for (Feature c : mFeatures.values()) {
                c.onAssociatedSubscriptionUpdated(newSubId);
            }
        }
    }

    /**
     * Call before this controller is destroyed to tear down associated features.
     */
    public void destroy() {
        synchronized (mLock) {
            Log.i(LOG_TAG, "destroy: slotId=" + mSlotId);
            if (mFeatureConnector != null) {
                mFeatureConnector.disconnect();
            }
            for (Feature c : mFeatures.values()) {
                c.onRcsDisconnected();
                c.onDestroy();
            }
            mFeatures.clear();
        }
    }

    @VisibleForTesting
    public void setFeatureConnectorFactory(FeatureConnectorFactory factory) {
        mFeatureFactory = factory;
    }

    /**
     * Add a {@link RegistrationManager.RegistrationCallback} callback that gets called when IMS
     * registration has changed for a specific subscription.
     */
    public void registerImsRegistrationCallback(int subId, IImsRegistrationCallback callback)
            throws ImsException {
        RcsFeatureManager manager = getFeatureManager();
        if (manager == null) {
            throw new ImsException("Service is not available",
                    ImsException.CODE_ERROR_SERVICE_UNAVAILABLE);
        }
        manager.registerImsRegistrationCallback(subId, callback);
    }

    /**
     * Removes a previously registered {@link RegistrationManager.RegistrationCallback} callback
     * that is associated with a specific subscription.
     */
    public void unregisterImsRegistrationCallback(int subId, IImsRegistrationCallback callback) {
        RcsFeatureManager manager = getFeatureManager();
        if (manager != null) {
            manager.unregisterImsRegistrationCallback(subId, callback);
        }
    }

    /**
     * Register an {@link ImsRcsManager.AvailabilityCallback} with the associated RcsFeature,
     * which will provide availability updates.
     */
    public void registerRcsAvailabilityCallback(int subId, IImsCapabilityCallback callback)
            throws ImsException {
        RcsFeatureManager manager = getFeatureManager();
        if (manager == null) {
            throw new ImsException("Service is not available",
                    ImsException.CODE_ERROR_SERVICE_UNAVAILABLE);
        }
        manager.registerRcsAvailabilityCallback(subId, callback);
    }

    /**
     * Remove a registered {@link ImsRcsManager.AvailabilityCallback} from the RcsFeature.
     */
    public void unregisterRcsAvailabilityCallback(int subId, IImsCapabilityCallback callback) {
        RcsFeatureManager manager = getFeatureManager();
        if (manager != null) {
            manager.unregisterRcsAvailabilityCallback(subId, callback);
        }
    }

    /**
     * Query for the specific capability.
     */
    public boolean isCapable(int capability, int radioTech)
            throws android.telephony.ims.ImsException {
        RcsFeatureManager manager = getFeatureManager();
        if (manager == null) {
            throw new ImsException("Service is not available",
                    ImsException.CODE_ERROR_SERVICE_UNAVAILABLE);
        }
        return manager.isCapable(capability, radioTech);
    }

    /**
     * Query the availability of an IMS RCS capability.
     */
    public boolean isAvailable(int capability) throws android.telephony.ims.ImsException {
        RcsFeatureManager manager = getFeatureManager();
        if (manager == null) {
            throw new ImsException("Service is not available",
                    ImsException.CODE_ERROR_SERVICE_UNAVAILABLE);
        }
        return manager.isAvailable(capability);
    }

    /**
     * Get the IMS RCS registration technology for this Phone.
     */
    public void getRegistrationTech(Consumer<Integer> callback) {
        RcsFeatureManager manager = getFeatureManager();
        if (manager != null) {
            manager.getImsRegistrationTech(callback);
        }
        callback.accept(ImsRegistrationImplBase.REGISTRATION_TECH_NONE);
    }

    /**
     * Retrieve the current RCS registration state.
     */
    public void getRegistrationState(Consumer<Integer> callback) {
        callback.accept(mImsRcsRegistrationHelper.getImsRegistrationState());
    }

    private void setupConnectionToService(RcsFeatureManager manager) throws ImsException {
        // Open persistent listener connection, sends RcsFeature#onFeatureReady.
        manager.openConnection();
        manager.updateCapabilities();
        manager.registerImsRegistrationCallback(mImsRcsRegistrationHelper.getCallbackBinder());
    }

    private void removeConnectionToService() {
        RcsFeatureManager manager = getFeatureManager();
        if (manager != null) {
            manager.unregisterImsRegistrationCallback(
                    mImsRcsRegistrationHelper.getCallbackBinder());
            // Remove persistent listener connection.
            manager.releaseConnection();
        }
        mImsRcsRegistrationHelper.reset();
    }

    private void updateConnectionStatus(RcsFeatureManager manager) {
        synchronized (mLock) {
            mFeatureManager = manager;
            if (mFeatureManager != null) {
                for (Feature c : mFeatures.values()) {
                    c.onRcsConnected(manager);
                }
            } else {
                for (Feature c : mFeatures.values()) {
                    c.onRcsDisconnected();
                }
            }
        }
    }

    private RcsFeatureManager getFeatureManager() {
        synchronized (mLock) {
            return mFeatureManager;
        }
    }

    /**
     * Dump this controller's instance information for usage in dumpsys.
     */
    public void dump(FileDescriptor fd, PrintWriter printWriter, String[] args) {
        IndentingPrintWriter pw = new IndentingPrintWriter(printWriter, "  ");
        pw.print("slotId=");
        pw.println(mSlotId);
        pw.print("RegistrationState=");
        pw.println(mImsRcsRegistrationHelper.getImsRegistrationState());
        pw.print("connected=");
        synchronized (mLock) {
            pw.println(mFeatureManager != null);
        }
    }

    private void logw(String log) {
        Log.w(LOG_TAG, getLogPrefix().append(log).toString());
    }

    private StringBuilder getLogPrefix() {
        StringBuilder sb = new StringBuilder("[");
        sb.append(mSlotId);
        sb.append("] ");
        return sb;
    }
}
