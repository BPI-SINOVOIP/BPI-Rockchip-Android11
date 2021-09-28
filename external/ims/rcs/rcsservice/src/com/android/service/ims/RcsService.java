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

package com.android.service.ims;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.net.ConnectivityManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.provider.Settings;
import android.provider.Telephony;
import android.telecom.TelecomManager;
import android.telephony.AccessNetworkConstants;
import android.telephony.SubscriptionManager;
import android.telephony.ims.ImsException;
import android.telephony.ims.ImsMmTelManager;
import android.telephony.ims.ImsReasonInfo;
import android.telephony.ims.ProvisioningManager;
import android.telephony.ims.RcsContactUceCapability;
import android.telephony.ims.RegistrationManager;
import android.telephony.ims.feature.MmTelFeature;

import com.android.ims.IRcsPresenceListener;
import com.android.ims.RcsPresenceInfo;
import com.android.ims.ResultCode;
import com.android.ims.RcsPresence;
import com.android.ims.internal.IRcsPresence;
import com.android.ims.internal.IRcsService;
import com.android.ims.internal.Logger;
import com.android.service.ims.R;
import com.android.service.ims.presence.ContactCapabilityResponse;
import com.android.service.ims.presence.PresenceBase;
import com.android.service.ims.presence.PresencePublication;
import com.android.service.ims.presence.PresenceSubscriber;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

public class RcsService extends Service {

    private static final int IMS_SERVICE_RETRY_TIMEOUT_MS = 5000;

    private Logger logger = Logger.getLogger(this.getClass().getName());

    private RcsStackAdaptor mRcsStackAdaptor = null;
    private PresencePublication mPublication = null;
    private PresenceSubscriber mSubscriber = null;

    private Handler mRetryHandler;
    private Runnable mRegisterCallbacks = this::registerImsCallbacksAndSetAssociatedSubscription;
    private int mNetworkRegistrationType = AccessNetworkConstants.TRANSPORT_TYPE_INVALID;

    private int mAssociatedSubscription = SubscriptionManager.INVALID_SUBSCRIPTION_ID;

