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

package com.android.phone;

import android.net.Uri;
import android.os.Binder;
import android.os.RemoteException;
import android.os.ServiceSpecificException;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyFrameworkInitializer;
import android.telephony.ims.ImsException;
import android.telephony.ims.RegistrationManager;
import android.telephony.ims.aidl.IImsCapabilityCallback;
import android.telephony.ims.aidl.IImsRcsController;
import android.telephony.ims.aidl.IImsRegistrationCallback;
import android.telephony.ims.aidl.IRcsUceControllerCallback;
import android.telephony.ims.feature.RcsFeature;
import android.telephony.ims.stub.ImsRegistrationImplBase;
import android.util.Log;

import com.android.ims.ImsManager;
import com.android.internal.telephony.IIntegerConsumer;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.TelephonyPermissions;
import com.android.internal.telephony.imsphone.ImsPhone;
import com.android.services.telephony.rcs.RcsFeatureController;
import com.android.services.telephony.rcs.TelephonyRcsService;
import com.android.services.telephony.rcs.UserCapabilityExchangeImpl;

import java.util.List;

/**
 * Implementation of the IImsRcsController interface.
 */
public class ImsRcsController extends IImsRcsController.Stub {
    private static final String TAG = "ImsRcsController";

    /** The singleton instance. */
    private static ImsRcsController sInstance;

    private PhoneGlobals mApp;
    private TelephonyRcsService mRcsService;

    /**
     * Initialize the singleton ImsRcsController instance.
     * This is only done once, at startup, from PhoneApp.onCreate().
     */
    static ImsRcsController init(PhoneGlobals app) {
        synchronized (ImsRcsController.class) {
            if (sInstance == null) {
                sInstance = new ImsRcsController(app);
            } else {
                Log.wtf(TAG, "init() called multiple times!  sInstance = " + sInstance);
            }
            return sInstance;
        }
    }

    /** Private constructor; @see init() */
    private ImsRcsController(PhoneGlobals app) {
        Log.i(TAG, "ImsRcsController");
        mApp = app;
        TelephonyFrameworkInitializer
                .getTelephonyServiceManager().getTelephonyImsServiceRegisterer().register(this);
    }

