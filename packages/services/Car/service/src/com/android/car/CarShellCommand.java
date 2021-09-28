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
package com.android.car;

import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationSetValue.ASSOCIATE_CURRENT_USER;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationSetValue.DISASSOCIATE_ALL_USERS;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationSetValue.DISASSOCIATE_CURRENT_USER;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_1;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_2;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_3;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_4;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.KEY_FOB;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.UiModeManager;
import android.car.Car;
import android.car.input.CarInputManager;
import android.car.input.RotaryEvent;
import android.car.user.CarUserManager;
import android.car.user.UserCreationResult;
import android.car.user.UserIdentificationAssociationResponse;
import android.car.user.UserRemovalResult;
import android.car.user.UserSwitchResult;
import android.car.userlib.HalCallback;
import android.car.userlib.UserHalHelper;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.hardware.automotive.vehicle.V2_0.CreateUserRequest;
import android.hardware.automotive.vehicle.V2_0.CreateUserStatus;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoResponse;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoResponseAction;
import android.hardware.automotive.vehicle.V2_0.RemoveUserRequest;
import android.hardware.automotive.vehicle.V2_0.SwitchUserMessageType;
import android.hardware.automotive.vehicle.V2_0.SwitchUserRequest;
import android.hardware.automotive.vehicle.V2_0.SwitchUserStatus;
import android.hardware.automotive.vehicle.V2_0.UserFlags;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociation;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationSetValue;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationValue;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationGetRequest;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationResponse;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationSetAssociation;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationSetRequest;
import android.hardware.automotive.vehicle.V2_0.UserInfo;
import android.hardware.automotive.vehicle.V2_0.UsersInfo;
import android.hardware.automotive.vehicle.V2_0.VehicleArea;
import android.os.Binder;
import android.os.Build;
import android.os.Process;
import android.os.ShellCommand;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.Log;
import android.util.SparseArray;
import android.view.KeyEvent;

import com.android.car.am.FixedActivityService;
import com.android.car.audio.CarAudioService;
import com.android.car.garagemode.GarageModeService;
import com.android.car.hal.InputHalService;
import com.android.car.hal.UserHalService;
import com.android.car.hal.VehicleHal;
import com.android.car.pm.CarPackageManagerService;
import com.android.car.systeminterface.SystemInterface;
import com.android.car.trust.CarTrustedDeviceService;
import com.android.car.user.CarUserService;
import com.android.internal.infra.AndroidFuture;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

final class CarShellCommand extends ShellCommand {

    private static final String NO_INITIAL_USER = "N/A";

    private static final String TAG = CarShellCommand.class.getSimpleName();
    private static final boolean VERBOSE = false;

    private static final String COMMAND_HELP = "-h";
    private static final String COMMAND_DAY_NIGHT_MODE = "day-night-mode";
    private static final String COMMAND_INJECT_VHAL_EVENT = "inject-vhal-event";
    private static final String COMMAND_INJECT_ERROR_EVENT = "inject-error-event";
    private static final String COMMAND_ENABLE_UXR = "enable-uxr";
    private static final String COMMAND_GARAGE_MODE = "garage-mode";
    private static final String COMMAND_GET_DO_ACTIVITIES = "get-do-activities";
    private static final String COMMAND_GET_CARPROPERTYCONFIG = "get-carpropertyconfig";
    private static final String COMMAND_GET_PROPERTY_VALUE = "get-property-value";
    private static final String COMMAND_PROJECTION_AP_TETHERING = "projection-tethering";
    private static final String COMMAND_PROJECTION_UI_MODE = "projection-ui-mode";
    private static final String COMMAND_RESUME = "resume";
    private static final String COMMAND_SUSPEND = "suspend";
    private static final String COMMAND_ENABLE_TRUSTED_DEVICE = "enable-trusted-device";
    private static final String COMMAND_REMOVE_TRUSTED_DEVICES = "remove-trusted-devices";
    private static final String COMMAND_SET_UID_TO_ZONE = "set-audio-zone-for-uid";
    private static final String COMMAND_START_FIXED_ACTIVITY_MODE = "start-fixed-activity-mode";
    private static final String COMMAND_STOP_FIXED_ACTIVITY_MODE = "stop-fixed-activity-mode";
    private static final String COMMAND_ENABLE_FEATURE = "enable-feature";
    private static final String COMMAND_DISABLE_FEATURE = "disable-feature";
    private static final String COMMAND_INJECT_KEY = "inject-key";
    private static final String COMMAND_INJECT_ROTARY = "inject-rotary";
    private static final String COMMAND_GET_INITIAL_USER_INFO = "get-initial-user-info";
    private static final String COMMAND_SWITCH_USER = "switch-user";
    private static final String COMMAND_REMOVE_USER = "remove-user";
    private static final String COMMAND_CREATE_USER = "create-user";
    private static final String COMMAND_GET_INITIAL_USER = "get-initial-user";
    private static final String COMMAND_SET_USER_ID_TO_OCCUPANT_ZONE =
            "set-occupant-zone-for-user";
    private static final String COMMAND_RESET_USER_ID_IN_OCCUPANT_ZONE =
            "reset-user-in-occupant-zone";
    private static final String COMMAND_GET_USER_AUTH_ASSOCIATION =
            "get-user-auth-association";
    private static final String COMMAND_SET_USER_AUTH_ASSOCIATION =
            "set-user-auth-association";
    private static final String COMMAND_POWER_OFF = "power-off";
    private static final String POWER_OFF_SKIP_GARAGEMODE = "--skip-garagemode";
    private static final String POWER_OFF_SHUTDOWN = "--shutdown";