    private SubscriptionManager.OnSubscriptionsChangedListener mOnSubscriptionsChangedListener
            = new SubscriptionManager.OnSubscriptionsChangedListener() {
        @Override
        public void onSubscriptionsChanged() {
            registerImsCallbacksAndSetAssociatedSubscription();
        }
    };

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            switch (intent.getAction()) {
                case TelecomManager.ACTION_TTY_PREFERRED_MODE_CHANGED: {
                    int newPreferredTtyMode = intent.getIntExtra(
                            TelecomManager.EXTRA_TTY_PREFERRED_MODE,
                            TelecomManager.TTY_MODE_OFF);
                    if (mPublication != null) {
                        mPublication.onTtyPreferredModeChanged(newPreferredTtyMode);
                    }
                    break;
                }
                case Intent.ACTION_AIRPLANE_MODE_CHANGED: {
                    boolean airplaneMode = intent.getBooleanExtra("state", false);
                    if (mPublication != null) {
                        mPublication.onAirplaneModeChanged(airplaneMode);
                    }
                    break;
                }
                case SubscriptionManager.ACTION_DEFAULT_SUBSCRIPTION_CHANGED: {
                    mRetryHandler.removeCallbacks(mRegisterCallbacks);
                    mRetryHandler.post(mRegisterCallbacks);
                }
            }
        }
    };

    private class CapabilityResultListener implements ContactCapabilityResponse {

        private final IRcsPresenceListener mListener;

        public CapabilityResultListener(IRcsPresenceListener listener) {
            mListener = listener;
        }

        @Override
        public void onSuccess(int reqId) {
            try {
                mListener.onSuccess(reqId);
            } catch (RemoteException e) {
                logger.warn("CapabilityResultListener: onSuccess exception = " + e.getMessage());
            }
        }

        @Override
        public void onError(int reqId, int resultCode) {
            try {
                mListener.onError(reqId, resultCode);
            } catch (RemoteException e) {
                logger.warn("CapabilityResultListener: onError exception = " + e.getMessage());
            }
        }

        @Override
        public void onFinish(int reqId) {
            try {
                mListener.onFinish(reqId);
            } catch (RemoteException e) {
                logger.warn("CapabilityResultListener: onFinish exception = " + e.getMessage());
            }
        }

        @Override
        public void onTimeout(int reqId) {
            try {
                mListener.onTimeout(reqId);
            } catch (RemoteException e) {
                logger.warn("CapabilityResultListener: onTimeout exception = " + e.getMessage());
            }
        }

        @Override
        public void onCapabilitiesUpdated(int reqId,
                List<RcsContactUceCapability> contactCapabilities, boolean updateLastTimestamp) {
            ArrayList<RcsPresenceInfo> presenceInfoList = contactCapabilities.stream().map(
                    PresenceInfoParser::getRcsPresenceInfo).collect(
                    Collectors.toCollection(ArrayList::new));

            logger.debug("capabilities updated:");
            for (RcsPresenceInfo info : presenceInfoList) {
                logger.debug("capabilities updated: info -" + info);
            }
            // For some reason it uses an intent to send this info back instead of just using the
            // active binder...
            Intent intent = new Intent(RcsPresence.ACTION_PRESENCE_CHANGED);
            intent.putParcelableArrayListExtra(RcsPresence.EXTRA_PRESENCE_INFO_LIST,
                    presenceInfoList);
            intent.putExtra("updateLastTimestamp", updateLastTimestamp);
            launchPersistService(intent);
        }
    }

    private void launchPersistService(Intent intent) {
        ComponentName component = new ComponentName("com.android.service.ims.presence",
                "com.android.service.ims.presence.PersistService");
        intent.setComponent(component);
        startService(intent);
    }

    @Override
    public void onCreate() {
        super.onCreate();

        logger.debug("RcsService onCreate");

        mRcsStackAdaptor = RcsStackAdaptor.getInstance(this);

        mPublication = new PresencePublication(mRcsStackAdaptor, this,
                getResources().getStringArray(
                        R.array.config_volte_provision_error_on_publish_response),
                getResources().getStringArray(
                        R.array.config_rcs_provision_error_on_publish_response));
        mRcsStackAdaptor.getListener().setPresencePublication(mPublication);

        mSubscriber = new PresenceSubscriber(mRcsStackAdaptor, this,
                getResources().getStringArray(
                        R.array.config_volte_provision_error_on_subscribe_response),
                getResources().getStringArray(
                        R.array.config_rcs_provision_error_on_subscribe_response));
        mRcsStackAdaptor.getListener().setPresenceSubscriber(mSubscriber);
        mPublication.setSubscriber(mSubscriber);

        ConnectivityManager cm = ConnectivityManager.from(this);
        if (cm != null) {
            boolean enabled = Settings.Global.getInt(getContentResolver(),
                    Settings.Global.MOBILE_DATA, 1) == 1;
            logger.debug("Mobile data enabled status: " + (enabled ? "ON" : "OFF"));

            onMobileDataEnabled(enabled);
        }

        // TODO: support MSIM
        ServiceManager.addService("rcs", mBinder);

        mObserver = new MobileDataContentObserver();
        getContentResolver().registerContentObserver(
                Settings.Global.getUriFor(Settings.Global.MOBILE_DATA),
                false, mObserver);

        mSiminfoSettingObserver = new SimInfoContentObserver();
        getContentResolver().registerContentObserver(Telephony.SimInfo.CONTENT_URI, false,
                mSiminfoSettingObserver);

        mRetryHandler = new Handler(Looper.getMainLooper());
        registerBroadcastReceiver();
        registerSubscriptionChangedListener();
    }

    private void registerBroadcastReceiver() {
        IntentFilter filter = new IntentFilter(TelecomManager.ACTION_TTY_PREFERRED_MODE_CHANGED);
        filter.addAction(Intent.ACTION_AIRPLANE_MODE_CHANGED);
        filter.addAction(SubscriptionManager.ACTION_DEFAULT_SUBSCRIPTION_CHANGED);
        registerReceiver(mReceiver, filter);
    }

    private void unregisterBroadcastReceiver() {
        unregisterReceiver(mReceiver);
    }

    private void registerSubscriptionChangedListener() {
        SubscriptionManager subscriptionManager = getSystemService(SubscriptionManager.class);
        if (subscriptionManager != null) {
            // This will call back after the listener is added automatically.
            subscriptionManager.addOnSubscriptionsChangedListener(mOnSubscriptionsChangedListener);
        } else {
            logger.error("SubscriptionManager not available! Retrying...");
            // Retry this again after some time.
            mRetryHandler.postDelayed(this::registerSubscriptionChangedListener,
                    IMS_SERVICE_RETRY_TIMEOUT_MS);
        }
    }

    private void registerImsCallbacksAndSetAssociatedSubscription() {
        SubscriptionManager sm = getSystemService(SubscriptionManager.class);
        if (sm == null) {
            logger.warn("handleSubscriptionsChanged: SubscriptionManager is null!");
            return;
        }
        int defaultSub = RcsSettingUtils.getDefaultSubscriptionId(this);
        // If the presence SIP PUBLISH procedure is not supported, treat it as if there is no valid
        // associated sub
        if (!RcsSettingUtils.isPublishEnabled(this, defaultSub)) {
            defaultSub = SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        }

        try {
            if (defaultSub == mAssociatedSubscription) {
                // Don't register duplicate callbacks for the same subscription.
                return;
            }
            if (mAssociatedSubscription != SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
                // Get rid of any existing registrations.
                ImsMmTelManager oldImsManager = ImsMmTelManager.createForSubscriptionId(
                        mAssociatedSubscription);
                ProvisioningManager oldProvisioningManager =
                        ProvisioningManager.createForSubscriptionId(mAssociatedSubscription);
                oldImsManager.unregisterImsRegistrationCallback(mImsRegistrationCallback);
                oldImsManager.unregisterMmTelCapabilityCallback(mCapabilityCallback);
                oldProvisioningManager.unregisterProvisioningChangedCallback(
                        mProvisioningChangedCallback);
                logger.print("callbacks unregistered for sub " + mAssociatedSubscription);
            }
            if (SubscriptionManager.isValidSubscriptionId(defaultSub)) {
                ImsMmTelManager imsManager = ImsMmTelManager.createForSubscriptionId(defaultSub);
                ProvisioningManager provisioningManager =
                        ProvisioningManager.createForSubscriptionId(defaultSub);
                // move over registrations if the new sub id is valid.
                imsManager.registerImsRegistrationCallback(getMainExecutor(),
                        mImsRegistrationCallback);
                imsManager.registerMmTelCapabilityCallback(getMainExecutor(), mCapabilityCallback);
                provisioningManager.registerProvisioningChangedCallback(getMainExecutor(),
                        mProvisioningChangedCallback);
                mAssociatedSubscription = defaultSub;
                logger.print("callbacks registered for sub " + mAssociatedSubscription);
            } else {
                mAssociatedSubscription = SubscriptionManager.INVALID_SUBSCRIPTION_ID;
            }
            logger.print("registerImsCallbacksAndSetAssociatedSubscription: new default="
                    + mAssociatedSubscription);
            handleAssociatedSubscriptionChanged(mAssociatedSubscription);
        } catch (ImsException e) {
            logger.info("Couldn't register callbacks for " + defaultSub + ": "
                    + e.getMessage());
            if (e.getCode() == ImsException.CODE_ERROR_SERVICE_UNAVAILABLE) {
                handleAssociatedSubscriptionChanged(SubscriptionManager.INVALID_SUBSCRIPTION_ID);
                // IMS temporarily unavailable. Retry after a few seconds.
                mRetryHandler.removeCallbacks(mRegisterCallbacks);
                mRetryHandler.postDelayed(mRegisterCallbacks, IMS_SERVICE_RETRY_TIMEOUT_MS);
            }
        }
    }

    private void handleAssociatedSubscriptionChanged(int newSubId) {
        if (mSubscriber != null) {
            mSubscriber.handleAssociatedSubscriptionChanged(newSubId);
        }
        if (mPublication != null) {
            mPublication.handleAssociatedSubscriptionChanged(newSubId);
        }
        if (mRcsStackAdaptor != null) {
            mRcsStackAdaptor.handleAssociatedSubscriptionChanged(newSubId);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        logger.debug("RcsService onStartCommand");

        return super.onStartCommand(intent, flags, startId);
    }

    /**
      * Cleans up when the service is destroyed
      */
    @Override
    public void onDestroy() {
        unregisterBroadcastReceiver();
        getContentResolver().unregisterContentObserver(mObserver);
        getContentResolver().unregisterContentObserver(mSiminfoSettingObserver);
        getSystemService(SubscriptionManager.class)
                .removeOnSubscriptionsChangedListener(mOnSubscriptionsChangedListener);

        mRcsStackAdaptor.finish();
        mPublication = null;
        mSubscriber = null;

        logger.debug("RcsService onDestroy");
        super.onDestroy();
    }

    public PresencePublication getPublication() {
        return mPublication;
    }

    IRcsPresence.Stub mIRcsPresenceImpl = new IRcsPresence.Stub() {
        /**
         * Asyncrhonously request the latest capability for a given contact list.
         * The result will be saved to DB directly if the contactNumber can be found in DB.
         * And then send intent com.android.ims.presence.CAPABILITY_STATE_CHANGED to notify it.
         * @param contactsNumber the contact list which will request capability.
         *                       Currently only support phone number.
         * @param listener the listener to get the response.
         * @return the resultCode which is defined by ResultCode.
         * @note framework uses only.
         * @hide
         */
        public int requestCapability(List<String> contactsNumber,
            IRcsPresenceListener listener){
            logger.debug("calling requestCapability");
            if(mSubscriber == null){
                logger.debug("requestCapability, mPresenceSubscriber == null");
                return ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
            }

            return mSubscriber.requestCapability(contactsNumber,
                    new CapabilityResultListener(listener));
         }

        /**
         * Asyncrhonously request the latest presence for a given contact.
         * The result will be saved to DB directly if it can be found in DB. And then send intent
         * com.android.ims.presence.AVAILABILITY_STATE_CHANGED to notify it.
         * @param contactNumber the contact which will request available.
         *                       Currently only support phone number.
         * @param listener the listener to get the response.
         * @return the resultCode which is defined by ResultCode.
         * @note framework uses only.
         * @hide
         */
        public int requestAvailability(String contactNumber, IRcsPresenceListener listener){
            if(mSubscriber == null){
                logger.error("requestAvailability, mPresenceSubscriber is null");
                return ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
            }

            // check availability cache (in RAM).
            return mSubscriber.requestAvailability(contactNumber,
                    new CapabilityResultListener(listener), false);
        }

        /**
         * Same as requestAvailability. but requestAvailability will consider throttle to avoid too
         * fast call. Which means it will not send the request to network in next 60s for the same
         * request.
         * The error code SUBSCRIBE_TOO_FREQUENTLY will be returned under the case.
         * But for this funcation it will always send the request to network.
         *
         * @see IRcsPresenceListener
         * @see RcsManager.ResultCode
         * @see ResultCode.SUBSCRIBE_TOO_FREQUENTLY
         */
        public int requestAvailabilityNoThrottle(String contactNumber,
                IRcsPresenceListener listener) {
            if(mSubscriber == null){
                logger.error("requestAvailabilityNoThrottle, mPresenceSubscriber is null");
                return ResultCode.ERROR_SERVICE_NOT_AVAILABLE;
            }

            // check availability cache (in RAM).
            return mSubscriber.requestAvailability(contactNumber,
                    new CapabilityResultListener(listener), true);
        }

        public int getPublishState() throws RemoteException {
            return publisherPublishStateToPublishState(mPublication.getPublishState());
        }
    };

    @Override
    public IBinder onBind(Intent arg0) {
        return mBinder;
    }

    /**
     * Receives notifications when Mobile data is enabled or disabled.
     */
    private class MobileDataContentObserver extends ContentObserver {
        public MobileDataContentObserver() {
            super(new Handler());
        }

        @Override
        public void onChange(final boolean selfChange) {
            boolean enabled = Settings.Global.getInt(getContentResolver(),
                    Settings.Global.MOBILE_DATA, 1) == 1;
            logger.debug("Mobile data enabled status: " + (enabled ? "ON" : "OFF"));
            onMobileDataEnabled(enabled);
        }
    }

    /** Observer to get notified when Mobile data enabled status changes */
    private MobileDataContentObserver mObserver;

    private void onMobileDataEnabled(final boolean enabled) {
        logger.debug("Enter onMobileDataEnabled: " + enabled);
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                try{
                    if(mPublication != null){
                        mPublication.onMobileDataChanged(enabled);
                        return;
                    }
                }catch(Exception e){
                    logger.error("Exception onMobileDataEnabled:", e);
                }
            }
        }, "onMobileDataEnabled thread");

        thread.start();
    }


    private SimInfoContentObserver mSiminfoSettingObserver;

    /**
     * Receives notifications when the TelephonyProvider is changed.
     */
    private class SimInfoContentObserver extends ContentObserver {
        public SimInfoContentObserver() {
            super(new Handler());
        }

        @Override
        public void onChange(final boolean selfChange) {
            if (mAssociatedSubscription == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
                return;
            }
            ImsMmTelManager ims = ImsMmTelManager.createForSubscriptionId(mAssociatedSubscription);
            try {
                boolean enabled = ims.isVtSettingEnabled();
                logger.debug("vt enabled status: " + (enabled ? "ON" : "OFF"));
                onVtEnabled(enabled);
            } catch (Exception e) {
                logger.info("Exception getting VT status for sub:" + mAssociatedSubscription
                        + ", Exception = " + e.getMessage());
            }
        }
    }

    private void onVtEnabled(boolean enabled) {
        if(mPublication != null){
            mPublication.onVtEnabled(enabled);
        }
    }

    private final IRcsService.Stub mBinder = new IRcsService.Stub() {
        /**
         * return true if the rcs service is ready for use.
         */
        public boolean isRcsServiceAvailable(){
            logger.debug("calling isRcsServiceAvailable");
            if(mRcsStackAdaptor == null){
                return false;
            }

            return mRcsStackAdaptor.isImsEnableState();
        }

        /**
         * Gets the presence interface.
         *
         * @see IRcsPresence
         */
        public IRcsPresence getRcsPresenceInterface(){
            return mIRcsPresenceImpl;
        }
    };

    private RegistrationManager.RegistrationCallback mImsRegistrationCallback =
            new RegistrationManager.RegistrationCallback() {

        @Override
        public void onRegistered(int imsTransportType) {
            logger.debug("onImsConnected imsTransportType=" + imsTransportType);
            mNetworkRegistrationType = imsTransportType;
            if(mPublication != null) {
                mPublication.onImsConnected();
            }
        }

        @Override
        public void onUnregistered(ImsReasonInfo info) {
            logger.debug("onImsDisconnected");
            mNetworkRegistrationType = AccessNetworkConstants.TRANSPORT_TYPE_INVALID;
            if(mPublication != null) {
                mPublication.onImsDisconnected();
            }
        }
    };

    private ImsMmTelManager.CapabilityCallback mCapabilityCallback =
            new ImsMmTelManager.CapabilityCallback() {

        @Override
        public void onCapabilitiesStatusChanged(MmTelFeature.MmTelCapabilities capabilities) {
            mPublication.onFeatureCapabilityChanged(mNetworkRegistrationType, capabilities);
        }
    };

    private ProvisioningManager.Callback mProvisioningChangedCallback =
            new ProvisioningManager.Callback() {

        @Override
        public void onProvisioningIntChanged(int item, int value) {
            switch (item) {
                case ProvisioningManager.KEY_EAB_PROVISIONING_STATUS:
                    // intentional fallthrough
                case ProvisioningManager.KEY_VOLTE_PROVISIONING_STATUS:
                    // intentional fallthrough
                case ProvisioningManager.KEY_VT_PROVISIONING_STATUS:
                    if (mPublication != null) {
                        mPublication.handleProvisioningChanged();
                    }
            }
        }
    };

    private static int publisherPublishStateToPublishState(int publisherPublishState) {
        switch(publisherPublishState) {
            case PresenceBase.PUBLISH_STATE_200_OK:
                return RcsPresence.PublishState.PUBLISH_STATE_200_OK;
            case PresenceBase.PUBLISH_STATE_NOT_PUBLISHED:
                return RcsPresence.PublishState.PUBLISH_STATE_NOT_PUBLISHED;
            case PresenceBase.PUBLISH_STATE_VOLTE_PROVISION_ERROR:
                return RcsPresence.PublishState.PUBLISH_STATE_VOLTE_PROVISION_ERROR;
            case PresenceBase.PUBLISH_STATE_RCS_PROVISION_ERROR:
                return RcsPresence.PublishState.PUBLISH_STATE_RCS_PROVISION_ERROR;
            case PresenceBase.PUBLISH_STATE_REQUEST_TIMEOUT:
                return RcsPresence.PublishState.PUBLISH_STATE_REQUEST_TIMEOUT;
            case PresenceBase.PUBLISH_STATE_OTHER_ERROR:
                return RcsPresence.PublishState.PUBLISH_STATE_OTHER_ERROR;

        }
        return PresenceBase.PUBLISH_STATE_OTHER_ERROR;
    }
}

