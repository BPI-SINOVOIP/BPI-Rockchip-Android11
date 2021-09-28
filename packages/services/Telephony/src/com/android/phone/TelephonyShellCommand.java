/*
 * Copyright (C) 2018 The Android Open Source Project
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

import android.content.Context;
import android.os.BasicShellCommandHandler;
import android.os.Binder;
import android.os.PersistableBundle;
import android.os.Process;
import android.os.RemoteException;
import android.provider.BlockedNumberContract;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.telephony.emergency.EmergencyNumber;
import android.telephony.ims.feature.ImsFeature;
import android.util.Log;

import com.android.internal.telephony.ITelephony;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneFactory;
import com.android.internal.telephony.emergency.EmergencyNumberTracker;
import com.android.internal.telephony.util.TelephonyUtils;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

/**
 * Takes actions based on the adb commands given by "adb shell cmd phone ...". Be careful, no
 * permission checks have been done before onCommand was called. Make sure any commands processed
 * here also contain the appropriate permissions checks.
 */

public class TelephonyShellCommand extends BasicShellCommandHandler {

    private static final String LOG_TAG = "TelephonyShellCommand";
    // Don't commit with this true.
    private static final boolean VDBG = true;
    private static final int DEFAULT_PHONE_ID = 0;

    private static final String IMS_SUBCOMMAND = "ims";
    private static final String NUMBER_VERIFICATION_SUBCOMMAND = "numverify";
    private static final String EMERGENCY_CALLBACK_MODE = "emergency-callback-mode";
    private static final String EMERGENCY_NUMBER_TEST_MODE = "emergency-number-test-mode";
    private static final String END_BLOCK_SUPPRESSION = "end-block-suppression";
    private static final String CARRIER_CONFIG_SUBCOMMAND = "cc";
    private static final String DATA_TEST_MODE = "data";
    private static final String DATA_ENABLE = "enable";
    private static final String DATA_DISABLE = "disable";

    private static final String IMS_SET_CARRIER_SERVICE = "set-ims-service";
    private static final String IMS_GET_CARRIER_SERVICE = "get-ims-service";
    private static final String IMS_ENABLE = "enable";
    private static final String IMS_DISABLE = "disable";
    // Used to disable or enable processing of conference event package data from the network.
    // This is handy for testing scenarios where CEP data does not exist on a network which does
    // support CEP data.
    private static final String IMS_CEP = "conference-event-package";

    private static final String NUMBER_VERIFICATION_OVERRIDE_PACKAGE = "override-package";
    private static final String NUMBER_VERIFICATION_FAKE_CALL = "fake-call";

    private static final String CC_GET_VALUE = "get-value";
    private static final String CC_SET_VALUE = "set-value";
    private static final String CC_CLEAR_VALUES = "clear-values";

    // Take advantage of existing methods that already contain permissions checks when possible.
    private final ITelephony mInterface;

    private SubscriptionManager mSubscriptionManager;
    private CarrierConfigManager mCarrierConfigManager;
    private Context mContext;

    private enum CcType {
        BOOLEAN, DOUBLE, DOUBLE_ARRAY, INT, INT_ARRAY, LONG, LONG_ARRAY, STRING,
                STRING_ARRAY, UNKNOWN
    }

    private class CcOptionParseResult {
        public int mSubId;
        public boolean mPersistent;
    }

    // Maps carrier config keys to type. It is possible to infer the type for most carrier config
    // keys by looking at the end of the string which usually tells the type.
    // For instance: "xxxx_string", "xxxx_string_array", etc.
    // The carrier config keys in this map does not follow this convention. It is therefore not
    // possible to infer the type for these keys by looking at the string.
    private static final Map<String, CcType> CC_TYPE_MAP = new HashMap<String, CcType>() {{
            put(CarrierConfigManager.Gps.KEY_A_GLONASS_POS_PROTOCOL_SELECT_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_ES_EXTENSION_SEC_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_GPS_LOCK_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_LPP_PROFILE_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_NFW_PROXY_APPS_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_SUPL_ES_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_SUPL_HOST_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_SUPL_MODE_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_SUPL_PORT_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_SUPL_VER_STRING, CcType.STRING);
            put(CarrierConfigManager.Gps.KEY_USE_EMERGENCY_PDN_FOR_EMERGENCY_SUPL_STRING,
                    CcType.STRING);
            put(CarrierConfigManager.KEY_CARRIER_APP_NO_WAKE_SIGNAL_CONFIG_STRING_ARRAY,
                    CcType.STRING_ARRAY);
            put(CarrierConfigManager.KEY_CARRIER_APP_WAKE_SIGNAL_CONFIG_STRING_ARRAY,
                    CcType.STRING_ARRAY);
            put(CarrierConfigManager.KEY_CARRIER_CALL_SCREENING_APP_STRING, CcType.STRING);
            put(CarrierConfigManager.KEY_MMS_EMAIL_GATEWAY_NUMBER_STRING, CcType.STRING);
            put(CarrierConfigManager.KEY_MMS_HTTP_PARAMS_STRING, CcType.STRING);
            put(CarrierConfigManager.KEY_MMS_NAI_SUFFIX_STRING, CcType.STRING);
            put(CarrierConfigManager.KEY_MMS_UA_PROF_TAG_NAME_STRING, CcType.STRING);
            put(CarrierConfigManager.KEY_MMS_UA_PROF_URL_STRING, CcType.STRING);
            put(CarrierConfigManager.KEY_MMS_USER_AGENT_STRING, CcType.STRING);
            put(CarrierConfigManager.KEY_RATCHET_RAT_FAMILIES, CcType.STRING_ARRAY);
        }
    };

