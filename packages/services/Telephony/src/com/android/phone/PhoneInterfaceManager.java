/*
 * Copyright (C) 2006 The Android Open Source Project
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

import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import static com.android.internal.telephony.PhoneConstants.PHONE_TYPE_CDMA;
import static com.android.internal.telephony.PhoneConstants.PHONE_TYPE_GSM;
import static com.android.internal.telephony.PhoneConstants.PHONE_TYPE_IMS;
import static com.android.internal.telephony.PhoneConstants.SUBSCRIPTION_KEY;

import android.Manifest.permission;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.AppOpsManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.ComponentInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.AsyncResult;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.ParcelFileDescriptor;
import android.os.ParcelUuid;
import android.os.PersistableBundle;
import android.os.Process;
import android.os.RemoteException;
import android.os.ResultReceiver;
import android.os.ServiceSpecificException;
import android.os.UserHandle;
import android.os.UserManager;
import android.os.WorkSource;
import android.preference.PreferenceManager;
import android.provider.DeviceConfig;
import android.provider.Settings;
import android.provider.Telephony;
import android.sysprop.TelephonyProperties;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telephony.Annotation.ApnType;
import android.telephony.CallForwardingInfo;
import android.telephony.CarrierConfigManager;
import android.telephony.CarrierRestrictionRules;
import android.telephony.CellIdentity;
import android.telephony.CellIdentityCdma;
import android.telephony.CellIdentityGsm;
import android.telephony.CellInfo;
import android.telephony.CellInfoGsm;
import android.telephony.CellInfoWcdma;
import android.telephony.ClientRequestStats;
import android.telephony.ICellInfoCallback;
import android.telephony.IccOpenLogicalChannelResponse;
import android.telephony.LocationAccessPolicy;
import android.telephony.ModemActivityInfo;
import android.telephony.NeighboringCellInfo;
import android.telephony.NetworkScanRequest;
import android.telephony.PhoneCapability;
import android.telephony.PhoneNumberRange;
import android.telephony.RadioAccessFamily;
import android.telephony.RadioAccessSpecifier;
import android.telephony.ServiceState;
import android.telephony.SignalStrength;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyFrameworkInitializer;
import android.telephony.TelephonyHistogram;
import android.telephony.TelephonyManager;
import android.telephony.TelephonyScanManager;
import android.telephony.UiccCardInfo;
import android.telephony.UiccSlotInfo;
import android.telephony.UssdResponse;
import android.telephony.VisualVoicemailSmsFilterSettings;
import android.telephony.data.ApnSetting;
import android.telephony.emergency.EmergencyNumber;
import android.telephony.ims.ImsException;
import android.telephony.ims.ProvisioningManager;
import android.telephony.ims.RegistrationManager;
import android.telephony.ims.aidl.IImsCapabilityCallback;
import android.telephony.ims.aidl.IImsConfig;
import android.telephony.ims.aidl.IImsConfigCallback;
import android.telephony.ims.aidl.IImsMmTelFeature;
import android.telephony.ims.aidl.IImsRcsFeature;
import android.telephony.ims.aidl.IImsRegistration;
import android.telephony.ims.aidl.IImsRegistrationCallback;
import android.telephony.ims.feature.ImsFeature;
import android.telephony.ims.feature.MmTelFeature;
import android.telephony.ims.feature.RcsFeature;
import android.telephony.ims.stub.ImsConfigImplBase;
import android.telephony.ims.stub.ImsRegistrationImplBase;
import android.text.TextUtils;
import android.util.ArraySet;
import android.util.EventLog;
import android.util.Log;
import android.util.Pair;

import com.android.ims.ImsManager;
import com.android.ims.internal.IImsServiceFeatureCallback;
import com.android.internal.telephony.CallForwardInfo;
import com.android.internal.telephony.CallManager;
import com.android.internal.telephony.CallStateException;
import com.android.internal.telephony.CarrierInfoManager;
import com.android.internal.telephony.CarrierResolver;
import com.android.internal.telephony.CellNetworkScanResult;
import com.android.internal.telephony.CommandException;
import com.android.internal.telephony.CommandsInterface;
import com.android.internal.telephony.DefaultPhoneNotifier;
import com.android.internal.telephony.GsmCdmaPhone;
import com.android.internal.telephony.HalVersion;
import com.android.internal.telephony.IBooleanConsumer;
import com.android.internal.telephony.IIntegerConsumer;
import com.android.internal.telephony.INumberVerificationCallback;
import com.android.internal.telephony.ITelephony;
import com.android.internal.telephony.IccCard;
import com.android.internal.telephony.LocaleTracker;
import com.android.internal.telephony.NetworkScanRequestTracker;
import com.android.internal.telephony.OperatorInfo;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneConfigurationManager;
import com.android.internal.telephony.PhoneConstantConversions;
import com.android.internal.telephony.PhoneConstants;
import com.android.internal.telephony.PhoneFactory;
import com.android.internal.telephony.ProxyController;
import com.android.internal.telephony.RIL;
import com.android.internal.telephony.RILConstants;
import com.android.internal.telephony.ServiceStateTracker;
import com.android.internal.telephony.SmsController;
import com.android.internal.telephony.SmsPermissions;
import com.android.internal.telephony.SubscriptionController;
import com.android.internal.telephony.TelephonyIntents;
import com.android.internal.telephony.TelephonyPermissions;
import com.android.internal.telephony.dataconnection.ApnSettingUtils;
import com.android.internal.telephony.emergency.EmergencyNumberTracker;
import com.android.internal.telephony.euicc.EuiccConnector;
import com.android.internal.telephony.ims.ImsResolver;
import com.android.internal.telephony.imsphone.ImsPhone;
import com.android.internal.telephony.imsphone.ImsPhoneCallTracker;
import com.android.internal.telephony.metrics.TelephonyMetrics;
import com.android.internal.telephony.uicc.IccCardApplicationStatus.AppType;
import com.android.internal.telephony.uicc.IccIoResult;
import com.android.internal.telephony.uicc.IccRecords;
import com.android.internal.telephony.uicc.IccUtils;
import com.android.internal.telephony.uicc.SIMRecords;
import com.android.internal.telephony.uicc.UiccCard;
import com.android.internal.telephony.uicc.UiccCardApplication;
import com.android.internal.telephony.uicc.UiccController;
import com.android.internal.telephony.uicc.UiccProfile;
import com.android.internal.telephony.uicc.UiccSlot;
import com.android.internal.telephony.util.LocaleUtils;
import com.android.internal.telephony.util.VoicemailNotificationSettingsUtil;
import com.android.internal.util.HexDump;
import com.android.phone.settings.PickSmsSubscriptionActivity;
import com.android.phone.vvm.PhoneAccountHandleConverter;
import com.android.phone.vvm.RemoteVvmTaskManager;
import com.android.phone.vvm.VisualVoicemailSettingsUtil;
import com.android.phone.vvm.VisualVoicemailSmsFilterConfig;
import com.android.telephony.Rlog;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

/**
 * Implementation of the ITelephony interface.
 */
public class PhoneInterfaceManager extends ITelephony.Stub {
    private static final String LOG_TAG = "PhoneInterfaceManager";
    private static final boolean DBG = (PhoneGlobals.DBG_LEVEL >= 2);
    private static final boolean DBG_LOC = false;
    private static final boolean DBG_MERGE = false;

    // Message codes used with mMainThreadHandler
    private static final int CMD_HANDLE_PIN_MMI = 1;
    private static final int CMD_TRANSMIT_APDU_LOGICAL_CHANNEL = 7;
    private static final int EVENT_TRANSMIT_APDU_LOGICAL_CHANNEL_DONE = 8;
    private static final int CMD_OPEN_CHANNEL = 9;
    private static final int EVENT_OPEN_CHANNEL_DONE = 10;
    private static final int CMD_CLOSE_CHANNEL = 11;
    private static final int EVENT_CLOSE_CHANNEL_DONE = 12;
    private static final int CMD_NV_READ_ITEM = 13;
    private static final int EVENT_NV_READ_ITEM_DONE = 14;
    private static final int CMD_NV_WRITE_ITEM = 15;
    private static final int EVENT_NV_WRITE_ITEM_DONE = 16;
    private static final int CMD_NV_WRITE_CDMA_PRL = 17;
    private static final int EVENT_NV_WRITE_CDMA_PRL_DONE = 18;
    private static final int CMD_RESET_MODEM_CONFIG = 19;
    private static final int EVENT_RESET_MODEM_CONFIG_DONE = 20;
    private static final int CMD_GET_PREFERRED_NETWORK_TYPE = 21;
    private static final int EVENT_GET_PREFERRED_NETWORK_TYPE_DONE = 22;
    private static final int CMD_SET_PREFERRED_NETWORK_TYPE = 23;
    private static final int EVENT_SET_PREFERRED_NETWORK_TYPE_DONE = 24;
    private static final int CMD_SEND_ENVELOPE = 25;
    private static final int EVENT_SEND_ENVELOPE_DONE = 26;
    private static final int CMD_INVOKE_OEM_RIL_REQUEST_RAW = 27;
    private static final int EVENT_INVOKE_OEM_RIL_REQUEST_RAW_DONE = 28;
    private static final int CMD_TRANSMIT_APDU_BASIC_CHANNEL = 29;
    private static final int EVENT_TRANSMIT_APDU_BASIC_CHANNEL_DONE = 30;
    private static final int CMD_EXCHANGE_SIM_IO = 31;
    private static final int EVENT_EXCHANGE_SIM_IO_DONE = 32;
    private static final int CMD_SET_VOICEMAIL_NUMBER = 33;
    private static final int EVENT_SET_VOICEMAIL_NUMBER_DONE = 34;
    private static final int CMD_SET_NETWORK_SELECTION_MODE_AUTOMATIC = 35;
    private static final int EVENT_SET_NETWORK_SELECTION_MODE_AUTOMATIC_DONE = 36;
    private static final int CMD_GET_MODEM_ACTIVITY_INFO = 37;
    private static final int EVENT_GET_MODEM_ACTIVITY_INFO_DONE = 38;
    private static final int CMD_PERFORM_NETWORK_SCAN = 39;
    private static final int EVENT_PERFORM_NETWORK_SCAN_DONE = 40;
    private static final int CMD_SET_NETWORK_SELECTION_MODE_MANUAL = 41;
    private static final int EVENT_SET_NETWORK_SELECTION_MODE_MANUAL_DONE = 42;
    private static final int CMD_SET_ALLOWED_CARRIERS = 43;
    private static final int EVENT_SET_ALLOWED_CARRIERS_DONE = 44;
    private static final int CMD_GET_ALLOWED_CARRIERS = 45;
    private static final int EVENT_GET_ALLOWED_CARRIERS_DONE = 46;
    private static final int CMD_HANDLE_USSD_REQUEST = 47;
    private static final int CMD_GET_FORBIDDEN_PLMNS = 48;
    private static final int EVENT_GET_FORBIDDEN_PLMNS_DONE = 49;
    private static final int CMD_SWITCH_SLOTS = 50;
    private static final int EVENT_SWITCH_SLOTS_DONE = 51;
    private static final int CMD_GET_NETWORK_SELECTION_MODE = 52;
    private static final int EVENT_GET_NETWORK_SELECTION_MODE_DONE = 53;
    private static final int CMD_GET_CDMA_ROAMING_MODE = 54;
    private static final int EVENT_GET_CDMA_ROAMING_MODE_DONE = 55;
    private static final int CMD_SET_CDMA_ROAMING_MODE = 56;
    private static final int EVENT_SET_CDMA_ROAMING_MODE_DONE = 57;
    private static final int CMD_SET_CDMA_SUBSCRIPTION_MODE = 58;
    private static final int EVENT_SET_CDMA_SUBSCRIPTION_MODE_DONE = 59;
    private static final int CMD_GET_ALL_CELL_INFO = 60;
    private static final int EVENT_GET_ALL_CELL_INFO_DONE = 61;
    private static final int CMD_GET_CELL_LOCATION = 62;
    private static final int EVENT_GET_CELL_LOCATION_DONE = 63;
    private static final int CMD_MODEM_REBOOT = 64;
    private static final int EVENT_CMD_MODEM_REBOOT_DONE = 65;
    private static final int CMD_REQUEST_CELL_INFO_UPDATE = 66;
    private static final int EVENT_REQUEST_CELL_INFO_UPDATE_DONE = 67;
    private static final int CMD_REQUEST_ENABLE_MODEM = 68;
    private static final int EVENT_ENABLE_MODEM_DONE = 69;
    private static final int CMD_GET_MODEM_STATUS = 70;
    private static final int EVENT_GET_MODEM_STATUS_DONE = 71;
    private static final int CMD_SET_FORBIDDEN_PLMNS = 72;
    private static final int EVENT_SET_FORBIDDEN_PLMNS_DONE = 73;
    private static final int CMD_ERASE_MODEM_CONFIG = 74;
    private static final int EVENT_ERASE_MODEM_CONFIG_DONE = 75;
    private static final int CMD_CHANGE_ICC_LOCK_PASSWORD = 76;
    private static final int EVENT_CHANGE_ICC_LOCK_PASSWORD_DONE = 77;
    private static final int CMD_SET_ICC_LOCK_ENABLED = 78;
    private static final int EVENT_SET_ICC_LOCK_ENABLED_DONE = 79;
    private static final int CMD_SET_SYSTEM_SELECTION_CHANNELS = 80;
    private static final int EVENT_SET_SYSTEM_SELECTION_CHANNELS_DONE = 81;
    private static final int MSG_NOTIFY_USER_ACTIVITY = 82;
    private static final int CMD_GET_CALL_FORWARDING = 83;
    private static final int EVENT_GET_CALL_FORWARDING_DONE = 84;
    private static final int CMD_SET_CALL_FORWARDING = 85;
    private static final int EVENT_SET_CALL_FORWARDING_DONE = 86;
    private static final int CMD_GET_CALL_WAITING = 87;
    private static final int EVENT_GET_CALL_WAITING_DONE = 88;
    private static final int CMD_SET_CALL_WAITING = 89;
    private static final int EVENT_SET_CALL_WAITING_DONE = 90;

    // Parameters of select command.
    private static final int SELECT_COMMAND = 0xA4;
    private static final int SELECT_P1 = 0x04;
    private static final int SELECT_P2 = 0;
    private static final int SELECT_P3 = 0x10;

    /** The singleton instance. */
    private static PhoneInterfaceManager sInstance;

    private PhoneGlobals mApp;
    private CallManager mCM;
    private ImsResolver mImsResolver;
    private UserManager mUserManager;
    private AppOpsManager mAppOps;
    private MainThreadHandler mMainThreadHandler;
    private SubscriptionController mSubscriptionController;
    private SharedPreferences mTelephonySharedPreferences;
    private PhoneConfigurationManager mPhoneConfigurationManager;

    /** User Activity */
    private AtomicBoolean mNotifyUserActivity;
    private static final int USER_ACTIVITY_NOTIFICATION_DELAY = 200;

    private static final String PREF_CARRIERS_ALPHATAG_PREFIX = "carrier_alphtag_";
    private static final String PREF_CARRIERS_NUMBER_PREFIX = "carrier_number_";
    private static final String PREF_CARRIERS_SUBSCRIBER_PREFIX = "carrier_subscriber_";
    private static final String PREF_PROVISION_IMS_MMTEL_PREFIX = "provision_ims_mmtel_";

    // String to store multi SIM allowed
    private static final String PREF_MULTI_SIM_RESTRICTED = "multisim_restricted";

    // The AID of ISD-R.
    private static final String ISDR_AID = "A0000005591010FFFFFFFF8900000100";

    private NetworkScanRequestTracker mNetworkScanRequestTracker;

    private static final int TYPE_ALLOCATION_CODE_LENGTH = 8;
    private static final int MANUFACTURER_CODE_LENGTH = 8;

    /**
     * Experiment flag to enable erase modem config on reset network, default value is false
     */
    public static final String RESET_NETWORK_ERASE_MODEM_CONFIG_ENABLED =
            "reset_network_erase_modem_config_enabled";

    /**
     * A request object to use for transmitting data to an ICC.
     */
    private static final class IccAPDUArgument {
        public int channel, cla, command, p1, p2, p3;
        public String data;

        public IccAPDUArgument(int channel, int cla, int command,
                int p1, int p2, int p3, String data) {
            this.channel = channel;
            this.cla = cla;
            this.command = command;
            this.p1 = p1;
            this.p2 = p2;
            this.p3 = p3;
            this.data = data;
        }
    }

    /**
     * A request object to use for transmitting data to an ICC.
     */
    private static final class ManualNetworkSelectionArgument {
        public OperatorInfo operatorInfo;
        public boolean persistSelection;

        public ManualNetworkSelectionArgument(OperatorInfo operatorInfo, boolean persistSelection) {
            this.operatorInfo = operatorInfo;
            this.persistSelection = persistSelection;
        }
    }

    /**
     * A request object for use with {@link MainThreadHandler}. Requesters should wait() on the
     * request after sending. The main thread will notify the request when it is complete.
     */
    private static final class MainThreadRequest {
        /** The argument to use for the request */
        public Object argument;
        /** The result of the request that is run on the main thread */
        public Object result;
        // The subscriber id that this request applies to. Defaults to
        // SubscriptionManager.INVALID_SUBSCRIPTION_ID
        public Integer subId = SubscriptionManager.INVALID_SUBSCRIPTION_ID;

        // In cases where subId is unavailable, the caller needs to specify the phone.
        public Phone phone;

        public WorkSource workSource;

        public MainThreadRequest(Object argument) {
            this.argument = argument;
        }

        MainThreadRequest(Object argument, Phone phone, WorkSource workSource) {
            this.argument = argument;
            if (phone != null) {
                this.phone = phone;
            }
            this.workSource = workSource;
        }

        MainThreadRequest(Object argument, Integer subId, WorkSource workSource) {
            this.argument = argument;
            if (subId != null) {
                this.subId = subId;
            }
            this.workSource = workSource;
        }
    }

    private static final class IncomingThirdPartyCallArgs {
        public final ComponentName component;
        public final String callId;
        public final String callerDisplayName;

        public IncomingThirdPartyCallArgs(ComponentName component, String callId,
                String callerDisplayName) {
            this.component = component;
            this.callId = callId;
            this.callerDisplayName = callerDisplayName;
        }
    }

    /**
     * A handler that processes messages on the main thread in the phone process. Since many
     * of the Phone calls are not thread safe this is needed to shuttle the requests from the
     * inbound binder threads to the main thread in the phone process.  The Binder thread
     * may provide a {@link MainThreadRequest} object in the msg.obj field that they are waiting
     * on, which will be notified when the operation completes and will contain the result of the
     * request.
     *
     * <p>If a MainThreadRequest object is provided in the msg.obj field,
     * note that request.result must be set to something non-null for the calling thread to
     * unblock.
     */
    private final class MainThreadHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            MainThreadRequest request;
            Message onCompleted;
            AsyncResult ar;
            UiccCard uiccCard;
            IccAPDUArgument iccArgument;
            final Phone defaultPhone = getDefaultPhone();

            switch (msg.what) {
                case CMD_HANDLE_USSD_REQUEST: {
                    request = (MainThreadRequest) msg.obj;
                    final Phone phone = getPhoneFromRequest(request);
                    Pair<String, ResultReceiver> ussdObject = (Pair) request.argument;
                    String ussdRequest =  ussdObject.first;
                    ResultReceiver wrappedCallback = ussdObject.second;

                    if (!isUssdApiAllowed(request.subId)) {
                        // Carrier does not support use of this API, return failure.
                        Rlog.w(LOG_TAG, "handleUssdRequest: carrier does not support USSD apis.");
                        UssdResponse response = new UssdResponse(ussdRequest, null);
                        Bundle returnData = new Bundle();
                        returnData.putParcelable(TelephonyManager.USSD_RESPONSE, response);
                        wrappedCallback.send(TelephonyManager.USSD_RETURN_FAILURE, returnData);

                        request.result = true;
                        notifyRequester(request);
                        return;
                    }

                    try {
                        request.result = phone != null
                                ? phone.handleUssdRequest(ussdRequest, wrappedCallback) : false;
                    } catch (CallStateException cse) {
                        request.result = false;
                    }
                    // Wake up the requesting thread
                    notifyRequester(request);
                    break;
                }

                case CMD_HANDLE_PIN_MMI: {
                    request = (MainThreadRequest) msg.obj;
                    final Phone phone = getPhoneFromRequest(request);
                    request.result = phone != null ?
                            getPhoneFromRequest(request).handlePinMmi((String) request.argument)
                            : false;
                    // Wake up the requesting thread
                    notifyRequester(request);
                    break;
                }

                case CMD_TRANSMIT_APDU_LOGICAL_CHANNEL:
                    request = (MainThreadRequest) msg.obj;
                    iccArgument = (IccAPDUArgument) request.argument;
                    uiccCard = getUiccCardFromRequest(request);
                    if (uiccCard == null) {
                        loge("iccTransmitApduLogicalChannel: No UICC");
                        request.result = new IccIoResult(0x6F, 0, (byte[])null);
                        notifyRequester(request);
                    } else {
                        onCompleted = obtainMessage(EVENT_TRANSMIT_APDU_LOGICAL_CHANNEL_DONE,
                            request);
                        uiccCard.iccTransmitApduLogicalChannel(
                            iccArgument.channel, iccArgument.cla, iccArgument.command,
                            iccArgument.p1, iccArgument.p2, iccArgument.p3, iccArgument.data,
                            onCompleted);
                    }
                    break;

                case EVENT_TRANSMIT_APDU_LOGICAL_CHANNEL_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;
                    } else {
                        request.result = new IccIoResult(0x6F, 0, (byte[])null);
                        if (ar.result == null) {
                            loge("iccTransmitApduLogicalChannel: Empty response");
                        } else if (ar.exception instanceof CommandException) {
                            loge("iccTransmitApduLogicalChannel: CommandException: " +
                                    ar.exception);
                        } else {
                            loge("iccTransmitApduLogicalChannel: Unknown exception");
                        }
                    }
                    notifyRequester(request);
                    break;

                case CMD_TRANSMIT_APDU_BASIC_CHANNEL:
                    request = (MainThreadRequest) msg.obj;
                    iccArgument = (IccAPDUArgument) request.argument;
                    uiccCard = getUiccCardFromRequest(request);
                    if (uiccCard == null) {
                        loge("iccTransmitApduBasicChannel: No UICC");
                        request.result = new IccIoResult(0x6F, 0, (byte[])null);
                        notifyRequester(request);
                    } else {
                        onCompleted = obtainMessage(EVENT_TRANSMIT_APDU_BASIC_CHANNEL_DONE,
                            request);
                        uiccCard.iccTransmitApduBasicChannel(
                            iccArgument.cla, iccArgument.command, iccArgument.p1, iccArgument.p2,
                            iccArgument.p3, iccArgument.data, onCompleted);
                    }
                    break;