    /**
     * Register a {@link RegistrationManager.RegistrationCallback} to receive IMS network
     * registration state.
     */
    @Override
    public void registerImsRegistrationCallback(int subId, IImsRegistrationCallback callback) {
        enforceReadPrivilegedPermission("registerImsRegistrationCallback");
        final long token = Binder.clearCallingIdentity();
        try {
            getRcsFeatureController(subId).registerImsRegistrationCallback(subId, callback);
        } catch (ImsException e) {
            Log.e(TAG, "registerImsRegistrationCallback: sudId=" + subId + ", " + e.getMessage());
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Removes an existing {@link RegistrationManager.RegistrationCallback}.
     */
    @Override
    public void unregisterImsRegistrationCallback(int subId, IImsRegistrationCallback callback) {
        enforceReadPrivilegedPermission("unregisterImsRegistrationCallback");
        final long token = Binder.clearCallingIdentity();
        try {
            getRcsFeatureController(subId).unregisterImsRegistrationCallback(subId, callback);
        } catch (ServiceSpecificException e) {
            Log.e(TAG, "unregisterImsRegistrationCallback: error=" + e.errorCode);
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Get the IMS service registration state for the RcsFeature associated with this sub id.
     */
    @Override
    public void getImsRcsRegistrationState(int subId, IIntegerConsumer consumer) {
        enforceReadPrivilegedPermission("getImsRcsRegistrationState");
        final long token = Binder.clearCallingIdentity();
        try {
            getRcsFeatureController(subId).getRegistrationState(regState -> {
                try {
                    consumer.accept((regState == null)
                            ? RegistrationManager.REGISTRATION_STATE_NOT_REGISTERED : regState);
                } catch (RemoteException e) {
                    Log.w(TAG, "getImsRcsRegistrationState: callback is not available.");
                }
            });
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Gets the Transport Type associated with the current IMS RCS registration.
     */
    @Override
    public void getImsRcsRegistrationTransportType(int subId, IIntegerConsumer consumer) {
        enforceReadPrivilegedPermission("getImsRcsRegistrationTransportType");
        final long token = Binder.clearCallingIdentity();
        try {
            getRcsFeatureController(subId).getRegistrationTech(regTech -> {
                // Convert registration tech from ImsRegistrationImplBase -> RegistrationManager
                int regTechConverted = (regTech == null)
                        ? ImsRegistrationImplBase.REGISTRATION_TECH_NONE : regTech;
                regTechConverted = RegistrationManager.IMS_REG_TO_ACCESS_TYPE_MAP.get(
                        regTechConverted);
                try {
                    consumer.accept(regTechConverted);
                } catch (RemoteException e) {
                    Log.w(TAG, "getImsRcsRegistrationTransportType: callback is not available.");
                }
            });
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Register a capability callback which will provide RCS availability updates for the
     * subscription specified.
     *
     * @param subId the subscription ID
     * @param callback The ImsCapabilityCallback to be registered.
     */
    @Override
    public void registerRcsAvailabilityCallback(int subId, IImsCapabilityCallback callback) {
        enforceReadPrivilegedPermission("registerRcsAvailabilityCallback");
        final long token = Binder.clearCallingIdentity();
        try {
            getRcsFeatureController(subId).registerRcsAvailabilityCallback(subId, callback);
        } catch (ImsException e) {
            Log.e(TAG, "registerRcsAvailabilityCallback: sudId=" + subId + ", " + e.getMessage());
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Remove the registered capability callback.
     *
     * @param subId the subscription ID
     * @param callback The ImsCapabilityCallback to be removed.
     */
    @Override
    public void unregisterRcsAvailabilityCallback(int subId, IImsCapabilityCallback callback) {
        enforceReadPrivilegedPermission("unregisterRcsAvailabilityCallback");
        final long token = Binder.clearCallingIdentity();
        try {
            getRcsFeatureController(subId).unregisterRcsAvailabilityCallback(subId, callback);
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Query for the capability of an IMS RCS service
     *
     * @param subId the subscription ID
     * @param capability the RCS capability to query.
     * @param radioTech the radio tech that this capability failed for
     * @return true if the RCS capability is capable for this subscription, false otherwise.
     */
    @Override
    public boolean isCapable(int subId,
            @RcsFeature.RcsImsCapabilities.RcsImsCapabilityFlag int capability,
            @ImsRegistrationImplBase.ImsRegistrationTech int radioTech) {
        enforceReadPrivilegedPermission("isCapable");
        final long token = Binder.clearCallingIdentity();
        try {
            return getRcsFeatureController(subId).isCapable(capability, radioTech);
        } catch (ImsException e) {
            Log.e(TAG, "isCapable: sudId=" + subId
                    + ", capability=" + capability + ", " + e.getMessage());
            return false;
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Query the availability of an IMS RCS capability.
     *
     * @param subId the subscription ID
     * @param capability the RCS capability to query.
     * @return true if the RCS capability is currently available for the associated subscription,
     * false otherwise.
     */
    @Override
    public boolean isAvailable(int subId,
            @RcsFeature.RcsImsCapabilities.RcsImsCapabilityFlag int capability) {
        enforceReadPrivilegedPermission("isAvailable");
        final long token = Binder.clearCallingIdentity();
        try {
            return getRcsFeatureController(subId).isAvailable(capability);
        } catch (ImsException e) {
            Log.e(TAG, "isAvailable: sudId=" + subId
                    + ", capability=" + capability + ", " + e.getMessage());
            return false;
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    @Override
    public void requestCapabilities(int subId, String callingPackage, String callingFeatureId,
            List<Uri> contactNumbers, IRcsUceControllerCallback c) {
        enforceReadPrivilegedPermission("requestCapabilities");
        if (!isUceSettingEnabled(subId, callingPackage, callingFeatureId)) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "The user has not enabled UCE for this subscription.");
        }
        final long token = Binder.clearCallingIdentity();
        try {
            UserCapabilityExchangeImpl uce = getRcsFeatureController(subId).getFeature(
                    UserCapabilityExchangeImpl.class);
            if (uce == null) {
                throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                        "This subscription does not support UCE.");
            }
            uce.requestCapabilities(contactNumbers, c);
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    @Override
    public int getUcePublishState(int subId) {
        enforceReadPrivilegedPermission("getUcePublishState");
        final long token = Binder.clearCallingIdentity();
        try {
            UserCapabilityExchangeImpl uce = getRcsFeatureController(subId).getFeature(
                    UserCapabilityExchangeImpl.class);
            if (uce == null) {
                throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                        "This subscription does not support UCE.");
            }
            return uce.getUcePublishState();
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    @Override
    public boolean isUceSettingEnabled(int subId, String callingPackage, String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId, "isUceSettingEnabled")) {
            Log.w(TAG, "isUceSettingEnabled: READ_PHONE_STATE app op disabled when accessing "
                    + "isUceSettingEnabled");
            return false;
        }
        final long token = Binder.clearCallingIdentity();
        try {
            return SubscriptionManager.getBooleanSubscriptionProperty(subId,
                    SubscriptionManager.IMS_RCS_UCE_ENABLED, false /*defaultValue*/, mApp);
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    @Override
    public void setUceSettingEnabled(int subId, boolean isEnabled) {
        enforceModifyPermission();
        final long token = Binder.clearCallingIdentity();
        try {
            SubscriptionManager.setSubscriptionProperty(subId,
                    SubscriptionManager.IMS_RCS_UCE_ENABLED, (isEnabled ? "1" : "0"));
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Make sure either called from same process as self (phone) or IPC caller has read privilege.
     *
     * @throws SecurityException if the caller does not have the required permission
     */
    private void enforceReadPrivilegedPermission(String message) {
        mApp.enforceCallingOrSelfPermission(
                android.Manifest.permission.READ_PRIVILEGED_PHONE_STATE, message);
    }

    /**
     * Make sure the caller has the MODIFY_PHONE_STATE permission.
     *
     * @throws SecurityException if the caller does not have the required permission
     */
    private void enforceModifyPermission() {
        mApp.enforceCallingOrSelfPermission(android.Manifest.permission.MODIFY_PHONE_STATE, null);
    }

    /**
     * Retrieve ImsPhone instance.
     *
     * @param subId the subscription ID
     * @return The ImsPhone instance
     * @throws ServiceSpecificException if getting ImsPhone instance failed.
     */
    private ImsPhone getImsPhone(int subId) {
        if (!ImsManager.isImsSupportedOnDevice(mApp)) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "IMS is not available on device.");
        }
        Phone phone = PhoneGlobals.getPhone(subId);
        if (phone == null) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_INVALID_SUBSCRIPTION,
                    "Invalid subscription Id: " + subId);
        }
        ImsPhone imsPhone = (ImsPhone) phone.getImsPhone();
        if (imsPhone == null) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_SERVICE_UNAVAILABLE,
                    "Cannot find ImsPhone instance: " + subId);
        }
        return imsPhone;
    }

    /**
     * Retrieve RcsFeatureManager instance.
     *
     * @param subId the subscription ID
     * @return The RcsFeatureManager instance
     * @throws ServiceSpecificException if getting RcsFeatureManager instance failed.
     */
    private RcsFeatureController getRcsFeatureController(int subId) {
        if (!ImsManager.isImsSupportedOnDevice(mApp)) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "IMS is not available on device.");
        }
        if (mRcsService == null) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "IMS is not available on device.");
        }
        Phone phone = PhoneGlobals.getPhone(subId);
        if (phone == null) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_INVALID_SUBSCRIPTION,
                    "Invalid subscription Id: " + subId);
        }
        int slotId = phone.getPhoneId();
        RcsFeatureController c = mRcsService.getFeatureController(slotId);
        if (c == null) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "The requested operation is not supported for subId " + subId);
        }
        return c;
    }

    void setRcsService(TelephonyRcsService rcsService) {
        mRcsService = rcsService;
    }
}