    public TelephonyShellCommand(ITelephony binder, Context context) {
        mInterface = binder;
        mCarrierConfigManager =
                (CarrierConfigManager) context.getSystemService(Context.CARRIER_CONFIG_SERVICE);
        mSubscriptionManager = (SubscriptionManager)
                context.getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        mContext = context;
    }

    @Override
    public int onCommand(String cmd) {
        if (cmd == null) {
            return handleDefaultCommands(null);
        }

        switch (cmd) {
            case IMS_SUBCOMMAND: {
                return handleImsCommand();
            }
            case NUMBER_VERIFICATION_SUBCOMMAND:
                return handleNumberVerificationCommand();
            case EMERGENCY_CALLBACK_MODE:
                return handleEmergencyCallbackModeCommand();
            case EMERGENCY_NUMBER_TEST_MODE:
                return handleEmergencyNumberTestModeCommand();
            case CARRIER_CONFIG_SUBCOMMAND: {
                return handleCcCommand();
            }
            case DATA_TEST_MODE:
                return handleDataTestModeCommand();
            case END_BLOCK_SUPPRESSION:
                return handleEndBlockSuppressionCommand();
            default: {
                return handleDefaultCommands(cmd);
            }
        }
    }

    @Override
    public void onHelp() {
        PrintWriter pw = getOutPrintWriter();
        pw.println("Telephony Commands:");
        pw.println("  help");
        pw.println("    Print this help text.");
        pw.println("  ims");
        pw.println("    IMS Commands.");
        pw.println("  emergency-number-test-mode");
        pw.println("    Emergency Number Test Mode Commands.");
        pw.println("  end-block-suppression");
        pw.println("    End Block Suppression command.");
        pw.println("  data");
        pw.println("    Data Test Mode Commands.");
        pw.println("  cc");
        pw.println("    Carrier Config Commands.");
        onHelpIms();
        onHelpEmergencyNumber();
        onHelpEndBlockSupperssion();
        onHelpDataTestMode();
        onHelpCc();
    }

    private void onHelpIms() {
        PrintWriter pw = getOutPrintWriter();
        pw.println("IMS Commands:");
        pw.println("  ims set-ims-service [-s SLOT_ID] (-c | -d | -f) PACKAGE_NAME");
        pw.println("    Sets the ImsService defined in PACKAGE_NAME to to be the bound");
        pw.println("    ImsService. Options are:");
        pw.println("      -s: the slot ID that the ImsService should be bound for. If no option");
        pw.println("          is specified, it will choose the default voice SIM slot.");
        pw.println("      -c: Override the ImsService defined in the carrier configuration.");
        pw.println("      -d: Override the ImsService defined in the device overlay.");
        pw.println("      -f: Set the feature that this override if for, if no option is");
        pw.println("          specified, the new package name will be used for all features.");
        pw.println("  ims get-ims-service [-s SLOT_ID] [-c | -d]");
        pw.println("    Gets the package name of the currently defined ImsService.");
        pw.println("    Options are:");
        pw.println("      -s: The SIM slot ID for the registered ImsService. If no option");
        pw.println("          is specified, it will choose the default voice SIM slot.");
        pw.println("      -c: The ImsService defined as the carrier configured ImsService.");
        pw.println("      -c: The ImsService defined as the device default ImsService.");
        pw.println("      -f: The feature type that the query will be requested for. If none is");
        pw.println("          specified, the returned package name will correspond to MMTEL.");
        pw.println("  ims enable [-s SLOT_ID]");
        pw.println("    enables IMS for the SIM slot specified, or for the default voice SIM slot");
        pw.println("    if none is specified.");
        pw.println("  ims disable [-s SLOT_ID]");
        pw.println("    disables IMS for the SIM slot specified, or for the default voice SIM");
        pw.println("    slot if none is specified.");
        pw.println("  ims conference-event-package [enable/disable]");
        pw.println("    enables or disables handling or network conference event package data.");
    }

    private void onHelpNumberVerification() {
        PrintWriter pw = getOutPrintWriter();
        pw.println("Number verification commands");
        pw.println("  numverify override-package PACKAGE_NAME;");
        pw.println("    Set the authorized package for number verification.");
        pw.println("    Leave the package name blank to reset.");
        pw.println("  numverify fake-call NUMBER;");
        pw.println("    Fake an incoming call from NUMBER. This is for testing. Output will be");
        pw.println("    1 if the call would have been intercepted, 0 otherwise.");
    }

    private void onHelpDataTestMode() {
        PrintWriter pw = getOutPrintWriter();
        pw.println("Mobile Data Test Mode Commands:");
        pw.println("  data enable: enable mobile data connectivity");
        pw.println("  data disable: disable mobile data connectivity");
    }