    // Whitelist of commands allowed in user build. All these command should be protected with
    // a permission. K: command, V: required permission.
    // Only commands with permission already granted to shell user should be allowed.
    // Commands that can affect safety should be never allowed in user build.
    private static final ArrayMap<String, String> USER_BUILD_COMMAND_TO_PERMISSION_MAP;
    static {
        USER_BUILD_COMMAND_TO_PERMISSION_MAP = new ArrayMap<>();
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_GARAGE_MODE,
                android.Manifest.permission.DEVICE_POWER);
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_RESUME,
                android.Manifest.permission.DEVICE_POWER);
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_SUSPEND,
                android.Manifest.permission.DEVICE_POWER);
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_GET_INITIAL_USER,
                android.Manifest.permission.INTERACT_ACROSS_USERS_FULL);
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_GET_INITIAL_USER_INFO,
                android.Manifest.permission.MANAGE_USERS);
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_SWITCH_USER,
                android.Manifest.permission.MANAGE_USERS);
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_REMOVE_USER,
                android.Manifest.permission.MANAGE_USERS);
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_CREATE_USER,
                android.Manifest.permission.MANAGE_USERS);
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_GET_USER_AUTH_ASSOCIATION,
                android.Manifest.permission.MANAGE_USERS);
        USER_BUILD_COMMAND_TO_PERMISSION_MAP.put(COMMAND_SET_USER_AUTH_ASSOCIATION,
                android.Manifest.permission.MANAGE_USERS);
    }

    private static final String DEVICE_POWER_PERMISSION = "android.permission.DEVICE_POWER";

    private static final String PARAM_DAY_MODE = "day";
    private static final String PARAM_NIGHT_MODE = "night";
    private static final String PARAM_SENSOR_MODE = "sensor";
    private static final String PARAM_VEHICLE_PROPERTY_AREA_GLOBAL = "0";
    private static final String PARAM_ON_MODE = "on";
    private static final String PARAM_OFF_MODE = "off";
    private static final String PARAM_QUERY_MODE = "query";
    private static final String PARAM_REBOOT = "reboot";

    private static final int RESULT_OK = 0;
    private static final int RESULT_ERROR = -1; // Arbitrary value, any non-0 is fine

    private static final int DEFAULT_HAL_TIMEOUT_MS = 1_000;

    private static final int INVALID_USER_AUTH_TYPE_OR_VALUE = -1;

    private static final SparseArray<String> VALID_USER_AUTH_TYPES;
    private static final String VALID_USER_AUTH_TYPES_HELP;

    private static final SparseArray<String> VALID_USER_AUTH_SET_VALUES;
    private static final String VALID_USER_AUTH_SET_VALUES_HELP;

    static {
        VALID_USER_AUTH_TYPES = new SparseArray<String>(5);
        VALID_USER_AUTH_TYPES.put(KEY_FOB, UserIdentificationAssociationType.toString(KEY_FOB));
        VALID_USER_AUTH_TYPES.put(CUSTOM_1, UserIdentificationAssociationType.toString(CUSTOM_1));
        VALID_USER_AUTH_TYPES.put(CUSTOM_2, UserIdentificationAssociationType.toString(CUSTOM_2));
        VALID_USER_AUTH_TYPES.put(CUSTOM_3, UserIdentificationAssociationType.toString(CUSTOM_3));
        VALID_USER_AUTH_TYPES.put(CUSTOM_4, UserIdentificationAssociationType.toString(CUSTOM_4));
        VALID_USER_AUTH_TYPES_HELP = getHelpString("types", VALID_USER_AUTH_TYPES);

        VALID_USER_AUTH_SET_VALUES = new SparseArray<String>(3);
        VALID_USER_AUTH_SET_VALUES.put(ASSOCIATE_CURRENT_USER,
                UserIdentificationAssociationSetValue.toString(ASSOCIATE_CURRENT_USER));
        VALID_USER_AUTH_SET_VALUES.put(DISASSOCIATE_CURRENT_USER,
                UserIdentificationAssociationSetValue.toString(DISASSOCIATE_CURRENT_USER));
        VALID_USER_AUTH_SET_VALUES.put(DISASSOCIATE_ALL_USERS,
                UserIdentificationAssociationSetValue.toString(DISASSOCIATE_ALL_USERS));
        VALID_USER_AUTH_SET_VALUES_HELP = getHelpString("values", VALID_USER_AUTH_SET_VALUES);
    }

    @NonNull
    private static String getHelpString(@NonNull String name, @NonNull SparseArray<String> values) {
        StringBuilder help = new StringBuilder("Valid ").append(name).append(" are: ");
        int size = values.size();
        for (int i = 0; i < size; i++) {
            help.append(values.valueAt(i));
            if (i != size - 1) {
                help.append(", ");
            }
        }
        return help.append('.').toString();
    }

    private final Context mContext;
    private final VehicleHal mHal;
    private final CarAudioService mCarAudioService;
    private final CarPackageManagerService mCarPackageManagerService;
    private final CarProjectionService mCarProjectionService;
    private final CarPowerManagementService mCarPowerManagementService;
    private final CarTrustedDeviceService mCarTrustedDeviceService;
    private final FixedActivityService mFixedActivityService;
    private final CarFeatureController mFeatureController;
    private final CarInputService mCarInputService;
    private final CarNightService mCarNightService;
    private final SystemInterface mSystemInterface;
    private final GarageModeService mGarageModeService;
    private final CarUserService mCarUserService;
    private final CarOccupantZoneService mCarOccupantZoneService;

    CarShellCommand(Context context,
            VehicleHal hal,
            CarAudioService carAudioService,
            CarPackageManagerService carPackageManagerService,
            CarProjectionService carProjectionService,
            CarPowerManagementService carPowerManagementService,
            CarTrustedDeviceService carTrustedDeviceService,
            FixedActivityService fixedActivityService,
            CarFeatureController featureController,
            CarInputService carInputService,
            CarNightService carNightService,
            SystemInterface systemInterface,
            GarageModeService garageModeService,
            CarUserService carUserService,
            CarOccupantZoneService carOccupantZoneService) {
        mContext = context;
        mHal = hal;
        mCarAudioService = carAudioService;
        mCarPackageManagerService = carPackageManagerService;
        mCarProjectionService = carProjectionService;
        mCarPowerManagementService = carPowerManagementService;
        mCarTrustedDeviceService = carTrustedDeviceService;
        mFixedActivityService = fixedActivityService;
        mFeatureController = featureController;
        mCarInputService = carInputService;
        mCarNightService = carNightService;
        mSystemInterface = systemInterface;
        mGarageModeService = garageModeService;
        mCarUserService = carUserService;
        mCarOccupantZoneService = carOccupantZoneService;
    }

    @Override
    public int onCommand(String cmd) {
        if (cmd == null) {
            onHelp();
            return RESULT_ERROR;
        }
        ArrayList<String> argsList = new ArrayList<>();
        argsList.add(cmd);
        String arg = null;
        do {
            arg = getNextArg();
            if (arg != null) {
                argsList.add(arg);
            }
        } while (arg != null);
        String[] args = new String[argsList.size()];
        argsList.toArray(args);
        return exec(args, getOutPrintWriter());
    }

    @Override
    public void onHelp() {
        showHelp(getOutPrintWriter());
    }

    private static void showHelp(PrintWriter pw) {
        pw.println("Car service commands:");
        pw.println("\t-h");
        pw.println("\t  Print this help text.");
        pw.println("\tday-night-mode [day|night|sensor]");
        pw.println("\t  Force into day/night mode or restore to auto.");
        pw.println("\tinject-vhal-event property [zone] data(can be comma separated list) "
                + "[-t delay_time_seconds]");
        pw.println("\t  Inject a vehicle property for testing.");
        pw.println("\t  delay_time_seconds: the event timestamp is increased by certain second.");
        pw.println("\t  If not specified, it will be 0.");
        pw.println("\tinject-error-event property zone errorCode");
        pw.println("\t  Inject an error event from VHAL for testing.");
        pw.println("\tenable-uxr true|false");
        pw.println("\t  Enable/Disable UX restrictions and App blocking.");
        pw.println("\tgarage-mode [on|off|query|reboot]");
        pw.println("\t  Force into or out of garage mode, or check status.");
        pw.println("\t  With 'reboot', enter garage mode, then reboot when it completes.");
        pw.println("\tget-do-activities pkgname");
        pw.println("\t  Get Distraction Optimized activities in given package.");
        pw.println("\tget-carpropertyconfig [propertyId]");
        pw.println("\t  Get a CarPropertyConfig by Id in Hex or list all CarPropertyConfigs");
        pw.println("\tget-property-value [propertyId] [areaId]");
        pw.println("\t  Get a vehicle property value by property id in Hex and areaId");
        pw.println("\t  or list all property values for all areaId");
        pw.println("\tsuspend");
        pw.println("\t  Suspend the system to Deep Sleep.");
        pw.println("\tresume");
        pw.println("\t  Wake the system up after a 'suspend.'");
        pw.println("\tenable-trusted-device true|false");
        pw.println("\t  Enable/Disable Trusted device feature.");
        pw.println("\tremove-trusted-devices");
        pw.println("\t  Remove all trusted devices for the current foreground user.");
        pw.println("\tprojection-tethering [true|false]");
        pw.println("\t  Whether tethering should be used when creating access point for"
                + " wireless projection");
        pw.println("\t--metrics");
        pw.println("\t  When used with dumpsys, only metrics will be in the dumpsys output.");
        pw.printf("\t%s [zoneid] [uid]\n", COMMAND_SET_UID_TO_ZONE);
        pw.println("\t  Maps the audio zoneid to uid.");
        pw.println("\tstart-fixed-activity displayId packageName activityName");
        pw.println("\t  Start an Activity the specified display as fixed mode");
        pw.println("\tstop-fixed-mode displayId");
        pw.println("\t  Stop fixed Activity mode for the given display. "
                + "The Activity will not be restarted upon crash.");
        pw.println("\tenable-feature featureName");
        pw.println("\t  Enable the requested feature. Change will happen after reboot.");
        pw.println("\t  This requires root/su.");
        pw.println("\tdisable-feature featureName");
        pw.println("\t  Disable the requested feature. Change will happen after reboot");
        pw.println("\t  This requires root/su.");
        pw.println("\tinject-key [-d display] [-t down_delay_ms | -a down|up] key_code");
        pw.println("\t  inject key down and/or up event to car service");
        pw.println("\t  display: 0 for main, 1 for cluster. If not specified, it will be 0.");
        pw.println("\t  down_delay_ms: delay from down to up key event. If not specified,");
        pw.println("\t                 it will be 0");
        pw.println("\t  key_code: int key code defined in android KeyEvent");
        pw.println("\t  If -a isn't specified, both down and up will be injected.");
        pw.println("\tinject-rotary [-d display] [-i input_type] [-c clockwise]");
        pw.println("\t              [-dt delta_times_ms]");
        pw.println("\t  inject rotary input event to car service.");
        pw.println("\t  display: 0 for main, 1 for cluster. If not specified, it will be 0.");
        pw.println("\t  input_type: 10 for navigation controller input, 11 for volume");
        pw.println("\t              controller input. If not specified, it will be 10.");
        pw.println("\t  clockwise: true if the event is clockwise, false if the event is");
        pw.println("\t             counter-clockwise. If not specified, it will be false.");
        pw.println("\t  delta_times_ms: a list of delta time (current time minus event time)");
        pw.println("\t                  in descending order. If not specified, it will be 0.");

        pw.printf("\t%s <REQ_TYPE> [--hal-only] [--timeout TIMEOUT_MS]\n",
                COMMAND_GET_INITIAL_USER_INFO);
        pw.println("\t  Calls the Vehicle HAL to get the initial boot info, passing the given");
        pw.println("\t  REQ_TYPE (which could be either FIRST_BOOT, FIRST_BOOT_AFTER_OTA, ");
        pw.println("\t  COLD_BOOT, RESUME, or any numeric value that would be passed 'as-is')");
        pw.println("\t  and an optional TIMEOUT_MS to wait for the HAL response (if not set,");
        pw.println("\t  it will use a  default value).");
        pw.println("\t  The --hal-only option only calls HAL, without using CarUserService.");

        pw.printf("\t%s <USER_ID> [--hal-only] [--timeout TIMEOUT_MS]\n", COMMAND_SWITCH_USER);
        pw.println("\t  Switches to user USER_ID using the HAL integration.");
        pw.println("\t  The --hal-only option only calls HAL, without switching the user,");
        pw.println("\t  while the --timeout defines how long to wait for the HAL response.");

        pw.printf("\t%s <USER_ID> [--hal-only]\n", COMMAND_REMOVE_USER);
        pw.println("\t  Removes user with USER_ID using the HAL integration.");
        pw.println("\t  The --hal-only option only calls HAL, without removing the user,");

        pw.printf("\t%s [--hal-only] [--timeout TIMEOUT_MS] [--type TYPE] [--flags FLAGS] [NAME]\n",
                COMMAND_CREATE_USER);
        pw.println("\t  Creates a new user using the HAL integration.");
        pw.println("\t  The --hal-only uses UserManager to create the user,");
        pw.println("\t  while the --timeout defines how long to wait for the HAL response.");

        pw.printf("\t%s\n", COMMAND_GET_INITIAL_USER);
        pw.printf("\t  Gets the id of the initial user (or %s when it's not available)\n",
                NO_INITIAL_USER);

        pw.printf("\t%s [occupantZoneId] [userId]\n", COMMAND_SET_USER_ID_TO_OCCUPANT_ZONE);
        pw.println("\t  Maps the occupant zone id to user id.");
        pw.printf("\t%s [occupantZoneId]\n", COMMAND_RESET_USER_ID_IN_OCCUPANT_ZONE);
        pw.println("\t  Unmaps the user assigned to occupant zone id.");

        pw.printf("\t%s [--hal-only] [--user USER_ID] TYPE1 [..TYPE_N]\n",
                COMMAND_GET_USER_AUTH_ASSOCIATION);
        pw.println("\t  Gets the N user authentication values for the N types for the given user");
        pw.println("\t  (or current user when not specified).");
        pw.println("\t  By defautt it calls CarUserManager, but using --hal-only will call just "
                + "UserHalService.");

        pw.printf("\t%s [--hal-only] [--user USER_ID] TYPE1 VALUE1 [..TYPE_N VALUE_N]\n",
                COMMAND_SET_USER_AUTH_ASSOCIATION);
        pw.println("\t  Sets the N user authentication types with the N values for the given user");
        pw.println("\t  (or current user when not specified).");
        pw.println("\t  By defautt it calls CarUserManager, but using --hal-only will call just "
                + "UserHalService.");

        pw.printf("\t  %s\n", VALID_USER_AUTH_TYPES_HELP);
        pw.printf("\t  %s\n", VALID_USER_AUTH_SET_VALUES_HELP);

        pw.printf("\t%s [%s] [%s]\n", COMMAND_POWER_OFF, POWER_OFF_SKIP_GARAGEMODE,
                POWER_OFF_SHUTDOWN);
        pw.println("\t  Powers off the car.");
    }

    private static int showInvalidArguments(PrintWriter pw) {
        pw.println("Incorrect number of arguments.");
        showHelp(pw);
        return RESULT_ERROR;
    }

    private void runSetZoneIdForUid(String zoneString, String uidString) {
        int uid = Integer.parseInt(uidString);
        int zoneId = Integer.parseInt(zoneString);
        mCarAudioService.setZoneIdForUid(zoneId, uid);
    }

    private void runSetOccupantZoneIdForUserId(String occupantZoneIdString,
            String userIdString) {
        int userId = Integer.parseInt(userIdString);
        int occupantZoneId = Integer.parseInt(occupantZoneIdString);
        if (!mCarOccupantZoneService.assignProfileUserToOccupantZone(occupantZoneId, userId)) {
            throw new IllegalStateException("Failed to set userId " + userId + " to occupantZoneId "
                    + occupantZoneIdString);
        }
    }

    private void runResetOccupantZoneId(String occupantZoneIdString) {
        int occupantZoneId = Integer.parseInt(occupantZoneIdString);
        if (!mCarOccupantZoneService
                .assignProfileUserToOccupantZone(occupantZoneId, UserHandle.USER_NULL)) {
            throw new IllegalStateException("Failed to reset occupantZoneId "
                    + occupantZoneIdString);
        }
    }

    int exec(String[] args, PrintWriter writer) {
        String cmd = args[0];
        String requiredPermission = USER_BUILD_COMMAND_TO_PERMISSION_MAP.get(cmd);
        if (VERBOSE) {
            Log.v(TAG, "cmd: " + cmd + ", requiredPermission: " + requiredPermission);
        }
        if (Build.IS_USER && requiredPermission == null) {
            throw new SecurityException("The command " + cmd + "requires non-user build");
        }
        if (requiredPermission != null) {
            if (!ICarImpl.hasPermission(mContext, requiredPermission)) {
                throw new SecurityException("The command " + cmd + "requires permission:"
                        + requiredPermission);
            }
        }
        switch (cmd) {
            case COMMAND_HELP:
                showHelp(writer);
                break;
            case COMMAND_DAY_NIGHT_MODE: {
                String value = args.length < 2 ? "" : args[1];
                forceDayNightMode(value, writer);
                break;
            }
            case COMMAND_GARAGE_MODE: {
                String value = args.length < 2 ? "" : args[1];
                forceGarageMode(value, writer);
                break;
            }
            case COMMAND_INJECT_VHAL_EVENT:
                String zone = PARAM_VEHICLE_PROPERTY_AREA_GLOBAL;
                String data;
                int argNum = args.length;
                if (argNum < 3 || argNum > 6) {
                    return showInvalidArguments(writer);
                }
                String delayTime = args[argNum - 2].equals("-t") ?  args[argNum - 1] : "0";
                if (argNum == 4 || argNum == 6) {
                    // Zoned
                    zone = args[2];
                    data = args[3];
                } else {
                    // Global
                    data = args[2];
                }
                injectVhalEvent(args[1], zone, data, false, delayTime, writer);
                break;
            case COMMAND_INJECT_ERROR_EVENT:
                if (args.length != 4) {
                    return showInvalidArguments(writer);
                }
                String errorAreaId = args[2];
                String errorCode = args[3];
                injectVhalEvent(args[1], errorAreaId, errorCode, true, "0", writer);
                break;
            case COMMAND_ENABLE_UXR:
                if (args.length != 2) {
                    return showInvalidArguments(writer);
                }
                boolean enableBlocking = Boolean.valueOf(args[1]);
                if (mCarPackageManagerService != null) {
                    mCarPackageManagerService.setEnableActivityBlocking(enableBlocking);
                }
                break;
            case COMMAND_GET_DO_ACTIVITIES:
                if (args.length != 2) {
                    return showInvalidArguments(writer);
                }
                String pkgName = args[1].toLowerCase();
                if (mCarPackageManagerService != null) {
                    String[] doActivities =
                            mCarPackageManagerService.getDistractionOptimizedActivities(
                                    pkgName);
                    if (doActivities != null) {
                        writer.println("DO Activities for " + pkgName);
                        for (String a : doActivities) {
                            writer.println(a);
                        }
                    } else {
                        writer.println("No DO Activities for " + pkgName);
                    }
                }
                break;
            case COMMAND_GET_CARPROPERTYCONFIG:
                String propertyId = args.length < 2 ? "" : args[1];
                mHal.dumpPropertyConfigs(writer, propertyId);
                break;
            case COMMAND_GET_PROPERTY_VALUE:
                String propId = args.length < 2 ? "" : args[1];
                String areaId = args.length < 3 ? "" : args[2];
                mHal.dumpPropertyValueByCommend(writer, propId, areaId);
                break;
            case COMMAND_PROJECTION_UI_MODE:
                if (args.length != 2) {
                    return showInvalidArguments(writer);
                }
                mCarProjectionService.setUiMode(Integer.valueOf(args[1]));
                break;
            case COMMAND_PROJECTION_AP_TETHERING:
                if (args.length != 2) {
                    return showInvalidArguments(writer);
                }
                mCarProjectionService.setAccessPointTethering(Boolean.valueOf(args[1]));
                break;
            case COMMAND_RESUME:
                mCarPowerManagementService.forceSimulatedResume();
                writer.println("Resume: Simulating resuming from Deep Sleep");
                break;
            case COMMAND_SUSPEND:
                mCarPowerManagementService.forceSuspendAndMaybeReboot(false);
                writer.println("Suspend: Simulating powering down to Deep Sleep");
                break;
            case COMMAND_ENABLE_TRUSTED_DEVICE:
                if (args.length != 2) {
                    return showInvalidArguments(writer);
                }
                mCarTrustedDeviceService.getCarTrustAgentEnrollmentService()
                        .setTrustedDeviceEnrollmentEnabled(Boolean.valueOf(args[1]));
                mCarTrustedDeviceService.getCarTrustAgentUnlockService()
                        .setTrustedDeviceUnlockEnabled(Boolean.valueOf(args[1]));
                break;
            case COMMAND_REMOVE_TRUSTED_DEVICES:
                mCarTrustedDeviceService.getCarTrustAgentEnrollmentService()
                        .removeAllTrustedDevices(ActivityManager.getCurrentUser());
                break;
            case COMMAND_SET_UID_TO_ZONE:
                if (args.length != 3) {
                    return showInvalidArguments(writer);
                }
                runSetZoneIdForUid(args[1], args[2]);
                break;
            case COMMAND_SET_USER_ID_TO_OCCUPANT_ZONE:
                if (args.length != 3) {
                    return showInvalidArguments(writer);
                }
                runSetOccupantZoneIdForUserId(args[1], args[2]);
                break;
            case COMMAND_RESET_USER_ID_IN_OCCUPANT_ZONE:
                if (args.length != 2) {
                    return showInvalidArguments(writer);
                }
                runResetOccupantZoneId(args[1]);
                break;
            case COMMAND_START_FIXED_ACTIVITY_MODE:
                startFixedActivity(args, writer);
                break;
            case COMMAND_STOP_FIXED_ACTIVITY_MODE:
                stopFixedMode(args, writer);
                break;
            case COMMAND_ENABLE_FEATURE:
                if (args.length != 2) {
                    return showInvalidArguments(writer);
                }
                enableDisableFeature(args, writer, /* enable= */ true);
                break;
            case COMMAND_DISABLE_FEATURE:
                if (args.length != 2) {
                    return showInvalidArguments(writer);
                }
                enableDisableFeature(args, writer, /* enable= */ false);
                break;
            case COMMAND_INJECT_KEY:
                if (args.length < 2) {
                    return showInvalidArguments(writer);
                }
                injectKey(args, writer);
                break;
            case COMMAND_INJECT_ROTARY:
                if (args.length < 1) {
                    return showInvalidArguments(writer);
                }
                injectRotary(args, writer);
                break;
            case COMMAND_GET_INITIAL_USER_INFO:
                getInitialUserInfo(args, writer);
                break;
            case COMMAND_SWITCH_USER:
                switchUser(args, writer);
                break;
            case COMMAND_REMOVE_USER:
                removeUser(args, writer);
                break;
            case COMMAND_CREATE_USER:
                createUser(args, writer);
                break;
            case COMMAND_GET_INITIAL_USER:
                getInitialUser(writer);
                break;
            case COMMAND_GET_USER_AUTH_ASSOCIATION:
                getUserAuthAssociation(args, writer);
                break;
            case COMMAND_SET_USER_AUTH_ASSOCIATION:
                setUserAuthAssociation(args, writer);
                break;
            case COMMAND_POWER_OFF:
                powerOff(args, writer);
                break;

            default:
                writer.println("Unknown command: \"" + cmd + "\"");
                showHelp(writer);
                return RESULT_ERROR;
        }
        return RESULT_OK;
    }

    private void startFixedActivity(String[] args, PrintWriter writer) {
        if (args.length != 4) {
            writer.println("Incorrect number of arguments");
            showHelp(writer);
            return;
        }
        int displayId;
        try {
            displayId = Integer.parseInt(args[1]);
        } catch (NumberFormatException e) {
            writer.println("Wrong display id:" + args[1]);
            return;
        }
        String packageName = args[2];
        String activityName = args[3];
        Intent intent = new Intent();
        intent.setComponent(new ComponentName(packageName, activityName));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(displayId);
        if (!mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent, options,
                displayId, ActivityManager.getCurrentUser())) {
            writer.println("Failed to start");
            return;
        }
        writer.println("Succeeded");
    }

    private void stopFixedMode(String[] args, PrintWriter writer) {
        if (args.length != 2) {
            writer.println("Incorrect number of arguments");
            showHelp(writer);
            return;
        }
        int displayId;
        try {
            displayId = Integer.parseInt(args[1]);
        } catch (NumberFormatException e) {
            writer.println("Wrong display id:" + args[1]);
            return;
        }
        mFixedActivityService.stopFixedActivityMode(displayId);
    }

    private void enableDisableFeature(String[] args, PrintWriter writer, boolean enable) {
        if (Binder.getCallingUid() != Process.ROOT_UID) {
            writer.println("Only allowed to root/su");
            return;
        }
        String featureName = args[1];
        long id = Binder.clearCallingIdentity();
        // no permission check here
        int r;
        if (enable) {
            r = mFeatureController.enableFeature(featureName);
        } else {
            r = mFeatureController.disableFeature(featureName);
        }
        switch (r) {
            case Car.FEATURE_REQUEST_SUCCESS:
                if (enable) {
                    writer.println("Enabled feature:" + featureName);
                } else {
                    writer.println("Disabled feature:" + featureName);
                }
                break;
            case Car.FEATURE_REQUEST_ALREADY_IN_THE_STATE:
                if (enable) {
                    writer.println("Already enabled:" + featureName);
                } else {
                    writer.println("Already disabled:" + featureName);
                }
                break;
            case Car.FEATURE_REQUEST_MANDATORY:
                writer.println("Cannot change mandatory feature:" + featureName);
                break;
            case Car.FEATURE_REQUEST_NOT_EXISTING:
                writer.println("Non-existing feature:" + featureName);
                break;
            default:
                writer.println("Unknown error:" + r);
                break;
        }
        Binder.restoreCallingIdentity(id);
    }

    private void injectKey(String[] args, PrintWriter writer) {
        int i = 1; // 0 is command itself
        int display = InputHalService.DISPLAY_MAIN;
        int delayMs = 0;
        int keyCode = KeyEvent.KEYCODE_UNKNOWN;
        int action = -1;
        try {
            while (i < args.length) {
                switch (args[i]) {
                    case "-d":
                        i++;
                        display = Integer.parseInt(args[i]);
                        break;
                    case "-t":
                        i++;
                        delayMs = Integer.parseInt(args[i]);
                        break;
                    case "-a":
                        i++;
                        if (args[i].equalsIgnoreCase("down")) {
                            action = KeyEvent.ACTION_DOWN;
                        } else if (args[i].equalsIgnoreCase("up")) {
                            action = KeyEvent.ACTION_UP;
                        } else {
                            throw new IllegalArgumentException("Invalid action: " + args[i]);
                        }
                        break;
                    default:
                        if (keyCode != KeyEvent.KEYCODE_UNKNOWN) {
                            throw new IllegalArgumentException("key_code already set:"
                                    + keyCode);
                        }
                        keyCode = Integer.parseInt(args[i]);
                }
                i++;
            }
        } catch (Exception e) {
            writer.println("Invalid args:" + e);
            showHelp(writer);
            return;
        }
        if (keyCode == KeyEvent.KEYCODE_UNKNOWN) {
            writer.println("Missing key code or invalid keycode");
            showHelp(writer);
            return;
        }
        if (display != InputHalService.DISPLAY_MAIN
                && display != InputHalService.DISPLAY_INSTRUMENT_CLUSTER) {
            writer.println("Invalid display:" + display);
            showHelp(writer);
            return;
        }
        if (delayMs < 0) {
            writer.println("Invalid delay:" + delayMs);
            showHelp(writer);

            return;
        }
        if (action == -1) {
            injectKeyEvent(KeyEvent.ACTION_DOWN, keyCode, display);
            SystemClock.sleep(delayMs);
            injectKeyEvent(KeyEvent.ACTION_UP, keyCode, display);
        } else {
            injectKeyEvent(action, keyCode, display);
        }
        writer.println("Succeeded");
    }

    private void injectKeyEvent(int action, int keyCode, int display) {
        mCarInputService.onKeyEvent(new KeyEvent(action, keyCode), display);
    }

    private void injectRotary(String[] args, PrintWriter writer) {
        int i = 1; // 0 is command itself
        int display = InputHalService.DISPLAY_MAIN;
        int inputType = CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION;
        boolean clockwise = false;
        List<Long> deltaTimeMs = new ArrayList<>();
        try {
            while (i < args.length) {
                switch (args[i]) {
                    case "-d":
                        i++;
                        display = Integer.parseInt(args[i]);
                        break;
                    case "-i":
                        i++;
                        inputType = Integer.parseInt(args[i]);
                        break;
                    case "-c":
                        i++;
                        clockwise = Boolean.parseBoolean(args[i]);
                        break;
                    case "-dt":
                        i++;
                        while (i < args.length) {
                            deltaTimeMs.add(Long.parseLong(args[i]));
                            i++;
                        }
                        break;
                    default:
                        writer.println("Invalid option at index " + i + ": " + args[i]);
                        return;
                }
                i++;
            }
        } catch (Exception e) {
            writer.println("Invalid args:" + e);
            showHelp(writer);
            return;
        }
        if (deltaTimeMs.isEmpty()) {
            deltaTimeMs.add(0L);
        }
        for (int j = 0; j < deltaTimeMs.size(); j++) {
            if (deltaTimeMs.get(j) < 0) {
                writer.println("Delta time shouldn't be negative: " + deltaTimeMs.get(j));
                showHelp(writer);
                return;
            }
            if (j > 0 && deltaTimeMs.get(j) > deltaTimeMs.get(j - 1)) {
                writer.println("Delta times should be in descending order");
                showHelp(writer);
                return;
            }
        }
        long[] uptimeMs = new long[deltaTimeMs.size()];
        long currentUptime = SystemClock.uptimeMillis();
        for (int j = 0; j < deltaTimeMs.size(); j++) {
            uptimeMs[j] = currentUptime - deltaTimeMs.get(j);
        }
        RotaryEvent rotaryEvent = new RotaryEvent(inputType, clockwise, uptimeMs);
        mCarInputService.onRotaryEvent(rotaryEvent, display);
        writer.println("Succeeded in injecting: " + rotaryEvent);
    }

    private void getInitialUserInfo(String[] args, PrintWriter writer) {
        if (args.length < 2) {
            writer.println("Insufficient number of args");
            return;
        }

        // Gets the request type
        String typeArg = args[1];
        int requestType = UserHalHelper.parseInitialUserInfoRequestType(typeArg);
        boolean halOnly = false;

        int timeout = DEFAULT_HAL_TIMEOUT_MS;
        for (int i = 2; i < args.length; i++) {
            String arg = args[i];
            switch (arg) {
                case "--timeout":
                    timeout = Integer.parseInt(args[++i]);
                    break;
                case "--hal-only":
                    halOnly = true;
                    break;
                default:
                    writer.println("Invalid option at index " + i + ": " + arg);
                    return;

            }
        }

        Log.d(TAG, "handleGetInitialUserInfo(): type=" + requestType + " (" + typeArg
                + "), timeout=" + timeout);

        CountDownLatch latch = new CountDownLatch(1);
        HalCallback<InitialUserInfoResponse> callback = (status, resp) -> {
            try {
                Log.d(TAG, "GetUserInfoResponse: status=" + status + ", resp=" + resp);
                writer.printf("Call status: %s\n",
                        UserHalHelper.halCallbackStatusToString(status));
                if (status != HalCallback.STATUS_OK) {
                    return;
                }
                writer.printf("Request id: %d\n", resp.requestId);
                writer.printf("Action: %s\n",
                        InitialUserInfoResponseAction.toString(resp.action));
                if (!TextUtils.isEmpty(resp.userNameToCreate)) {
                    writer.printf("User name: %s\n", resp.userNameToCreate);
                }
                if (resp.userToSwitchOrCreate.userId != UserHandle.USER_NULL) {
                    writer.printf("User id: %d\n", resp.userToSwitchOrCreate.userId);
                }
                if (resp.userToSwitchOrCreate.flags != UserFlags.NONE) {
                    writer.printf("User flags: %s\n",
                            UserHalHelper.userFlagsToString(resp.userToSwitchOrCreate.flags));
                }
                if (!TextUtils.isEmpty(resp.userLocales)) {
                    writer.printf("User locales: %s\n", resp.userLocales);
                }
            } finally {
                latch.countDown();
            }
        };
        if (halOnly) {
            UsersInfo usersInfo = generateUsersInfo();
            mHal.getUserHal().getInitialUserInfo(requestType, timeout, usersInfo, callback);
        } else {
            mCarUserService.getInitialUserInfo(requestType, callback);
        }
        waitForHal(writer, latch, timeout);
    }

    private UsersInfo generateUsersInfo() {
        return UserHalHelper.newUsersInfo(UserManager.get(mContext));
    }

    private int getUserHalFlags(@UserIdInt int userId) {
        return UserHalHelper.getFlags(UserManager.get(mContext), userId);
    }

    private static void waitForHal(PrintWriter writer, CountDownLatch latch, int timeoutMs) {
        try {
            if (!latch.await(timeoutMs, TimeUnit.MILLISECONDS)) {
                writer.printf("HAL didn't respond in %dms\n", timeoutMs);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            writer.println("Interrupted waiting for HAL");
        }
        return;
    }

    private void switchUser(String[] args, PrintWriter writer) {
        if (args.length < 2) {
            writer.println("Insufficient number of args");
            return;
        }

        int targetUserId = Integer.parseInt(args[1]);
        int timeout = DEFAULT_HAL_TIMEOUT_MS;
        boolean halOnly = false;

        for (int i = 2; i < args.length; i++) {
            String arg = args[i];
            switch (arg) {
                case "--timeout":
                    timeout = Integer.parseInt(args[++i]);
                    break;
                case "--hal-only":
                    halOnly = true;
                    break;
                default:
                    writer.println("Invalid option at index " + i + ": " + arg);
                    return;
            }
        }

        Log.d(TAG, "switchUser(): target=" + targetUserId + ", halOnly=" + halOnly
                + ", timeout=" + timeout);

        if (halOnly) {
            CountDownLatch latch = new CountDownLatch(1);
            UserHalService userHal = mHal.getUserHal();
            UserInfo targetUserInfo = new UserInfo();
            targetUserInfo.userId = targetUserId;
            targetUserInfo.flags = getUserHalFlags(targetUserId);

            SwitchUserRequest request = new SwitchUserRequest();
            request.targetUser = targetUserInfo;
            request.usersInfo = generateUsersInfo();

            userHal.switchUser(request, timeout, (status, resp) -> {
                try {
                    Log.d(TAG, "SwitchUserResponse: status=" + status + ", resp=" + resp);
                    writer.printf("Call Status: %s\n",
                            UserHalHelper.halCallbackStatusToString(status));
                    if (status != HalCallback.STATUS_OK) {
                        return;
                    }
                    writer.printf("Request id: %d\n", resp.requestId);
                    writer.printf("Message type: %s\n",
                            SwitchUserMessageType.toString(resp.messageType));
                    writer.printf("Switch Status: %s\n", SwitchUserStatus.toString(resp.status));
                    String errorMessage = resp.errorMessage;
                    if (!TextUtils.isEmpty(errorMessage)) {
                        writer.printf("Error message: %s", errorMessage);
                    }
                    // If HAL returned OK, make a "post-switch" call to the HAL indicating an
                    // Android error. This is to "rollback" the HAL switch.
                    if (status == HalCallback.STATUS_OK
                            && resp.status == SwitchUserStatus.SUCCESS) {
                        userHal.postSwitchResponse(request);
                    }
                } finally {
                    latch.countDown();
                }
            });
            waitForHal(writer, latch, timeout);
            return;
        }
        CarUserManager carUserManager = getCarUserManager(mContext);
        AndroidFuture<UserSwitchResult> future = carUserManager.switchUser(targetUserId);
        UserSwitchResult result = waitForFuture(writer, future, timeout);
        if (result == null) return;
        writer.printf("UserSwitchResult: status=%s",
                UserSwitchResult.statusToString(result.getStatus()));
        String msg = result.getErrorMessage();
        if (!TextUtils.isEmpty(msg)) {
            writer.printf(", errorMessage=%s", msg);
        }
        writer.println();
    }

    private void createUser(String[] args, PrintWriter writer) {
        int timeout = DEFAULT_HAL_TIMEOUT_MS;
        int flags = 0;
        boolean halOnly = false;
        String name = null;
        String userType = null;

        for (int i = 1; i < args.length; i++) {
            String arg = args[i];
            switch (arg) {
                case "--timeout":
                    timeout = Integer.parseInt(args[++i]);
                    break;
                case "--hal-only":
                    halOnly = true;
                    break;
                case "--flags":
                    flags = Integer.parseInt(args[++i]);
                    break;
                case "--type":
                    userType = args[++i];
                    break;
                default:
                    if (name != null) {
                        writer.println("Invalid option at index " + i + ": " + arg);
                        return;
                    }
                    name = arg;
            }
        }

        if (userType == null) {
            userType = android.content.pm.UserInfo.getDefaultUserType(flags);
        }

        Log.d(TAG, "createUser(): name=" + name + ", userType=" + userType
                + ", flags=" + UserHalHelper.userFlagsToString(flags)
                + ", halOnly=" + halOnly + ", timeout=" + timeout);

        if (!halOnly) {
            CarUserManager carUserManager = getCarUserManager(mContext);
            AndroidFuture<UserCreationResult> future = carUserManager
                    .createUser(name, userType, flags);

            UserCreationResult result = waitForFuture(writer, future, timeout);
            if (result == null) return;

            android.content.pm.UserInfo user = result.getUser();
            writer.printf("UserCreationResult: status=%s, user=%s",
                    UserCreationResult.statusToString(result.getStatus()),
                    user == null ? "N/A" : user.toFullString());
            String msg = result.getErrorMessage();
            if (!TextUtils.isEmpty(msg)) {
                writer.printf(", errorMessage=%s", msg);
            }
            writer.println();
            return;
        }

        CountDownLatch latch = new CountDownLatch(1);
        UserHalService userHal = mHal.getUserHal();

        CreateUserRequest request = new CreateUserRequest();

        UserManager um = UserManager.get(mContext);
        android.content.pm.UserInfo newUser = um.createUser(name, userType, flags);
        if (newUser == null) {
            writer.printf("Failed to create user");
            return;
        }
        writer.printf("New user: %s\n", newUser.toFullString());
        Log.i(TAG, "Created new user: " + newUser.toFullString());

        request.newUserInfo.userId = newUser.id;
        request.newUserInfo.flags = UserHalHelper.convertFlags(newUser);

        request.usersInfo = generateUsersInfo();

        AtomicBoolean halOk = new AtomicBoolean(false);
        try {
            userHal.createUser(request, timeout, (status, resp) -> {
                Log.d(TAG, "CreateUserResponse: status=" + status + ", resp=" + resp);
                writer.printf("Call Status: %s\n",
                        UserHalHelper.halCallbackStatusToString(status));
                if (status == HalCallback.STATUS_OK) {
                    halOk.set(resp.status == CreateUserStatus.SUCCESS);
                    writer.printf("Request id: %d\n", resp.requestId);
                    writer.printf("Create Status: %s\n", CreateUserStatus.toString(resp.status));
                    String errorMessage = resp.errorMessage;
                    if (!TextUtils.isEmpty(errorMessage)) {
                        writer.printf("Error message: %s", errorMessage);
                    }
                }
                latch.countDown();
            });
            waitForHal(writer, latch, timeout);
        } catch (Exception e) {
            writer.printf("HAL failed: %s\n", e);
        } finally {
            if (!halOk.get()) {
                writer.printf("Removing user %d due to HAL failure\n", newUser.id);
                boolean removed = um.removeUser(newUser.id);
                writer.printf("User removed: %b\n", removed);
            }
        }
    }

    private void removeUser(String[] args, PrintWriter writer) {
        if (args.length < 2) {
            writer.println("Insufficient number of args");
            return;
        }

        int userId = Integer.parseInt(args[1]);
        boolean halOnly = false;

        for (int i = 2; i < args.length; i++) {
            String arg = args[i];
            switch (arg) {
                case "--hal-only":
                    halOnly = true;
                    break;
                default:
                    writer.println("Invalid option at index " + i + ": " + arg);
                    return;
            }
        }

        Log.d(TAG, "handleRemoveUser(): User to remove=" + userId + ", halOnly=" + halOnly);

        if (halOnly) {
            UserHalService userHal = mHal.getUserHal();
            UsersInfo usersInfo = generateUsersInfo();
            UserInfo userInfo = new UserInfo();
            userInfo.userId = userId;
            userInfo.flags = getUserHalFlags(userId);

            RemoveUserRequest request = new RemoveUserRequest();
            request.removedUserInfo = userInfo;
            request.usersInfo = usersInfo;

            userHal.removeUser(request);
            writer.printf("User removal sent for HAL only.\n");
            return;
        }

        CarUserManager carUserManager = getCarUserManager(mContext);
        UserRemovalResult result = carUserManager.removeUser(userId);
        if (result == null) return;
        writer.printf("UserRemovalResult: status = %s\n",
                UserRemovalResult.statusToString(result.getStatus()));
    }

    private static <T> T waitForFuture(@NonNull PrintWriter writer,
            @NonNull AndroidFuture<T> future, int timeoutMs) {
        T result = null;
        try {
            result = future.get(timeoutMs, TimeUnit.MILLISECONDS);
            if (result == null) {
                writer.printf("Service didn't respond in %d ms", timeoutMs);
            }
        } catch (Exception e) {
            writer.printf("Exception getting future: %s",  e);
        }
        return result;
    }

    private void getInitialUser(PrintWriter writer) {
        android.content.pm.UserInfo user = mCarUserService.getInitialUser();
        writer.println(user == null ? NO_INITIAL_USER : user.id);
    }

    private void getUserAuthAssociation(String[] args, PrintWriter writer) {
        if (args.length < 2) {
            writer.println("invalid usage, must pass at least 1 argument");
            return;
        }

        boolean halOnly = false;
        int userId = UserHandle.USER_CURRENT;

        UserIdentificationGetRequest request = new UserIdentificationGetRequest();
        for (int i = 1; i < args.length; i++) {
            String arg = args[i];
            switch (arg) {
                case "--user":
                    try {
                        userId = Integer.parseInt(args[++i]);
                    } catch (Exception e) {
                        writer.printf("Invalid user id at index %d (from %s): %s\n", i + 1,
                                Arrays.toString(args), arg);
                    }
                    break;
                case "--hal-only":
                    halOnly = true;
                    break;
                default:
                    int type = parseAuthArg(VALID_USER_AUTH_TYPES, arg);
                    if (type == INVALID_USER_AUTH_TYPE_OR_VALUE) {
                        writer.printf("Invalid type at index %d (from %s): %s. %s\n", i + 1,
                                Arrays.toString(args), arg, VALID_USER_AUTH_TYPES_HELP);
                        return;
                    }
                    request.associationTypes.add(type);
            }

        }
        if (userId == UserHandle.USER_CURRENT) {
            userId = ActivityManager.getCurrentUser();
        }
        int requestSize = request.associationTypes.size();
        if (halOnly) {
            request.numberAssociationTypes = requestSize;
            request.userInfo.userId = userId;
            request.userInfo.flags = getUserHalFlags(userId);

            Log.d(TAG, "getUserAuthAssociation(): user=" + userId + ", halOnly=" + halOnly
                    + ", request=" + request);
            UserIdentificationResponse response = mHal.getUserHal().getUserAssociation(request);
            Log.d(TAG, "getUserAuthAssociation(): response=" + response);
            showResponse(writer, response);
            return;
        }

        CarUserManager carUserManager = getCarUserManager(writer, userId);
        int[] types = new int[requestSize];
        for (int i = 0; i < requestSize; i++) {
            types[i] = request.associationTypes.get(i);
        }
        UserIdentificationAssociationResponse response = carUserManager
                .getUserIdentificationAssociation(types);
        showResponse(writer, response);
    }

    private CarUserManager getCarUserManager(@NonNull PrintWriter writer, @UserIdInt int userId) {
        Context context;
        if (userId == mContext.getUserId()) {
            context = mContext;
        } else {
            context = mContext.createContextAsUser(UserHandle.of(userId), /* flags= */ 0);
        }
        int actualUserId = Binder.getCallingUid();
        if (actualUserId != userId) {
            writer.printf("Emulating call for user id %d, but caller's user id is %d, so that's "
                    + "what CarUserService will use when calling HAL.\n", userId, actualUserId);
        }

        return getCarUserManager(context);
    }

    private CarUserManager getCarUserManager(@NonNull Context context) {
        Car car = Car.createCar(context);
        CarUserManager carUserManager = (CarUserManager) car.getCarManager(Car.CAR_USER_SERVICE);
        return carUserManager;
    }

    private void showResponse(@NonNull PrintWriter writer,
            @NonNull UserIdentificationResponse response) {
        if (response == null) {
            writer.println("null response");
            return;
        }

        if (!TextUtils.isEmpty(response.errorMessage)) {
            writer.printf("Error message: %s\n", response.errorMessage);
        }
        int numberAssociations = response.associations.size();
        writer.printf("%d associations:\n", numberAssociations);
        for (int i = 0; i < numberAssociations; i++) {
            UserIdentificationAssociation association = response.associations.get(i);
            writer.printf("  %s\n", association);
        }
    }

    private void showResponse(@NonNull PrintWriter writer,
            @NonNull UserIdentificationAssociationResponse response) {
        if (response == null) {
            writer.println("null response");
            return;
        }
        if (!response.isSuccess()) {
            writer.printf("failed response: %s\n", response);
            return;
        }
        String errorMessage = response.getErrorMessage();
        if (!TextUtils.isEmpty(errorMessage)) {
            writer.printf("Error message: %s\n", errorMessage);
        }
        int[] values = response.getValues();
        if (values == null) {
            writer.printf("no associations on %s\n", response);
            return;
        }
        writer.printf("%d associations:\n", values.length);
        for (int i = 0; i < values.length; i++) {
            writer.printf("  %s\n", UserIdentificationAssociationValue.toString(values[i]));
        }
    }

    private void setUserAuthAssociation(String[] args, PrintWriter writer) {
        if (args.length < 3) {
            writer.println("invalid usage, must pass at least 4 arguments");
            return;
        }

        boolean halOnly = false;
        int timeout = DEFAULT_HAL_TIMEOUT_MS;
        int userId = UserHandle.USER_CURRENT;

        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        for (int i = 1; i < args.length; i++) {
            String arg = args[i];
            switch (arg) {
                case "--user":
                    try {
                        userId = Integer.parseInt(args[++i]);
                    } catch (Exception e) {
                        writer.printf("Invalid user id at index %d (from %s): %s\n", i + 1,
                                Arrays.toString(args), arg);
                    }
                    break;
                case "--hal-only":
                    halOnly = true;
                    break;
                case "--timeout":
                    timeout = Integer.parseInt(args[++i]);
                    break;
                default:
                    UserIdentificationSetAssociation association =
                            new UserIdentificationSetAssociation();
                    association.type = parseAuthArg(VALID_USER_AUTH_TYPES, arg);
                    if (association.type == INVALID_USER_AUTH_TYPE_OR_VALUE) {
                        writer.printf("Invalid type at index %d (from %s): %s. %s\n", i + 1,
                                Arrays.toString(args), arg, VALID_USER_AUTH_TYPES_HELP);
                        return;
                    }
                    association.value = parseAuthArg(VALID_USER_AUTH_SET_VALUES, args[++i]);
                    if (association.value == INVALID_USER_AUTH_TYPE_OR_VALUE) {
                        writer.printf("Invalid value at index %d (from %s): %s. %s\n", i + 1,
                                Arrays.toString(args), arg, VALID_USER_AUTH_SET_VALUES_HELP);
                        return;
                    }
                    request.associations.add(association);
            }

        }
        if (userId == UserHandle.USER_CURRENT) {
            userId = ActivityManager.getCurrentUser();
        }
        int requestSize = request.associations.size();
        if (halOnly) {
            request.numberAssociations = requestSize;
            request.userInfo.userId = userId;
            request.userInfo.flags = getUserHalFlags(userId);

            Log.d(TAG, "setUserAuthAssociation(): user=" + userId + ", halOnly=" + halOnly
                    + ", request=" + request);
            CountDownLatch latch = new CountDownLatch(1);
            mHal.getUserHal().setUserAssociation(timeout, request, (status, response) -> {
                Log.d(TAG, "setUserAuthAssociation(): response=" + response);
                try {
                    showResponse(writer, response);
                } finally {
                    latch.countDown();
                }
            });
            waitForHal(writer, latch, timeout);
            return;
        }
        CarUserManager carUserManager = getCarUserManager(writer, userId);
        int[] types = new int[requestSize];
        int[] values = new int[requestSize];
        for (int i = 0; i < requestSize; i++) {
            UserIdentificationSetAssociation association = request.associations.get(i);
            types[i] = association.type;
            values[i] = association.value;
        }
        AndroidFuture<UserIdentificationAssociationResponse> future = carUserManager
                .setUserIdentificationAssociation(types, values);
        UserIdentificationAssociationResponse response = waitForFuture(writer, future, timeout);
        if (response != null) {
            showResponse(writer, response);
        }
    }

    private static int parseAuthArg(@NonNull SparseArray<String> types, @NonNull String type) {
        for (int i = 0; i < types.size(); i++) {
            if (types.valueAt(i).equals(type)) {
                return types.keyAt(i);
            }
        }
        return INVALID_USER_AUTH_TYPE_OR_VALUE;
    }

    private void forceDayNightMode(String arg, PrintWriter writer) {
        int mode;
        switch (arg) {
            case PARAM_DAY_MODE:
                mode = CarNightService.FORCED_DAY_MODE;
                break;
            case PARAM_NIGHT_MODE:
                mode = CarNightService.FORCED_NIGHT_MODE;
                break;
            case PARAM_SENSOR_MODE:
                mode = CarNightService.FORCED_SENSOR_MODE;
                break;
            default:
                writer.println("Unknown value. Valid argument: " + PARAM_DAY_MODE + "|"
                        + PARAM_NIGHT_MODE + "|" + PARAM_SENSOR_MODE);
                return;
        }
        int current = mCarNightService.forceDayNightMode(mode);
        String currentMode = null;
        switch (current) {
            case UiModeManager.MODE_NIGHT_AUTO:
                currentMode = PARAM_SENSOR_MODE;
                break;
            case UiModeManager.MODE_NIGHT_YES:
                currentMode = PARAM_NIGHT_MODE;
                break;
            case UiModeManager.MODE_NIGHT_NO:
                currentMode = PARAM_DAY_MODE;
                break;
        }
        writer.println("DayNightMode changed to: " + currentMode);
    }

    private void forceGarageMode(String arg, PrintWriter writer) {
        switch (arg) {
            case PARAM_ON_MODE:
                mSystemInterface.setDisplayState(false);
                mGarageModeService.forceStartGarageMode();
                writer.println("Garage mode: " + mGarageModeService.isGarageModeActive());
                break;
            case PARAM_OFF_MODE:
                mSystemInterface.setDisplayState(true);
                mGarageModeService.stopAndResetGarageMode();
                writer.println("Garage mode: " + mGarageModeService.isGarageModeActive());
                break;
            case PARAM_QUERY_MODE:
                mGarageModeService.dump(writer);
                break;
            case PARAM_REBOOT:
                mCarPowerManagementService.forceSuspendAndMaybeReboot(true);
                writer.println("Entering Garage Mode. Will reboot when it completes.");
                break;
            default:
                writer.println("Unknown value. Valid argument: " + PARAM_ON_MODE + "|"
                        + PARAM_OFF_MODE + "|" + PARAM_QUERY_MODE + "|" + PARAM_REBOOT);
        }
    }

    private void powerOff(String[] args, PrintWriter writer) {
        int index = 1;
        boolean skipGarageMode = false;
        boolean shutdown = false;
        while (index < args.length) {
            switch (args[index]) {
                case POWER_OFF_SKIP_GARAGEMODE:
                    skipGarageMode = true;
                    break;
                case POWER_OFF_SHUTDOWN:
                    shutdown = true;
                    break;
                default:
                    writer.printf("Not supported option: %s\n", args[index]);
                    return;
            }
            index++;
        }
        mCarPowerManagementService.powerOffFromCommand(skipGarageMode, shutdown);
    }

    /**
     * Inject a fake  VHAL event
     *
     * @param property the Vehicle property Id as defined in the HAL
     * @param zone     Zone that this event services
     * @param isErrorEvent indicates the type of event
     * @param value    Data value of the event
     * @param delayTime the event timestamp is increased by delayTime
     * @param writer   PrintWriter
     */
    private void injectVhalEvent(String property, String zone, String value,
            boolean isErrorEvent, String delayTime, PrintWriter writer) {
        if (zone != null && (zone.equalsIgnoreCase(PARAM_VEHICLE_PROPERTY_AREA_GLOBAL))) {
            if (!isPropertyAreaTypeGlobal(property)) {
                writer.println("Property area type inconsistent with given zone");
                return;
            }
        }
        try {
            if (isErrorEvent) {
                mHal.injectOnPropertySetError(property, zone, value);
            } else {
                mHal.injectVhalEvent(property, zone, value, delayTime);
            }
        } catch (NumberFormatException e) {
            writer.println("Invalid property Id zone Id or value" + e);
            showHelp(writer);
        }
    }

    // Check if the given property is global
    private static boolean isPropertyAreaTypeGlobal(@Nullable String property) {
        if (property == null) {
            return false;
        }
        return (Integer.decode(property) & VehicleArea.MASK) == VehicleArea.GLOBAL;
    }
}