                case EVENT_TRANSMIT_APDU_BASIC_CHANNEL_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;
                    } else {
                        request.result = new IccIoResult(0x6F, 0, (byte[])null);
                        if (ar.result == null) {
                            loge("iccTransmitApduBasicChannel: Empty response");
                        } else if (ar.exception instanceof CommandException) {
                            loge("iccTransmitApduBasicChannel: CommandException: " +
                                    ar.exception);
                        } else {
                            loge("iccTransmitApduBasicChannel: Unknown exception");
                        }
                    }
                    notifyRequester(request);
                    break;

                case CMD_EXCHANGE_SIM_IO:
                    request = (MainThreadRequest) msg.obj;
                    iccArgument = (IccAPDUArgument) request.argument;
                    uiccCard = getUiccCardFromRequest(request);
                    if (uiccCard == null) {
                        loge("iccExchangeSimIO: No UICC");
                        request.result = new IccIoResult(0x6F, 0, (byte[])null);
                        notifyRequester(request);
                    } else {
                        onCompleted = obtainMessage(EVENT_EXCHANGE_SIM_IO_DONE,
                                request);
                        uiccCard.iccExchangeSimIO(iccArgument.cla, /* fileID */
                                iccArgument.command, iccArgument.p1, iccArgument.p2, iccArgument.p3,
                                iccArgument.data, onCompleted);
                    }
                    break;

                case EVENT_EXCHANGE_SIM_IO_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;
                    } else {
                        request.result = new IccIoResult(0x6f, 0, (byte[])null);
                    }
                    notifyRequester(request);
                    break;

                case CMD_SEND_ENVELOPE:
                    request = (MainThreadRequest) msg.obj;
                    uiccCard = getUiccCardFromRequest(request);
                    if (uiccCard == null) {
                        loge("sendEnvelopeWithStatus: No UICC");
                        request.result = new IccIoResult(0x6F, 0, (byte[])null);
                        notifyRequester(request);
                    } else {
                        onCompleted = obtainMessage(EVENT_SEND_ENVELOPE_DONE, request);
                        uiccCard.sendEnvelopeWithStatus((String)request.argument, onCompleted);
                    }
                    break;

                case EVENT_SEND_ENVELOPE_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;
                    } else {
                        request.result = new IccIoResult(0x6F, 0, (byte[])null);
                        if (ar.result == null) {
                            loge("sendEnvelopeWithStatus: Empty response");
                        } else if (ar.exception instanceof CommandException) {
                            loge("sendEnvelopeWithStatus: CommandException: " +
                                    ar.exception);
                        } else {
                            loge("sendEnvelopeWithStatus: exception:" + ar.exception);
                        }
                    }
                    notifyRequester(request);
                    break;

                case CMD_OPEN_CHANNEL:
                    request = (MainThreadRequest) msg.obj;
                    uiccCard = getUiccCardFromRequest(request);
                    Pair<String, Integer> openChannelArgs = (Pair<String, Integer>) request.argument;
                    if (uiccCard == null) {
                        loge("iccOpenLogicalChannel: No UICC");
                        request.result = new IccOpenLogicalChannelResponse(-1,
                            IccOpenLogicalChannelResponse.STATUS_MISSING_RESOURCE, null);
                        notifyRequester(request);
                    } else {
                        onCompleted = obtainMessage(EVENT_OPEN_CHANNEL_DONE, request);
                        uiccCard.iccOpenLogicalChannel(openChannelArgs.first,
                                openChannelArgs.second, onCompleted);
                    }
                    break;

                case EVENT_OPEN_CHANNEL_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    IccOpenLogicalChannelResponse openChannelResp;
                    if (ar.exception == null && ar.result != null) {
                        int[] result = (int[]) ar.result;
                        int channelId = result[0];
                        byte[] selectResponse = null;
                        if (result.length > 1) {
                            selectResponse = new byte[result.length - 1];
                            for (int i = 1; i < result.length; ++i) {
                                selectResponse[i - 1] = (byte) result[i];
                            }
                        }
                        openChannelResp = new IccOpenLogicalChannelResponse(channelId,
                            IccOpenLogicalChannelResponse.STATUS_NO_ERROR, selectResponse);
                    } else {
                        if (ar.result == null) {
                            loge("iccOpenLogicalChannel: Empty response");
                        }
                        if (ar.exception != null) {
                            loge("iccOpenLogicalChannel: Exception: " + ar.exception);
                        }

                        int errorCode = IccOpenLogicalChannelResponse.STATUS_UNKNOWN_ERROR;
                        if (ar.exception instanceof CommandException) {
                            CommandException.Error error =
                                ((CommandException) (ar.exception)).getCommandError();
                            if (error == CommandException.Error.MISSING_RESOURCE) {
                                errorCode = IccOpenLogicalChannelResponse.STATUS_MISSING_RESOURCE;
                            } else if (error == CommandException.Error.NO_SUCH_ELEMENT) {
                                errorCode = IccOpenLogicalChannelResponse.STATUS_NO_SUCH_ELEMENT;
                            }
                        }
                        openChannelResp = new IccOpenLogicalChannelResponse(
                            IccOpenLogicalChannelResponse.INVALID_CHANNEL, errorCode, null);
                    }
                    request.result = openChannelResp;
                    notifyRequester(request);
                    break;

                case CMD_CLOSE_CHANNEL:
                    request = (MainThreadRequest) msg.obj;
                    uiccCard = getUiccCardFromRequest(request);
                    if (uiccCard == null) {
                        loge("iccCloseLogicalChannel: No UICC");
                        request.result = false;
                        notifyRequester(request);
                    } else {
                        onCompleted = obtainMessage(EVENT_CLOSE_CHANNEL_DONE, request);
                        uiccCard.iccCloseLogicalChannel((Integer) request.argument, onCompleted);
                    }
                    break;

                case EVENT_CLOSE_CHANNEL_DONE:
                    handleNullReturnEvent(msg, "iccCloseLogicalChannel");
                    break;

                case CMD_NV_READ_ITEM:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_NV_READ_ITEM_DONE, request);
                    defaultPhone.nvReadItem((Integer) request.argument, onCompleted,
                            request.workSource);
                    break;

                case EVENT_NV_READ_ITEM_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;     // String
                    } else {
                        request.result = "";
                        if (ar.result == null) {
                            loge("nvReadItem: Empty response");
                        } else if (ar.exception instanceof CommandException) {
                            loge("nvReadItem: CommandException: " +
                                    ar.exception);
                        } else {
                            loge("nvReadItem: Unknown exception");
                        }
                    }
                    notifyRequester(request);
                    break;

                case CMD_NV_WRITE_ITEM:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_NV_WRITE_ITEM_DONE, request);
                    Pair<Integer, String> idValue = (Pair<Integer, String>) request.argument;
                    defaultPhone.nvWriteItem(idValue.first, idValue.second, onCompleted,
                            request.workSource);
                    break;

                case EVENT_NV_WRITE_ITEM_DONE:
                    handleNullReturnEvent(msg, "nvWriteItem");
                    break;

                case CMD_NV_WRITE_CDMA_PRL:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_NV_WRITE_CDMA_PRL_DONE, request);
                    defaultPhone.nvWriteCdmaPrl((byte[]) request.argument, onCompleted);
                    break;

                case EVENT_NV_WRITE_CDMA_PRL_DONE:
                    handleNullReturnEvent(msg, "nvWriteCdmaPrl");
                    break;

                case CMD_RESET_MODEM_CONFIG:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_RESET_MODEM_CONFIG_DONE, request);
                    defaultPhone.resetModemConfig(onCompleted);
                    break;

                case EVENT_RESET_MODEM_CONFIG_DONE:
                    handleNullReturnEvent(msg, "resetModemConfig");
                    break;

                case CMD_GET_PREFERRED_NETWORK_TYPE:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_GET_PREFERRED_NETWORK_TYPE_DONE, request);
                    getPhoneFromRequest(request).getPreferredNetworkType(onCompleted);
                    break;

                case EVENT_GET_PREFERRED_NETWORK_TYPE_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;     // Integer
                    } else {
                        request.result = null;
                        if (ar.result == null) {
                            loge("getPreferredNetworkType: Empty response");
                        } else if (ar.exception instanceof CommandException) {
                            loge("getPreferredNetworkType: CommandException: " +
                                    ar.exception);
                        } else {
                            loge("getPreferredNetworkType: Unknown exception");
                        }
                    }
                    notifyRequester(request);
                    break;

                case CMD_SET_PREFERRED_NETWORK_TYPE:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_SET_PREFERRED_NETWORK_TYPE_DONE, request);
                    int networkType = (Integer) request.argument;
                    getPhoneFromRequest(request).setPreferredNetworkType(networkType, onCompleted);
                    break;

                case EVENT_SET_PREFERRED_NETWORK_TYPE_DONE:
                    handleNullReturnEvent(msg, "setPreferredNetworkType");
                    break;

                case CMD_INVOKE_OEM_RIL_REQUEST_RAW:
                    request = (MainThreadRequest)msg.obj;
                    onCompleted = obtainMessage(EVENT_INVOKE_OEM_RIL_REQUEST_RAW_DONE, request);
                    defaultPhone.invokeOemRilRequestRaw((byte[]) request.argument, onCompleted);
                    break;

                case EVENT_INVOKE_OEM_RIL_REQUEST_RAW_DONE:
                    ar = (AsyncResult)msg.obj;
                    request = (MainThreadRequest)ar.userObj;
                    request.result = ar;
                    notifyRequester(request);
                    break;

                case CMD_SET_VOICEMAIL_NUMBER:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_SET_VOICEMAIL_NUMBER_DONE, request);
                    Pair<String, String> tagNum = (Pair<String, String>) request.argument;
                    getPhoneFromRequest(request).setVoiceMailNumber(tagNum.first, tagNum.second,
                            onCompleted);
                    break;

                case EVENT_SET_VOICEMAIL_NUMBER_DONE:
                    handleNullReturnEvent(msg, "setVoicemailNumber");
                    break;

                case CMD_SET_NETWORK_SELECTION_MODE_AUTOMATIC:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_SET_NETWORK_SELECTION_MODE_AUTOMATIC_DONE,
                            request);
                    getPhoneFromRequest(request).setNetworkSelectionModeAutomatic(onCompleted);
                    break;

                case EVENT_SET_NETWORK_SELECTION_MODE_AUTOMATIC_DONE:
                    handleNullReturnEvent(msg, "setNetworkSelectionModeAutomatic");
                    break;

                case CMD_PERFORM_NETWORK_SCAN:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_PERFORM_NETWORK_SCAN_DONE, request);
                    getPhoneFromRequest(request).getAvailableNetworks(onCompleted);
                    break;

                case CMD_GET_CALL_FORWARDING:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_GET_CALL_FORWARDING_DONE, request);
                    int callForwardingReason = (Integer) request.argument;
                    getPhoneFromRequest(request).getCallForwardingOption(
                            callForwardingReason, onCompleted);
                    break;

                case EVENT_GET_CALL_FORWARDING_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    CallForwardingInfo callForwardingInfo = null;
                    if (ar.exception == null && ar.result != null) {
                        CallForwardInfo[] callForwardInfos = (CallForwardInfo[]) ar.result;
                        for (CallForwardInfo callForwardInfo : callForwardInfos) {
                            // Service Class is a bit mask per 3gpp 27.007. Search for
                            // any service for voice call.
                            if ((callForwardInfo.serviceClass
                                    & CommandsInterface.SERVICE_CLASS_VOICE) > 0) {
                                callForwardingInfo = new CallForwardingInfo(
                                        callForwardInfo.serviceClass, callForwardInfo.reason,
                                                callForwardInfo.number,
                                                        callForwardInfo.timeSeconds);
                                break;
                            }
                        }
                        // Didn't find a call forward info for voice call.
                        if (callForwardingInfo == null) {
                            callForwardingInfo = new CallForwardingInfo(
                                    CallForwardingInfo.STATUS_UNKNOWN_ERROR,
                                            0 /* reason */, null /* number */, 0 /* timeout */);
                        }
                    } else {
                        if (ar.result == null) {
                            loge("EVENT_GET_CALL_FORWARDING_DONE: Empty response");
                        }
                        if (ar.exception != null) {
                            loge("EVENT_GET_CALL_FORWARDING_DONE: Exception: " + ar.exception);
                        }
                        int errorCode = CallForwardingInfo.STATUS_UNKNOWN_ERROR;
                        if (ar.exception instanceof CommandException) {
                            CommandException.Error error =
                                    ((CommandException) (ar.exception)).getCommandError();
                            if (error == CommandException.Error.FDN_CHECK_FAILURE) {
                                errorCode = CallForwardingInfo.STATUS_FDN_CHECK_FAILURE;
                            } else if (error == CommandException.Error.REQUEST_NOT_SUPPORTED) {
                                errorCode = CallForwardingInfo.STATUS_NOT_SUPPORTED;
                            }
                        }
                        callForwardingInfo = new CallForwardingInfo(
                                errorCode, 0 /* reason */, null /* number */, 0 /* timeout */);
                    }
                    request.result = callForwardingInfo;
                    notifyRequester(request);
                    break;

                case CMD_SET_CALL_FORWARDING:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_SET_CALL_FORWARDING_DONE, request);
                    CallForwardingInfo callForwardingInfoToSet =
                            (CallForwardingInfo) request.argument;
                    getPhoneFromRequest(request).setCallForwardingOption(
                            callForwardingInfoToSet.getStatus(),
                            callForwardingInfoToSet.getReason(),
                            callForwardingInfoToSet.getNumber(),
                            callForwardingInfoToSet.getTimeoutSeconds(), onCompleted);
                    break;

                case EVENT_SET_CALL_FORWARDING_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null) {
                        request.result = true;
                    } else {
                        request.result = false;
                        loge("setCallForwarding exception: " + ar.exception);
                    }
                    notifyRequester(request);
                    break;

                case CMD_GET_CALL_WAITING:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_GET_CALL_WAITING_DONE, request);
                    getPhoneFromRequest(request).getCallWaiting(onCompleted);
                    break;

                case EVENT_GET_CALL_WAITING_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    int callForwardingStatus = TelephonyManager.CALL_WAITING_STATUS_UNKNOWN_ERROR;
                    if (ar.exception == null && ar.result != null) {
                        int[] callForwardResults = (int[]) ar.result;
                        // Service Class is a bit mask per 3gpp 27.007.
                        // Search for any service for voice call.
                        if (callForwardResults.length > 1
                                && ((callForwardResults[1]
                                        & CommandsInterface.SERVICE_CLASS_VOICE) > 0)) {
                            callForwardingStatus = callForwardResults[0] == 0
                                    ? TelephonyManager.CALL_WAITING_STATUS_INACTIVE
                                            : TelephonyManager.CALL_WAITING_STATUS_ACTIVE;
                        } else {
                            callForwardingStatus = TelephonyManager.CALL_WAITING_STATUS_INACTIVE;
                        }
                    } else {
                        if (ar.result == null) {
                            loge("EVENT_GET_CALL_WAITING_DONE: Empty response");
                        }
                        if (ar.exception != null) {
                            loge("EVENT_GET_CALL_WAITING_DONE: Exception: " + ar.exception);
                        }
                        if (ar.exception instanceof CommandException) {
                            CommandException.Error error =
                                    ((CommandException) (ar.exception)).getCommandError();
                            if (error == CommandException.Error.REQUEST_NOT_SUPPORTED) {
                                callForwardingStatus =
                                        TelephonyManager.CALL_WAITING_STATUS_NOT_SUPPORTED;
                            }
                        }
                    }
                    request.result = callForwardingStatus;
                    notifyRequester(request);
                    break;

                case CMD_SET_CALL_WAITING:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_SET_CALL_WAITING_DONE, request);
                    boolean isEnable = (Boolean) request.argument;
                    getPhoneFromRequest(request).setCallWaiting(isEnable, onCompleted);
                    break;

                case EVENT_SET_CALL_WAITING_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null) {
                        request.result = true;
                    } else {
                        request.result = false;
                        loge("setCallWaiting exception: " + ar.exception);
                    }
                    notifyRequester(request);
                    break;

                case EVENT_PERFORM_NETWORK_SCAN_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    CellNetworkScanResult cellScanResult;
                    if (ar.exception == null && ar.result != null) {
                        cellScanResult = new CellNetworkScanResult(
                                CellNetworkScanResult.STATUS_SUCCESS,
                                (List<OperatorInfo>) ar.result);
                    } else {
                        if (ar.result == null) {
                            loge("getCellNetworkScanResults: Empty response");
                        }
                        if (ar.exception != null) {
                            loge("getCellNetworkScanResults: Exception: " + ar.exception);
                        }
                        int errorCode = CellNetworkScanResult.STATUS_UNKNOWN_ERROR;
                        if (ar.exception instanceof CommandException) {
                            CommandException.Error error =
                                ((CommandException) (ar.exception)).getCommandError();
                            if (error == CommandException.Error.RADIO_NOT_AVAILABLE) {
                                errorCode = CellNetworkScanResult.STATUS_RADIO_NOT_AVAILABLE;
                            } else if (error == CommandException.Error.GENERIC_FAILURE) {
                                errorCode = CellNetworkScanResult.STATUS_RADIO_GENERIC_FAILURE;
                            }
                        }
                        cellScanResult = new CellNetworkScanResult(errorCode, null);
                    }
                    request.result = cellScanResult;
                    notifyRequester(request);
                    break;

                case CMD_SET_NETWORK_SELECTION_MODE_MANUAL:
                    request = (MainThreadRequest) msg.obj;
                    ManualNetworkSelectionArgument selArg =
                            (ManualNetworkSelectionArgument) request.argument;
                    onCompleted = obtainMessage(EVENT_SET_NETWORK_SELECTION_MODE_MANUAL_DONE,
                            request);
                    getPhoneFromRequest(request).selectNetworkManually(selArg.operatorInfo,
                            selArg.persistSelection, onCompleted);
                    break;

                case EVENT_SET_NETWORK_SELECTION_MODE_MANUAL_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null) {
                        request.result = true;
                    } else {
                        request.result = false;
                        loge("setNetworkSelectionModeManual " + ar.exception);
                    }
                    notifyRequester(request);
                    mApp.onNetworkSelectionChanged(request.subId);
                    break;

                case CMD_GET_MODEM_ACTIVITY_INFO:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_GET_MODEM_ACTIVITY_INFO_DONE, request);
                    if (defaultPhone != null) {
                        defaultPhone.getModemActivityInfo(onCompleted, request.workSource);
                    } else {
                        ResultReceiver result = (ResultReceiver) request.argument;
                        Bundle bundle = new Bundle();
                        bundle.putParcelable(TelephonyManager.MODEM_ACTIVITY_RESULT_KEY,
                                new ModemActivityInfo(0, 0, 0, new int[0], 0));
                        result.send(0, bundle);
                    }
                    break;

                case EVENT_GET_MODEM_ACTIVITY_INFO_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    ResultReceiver result = (ResultReceiver) request.argument;

                    ModemActivityInfo ret = new ModemActivityInfo(0, 0, 0, new int[0], 0);
                    if (ar.exception == null && ar.result != null) {
                        // Update the last modem activity info and the result of the request.
                        ModemActivityInfo info = (ModemActivityInfo) ar.result;
                        if (isModemActivityInfoValid(info)) {
                            int[] mergedTxTimeMs = new int[ModemActivityInfo.TX_POWER_LEVELS];
                            int[] txTimeMs = info.getTransmitTimeMillis();
                            int[] lastModemTxTimeMs = mLastModemActivityInfo
                                    .getTransmitTimeMillis();
                            for (int i = 0; i < mergedTxTimeMs.length; i++) {
                                mergedTxTimeMs[i] = txTimeMs[i] + lastModemTxTimeMs[i];
                            }
                            mLastModemActivityInfo.setTimestamp(info.getTimestamp());
                            mLastModemActivityInfo.setSleepTimeMillis(info.getSleepTimeMillis()
                                    + mLastModemActivityInfo.getSleepTimeMillis());
                            mLastModemActivityInfo.setIdleTimeMillis(info.getIdleTimeMillis()
                                    + mLastModemActivityInfo.getIdleTimeMillis());
                            mLastModemActivityInfo.setTransmitTimeMillis(mergedTxTimeMs);
                            mLastModemActivityInfo.setReceiveTimeMillis(
                                    info.getReceiveTimeMillis()
                                            + mLastModemActivityInfo.getReceiveTimeMillis());
                        }
                        ret = new ModemActivityInfo(mLastModemActivityInfo.getTimestamp(),
                                mLastModemActivityInfo.getSleepTimeMillis(),
                                mLastModemActivityInfo.getIdleTimeMillis(),
                                mLastModemActivityInfo.getTransmitTimeMillis(),
                                mLastModemActivityInfo.getReceiveTimeMillis());
                    } else {
                        if (ar.result == null) {
                            loge("queryModemActivityInfo: Empty response");
                        } else if (ar.exception instanceof CommandException) {
                            loge("queryModemActivityInfo: CommandException: " +
                                    ar.exception);
                        } else {
                            loge("queryModemActivityInfo: Unknown exception");
                        }
                    }
                    Bundle bundle = new Bundle();
                    bundle.putParcelable(TelephonyManager.MODEM_ACTIVITY_RESULT_KEY, ret);
                    result.send(0, bundle);
                    notifyRequester(request);
                    break;

                case CMD_SET_ALLOWED_CARRIERS:
                    request = (MainThreadRequest) msg.obj;
                    CarrierRestrictionRules argument =
                            (CarrierRestrictionRules) request.argument;
                    onCompleted = obtainMessage(EVENT_SET_ALLOWED_CARRIERS_DONE, request);
                    defaultPhone.setAllowedCarriers(argument, onCompleted, request.workSource);
                    break;

                case EVENT_SET_ALLOWED_CARRIERS_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;
                    } else {
                        request.result = TelephonyManager.SET_CARRIER_RESTRICTION_ERROR;
                        if (ar.exception instanceof CommandException) {
                            loge("setAllowedCarriers: CommandException: " + ar.exception);
                            CommandException.Error error =
                                    ((CommandException) (ar.exception)).getCommandError();
                            if (error == CommandException.Error.REQUEST_NOT_SUPPORTED) {
                                request.result =
                                        TelephonyManager.SET_CARRIER_RESTRICTION_NOT_SUPPORTED;
                            }
                        } else {
                            loge("setAllowedCarriers: Unknown exception");
                        }
                    }
                    notifyRequester(request);
                    break;

                case CMD_GET_ALLOWED_CARRIERS:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_GET_ALLOWED_CARRIERS_DONE, request);
                    defaultPhone.getAllowedCarriers(onCompleted, request.workSource);
                    break;

                case EVENT_GET_ALLOWED_CARRIERS_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;
                    } else {
                        request.result = new IllegalStateException(
                            "Failed to get carrier restrictions");
                        if (ar.result == null) {
                            loge("getAllowedCarriers: Empty response");
                        } else if (ar.exception instanceof CommandException) {
                            loge("getAllowedCarriers: CommandException: " +
                                    ar.exception);
                        } else {
                            loge("getAllowedCarriers: Unknown exception");
                        }
                    }
                    notifyRequester(request);
                    break;

                case EVENT_GET_FORBIDDEN_PLMNS_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;
                    } else {
                        request.result = new IllegalArgumentException(
                                "Failed to retrieve Forbidden Plmns");
                        if (ar.result == null) {
                            loge("getForbiddenPlmns: Empty response");
                        } else {
                            loge("getForbiddenPlmns: Unknown exception");
                        }
                    }
                    notifyRequester(request);
                    break;

                case CMD_GET_FORBIDDEN_PLMNS:
                    request = (MainThreadRequest) msg.obj;
                    uiccCard = getUiccCardFromRequest(request);
                    if (uiccCard == null) {
                        loge("getForbiddenPlmns() UiccCard is null");
                        request.result = new IllegalArgumentException(
                                "getForbiddenPlmns() UiccCard is null");
                        notifyRequester(request);
                        break;
                    }
                    Integer appType = (Integer) request.argument;
                    UiccCardApplication uiccApp = uiccCard.getApplicationByType(appType);
                    if (uiccApp == null) {
                        loge("getForbiddenPlmns() no app with specified type -- "
                                + appType);
                        request.result = new IllegalArgumentException("Failed to get UICC App");
                        notifyRequester(request);
                        break;
                    } else {
                        if (DBG) logv("getForbiddenPlmns() found app " + uiccApp.getAid()
                                + " specified type -- " + appType);
                    }
                    onCompleted = obtainMessage(EVENT_GET_FORBIDDEN_PLMNS_DONE, request);
                    ((SIMRecords) uiccApp.getIccRecords()).getForbiddenPlmns(
                              onCompleted);
                    break;

                case CMD_SWITCH_SLOTS:
                    request = (MainThreadRequest) msg.obj;
                    int[] physicalSlots = (int[]) request.argument;
                    onCompleted = obtainMessage(EVENT_SWITCH_SLOTS_DONE, request);
                    UiccController.getInstance().switchSlots(physicalSlots, onCompleted);
                    break;

                case EVENT_SWITCH_SLOTS_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    request.result = (ar.exception == null);
                    notifyRequester(request);
                    break;
                case CMD_GET_NETWORK_SELECTION_MODE:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_GET_NETWORK_SELECTION_MODE_DONE, request);
                    getPhoneFromRequest(request).getNetworkSelectionMode(onCompleted);
                    break;

                case EVENT_GET_NETWORK_SELECTION_MODE_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception != null) {
                        request.result = TelephonyManager.NETWORK_SELECTION_MODE_UNKNOWN;
                    } else {
                        int mode = ((int[]) ar.result)[0];
                        if (mode == 0) {
                            request.result = TelephonyManager.NETWORK_SELECTION_MODE_AUTO;
                        } else {
                            request.result = TelephonyManager.NETWORK_SELECTION_MODE_MANUAL;
                        }
                    }
                    notifyRequester(request);
                    break;
                case CMD_GET_CDMA_ROAMING_MODE:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_GET_CDMA_ROAMING_MODE_DONE, request);
                    getPhoneFromRequest(request).queryCdmaRoamingPreference(onCompleted);
                    break;
                case EVENT_GET_CDMA_ROAMING_MODE_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception != null) {
                        request.result = TelephonyManager.CDMA_ROAMING_MODE_RADIO_DEFAULT;
                    } else {
                        request.result = ((int[]) ar.result)[0];
                    }
                    notifyRequester(request);
                    break;
                case CMD_SET_CDMA_ROAMING_MODE:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_SET_CDMA_ROAMING_MODE_DONE, request);
                    int mode = (int) request.argument;
                    getPhoneFromRequest(request).setCdmaRoamingPreference(mode, onCompleted);
                    break;
                case EVENT_SET_CDMA_ROAMING_MODE_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    request.result = ar.exception == null;
                    notifyRequester(request);
                    break;
                case CMD_SET_CDMA_SUBSCRIPTION_MODE:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_SET_CDMA_SUBSCRIPTION_MODE_DONE, request);
                    int subscriptionMode = (int) request.argument;
                    getPhoneFromRequest(request).setCdmaSubscription(subscriptionMode, onCompleted);
                    break;
                case EVENT_SET_CDMA_SUBSCRIPTION_MODE_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    request.result = ar.exception == null;
                    notifyRequester(request);
                    break;
                case CMD_GET_ALL_CELL_INFO:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_GET_ALL_CELL_INFO_DONE, request);
                    request.phone.requestCellInfoUpdate(request.workSource, onCompleted);
                    break;
                case EVENT_GET_ALL_CELL_INFO_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    // If a timeout occurs, the response will be null
                    request.result = (ar.exception == null && ar.result != null)
                            ? ar.result : new ArrayList<CellInfo>();
                    synchronized (request) {
                        request.notifyAll();
                    }
                    break;
                case CMD_REQUEST_CELL_INFO_UPDATE:
                    request = (MainThreadRequest) msg.obj;
                    request.phone.requestCellInfoUpdate(request.workSource,
                            obtainMessage(EVENT_REQUEST_CELL_INFO_UPDATE_DONE, request));
                    break;
                case EVENT_REQUEST_CELL_INFO_UPDATE_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    ICellInfoCallback cb = (ICellInfoCallback) request.argument;
                    try {
                        if (ar.exception != null) {
                            Log.e(LOG_TAG, "Exception retrieving CellInfo=" + ar.exception);
                            cb.onError(
                                    TelephonyManager.CellInfoCallback.ERROR_MODEM_ERROR,
                                    ar.exception.getClass().getName(),
                                    ar.exception.toString());
                        } else if (ar.result == null) {
                            Log.w(LOG_TAG, "Timeout Waiting for CellInfo!");
                            cb.onError(TelephonyManager.CellInfoCallback.ERROR_TIMEOUT, null, null);
                        } else {
                            // use the result as returned
                            cb.onCellInfo((List<CellInfo>) ar.result);
                        }
                    } catch (RemoteException re) {
                        Log.w(LOG_TAG, "Discarded CellInfo due to Callback RemoteException");
                    }
                    break;
                case CMD_GET_CELL_LOCATION:
                    request = (MainThreadRequest) msg.obj;
                    WorkSource ws = (WorkSource) request.argument;
                    Phone phone = getPhoneFromRequest(request);
                    phone.getCellIdentity(ws, obtainMessage(EVENT_GET_CELL_LOCATION_DONE, request));
                    break;
                case EVENT_GET_CELL_LOCATION_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null) {
                        request.result = ar.result;
                    } else {
                        phone = getPhoneFromRequest(request);
                        request.result = (phone.getPhoneType() == PhoneConstants.PHONE_TYPE_CDMA)
                                ? new CellIdentityCdma() : new CellIdentityGsm();
                    }

                    synchronized (request) {
                        request.notifyAll();
                    }
                    break;
                case CMD_MODEM_REBOOT:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_RESET_MODEM_CONFIG_DONE, request);
                    defaultPhone.rebootModem(onCompleted);
                    break;
                case EVENT_CMD_MODEM_REBOOT_DONE:
                    handleNullReturnEvent(msg, "rebootModem");
                    break;
                case CMD_REQUEST_ENABLE_MODEM:
                    request = (MainThreadRequest) msg.obj;
                    boolean enable = (boolean) request.argument;
                    onCompleted = obtainMessage(EVENT_ENABLE_MODEM_DONE, request);
                    onCompleted.arg1 = enable ? 1 : 0;
                    PhoneConfigurationManager.getInstance()
                            .enablePhone(request.phone, enable, onCompleted);
                    break;
                case EVENT_ENABLE_MODEM_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    request.result = (ar.exception == null);
                    int phoneId = request.phone.getPhoneId();
                    //update the cache as modem status has changed
                    if ((boolean) request.result) {
                        mPhoneConfigurationManager.addToPhoneStatusCache(phoneId, msg.arg1 == 1);
                        updateModemStateMetrics();
                    } else {
                        Log.e(LOG_TAG, msg.what + " failure. Not updating modem status."
                                + ar.exception);
                    }
                    notifyRequester(request);
                    break;
                case CMD_GET_MODEM_STATUS:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_GET_MODEM_STATUS_DONE, request);
                    PhoneConfigurationManager.getInstance()
                            .getPhoneStatusFromModem(request.phone, onCompleted);
                    break;
                case EVENT_GET_MODEM_STATUS_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    int id = request.phone.getPhoneId();
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;
                        //update the cache as modem status has changed
                        mPhoneConfigurationManager.addToPhoneStatusCache(id,
                                (boolean) request.result);
                    } else {
                        // Return true if modem status cannot be retrieved. For most cases,
                        // modem status is on. And for older version modems, GET_MODEM_STATUS
                        // and disable modem are not supported. Modem is always on.
                        // TODO: this should be fixed in R to support a third
                        // status UNKNOWN b/131631629
                        request.result = true;
                        Log.e(LOG_TAG, msg.what + " failure. Not updating modem status."
                                + ar.exception);
                    }
                    notifyRequester(request);
                    break;
                case CMD_SET_SYSTEM_SELECTION_CHANNELS: {
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_SET_SYSTEM_SELECTION_CHANNELS_DONE, request);
                    Pair<List<RadioAccessSpecifier>, Consumer<Boolean>> args =
                            (Pair<List<RadioAccessSpecifier>, Consumer<Boolean>>) request.argument;
                    request.phone.setSystemSelectionChannels(args.first, onCompleted);
                    break;
                }
                case EVENT_SET_SYSTEM_SELECTION_CHANNELS_DONE: {
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    Pair<List<RadioAccessSpecifier>, Consumer<Boolean>> args =
                            (Pair<List<RadioAccessSpecifier>, Consumer<Boolean>>) request.argument;
                    args.second.accept(ar.exception == null);
                    notifyRequester(request);
                    break;
                }
                case EVENT_SET_FORBIDDEN_PLMNS_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null && ar.result != null) {
                        request.result = ar.result;
                    } else {
                        request.result = -1;
                        loge("Failed to set Forbidden Plmns");
                        if (ar.result == null) {
                            loge("setForbidenPlmns: Empty response");
                        } else if (ar.exception != null) {
                            loge("setForbiddenPlmns: Exception: " + ar.exception);
                            request.result = -1;
                        } else {
                            loge("setForbiddenPlmns: Unknown exception");
                        }
                    }
                    notifyRequester(request);
                    break;
                case CMD_SET_FORBIDDEN_PLMNS:
                    request = (MainThreadRequest) msg.obj;
                    uiccCard = getUiccCardFromRequest(request);
                    if (uiccCard == null) {
                        loge("setForbiddenPlmns: UiccCard is null");
                        request.result = -1;
                        notifyRequester(request);
                        break;
                    }
                    Pair<Integer, List<String>> setFplmnsArgs =
                            (Pair<Integer, List<String>>) request.argument;
                    appType = setFplmnsArgs.first;
                    List<String> fplmns = setFplmnsArgs.second;
                    uiccApp = uiccCard.getApplicationByType(appType);
                    if (uiccApp == null) {
                        loge("setForbiddenPlmns: no app with specified type -- " + appType);
                        request.result = -1;
                        loge("Failed to get UICC App");
                        notifyRequester(request);
                    } else {
                        onCompleted = obtainMessage(EVENT_SET_FORBIDDEN_PLMNS_DONE, request);
                        ((SIMRecords) uiccApp.getIccRecords())
                                .setForbiddenPlmns(onCompleted, fplmns);
                    }
                    break;
                case CMD_ERASE_MODEM_CONFIG:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_ERASE_MODEM_CONFIG_DONE, request);
                    defaultPhone.eraseModemConfig(onCompleted);
                    break;
                case EVENT_ERASE_MODEM_CONFIG_DONE:
                    handleNullReturnEvent(msg, "eraseModemConfig");
                    break;

                case CMD_CHANGE_ICC_LOCK_PASSWORD:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_CHANGE_ICC_LOCK_PASSWORD_DONE, request);
                    Pair<String, String> changed = (Pair<String, String>) request.argument;
                    getPhoneFromRequest(request).getIccCard().changeIccLockPassword(
                            changed.first, changed.second, onCompleted);
                    break;
                case EVENT_CHANGE_ICC_LOCK_PASSWORD_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null) {
                        request.result = TelephonyManager.CHANGE_ICC_LOCK_SUCCESS;
                    } else {
                        request.result = msg.arg1;
                    }
                    notifyRequester(request);
                    break;

                case CMD_SET_ICC_LOCK_ENABLED:
                    request = (MainThreadRequest) msg.obj;
                    onCompleted = obtainMessage(EVENT_SET_ICC_LOCK_ENABLED_DONE, request);
                    Pair<Boolean, String> enabled = (Pair<Boolean, String>) request.argument;
                    getPhoneFromRequest(request).getIccCard().setIccLockEnabled(
                            enabled.first, enabled.second, onCompleted);
                    break;
                case EVENT_SET_ICC_LOCK_ENABLED_DONE:
                    ar = (AsyncResult) msg.obj;
                    request = (MainThreadRequest) ar.userObj;
                    if (ar.exception == null) {
                        request.result = TelephonyManager.CHANGE_ICC_LOCK_SUCCESS;
                    } else {
                        request.result = msg.arg1;
                    }
                    notifyRequester(request);
                    break;

                case MSG_NOTIFY_USER_ACTIVITY:
                    removeMessages(MSG_NOTIFY_USER_ACTIVITY);
                    Intent intent = new Intent(TelephonyIntents.ACTION_USER_ACTIVITY_NOTIFICATION);
                    intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
                    getDefaultPhone().getContext().sendBroadcastAsUser(
                            intent, UserHandle.ALL, permission.USER_ACTIVITY);
                    break;
                default:
                    Log.w(LOG_TAG, "MainThreadHandler: unexpected message code: " + msg.what);
                    break;
            }
        }

        private void notifyRequester(MainThreadRequest request) {
            synchronized (request) {
                request.notifyAll();
            }
        }

        private void handleNullReturnEvent(Message msg, String command) {
            AsyncResult ar = (AsyncResult) msg.obj;
            MainThreadRequest request = (MainThreadRequest) ar.userObj;
            if (ar.exception == null) {
                request.result = true;
            } else {
                request.result = false;
                if (ar.exception instanceof CommandException) {
                    loge(command + ": CommandException: " + ar.exception);
                } else {
                    loge(command + ": Unknown exception");
                }
            }
            notifyRequester(request);
        }
    }

    /**
     * Posts the specified command to be executed on the main thread,
     * waits for the request to complete, and returns the result.
     * @see #sendRequestAsync
     */
    private Object sendRequest(int command, Object argument) {
        return sendRequest(
                command, argument, SubscriptionManager.INVALID_SUBSCRIPTION_ID, null, null);
    }

    /**
     * Posts the specified command to be executed on the main thread,
     * waits for the request to complete, and returns the result.
     * @see #sendRequestAsync
     */
    private Object sendRequest(int command, Object argument, WorkSource workSource) {
        return sendRequest(command, argument,  SubscriptionManager.INVALID_SUBSCRIPTION_ID,
                null, workSource);
    }

    /**
     * Posts the specified command to be executed on the main thread,
     * waits for the request to complete, and returns the result.
     * @see #sendRequestAsync
     */
    private Object sendRequest(int command, Object argument, Integer subId) {
        return sendRequest(command, argument, subId, null, null);
    }

    /**
     * Posts the specified command to be executed on the main thread,
     * waits for the request to complete, and returns the result.
     * @see #sendRequestAsync
     */
    private Object sendRequest(int command, Object argument, int subId, WorkSource workSource) {
        return sendRequest(command, argument, subId, null, workSource);
    }

    /**
     * Posts the specified command to be executed on the main thread,
     * waits for the request to complete, and returns the result.
     * @see #sendRequestAsync
     */
    private Object sendRequest(int command, Object argument, Phone phone, WorkSource workSource) {
        return sendRequest(
                command, argument, SubscriptionManager.INVALID_SUBSCRIPTION_ID, phone, workSource);
    }

    /**
     * Posts the specified command to be executed on the main thread,
     * waits for the request to complete, and returns the result.
     * @see #sendRequestAsync
     */
    private Object sendRequest(
            int command, Object argument, Integer subId, Phone phone, WorkSource workSource) {
        if (Looper.myLooper() == mMainThreadHandler.getLooper()) {
            throw new RuntimeException("This method will deadlock if called from the main thread.");
        }

        MainThreadRequest request = null;
        if (subId != SubscriptionManager.INVALID_SUBSCRIPTION_ID && phone != null) {
            throw new IllegalArgumentException("subId and phone cannot both be specified!");
        } else if (phone != null) {
            request = new MainThreadRequest(argument, phone, workSource);
        } else {
            request = new MainThreadRequest(argument, subId, workSource);
        }

        Message msg = mMainThreadHandler.obtainMessage(command, request);
        msg.sendToTarget();

        // Wait for the request to complete
        synchronized (request) {
            while (request.result == null) {
                try {
                    request.wait();
                } catch (InterruptedException e) {
                    // Do nothing, go back and wait until the request is complete
                }
            }
        }
        return request.result;
    }

    /**
     * Asynchronous ("fire and forget") version of sendRequest():
     * Posts the specified command to be executed on the main thread, and
     * returns immediately.
     * @see #sendRequest
     */
    private void sendRequestAsync(int command) {
        mMainThreadHandler.sendEmptyMessage(command);
    }

    /**
     * Same as {@link #sendRequestAsync(int)} except it takes an argument.
     * @see {@link #sendRequest(int)}
     */
    private void sendRequestAsync(int command, Object argument) {
        sendRequestAsync(command, argument, null, null);
    }

    /**
     * Same as {@link #sendRequestAsync(int,Object)} except it takes a Phone and WorkSource.
     * @see {@link #sendRequest(int,Object)}
     */
    private void sendRequestAsync(
            int command, Object argument, Phone phone, WorkSource workSource) {
        MainThreadRequest request = new MainThreadRequest(argument, phone, workSource);
        Message msg = mMainThreadHandler.obtainMessage(command, request);
        msg.sendToTarget();
    }

    /**
     * Initialize the singleton PhoneInterfaceManager instance.
     * This is only done once, at startup, from PhoneApp.onCreate().
     */
    /* package */ static PhoneInterfaceManager init(PhoneGlobals app) {
        synchronized (PhoneInterfaceManager.class) {
            if (sInstance == null) {
                sInstance = new PhoneInterfaceManager(app);
            } else {
                Log.wtf(LOG_TAG, "init() called multiple times!  sInstance = " + sInstance);
            }
            return sInstance;
        }
    }

    /** Private constructor; @see init() */
    private PhoneInterfaceManager(PhoneGlobals app) {
        mApp = app;
        mCM = PhoneGlobals.getInstance().mCM;
        mImsResolver = PhoneGlobals.getInstance().getImsResolver();
        mUserManager = (UserManager) app.getSystemService(Context.USER_SERVICE);
        mAppOps = (AppOpsManager)app.getSystemService(Context.APP_OPS_SERVICE);
        mMainThreadHandler = new MainThreadHandler();
        mSubscriptionController = SubscriptionController.getInstance();
        mTelephonySharedPreferences =
                PreferenceManager.getDefaultSharedPreferences(mApp);
        mNetworkScanRequestTracker = new NetworkScanRequestTracker();
        mPhoneConfigurationManager = PhoneConfigurationManager.getInstance();
        mNotifyUserActivity = new AtomicBoolean(false);

        publish();
    }

    private Phone getDefaultPhone() {
        Phone thePhone = getPhone(getDefaultSubscription());
        return (thePhone != null) ? thePhone : PhoneFactory.getDefaultPhone();
    }

    private void publish() {
        if (DBG) log("publish: " + this);

        TelephonyFrameworkInitializer
                .getTelephonyServiceManager()
                .getTelephonyServiceRegisterer()
                .register(this);
    }

    private Phone getPhoneFromRequest(MainThreadRequest request) {
        if (request.phone != null) {
            return request.phone;
        } else {
            return getPhoneFromSubId(request.subId);
        }
    }

    private Phone getPhoneFromSubId(int subId) {
        return (subId == SubscriptionManager.INVALID_SUBSCRIPTION_ID)
                ? getDefaultPhone() : getPhone(subId);
    }

    private UiccCard getUiccCardFromRequest(MainThreadRequest request) {
        Phone phone = getPhoneFromRequest(request);
        return phone == null ? null :
                UiccController.getInstance().getUiccCard(phone.getPhoneId());
    }

    // returns phone associated with the subId.
    private Phone getPhone(int subId) {
        return PhoneFactory.getPhone(mSubscriptionController.getPhoneId(subId));
    }

    private void sendEraseModemConfig(Phone phone) {
        if (phone != null) {
            TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                  mApp, phone.getSubId(), "eraseModemConfig");
            final long identity = Binder.clearCallingIdentity();
            try {
                Boolean success = (Boolean) sendRequest(CMD_ERASE_MODEM_CONFIG, null);
                if (DBG) log("eraseModemConfig:" + ' ' + (success ? "ok" : "fail"));
            } finally {
                Binder.restoreCallingIdentity(identity);
            }
        }
    }

    private boolean isImsAvailableOnDevice() {
        PackageManager pm = getDefaultPhone().getContext().getPackageManager();
        if (pm == null) {
            // For some reason package manger is not available.. This will fail internally anyway,
            // so do not throw error and allow.
            return true;
        }
        return pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_IMS, 0);
    }

    public void dial(String number) {
        dialForSubscriber(getPreferredVoiceSubscription(), number);
    }

    public void dialForSubscriber(int subId, String number) {
        if (DBG) log("dial: " + number);
        // No permission check needed here: This is just a wrapper around the
        // ACTION_DIAL intent, which is available to any app since it puts up
        // the UI before it does anything.

        final long identity = Binder.clearCallingIdentity();
        try {
            String url = createTelUrl(number);
            if (url == null) {
                return;
            }

            // PENDING: should we just silently fail if phone is offhook or ringing?
            PhoneConstants.State state = mCM.getState(subId);
            if (state != PhoneConstants.State.OFFHOOK && state != PhoneConstants.State.RINGING) {
                Intent intent = new Intent(Intent.ACTION_DIAL, Uri.parse(url));
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                mApp.startActivity(intent);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public void call(String callingPackage, String number) {
        callForSubscriber(getPreferredVoiceSubscription(), callingPackage, number);
    }

    public void callForSubscriber(int subId, String callingPackage, String number) {
        if (DBG) log("call: " + number);

        // This is just a wrapper around the ACTION_CALL intent, but we still
        // need to do a permission check since we're calling startActivity()
        // from the context of the phone app.
        enforceCallPermission();

        if (mAppOps.noteOp(AppOpsManager.OPSTR_CALL_PHONE, Binder.getCallingUid(), callingPackage)
                != AppOpsManager.MODE_ALLOWED) {
            return;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            String url = createTelUrl(number);
            if (url == null) {
                return;
            }

            boolean isValid = false;
            final List<SubscriptionInfo> slist = getActiveSubscriptionInfoListPrivileged();
            if (slist != null) {
                for (SubscriptionInfo subInfoRecord : slist) {
                    if (subInfoRecord.getSubscriptionId() == subId) {
                        isValid = true;
                        break;
                    }
                }
            }
            if (!isValid) {
                return;
            }

            Intent intent = new Intent(Intent.ACTION_CALL, Uri.parse(url));
            intent.putExtra(SUBSCRIPTION_KEY, subId);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mApp.startActivity(intent);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public boolean supplyPinForSubscriber(int subId, String pin) {
        int [] resultArray = supplyPinReportResultForSubscriber(subId, pin);
        return (resultArray[0] == PhoneConstants.PIN_RESULT_SUCCESS) ? true : false;
    }

    public boolean supplyPukForSubscriber(int subId, String puk, String pin) {
        int [] resultArray = supplyPukReportResultForSubscriber(subId, puk, pin);
        return (resultArray[0] == PhoneConstants.PIN_RESULT_SUCCESS) ? true : false;
    }

    public int[] supplyPinReportResultForSubscriber(int subId, String pin) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            final UnlockSim checkSimPin = new UnlockSim(getPhone(subId).getIccCard());
            checkSimPin.start();
            return checkSimPin.unlockSim(null, pin);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public int[] supplyPukReportResultForSubscriber(int subId, String puk, String pin) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            final UnlockSim checkSimPuk = new UnlockSim(getPhone(subId).getIccCard());
            checkSimPuk.start();
            return checkSimPuk.unlockSim(puk, pin);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Helper thread to turn async call to SimCard#supplyPin into
     * a synchronous one.
     */
    private static class UnlockSim extends Thread {

        private final IccCard mSimCard;

        private boolean mDone = false;
        private int mResult = PhoneConstants.PIN_GENERAL_FAILURE;
        private int mRetryCount = -1;

        // For replies from SimCard interface
        private Handler mHandler;

        // For async handler to identify request type
        private static final int SUPPLY_PIN_COMPLETE = 100;

        public UnlockSim(IccCard simCard) {
            mSimCard = simCard;
        }

        @Override
        public void run() {
            Looper.prepare();
            synchronized (UnlockSim.this) {
                mHandler = new Handler() {
                    @Override
                    public void handleMessage(Message msg) {
                        AsyncResult ar = (AsyncResult) msg.obj;
                        switch (msg.what) {
                            case SUPPLY_PIN_COMPLETE:
                                Log.d(LOG_TAG, "SUPPLY_PIN_COMPLETE");
                                synchronized (UnlockSim.this) {
                                    mRetryCount = msg.arg1;
                                    if (ar.exception != null) {
                                        if (ar.exception instanceof CommandException &&
                                                ((CommandException)(ar.exception)).getCommandError()
                                                == CommandException.Error.PASSWORD_INCORRECT) {
                                            mResult = PhoneConstants.PIN_PASSWORD_INCORRECT;
                                        } else {
                                            mResult = PhoneConstants.PIN_GENERAL_FAILURE;
                                        }
                                    } else {
                                        mResult = PhoneConstants.PIN_RESULT_SUCCESS;
                                    }
                                    mDone = true;
                                    UnlockSim.this.notifyAll();
                                }
                                break;
                        }
                    }
                };
                UnlockSim.this.notifyAll();
            }
            Looper.loop();
        }

        /*
         * Use PIN or PUK to unlock SIM card
         *
         * If PUK is null, unlock SIM card with PIN
         *
         * If PUK is not null, unlock SIM card with PUK and set PIN code
         */
        synchronized int[] unlockSim(String puk, String pin) {

            while (mHandler == null) {
                try {
                    wait();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
            Message callback = Message.obtain(mHandler, SUPPLY_PIN_COMPLETE);

            if (puk == null) {
                mSimCard.supplyPin(pin, callback);
            } else {
                mSimCard.supplyPuk(puk, pin, callback);
            }

            while (!mDone) {
                try {
                    Log.d(LOG_TAG, "wait for done");
                    wait();
                } catch (InterruptedException e) {
                    // Restore the interrupted status
                    Thread.currentThread().interrupt();
                }
            }
            Log.d(LOG_TAG, "done");
            int[] resultArray = new int[2];
            resultArray[0] = mResult;
            resultArray[1] = mRetryCount;
            return resultArray;
        }
    }

    public void updateServiceLocation() {
        updateServiceLocationForSubscriber(getDefaultSubscription());

    }

    public void updateServiceLocationForSubscriber(int subId) {
        // No permission check needed here: this call is harmless, and it's
        // needed for the ServiceState.requestStateUpdate() call (which is
        // already intentionally exposed to 3rd parties.)
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                phone.updateServiceLocation();
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Deprecated
    @Override
    public boolean isRadioOn(String callingPackage) {
        return isRadioOnWithFeature(callingPackage, null);
    }


    @Override
    public boolean isRadioOnWithFeature(String callingPackage, String callingFeatureId) {
        return isRadioOnForSubscriberWithFeature(getDefaultSubscription(), callingPackage,
                callingFeatureId);
    }

    @Deprecated
    @Override
    public boolean isRadioOnForSubscriber(int subId, String callingPackage) {
        return isRadioOnForSubscriberWithFeature(subId, callingPackage, null);
    }

    @Override
    public boolean isRadioOnForSubscriberWithFeature(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId, "isRadioOnForSubscriber")) {
            return false;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return isRadioOnForSubscriber(subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private boolean isRadioOnForSubscriber(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.getServiceState().getState() != ServiceState.STATE_POWER_OFF;
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public void toggleRadioOnOff() {
        toggleRadioOnOffForSubscriber(getDefaultSubscription());
    }

    public void toggleRadioOnOffForSubscriber(int subId) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                phone.setRadioPower(!isRadioOnForSubscriber(subId));
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public boolean setRadio(boolean turnOn) {
        return setRadioForSubscriber(getDefaultSubscription(), turnOn);
    }

    public boolean setRadioForSubscriber(int subId, boolean turnOn) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone == null) {
                return false;
            }
            if ((phone.getServiceState().getState() != ServiceState.STATE_POWER_OFF) != turnOn) {
                toggleRadioOnOffForSubscriber(subId);
            }
            return true;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public boolean needMobileRadioShutdown() {
        enforceReadPrivilegedPermission("needMobileRadioShutdown");
        /*
         * If any of the Radios are available, it will need to be
         * shutdown. So return true if any Radio is available.
         */
        final long identity = Binder.clearCallingIdentity();
        try {
            for (int i = 0; i < TelephonyManager.getDefault().getPhoneCount(); i++) {
                Phone phone = PhoneFactory.getPhone(i);
                if (phone != null && phone.isRadioAvailable()) return true;
            }
            logv(TelephonyManager.getDefault().getPhoneCount() + " Phones are shutdown.");
            return false;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void shutdownMobileRadios() {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            for (int i = 0; i < TelephonyManager.getDefault().getPhoneCount(); i++) {
                logv("Shutting down Phone " + i);
                shutdownRadioUsingPhoneId(i);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private void shutdownRadioUsingPhoneId(int phoneId) {
        Phone phone = PhoneFactory.getPhone(phoneId);
        if (phone != null && phone.isRadioAvailable()) {
            phone.shutdownRadio();
        }
    }

    public boolean setRadioPower(boolean turnOn) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone defaultPhone = PhoneFactory.getDefaultPhone();
            if (defaultPhone != null) {
                defaultPhone.setRadioPower(turnOn);
                return true;
            } else {
                loge("There's no default phone.");
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public boolean setRadioPowerForSubscriber(int subId, boolean turnOn) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                phone.setRadioPower(turnOn);
                return true;
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    // FIXME: subId version needed
    @Override
    public boolean enableDataConnectivity() {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            int subId = mSubscriptionController.getDefaultDataSubId();
            final Phone phone = getPhone(subId);
            if (phone != null) {
                phone.getDataEnabledSettings().setUserDataEnabled(true);
                return true;
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    // FIXME: subId version needed
    @Override
    public boolean disableDataConnectivity() {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            int subId = mSubscriptionController.getDefaultDataSubId();
            final Phone phone = getPhone(subId);
            if (phone != null) {
                phone.getDataEnabledSettings().setUserDataEnabled(false);
                return true;
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isDataConnectivityPossible(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.isDataAllowed(ApnSetting.TYPE_DEFAULT);
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public boolean handlePinMmi(String dialString) {
        return handlePinMmiForSubscriber(getDefaultSubscription(), dialString);
    }

    public void handleUssdRequest(int subId, String ussdRequest, ResultReceiver wrappedCallback) {
        enforceCallPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                return;
            }
            Pair<String, ResultReceiver> ussdObject = new Pair(ussdRequest, wrappedCallback);
            sendRequest(CMD_HANDLE_USSD_REQUEST, ussdObject, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    };

    public boolean handlePinMmiForSubscriber(int subId, String dialString) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (!SubscriptionManager.isValidSubscriptionId(subId)) {
                return false;
            }
            return (Boolean) sendRequest(CMD_HANDLE_PIN_MMI, dialString, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    public int getCallState() {
        return getCallStateForSlot(getSlotForDefaultSubscription());
    }

    public int getCallStateForSlot(int slotIndex) {
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = PhoneFactory.getPhone(slotIndex);
            return phone == null ? TelephonyManager.CALL_STATE_IDLE :
                    PhoneConstantConversions.convertCallState(phone.getState());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getDataState() {
        return getDataStateForSubId(mSubscriptionController.getDefaultDataSubId());
    }

    @Override
    public int getDataStateForSubId(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return PhoneConstantConversions.convertDataState(phone.getDataConnectionState());
            } else {
                return PhoneConstantConversions.convertDataState(
                        PhoneConstants.DataState.DISCONNECTED);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getDataActivity() {
        return getDataActivityForSubId(mSubscriptionController.getDefaultDataSubId());
    }

    @Override
    public int getDataActivityForSubId(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return DefaultPhoneNotifier.convertDataActivityState(phone.getDataActivityState());
            } else {
                return TelephonyManager.DATA_ACTIVITY_NONE;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public CellIdentity getCellLocation(String callingPackage, String callingFeatureId) {
        mApp.getSystemService(AppOpsManager.class)
                .checkPackage(Binder.getCallingUid(), callingPackage);

        LocationAccessPolicy.LocationPermissionResult locationResult =
                LocationAccessPolicy.checkLocationPermission(mApp,
                        new LocationAccessPolicy.LocationPermissionQuery.Builder()
                                .setCallingPackage(callingPackage)
                                .setCallingFeatureId(callingFeatureId)
                                .setCallingPid(Binder.getCallingPid())
                                .setCallingUid(Binder.getCallingUid())
                                .setMethod("getCellLocation")
                                .setMinSdkVersionForCoarse(Build.VERSION_CODES.BASE)
                                .setMinSdkVersionForFine(Build.VERSION_CODES.Q)
                                .build());
        switch (locationResult) {
            case DENIED_HARD:
                throw new SecurityException("Not allowed to access cell location");
            case DENIED_SOFT:
                return (getDefaultPhone().getPhoneType() == PhoneConstants.PHONE_TYPE_CDMA)
                        ? new CellIdentityCdma() : new CellIdentityGsm();
        }

        WorkSource workSource = getWorkSource(Binder.getCallingUid());
        final long identity = Binder.clearCallingIdentity();
        try {
            if (DBG_LOC) log("getCellLocation: is active user");
            int subId = mSubscriptionController.getDefaultDataSubId();
            return (CellIdentity) sendRequest(CMD_GET_CELL_LOCATION, workSource, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getNetworkCountryIsoForPhone(int phoneId) {
        // Reporting the correct network country is ambiguous when IWLAN could conflict with
        // registered cell info, so return a NULL country instead.
        final long identity = Binder.clearCallingIdentity();
        try {
            if (phoneId == SubscriptionManager.INVALID_PHONE_INDEX) {
                // Get default phone in this case.
                phoneId = SubscriptionManager.DEFAULT_PHONE_INDEX;
            }
            final int subId = mSubscriptionController.getSubIdUsingPhoneId(phoneId);
            Phone phone = PhoneFactory.getPhone(phoneId);
            if (phone == null) return "";
            ServiceStateTracker sst = phone.getServiceStateTracker();
            if (sst == null) return "";
            LocaleTracker lt = sst.getLocaleTracker();
            if (lt == null) return "";
            if (!TextUtils.isEmpty(lt.getCurrentCountry())) return lt.getCurrentCountry();
            EmergencyNumberTracker ent = phone.getEmergencyNumberTracker();
            return (ent == null) ? "" : ent.getEmergencyCountryIso();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void enableLocationUpdates() {
        enableLocationUpdatesForSubscriber(getDefaultSubscription());
    }

    @Override
    public void enableLocationUpdatesForSubscriber(int subId) {
        mApp.enforceCallingOrSelfPermission(
                android.Manifest.permission.CONTROL_LOCATION_UPDATES, null);

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                phone.enableLocationUpdates();
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void disableLocationUpdates() {
        disableLocationUpdatesForSubscriber(getDefaultSubscription());
    }

    @Override
    public void disableLocationUpdatesForSubscriber(int subId) {
        mApp.enforceCallingOrSelfPermission(
                android.Manifest.permission.CONTROL_LOCATION_UPDATES, null);

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                phone.disableLocationUpdates();
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the target SDK version number for a given package name.
     *
     * This call MUST be invoked before clearing the calling UID.
     *
     * @return target SDK if the package is found or INT_MAX.
     */
    private int getTargetSdk(String packageName) {
        try {
            final ApplicationInfo ai = mApp.getPackageManager().getApplicationInfoAsUser(
                    packageName, 0, UserHandle.getUserHandleForUid(Binder.getCallingUid()));
            if (ai != null) return ai.targetSdkVersion;
        } catch (PackageManager.NameNotFoundException unexpected) {
            loge("Failed to get package info for pkg="
                    + packageName + ", uid=" + Binder.getCallingUid());
        }
        return Integer.MAX_VALUE;
    }

    @Override
    @SuppressWarnings("unchecked")
    public List<NeighboringCellInfo> getNeighboringCellInfo(String callingPackage,
            String callingFeatureId) {
        final int targetSdk = getTargetSdk(callingPackage);
        if (targetSdk >= android.os.Build.VERSION_CODES.Q) {
            throw new SecurityException(
                    "getNeighboringCellInfo() is unavailable to callers targeting Q+ SDK levels.");
        }

        if (mAppOps.noteOp(AppOpsManager.OPSTR_NEIGHBORING_CELLS, Binder.getCallingUid(),
                callingPackage) != AppOpsManager.MODE_ALLOWED) {
            return null;
        }

        if (DBG_LOC) log("getNeighboringCellInfo: is active user");

        List<CellInfo> info = getAllCellInfo(callingPackage, callingFeatureId);
        if (info == null) return null;

        List<NeighboringCellInfo> neighbors = new ArrayList<NeighboringCellInfo>();
        for (CellInfo ci : info) {
            if (ci instanceof CellInfoGsm) {
                neighbors.add(new NeighboringCellInfo((CellInfoGsm) ci));
            } else if (ci instanceof CellInfoWcdma) {
                neighbors.add(new NeighboringCellInfo((CellInfoWcdma) ci));
            }
        }
        return (neighbors.size()) > 0 ? neighbors : null;
    }

    private List<CellInfo> getCachedCellInfo() {
        List<CellInfo> cellInfos = new ArrayList<CellInfo>();
        for (Phone phone : PhoneFactory.getPhones()) {
            List<CellInfo> info = phone.getAllCellInfo();
            if (info != null) cellInfos.addAll(info);
        }
        return cellInfos;
    }

    @Override
    public List<CellInfo> getAllCellInfo(String callingPackage, String callingFeatureId) {
        mApp.getSystemService(AppOpsManager.class)
                .checkPackage(Binder.getCallingUid(), callingPackage);

        LocationAccessPolicy.LocationPermissionResult locationResult =
                LocationAccessPolicy.checkLocationPermission(mApp,
                        new LocationAccessPolicy.LocationPermissionQuery.Builder()
                                .setCallingPackage(callingPackage)
                                .setCallingFeatureId(callingFeatureId)
                                .setCallingPid(Binder.getCallingPid())
                                .setCallingUid(Binder.getCallingUid())
                                .setMethod("getAllCellInfo")
                                .setMinSdkVersionForCoarse(Build.VERSION_CODES.BASE)
                                .setMinSdkVersionForFine(Build.VERSION_CODES.Q)
                                .build());
        switch (locationResult) {
            case DENIED_HARD:
                throw new SecurityException("Not allowed to access cell info");
            case DENIED_SOFT:
                return new ArrayList<>();
        }

        final int targetSdk = getTargetSdk(callingPackage);
        if (targetSdk >= android.os.Build.VERSION_CODES.Q) {
            return getCachedCellInfo();
        }

        if (DBG_LOC) log("getAllCellInfo: is active user");
        WorkSource workSource = getWorkSource(Binder.getCallingUid());
        final long identity = Binder.clearCallingIdentity();
        try {
            List<CellInfo> cellInfos = new ArrayList<CellInfo>();
            for (Phone phone : PhoneFactory.getPhones()) {
                final List<CellInfo> info = (List<CellInfo>) sendRequest(
                        CMD_GET_ALL_CELL_INFO, null, phone, workSource);
                if (info != null) cellInfos.addAll(info);
            }
            return cellInfos;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void requestCellInfoUpdate(int subId, ICellInfoCallback cb, String callingPackage,
            String callingFeatureId) {
        requestCellInfoUpdateInternal(subId, cb, callingPackage, callingFeatureId,
                getWorkSource(Binder.getCallingUid()));
    }

    @Override
    public void requestCellInfoUpdateWithWorkSource(int subId, ICellInfoCallback cb,
            String callingPackage, String callingFeatureId, WorkSource workSource) {
        enforceModifyPermission();
        requestCellInfoUpdateInternal(subId, cb, callingPackage, callingFeatureId, workSource);
    }

    private void requestCellInfoUpdateInternal(int subId, ICellInfoCallback cb,
            String callingPackage, String callingFeatureId, WorkSource workSource) {
        mApp.getSystemService(AppOpsManager.class)
                .checkPackage(Binder.getCallingUid(), callingPackage);

        LocationAccessPolicy.LocationPermissionResult locationResult =
                LocationAccessPolicy.checkLocationPermission(mApp,
                        new LocationAccessPolicy.LocationPermissionQuery.Builder()
                                .setCallingPackage(callingPackage)
                                .setCallingFeatureId(callingFeatureId)
                                .setCallingPid(Binder.getCallingPid())
                                .setCallingUid(Binder.getCallingUid())
                                .setMethod("requestCellInfoUpdate")
                                .setMinSdkVersionForCoarse(Build.VERSION_CODES.BASE)
                                .setMinSdkVersionForFine(Build.VERSION_CODES.BASE)
                                .build());
        switch (locationResult) {
            case DENIED_HARD:
                if (getTargetSdk(callingPackage) < Build.VERSION_CODES.Q) {
                    // Safetynet logging for b/154934934
                    EventLog.writeEvent(0x534e4554, "154934934", Binder.getCallingUid());
                }
                throw new SecurityException("Not allowed to access cell info");
            case DENIED_SOFT:
                if (getTargetSdk(callingPackage) < Build.VERSION_CODES.Q) {
                    // Safetynet logging for b/154934934
                    EventLog.writeEvent(0x534e4554, "154934934", Binder.getCallingUid());
                }
                try {
                    cb.onCellInfo(new ArrayList<CellInfo>());
                } catch (RemoteException re) {
                    // Drop without consequences
                }
                return;
        }


        final Phone phone = getPhoneFromSubId(subId);
        if (phone == null) throw new IllegalArgumentException("Invalid Subscription Id: " + subId);

        sendRequestAsync(CMD_REQUEST_CELL_INFO_UPDATE, cb, phone, workSource);
    }

    @Override
    public void setCellInfoListRate(int rateInMillis) {
        enforceModifyPermission();
        WorkSource workSource = getWorkSource(Binder.getCallingUid());

        final long identity = Binder.clearCallingIdentity();
        try {
            getDefaultPhone().setCellInfoListRate(rateInMillis, workSource);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getImeiForSlot(int slotIndex, String callingPackage, String callingFeatureId) {
        Phone phone = PhoneFactory.getPhone(slotIndex);
        if (phone == null) {
            return null;
        }
        int subId = phone.getSubId();
        if (!TelephonyPermissions.checkCallingOrSelfReadDeviceIdentifiers(mApp, subId,
                callingPackage, callingFeatureId, "getImeiForSlot")) {
            return null;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return phone.getImei();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getTypeAllocationCodeForSlot(int slotIndex) {
        Phone phone = PhoneFactory.getPhone(slotIndex);
        String tac = null;
        if (phone != null) {
            String imei = phone.getImei();
            tac = imei == null ? null : imei.substring(0, TYPE_ALLOCATION_CODE_LENGTH);
        }
        return tac;
    }

    @Override
    public String getMeidForSlot(int slotIndex, String callingPackage, String callingFeatureId) {
        Phone phone = PhoneFactory.getPhone(slotIndex);
        if (phone == null) {
            return null;
        }

        int subId = phone.getSubId();
        if (!TelephonyPermissions.checkCallingOrSelfReadDeviceIdentifiers(mApp, subId,
                callingPackage, callingFeatureId, "getMeidForSlot")) {
            return null;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return phone.getMeid();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getManufacturerCodeForSlot(int slotIndex) {
        Phone phone = PhoneFactory.getPhone(slotIndex);
        String manufacturerCode = null;
        if (phone != null) {
            String meid = phone.getMeid();
            manufacturerCode = meid == null ? null : meid.substring(0, MANUFACTURER_CODE_LENGTH);
        }
        return manufacturerCode;
    }

    @Override
    public String getDeviceSoftwareVersionForSlot(int slotIndex, String callingPackage,
            String callingFeatureId) {
        Phone phone = PhoneFactory.getPhone(slotIndex);
        if (phone == null) {
            return null;
        }
        int subId = phone.getSubId();
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "getDeviceSoftwareVersionForSlot")) {
            return null;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return phone.getDeviceSvn();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getSubscriptionCarrierId(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            return phone == null ? TelephonyManager.UNKNOWN_CARRIER_ID : phone.getCarrierId();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getSubscriptionCarrierName(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            return phone == null ? null : phone.getCarrierName();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getSubscriptionSpecificCarrierId(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            return phone == null ? TelephonyManager.UNKNOWN_CARRIER_ID
                    : phone.getSpecificCarrierId();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getSubscriptionSpecificCarrierName(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            return phone == null ? null : phone.getSpecificCarrierName();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getCarrierIdFromMccMnc(int slotIndex, String mccmnc, boolean isSubscriptionMccMnc) {
        if (!isSubscriptionMccMnc) {
            enforceReadPrivilegedPermission("getCarrierIdFromMccMnc");
        }
        final Phone phone = PhoneFactory.getPhone(slotIndex);
        if (phone == null) {
            return TelephonyManager.UNKNOWN_CARRIER_ID;
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            return CarrierResolver.getCarrierIdFromMccMnc(phone.getContext(), mccmnc);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    //
    // Internal helper methods.
    //

    /**
     * Make sure the caller has the MODIFY_PHONE_STATE permission.
     *
     * @throws SecurityException if the caller does not have the required permission
     */
    private void enforceModifyPermission() {
        mApp.enforceCallingOrSelfPermission(android.Manifest.permission.MODIFY_PHONE_STATE, null);
    }

    /**
     * Make sure the caller is system.
     *
     * @throws SecurityException if the caller is not system.
     */
    private void enforceSystemCaller() {
        if (Binder.getCallingUid() != Process.SYSTEM_UID) {
            throw new SecurityException("Caller must be system");
        }
    }

    private void enforceActiveEmergencySessionPermission() {
        mApp.enforceCallingOrSelfPermission(
                android.Manifest.permission.READ_ACTIVE_EMERGENCY_SESSION, null);
    }

    /**
     * Make sure the caller has the CALL_PHONE permission.
     *
     * @throws SecurityException if the caller does not have the required permission
     */
    private void enforceCallPermission() {
        mApp.enforceCallingOrSelfPermission(android.Manifest.permission.CALL_PHONE, null);
    }

    private void enforceSettingsPermission() {
        mApp.enforceCallingOrSelfPermission(android.Manifest.permission.NETWORK_SETTINGS, null);
    }

    private String createTelUrl(String number) {
        if (TextUtils.isEmpty(number)) {
            return null;
        }

        return "tel:" + number;
    }

    private static void log(String msg) {
        Log.d(LOG_TAG, "[PhoneIntfMgr] " + msg);
    }

    private static void logv(String msg) {
        Log.v(LOG_TAG, "[PhoneIntfMgr] " + msg);
    }

    private static void loge(String msg) {
        Log.e(LOG_TAG, "[PhoneIntfMgr] " + msg);
    }

    @Override
    public int getActivePhoneType() {
        return getActivePhoneTypeForSlot(getSlotForDefaultSubscription());
    }

    @Override
    public int getActivePhoneTypeForSlot(int slotIndex) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = PhoneFactory.getPhone(slotIndex);
            if (phone == null) {
                return PhoneConstants.PHONE_TYPE_NONE;
            } else {
                return phone.getPhoneType();
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the CDMA ERI icon index to display
     */
    @Override
    public int getCdmaEriIconIndex(String callingPackage, String callingFeatureId) {
        return getCdmaEriIconIndexForSubscriber(getDefaultSubscription(), callingPackage,
                callingFeatureId);
    }

    @Override
    public int getCdmaEriIconIndexForSubscriber(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "getCdmaEriIconIndexForSubscriber")) {
            return -1;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.getCdmaEriIconIndex();
            } else {
                return -1;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the CDMA ERI icon mode,
     * 0 - ON
     * 1 - FLASHING
     */
    @Override
    public int getCdmaEriIconMode(String callingPackage, String callingFeatureId) {
        return getCdmaEriIconModeForSubscriber(getDefaultSubscription(), callingPackage,
                callingFeatureId);
    }

    @Override
    public int getCdmaEriIconModeForSubscriber(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "getCdmaEriIconModeForSubscriber")) {
            return -1;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.getCdmaEriIconMode();
            } else {
                return -1;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the CDMA ERI text,
     */
    @Override
    public String getCdmaEriText(String callingPackage, String callingFeatureId) {
        return getCdmaEriTextForSubscriber(getDefaultSubscription(), callingPackage,
                callingFeatureId);
    }

    @Override
    public String getCdmaEriTextForSubscriber(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "getCdmaEriIconTextForSubscriber")) {
            return null;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.getCdmaEriText();
            } else {
                return null;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the CDMA MDN.
     */
    @Override
    public String getCdmaMdn(int subId) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "getCdmaMdn");

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null && phone.getPhoneType() == PhoneConstants.PHONE_TYPE_CDMA) {
                return phone.getLine1Number();
            } else {
                loge("getCdmaMdn: no phone found. Invalid subId: " + subId);
                return null;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the CDMA MIN.
     */
    @Override
    public String getCdmaMin(int subId) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "getCdmaMin");

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null && phone.getPhoneType() == PhoneConstants.PHONE_TYPE_CDMA) {
                return phone.getCdmaMin();
            } else {
                return null;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void requestNumberVerification(PhoneNumberRange range, long timeoutMillis,
            INumberVerificationCallback callback, String callingPackage) {
        if (mApp.checkCallingOrSelfPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
                != PERMISSION_GRANTED) {
            throw new SecurityException("Caller must hold the MODIFY_PHONE_STATE permission");
        }
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);

        String authorizedPackage = NumberVerificationManager.getAuthorizedPackage(mApp);
        if (!TextUtils.equals(callingPackage, authorizedPackage)) {
            throw new SecurityException("Calling package must be configured in the device config");
        }

        if (range == null) {
            throw new NullPointerException("Range must be non-null");
        }

        timeoutMillis = Math.min(timeoutMillis,
                TelephonyManager.getMaxNumberVerificationTimeoutMillis());

        NumberVerificationManager.getInstance().requestVerification(range, callback, timeoutMillis);
    }

    /**
     * Returns true if CDMA provisioning needs to run.
     */
    public boolean needsOtaServiceProvisioning() {
        final long identity = Binder.clearCallingIdentity();
        try {
            return getDefaultPhone().needsOtaServiceProvisioning();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Sets the voice mail number of a given subId.
     */
    @Override
    public boolean setVoiceMailNumber(int subId, String alphaTag, String number) {
        TelephonyPermissions.enforceCallingOrSelfCarrierPrivilege(
                mApp, subId, "setVoiceMailNumber");

        final long identity = Binder.clearCallingIdentity();
        try {
            Boolean success = (Boolean) sendRequest(CMD_SET_VOICEMAIL_NUMBER,
                    new Pair<String, String>(alphaTag, number), new Integer(subId));
            return success;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public Bundle getVisualVoicemailSettings(String callingPackage, int subId) {
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        TelecomManager tm = mApp.getSystemService(TelecomManager.class);
        String systemDialer = tm.getSystemDialerPackage();
        if (!TextUtils.equals(callingPackage, systemDialer)) {
            throw new SecurityException("caller must be system dialer");
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            PhoneAccountHandle phoneAccountHandle = PhoneAccountHandleConverter.fromSubId(subId);
            if (phoneAccountHandle == null) {
                return null;
            }
            return VisualVoicemailSettingsUtil.dump(mApp, phoneAccountHandle);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getVisualVoicemailPackageName(String callingPackage, String callingFeatureId,
            int subId) {
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "getVisualVoicemailPackageName")) {
            return null;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return RemoteVvmTaskManager.getRemotePackage(mApp, subId).getPackageName();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void enableVisualVoicemailSmsFilter(String callingPackage, int subId,
            VisualVoicemailSmsFilterSettings settings) {
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);

        final long identity = Binder.clearCallingIdentity();
        try {
            VisualVoicemailSmsFilterConfig.enableVisualVoicemailSmsFilter(
                    mApp, callingPackage, subId, settings);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void disableVisualVoicemailSmsFilter(String callingPackage, int subId) {
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);

        final long identity = Binder.clearCallingIdentity();
        try {
            VisualVoicemailSmsFilterConfig.disableVisualVoicemailSmsFilter(
                    mApp, callingPackage, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public VisualVoicemailSmsFilterSettings getVisualVoicemailSmsFilterSettings(
            String callingPackage, int subId) {
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);

        final long identity = Binder.clearCallingIdentity();
        try {
            return VisualVoicemailSmsFilterConfig.getVisualVoicemailSmsFilterSettings(
                    mApp, callingPackage, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public VisualVoicemailSmsFilterSettings getActiveVisualVoicemailSmsFilterSettings(int subId) {
        enforceReadPrivilegedPermission("getActiveVisualVoicemailSmsFilterSettings");

        final long identity = Binder.clearCallingIdentity();
        try {
            return VisualVoicemailSmsFilterConfig.getActiveVisualVoicemailSmsFilterSettings(
                    mApp, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void sendVisualVoicemailSmsForSubscriber(String callingPackage,
            String callingAttributionTag, int subId, String number, int port, String text,
            PendingIntent sentIntent) {
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        enforceVisualVoicemailPackage(callingPackage, subId);
        enforceSendSmsPermission();
        SmsController smsController = PhoneFactory.getSmsController();
        smsController.sendVisualVoicemailSmsForSubscriber(callingPackage, callingAttributionTag,
                subId, number, port, text, sentIntent);
    }

    /**
     * Sets the voice activation state of a given subId.
     */
    @Override
    public void setVoiceActivationState(int subId, int activationState) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setVoiceActivationState");

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                phone.setVoiceActivationState(activationState);
            } else {
                loge("setVoiceActivationState fails with invalid subId: " + subId);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Sets the data activation state of a given subId.
     */
    @Override
    public void setDataActivationState(int subId, int activationState) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setDataActivationState");

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                phone.setDataActivationState(activationState);
            } else {
                loge("setDataActivationState fails with invalid subId: " + subId);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the voice activation state of a given subId.
     */
    @Override
    public int getVoiceActivationState(int subId, String callingPackage) {
        enforceReadPrivilegedPermission("getVoiceActivationState");

        final Phone phone = getPhone(subId);
        final long identity = Binder.clearCallingIdentity();
        try {
            if (phone != null) {
                return phone.getVoiceActivationState();
            } else {
                return TelephonyManager.SIM_ACTIVATION_STATE_UNKNOWN;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the data activation state of a given subId.
     */
    @Override
    public int getDataActivationState(int subId, String callingPackage) {
        enforceReadPrivilegedPermission("getDataActivationState");

        final Phone phone = getPhone(subId);
        final long identity = Binder.clearCallingIdentity();
        try {
            if (phone != null) {
                return phone.getDataActivationState();
            } else {
                return TelephonyManager.SIM_ACTIVATION_STATE_UNKNOWN;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the unread count of voicemails for a subId
     */
    @Override
    public int getVoiceMessageCountForSubscriber(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "getVoiceMessageCountForSubscriber")) {
            return 0;
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.getVoiceMessageCount();
            } else {
                return 0;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
      * returns true, if the device is in a state where both voice and data
      * are supported simultaneously. This can change based on location or network condition.
     */
    @Override
    public boolean isConcurrentVoiceAndDataAllowed(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            return (phone == null ? false : phone.isConcurrentVoiceAndDataAllowed());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Send the dialer code if called from the current default dialer or the caller has
     * carrier privilege.
     * @param inputCode The dialer code to send
     */
    @Override
    public void sendDialerSpecialCode(String callingPackage, String inputCode) {
        final Phone defaultPhone = getDefaultPhone();
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        TelecomManager tm = defaultPhone.getContext().getSystemService(TelecomManager.class);
        String defaultDialer = tm.getDefaultDialerPackage();
        if (!TextUtils.equals(callingPackage, defaultDialer)) {
            TelephonyPermissions.enforceCallingOrSelfCarrierPrivilege(mApp,
                    getDefaultSubscription(), "sendDialerSpecialCode");
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            defaultPhone.sendDialerSpecialCode(inputCode);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getNetworkSelectionMode(int subId) {
        TelephonyPermissions
                    .enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                    mApp, subId, "getNetworkSelectionMode");
        final long identity = Binder.clearCallingIdentity();
        try {
            if (!isActiveSubscription(subId)) {
                return TelephonyManager.NETWORK_SELECTION_MODE_UNKNOWN;
            }
            return (int) sendRequest(CMD_GET_NETWORK_SELECTION_MODE, null /* argument */, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isInEmergencySmsMode() {
        enforceReadPrivilegedPermission("isInEmergencySmsMode");
        final long identity = Binder.clearCallingIdentity();
        try {
            for (Phone phone : PhoneFactory.getPhones()) {
                if (phone.isInEmergencySmsMode()) {
                    return true;
                }
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return false;
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     * @param c The callback that will be used to send the result.
     */
    @Override
    public void registerImsRegistrationCallback(int subId, IImsRegistrationCallback c)
            throws RemoteException {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "registerImsRegistrationCallback");

        if (!ImsManager.isImsSupportedOnDevice(mApp)) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "IMS not available on device.");
        }
        final long token = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp, getSlotIndexOrException(subId))
                    .addRegistrationCallbackForSubscription(c, subId);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     * @param c The callback that will be used to send the result.
     */
    @Override
    public void unregisterImsRegistrationCallback(int subId, IImsRegistrationCallback c) {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "unregisterImsRegistrationCallback");
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            throw new IllegalArgumentException("Invalid Subscription ID: " + subId);
        }
        final long token = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone.
            ImsManager.getInstance(mApp, getSlotIndexOrException(subId))
                    .removeRegistrationCallbackForSubscription(c, subId);
        } catch (ImsException e) {
            Log.i(LOG_TAG, "unregisterImsRegistrationCallback: " + subId
                    + "is inactive, ignoring unregister.");
            // If the subscription is no longer active, just return, since the callback
            // will already have been removed internally.
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Get the IMS service registration state for the MmTelFeature associated with this sub id.
     */
    @Override
    public void getImsMmTelRegistrationState(int subId, IIntegerConsumer consumer) {
        enforceReadPrivilegedPermission("getImsMmTelRegistrationState");
        if (!ImsManager.isImsSupportedOnDevice(mApp)) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "IMS not available on device.");
        }
        final long token = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone == null) {
                Log.w(LOG_TAG, "getImsMmTelRegistrationState: called with an invalid subscription '"
                        + subId + "'");
                throw new ServiceSpecificException(ImsException.CODE_ERROR_INVALID_SUBSCRIPTION);
            }
            phone.getImsRegistrationState(regState -> {
                try {
                    consumer.accept((regState == null)
                            ? RegistrationManager.REGISTRATION_STATE_NOT_REGISTERED : regState);
                } catch (RemoteException e) {
                    // Ignore if the remote process is no longer available to call back.
                    Log.w(LOG_TAG, "getImsMmTelRegistrationState: callback not available.");
                }
            });
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Get the transport type for the IMS service registration state.
     */
    @Override
    public void getImsMmTelRegistrationTransportType(int subId, IIntegerConsumer consumer) {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "getImsMmTelRegistrationTransportType");
        if (!ImsManager.isImsSupportedOnDevice(mApp)) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "IMS not available on device.");
        }
        final long token = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone == null) {
                Log.w(LOG_TAG, "getImsMmTelRegistrationState: called with an invalid subscription '"
                        + subId + "'");
                throw new ServiceSpecificException(ImsException.CODE_ERROR_INVALID_SUBSCRIPTION);
            }
            phone.getImsRegistrationTech(regTech -> {
                // Convert registration tech from ImsRegistrationImplBase -> RegistrationManager
                int regTechConverted = (regTech == null)
                        ? ImsRegistrationImplBase.REGISTRATION_TECH_NONE : regTech;
                regTechConverted = RegistrationManager.IMS_REG_TO_ACCESS_TYPE_MAP.get(
                        regTechConverted);
                try {
                    consumer.accept(regTechConverted);
                } catch (RemoteException e) {
                    // Ignore if the remote process is no longer available to call back.
                    Log.w(LOG_TAG, "getImsMmTelRegistrationState: callback not available.");
                }
            });
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     * @param c The callback that will be used to send the result.
     */
    @Override
    public void registerMmTelCapabilityCallback(int subId, IImsCapabilityCallback c)
            throws RemoteException {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "registerMmTelCapabilityCallback");
        if (!ImsManager.isImsSupportedOnDevice(mApp)) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "IMS not available on device.");
        }
        // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
        final long token = Binder.clearCallingIdentity();
        try {
            ImsManager.getInstance(mApp, getSlotIndexOrException(subId))
                    .addCapabilitiesCallbackForSubscription(c, subId);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     * @param c The callback that will be used to send the result.
     */
    @Override
    public void unregisterMmTelCapabilityCallback(int subId, IImsCapabilityCallback c) {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "unregisterMmTelCapabilityCallback");
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            throw new IllegalArgumentException("Invalid Subscription ID: " + subId);
        }

        final long token = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone.
            ImsManager.getInstance(mApp, getSlotIndexOrException(subId))
                        .removeCapabilitiesCallbackForSubscription(c, subId);
        } catch (ImsException e) {
            Log.i(LOG_TAG, "unregisterMmTelCapabilityCallback: " + subId
                     + "is inactive, ignoring unregister.");
             // If the subscription is no longer active, just return, since the callback
             // will already have been removed internally.
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    @Override
    public boolean isCapable(int subId, int capability, int regTech) {
        enforceReadPrivilegedPermission("isCapable");
        // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
        final long token = Binder.clearCallingIdentity();
        try {
            return ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).queryMmTelCapability(capability, regTech);
        } catch (com.android.ims.ImsException e) {
            Log.w(LOG_TAG, "IMS isCapable - service unavailable: " + e.getMessage());
            return false;
        } catch (ImsException e) {
            Log.i(LOG_TAG, "isCapable: " + subId + " is inactive, returning false.");
            return false;
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    @Override
    public boolean isAvailable(int subId, int capability, int regTech) {
        enforceReadPrivilegedPermission("isAvailable");
        final long token = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone == null) return false;
            return phone.isImsCapabilityAvailable(capability, regTech);
        } catch (com.android.ims.ImsException e) {
            Log.w(LOG_TAG, "IMS isAvailable - service unavailable: " + e.getMessage());
            return false;
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Determines if the MmTel feature capability is supported by the carrier configuration for this
     * subscription.
     * @param subId The subscription to use to check the configuration.
     * @param callback The callback that will be used to send the result.
     * @param capability The MmTelFeature capability that will be used to send the result.
     * @param transportType The transport type of the MmTelFeature capability.
     */
    @Override
    public void isMmTelCapabilitySupported(int subId, IIntegerConsumer callback, int capability,
            int transportType) {
        enforceReadPrivilegedPermission("isMmTelCapabilitySupported");
        if (!ImsManager.isImsSupportedOnDevice(mApp)) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "IMS not available on device.");
        }
        final long token = Binder.clearCallingIdentity();
        try {
            int slotId = getSlotIndex(subId);
            if (slotId <= SubscriptionManager.INVALID_SIM_SLOT_INDEX) {
                Log.w(LOG_TAG, "isMmTelCapabilitySupported: called with an inactive subscription '"
                        + subId + "'");
                throw new ServiceSpecificException(ImsException.CODE_ERROR_INVALID_SUBSCRIPTION);
            }
            ImsManager.getInstance(mApp, slotId).isSupported(capability,
                    transportType, aBoolean -> {
                        try {
                            callback.accept((aBoolean == null) ? 0 : (aBoolean ? 1 : 0));
                        } catch (RemoteException e) {
                            Log.w(LOG_TAG, "isMmTelCapabilitySupported: remote caller is not "
                                    + "running. Ignore");
                        }
                    });
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     */
    @Override
    public boolean isAdvancedCallingSettingEnabled(int subId) {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "isAdvancedCallingSettingEnabled");

        // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
        final long token = Binder.clearCallingIdentity();
        try {
            return ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).isEnhanced4gLteModeSettingEnabledByUser();
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    @Override
    public void setAdvancedCallingSettingEnabled(int subId, boolean isEnabled) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setAdvancedCallingSettingEnabled");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).setEnhanced4gLteModeSetting(isEnabled);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     */
    @Override
    public boolean isVtSettingEnabled(int subId) {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "isVtSettingEnabled");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            return ImsManager.getInstance(mApp, getSlotIndexOrException(subId)).isVtEnabledByUser();
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setVtSettingEnabled(int subId, boolean isEnabled) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setVtSettingEnabled");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp, getSlotIndexOrException(subId)).setVtSetting(isEnabled);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     */
    @Override
    public boolean isVoWiFiSettingEnabled(int subId) {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "isVoWiFiSettingEnabled");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            return ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).isWfcEnabledByUser();
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setVoWiFiSettingEnabled(int subId, boolean isEnabled) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setVoWiFiSettingEnabled");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp, getSlotIndexOrException(subId)).setWfcSetting(isEnabled);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     */
    @Override
    public boolean isVoWiFiRoamingSettingEnabled(int subId) {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "isVoWiFiRoamingSettingEnabled");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            return ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).isWfcRoamingEnabledByUser();
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setVoWiFiRoamingSettingEnabled(int subId, boolean isEnabled) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setVoWiFiRoamingSettingEnabled");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).setWfcRoamingSetting(isEnabled);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setVoWiFiNonPersistent(int subId, boolean isCapable, int mode) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setVoWiFiNonPersistent");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).setWfcNonPersistent(isCapable, mode);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     */
    @Override
    public int getVoWiFiModeSetting(int subId) {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "getVoWiFiModeSetting");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            return ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).getWfcMode(false /*isRoaming*/);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setVoWiFiModeSetting(int subId, int mode) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setVoWiFiModeSetting");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).setWfcMode(mode, false /*isRoaming*/);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getVoWiFiRoamingModeSetting(int subId) {
        enforceReadPrivilegedPermission("getVoWiFiRoamingModeSetting");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            return ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).getWfcMode(true /*isRoaming*/);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setVoWiFiRoamingModeSetting(int subId, int mode) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setVoWiFiRoamingModeSetting");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).setWfcMode(mode, true /*isRoaming*/);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setRttCapabilitySetting(int subId, boolean isEnabled) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setRttCapabilityEnabled");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp, getSlotIndexOrException(subId)).setRttEnabled(isEnabled);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Requires carrier privileges or READ_PRECISE_PHONE_STATE permission.
     * @param subId The subscription to use to check the configuration.
     */
    @Override
    public boolean isTtyOverVolteEnabled(int subId) {
        TelephonyPermissions.enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "isTtyOverVolteEnabled");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            return ImsManager.getInstance(mApp,
                    getSlotIndexOrException(subId)).isTtyOnVoLteCapable();
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void registerImsProvisioningChangedCallback(int subId, IImsConfigCallback callback) {
        enforceReadPrivilegedPermission("registerImsProvisioningChangedCallback");
        final long identity = Binder.clearCallingIdentity();
        try {
            if (!isImsAvailableOnDevice()) {
                throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                        "IMS not available on device.");
            }
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp, getSlotIndexOrException(subId))
                    .addProvisioningCallbackForSubscription(callback, subId);
        } catch (ImsException e) {
            throw new ServiceSpecificException(e.getCode());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void unregisterImsProvisioningChangedCallback(int subId, IImsConfigCallback callback) {
        enforceReadPrivilegedPermission("unregisterImsProvisioningChangedCallback");
        final long identity = Binder.clearCallingIdentity();
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            throw new IllegalArgumentException("Invalid Subscription ID: " + subId);
        }
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            ImsManager.getInstance(mApp, getSlotIndexOrException(subId))
                    .removeProvisioningCallbackForSubscription(callback, subId);
        } catch (ImsException e) {
            Log.i(LOG_TAG, "unregisterImsProvisioningChangedCallback: " + subId
                    + "is inactive, ignoring unregister.");
            // If the subscription is no longer active, just return, since the callback will already
            // have been removed internally.
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }


    private void checkModifyPhoneStatePermission(int subId, String message) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                message);
    }

    private boolean isImsProvisioningRequired(int subId, int capability,
            boolean isMmtelCapability) {
        Phone phone = getPhone(subId);
        if (phone == null) {
            loge("phone instance null for subid " + subId);
            return false;
        }
        if (isMmtelCapability) {
            if (!doesImsCapabilityRequireProvisioning(phone.getContext(), subId, capability)) {
                return false;
            }
        } else {
            if (!doesRcsCapabilityRequireProvisioning(phone.getContext(), subId, capability)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void setRcsProvisioningStatusForCapability(int subId, int capability,
            boolean isProvisioned) {
        checkModifyPhoneStatePermission(subId, "setRcsProvisioningStatusForCapability");

        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            if (!isImsProvisioningRequired(subId, capability, false)) {
                return;
            }

            // this capability requires provisioning, route to the correct API.
            ImsManager ims = ImsManager.getInstance(mApp, getSlotIndex(subId));
            switch (capability) {
                case RcsFeature.RcsImsCapabilities.CAPABILITY_TYPE_OPTIONS_UCE:
                case RcsFeature.RcsImsCapabilities.CAPABILITY_TYPE_PRESENCE_UCE:
                    ims.setEabProvisioned(isProvisioned);
                    break;
                default: {
                    throw new IllegalArgumentException("Tried to set provisioning for "
                            + "rcs capability '" + capability + "', which does not require "
                            + "provisioning.");
                }
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }

    }


    @Override
    public boolean getRcsProvisioningStatusForCapability(int subId, int capability) {
        enforceReadPrivilegedPermission("getRcsProvisioningStatusForCapability");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            if (!isImsProvisioningRequired(subId, capability, false)) {
                return true;
            }

            ImsManager ims = ImsManager.getInstance(mApp, getSlotIndex(subId));
            switch (capability) {
                case RcsFeature.RcsImsCapabilities.CAPABILITY_TYPE_OPTIONS_UCE:
                case RcsFeature.RcsImsCapabilities.CAPABILITY_TYPE_PRESENCE_UCE:
                    return ims.isEabProvisionedOnDevice();

                default: {
                    throw new IllegalArgumentException("Tried to get rcs provisioning for "
                            + "capability '" + capability + "', which does not require "
                            + "provisioning.");
                }
            }

        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setImsProvisioningStatusForCapability(int subId, int capability, int tech,
            boolean isProvisioned) {
        if (tech != ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN
                && tech != ImsRegistrationImplBase.REGISTRATION_TECH_LTE) {
            throw new IllegalArgumentException("Registration technology '" + tech + "' is invalid");
        }
        checkModifyPhoneStatePermission(subId, "setImsProvisioningStatusForCapability");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            if (!isImsProvisioningRequired(subId, capability, true)) {
                return;
            }

            // this capability requires provisioning, route to the correct API.
            ImsManager ims = ImsManager.getInstance(mApp, getSlotIndex(subId));
            switch (capability) {
                case MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE: {
                    if (tech == ImsRegistrationImplBase.REGISTRATION_TECH_LTE) {
                        ims.setVolteProvisioned(isProvisioned);
                    } else if (tech == ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN) {
                        ims.setWfcProvisioned(isProvisioned);
                    }
                    break;
                }
                case MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VIDEO: {
                    // There is currently no difference in VT provisioning type.
                    ims.setVtProvisioned(isProvisioned);
                    break;
                }
                case MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_UT: {
                    // There is no "deprecated" UT provisioning mechanism through ImsConfig, so
                    // change the capability of the feature instead if needed.
                    if (isMmTelCapabilityProvisionedInCache(subId, capability, tech)
                            == isProvisioned) {
                        // No change in provisioning.
                        return;
                    }
                    cacheMmTelCapabilityProvisioning(subId, capability, tech, isProvisioned);
                    try {
                        ims.changeMmTelCapability(capability, tech, isProvisioned);
                    } catch (com.android.ims.ImsException e) {
                        loge("setImsProvisioningStatusForCapability: couldn't change UT capability"
                                + ", Exception" + e.getMessage());
                    }
                    break;
                }
                default: {
                    throw new IllegalArgumentException("Tried to set provisioning for "
                            + "MmTel capability '" + capability + "', which does not require "
                            + "provisioning. ");
                }
            }

        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean getImsProvisioningStatusForCapability(int subId, int capability, int tech) {
        if (tech != ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN
                && tech != ImsRegistrationImplBase.REGISTRATION_TECH_LTE) {
            throw new IllegalArgumentException("Registration technology '" + tech + "' is invalid");
        }
        enforceReadPrivilegedPermission("getProvisioningStatusForCapability");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            if (!isImsProvisioningRequired(subId, capability, true)) {
                return true;
            }

            ImsManager ims = ImsManager.getInstance(mApp, getSlotIndex(subId));
            switch (capability) {
                case MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE: {
                    if (tech == ImsRegistrationImplBase.REGISTRATION_TECH_LTE) {
                        return ims.isVolteProvisionedOnDevice();
                    } else if (tech == ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN) {
                        return ims.isWfcProvisionedOnDevice();
                    }
                    // This should never happen, since we are checking tech above to make sure it
                    // is either LTE or IWLAN.
                    throw new IllegalArgumentException("Invalid radio technology for voice "
                            + "capability.");
                }
                case MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VIDEO: {
                    // There is currently no difference in VT provisioning type.
                    return ims.isVtProvisionedOnDevice();
                }
                case MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_UT: {
                    // There is no "deprecated" UT provisioning mechanism, so get from shared prefs.
                    return isMmTelCapabilityProvisionedInCache(subId, capability, tech);
                }
                default: {
                    throw new IllegalArgumentException(
                            "Tried to get provisioning for MmTel capability '" + capability
                                    + "', which does not require provisioning.");
                }
            }

        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isMmTelCapabilityProvisionedInCache(int subId, int capability, int tech) {
        if (tech != ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN
                && tech != ImsRegistrationImplBase.REGISTRATION_TECH_LTE) {
            throw new IllegalArgumentException("Registration technology '" + tech + "' is invalid");
        }
        enforceReadPrivilegedPermission("isMmTelCapabilityProvisionedInCache");
        int provisionedBits = getMmTelCapabilityProvisioningBitfield(subId, tech);
        return (provisionedBits & capability) > 0;
    }

    @Override
    public void cacheMmTelCapabilityProvisioning(int subId, int capability, int tech,
            boolean isProvisioned) {
        if (tech != ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN
                && tech != ImsRegistrationImplBase.REGISTRATION_TECH_LTE) {
            throw new IllegalArgumentException("Registration technology '" + tech + "' is invalid");
        }
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setProvisioningStatusForCapability");
        int provisionedBits = getMmTelCapabilityProvisioningBitfield(subId, tech);
        // If the current provisioning status for capability already matches isProvisioned,
        // do nothing.
        if (((provisionedBits & capability) > 0) == isProvisioned) {
            return;
        }
        if (isProvisioned) {
            setMmTelCapabilityProvisioningBitfield(subId, tech, (provisionedBits | capability));
        } else {
            setMmTelCapabilityProvisioningBitfield(subId, tech, (provisionedBits & ~capability));
        }
    }

    /**
     * @return the bitfield containing the MmTel provisioning for the provided subscription and
     * technology. The bitfield should mirror the bitfield defined by
     * {@link MmTelFeature.MmTelCapabilities.MmTelCapability}.
     */
    private int getMmTelCapabilityProvisioningBitfield(int subId, int tech) {
        String key = getMmTelProvisioningKey(subId, tech);
        // Default is no capabilities are provisioned.
        return mTelephonySharedPreferences.getInt(key, 0 /*default*/);
    }

    /**
     * Sets the MmTel capability provisioning bitfield (defined by
     *     {@link MmTelFeature.MmTelCapabilities.MmTelCapability}) for the subscription and
     *     technology specified.
     *
     * Note: This is a synchronous command and should not be called on UI thread.
     */
    private void setMmTelCapabilityProvisioningBitfield(int subId, int tech, int newField) {
        final SharedPreferences.Editor editor = mTelephonySharedPreferences.edit();
        String key = getMmTelProvisioningKey(subId, tech);
        editor.putInt(key, newField);
        editor.commit();
    }

    private static String getMmTelProvisioningKey(int subId, int tech) {
        // resulting key is provision_ims_mmtel_{subId}_{tech}
        return PREF_PROVISION_IMS_MMTEL_PREFIX + subId + "_" + tech;
    }

    /**
     * Query CarrierConfig to see if the specified capability requires provisioning for the
     * carrier associated with the subscription id.
     */
    private boolean doesImsCapabilityRequireProvisioning(Context context, int subId,
            int capability) {
        CarrierConfigManager configManager = new CarrierConfigManager(context);
        PersistableBundle c = configManager.getConfigForSubId(subId);
        boolean requireUtProvisioning = c.getBoolean(
                CarrierConfigManager.KEY_CARRIER_SUPPORTS_SS_OVER_UT_BOOL, false)
                && c.getBoolean(CarrierConfigManager.KEY_CARRIER_UT_PROVISIONING_REQUIRED_BOOL,
                false);
        boolean requireVoiceVtProvisioning = c.getBoolean(
                CarrierConfigManager.KEY_CARRIER_VOLTE_PROVISIONING_REQUIRED_BOOL, false);

        // First check to make sure that the capability requires provisioning.
        switch (capability) {
            case MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE:
                // intentional fallthrough
            case MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VIDEO: {
                if (requireVoiceVtProvisioning) {
                    // Voice and Video requires provisioning
                    return true;
                }
                break;
            }
            case MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_UT: {
                if (requireUtProvisioning) {
                    // UT requires provisioning
                    return true;
                }
                break;
            }
        }
        return false;
    }

    private boolean doesRcsCapabilityRequireProvisioning(Context context, int subId,
            int capability) {
        CarrierConfigManager configManager = new CarrierConfigManager(context);
        PersistableBundle c = configManager.getConfigForSubId(subId);

        boolean requireRcsProvisioning = c.getBoolean(
                CarrierConfigManager.KEY_CARRIER_RCS_PROVISIONING_REQUIRED_BOOL, false);

        // First check to make sure that the capability requires provisioning.
        switch (capability) {
            case RcsFeature.RcsImsCapabilities.CAPABILITY_TYPE_PRESENCE_UCE:
                // intentional fallthrough
            case RcsFeature.RcsImsCapabilities.CAPABILITY_TYPE_OPTIONS_UCE: {
                if (requireRcsProvisioning) {
                    // OPTION or PRESENCE requires provisioning
                    return true;
                }
                break;
            }
        }
        return false;
    }

    @Override
    public int getImsProvisioningInt(int subId, int key) {
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            throw new IllegalArgumentException("Invalid Subscription id '" + subId + "'");
        }
        enforceReadPrivilegedPermission("getImsProvisioningInt");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            int slotId = getSlotIndex(subId);
            if (slotId <= SubscriptionManager.INVALID_SIM_SLOT_INDEX) {
                Log.w(LOG_TAG, "getImsProvisioningInt: called with an inactive subscription '"
                        + subId + "' for key:" + key);
                return ImsConfigImplBase.CONFIG_RESULT_UNKNOWN;
            }
            return ImsManager.getInstance(mApp, slotId).getConfigInterface().getConfigInt(key);
        } catch (com.android.ims.ImsException e) {
            Log.w(LOG_TAG, "getImsProvisioningInt: ImsService is not available for subscription '"
                    + subId + "' for key:" + key);
            return ImsConfigImplBase.CONFIG_RESULT_UNKNOWN;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getImsProvisioningString(int subId, int key) {
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            throw new IllegalArgumentException("Invalid Subscription id '" + subId + "'");
        }
        enforceReadPrivilegedPermission("getImsProvisioningString");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            int slotId = getSlotIndex(subId);
            if (slotId <= SubscriptionManager.INVALID_SIM_SLOT_INDEX) {
                Log.w(LOG_TAG, "getImsProvisioningString: called for an inactive subscription id '"
                        + subId + "' for key:" + key);
                return ProvisioningManager.STRING_QUERY_RESULT_ERROR_GENERIC;
            }
            return ImsManager.getInstance(mApp, slotId).getConfigInterface().getConfigString(key);
        } catch (com.android.ims.ImsException e) {
            Log.w(LOG_TAG, "getImsProvisioningString: ImsService is not available for sub '"
                    + subId + "' for key:" + key);
            return ProvisioningManager.STRING_QUERY_RESULT_ERROR_NOT_READY;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int setImsProvisioningInt(int subId, int key, int value) {
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            throw new IllegalArgumentException("Invalid Subscription id '" + subId + "'");
        }
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setImsProvisioningInt");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            int slotId = getSlotIndex(subId);
            if (slotId <= SubscriptionManager.INVALID_SIM_SLOT_INDEX) {
                Log.w(LOG_TAG, "setImsProvisioningInt: called with an inactive subscription id '"
                        + subId + "' for key:" + key);
                return ImsConfigImplBase.CONFIG_RESULT_FAILED;
            }
            return ImsManager.getInstance(mApp, slotId).getConfigInterface().setConfig(key, value);
        } catch (com.android.ims.ImsException e) {
            Log.w(LOG_TAG, "setImsProvisioningInt: ImsService unavailable for sub '" + subId
                    + "' for key:" + key);
            return ImsConfigImplBase.CONFIG_RESULT_FAILED;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int setImsProvisioningString(int subId, int key, String value) {
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            throw new IllegalArgumentException("Invalid Subscription id '" + subId + "'");
        }
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp, subId,
                "setImsProvisioningString");
        final long identity = Binder.clearCallingIdentity();
        try {
            // TODO: Refactor to remove ImsManager dependence and query through ImsPhone directly.
            int slotId = getSlotIndex(subId);
            if (slotId <= SubscriptionManager.INVALID_SIM_SLOT_INDEX) {
                Log.w(LOG_TAG, "setImsProvisioningString: called with an inactive subscription id '"
                        + subId + "' for key:" + key);
                return ImsConfigImplBase.CONFIG_RESULT_FAILED;
            }
            return ImsManager.getInstance(mApp, slotId).getConfigInterface().setConfig(key, value);
        } catch (com.android.ims.ImsException e) {
            Log.w(LOG_TAG, "setImsProvisioningString: ImsService unavailable for sub '" + subId
                    + "' for key:" + key);
            return ImsConfigImplBase.CONFIG_RESULT_FAILED;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private int getSlotIndexOrException(int subId) throws ImsException {
        int slotId = SubscriptionManager.getSlotIndex(subId);
        if (!SubscriptionManager.isValidSlotIndex(slotId)) {
            throw new ImsException("Invalid Subscription Id, subId=" + subId,
                    ImsException.CODE_ERROR_INVALID_SUBSCRIPTION);
        }
        return slotId;
    }

    private int getSlotIndex(int subId) {
        int slotId = SubscriptionManager.getSlotIndex(subId);
        if (!SubscriptionManager.isValidSlotIndex(slotId)) {
            return SubscriptionManager.INVALID_SIM_SLOT_INDEX;
        }
        return slotId;
    }

    /**
     * Returns the data network type for a subId; does not throw SecurityException.
     */
    @Override
    public int getNetworkTypeForSubscriber(int subId, String callingPackage,
            String callingFeatureId) {
        final int targetSdk = getTargetSdk(callingPackage);
        if (targetSdk > android.os.Build.VERSION_CODES.Q) {
            return getDataNetworkTypeForSubscriber(subId, callingPackage, callingFeatureId);
        } else if (targetSdk == android.os.Build.VERSION_CODES.Q
                && !TelephonyPermissions.checkCallingOrSelfReadPhoneStateNoThrow(
                        mApp, subId, callingPackage, callingFeatureId,
                "getNetworkTypeForSubscriber")) {
            return TelephonyManager.NETWORK_TYPE_UNKNOWN;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.getServiceState().getDataNetworkType();
            } else {
                return TelephonyManager.NETWORK_TYPE_UNKNOWN;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the data network type
     */
    @Override
    public int getDataNetworkType(String callingPackage, String callingFeatureId) {
        return getDataNetworkTypeForSubscriber(getDefaultSubscription(), callingPackage,
                callingFeatureId);
    }

    /**
     * Returns the data network type for a subId
     */
    @Override
    public int getDataNetworkTypeForSubscriber(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "getDataNetworkTypeForSubscriber")) {
            return TelephonyManager.NETWORK_TYPE_UNKNOWN;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.getServiceState().getDataNetworkType();
            } else {
                return TelephonyManager.NETWORK_TYPE_UNKNOWN;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the Voice network type for a subId
     */
    @Override
    public int getVoiceNetworkTypeForSubscriber(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "getDataNetworkTypeForSubscriber")) {
            return TelephonyManager.NETWORK_TYPE_UNKNOWN;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.getServiceState().getVoiceNetworkType();
            } else {
                return TelephonyManager.NETWORK_TYPE_UNKNOWN;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * @return true if a ICC card is present
     */
    public boolean hasIccCard() {
        // FIXME Make changes to pass defaultSimId of type int
        return hasIccCardUsingSlotIndex(mSubscriptionController.getSlotIndex(
                getDefaultSubscription()));
    }

    /**
     * @return true if a ICC card is present for a slotIndex
     */
    @Override
    public boolean hasIccCardUsingSlotIndex(int slotIndex) {
        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = PhoneFactory.getPhone(slotIndex);
            if (phone != null) {
                return phone.getIccCard().hasIccCard();
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Return if the current radio is LTE on CDMA. This
     * is a tri-state return value as for a period of time
     * the mode may be unknown.
     *
     * @param callingPackage the name of the package making the call.
     * @return {@link Phone#LTE_ON_CDMA_UNKNOWN}, {@link Phone#LTE_ON_CDMA_FALSE}
     * or {@link Phone#LTE_ON_CDMA_TRUE}
     */
    @Override
    public int getLteOnCdmaMode(String callingPackage, String callingFeatureId) {
        return getLteOnCdmaModeForSubscriber(getDefaultSubscription(), callingPackage,
                callingFeatureId);
    }

    @Override
    public int getLteOnCdmaModeForSubscriber(int subId, String callingPackage,
            String callingFeatureId) {
        try {
            enforceReadPrivilegedPermission("getLteOnCdmaModeForSubscriber");
        } catch (SecurityException e) {
            return PhoneConstants.LTE_ON_CDMA_UNKNOWN;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone == null) {
                return PhoneConstants.LTE_ON_CDMA_UNKNOWN;
            } else {
                return phone.getLteOnCdmaMode();
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * {@hide}
     * Returns Default subId, 0 in the case of single standby.
     */
    private int getDefaultSubscription() {
        return mSubscriptionController.getDefaultSubId();
    }

    private int getSlotForDefaultSubscription() {
        return mSubscriptionController.getPhoneId(getDefaultSubscription());
    }

    private int getPreferredVoiceSubscription() {
        return mSubscriptionController.getDefaultVoiceSubId();
    }

    private boolean isActiveSubscription(int subId) {
        return mSubscriptionController.isActiveSubId(subId);
    }

    /**
     * @see android.telephony.TelephonyManager.WifiCallingChoices
     */
    public int getWhenToMakeWifiCalls() {
        final long identity = Binder.clearCallingIdentity();
        try {
            return Settings.System.getInt(mApp.getContentResolver(),
                    Settings.System.WHEN_TO_MAKE_WIFI_CALLS,
                    getWhenToMakeWifiCallsDefaultPreference());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * @see android.telephony.TelephonyManager.WifiCallingChoices
     */
    public void setWhenToMakeWifiCalls(int preference) {
        final long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) log("setWhenToMakeWifiCallsStr, storing setting = " + preference);
            Settings.System.putInt(mApp.getContentResolver(),
                    Settings.System.WHEN_TO_MAKE_WIFI_CALLS, preference);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private static int getWhenToMakeWifiCallsDefaultPreference() {
        // TODO: Use a build property to choose this value.
        return TelephonyManager.WifiCallingChoices.ALWAYS_USE;
    }

    private Phone getPhoneFromSlotIdOrThrowException(int slotIndex) {
        int phoneId = UiccController.getInstance().getPhoneIdFromSlotId(slotIndex);
        if (phoneId == -1) {
            throw new IllegalArgumentException("Given slot index: " + slotIndex
                    + " does not correspond to an active phone");
        }
        return PhoneFactory.getPhone(phoneId);
    }

    @Override
    public IccOpenLogicalChannelResponse iccOpenLogicalChannel(
            int subId, String callingPackage, String aid, int p2) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "iccOpenLogicalChannel");
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        if (DBG) {
            log("iccOpenLogicalChannel: subId=" + subId + " aid=" + aid + " p2=" + p2);
        }
        return iccOpenLogicalChannelWithPermission(getPhoneFromSubId(subId), callingPackage, aid,
                p2);
    }


    @Override
    public IccOpenLogicalChannelResponse iccOpenLogicalChannelBySlot(
            int slotIndex, String callingPackage, String aid, int p2) {
        enforceModifyPermission();
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        if (DBG) {
            log("iccOpenLogicalChannelBySlot: slot=" + slotIndex + " aid=" + aid + " p2=" + p2);
        }
        return iccOpenLogicalChannelWithPermission(getPhoneFromSlotIdOrThrowException(slotIndex),
                callingPackage, aid, p2);
    }

    private IccOpenLogicalChannelResponse iccOpenLogicalChannelWithPermission(Phone phone,
            String callingPackage, String aid, int p2) {
        final long identity = Binder.clearCallingIdentity();
        try {
            if (TextUtils.equals(ISDR_AID, aid)) {
                // Only allows LPA to open logical channel to ISD-R.
                ComponentInfo bestComponent = EuiccConnector.findBestComponent(getDefaultPhone()
                        .getContext().getPackageManager());
                if (bestComponent == null
                        || !TextUtils.equals(callingPackage, bestComponent.packageName)) {
                    loge("The calling package is not allowed to access ISD-R.");
                    throw new SecurityException(
                            "The calling package is not allowed to access ISD-R.");
                }
            }

            IccOpenLogicalChannelResponse response = (IccOpenLogicalChannelResponse) sendRequest(
                    CMD_OPEN_CHANNEL, new Pair<String, Integer>(aid, p2), phone,
                    null /* workSource */);
            if (DBG) log("iccOpenLogicalChannelWithPermission: " + response);
            return response;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean iccCloseLogicalChannel(int subId, int channel) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "iccCloseLogicalChannel");
        if (DBG) log("iccCloseLogicalChannel: subId=" + subId + " chnl=" + channel);
        return iccCloseLogicalChannelWithPermission(getPhoneFromSubId(subId), channel);
    }

    @Override
    public boolean iccCloseLogicalChannelBySlot(int slotIndex, int channel) {
        enforceModifyPermission();
        if (DBG) log("iccCloseLogicalChannelBySlot: slotIndex=" + slotIndex + " chnl=" + channel);
        return iccCloseLogicalChannelWithPermission(getPhoneFromSlotIdOrThrowException(slotIndex),
                channel);
    }

    private boolean iccCloseLogicalChannelWithPermission(Phone phone, int channel) {
        final long identity = Binder.clearCallingIdentity();
        try {
            if (channel < 0) {
                return false;
            }
            Boolean success = (Boolean) sendRequest(CMD_CLOSE_CHANNEL, channel, phone,
                    null /* workSource */);
            if (DBG) log("iccCloseLogicalChannelWithPermission: " + success);
            return success;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String iccTransmitApduLogicalChannel(int subId, int channel, int cla,
            int command, int p1, int p2, int p3, String data) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "iccTransmitApduLogicalChannel");
        if (DBG) {
            log("iccTransmitApduLogicalChannel: subId=" + subId + " chnl=" + channel
                    + " cla=" + cla + " cmd=" + command + " p1=" + p1 + " p2=" + p2 + " p3="
                    + p3 + " data=" + data);
        }
        return iccTransmitApduLogicalChannelWithPermission(getPhoneFromSubId(subId), channel, cla,
                command, p1, p2, p3, data);
    }

    @Override
    public String iccTransmitApduLogicalChannelBySlot(int slotIndex, int channel, int cla,
            int command, int p1, int p2, int p3, String data) {
        enforceModifyPermission();
        if (DBG) {
            log("iccTransmitApduLogicalChannelBySlot: slotIndex=" + slotIndex + " chnl=" + channel
                    + " cla=" + cla + " cmd=" + command + " p1=" + p1 + " p2=" + p2 + " p3="
                    + p3 + " data=" + data);
        }
        return iccTransmitApduLogicalChannelWithPermission(
                getPhoneFromSlotIdOrThrowException(slotIndex), channel, cla, command, p1, p2, p3,
                data);
    }

    private String iccTransmitApduLogicalChannelWithPermission(Phone phone, int channel, int cla,
            int command, int p1, int p2, int p3, String data) {
        final long identity = Binder.clearCallingIdentity();
        try {
            if (channel <= 0) {
                return "";
            }

            IccIoResult response = (IccIoResult) sendRequest(CMD_TRANSMIT_APDU_LOGICAL_CHANNEL,
                    new IccAPDUArgument(channel, cla, command, p1, p2, p3, data), phone,
                    null /* workSource */);
            if (DBG) log("iccTransmitApduLogicalChannelWithPermission: " + response);

            // Append the returned status code to the end of the response payload.
            String s = Integer.toHexString(
                    (response.sw1 << 8) + response.sw2 + 0x10000).substring(1);
            if (response.payload != null) {
                s = IccUtils.bytesToHexString(response.payload) + s;
            }
            return s;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String iccTransmitApduBasicChannel(int subId, String callingPackage, int cla,
            int command, int p1, int p2, int p3, String data) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "iccTransmitApduBasicChannel");
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        if (DBG) {
            log("iccTransmitApduBasicChannel: subId=" + subId + " cla=" + cla + " cmd="
                    + command + " p1=" + p1 + " p2=" + p2 + " p3=" + p3 + " data=" + data);
        }
        return iccTransmitApduBasicChannelWithPermission(getPhoneFromSubId(subId), callingPackage,
                cla, command, p1, p2, p3, data);
    }

    @Override
    public String iccTransmitApduBasicChannelBySlot(int slotIndex, String callingPackage, int cla,
            int command, int p1, int p2, int p3, String data) {
        enforceModifyPermission();
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        if (DBG) {
            log("iccTransmitApduBasicChannelBySlot: slotIndex=" + slotIndex + " cla=" + cla
                    + " cmd=" + command + " p1=" + p1 + " p2=" + p2 + " p3=" + p3
                    + " data=" + data);
        }

        return iccTransmitApduBasicChannelWithPermission(
                getPhoneFromSlotIdOrThrowException(slotIndex), callingPackage, cla, command, p1,
                p2, p3, data);
    }

    // open APDU basic channel assuming the caller has sufficient permissions
    private String iccTransmitApduBasicChannelWithPermission(Phone phone, String callingPackage,
            int cla, int command, int p1, int p2, int p3, String data) {
        final long identity = Binder.clearCallingIdentity();
        try {
            if (command == SELECT_COMMAND && p1 == SELECT_P1 && p2 == SELECT_P2 && p3 == SELECT_P3
                    && TextUtils.equals(ISDR_AID, data)) {
                // Only allows LPA to select ISD-R.
                ComponentInfo bestComponent = EuiccConnector.findBestComponent(getDefaultPhone()
                        .getContext().getPackageManager());
                if (bestComponent == null
                        || !TextUtils.equals(callingPackage, bestComponent.packageName)) {
                    loge("The calling package is not allowed to select ISD-R.");
                    throw new SecurityException(
                            "The calling package is not allowed to select ISD-R.");
                }
            }

            IccIoResult response = (IccIoResult) sendRequest(CMD_TRANSMIT_APDU_BASIC_CHANNEL,
                    new IccAPDUArgument(0, cla, command, p1, p2, p3, data), phone,
                    null /* workSource */);
            if (DBG) log("iccTransmitApduBasicChannelWithPermission: " + response);

            // Append the returned status code to the end of the response payload.
            String s = Integer.toHexString(
                    (response.sw1 << 8) + response.sw2 + 0x10000).substring(1);
            if (response.payload != null) {
                s = IccUtils.bytesToHexString(response.payload) + s;
            }
            return s;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public byte[] iccExchangeSimIO(int subId, int fileID, int command, int p1, int p2, int p3,
            String filePath) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "iccExchangeSimIO");

        final long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) {
                log("Exchange SIM_IO " + subId + ":" + fileID + ":" + command + " "
                        + p1 + " " + p2 + " " + p3 + ":" + filePath);
            }

            IccIoResult response =
                    (IccIoResult) sendRequest(CMD_EXCHANGE_SIM_IO,
                            new IccAPDUArgument(-1, fileID, command, p1, p2, p3, filePath),
                            subId);

            if (DBG) {
                log("Exchange SIM_IO [R]" + response);
            }

            byte[] result = null;
            int length = 2;
            if (response.payload != null) {
                length = 2 + response.payload.length;
                result = new byte[length];
                System.arraycopy(response.payload, 0, result, 0, response.payload.length);
            } else {
                result = new byte[length];
            }

            result[length - 1] = (byte) response.sw2;
            result[length - 2] = (byte) response.sw1;
            return result;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the forbidden PLMN List from the given app type (ex APPTYPE_USIM)
     * on a particular subscription
     */
    public String[] getForbiddenPlmns(int subId, int appType, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId, "getForbiddenPlmns")) {
            return null;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            if (appType != TelephonyManager.APPTYPE_USIM
                    && appType != TelephonyManager.APPTYPE_SIM) {
                loge("getForbiddenPlmnList(): App Type must be USIM or SIM");
                return null;
            }
            Object response = sendRequest(
                    CMD_GET_FORBIDDEN_PLMNS, new Integer(appType), subId);
            if (response instanceof String[]) {
                return (String[]) response;
            }
            // Response is an Exception of some kind
            // which is signalled to the user as a NULL retval
            return null;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Set the forbidden PLMN list from the given app type (ex APPTYPE_USIM) on a particular
     * subscription.
     *
     * @param subId the id of the subscription.
     * @param appType the uicc app type, must be USIM or SIM.
     * @param fplmns the Forbiden plmns list that needed to be written to the SIM.
     * @param callingPackage the op Package name.
     * @param callingFeatureId the feature in the package.
     * @return number of fplmns that is successfully written to the SIM.
     */
    public int setForbiddenPlmns(int subId, int appType, List<String> fplmns, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(mApp, subId, callingPackage,
                callingFeatureId, "setForbiddenPlmns")) {
            if (DBG) logv("no permissions for setForbiddenplmns");
            throw new IllegalStateException("No Permissions for setForbiddenPlmns");
        }
        if (appType != TelephonyManager.APPTYPE_USIM && appType != TelephonyManager.APPTYPE_SIM) {
            loge("setForbiddenPlmnList(): App Type must be USIM or SIM");
            throw new IllegalArgumentException("Invalid appType: App Type must be USIM or SIM");
        }
        if (fplmns == null) {
            throw new IllegalArgumentException("Fplmn List provided is null");
        }
        for (String fplmn : fplmns) {
            if (!CellIdentity.isValidPlmn(fplmn)) {
                throw new IllegalArgumentException("Invalid fplmn provided: " + fplmn);
            }
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            Object response = sendRequest(
                    CMD_SET_FORBIDDEN_PLMNS,
                    new Pair<Integer, List<String>>(new Integer(appType), fplmns),
                    subId);
            return (int) response;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String sendEnvelopeWithStatus(int subId, String content) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "sendEnvelopeWithStatus");

        final long identity = Binder.clearCallingIdentity();
        try {
            IccIoResult response = (IccIoResult) sendRequest(CMD_SEND_ENVELOPE, content, subId);
            if (response.payload == null) {
                return "";
            }

            // Append the returned status code to the end of the response payload.
            String s = Integer.toHexString(
                    (response.sw1 << 8) + response.sw2 + 0x10000).substring(1);
            s = IccUtils.bytesToHexString(response.payload) + s;
            return s;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Read one of the NV items defined in {@link com.android.internal.telephony.RadioNVItems}
     * and {@code ril_nv_items.h}. Used for device configuration by some CDMA operators.
     *
     * @param itemID the ID of the item to read
     * @return the NV item as a String, or null on error.
     */
    @Override
    public String nvReadItem(int itemID) {
        WorkSource workSource = getWorkSource(Binder.getCallingUid());
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, getDefaultSubscription(), "nvReadItem");

        final long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) log("nvReadItem: item " + itemID);
            String value = (String) sendRequest(CMD_NV_READ_ITEM, itemID, workSource);
            if (DBG) log("nvReadItem: item " + itemID + " is \"" + value + '"');
            return value;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Write one of the NV items defined in {@link com.android.internal.telephony.RadioNVItems}
     * and {@code ril_nv_items.h}. Used for device configuration by some CDMA operators.
     *
     * @param itemID the ID of the item to read
     * @param itemValue the value to write, as a String
     * @return true on success; false on any failure
     */
    @Override
    public boolean nvWriteItem(int itemID, String itemValue) {
        WorkSource workSource = getWorkSource(Binder.getCallingUid());
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, getDefaultSubscription(), "nvWriteItem");

        final long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) log("nvWriteItem: item " + itemID + " value \"" + itemValue + '"');
            Boolean success = (Boolean) sendRequest(CMD_NV_WRITE_ITEM,
                    new Pair<Integer, String>(itemID, itemValue), workSource);
            if (DBG) log("nvWriteItem: item " + itemID + ' ' + (success ? "ok" : "fail"));
            return success;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Update the CDMA Preferred Roaming List (PRL) in the radio NV storage.
     * Used for device configuration by some CDMA operators.
     *
     * @param preferredRoamingList byte array containing the new PRL
     * @return true on success; false on any failure
     */
    @Override
    public boolean nvWriteCdmaPrl(byte[] preferredRoamingList) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, getDefaultSubscription(), "nvWriteCdmaPrl");

        final long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) log("nvWriteCdmaPrl: value: " + HexDump.toHexString(preferredRoamingList));
            Boolean success = (Boolean) sendRequest(CMD_NV_WRITE_CDMA_PRL, preferredRoamingList);
            if (DBG) log("nvWriteCdmaPrl: " + (success ? "ok" : "fail"));
            return success;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Rollback modem configurations to factory default except some config which are in whitelist.
     * Used for device configuration by some CDMA operators.
     *
     * @param slotIndex - device slot.
     *
     * @return true on success; false on any failure
     */
    @Override
    public boolean resetModemConfig(int slotIndex) {
        Phone phone = PhoneFactory.getPhone(slotIndex);
        if (phone != null) {
            TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                    mApp, phone.getSubId(), "resetModemConfig");

            final long identity = Binder.clearCallingIdentity();
            try {
                Boolean success = (Boolean) sendRequest(CMD_RESET_MODEM_CONFIG, null);
                if (DBG) log("resetModemConfig:" + ' ' + (success ? "ok" : "fail"));
                return success;
            } finally {
                Binder.restoreCallingIdentity(identity);
            }
        }
        return false;
    }

    /**
     * Generate a radio modem reset. Used for device configuration by some CDMA operators.
     *
     * @param slotIndex - device slot.
     *
     * @return true on success; false on any failure
     */
    @Override
    public boolean rebootModem(int slotIndex) {
        Phone phone = PhoneFactory.getPhone(slotIndex);
        if (phone != null) {
            TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                    mApp, phone.getSubId(), "rebootModem");

            final long identity = Binder.clearCallingIdentity();
            try {
                Boolean success = (Boolean) sendRequest(CMD_MODEM_REBOOT, null);
                if (DBG) log("rebootModem:" + ' ' + (success ? "ok" : "fail"));
                return success;
            } finally {
                Binder.restoreCallingIdentity(identity);
            }
        }
        return false;
    }

    public String[] getPcscfAddress(String apnType, String callingPackage,
            String callingFeatureId) {
        final Phone defaultPhone = getDefaultPhone();
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(mApp, defaultPhone.getSubId(),
                callingPackage, callingFeatureId, "getPcscfAddress")) {
            return new String[0];
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return defaultPhone.getPcscfAddress(apnType);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Toggle IMS disable and enable for the framework to reset it. See {@link #enableIms(int)} and
     * {@link #disableIms(int)}.
     * @param slotIndex device slot.
     */
    public void resetIms(int slotIndex) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) {
                // may happen if the does not support IMS.
                return;
            }
            mImsResolver.disableIms(slotIndex);
            mImsResolver.enableIms(slotIndex);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Enables IMS for the framework. This will trigger IMS registration and ImsFeature capability
     * status updates, if not already enabled.
     */
    public void enableIms(int slotId) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) {
                // may happen if the device does not support IMS.
                return;
            }
            mImsResolver.enableIms(slotId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Disables IMS for the framework. This will trigger IMS de-registration and trigger ImsFeature
     * status updates to disabled.
     */
    public void disableIms(int slotId) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) {
                // may happen if the device does not support IMS.
                return;
            }
            mImsResolver.disableIms(slotId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the {@link IImsMmTelFeature} that corresponds to the given slot Id for the MMTel
     * feature or {@link null} if the service is not available. If the feature is available, the
     * {@link IImsServiceFeatureCallback} callback is registered as a listener for feature updates.
     */
    public IImsMmTelFeature getMmTelFeatureAndListen(int slotId,
            IImsServiceFeatureCallback callback) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) {
                // may happen if the device does not support IMS.
                return null;
            }
            return mImsResolver.getMmTelFeatureAndListen(slotId, callback);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the {@link IImsRcsFeature} that corresponds to the given slot Id for the RCS
     * feature during emergency calling or {@link null} if the service is not available. If the
     * feature is available, the {@link IImsServiceFeatureCallback} callback is registered as a
     * listener for feature updates.
     */
    public IImsRcsFeature getRcsFeatureAndListen(int slotId, IImsServiceFeatureCallback callback) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) {
                // may happen if the device does not support IMS.
                return null;
            }
            return mImsResolver.getRcsFeatureAndListen(slotId, callback);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Unregister a previously registered IImsServiceFeatureCallback associated with an ImsFeature.
     */
    public void unregisterImsFeatureCallback(int slotId, int featureType,
            IImsServiceFeatureCallback callback) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) return;
            mImsResolver.unregisterImsFeatureCallback(slotId, featureType, callback);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the {@link IImsRegistration} structure associated with the slotId and feature
     * specified or null if IMS is not supported on the slot specified.
     */
    public IImsRegistration getImsRegistration(int slotId, int feature) throws RemoteException {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) {
                // may happen if the device does not support IMS.
                return null;
            }
            return mImsResolver.getImsRegistration(slotId, feature);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the {@link IImsConfig} structure associated with the slotId and feature
     * specified or null if IMS is not supported on the slot specified.
     */
    public IImsConfig getImsConfig(int slotId, int feature) throws RemoteException {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) {
                // may happen if the device does not support IMS.
                return null;
            }
            return mImsResolver.getImsConfig(slotId, feature);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Sets the ImsService Package Name that Telephony will bind to.
     *
     * @param slotIndex the slot ID that the ImsService should bind for.
     * @param isCarrierService true if the ImsService is the carrier override, false if the
     *         ImsService is the device default ImsService.
     * @param featureTypes An integer array of feature types associated with a packageName.
     * @param packageName The name of the package that the current configuration will be replaced
     *                    with.
     * @return true if setting the ImsService to bind to succeeded, false if it did not.
     */
    public boolean setBoundImsServiceOverride(int slotIndex, boolean isCarrierService,
            int[] featureTypes, String packageName) {
        int[] subIds = SubscriptionManager.getSubId(slotIndex);
        TelephonyPermissions.enforceShellOnly(Binder.getCallingUid(), "setBoundImsServiceOverride");
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(mApp,
                (subIds != null ? subIds[0] : SubscriptionManager.INVALID_SUBSCRIPTION_ID),
                "setBoundImsServiceOverride");

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) {
                // may happen if the device does not support IMS.
                return false;
            }
            Map<Integer, String> featureConfig = new HashMap<>();
            for (int featureType : featureTypes) {
                featureConfig.put(featureType, packageName);
            }
            return mImsResolver.overrideImsServiceConfiguration(slotIndex, isCarrierService,
                    featureConfig);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Return the package name of the currently bound ImsService.
     *
     * @param slotId The slot that the ImsService is associated with.
     * @param isCarrierImsService true, if the ImsService is a carrier override, false if it is
     *         the device default.
     * @param featureType The feature associated with the queried configuration.
     * @return the package name of the ImsService configuration.
     */
    public String getBoundImsServicePackage(int slotId, boolean isCarrierImsService,
            @ImsFeature.FeatureType int featureType) {
        int[] subIds = SubscriptionManager.getSubId(slotId);
        TelephonyPermissions
                .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                mApp, (subIds != null ? subIds[0] : SubscriptionManager.INVALID_SUBSCRIPTION_ID),
                "getBoundImsServicePackage");

        final long identity = Binder.clearCallingIdentity();
        try {
            if (mImsResolver == null) {
                // may happen if the device does not support IMS.
                return "";
            }
            // TODO: change API to query RCS separately.
            return mImsResolver.getImsServiceConfiguration(slotId, isCarrierImsService,
                    featureType);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the MmTelFeature state associated with the requested subscription id.
     * @param subId The subscription that the MmTelFeature is associated with.
     * @param callback A callback with an integer containing the
     * {@link android.telephony.ims.feature.ImsFeature.ImsState} associated with the MmTelFeature.
     */
    @Override
    public void getImsMmTelFeatureState(int subId, IIntegerConsumer callback) {
        enforceReadPrivilegedPermission("getImsMmTelFeatureState");
        if (!ImsManager.isImsSupportedOnDevice(mApp)) {
            throw new ServiceSpecificException(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION,
                    "IMS not available on device.");
        }
        final long token = Binder.clearCallingIdentity();
        try {
            int slotId = getSlotIndex(subId);
            if (slotId <= SubscriptionManager.INVALID_SIM_SLOT_INDEX) {
                Log.w(LOG_TAG, "getImsMmTelFeatureState: called with an inactive subscription '"
                        + subId + "'");
                throw new ServiceSpecificException(ImsException.CODE_ERROR_INVALID_SUBSCRIPTION);
            }
            ImsManager.getInstance(mApp, slotId).getImsServiceState(anInteger -> {
                try {
                    callback.accept(anInteger == null ? ImsFeature.STATE_UNAVAILABLE : anInteger);
                } catch (RemoteException e) {
                    Log.w(LOG_TAG, "getImsMmTelFeatureState: remote caller is no longer running. "
                            + "Ignore");
                }
            });
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    public void setImsRegistrationState(boolean registered) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            getDefaultPhone().setImsRegistrationState(registered);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Set the network selection mode to automatic.
     *
     */
    @Override
    public void setNetworkSelectionModeAutomatic(int subId) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setNetworkSelectionModeAutomatic");

        final long identity = Binder.clearCallingIdentity();
        try {
            if (!isActiveSubscription(subId)) {
                return;
            }
            if (DBG) log("setNetworkSelectionModeAutomatic: subId " + subId);
            sendRequest(CMD_SET_NETWORK_SELECTION_MODE_AUTOMATIC, null, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

   /**
     * Ask the radio to connect to the input network and change selection mode to manual.
     *
     * @param subId the id of the subscription.
     * @param operatorInfo the operator information, included the PLMN, long name and short name of
     * the operator to attach to.
     * @param persistSelection whether the selection will persist until reboot. If true, only allows
     * attaching to the selected PLMN until reboot; otherwise, attach to the chosen PLMN and resume
     * normal network selection next time.
     * @return {@code true} on success; {@code true} on any failure.
     */
    @Override
    public boolean setNetworkSelectionModeManual(
            int subId, OperatorInfo operatorInfo, boolean persistSelection) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setNetworkSelectionModeManual");

        if (!isActiveSubscription(subId)) {
            return false;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            ManualNetworkSelectionArgument arg = new ManualNetworkSelectionArgument(operatorInfo,
                    persistSelection);
            if (DBG) {
                log("setNetworkSelectionModeManual: subId: " + subId
                        + " operator: " + operatorInfo);
            }
            return (Boolean) sendRequest(CMD_SET_NETWORK_SELECTION_MODE_MANUAL, arg, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }
     /**
     * Get the manual network selection
     *
     * @param subId the id of the subscription.
     *
     * @return the previously saved user selected PLMN
     */
    @Override
    public String getManualNetworkSelectionPlmn(int subId) {
        TelephonyPermissions
                    .enforeceCallingOrSelfReadPrecisePhoneStatePermissionOrCarrierPrivilege(
                    mApp, subId, "getManualNetworkSelectionPlmn");

        final long identity = Binder.clearCallingIdentity();
        try {
            if (!isActiveSubscription(subId)) {
                return "";
            }

            final Phone phone = getPhone(subId);
            if (phone == null) {
                return "";
            }
            OperatorInfo networkSelection = phone.getSavedNetworkSelection();
            return TextUtils.isEmpty(networkSelection.getOperatorNumeric())
                ? phone.getManualNetworkSelectionPlmn() : networkSelection.getOperatorNumeric();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Scans for available networks.
     */
    @Override
    public CellNetworkScanResult getCellNetworkScanResults(int subId, String callingPackage,
            String callingFeatureId) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "getCellNetworkScanResults");
        LocationAccessPolicy.LocationPermissionResult locationResult =
                LocationAccessPolicy.checkLocationPermission(mApp,
                        new LocationAccessPolicy.LocationPermissionQuery.Builder()
                                .setCallingPackage(callingPackage)
                                .setCallingFeatureId(callingFeatureId)
                                .setCallingPid(Binder.getCallingPid())
                                .setCallingUid(Binder.getCallingUid())
                                .setMethod("getCellNetworkScanResults")
                                .setMinSdkVersionForFine(Build.VERSION_CODES.Q)
                                .build());
        switch (locationResult) {
            case DENIED_HARD:
                throw new SecurityException("Not allowed to access scan results -- location");
            case DENIED_SOFT:
                return null;
        }

        long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) log("getCellNetworkScanResults: subId " + subId);
            return (CellNetworkScanResult) sendRequest(
                    CMD_PERFORM_NETWORK_SCAN, null, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the call forwarding info, given the call forwarding reason.
     */
    @Override
    public CallForwardingInfo getCallForwarding(int subId, int callForwardingReason) {
        enforceReadPrivilegedPermission("getCallForwarding");
        long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) {
                log("getCallForwarding: subId " + subId
                        + " callForwardingReason" + callForwardingReason);
            }
            return (CallForwardingInfo) sendRequest(
                    CMD_GET_CALL_FORWARDING, callForwardingReason, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Sets the voice call forwarding info including status (enable/disable), call forwarding
     * reason, the number to forward, and the timeout before the forwarding is attempted.
     */
    @Override
    public boolean setCallForwarding(int subId, CallForwardingInfo callForwardingInfo) {
        enforceModifyPermission();
        long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) {
                log("setCallForwarding: subId " + subId
                        + " callForwardingInfo" + callForwardingInfo);
            }
            return (Boolean) sendRequest(CMD_SET_CALL_FORWARDING, callForwardingInfo, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the call forwarding info, given the call forwarding reason.
     */
    @Override
    public int getCallWaitingStatus(int subId) {
        enforceReadPrivilegedPermission("getCallForwarding");
        long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) log("getCallWaitingStatus: subId " + subId);
            return (Integer) sendRequest(CMD_GET_CALL_WAITING, null, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Sets the voice call forwarding info including status (enable/disable), call forwarding
     * reason, the number to forward, and the timeout before the forwarding is attempted.
     */
    @Override
    public boolean setCallWaitingStatus(int subId, boolean isEnable) {
        enforceModifyPermission();
        long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) log("setCallWaitingStatus: subId " + subId + " isEnable: " + isEnable);
            return (Boolean) sendRequest(CMD_SET_CALL_WAITING, isEnable, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Starts a new network scan and returns the id of this scan.
     *
     * @param subId id of the subscription
     * @param request contains the radio access networks with bands/channels to scan
     * @param messenger callback messenger for scan results or errors
     * @param binder for the purpose of auto clean when the user thread crashes
     * @return the id of the requested scan which can be used to stop the scan.
     */
    @Override
    public int requestNetworkScan(int subId, NetworkScanRequest request, Messenger messenger,
            IBinder binder, String callingPackage, String callingFeatureId) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "requestNetworkScan");
        LocationAccessPolicy.LocationPermissionResult locationResult =
                LocationAccessPolicy.checkLocationPermission(mApp,
                        new LocationAccessPolicy.LocationPermissionQuery.Builder()
                                .setCallingPackage(callingPackage)
                                .setCallingFeatureId(callingFeatureId)
                                .setCallingPid(Binder.getCallingPid())
                                .setCallingUid(Binder.getCallingUid())
                                .setMethod("requestNetworkScan")
                                .setMinSdkVersionForFine(Build.VERSION_CODES.Q)
                                .build());
        if (locationResult != LocationAccessPolicy.LocationPermissionResult.ALLOWED) {
            SecurityException e = checkNetworkRequestForSanitizedLocationAccess(
                    request, subId, callingPackage);
            if (e != null) {
                if (locationResult == LocationAccessPolicy.LocationPermissionResult.DENIED_HARD) {
                    throw e;
                } else {
                    loge(e.getMessage());
                    return TelephonyScanManager.INVALID_SCAN_ID;
                }
            }
        }
        int callingUid = Binder.getCallingUid();
        int callingPid = Binder.getCallingPid();
        final long identity = Binder.clearCallingIdentity();
        try {
            return mNetworkScanRequestTracker.startNetworkScan(
                    request, messenger, binder, getPhone(subId),
                    callingUid, callingPid, callingPackage);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private SecurityException checkNetworkRequestForSanitizedLocationAccess(
            NetworkScanRequest request, int subId, String callingPackage) {
        boolean hasCarrierPriv = checkCarrierPrivilegesForPackage(subId, callingPackage)
                == TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS;
        boolean hasNetworkScanPermission =
                mApp.checkCallingOrSelfPermission(android.Manifest.permission.NETWORK_SCAN)
                == PERMISSION_GRANTED;

        if (!hasCarrierPriv && !hasNetworkScanPermission) {
            return new SecurityException("permission.NETWORK_SCAN or carrier privileges is needed"
                    + " for network scans without location access.");
        }

        if (request.getSpecifiers() != null && request.getSpecifiers().length > 0) {
            for (RadioAccessSpecifier ras : request.getSpecifiers()) {
                if (ras.getChannels() != null && ras.getChannels().length > 0) {
                    return new SecurityException("Specific channels must not be"
                            + " scanned without location access.");
                }
            }
        }

        return null;
    }

    /**
     * Stops an existing network scan with the given scanId.
     *
     * @param subId id of the subscription
     * @param scanId id of the scan that needs to be stopped
     */
    @Override
    public void stopNetworkScan(int subId, int scanId) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "stopNetworkScan");

        int callingUid = Binder.getCallingUid();
        final long identity = Binder.clearCallingIdentity();
        try {
            mNetworkScanRequestTracker.stopNetworkScan(scanId, callingUid);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the calculated preferred network type.
     * Used for debugging incorrect network type.
     *
     * @return the preferred network type, defined in RILConstants.java.
     */
    @Override
    public int getCalculatedPreferredNetworkType(String callingPackage, String callingFeatureId) {
        final Phone defaultPhone = getDefaultPhone();
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(mApp, defaultPhone.getSubId(),
                callingPackage, callingFeatureId, "getCalculatedPreferredNetworkType")) {
            return RILConstants.PREFERRED_NETWORK_MODE;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            // FIXME: need to get SubId from somewhere.
            return PhoneFactory.calculatePreferredNetworkType(defaultPhone.getContext(), 0);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the preferred network type.
     * Used for device configuration by some CDMA operators.
     *
     * @return the preferred network type, defined in RILConstants.java.
     */
    @Override
    public int getPreferredNetworkType(int subId) {
        TelephonyPermissions
                .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                        mApp, subId, "getPreferredNetworkType");

        final long identity = Binder.clearCallingIdentity();
        try {
            if (DBG) log("getPreferredNetworkType");
            int[] result = (int[]) sendRequest(CMD_GET_PREFERRED_NETWORK_TYPE, null, subId);
            int networkType = (result != null ? result[0] : -1);
            if (DBG) log("getPreferredNetworkType: " + networkType);
            return networkType;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Set the preferred network type.
     *
     * @param networkType the preferred network type, defined in RILConstants.java.
     * @return true on success; false on any failure.
     */
    @Override
    public boolean setPreferredNetworkType(int subId, int networkType) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setPreferredNetworkType");

        final long identity = Binder.clearCallingIdentity();
        try {
            Boolean success = (Boolean) sendRequest(
                    CMD_SET_PREFERRED_NETWORK_TYPE, networkType, subId);

            if (success) {
                Settings.Global.putInt(mApp.getContentResolver(),
                        Settings.Global.PREFERRED_NETWORK_MODE + subId, networkType);
            }
            if (DBG) log("setPreferredNetworkType: " + (success ? "ok" : "fail"));
            return success;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the allowed network types that store in the telephony provider.
     *
     * @param subId the id of the subscription.
     * @return allowedNetworkTypes the allowed network types.
     */
    @Override
    public long getAllowedNetworkTypes(int subId) {
        TelephonyPermissions
                .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                    mApp, subId, "getAllowedNetworkTypes");

        final long identity = Binder.clearCallingIdentity();
        try {
            return SubscriptionManager.getLongSubscriptionProperty(
                    subId, SubscriptionManager.ALLOWED_NETWORK_TYPES, -1, mApp);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Set the allowed network types.
     *
     * @param subId the id of the subscription.
     * @param allowedNetworkTypes the allowed network types.
     * @return true on success; false on any failure.
     */
    @Override
    public boolean setAllowedNetworkTypes(int subId, long allowedNetworkTypes) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setAllowedNetworkTypes");

        SubscriptionManager.setSubscriptionProperty(subId,
                SubscriptionManager.ALLOWED_NETWORK_TYPES,
                String.valueOf(allowedNetworkTypes));

        int preferredNetworkMode = Settings.Global.getInt(mApp.getContentResolver(),
                Settings.Global.PREFERRED_NETWORK_MODE + subId,
                RILConstants.PREFERRED_NETWORK_MODE);
        return setPreferredNetworkType(subId, preferredNetworkMode);
    }

    /**
     * Get the allowed network types for certain reason.
     *
     * @param subId the id of the subscription.
     * @param reason the reason the allowed network type change is taking place
     * @return the allowed network types.
     */
    @Override
    public long getAllowedNetworkTypesForReason(int subId,
            @TelephonyManager.AllowedNetworkTypesReason int reason) {
        TelephonyPermissions
                .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                        mApp, subId, "getAllowedNetworkTypesForReason");
        final long identity = Binder.clearCallingIdentity();
        try {
            return getPhoneFromSubId(subId).getAllowedNetworkTypes(reason);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the effective allowed network types on the device.
     * This API will return an intersection of allowed network types for all reasons,
     * including the configuration done through setAllowedNetworkTypes
     *
     * @param subId the id of the subscription.
     * @return the allowed network types
     */
    @Override
    public long getEffectiveAllowedNetworkTypes(int subId) {
        TelephonyPermissions
                .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                        mApp, subId, "getEffectiveAllowedNetworkTypes");
        final long identity = Binder.clearCallingIdentity();
        try {
            return getPhoneFromSubId(subId).getEffectiveAllowedNetworkTypes();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Set the allowed network types of the device and
     * provide the reason triggering the allowed network change.
     *
     * @param subId the id of the subscription.
     * @param reason the reason the allowed network type change is taking place
     * @param allowedNetworkTypes the allowed network types.
     * @return true on success; false on any failure.
     */
    @Override
    public boolean setAllowedNetworkTypesForReason(int subId,
            @TelephonyManager.AllowedNetworkTypesReason int reason, long allowedNetworkTypes) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setAllowedNetworkTypesForReason");
        final long identity = Binder.clearCallingIdentity();
        try {
            getPhoneFromSubId(subId).setAllowedNetworkTypes(reason, allowedNetworkTypes);
            int preferredNetworkMode = Settings.Global.getInt(mApp.getContentResolver(),
                    Settings.Global.PREFERRED_NETWORK_MODE + subId,
                    RILConstants.PREFERRED_NETWORK_MODE);
            return setPreferredNetworkType(subId, preferredNetworkMode);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Check whether DUN APN is required for tethering with subId.
     *
     * @param subId the id of the subscription to require tethering.
     * @return {@code true} if DUN APN is required for tethering.
     * @hide
     */
    @Override
    public boolean isTetheringApnRequiredForSubscriber(int subId) {
        enforceModifyPermission();
        final long identity = Binder.clearCallingIdentity();
        final Phone phone = getPhone(subId);
        try {
            if (phone != null) {
                return phone.hasMatchedTetherApnSetting();
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Set mobile data enabled
     * Used by the user through settings etc to turn on/off mobile data
     *
     * @param enable {@code true} turn turn data on, else {@code false}
     */
    @Override
    public void setUserDataEnabled(int subId, boolean enable) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setUserDataEnabled");

        final long identity = Binder.clearCallingIdentity();
        try {
            int phoneId = mSubscriptionController.getPhoneId(subId);
            if (DBG) log("setUserDataEnabled: subId=" + subId + " phoneId=" + phoneId);
            Phone phone = PhoneFactory.getPhone(phoneId);
            if (phone != null) {
                if (DBG) log("setUserDataEnabled: subId=" + subId + " enable=" + enable);
                phone.getDataEnabledSettings().setUserDataEnabled(enable);
            } else {
                loge("setUserDataEnabled: no phone found. Invalid subId=" + subId);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Enable or disable always reporting signal strength changes from radio.
     *
     * @param isEnable {@code true} for enabling; {@code false} for disabling.
     */
    @Override
    public void setAlwaysReportSignalStrength(int subId, boolean isEnable) {
        enforceModifyPermission();
        enforceSystemCaller();

        final long identity = Binder.clearCallingIdentity();
        final Phone phone = getPhone(subId);
        try {
            if (phone != null) {
                if (DBG) {
                    log("setAlwaysReportSignalStrength: subId=" + subId
                            + " isEnable=" + isEnable);
                }
                phone.setAlwaysReportSignalStrength(isEnable);
            } else {
                loge("setAlwaysReportSignalStrength: no phone found for subId="
                        + subId);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the user enabled state of Mobile Data.
     *
     * TODO: remove and use isUserDataEnabled.
     * This can't be removed now because some vendor codes
     * calls through ITelephony directly while they should
     * use TelephonyManager.
     *
     * @return true on enabled
     */
    @Override
    public boolean getDataEnabled(int subId) {
        return isUserDataEnabled(subId);
    }

    /**
     * Get whether mobile data is enabled per user setting.
     *
     * There are other factors deciding whether mobile data is actually enabled, but they are
     * not considered here. See {@link #isDataEnabled(int)} for more details.
     *
     * Accepts either ACCESS_NETWORK_STATE, MODIFY_PHONE_STATE or carrier privileges.
     *
     * @return {@code true} if data is enabled else {@code false}
     */
    @Override
    public boolean isUserDataEnabled(int subId) {
        try {
            mApp.enforceCallingOrSelfPermission(android.Manifest.permission.ACCESS_NETWORK_STATE,
                    null);
        } catch (Exception e) {
            TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                    mApp, subId, "isUserDataEnabled");
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            int phoneId = mSubscriptionController.getPhoneId(subId);
            if (DBG) log("isUserDataEnabled: subId=" + subId + " phoneId=" + phoneId);
            Phone phone = PhoneFactory.getPhone(phoneId);
            if (phone != null) {
                boolean retVal = phone.isUserDataEnabled();
                if (DBG) log("isUserDataEnabled: subId=" + subId + " retVal=" + retVal);
                return retVal;
            } else {
                if (DBG) loge("isUserDataEnabled: no phone subId=" + subId + " retVal=false");
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Checks if the device is capable of mobile data by considering whether whether the
     * user has enabled mobile data, whether the carrier has enabled mobile data, and
     * whether the network policy allows data connections.
     *
     * @return {@code true} if the overall data connection is capable; {@code false} if not.
     */
    @Override
    public boolean isDataEnabled(int subId) {
        enforceReadPrivilegedPermission("isDataEnabled");

        final long identity = Binder.clearCallingIdentity();
        try {
            int phoneId = mSubscriptionController.getPhoneId(subId);
            if (DBG) log("isDataEnabled: subId=" + subId + " phoneId=" + phoneId);
            Phone phone = PhoneFactory.getPhone(phoneId);
            if (phone != null) {
                boolean retVal = phone.getDataEnabledSettings().isDataEnabled();
                if (DBG) log("isDataEnabled: subId=" + subId + " retVal=" + retVal);
                return retVal;
            } else {
                if (DBG) loge("isDataEnabled: no phone subId=" + subId + " retVal=false");
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private int getCarrierPrivilegeStatusFromCarrierConfigRules(int privilegeFromSim, int uid,
            Phone phone) {
        if (uid == Process.PHONE_UID) {
            // Skip the check if it's the phone UID (system UID removed in b/184713596)
            // TODO (b/184954344): Check for system/phone UID at call site instead of here
            return TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS;
        }

        //load access rules from carrier configs, and check those as well: b/139133814
        SubscriptionController subController = SubscriptionController.getInstance();
        if (privilegeFromSim == TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS
                || subController == null) return privilegeFromSim;

        PackageManager pkgMgr = phone.getContext().getPackageManager();
        String[] packages = pkgMgr.getPackagesForUid(uid);

        final long identity = Binder.clearCallingIdentity();
        try {
            SubscriptionInfo subInfo = subController.getSubscriptionInfo(phone.getSubId());
            SubscriptionManager subManager = (SubscriptionManager)
                    phone.getContext().getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);
            for (String pkg : packages) {
                if (subManager.canManageSubscription(subInfo, pkg)) {
                    return TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS;
                }
            }
            return privilegeFromSim;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private int getCarrierPrivilegeStatusFromCarrierConfigRules(int privilegeFromSim, Phone phone,
            String pkgName) {
        //load access rules from carrier configs, and check those as well: b/139133814
        SubscriptionController subController = SubscriptionController.getInstance();
        if (privilegeFromSim == TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS
                || subController == null) return privilegeFromSim;

        final long identity = Binder.clearCallingIdentity();
        try {
            SubscriptionInfo subInfo = subController.getSubscriptionInfo(phone.getSubId());
            SubscriptionManager subManager = (SubscriptionManager)
                    phone.getContext().getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);
            return subManager.canManageSubscription(subInfo, pkgName)
                ? TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS : privilegeFromSim;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getCarrierPrivilegeStatus(int subId) {
        final Phone phone = getPhone(subId);
        if (phone == null) {
            loge("getCarrierPrivilegeStatus: Invalid subId");
            return TelephonyManager.CARRIER_PRIVILEGE_STATUS_NO_ACCESS;
        }
        UiccCard card = UiccController.getInstance().getUiccCard(phone.getPhoneId());
        if (card == null) {
            loge("getCarrierPrivilegeStatus: No UICC");
            return TelephonyManager.CARRIER_PRIVILEGE_STATUS_RULES_NOT_LOADED;
        }

        return getCarrierPrivilegeStatusFromCarrierConfigRules(
            card.getCarrierPrivilegeStatusForCurrentTransaction(
                phone.getContext().getPackageManager()), Binder.getCallingUid(), phone);
    }

    @Override
    public int getCarrierPrivilegeStatusForUid(int subId, int uid) {
        enforceReadPrivilegedPermission("getCarrierPrivilegeStatusForUid");
        final Phone phone = getPhone(subId);
        if (phone == null) {
            loge("getCarrierPrivilegeStatusForUid: Invalid subId");
            return TelephonyManager.CARRIER_PRIVILEGE_STATUS_NO_ACCESS;
        }
        UiccProfile profile =
                UiccController.getInstance().getUiccProfileForPhone(phone.getPhoneId());
        if (profile == null) {
            loge("getCarrierPrivilegeStatusForUid: No UICC");
            return TelephonyManager.CARRIER_PRIVILEGE_STATUS_RULES_NOT_LOADED;
        }
        return getCarrierPrivilegeStatusFromCarrierConfigRules(
                profile.getCarrierPrivilegeStatusForUid(
                        phone.getContext().getPackageManager(), uid), uid, phone);
    }

    @Override
    public int checkCarrierPrivilegesForPackage(int subId, String pkgName) {
        if (TextUtils.isEmpty(pkgName)) {
            return TelephonyManager.CARRIER_PRIVILEGE_STATUS_NO_ACCESS;
        }

        int phoneId = SubscriptionManager.getPhoneId(subId);
        UiccCard card = UiccController.getInstance().getUiccCard(phoneId);
        if (card == null) {
            loge("checkCarrierPrivilegesForPackage: No UICC on subId " + subId);
            return TelephonyManager.CARRIER_PRIVILEGE_STATUS_RULES_NOT_LOADED;
        }
        return getCarrierPrivilegeStatusFromCarrierConfigRules(
            card.getCarrierPrivilegeStatus(mApp.getPackageManager(), pkgName),
            getPhone(phoneId), pkgName);
    }

    @Override
    public int checkCarrierPrivilegesForPackageAnyPhone(String pkgName) {
        if (TextUtils.isEmpty(pkgName))
            return TelephonyManager.CARRIER_PRIVILEGE_STATUS_NO_ACCESS;
        int result = TelephonyManager.CARRIER_PRIVILEGE_STATUS_RULES_NOT_LOADED;
        for (int i = 0; i < TelephonyManager.getDefault().getPhoneCount(); i++) {
            UiccCard card = UiccController.getInstance().getUiccCard(i);
            if (card == null) {
              // No UICC in that slot.
              continue;
            }

            result = getCarrierPrivilegeStatusFromCarrierConfigRules(
                card.getCarrierPrivilegeStatus(mApp.getPackageManager(), pkgName),
                getPhone(i), pkgName);
            if (result == TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS) {
                break;
            }
        }

        return result;
    }

    @Override
    public List<String> getCarrierPackageNamesForIntentAndPhone(Intent intent, int phoneId) {
        if (!SubscriptionManager.isValidPhoneId(phoneId)) {
            loge("phoneId " + phoneId + " is not valid.");
            return null;
        }
        UiccCard card = UiccController.getInstance().getUiccCard(phoneId);
        if (card == null) {
            loge("getCarrierPackageNamesForIntentAndPhone: No UICC");
            return null ;
        }
        return card.getCarrierPackageNamesForIntent(mApp.getPackageManager(), intent);
    }

    @Override
    public List<String> getPackagesWithCarrierPrivileges(int phoneId) {
        PackageManager pm = mApp.getPackageManager();
        List<String> privilegedPackages = new ArrayList<>();
        List<PackageInfo> packages = null;
        UiccCard card = UiccController.getInstance().getUiccCard(phoneId);
        // has UICC in that slot.
        if (card != null) {
            if (card.hasCarrierPrivilegeRules()) {
                if (packages == null) {
                    // Only check packages in user 0 for now
                    packages = pm.getInstalledPackagesAsUser(
                        PackageManager.MATCH_DISABLED_COMPONENTS
                            | PackageManager.MATCH_DISABLED_UNTIL_USED_COMPONENTS
                            | PackageManager.GET_SIGNING_CERTIFICATES,
                            UserHandle.SYSTEM.getIdentifier());
                }
                for (int p = packages.size() - 1; p >= 0; p--) {
                    PackageInfo pkgInfo = packages.get(p);
                    if (pkgInfo != null && pkgInfo.packageName != null
                            && card.getCarrierPrivilegeStatus(pkgInfo)
                            == TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS) {
                        privilegedPackages.add(pkgInfo.packageName);
                    }
                }
            }
        }
        return privilegedPackages;
    }

    @Override
    public List<String> getPackagesWithCarrierPrivilegesForAllPhones() {
        enforceReadPrivilegedPermission("getPackagesWithCarrierPrivilegesForAllPhones");

        final long identity = Binder.clearCallingIdentity();

        List<String> privilegedPackages = new ArrayList<>();
        try {
            for (int i = 0; i < TelephonyManager.getDefault().getPhoneCount(); i++) {
                privilegedPackages.addAll(getPackagesWithCarrierPrivileges(i));
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return privilegedPackages;
    }

    private String getIccId(int subId) {
        final Phone phone = getPhone(subId);
        UiccCard card = phone == null ? null : phone.getUiccCard();
        if (card == null) {
            loge("getIccId: No UICC");
            return null;
        }
        String iccId = card.getIccId();
        if (TextUtils.isEmpty(iccId)) {
            loge("getIccId: ICC ID is null or empty.");
            return null;
        }
        return iccId;
    }

    @Override
    public boolean setLine1NumberForDisplayForSubscriber(int subId, String alphaTag,
            String number) {
        TelephonyPermissions.enforceCallingOrSelfCarrierPrivilege(mApp,
                subId, "setLine1NumberForDisplayForSubscriber");

        final long identity = Binder.clearCallingIdentity();
        try {
            final String iccId = getIccId(subId);
            final Phone phone = getPhone(subId);
            if (phone == null) {
                return false;
            }
            final String subscriberId = phone.getSubscriberId();

            if (DBG_MERGE) {
                Rlog.d(LOG_TAG, "Setting line number for ICC=" + iccId + ", subscriberId="
                        + subscriberId + " to " + number);
            }

            if (TextUtils.isEmpty(iccId)) {
                return false;
            }

            final SharedPreferences.Editor editor = mTelephonySharedPreferences.edit();

            final String alphaTagPrefKey = PREF_CARRIERS_ALPHATAG_PREFIX + iccId;
            if (alphaTag == null) {
                editor.remove(alphaTagPrefKey);
            } else {
                editor.putString(alphaTagPrefKey, alphaTag);
            }

            // Record both the line number and IMSI for this ICCID, since we need to
            // track all merged IMSIs based on line number
            final String numberPrefKey = PREF_CARRIERS_NUMBER_PREFIX + iccId;
            final String subscriberPrefKey = PREF_CARRIERS_SUBSCRIBER_PREFIX + iccId;
            if (number == null) {
                editor.remove(numberPrefKey);
                editor.remove(subscriberPrefKey);
            } else {
                editor.putString(numberPrefKey, number);
                editor.putString(subscriberPrefKey, subscriberId);
            }

            editor.commit();
            return true;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getLine1NumberForDisplay(int subId, String callingPackage,
            String callingFeatureId) {
        // This is open to apps with WRITE_SMS.
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneNumber(
                mApp, subId, callingPackage, callingFeatureId, "getLine1NumberForDisplay")) {
            if (DBG_MERGE) log("getLine1NumberForDisplay returning null due to permission");
            return null;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            String iccId = getIccId(subId);
            if (iccId != null) {
                String numberPrefKey = PREF_CARRIERS_NUMBER_PREFIX + iccId;
                if (DBG_MERGE) {
                    log("getLine1NumberForDisplay returning "
                            + mTelephonySharedPreferences.getString(numberPrefKey, null));
                }
                return mTelephonySharedPreferences.getString(numberPrefKey, null);
            }
            if (DBG_MERGE) log("getLine1NumberForDisplay returning null as iccId is null");
            return null;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getLine1AlphaTagForDisplay(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId, "getLine1AlphaTagForDisplay")) {
            return null;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            String iccId = getIccId(subId);
            if (iccId != null) {
                String alphaTagPrefKey = PREF_CARRIERS_ALPHATAG_PREFIX + iccId;
                return mTelephonySharedPreferences.getString(alphaTagPrefKey, null);
            }
            return null;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String[] getMergedSubscriberIds(int subId, String callingPackage,
            String callingFeatureId) {
        // This API isn't public, so no need to provide a valid subscription ID - we're not worried
        // about carrier-privileged callers not having access.
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, SubscriptionManager.INVALID_SUBSCRIPTION_ID, callingPackage,
                callingFeatureId, "getMergedSubscriberIds")) {
            return null;
        }

        // Clear calling identity, when calling TelephonyManager, because callerUid must be
        // the process, where TelephonyManager was instantiated.
        // Otherwise AppOps check will fail.
        final long identity  = Binder.clearCallingIdentity();
        try {
            final Context context = mApp;
            final TelephonyManager tele = TelephonyManager.from(context);
            final SubscriptionManager sub = SubscriptionManager.from(context);

            // Figure out what subscribers are currently active
            final ArraySet<String> activeSubscriberIds = new ArraySet<>();

            // Only consider subs which match the current subId
            // This logic can be simplified. See b/131189269 for progress.
            if (isActiveSubscription(subId)) {
                activeSubscriberIds.add(tele.getSubscriberId(subId));
            }

            // First pass, find a number override for an active subscriber
            String mergeNumber = null;
            final Map<String, ?> prefs = mTelephonySharedPreferences.getAll();
            for (String key : prefs.keySet()) {
                if (key.startsWith(PREF_CARRIERS_SUBSCRIBER_PREFIX)) {
                    final String subscriberId = (String) prefs.get(key);
                    if (activeSubscriberIds.contains(subscriberId)) {
                        final String iccId = key.substring(
                                PREF_CARRIERS_SUBSCRIBER_PREFIX.length());
                        final String numberKey = PREF_CARRIERS_NUMBER_PREFIX + iccId;
                        mergeNumber = (String) prefs.get(numberKey);
                        if (DBG_MERGE) {
                            Rlog.d(LOG_TAG, "Found line number " + mergeNumber
                                    + " for active subscriber " + subscriberId);
                        }
                        if (!TextUtils.isEmpty(mergeNumber)) {
                            break;
                        }
                    }
                }
            }

            // Shortcut when no active merged subscribers
            if (TextUtils.isEmpty(mergeNumber)) {
                return null;
            }

            // Second pass, find all subscribers under that line override
            final ArraySet<String> result = new ArraySet<>();
            for (String key : prefs.keySet()) {
                if (key.startsWith(PREF_CARRIERS_NUMBER_PREFIX)) {
                    final String number = (String) prefs.get(key);
                    if (mergeNumber.equals(number)) {
                        final String iccId = key.substring(PREF_CARRIERS_NUMBER_PREFIX.length());
                        final String subscriberKey = PREF_CARRIERS_SUBSCRIBER_PREFIX + iccId;
                        final String subscriberId = (String) prefs.get(subscriberKey);
                        if (!TextUtils.isEmpty(subscriberId)) {
                            result.add(subscriberId);
                        }
                    }
                }
            }

            final String[] resultArray = result.toArray(new String[result.size()]);
            Arrays.sort(resultArray);
            if (DBG_MERGE) {
                Rlog.d(LOG_TAG,
                        "Found subscribers " + Arrays.toString(resultArray) + " after merge");
            }
            return resultArray;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String[] getMergedImsisFromGroup(int subId, String callingPackage) {
        enforceReadPrivilegedPermission("getMergedImsisFromGroup");

        final long identity = Binder.clearCallingIdentity();
        try {
            final TelephonyManager telephonyManager = mApp.getSystemService(
                    TelephonyManager.class);
            String subscriberId = telephonyManager.getSubscriberId(subId);
            if (subscriberId == null) {
                if (DBG) {
                    log("getMergedImsisFromGroup can't find subscriberId for subId "
                            + subId);
                }
                return null;
            }

            final SubscriptionInfo info = SubscriptionController.getInstance()
                    .getSubscriptionInfo(subId);
            final ParcelUuid groupUuid = info.getGroupUuid();
            // If it doesn't belong to any group, return just subscriberId of itself.
            if (groupUuid == null) {
                return new String[]{subscriberId};
            }

            // Get all subscriberIds from the group.
            final List<String> mergedSubscriberIds = new ArrayList<>();
            final List<SubscriptionInfo> groupInfos = SubscriptionController.getInstance()
                    .getSubscriptionsInGroup(groupUuid, mApp.getOpPackageName(),
                            mApp.getAttributionTag());
            for (SubscriptionInfo subInfo : groupInfos) {
                subscriberId = telephonyManager.getSubscriberId(subInfo.getSubscriptionId());
                if (subscriberId != null) {
                    mergedSubscriberIds.add(subscriberId);
                }
            }

            return mergedSubscriberIds.toArray(new String[mergedSubscriberIds.size()]);
        } finally {
            Binder.restoreCallingIdentity(identity);

        }
    }

    @Override
    public boolean setOperatorBrandOverride(int subId, String brand) {
        TelephonyPermissions.enforceCallingOrSelfCarrierPrivilege(mApp,
                subId, "setOperatorBrandOverride");

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            return phone == null ? false : phone.setOperatorBrandOverride(brand);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean setRoamingOverride(int subId, List<String> gsmRoamingList,
            List<String> gsmNonRoamingList, List<String> cdmaRoamingList,
            List<String> cdmaNonRoamingList) {
        TelephonyPermissions.enforceCallingOrSelfCarrierPrivilege(
                mApp, subId, "setRoamingOverride");

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone == null) {
                return false;
            }
            return phone.setRoamingOverride(gsmRoamingList, gsmNonRoamingList, cdmaRoamingList,
                    cdmaNonRoamingList);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    @Deprecated
    public int invokeOemRilRequestRaw(byte[] oemReq, byte[] oemResp) {
        enforceModifyPermission();

        int returnValue = 0;
        try {
            AsyncResult result = (AsyncResult) sendRequest(CMD_INVOKE_OEM_RIL_REQUEST_RAW, oemReq);
            if(result.exception == null) {
                if (result.result != null) {
                    byte[] responseData = (byte[])(result.result);
                    if(responseData.length > oemResp.length) {
                        Log.w(LOG_TAG, "Buffer to copy response too small: Response length is " +
                                responseData.length +  "bytes. Buffer Size is " +
                                oemResp.length + "bytes.");
                    }
                    System.arraycopy(responseData, 0, oemResp, 0, responseData.length);
                    returnValue = responseData.length;
                }
            } else {
                CommandException ex = (CommandException) result.exception;
                returnValue = ex.getCommandError().ordinal();
                if(returnValue > 0) returnValue *= -1;
            }
        } catch (RuntimeException e) {
            Log.w(LOG_TAG, "sendOemRilRequestRaw: Runtime Exception");
            returnValue = (CommandException.Error.GENERIC_FAILURE.ordinal());
            if(returnValue > 0) returnValue *= -1;
        }

        return returnValue;
    }

    @Override
    public void setRadioCapability(RadioAccessFamily[] rafs) {
        try {
            ProxyController.getInstance().setRadioCapability(rafs);
        } catch (RuntimeException e) {
            Log.w(LOG_TAG, "setRadioCapability: Runtime Exception");
        }
    }

    @Override
    public int getRadioAccessFamily(int phoneId, String callingPackage) {
        Phone phone = PhoneFactory.getPhone(phoneId);
        try {
            TelephonyPermissions
                    .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                            mApp, phone.getSubId(), "getRadioAccessFamily");
        } catch (SecurityException e) {
            EventLog.writeEvent(0x534e4554, "150857259", -1, "Missing Permission");
            throw e;
        }
        int raf = RadioAccessFamily.RAF_UNKNOWN;
        if (phone == null) {
            return raf;
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            TelephonyPermissions
                    .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                            mApp, phone.getSubId(), "getRadioAccessFamily");
            raf = ProxyController.getInstance().getRadioAccessFamily(phoneId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return raf;
    }

    @Override
    public void enableVideoCalling(boolean enable) {
        final Phone defaultPhone = getDefaultPhone();
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            ImsManager.getInstance(defaultPhone.getContext(),
                    defaultPhone.getPhoneId()).setVtSetting(enable);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isVideoCallingEnabled(String callingPackage, String callingFeatureId) {
        final Phone defaultPhone = getDefaultPhone();
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(mApp, defaultPhone.getSubId(),
                callingPackage, callingFeatureId, "isVideoCallingEnabled")) {
            return false;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            // Check the user preference and the  system-level IMS setting. Even if the user has
            // enabled video calling, if IMS is disabled we aren't able to support video calling.
            // In the long run, we may instead need to check if there exists a connection service
            // which can support video calling.
            ImsManager imsManager =
                    ImsManager.getInstance(defaultPhone.getContext(), defaultPhone.getPhoneId());
            return imsManager.isVtEnabledByPlatform()
                    && imsManager.isEnhanced4gLteModeSettingEnabledByUser()
                    && imsManager.isVtEnabledByUser();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean canChangeDtmfToneLength(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "isVideoCallingEnabled")) {
            return false;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            CarrierConfigManager configManager =
                    (CarrierConfigManager) mApp.getSystemService(Context.CARRIER_CONFIG_SERVICE);
            return configManager.getConfigForSubId(subId)
                    .getBoolean(CarrierConfigManager.KEY_DTMF_TYPE_ENABLED_BOOL);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isWorldPhone(int subId, String callingPackage, String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId, "isVideoCallingEnabled")) {
            return false;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            CarrierConfigManager configManager =
                    (CarrierConfigManager) mApp.getSystemService(Context.CARRIER_CONFIG_SERVICE);
            return configManager.getConfigForSubId(subId)
                    .getBoolean(CarrierConfigManager.KEY_WORLD_PHONE_BOOL);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isTtyModeSupported() {
        TelecomManager telecomManager = mApp.getSystemService(TelecomManager.class);
        return telecomManager.isTtySupported();
    }

    @Override
    public boolean isHearingAidCompatibilitySupported() {
        final long identity = Binder.clearCallingIdentity();
        try {
            return mApp.getResources().getBoolean(R.bool.hac_enabled);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Determines whether the device currently supports RTT (Real-time text). Based both on carrier
     * support for the feature and device firmware support.
     *
     * @return {@code true} if the device and carrier both support RTT, {@code false} otherwise.
     */
    @Override
    public boolean isRttSupported(int subscriptionId) {
        final long identity = Binder.clearCallingIdentity();
        final Phone phone = getPhone(subscriptionId);
        if (phone == null) {
            loge("isRttSupported: no Phone found. Invalid subId:" + subscriptionId);
            return false;
        }
        try {
            boolean isCarrierSupported = mApp.getCarrierConfigForSubId(subscriptionId).getBoolean(
                    CarrierConfigManager.KEY_RTT_SUPPORTED_BOOL);
            boolean isDeviceSupported =
                    phone.getContext().getResources().getBoolean(R.bool.config_support_rtt);
            return isCarrierSupported && isDeviceSupported;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Determines whether the user has turned on RTT. If the carrier wants to ignore the user-set
     * RTT setting, will return true if the device and carrier both support RTT.
     * Otherwise. only returns true if the device and carrier both also support RTT.
     */
    public boolean isRttEnabled(int subscriptionId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            boolean isRttSupported = isRttSupported(subscriptionId);
            boolean isUserRttSettingOn = Settings.Secure.getInt(
                    mApp.getContentResolver(), Settings.Secure.RTT_CALLING_MODE, 0) != 0;
            boolean shouldIgnoreUserRttSetting = mApp.getCarrierConfigForSubId(subscriptionId)
                    .getBoolean(CarrierConfigManager.KEY_IGNORE_RTT_MODE_SETTING_BOOL);
            return isRttSupported && (isUserRttSettingOn || shouldIgnoreUserRttSetting);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Deprecated
    @Override
    public String getDeviceId(String callingPackage) {
        return getDeviceIdWithFeature(callingPackage, null);
    }

    /**
     * Returns the unique device ID of phone, for example, the IMEI for
     * GSM and the MEID for CDMA phones. Return null if device ID is not available.
     *
     * <p>Requires Permission:
     *   {@link android.Manifest.permission#READ_PHONE_STATE READ_PHONE_STATE}
     */
    @Override
    public String getDeviceIdWithFeature(String callingPackage, String callingFeatureId) {
        final Phone phone = PhoneFactory.getPhone(0);
        if (phone == null) {
            return null;
        }
        int subId = phone.getSubId();
        if (!TelephonyPermissions.checkCallingOrSelfReadDeviceIdentifiers(mApp, subId,
                callingPackage, callingFeatureId, "getDeviceId")) {
            return null;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return phone.getDeviceId();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * {@hide}
     * Returns the IMS Registration Status on a particular subid
     *
     * @param subId
     */
    public boolean isImsRegistered(int subId) {
        Phone phone = getPhone(subId);
        if (phone != null) {
            return phone.isImsRegistered();
        } else {
            return false;
        }
    }

    @Override
    public int getSubIdForPhoneAccount(PhoneAccount phoneAccount) {
        final long identity = Binder.clearCallingIdentity();
        try {
            return PhoneUtils.getSubIdForPhoneAccount(phoneAccount);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getSubIdForPhoneAccountHandle(
            PhoneAccountHandle phoneAccountHandle, String callingPackage, String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(mApp, getDefaultSubscription(),
                callingPackage, callingFeatureId, "getSubIdForPhoneAccountHandle")) {
            throw new SecurityException("Requires READ_PHONE_STATE permission.");
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            return PhoneUtils.getSubIdForPhoneAccountHandle(phoneAccountHandle);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public @Nullable PhoneAccountHandle getPhoneAccountHandleForSubscriptionId(int subscriptionId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subscriptionId);
            if (phone == null) {
                return null;
            }
            return PhoneUtils.makePstnPhoneAccountHandle(phone);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * @return the VoWiFi calling availability.
     */
    public boolean isWifiCallingAvailable(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.isWifiCallingEnabled();
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * @return the VT calling availability.
     */
    public boolean isVideoTelephonyAvailable(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.isVideoEnabled();
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * @return the IMS registration technology for the MMTEL feature. Valid return values are
     * defined in {@link ImsRegistrationImplBase}.
     */
    public @ImsRegistrationImplBase.ImsRegistrationTech int getImsRegTechnologyForMmTel(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone != null) {
                return phone.getImsRegistrationTech();
            } else {
                return ImsRegistrationImplBase.REGISTRATION_TECH_NONE;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void factoryReset(int subId) {
        enforceSettingsPermission();
        if (mUserManager.hasUserRestriction(UserManager.DISALLOW_NETWORK_RESET)) {
            return;
        }

        final long identity = Binder.clearCallingIdentity();

        try {
            if (SubscriptionManager.isUsableSubIdValue(subId) && !mUserManager.hasUserRestriction(
                    UserManager.DISALLOW_CONFIG_MOBILE_NETWORKS)) {
                setUserDataEnabled(subId, getDefaultDataEnabled());
                setNetworkSelectionModeAutomatic(subId);
                setPreferredNetworkType(subId, getDefaultNetworkType(subId));
                setDataRoamingEnabled(subId, getDefaultDataRoamingEnabled(subId));
                CarrierInfoManager.deleteAllCarrierKeysForImsiEncryption(mApp);
            }
            // There has been issues when Sms raw table somehow stores orphan
            // fragments. They lead to garbled message when new fragments come
            // in and combined with those stale ones. In case this happens again,
            // user can reset all network settings which will clean up this table.
            cleanUpSmsRawTable(getDefaultPhone().getContext());
            // Clean up IMS settings as well here.
            int slotId = getSlotIndex(subId);
            if (slotId > SubscriptionManager.INVALID_SIM_SLOT_INDEX) {
                ImsManager.getInstance(mApp, slotId).factoryReset();
            }

            // Erase modem config if erase modem on network setting is enabled.
            String configValue = DeviceConfig.getProperty(DeviceConfig.NAMESPACE_TELEPHONY,
                    RESET_NETWORK_ERASE_MODEM_CONFIG_ENABLED);
            if (configValue != null && Boolean.parseBoolean(configValue)) {
              sendEraseModemConfig(getDefaultPhone());
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private void cleanUpSmsRawTable(Context context) {
        ContentResolver resolver = context.getContentResolver();
        Uri uri = Uri.withAppendedPath(Telephony.Sms.CONTENT_URI, "raw/permanentDelete");
        resolver.delete(uri, null, null);
    }

    @Override
    public String getSimLocaleForSubscriber(int subId) {
        enforceReadPrivilegedPermission("getSimLocaleForSubscriber, subId: " + subId);
        final Phone phone = getPhone(subId);
        if (phone == null) {
            log("getSimLocaleForSubscriber, invalid subId");
            return null;
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            final SubscriptionInfo info = mSubscriptionController.getActiveSubscriptionInfo(subId,
                    phone.getContext().getOpPackageName(), phone.getContext().getAttributionTag());
            if (info == null) {
                log("getSimLocaleForSubscriber, inactive subId: " + subId);
                return null;
            }
            // Try and fetch the locale from the carrier properties or from the SIM language
            // preferences (EF-PL and EF-LI)...
            final int mcc = info.getMcc();
            String simLanguage = null;
            final Locale localeFromDefaultSim = phone.getLocaleFromSimAndCarrierPrefs();
            if (localeFromDefaultSim != null) {
                if (!localeFromDefaultSim.getCountry().isEmpty()) {
                    if (DBG) log("Using locale from subId: " + subId + " locale: "
                            + localeFromDefaultSim);
                    return localeFromDefaultSim.toLanguageTag();
                } else {
                    simLanguage = localeFromDefaultSim.getLanguage();
                }
            }

            // The SIM language preferences only store a language (e.g. fr = French), not an
            // exact locale (e.g. fr_FR = French/France). So, if the locale returned from
            // the SIM and carrier preferences does not include a country we add the country
            // determined from the SIM MCC to provide an exact locale.
            final Locale mccLocale = LocaleUtils.getLocaleFromMcc(mApp, mcc, simLanguage);
            if (mccLocale != null) {
                if (DBG) log("No locale from SIM, using mcc locale:" + mccLocale);
                return mccLocale.toLanguageTag();
            }

            if (DBG) log("No locale found - returning null");
            return null;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private List<SubscriptionInfo> getAllSubscriptionInfoList() {
        return mSubscriptionController.getAllSubInfoList(mApp.getOpPackageName(),
                mApp.getAttributionTag());
    }

    /**
     * NOTE: this method assumes permission checks are done and caller identity has been cleared.
     */
    private List<SubscriptionInfo> getActiveSubscriptionInfoListPrivileged() {
        return mSubscriptionController.getActiveSubscriptionInfoList(mApp.getOpPackageName(),
                mApp.getAttributionTag());
    }

    private final ModemActivityInfo mLastModemActivityInfo =
            new ModemActivityInfo(0, 0, 0, new int[0], 0);

    /**
     * Responds to the ResultReceiver with the {@link android.telephony.ModemActivityInfo} object
     * representing the state of the modem.
     *
     * NOTE: The underlying implementation clears the modem state, so there should only ever be one
     * caller to it. Everyone should call this class to get cumulative data.
     * @hide
     */
    @Override
    public void requestModemActivityInfo(ResultReceiver result) {
        enforceModifyPermission();
        WorkSource workSource = getWorkSource(Binder.getCallingUid());

        final long identity = Binder.clearCallingIdentity();
        try {
            sendRequestAsync(CMD_GET_MODEM_ACTIVITY_INFO, result, null, workSource);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    // Checks that ModemActivityInfo is valid. Sleep time, Idle time, Rx time and Tx time should be
    // less than total activity duration.
    private boolean isModemActivityInfoValid(ModemActivityInfo info) {
        if (info == null) {
            return false;
        }
        int activityDurationMs =
            (int) (info.getTimestamp() - mLastModemActivityInfo.getTimestamp());
        int totalTxTimeMs = 0;
        int[] txTimeMs = info.getTransmitTimeMillis();
        for (int i = 0; i < info.getTransmitPowerInfo().size(); i++) {
            totalTxTimeMs += txTimeMs[i];
        }
        return (info.isValid()
            && (info.getSleepTimeMillis() <= activityDurationMs)
            && (info.getIdleTimeMillis() <= activityDurationMs)
            && (info.getReceiveTimeMillis() <= activityDurationMs)
            && (totalTxTimeMs <= activityDurationMs));
    }

    /**
     * {@hide}
     * Returns the service state information on specified subscription.
     */
    @Override
    public ServiceState getServiceStateForSubscriber(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId, "getServiceStateForSubscriber")) {
            return null;
        }

        LocationAccessPolicy.LocationPermissionResult fineLocationResult =
                LocationAccessPolicy.checkLocationPermission(mApp,
                        new LocationAccessPolicy.LocationPermissionQuery.Builder()
                                .setCallingPackage(callingPackage)
                                .setCallingFeatureId(callingFeatureId)
                                .setCallingPid(Binder.getCallingPid())
                                .setCallingUid(Binder.getCallingUid())
                                .setMethod("getServiceStateForSubscriber")
                                .setLogAsInfo(true)
                                .setMinSdkVersionForFine(Build.VERSION_CODES.Q)
                                .build());

        LocationAccessPolicy.LocationPermissionResult coarseLocationResult =
                LocationAccessPolicy.checkLocationPermission(mApp,
                        new LocationAccessPolicy.LocationPermissionQuery.Builder()
                                .setCallingPackage(callingPackage)
                                .setCallingFeatureId(callingFeatureId)
                                .setCallingPid(Binder.getCallingPid())
                                .setCallingUid(Binder.getCallingUid())
                                .setMethod("getServiceStateForSubscriber")
                                .setLogAsInfo(true)
                                .setMinSdkVersionForCoarse(Build.VERSION_CODES.Q)
                                .build());
        // We don't care about hard or soft here -- all we need to know is how much info to scrub.
        boolean hasFinePermission =
                fineLocationResult == LocationAccessPolicy.LocationPermissionResult.ALLOWED;
        boolean hasCoarsePermission =
                coarseLocationResult == LocationAccessPolicy.LocationPermissionResult.ALLOWED;

        final long identity = Binder.clearCallingIdentity();
        try {
            // isActiveSubId requires READ_PHONE_STATE, which we already check for above
            if (!mSubscriptionController.isActiveSubId(subId, callingPackage, callingFeatureId)) {
                Rlog.d(LOG_TAG,
                        "getServiceStateForSubscriber returning null for inactive subId=" + subId);
                return null;
            }

            final Phone phone = getPhone(subId);
            if (phone == null) {
                return null;
            }

            ServiceState ss = phone.getServiceState();

            // Scrub out the location info in ServiceState depending on what level of access
            // the caller has.
            if (hasFinePermission) return ss;
            if (hasCoarsePermission) return ss.createLocationInfoSanitizedCopy(false);
            return ss.createLocationInfoSanitizedCopy(true);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns the URI for the per-account voicemail ringtone set in Phone settings.
     *
     * @param accountHandle The handle for the {@link PhoneAccount} for which to retrieve the
     * voicemail ringtone.
     * @return The URI for the ringtone to play when receiving a voicemail from a specific
     * PhoneAccount.
     */
    @Override
    public Uri getVoicemailRingtoneUri(PhoneAccountHandle accountHandle) {
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = PhoneUtils.getPhoneForPhoneAccountHandle(accountHandle);
            if (phone == null) {
                phone = getDefaultPhone();
            }

            return VoicemailNotificationSettingsUtil.getRingtoneUri(phone.getContext());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Sets the per-account voicemail ringtone.
     *
     * <p>Requires that the calling app is the default dialer, or has carrier privileges, or
     * has permission {@link android.Manifest.permission#MODIFY_PHONE_STATE MODIFY_PHONE_STATE}.
     *
     * @param phoneAccountHandle The handle for the {@link PhoneAccount} for which to set the
     * voicemail ringtone.
     * @param uri The URI for the ringtone to play when receiving a voicemail from a specific
     * PhoneAccount.
     */
    @Override
    public void setVoicemailRingtoneUri(String callingPackage,
            PhoneAccountHandle phoneAccountHandle, Uri uri) {
        final Phone defaultPhone = getDefaultPhone();
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        TelecomManager tm = defaultPhone.getContext().getSystemService(TelecomManager.class);
        if (!TextUtils.equals(callingPackage, tm.getDefaultDialerPackage())) {
            TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                    mApp, PhoneUtils.getSubIdForPhoneAccountHandle(phoneAccountHandle),
                    "setVoicemailRingtoneUri");
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = PhoneUtils.getPhoneForPhoneAccountHandle(phoneAccountHandle);
            if (phone == null) {
                phone = defaultPhone;
            }
            VoicemailNotificationSettingsUtil.setRingtoneUri(phone.getContext(), uri);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns whether vibration is set for voicemail notification in Phone settings.
     *
     * @param accountHandle The handle for the {@link PhoneAccount} for which to retrieve the
     * voicemail vibration setting.
     * @return {@code true} if the vibration is set for this PhoneAccount, {@code false} otherwise.
     */
    @Override
    public boolean isVoicemailVibrationEnabled(PhoneAccountHandle accountHandle) {
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = PhoneUtils.getPhoneForPhoneAccountHandle(accountHandle);
            if (phone == null) {
                phone = getDefaultPhone();
            }

            return VoicemailNotificationSettingsUtil.isVibrationEnabled(phone.getContext());
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Sets the per-account voicemail vibration.
     *
     * <p>Requires that the calling app is the default dialer, or has carrier privileges, or
     * has permission {@link android.Manifest.permission#MODIFY_PHONE_STATE MODIFY_PHONE_STATE}.
     *
     * @param phoneAccountHandle The handle for the {@link PhoneAccount} for which to set the
     * voicemail vibration setting.
     * @param enabled Whether to enable or disable vibration for voicemail notifications from a
     * specific PhoneAccount.
     */
    @Override
    public void setVoicemailVibrationEnabled(String callingPackage,
            PhoneAccountHandle phoneAccountHandle, boolean enabled) {
        final Phone defaultPhone = getDefaultPhone();
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        TelecomManager tm = defaultPhone.getContext().getSystemService(TelecomManager.class);
        if (!TextUtils.equals(callingPackage, tm.getDefaultDialerPackage())) {
            TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                    mApp, PhoneUtils.getSubIdForPhoneAccountHandle(phoneAccountHandle),
                    "setVoicemailVibrationEnabled");
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = PhoneUtils.getPhoneForPhoneAccountHandle(phoneAccountHandle);
            if (phone == null) {
                phone = defaultPhone;
            }
            VoicemailNotificationSettingsUtil.setVibrationEnabled(phone.getContext(), enabled);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Make sure either called from same process as self (phone) or IPC caller has read privilege.
     *
     * @throws SecurityException if the caller does not have the required permission
     */
    private void enforceReadPrivilegedPermission(String message) {
        mApp.enforceCallingOrSelfPermission(android.Manifest.permission.READ_PRIVILEGED_PHONE_STATE,
                message);
    }

    /**
     * Make sure either called from same process as self (phone) or IPC caller has send SMS
     * permission.
     *
     * @throws SecurityException if the caller does not have the required permission
     */
    private void enforceSendSmsPermission() {
        mApp.enforceCallingOrSelfPermission(permission.SEND_SMS, null);
    }

    /**
     * Make sure called from the package in charge of visual voicemail.
     *
     * @throws SecurityException if the caller is not the visual voicemail package.
     */
    private void enforceVisualVoicemailPackage(String callingPackage, int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            ComponentName componentName =
                    RemoteVvmTaskManager.getRemotePackage(mApp, subId);
            if (componentName == null) {
                throw new SecurityException(
                        "Caller not current active visual voicemail package[null]");
            }
            String vvmPackage = componentName.getPackageName();
            if (!callingPackage.equals(vvmPackage)) {
                throw new SecurityException("Caller not current active visual voicemail package["
                        + vvmPackage + "]");
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Return the application ID for the app type.
     *
     * @param subId the subscription ID that this request applies to.
     * @param appType the uicc app type.
     * @return Application ID for specificied app type, or null if no uicc.
     */
    @Override
    public String getAidForAppType(int subId, int appType) {
        enforceReadPrivilegedPermission("getAidForAppType");
        Phone phone = getPhone(subId);

        final long identity = Binder.clearCallingIdentity();
        try {
            if (phone == null) {
                return null;
            }
            String aid = null;
            try {
                aid = UiccController.getInstance().getUiccCard(phone.getPhoneId())
                        .getApplicationByType(appType).getAid();
            } catch (Exception e) {
                Log.e(LOG_TAG, "Not getting aid. Exception ex=" + e);
            }
            return aid;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Return the Electronic Serial Number.
     *
     * @param subId the subscription ID that this request applies to.
     * @return ESN or null if error.
     */
    @Override
    public String getEsn(int subId) {
        enforceReadPrivilegedPermission("getEsn");
        Phone phone = getPhone(subId);

        final long identity = Binder.clearCallingIdentity();
        try {
            if (phone == null) {
                return null;
            }
            String esn = null;
            try {
                esn = phone.getEsn();
            } catch (Exception e) {
                Log.e(LOG_TAG, "Not getting ESN. Exception ex=" + e);
            }
            return esn;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Return the Preferred Roaming List Version.
     *
     * @param subId the subscription ID that this request applies to.
     * @return PRLVersion or null if error.
     */
    @Override
    public String getCdmaPrlVersion(int subId) {
        enforceReadPrivilegedPermission("getCdmaPrlVersion");
        Phone phone = getPhone(subId);

        final long identity = Binder.clearCallingIdentity();
        try {
            if (phone == null) {
                return null;
            }
            String cdmaPrlVersion = null;
            try {
                cdmaPrlVersion = phone.getCdmaPrlVersion();
            } catch (Exception e) {
                Log.e(LOG_TAG, "Not getting PRLVersion", e);
            }
            return cdmaPrlVersion;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get snapshot of Telephony histograms
     * @return List of Telephony histograms
     * @hide
     */
    @Override
    public List<TelephonyHistogram> getTelephonyHistograms() {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, getDefaultSubscription(), "getTelephonyHistograms");

        final long identity = Binder.clearCallingIdentity();
        try {
            return RIL.getTelephonyRILTimingHistograms();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * {@hide}
     * Set the allowed carrier list and the excluded carrier list, indicating the priority between
     * the two lists.
     * Require system privileges. In the future we may add this to carrier APIs.
     *
     * @return Integer with the result of the operation, as defined in {@link TelephonyManager}.
     */
    @Override
    @TelephonyManager.SetCarrierRestrictionResult
    public int setAllowedCarriers(CarrierRestrictionRules carrierRestrictionRules) {
        enforceModifyPermission();
        WorkSource workSource = getWorkSource(Binder.getCallingUid());

        if (carrierRestrictionRules == null) {
            throw new NullPointerException("carrier restriction cannot be null");
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return (int) sendRequest(CMD_SET_ALLOWED_CARRIERS, carrierRestrictionRules,
                    workSource);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * {@hide}
     * Get the allowed carrier list and the excluded carrier list, including the priority between
     * the two lists.
     * Require system privileges. In the future we may add this to carrier APIs.
     *
     * @return {@link android.telephony.CarrierRestrictionRules}
     */
    @Override
    public CarrierRestrictionRules getAllowedCarriers() {
        enforceReadPrivilegedPermission("getAllowedCarriers");
        WorkSource workSource = getWorkSource(Binder.getCallingUid());

        final long identity = Binder.clearCallingIdentity();
        try {
            Object response = sendRequest(CMD_GET_ALLOWED_CARRIERS, null, workSource);
            if (response instanceof CarrierRestrictionRules) {
                return (CarrierRestrictionRules) response;
            }
            // Response is an Exception of some kind,
            // which is signalled to the user as a NULL retval
            return null;
        } catch (Exception e) {
            Log.e(LOG_TAG, "getAllowedCarriers. Exception ex=" + e);
            return null;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Action set from carrier signalling broadcast receivers to enable/disable metered apns
     * @param subId the subscription ID that this action applies to.
     * @param enabled control enable or disable metered apns.
     * {@hide}
     */
    @Override
    public void carrierActionSetMeteredApnsEnabled(int subId, boolean enabled) {
        enforceModifyPermission();
        final Phone phone = getPhone(subId);

        final long identity = Binder.clearCallingIdentity();
        if (phone == null) {
            loge("carrierAction: SetMeteredApnsEnabled fails with invalid subId: " + subId);
            return;
        }
        try {
            phone.carrierActionSetMeteredApnsEnabled(enabled);
        } catch (Exception e) {
            Log.e(LOG_TAG, "carrierAction: SetMeteredApnsEnabled fails. Exception ex=" + e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Action set from carrier signalling broadcast receivers to enable/disable radio
     * @param subId the subscription ID that this action applies to.
     * @param enabled control enable or disable radio.
     * {@hide}
     */
    @Override
    public void carrierActionSetRadioEnabled(int subId, boolean enabled) {
        enforceModifyPermission();
        final Phone phone = getPhone(subId);

        final long identity = Binder.clearCallingIdentity();
        if (phone == null) {
            loge("carrierAction: SetRadioEnabled fails with invalid sibId: " + subId);
            return;
        }
        try {
            phone.carrierActionSetRadioEnabled(enabled);
        } catch (Exception e) {
            Log.e(LOG_TAG, "carrierAction: SetRadioEnabled fails. Exception ex=" + e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Action set from carrier signalling broadcast receivers to start/stop reporting the default
     * network status based on which carrier apps could apply actions accordingly,
     * enable/disable default url handler for example.
     *
     * @param subId the subscription ID that this action applies to.
     * @param report control start/stop reporting the default network status.
     * {@hide}
     */
    @Override
    public void carrierActionReportDefaultNetworkStatus(int subId, boolean report) {
        enforceModifyPermission();
        final Phone phone = getPhone(subId);

        final long identity = Binder.clearCallingIdentity();
        if (phone == null) {
            loge("carrierAction: ReportDefaultNetworkStatus fails with invalid sibId: " + subId);
            return;
        }
        try {
            phone.carrierActionReportDefaultNetworkStatus(report);
        } catch (Exception e) {
            Log.e(LOG_TAG, "carrierAction: ReportDefaultNetworkStatus fails. Exception ex=" + e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Action set from carrier signalling broadcast receivers to reset all carrier actions
     * @param subId the subscription ID that this action applies to.
     * {@hide}
     */
    @Override
    public void carrierActionResetAll(int subId) {
        enforceModifyPermission();
        final Phone phone = getPhone(subId);
        if (phone == null) {
            loge("carrierAction: ResetAll fails with invalid sibId: " + subId);
            return;
        }
        try {
            phone.carrierActionResetAll();
        } catch (Exception e) {
            Log.e(LOG_TAG, "carrierAction: ResetAll fails. Exception ex=" + e);
        }
    }

    /**
     * Called when "adb shell dumpsys phone" is invoked. Dump is also automatically invoked when a
     * bug report is being generated.
     */
    @Override
    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        if (mApp.checkCallingOrSelfPermission(android.Manifest.permission.DUMP)
                != PackageManager.PERMISSION_GRANTED) {
            writer.println("Permission Denial: can't dump Phone from pid="
                    + Binder.getCallingPid()
                    + ", uid=" + Binder.getCallingUid()
                    + "without permission "
                    + android.Manifest.permission.DUMP);
            return;
        }
        DumpsysHandler.dump(mApp, fd, writer, args);
    }

    @Override
    public int handleShellCommand(@NonNull ParcelFileDescriptor in,
            @NonNull ParcelFileDescriptor out, @NonNull ParcelFileDescriptor err,
            @NonNull String[] args) {
        return new TelephonyShellCommand(this, getDefaultPhone().getContext()).exec(
                this, in.getFileDescriptor(), out.getFileDescriptor(),
                        err.getFileDescriptor(), args);
    }

    /**
     * Policy control of data connection. Usually used when data limit is passed.
     * @param enabled True if enabling the data, otherwise disabling.
     * @param subId Subscription index
     * {@hide}
     */
    @Override
    public void setPolicyDataEnabled(boolean enabled, int subId) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone != null) {
                phone.getDataEnabledSettings().setPolicyDataEnabled(enabled);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get Client request stats
     * @return List of Client Request Stats
     * @hide
     */
    @Override
    public List<ClientRequestStats> getClientRequestStats(String callingPackage,
            String callingFeatureId, int subId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId, "getClientRequestStats")) {
            return null;
        }
        Phone phone = getPhone(subId);

        final long identity = Binder.clearCallingIdentity();
        try {
            if (phone != null) {
                return phone.getClientRequestStats();
            }

            return null;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private WorkSource getWorkSource(int uid) {
        String packageName = mApp.getPackageManager().getNameForUid(uid);
        return new WorkSource(uid, packageName);
    }

    /**
     * Set SIM card power state.
     *
     * @param slotIndex SIM slot id.
     * @param state  State of SIM (power down, power up, pass through)
     * - {@link android.telephony.TelephonyManager#CARD_POWER_DOWN}
     * - {@link android.telephony.TelephonyManager#CARD_POWER_UP}
     * - {@link android.telephony.TelephonyManager#CARD_POWER_UP_PASS_THROUGH}
     *
     **/
    @Override
    public void setSimPowerStateForSlot(int slotIndex, int state) {
        enforceModifyPermission();
        Phone phone = PhoneFactory.getPhone(slotIndex);

        WorkSource workSource = getWorkSource(Binder.getCallingUid());

        final long identity = Binder.clearCallingIdentity();
        try {
            if (phone != null) {
                phone.setSimPowerState(state, workSource);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private boolean isUssdApiAllowed(int subId) {
        CarrierConfigManager configManager =
                (CarrierConfigManager) mApp.getSystemService(Context.CARRIER_CONFIG_SERVICE);
        if (configManager == null) {
            return false;
        }
        PersistableBundle pb = configManager.getConfigForSubId(subId);
        if (pb == null) {
            return false;
        }
        return pb.getBoolean(
                CarrierConfigManager.KEY_ALLOW_USSD_REQUESTS_VIA_TELEPHONY_MANAGER_BOOL);
    }

    /**
     * Check if phone is in emergency callback mode
     * @return true if phone is in emergency callback mode
     * @param subId sub id
     */
    @Override
    public boolean getEmergencyCallbackMode(int subId) {
        enforceReadPrivilegedPermission("getEmergencyCallbackMode");
        final Phone phone = getPhone(subId);

        final long identity = Binder.clearCallingIdentity();
        try {
            if (phone != null) {
                return phone.isInEcm();
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the current signal strength information for the given subscription.
     * Because this information is not updated when the device is in a low power state
     * it should not be relied-upon to be current.
     * @param subId Subscription index
     * @return the most recent cached signal strength info from the modem
     */
    @Override
    public SignalStrength getSignalStrength(int subId) {
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone p = getPhone(subId);
            if (p == null) {
                return null;
            }

            return p.getSignalStrength();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the current modem radio state for the given slot.
     * @param slotIndex slot index.
     * @param callingPackage the name of the package making the call.
     * @param callingFeatureId The feature in the package.
     * @return the current radio power state from the modem
     */
    @Override
    public int getRadioPowerState(int slotIndex, String callingPackage, String callingFeatureId) {
        Phone phone = PhoneFactory.getPhone(slotIndex);
        if (phone != null) {
            if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(mApp, phone.getSubId(),
                    callingPackage, callingFeatureId, "getRadioPowerState")) {
                return TelephonyManager.RADIO_POWER_UNAVAILABLE;
            }

            final long identity = Binder.clearCallingIdentity();
            try {
                return phone.getRadioPowerState();
            } finally {
                Binder.restoreCallingIdentity(identity);
            }
        }
        return TelephonyManager.RADIO_POWER_UNAVAILABLE;
    }

    /**
     * Checks if data roaming is enabled on the subscription with id {@code subId}.
     *
     * <p>Requires one of the following permissions:
     * {@link android.Manifest.permission#ACCESS_NETWORK_STATE},
     * {@link android.Manifest.permission#READ_PHONE_STATE} or that the calling app has carrier
     * privileges.
     *
     * @param subId subscription id
     * @return {@code true} if data roaming is enabled on this subscription, otherwise return
     * {@code false}.
     */
    @Override
    public boolean isDataRoamingEnabled(int subId) {
        mApp.enforceCallingOrSelfPermission(android.Manifest.permission.ACCESS_NETWORK_STATE,
                null /* message */);

        boolean isEnabled = false;
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            isEnabled =  phone != null ? phone.getDataRoamingEnabled() : false;
        } catch (Exception e) {
            TelephonyPermissions.enforeceCallingOrSelfReadPhoneStatePermissionOrCarrierPrivilege(
                    mApp, subId, "isDataRoamingEnabled");
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return isEnabled;
    }


    /**
     * Enables/Disables the data roaming on the subscription with id {@code subId}.
     *
     * <p> Requires permission:
     * {@link android.Manifest.permission#MODIFY_PHONE_STATE} or that the calling app has carrier
     * privileges.
     *
     * @param subId subscription id
     * @param isEnabled {@code true} means enable, {@code false} means disable.
     */
    @Override
    public void setDataRoamingEnabled(int subId, boolean isEnabled) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setDataRoamingEnabled");

        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone != null) {
                phone.setDataRoamingEnabled(isEnabled);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isManualNetworkSelectionAllowed(int subId) {
        TelephonyPermissions
                .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "isManualNetworkSelectionAllowed");

        boolean isAllowed = true;
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone != null) {
                isAllowed = phone.isCspPlmnEnabled();
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return isAllowed;
    }

    @Override
    public List<UiccCardInfo> getUiccCardsInfo(String callingPackage) {
        // Verify that tha callingPackage belongs to the calling UID
        mApp.getSystemService(AppOpsManager.class)
                .checkPackage(Binder.getCallingUid(), callingPackage);

        boolean hasReadPermission = false;
        try {
            enforceReadPrivilegedPermission("getUiccCardsInfo");
            hasReadPermission = true;
        } catch (SecurityException e) {
            // even without READ_PRIVILEGED_PHONE_STATE, we allow the call to continue if the caller
            // has carrier privileges on an active UICC
            if (checkCarrierPrivilegesForPackageAnyPhone(callingPackage)
                        != TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS) {
                throw new SecurityException("Caller does not have permission.");
            }
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            UiccController uiccController = UiccController.getInstance();
            ArrayList<UiccCardInfo> cardInfos = uiccController.getAllUiccCardInfos();
            if (hasReadPermission) {
                return cardInfos;
            }

            // Remove private info if the caller doesn't have access
            ArrayList<UiccCardInfo> filteredInfos = new ArrayList<>();
            for (UiccCardInfo cardInfo : cardInfos) {
                // For an inactive eUICC, the UiccCard will be null even though the UiccCardInfo
                // is available
                UiccCard card = uiccController.getUiccCardForSlot(cardInfo.getSlotIndex());
                if (card == null || card.getUiccProfile() == null) {
                    // assume no access if the card or profile is unavailable
                    filteredInfos.add(cardInfo.getUnprivileged());
                    continue;
                }
                UiccProfile profile = card.getUiccProfile();
                if (profile.getCarrierPrivilegeStatus(mApp.getPackageManager(), callingPackage)
                        == TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS) {
                    filteredInfos.add(cardInfo);
                } else {
                    filteredInfos.add(cardInfo.getUnprivileged());
                }
            }
            return filteredInfos;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public UiccSlotInfo[] getUiccSlotsInfo() {
        enforceReadPrivilegedPermission("getUiccSlotsInfo");

        final long identity = Binder.clearCallingIdentity();
        try {
            UiccSlot[] slots = UiccController.getInstance().getUiccSlots();
            if (slots == null) {
                Rlog.i(LOG_TAG, "slots is null.");
                return null;
            }

            UiccSlotInfo[] infos = new UiccSlotInfo[slots.length];
            for (int i = 0; i < slots.length; i++) {
                UiccSlot slot = slots[i];
                if (slot == null) {
                    continue;
                }

                String cardId;
                UiccCard card = slot.getUiccCard();
                if (card != null) {
                    cardId = card.getCardId();
                } else {
                    cardId = slot.getEid();
                    if (TextUtils.isEmpty(cardId)) {
                        cardId = slot.getIccId();
                    }
                }

                if (cardId != null) {
                    // if cardId is an ICCID, strip off trailing Fs before exposing to user
                    // if cardId is an EID, it's all digits so this is fine
                    cardId = IccUtils.stripTrailingFs(cardId);
                }

                int cardState = 0;
                switch (slot.getCardState()) {
                    case CARDSTATE_ABSENT:
                        cardState = UiccSlotInfo.CARD_STATE_INFO_ABSENT;
                        break;
                    case CARDSTATE_PRESENT:
                        cardState = UiccSlotInfo.CARD_STATE_INFO_PRESENT;
                        break;
                    case CARDSTATE_ERROR:
                        cardState = UiccSlotInfo.CARD_STATE_INFO_ERROR;
                        break;
                    case CARDSTATE_RESTRICTED:
                        cardState = UiccSlotInfo.CARD_STATE_INFO_RESTRICTED;
                        break;
                    default:
                        break;

                }

                infos[i] = new UiccSlotInfo(
                        slot.isActive(),
                        slot.isEuicc(),
                        cardId,
                        cardState,
                        slot.getPhoneId(),
                        slot.isExtendedApduSupported(),
                        slot.isRemovable());
            }
            return infos;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean switchSlots(int[] physicalSlots) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            return (Boolean) sendRequest(CMD_SWITCH_SLOTS, physicalSlots);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getCardIdForDefaultEuicc(int subId, String callingPackage) {
        final long identity = Binder.clearCallingIdentity();
        try {
            return UiccController.getInstance().getCardIdForDefaultEuicc();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * A test API to reload the UICC profile.
     *
     * <p>Requires that the calling app has permission
     * {@link android.Manifest.permission#MODIFY_PHONE_STATE MODIFY_PHONE_STATE}.
     * @hide
     */
    @Override
    public void refreshUiccProfile(int subId) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone == null) {
                return;
            }
            UiccCard uiccCard = phone.getUiccCard();
            if (uiccCard == null) {
                return;
            }
            UiccProfile uiccProfile = uiccCard.getUiccProfile();
            if (uiccProfile == null) {
                return;
            }
            uiccProfile.refresh();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Returns false if the mobile data is disabled by default, otherwise return true.
     */
    private boolean getDefaultDataEnabled() {
        return TelephonyProperties.mobile_data().orElse(true);
    }

    /**
     * Returns true if the data roaming is enabled by default, i.e the system property
     * of {@link #DEFAULT_DATA_ROAMING_PROPERTY_NAME} is true or the config of
     * {@link CarrierConfigManager#KEY_CARRIER_DEFAULT_DATA_ROAMING_ENABLED_BOOL} is true.
     */
    private boolean getDefaultDataRoamingEnabled(int subId) {
        final CarrierConfigManager configMgr = (CarrierConfigManager)
                mApp.getSystemService(Context.CARRIER_CONFIG_SERVICE);
        boolean isDataRoamingEnabled = TelephonyProperties.data_roaming().orElse(false);
        isDataRoamingEnabled |= configMgr.getConfigForSubId(subId).getBoolean(
                CarrierConfigManager.KEY_CARRIER_DEFAULT_DATA_ROAMING_ENABLED_BOOL);
        return isDataRoamingEnabled;
    }

    /**
     * Returns the default network type for the given {@code subId}, if the default network type is
     * not set, return {@link Phone#PREFERRED_NT_MODE}.
     */
    private int getDefaultNetworkType(int subId) {
        List<Integer> list = TelephonyProperties.default_network();
        int phoneId = mSubscriptionController.getPhoneId(subId);
        if (phoneId >= 0 && phoneId < list.size() && list.get(phoneId) != null) {
            return list.get(phoneId);
        }
        return Phone.PREFERRED_NT_MODE;
    }

    @Override
    public void setCarrierTestOverride(int subId, String mccmnc, String imsi, String iccid, String
            gid1, String gid2, String plmn, String spn, String carrierPrivilegeRules, String apn) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone == null) {
                loge("setCarrierTestOverride fails with invalid subId: " + subId);
                return;
            }
            phone.setCarrierTestOverride(mccmnc, imsi, iccid, gid1, gid2, plmn, spn,
                    carrierPrivilegeRules, apn);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getCarrierIdListVersion(int subId) {
        enforceReadPrivilegedPermission("getCarrierIdListVersion");

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone == null) {
                loge("getCarrierIdListVersion fails with invalid subId: " + subId);
                return TelephonyManager.UNKNOWN_CARRIER_ID_LIST_VERSION;
            }
            return phone.getCarrierIdListVersion();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getNumberOfModemsWithSimultaneousDataConnections(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "getNumberOfModemsWithSimultaneousDataConnections")) {
            return -1;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return mPhoneConfigurationManager.getNumberOfModemsWithSimultaneousDataConnections();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getCdmaRoamingMode(int subId) {
        TelephonyPermissions
                .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                mApp, subId, "getCdmaRoamingMode");

        final long identity = Binder.clearCallingIdentity();
        try {
            return (int) sendRequest(CMD_GET_CDMA_ROAMING_MODE, null /* argument */, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean setCdmaRoamingMode(int subId, int mode) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setCdmaRoamingMode");

        final long identity = Binder.clearCallingIdentity();
        try {
            return (boolean) sendRequest(CMD_SET_CDMA_ROAMING_MODE, mode, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean setCdmaSubscriptionMode(int subId, int mode) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "setCdmaSubscriptionMode");

        final long identity = Binder.clearCallingIdentity();
        try {
            return (boolean) sendRequest(CMD_SET_CDMA_SUBSCRIPTION_MODE, mode, subId);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public Map<Integer, List<EmergencyNumber>> getEmergencyNumberList(
            String callingPackage, String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, getDefaultSubscription(), callingPackage, callingFeatureId,
                "getEmergencyNumberList")) {
            throw new SecurityException("Requires READ_PHONE_STATE permission.");
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            Map<Integer, List<EmergencyNumber>> emergencyNumberListInternal = new HashMap<>();
            for (Phone phone: PhoneFactory.getPhones()) {
                if (phone.getEmergencyNumberTracker() != null
                        && phone.getEmergencyNumberTracker().getEmergencyNumberList() != null) {
                    emergencyNumberListInternal.put(
                            phone.getSubId(),
                            phone.getEmergencyNumberTracker().getEmergencyNumberList());
                }
            }
            return emergencyNumberListInternal;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isEmergencyNumber(String number, boolean exactMatch) {
        final Phone defaultPhone = getDefaultPhone();
        if (!exactMatch) {
            TelephonyPermissions
                    .enforeceCallingOrSelfReadPrivilegedPhoneStatePermissionOrCarrierPrivilege(
                            mApp, defaultPhone.getSubId(), "isEmergencyNumber(Potential)");
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            for (Phone phone: PhoneFactory.getPhones()) {
                if (phone.getEmergencyNumberTracker() != null
                        && phone.getEmergencyNumberTracker() != null) {
                    if (phone.getEmergencyNumberTracker().isEmergencyNumber(
                            number, exactMatch)) {
                        return true;
                    }
                }
            }
            return false;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Start emergency callback mode for GsmCdmaPhone for testing.
     */
    @Override
    public void startEmergencyCallbackMode() {
        TelephonyPermissions.enforceShellOnly(Binder.getCallingUid(),
                "startEmergencyCallbackMode");
        enforceModifyPermission();
        final long identity = Binder.clearCallingIdentity();
        try {
            for (Phone phone : PhoneFactory.getPhones()) {
                Rlog.d(LOG_TAG, "startEmergencyCallbackMode phone type: " + phone.getPhoneType());
                if (phone != null && ((phone.getPhoneType() == PHONE_TYPE_GSM)
                        || (phone.getPhoneType() == PHONE_TYPE_CDMA))) {
                    GsmCdmaPhone gsmCdmaPhone = (GsmCdmaPhone) phone;
                    gsmCdmaPhone.obtainMessage(
                            GsmCdmaPhone.EVENT_EMERGENCY_CALLBACK_MODE_ENTER).sendToTarget();
                    Rlog.d(LOG_TAG, "startEmergencyCallbackMode: triggered");
                }
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Update emergency number list for test mode.
     */
    @Override
    public void updateEmergencyNumberListTestMode(int action, EmergencyNumber num) {
        TelephonyPermissions.enforceShellOnly(Binder.getCallingUid(),
                "updateEmergencyNumberListTestMode");

        final long identity = Binder.clearCallingIdentity();
        try {
            for (Phone phone: PhoneFactory.getPhones()) {
                EmergencyNumberTracker tracker = phone.getEmergencyNumberTracker();
                if (tracker != null) {
                    tracker.executeEmergencyNumberTestModeCommand(action, num);
                }
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the full emergency number list for test mode.
     */
    @Override
    public List<String> getEmergencyNumberListTestMode() {
        TelephonyPermissions.enforceShellOnly(Binder.getCallingUid(),
                "getEmergencyNumberListTestMode");

        final long identity = Binder.clearCallingIdentity();
        try {
            Set<String> emergencyNumbers = new HashSet<>();
            for (Phone phone: PhoneFactory.getPhones()) {
                EmergencyNumberTracker tracker = phone.getEmergencyNumberTracker();
                if (tracker != null) {
                    for (EmergencyNumber num : tracker.getEmergencyNumberList()) {
                        emergencyNumbers.add(num.getNumber());
                    }
                }
            }
            return new ArrayList<>(emergencyNumbers);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public int getEmergencyNumberDbVersion(int subId) {
        enforceReadPrivilegedPermission("getEmergencyNumberDbVersion");

        final long identity = Binder.clearCallingIdentity();
        try {
            final Phone phone = getPhone(subId);
            if (phone == null) {
                loge("getEmergencyNumberDbVersion fails with invalid subId: " + subId);
                return TelephonyManager.INVALID_EMERGENCY_NUMBER_DB_VERSION;
            }
            return phone.getEmergencyNumberDbVersion();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void notifyOtaEmergencyNumberDbInstalled() {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            for (Phone phone: PhoneFactory.getPhones()) {
                EmergencyNumberTracker tracker = phone.getEmergencyNumberTracker();
                if (tracker != null) {
                    tracker.updateOtaEmergencyNumberDatabase();
                }
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void updateOtaEmergencyNumberDbFilePath(ParcelFileDescriptor otaParcelFileDescriptor) {
        enforceActiveEmergencySessionPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            for (Phone phone: PhoneFactory.getPhones()) {
                EmergencyNumberTracker tracker = phone.getEmergencyNumberTracker();
                if (tracker != null) {
                    tracker.updateOtaEmergencyNumberDbFilePath(otaParcelFileDescriptor);
                }
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void resetOtaEmergencyNumberDbFilePath() {
        enforceActiveEmergencySessionPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            for (Phone phone: PhoneFactory.getPhones()) {
                EmergencyNumberTracker tracker = phone.getEmergencyNumberTracker();
                if (tracker != null) {
                    tracker.resetOtaEmergencyNumberDbFilePath();
                }
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public List<String> getCertsFromCarrierPrivilegeAccessRules(int subId) {
        enforceReadPrivilegedPermission("getCertsFromCarrierPrivilegeAccessRules");
        Phone phone = getPhone(subId);
        if (phone == null) {
            return null;
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            UiccProfile profile = UiccController.getInstance()
                    .getUiccProfileForPhone(phone.getPhoneId());
            if (profile != null) {
                return profile.getCertsFromCarrierPrivilegeAccessRules();
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return null;
    }

    /**
     * Enable or disable a modem stack.
     */
    @Override
    public boolean enableModemForSlot(int slotIndex, boolean enable) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = PhoneFactory.getPhone(slotIndex);
            if (phone == null) {
                return false;
            } else {
                return (Boolean) sendRequest(CMD_REQUEST_ENABLE_MODEM, enable, phone, null);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Whether a modem stack is enabled or not.
     */
    @Override
    public boolean isModemEnabledForSlot(int slotIndex, String callingPackage,
            String callingFeatureId) {
        Phone phone = PhoneFactory.getPhone(slotIndex);
        if (phone == null) return false;

        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, phone.getSubId(), callingPackage, callingFeatureId,
                "isModemEnabledForSlot")) {
            throw new SecurityException("Requires READ_PHONE_STATE permission.");
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            try {
                return mPhoneConfigurationManager.getPhoneStatusFromCache(phone.getPhoneId());
            } catch (NoSuchElementException ex) {
                return (Boolean) sendRequest(CMD_GET_MODEM_STATUS, null, phone, null);
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setMultiSimCarrierRestriction(boolean isMultiSimCarrierRestricted) {
        enforceModifyPermission();

        final long identity = Binder.clearCallingIdentity();
        try {
            mTelephonySharedPreferences.edit()
                    .putBoolean(PREF_MULTI_SIM_RESTRICTED, isMultiSimCarrierRestricted)
                    .commit();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    @TelephonyManager.IsMultiSimSupportedResult
    public int isMultiSimSupported(String callingPackage, String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(mApp,
                getDefaultPhone().getSubId(), callingPackage, callingFeatureId,
                "isMultiSimSupported")) {
            return TelephonyManager.MULTISIM_NOT_SUPPORTED_BY_HARDWARE;
        }

        final long identity = Binder.clearCallingIdentity();
        try {
            return isMultiSimSupportedInternal();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @TelephonyManager.IsMultiSimSupportedResult
    private int isMultiSimSupportedInternal() {
        // If the device has less than 2 SIM cards, indicate that multisim is restricted.
        int numPhysicalSlots = UiccController.getInstance().getUiccSlots().length;
        if (numPhysicalSlots < 2) {
            loge("isMultiSimSupportedInternal: requires at least 2 cards");
            return TelephonyManager.MULTISIM_NOT_SUPPORTED_BY_HARDWARE;
        }
        // Check if the hardware supports multisim functionality. If usage of multisim is not
        // supported by the modem, indicate that it is restricted.
        PhoneCapability staticCapability =
                mPhoneConfigurationManager.getStaticPhoneCapability();
        if (staticCapability == null) {
            loge("isMultiSimSupportedInternal: no static configuration available");
            return TelephonyManager.MULTISIM_NOT_SUPPORTED_BY_HARDWARE;
        }
        if (staticCapability.logicalModemList.size() < 2) {
            loge("isMultiSimSupportedInternal: maximum number of modem is < 2");
            return TelephonyManager.MULTISIM_NOT_SUPPORTED_BY_HARDWARE;
        }
        // Check if support of multiple SIMs is restricted by carrier
        if (mTelephonySharedPreferences.getBoolean(PREF_MULTI_SIM_RESTRICTED, false)) {
            return TelephonyManager.MULTISIM_NOT_SUPPORTED_BY_CARRIER;
        }

        return TelephonyManager.MULTISIM_ALLOWED;
    }

    /**
     * Switch configs to enable multi-sim or switch back to single-sim
     * Note: Switch from multi-sim to single-sim is only possible with MODIFY_PHONE_STATE
     * permission, but the other way around is possible with either MODIFY_PHONE_STATE
     * or carrier privileges
     * @param numOfSims number of active sims we want to switch to
     */
    @Override
    public void switchMultiSimConfig(int numOfSims) {
        if (numOfSims == 1) {
            enforceModifyPermission();
        } else {
            TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                    mApp, SubscriptionManager.DEFAULT_SUBSCRIPTION_ID, "switchMultiSimConfig");
        }
        final long identity = Binder.clearCallingIdentity();

        try {
            //only proceed if multi-sim is not restricted
            if (isMultiSimSupportedInternal() != TelephonyManager.MULTISIM_ALLOWED) {
                loge("switchMultiSimConfig not possible. It is restricted or not supported.");
                return;
            }
            mPhoneConfigurationManager.switchMultiSimConfig(numOfSims);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isApplicationOnUicc(int subId, int appType) {
        enforceReadPrivilegedPermission("isApplicationOnUicc");
        Phone phone = getPhone(subId);
        if (phone == null) {
            return false;
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            UiccCard uiccCard = phone.getUiccCard();
            if (uiccCard == null) {
                return false;
            }
            UiccProfile uiccProfile = uiccCard.getUiccProfile();
            if (uiccProfile == null) {
                return false;
            }
            if (TelephonyManager.APPTYPE_SIM <= appType
                    && appType <= TelephonyManager.APPTYPE_ISIM) {
                return uiccProfile.isApplicationOnIcc(AppType.values()[appType]);
            }
            return false;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get whether making changes to modem configurations will trigger reboot.
     * Return value defaults to true.
     */
    @Override
    public boolean doesSwitchMultiSimConfigTriggerReboot(int subId, String callingPackage,
            String callingFeatureId) {
        if (!TelephonyPermissions.checkCallingOrSelfReadPhoneState(
                mApp, subId, callingPackage, callingFeatureId,
                "doesSwitchMultiSimConfigTriggerReboot")) {
            return false;
        }
        final long identity = Binder.clearCallingIdentity();
        try {
            return mPhoneConfigurationManager.isRebootRequiredForModemConfigChange();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    private void updateModemStateMetrics() {
        TelephonyMetrics metrics = TelephonyMetrics.getInstance();
        // TODO: check the state for each modem if the api is ready.
        metrics.updateEnabledModemBitmap((1 << TelephonyManager.from(mApp).getPhoneCount()) - 1);
    }

    @Override
    public int[] getSlotsMapping() {
        enforceReadPrivilegedPermission("getSlotsMapping");

        final long identity = Binder.clearCallingIdentity();
        try {
            int phoneCount = TelephonyManager.getDefault().getPhoneCount();
            // All logical slots should have a mapping to a physical slot.
            int[] logicalSlotsMapping = new int[phoneCount];
            UiccSlotInfo[] slotInfos = getUiccSlotsInfo();
            for (int i = 0; i < slotInfos.length; i++) {
                if (SubscriptionManager.isValidPhoneId(slotInfos[i].getLogicalSlotIdx())) {
                    logicalSlotsMapping[slotInfos[i].getLogicalSlotIdx()] = i;
                }
            }
            return logicalSlotsMapping;
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Get the IRadio HAL Version
     */
    @Override
    public int getRadioHalVersion() {
        Phone phone = getDefaultPhone();
        if (phone == null) return -1;
        HalVersion hv = phone.getHalVersion();
        if (hv.equals(HalVersion.UNKNOWN)) return -1;
        return hv.major * 100 + hv.minor;
    }

    /**
     * Get the current calling package name.
     * @return the current calling package name
     */
    @Override
    public String getCurrentPackageName() {
        return mApp.getPackageManager().getPackagesForUid(Binder.getCallingUid())[0];
    }

    /**
     * Return whether data is enabled for certain APN type. This will tell if framework will accept
     * corresponding network requests on a subId.
     *
     *  Data is enabled if:
     *  1) user data is turned on, or
     *  2) APN is un-metered for this subscription, or
     *  3) APN type is whitelisted. E.g. MMS is whitelisted if
     *  {@link TelephonyManager#setAlwaysAllowMmsData} is turned on.
     *
     * @return whether data is allowed for a apn type.
     *
     * @hide
     */
    @Override
    public boolean isDataEnabledForApn(int apnType, int subId, String callingPackage) {
        enforceReadPrivilegedPermission("Needs READ_PRIVILEGED_PHONE_STATE for "
                + "isDataEnabledForApn");

        // Now that all security checks passes, perform the operation as ourselves.
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone == null) return false;

            boolean isMetered = ApnSettingUtils.isMeteredApnType(apnType, phone);
            return !isMetered || phone.getDataEnabledSettings().isDataEnabled(apnType);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isApnMetered(@ApnType int apnType, int subId) {
        enforceReadPrivilegedPermission("isApnMetered");

        // Now that all security checks passes, perform the operation as ourselves.
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone == null) return true; // By default return true.

            return ApnSettingUtils.isMeteredApnType(apnType, phone);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public void setSystemSelectionChannels(List<RadioAccessSpecifier> specifiers,
            int subscriptionId, IBooleanConsumer resultCallback) {
        enforceModifyPermission();
        long token = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subscriptionId);
            if (phone == null) {
                try {
                    if (resultCallback != null) {
                        resultCallback.accept(false);
                    }
                } catch (RemoteException e) {
                    // ignore
                }
                return;
            }
            Pair<List<RadioAccessSpecifier>, Consumer<Boolean>> argument =
                    Pair.create(specifiers, (x) -> {
                        try {
                            if (resultCallback != null) {
                                resultCallback.accept(x);
                            }
                        } catch (RemoteException e) {
                            // ignore
                        }
                    });
            sendRequestAsync(CMD_SET_SYSTEM_SELECTION_CHANNELS, argument, phone, null);
        } finally {
            Binder.restoreCallingIdentity(token);
        }
    }

    @Override
    public boolean isMvnoMatched(int subId, int mvnoType, @NonNull String mvnoMatchData) {
        enforceReadPrivilegedPermission("isMvnoMatched");
        IccRecords iccRecords = UiccController.getInstance().getIccRecords(
                SubscriptionManager.getPhoneId(subId), UiccController.APP_FAM_3GPP);
        if (iccRecords == null) {
            Log.d(LOG_TAG, "isMvnoMatched# IccRecords is null");
            return false;
        }
        return ApnSettingUtils.mvnoMatches(iccRecords, mvnoType, mvnoMatchData);
    }

    @Override
    public void enqueueSmsPickResult(String callingPackage, String callingAttributionTag,
            IIntegerConsumer pendingSubIdResult) {
        if (callingPackage == null) {
            callingPackage = getCurrentPackageName();
        }
        SmsPermissions permissions = new SmsPermissions(getDefaultPhone(), mApp,
                (AppOpsManager) mApp.getSystemService(Context.APP_OPS_SERVICE));
        if (!permissions.checkCallingCanSendSms(callingPackage, callingAttributionTag,
                "Sending message")) {
            throw new SecurityException("Requires SEND_SMS permission to perform this operation");
        }
        PickSmsSubscriptionActivity.addPendingResult(pendingSubIdResult);
        Intent intent = new Intent();
        intent.setClass(mApp, PickSmsSubscriptionActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Bring up choose default SMS subscription dialog right now
        intent.putExtra(PickSmsSubscriptionActivity.DIALOG_TYPE_KEY,
                PickSmsSubscriptionActivity.SMS_PICK_FOR_MESSAGE);
        mApp.startActivity(intent);
    }

    @Override
    public String getMmsUAProfUrl(int subId) {
        //TODO investigate if this API should require proper permission check in R b/133791609
        final long identity = Binder.clearCallingIdentity();
        try {
            return SubscriptionManager.getResourcesForSubId(getDefaultPhone().getContext(), subId)
                    .getString(com.android.internal.R.string.config_mms_user_agent_profile_url);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public String getMmsUserAgent(int subId) {
        //TODO investigate if this API should require proper permission check in R b/133791609
        final long identity = Binder.clearCallingIdentity();
        try {
            return SubscriptionManager.getResourcesForSubId(getDefaultPhone().getContext(), subId)
                    .getString(com.android.internal.R.string.config_mms_user_agent);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean setDataAllowedDuringVoiceCall(int subId, boolean allow) {
        enforceModifyPermission();

        // Now that all security checks passes, perform the operation as ourselves.
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone == null) return false;

            return phone.getDataEnabledSettings().setAllowDataDuringVoiceCall(allow);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean isDataAllowedInVoiceCall(int subId) {
        enforceReadPrivilegedPermission("isDataAllowedInVoiceCall");

        // Now that all security checks passes, perform the operation as ourselves.
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone == null) return false;

            return phone.getDataEnabledSettings().isDataAllowedInVoiceCall();
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    @Override
    public boolean setAlwaysAllowMmsData(int subId, boolean alwaysAllow) {
        enforceModifyPermission();

        // Now that all security checks passes, perform the operation as ourselves.
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone == null) return false;

            return phone.getDataEnabledSettings().setAlwaysAllowMmsData(alwaysAllow);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Updates whether conference event pacakge handling is enabled.
     * @param isCepEnabled {@code true} if CEP handling is enabled (default), or {@code false}
     *                                 otherwise.
     */
    @Override
    public void setCepEnabled(boolean isCepEnabled) {
        TelephonyPermissions.enforceShellOnly(Binder.getCallingUid(), "setCepEnabled");

        final long identity = Binder.clearCallingIdentity();
        try {
            Rlog.i(LOG_TAG, "setCepEnabled isCepEnabled=" + isCepEnabled);
            for (Phone phone : PhoneFactory.getPhones()) {
                Phone defaultPhone = phone.getImsPhone();
                if (defaultPhone != null && defaultPhone.getPhoneType() == PHONE_TYPE_IMS) {
                    ImsPhone imsPhone = (ImsPhone) defaultPhone;
                    ImsPhoneCallTracker imsPhoneCallTracker =
                            (ImsPhoneCallTracker) imsPhone.getCallTracker();
                    imsPhoneCallTracker.setConferenceEventPackageEnabled(isCepEnabled);
                    Rlog.i(LOG_TAG, "setCepEnabled isCepEnabled=" + isCepEnabled + ", for imsPhone "
                            + imsPhone.getMsisdn());
                }
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Notify that an RCS autoconfiguration XML file has been received for provisioning.
     *
     * @param config       The XML file to be read. ASCII/UTF8 encoded text if not compressed.
     * @param isCompressed The XML file is compressed in gzip format and must be decompressed
     *                     before being read.
     */
    @Override
    public void notifyRcsAutoConfigurationReceived(int subId, @NonNull byte[] config, boolean
            isCompressed) {
        TelephonyPermissions.enforceCallingOrSelfModifyPermissionOrCarrierPrivilege(
                mApp, subId, "notifyRcsAutoConfigurationReceived");
        try {
            IImsConfig configBinder = getImsConfig(getSlotIndex(subId), ImsFeature.FEATURE_RCS);
            if (configBinder == null) {
                Rlog.e(LOG_TAG, "null result for getImsConfig");
            } else {
                configBinder.notifyRcsAutoConfigurationReceived(config, isCompressed);
            }
        } catch (RemoteException e) {
            Rlog.e(LOG_TAG, "fail to getImsConfig " + e.getMessage());
        }
    }

    @Override
    public boolean isIccLockEnabled(int subId) {
        enforceReadPrivilegedPermission("isIccLockEnabled");

        // Now that all security checks passes, perform the operation as ourselves.
        final long identity = Binder.clearCallingIdentity();
        try {
            Phone phone = getPhone(subId);
            if (phone != null && phone.getIccCard() != null) {
                return phone.getIccCard().getIccLockEnabled();
            } else {
                return false;
            }
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
    }

    /**
     * Set the ICC pin lock enabled or disabled.
     *
     * @return an integer representing the status of IccLock enabled or disabled in the following
     * three cases:
     *   - {@link TelephonyManager#CHANGE_ICC_LOCK_SUCCESS} if enabled or disabled IccLock
     *   successfully.
     *   - Positive number and zero for remaining password attempts.
     *   - Negative number for other failure cases (such like enabling/disabling PIN failed).
     *
     */
    @Override
    public int setIccLockEnabled(int subId, boolean enabled, String password) {
        enforceModifyPermission();

        Phone phone = getPhone(subId);
        if (phone == null) {
            return 0;
        }
        // Now that all security checks passes, perform the operation as ourselves.
        final long identity = Binder.clearCallingIdentity();
        try {
            int attemptsRemaining = (int) sendRequest(CMD_SET_ICC_LOCK_ENABLED,
                    new Pair<Boolean, String>(enabled, password), phone, null);
            return attemptsRemaining;

        } catch (Exception e) {
            Log.e(LOG_TAG, "setIccLockEnabled. Exception e =" + e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return 0;
    }

    /**
     * Change the ICC password used in ICC pin lock.
     *
     * @return an integer representing the status of IccLock changed in the following three cases:
     *   - {@link TelephonyManager#CHANGE_ICC_LOCK_SUCCESS} if changed IccLock successfully.
     *   - Positive number and zero for remaining password attempts.
     *   - Negative number for other failure cases (such like enabling/disabling PIN failed).
     *
     */
    @Override
    public int changeIccLockPassword(int subId, String oldPassword, String newPassword) {
        enforceModifyPermission();

        Phone phone = getPhone(subId);
        if (phone == null) {
            return 0;
        }
        // Now that all security checks passes, perform the operation as ourselves.
        final long identity = Binder.clearCallingIdentity();
        try {
            int attemptsRemaining = (int) sendRequest(CMD_CHANGE_ICC_LOCK_PASSWORD,
                    new Pair<String, String>(oldPassword, newPassword), phone, null);
            return attemptsRemaining;

        } catch (Exception e) {
            Log.e(LOG_TAG, "changeIccLockPassword. Exception e =" + e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return 0;
    }

    /**
     * Request for receiving user activity notification
     */
    @Override
    public void requestUserActivityNotification() {
        if (!mNotifyUserActivity.get()
                && !mMainThreadHandler.hasMessages(MSG_NOTIFY_USER_ACTIVITY)) {
            mNotifyUserActivity.set(true);
        }
    }

    /**
     * Called when userActivity is signalled in the power manager.
     * This is safe to call from any thread, with any window manager locks held or not.
     */
    @Override
    public void userActivity() {
        // ***************************************
        // *  Inherited from PhoneWindowManager  *
        // ***************************************
        // THIS IS CALLED FROM DEEP IN THE POWER MANAGER
        // WITH ITS LOCKS HELD.
        //
        // This code must be VERY careful about the locks
        // it acquires.
        // In fact, the current code acquires way too many,
        // and probably has lurking deadlocks.

        if (Binder.getCallingUid() != Process.SYSTEM_UID) {
            throw new SecurityException("Only the OS may call notifyUserActivity()");
        }

        if (mNotifyUserActivity.getAndSet(false)) {
            mMainThreadHandler.sendEmptyMessageDelayed(MSG_NOTIFY_USER_ACTIVITY,
                    USER_ACTIVITY_NOTIFICATION_DELAY);
        }
    }

    @Override
    public boolean canConnectTo5GInDsdsMode() {
        return mApp.getResources().getBoolean(R.bool.config_5g_connection_in_dsds_mode);
    }
}