    private void onHelpEmergencyNumber() {
        PrintWriter pw = getOutPrintWriter();
        pw.println("Emergency Number Test Mode Commands:");
        pw.println("  emergency-number-test-mode ");
        pw.println("    Add(-a), Clear(-c), Print (-p) or Remove(-r) the emergency number list in"
                + " the test mode");
        pw.println("      -a <emergency number address>: add an emergency number address for the"
                + " test mode, only allows '0'-'9', '*', '#' or '+'.");
        pw.println("      -c: clear the emergency number list in the test mode.");
        pw.println("      -r <emergency number address>: remove an existing emergency number"
                + " address added by the test mode, only allows '0'-'9', '*', '#' or '+'.");
        pw.println("      -p: get the full emergency number list in the test mode.");
    }

    private void onHelpEndBlockSupperssion() {
        PrintWriter pw = getOutPrintWriter();
        pw.println("End Block Suppression command:");
        pw.println("  end-block-suppression: disable suppressing blocking by contact");
        pw.println("                         with emergency services.");
    }

    private void onHelpCc() {
        PrintWriter pw = getOutPrintWriter();
        pw.println("Carrier Config Commands:");
        pw.println("  cc get-value [-s SLOT_ID] [KEY]");
        pw.println("    Print carrier config values.");
        pw.println("    Options are:");
        pw.println("      -s: The SIM slot ID to read carrier config value for. If no option");
        pw.println("          is specified, it will choose the default voice SIM slot.");
        pw.println("    KEY: The key to the carrier config value to print. All values are printed");
        pw.println("         if KEY is not specified.");
        pw.println("  cc set-value [-s SLOT_ID] [-p] KEY [NEW_VALUE]");
        pw.println("    Set carrier config KEY to NEW_VALUE.");
        pw.println("    Options are:");
        pw.println("      -s: The SIM slot ID to set carrier config value for. If no option");
        pw.println("          is specified, it will choose the default voice SIM slot.");
        pw.println("      -p: Value will be stored persistent");
        pw.println("    NEW_VALUE specifies the new value for carrier config KEY. Null will be");
        pw.println("      used if NEW_VALUE is not set. Strings should be encapsulated with");
        pw.println("      quotation marks. Spaces needs to be escaped. Example: \"Hello\\ World\"");
        pw.println("      Separate items in arrays with space . Example: \"item1\" \"item2\"");
        pw.println("  cc clear-values [-s SLOT_ID]");
        pw.println("    Clear all carrier override values that has previously been set");
        pw.println("    with set-value");
        pw.println("    Options are:");
        pw.println("      -s: The SIM slot ID to clear carrier config values for. If no option");
        pw.println("          is specified, it will choose the default voice SIM slot.");
    }

    private int handleImsCommand() {
        String arg = getNextArg();
        if (arg == null) {
            onHelpIms();
            return 0;
        }

        switch (arg) {
            case IMS_SET_CARRIER_SERVICE: {
                return handleImsSetServiceCommand();
            }
            case IMS_GET_CARRIER_SERVICE: {
                return handleImsGetServiceCommand();
            }
            case IMS_ENABLE: {
                return handleEnableIms();
            }
            case IMS_DISABLE: {
                return handleDisableIms();
            }
            case IMS_CEP: {
                return handleCepChange();
            }
        }

        return -1;
    }

    private int handleDataTestModeCommand() {
        PrintWriter errPw = getErrPrintWriter();
        String arg = getNextArgRequired();
        if (arg == null) {
            onHelpDataTestMode();
            return 0;
        }
        switch (arg) {
            case DATA_ENABLE: {
                try {
                    mInterface.enableDataConnectivity();
                } catch (RemoteException ex) {
                    Log.w(LOG_TAG, "data enable, error " + ex.getMessage());
                    errPw.println("Exception: " + ex.getMessage());
                    return -1;
                }
                break;
            }
            case DATA_DISABLE: {
                try {
                    mInterface.disableDataConnectivity();
                } catch (RemoteException ex) {
                    Log.w(LOG_TAG, "data disable, error " + ex.getMessage());
                    errPw.println("Exception: " + ex.getMessage());
                    return -1;
                }
                break;
            }
            default:
                onHelpDataTestMode();
                break;
        }
        return 0;
    }

    private int handleEmergencyCallbackModeCommand() {
        PrintWriter errPw = getErrPrintWriter();
        try {
            mInterface.startEmergencyCallbackMode();
            Log.d(LOG_TAG, "handleEmergencyCallbackModeCommand: triggered");
        } catch (RemoteException ex) {
            Log.w(LOG_TAG, "emergency-callback-mode error: " + ex.getMessage());
            errPw.println("Exception: " + ex.getMessage());
            return -1;
        }
        return 0;
    }

    private int handleEmergencyNumberTestModeCommand() {
        PrintWriter errPw = getErrPrintWriter();
        String opt = getNextOption();
        if (opt == null) {
            onHelpEmergencyNumber();
            return 0;
        }

        switch (opt) {
            case "-a": {
                String emergencyNumberCmd = getNextArgRequired();
                if (emergencyNumberCmd == null
                        || !EmergencyNumber.validateEmergencyNumberAddress(emergencyNumberCmd)) {
                    errPw.println("An emergency number (only allow '0'-'9', '*', '#' or '+') needs"
                            + " to be specified after -a in the command ");
                    return -1;
                }
                try {
                    mInterface.updateEmergencyNumberListTestMode(
                            EmergencyNumberTracker.ADD_EMERGENCY_NUMBER_TEST_MODE,
                            new EmergencyNumber(emergencyNumberCmd, "", "",
                                    EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_UNSPECIFIED,
                                    new ArrayList<String>(),
                                    EmergencyNumber.EMERGENCY_NUMBER_SOURCE_TEST,
                                    EmergencyNumber.EMERGENCY_CALL_ROUTING_UNKNOWN));
                } catch (RemoteException ex) {
                    Log.w(LOG_TAG, "emergency-number-test-mode -a " + emergencyNumberCmd
                            + ", error " + ex.getMessage());
                    errPw.println("Exception: " + ex.getMessage());
                    return -1;
                }
                break;
            }
            case "-c": {
                try {
                    mInterface.updateEmergencyNumberListTestMode(
                            EmergencyNumberTracker.RESET_EMERGENCY_NUMBER_TEST_MODE, null);
                } catch (RemoteException ex) {
                    Log.w(LOG_TAG, "emergency-number-test-mode -c " + "error " + ex.getMessage());
                    errPw.println("Exception: " + ex.getMessage());
                    return -1;
                }
                break;
            }
            case "-r": {
                String emergencyNumberCmd = getNextArgRequired();
                if (emergencyNumberCmd == null
                        || !EmergencyNumber.validateEmergencyNumberAddress(emergencyNumberCmd)) {
                    errPw.println("An emergency number (only allow '0'-'9', '*', '#' or '+') needs"
                            + " to be specified after -r in the command ");
                    return -1;
                }
                try {
                    mInterface.updateEmergencyNumberListTestMode(
                            EmergencyNumberTracker.REMOVE_EMERGENCY_NUMBER_TEST_MODE,
                            new EmergencyNumber(emergencyNumberCmd, "", "",
                                    EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_UNSPECIFIED,
                                    new ArrayList<String>(),
                                    EmergencyNumber.EMERGENCY_NUMBER_SOURCE_TEST,
                                    EmergencyNumber.EMERGENCY_CALL_ROUTING_UNKNOWN));
                } catch (RemoteException ex) {
                    Log.w(LOG_TAG, "emergency-number-test-mode -r " + emergencyNumberCmd
                            + ", error " + ex.getMessage());
                    errPw.println("Exception: " + ex.getMessage());
                    return -1;
                }
                break;
            }
            case "-p": {
                try {
                    getOutPrintWriter().println(mInterface.getEmergencyNumberListTestMode());
                } catch (RemoteException ex) {
                    Log.w(LOG_TAG, "emergency-number-test-mode -p " + "error " + ex.getMessage());
                    errPw.println("Exception: " + ex.getMessage());
                    return -1;
                }
                break;
            }
            default:
                onHelpEmergencyNumber();
                break;
        }
        return 0;
    }

    private int handleNumberVerificationCommand() {
        String arg = getNextArg();
        if (arg == null) {
            onHelpNumberVerification();
            return 0;
        }

        if (!checkShellUid()) {
            return -1;
        }

        switch (arg) {
            case NUMBER_VERIFICATION_OVERRIDE_PACKAGE: {
                NumberVerificationManager.overrideAuthorizedPackage(getNextArg());
                return 0;
            }
            case NUMBER_VERIFICATION_FAKE_CALL: {
                boolean val = NumberVerificationManager.getInstance()
                        .checkIncomingCall(getNextArg());
                getOutPrintWriter().println(val ? "1" : "0");
                return 0;
            }
        }

        return -1;
    }

    // ims set-ims-service
    private int handleImsSetServiceCommand() {
        PrintWriter errPw = getErrPrintWriter();
        int slotId = getDefaultSlot();
        Boolean isCarrierService = null;
        List<Integer> featuresList = new ArrayList<>();

        String opt;
        while ((opt = getNextOption()) != null) {
            switch (opt) {
                case "-s": {
                    try {
                        slotId = Integer.parseInt(getNextArgRequired());
                    } catch (NumberFormatException e) {
                        errPw.println("ims set-ims-service requires an integer as a SLOT_ID.");
                        return -1;
                    }
                    break;
                }
                case "-c": {
                    isCarrierService = true;
                    break;
                }
                case "-d": {
                    isCarrierService = false;
                    break;
                }
                case "-f": {
                    String featureString = getNextArgRequired();
                    String[] features = featureString.split(",");
                    for (int i = 0; i < features.length; i++) {
                        try {
                            Integer result = Integer.parseInt(features[i]);
                            if (result < ImsFeature.FEATURE_EMERGENCY_MMTEL
                                    || result >= ImsFeature.FEATURE_MAX) {
                                errPw.println("ims set-ims-service -f " + result
                                        + " is an invalid feature.");
                                return -1;
                            }
                            featuresList.add(result);
                        } catch (NumberFormatException e) {
                            errPw.println("ims set-ims-service -f tried to parse " + features[i]
                                            + " as an integer.");
                            return -1;
                        }
                    }
                }
            }
        }
        // Mandatory param, either -c or -d
        if (isCarrierService == null) {
            errPw.println("ims set-ims-service requires either \"-c\" or \"-d\" to be set.");
            return -1;
        }

        String packageName = getNextArg();

        try {
            if (packageName == null) {
                packageName = "";
            }
            int[] featureArray = new int[featuresList.size()];
            for (int i = 0; i < featuresList.size(); i++) {
                featureArray[i] = featuresList.get(i);
            }
            boolean result = mInterface.setBoundImsServiceOverride(slotId, isCarrierService,
                    featureArray, packageName);
            if (VDBG) {
                Log.v(LOG_TAG, "ims set-ims-service -s " + slotId + " "
                        + (isCarrierService ? "-c " : "-d ")
                        + "-f " + featuresList + " "
                        + packageName + ", result=" + result);
            }
            getOutPrintWriter().println(result);
        } catch (RemoteException e) {
            Log.w(LOG_TAG, "ims set-ims-service -s " + slotId + " "
                    + (isCarrierService ? "-c " : "-d ")
                    + "-f " + featuresList + " "
                    + packageName + ", error" + e.getMessage());
            errPw.println("Exception: " + e.getMessage());
            return -1;
        }
        return 0;
    }

    // ims get-ims-service
    private int handleImsGetServiceCommand() {
        PrintWriter errPw = getErrPrintWriter();
        int slotId = getDefaultSlot();
        Boolean isCarrierService = null;
        Integer featureType = ImsFeature.FEATURE_MMTEL;

        String opt;
        while ((opt = getNextOption()) != null) {
            switch (opt) {
                case "-s": {
                    try {
                        slotId = Integer.parseInt(getNextArgRequired());
                    } catch (NumberFormatException e) {
                        errPw.println("ims set-ims-service requires an integer as a SLOT_ID.");
                        return -1;
                    }
                    break;
                }
                case "-c": {
                    isCarrierService = true;
                    break;
                }
                case "-d": {
                    isCarrierService = false;
                    break;
                }
                case "-f": {
                    try {
                        featureType = Integer.parseInt(getNextArg());
                    } catch (NumberFormatException e) {
                        errPw.println("ims get-ims-service -f requires valid integer as feature.");
                        return -1;
                    }
                    if (featureType < ImsFeature.FEATURE_EMERGENCY_MMTEL
                            || featureType >= ImsFeature.FEATURE_MAX) {
                        errPw.println("ims get-ims-service -f invalid feature.");
                        return -1;
                    }
                }
            }
        }
        // Mandatory param, either -c or -d
        if (isCarrierService == null) {
            errPw.println("ims get-ims-service requires either \"-c\" or \"-d\" to be set.");
            return -1;
        }

        String result;
        try {
            result = mInterface.getBoundImsServicePackage(slotId, isCarrierService, featureType);
        } catch (RemoteException e) {
            return -1;
        }
        if (VDBG) {
            Log.v(LOG_TAG, "ims get-ims-service -s " + slotId + " "
                    + (isCarrierService ? "-c " : "-d ")
                    + (featureType != null ? ("-f " + featureType) : "") + " , returned: "
                    + result);
        }
        getOutPrintWriter().println(result);
        return 0;
    }

    private int handleEnableIms() {
        int slotId = getDefaultSlot();
        String opt;
        while ((opt = getNextOption()) != null) {
            switch (opt) {
                case "-s": {
                    try {
                        slotId = Integer.parseInt(getNextArgRequired());
                    } catch (NumberFormatException e) {
                        getErrPrintWriter().println("ims enable requires an integer as a SLOT_ID.");
                        return -1;
                    }
                    break;
                }
            }
        }
        try {
            mInterface.enableIms(slotId);
        } catch (RemoteException e) {
            return -1;
        }
        if (VDBG) {
            Log.v(LOG_TAG, "ims enable -s " + slotId);
        }
        return 0;
    }

    private int handleDisableIms() {
        int slotId = getDefaultSlot();
        String opt;
        while ((opt = getNextOption()) != null) {
            switch (opt) {
                case "-s": {
                    try {
                        slotId = Integer.parseInt(getNextArgRequired());
                    } catch (NumberFormatException e) {
                        getErrPrintWriter().println(
                                "ims disable requires an integer as a SLOT_ID.");
                        return -1;
                    }
                    break;
                }
            }
        }
        try {
            mInterface.disableIms(slotId);
        } catch (RemoteException e) {
            return -1;
        }
        if (VDBG) {
            Log.v(LOG_TAG, "ims disable -s " + slotId);
        }
        return 0;
    }

    private int handleCepChange() {
        Log.i(LOG_TAG, "handleCepChange");
        String opt = getNextArg();
        if (opt == null) {
            return -1;
        }
        boolean isCepEnabled = opt.equals("enable");

        try {
            mInterface.setCepEnabled(isCepEnabled);
        } catch (RemoteException e) {
            return -1;
        }
        return 0;
    }

    private int getDefaultSlot() {
        int slotId = SubscriptionManager.getDefaultVoicePhoneId();
        if (slotId <= SubscriptionManager.INVALID_SIM_SLOT_INDEX
                || slotId == SubscriptionManager.DEFAULT_PHONE_INDEX) {
            // If there is no default, default to slot 0.
            slotId = DEFAULT_PHONE_ID;
        }
        return slotId;
    }

    // Parse options related to Carrier Config Commands.
    private CcOptionParseResult parseCcOptions(String tag, boolean allowOptionPersistent) {
        PrintWriter errPw = getErrPrintWriter();
        CcOptionParseResult result = new CcOptionParseResult();
        result.mSubId = SubscriptionManager.getDefaultSubscriptionId();
        result.mPersistent = false;

        String opt;
        while ((opt = getNextOption()) != null) {
            switch (opt) {
                case "-s": {
                    try {
                        result.mSubId = slotStringToSubId(tag, getNextArgRequired());
                        if (!SubscriptionManager.isValidSubscriptionId(result.mSubId)) {
                            errPw.println(tag + "No valid subscription found.");
                            return null;
                        }

                    } catch (IllegalArgumentException e) {
                        // Missing slot id
                        errPw.println(tag + "SLOT_ID expected after -s.");
                        return null;
                    }
                    break;
                }
                case "-p": {
                    if (allowOptionPersistent) {
                        result.mPersistent = true;
                    } else {
                        errPw.println(tag + "Unexpected option " + opt);
                        return null;
                    }
                    break;
                }
                default: {
                    errPw.println(tag + "Unknown option " + opt);
                    return null;
                }
            }
        }
        return result;
    }

    private int slotStringToSubId(String tag, String slotString) {
        int slotId = -1;
        try {
            slotId = Integer.parseInt(slotString);
        } catch (NumberFormatException e) {
            getErrPrintWriter().println(tag + slotString + " is not a valid number for SLOT_ID.");
            return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        }

        if (!SubscriptionManager.isValidPhoneId(slotId)) {
            getErrPrintWriter().println(tag + slotString + " is not a valid SLOT_ID.");
            return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        }

        Phone phone = PhoneFactory.getPhone(slotId);
        if (phone == null) {
            getErrPrintWriter().println(tag + "No subscription found in slot " + slotId + ".");
            return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        }
        return phone.getSubId();
    }

    private boolean checkShellUid() {
        // adb can run as root or as shell, depending on whether the device is rooted.
        return Binder.getCallingUid() == Process.SHELL_UID
                || Binder.getCallingUid() == Process.ROOT_UID;
    }

    private int handleCcCommand() {
        // Verify that the user is allowed to run the command. Only allowed in rooted device in a
        // non user build.
        if (Binder.getCallingUid() != Process.ROOT_UID || TelephonyUtils.IS_USER) {
            getErrPrintWriter().println("cc: Permission denied.");
            return -1;
        }

        String arg = getNextArg();
        if (arg == null) {
            onHelpCc();
            return 0;
        }

        switch (arg) {
            case CC_GET_VALUE: {
                return handleCcGetValue();
            }
            case CC_SET_VALUE: {
                return handleCcSetValue();
            }
            case CC_CLEAR_VALUES: {
                return handleCcClearValues();
            }
            default: {
                getErrPrintWriter().println("cc: Unknown argument: " + arg);
            }
        }
        return -1;
    }

    // cc get-value
    private int handleCcGetValue() {
        PrintWriter errPw = getErrPrintWriter();
        String tag = CARRIER_CONFIG_SUBCOMMAND + " " + CC_GET_VALUE + ": ";
        String key = null;

        // Parse all options
        CcOptionParseResult options =  parseCcOptions(tag, false);
        if (options == null) {
            return -1;
        }

        // Get bundle containing all carrier configuration values.
        PersistableBundle bundle = mCarrierConfigManager.getConfigForSubId(options.mSubId);
        if (bundle == null) {
            errPw.println(tag + "No carrier config values found for subId " + options.mSubId + ".");
            return -1;
        }

        // Get the key.
        key = getNextArg();
        if (key != null) {
            // A key was provided. Verify if it is a valid key
            if (!bundle.containsKey(key)) {
                errPw.println(tag + key + " is not a valid key.");
                return -1;
            }

            // Print the carrier config value for key.
            getOutPrintWriter().println(ccValueToString(key, getType(tag, key, bundle), bundle));
        } else {
            // No key provided. Show all values.
            // Iterate over a sorted list of all carrier config keys and print them.
            TreeSet<String> sortedSet = new TreeSet<String>(bundle.keySet());
            for (String k : sortedSet) {
                getOutPrintWriter().println(ccValueToString(k, getType(tag, k, bundle), bundle));
            }
        }
        return 0;
    }

    // cc set-value
    private int handleCcSetValue() {
        PrintWriter errPw = getErrPrintWriter();
        String tag = CARRIER_CONFIG_SUBCOMMAND + " " + CC_SET_VALUE + ": ";

        // Parse all options
        CcOptionParseResult options =  parseCcOptions(tag, true);
        if (options == null) {
            return -1;
        }

        // Get bundle containing all current carrier configuration values.
        PersistableBundle originalValues = mCarrierConfigManager.getConfigForSubId(options.mSubId);
        if (originalValues == null) {
            errPw.println(tag + "No carrier config values found for subId " + options.mSubId + ".");
            return -1;
        }

        // Get the key.
        String key = getNextArg();
        if (key == null || key.equals("")) {
            errPw.println(tag + "KEY is missing");
            return -1;
        }

        // Verify if the key is valid
        if (!originalValues.containsKey(key)) {
            errPw.println(tag + key + " is not a valid key.");
            return -1;
        }

        // Remaining arguments is a list of new values. Add them all into an ArrayList.
        ArrayList<String> valueList = new ArrayList<String>();
        while (peekNextArg() != null) {
            valueList.add(getNextArg());
        }

        // Find the type of the carrier config value
        CcType type = getType(tag, key, originalValues);
        if (type == CcType.UNKNOWN) {
            errPw.println(tag + "ERROR: Not possible to override key with unknown type.");
            return -1;
        }

        // Create an override bundle containing the key and value that should be overriden.
        PersistableBundle overrideBundle = getOverrideBundle(tag, type, key, valueList);
        if (overrideBundle == null) {
            return -1;
        }

        // Override the value
        mCarrierConfigManager.overrideConfig(options.mSubId, overrideBundle, options.mPersistent);

        // Find bundle containing all new carrier configuration values after the override.
        PersistableBundle newValues = mCarrierConfigManager.getConfigForSubId(options.mSubId);
        if (newValues == null) {
            errPw.println(tag + "No carrier config values found for subId " + options.mSubId + ".");
            return -1;
        }

        // Print the original and new value.
        String originalValueString = ccValueToString(key, type, originalValues);
        String newValueString = ccValueToString(key, type, newValues);
        getOutPrintWriter().println("Previous value: \n" + originalValueString);
        getOutPrintWriter().println("New value: \n" + newValueString);

        return 0;
    }

    // cc clear-values
    private int handleCcClearValues() {
        PrintWriter errPw = getErrPrintWriter();
        String tag = CARRIER_CONFIG_SUBCOMMAND + " " + CC_CLEAR_VALUES + ": ";

        // Parse all options
        CcOptionParseResult options =  parseCcOptions(tag, false);
        if (options == null) {
            return -1;
        }

        // Clear all values that has previously been set.
        mCarrierConfigManager.overrideConfig(options.mSubId, null, true);
        getOutPrintWriter()
                .println("All previously set carrier config override values has been cleared");
        return 0;
    }

    private CcType getType(String tag, String key, PersistableBundle bundle) {
        // Find the type by checking the type of the current value stored in the bundle.
        Object value = bundle.get(key);

        if (CC_TYPE_MAP.containsKey(key)) {
            return CC_TYPE_MAP.get(key);
        } else if (value != null) {
            if (value instanceof Boolean) {
                return CcType.BOOLEAN;
            } else if (value instanceof Double) {
                return CcType.DOUBLE;
            } else if (value instanceof double[]) {
                return CcType.DOUBLE_ARRAY;
            } else if (value instanceof Integer) {
                return CcType.INT;
            } else if (value instanceof int[]) {
                return CcType.INT_ARRAY;
            } else if (value instanceof Long) {
                return CcType.LONG;
            } else if (value instanceof long[]) {
                return CcType.LONG_ARRAY;
            } else if (value instanceof String) {
                return CcType.STRING;
            } else if (value instanceof String[]) {
                return CcType.STRING_ARRAY;
            }
        } else {
            // Current value was null and can therefore not be used in order to find the type.
            // Check the name of the key to infer the type. This check is not needed for primitive
            // data types (boolean, double, int and long), since they can not be null.
            if (key.endsWith("double_array")) {
                return CcType.DOUBLE_ARRAY;
            }
            if (key.endsWith("int_array")) {
                return CcType.INT_ARRAY;
            }
            if (key.endsWith("long_array")) {
                return CcType.LONG_ARRAY;
            }
            if (key.endsWith("string")) {
                return CcType.STRING;
            }
            if (key.endsWith("string_array") || key.endsWith("strings")) {
                return CcType.STRING_ARRAY;
            }
        }

        // Not possible to infer the type by looking at the current value or the key.
        PrintWriter errPw = getErrPrintWriter();
        errPw.println(tag + "ERROR: " + key + " has unknown type.");
        return CcType.UNKNOWN;
    }

    private String ccValueToString(String key, CcType type, PersistableBundle bundle) {
        String result;
        StringBuilder valueString = new StringBuilder();
        String typeString = type.toString();
        Object value = bundle.get(key);

        if (value == null) {
            valueString.append("null");
        } else {
            switch (type) {
                case DOUBLE_ARRAY: {
                    // Format the string representation of the int array as value1 value2......
                    double[] valueArray = (double[]) value;
                    for (int i = 0; i < valueArray.length; i++) {
                        if (i != 0) {
                            valueString.append(" ");
                        }
                        valueString.append(valueArray[i]);
                    }
                    break;
                }
                case INT_ARRAY: {
                    // Format the string representation of the int array as value1 value2......
                    int[] valueArray = (int[]) value;
                    for (int i = 0; i < valueArray.length; i++) {
                        if (i != 0) {
                            valueString.append(" ");
                        }
                        valueString.append(valueArray[i]);
                    }
                    break;
                }
                case LONG_ARRAY: {
                    // Format the string representation of the int array as value1 value2......
                    long[] valueArray = (long[]) value;
                    for (int i = 0; i < valueArray.length; i++) {
                        if (i != 0) {
                            valueString.append(" ");
                        }
                        valueString.append(valueArray[i]);
                    }
                    break;
                }
                case STRING: {
                    valueString.append("\"" + value.toString() + "\"");
                    break;
                }
                case STRING_ARRAY: {
                    // Format the string representation of the string array as "value1" "value2"....
                    String[] valueArray = (String[]) value;
                    for (int i = 0; i < valueArray.length; i++) {
                        if (i != 0) {
                            valueString.append(" ");
                        }
                        if (valueArray[i] != null) {
                            valueString.append("\"" + valueArray[i] + "\"");
                        } else {
                            valueString.append("null");
                        }
                    }
                    break;
                }
                default: {
                    valueString.append(value.toString());
                }
            }
        }
        return String.format("%-70s %-15s %s", key, typeString, valueString);
    }

    private PersistableBundle getOverrideBundle(String tag, CcType type, String key,
            ArrayList<String> valueList) {
        PrintWriter errPw = getErrPrintWriter();
        PersistableBundle bundle = new PersistableBundle();

        // First verify that a valid number of values has been provided for the type.
        switch (type) {
            case BOOLEAN:
            case DOUBLE:
            case INT:
            case LONG: {
                if (valueList.size() != 1) {
                    errPw.println(tag + "Expected 1 value for type " + type
                            + ". Found: " + valueList.size());
                    return null;
                }
                break;
            }
            case STRING: {
                if (valueList.size() > 1) {
                    errPw.println(tag + "Expected 0 or 1 values for type " + type
                            + ". Found: " + valueList.size());
                    return null;
                }
                break;
            }
        }

        // Parse the value according to type and add it to the Bundle.
        switch (type) {
            case BOOLEAN: {
                if ("true".equalsIgnoreCase(valueList.get(0))) {
                    bundle.putBoolean(key, true);
                } else if ("false".equalsIgnoreCase(valueList.get(0))) {
                    bundle.putBoolean(key, false);
                } else {
                    errPw.println(tag + "Unable to parse " + valueList.get(0) + " as a " + type);
                    return null;
                }
                break;
            }
            case DOUBLE: {
                try {
                    bundle.putDouble(key, Double.parseDouble(valueList.get(0)));
                } catch (NumberFormatException nfe) {
                    // Not a valid double
                    errPw.println(tag + "Unable to parse " + valueList.get(0) + " as a " + type);
                    return null;
                }
                break;
            }
            case DOUBLE_ARRAY: {
                double[] valueDoubleArray = null;
                if (valueList.size() > 0) {
                    valueDoubleArray = new double[valueList.size()];
                    for (int i = 0; i < valueList.size(); i++) {
                        try {
                            valueDoubleArray[i] = Double.parseDouble(valueList.get(i));
                        } catch (NumberFormatException nfe) {
                            // Not a valid double
                            errPw.println(
                                    tag + "Unable to parse " + valueList.get(i) + " as a double.");
                            return null;
                        }
                    }
                }
                bundle.putDoubleArray(key, valueDoubleArray);
                break;
            }
            case INT: {
                try {
                    bundle.putInt(key, Integer.parseInt(valueList.get(0)));
                } catch (NumberFormatException nfe) {
                    // Not a valid integer
                    errPw.println(tag + "Unable to parse " + valueList.get(0) + " as an " + type);
                    return null;
                }
                break;
            }
            case INT_ARRAY: {
                int[] valueIntArray = null;
                if (valueList.size() > 0) {
                    valueIntArray = new int[valueList.size()];
                    for (int i = 0; i < valueList.size(); i++) {
                        try {
                            valueIntArray[i] = Integer.parseInt(valueList.get(i));
                        } catch (NumberFormatException nfe) {
                            // Not a valid integer
                            errPw.println(tag
                                    + "Unable to parse " + valueList.get(i) + " as an integer.");
                            return null;
                        }
                    }
                }
                bundle.putIntArray(key, valueIntArray);
                break;
            }
            case LONG: {
                try {
                    bundle.putLong(key, Long.parseLong(valueList.get(0)));
                } catch (NumberFormatException nfe) {
                    // Not a valid long
                    errPw.println(tag + "Unable to parse " + valueList.get(0) + " as a " + type);
                    return null;
                }
                break;
            }
            case LONG_ARRAY: {
                long[] valueLongArray = null;
                if (valueList.size() > 0) {
                    valueLongArray = new long[valueList.size()];
                    for (int i = 0; i < valueList.size(); i++) {
                        try {
                            valueLongArray[i] = Long.parseLong(valueList.get(i));
                        } catch (NumberFormatException nfe) {
                            // Not a valid long
                            errPw.println(
                                    tag + "Unable to parse " + valueList.get(i) + " as a long");
                            return null;
                        }
                    }
                }
                bundle.putLongArray(key, valueLongArray);
                break;
            }
            case STRING: {
                String value = null;
                if (valueList.size() > 0) {
                    value = valueList.get(0);
                }
                bundle.putString(key, value);
                break;
            }
            case STRING_ARRAY: {
                String[] valueStringArray = null;
                if (valueList.size() > 0) {
                    valueStringArray = new String[valueList.size()];
                    valueList.toArray(valueStringArray);
                }
                bundle.putStringArray(key, valueStringArray);
                break;
            }
        }
        return bundle;
    }

    private int handleEndBlockSuppressionCommand() {
        if (!checkShellUid()) {
            return -1;
        }

        if (BlockedNumberContract.SystemContract.getBlockSuppressionStatus(mContext).isSuppressed) {
            BlockedNumberContract.SystemContract.endBlockSuppression(mContext);
        }
        return 0;
    }
}
