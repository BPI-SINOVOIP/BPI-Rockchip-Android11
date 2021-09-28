/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.cellbroadcastservice;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;

import static com.android.cellbroadcastservice.CbSendMessageCalculator.SEND_MESSAGE_ACTION_AMBIGUOUS;
import static com.android.cellbroadcastservice.CbSendMessageCalculator.SEND_MESSAGE_ACTION_NO_COORDINATES;
import static com.android.cellbroadcastservice.CbSendMessageCalculator.SEND_MESSAGE_ACTION_SEND;
import static com.android.cellbroadcastservice.CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_ERROR__TYPE__UNEXPECTED_CDMA_MESSAGE_TYPE_FROM_FWK;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.database.Cursor;
import android.location.Location;
import android.location.LocationManager;
import android.location.LocationRequest;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.HandlerExecutor;
import android.os.Looper;
import android.os.Message;
import android.os.Process;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.provider.Telephony;
import android.provider.Telephony.CellBroadcasts;
import android.telephony.CbGeoUtils.Geometry;
import android.telephony.CbGeoUtils.LatLng;
import android.telephony.CellBroadcastIntents;
import android.telephony.SmsCbMessage;
import android.telephony.SubscriptionManager;
import android.telephony.cdma.CdmaSmsCbProgramData;
import android.text.TextUtils;
import android.util.LocalLog;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;

import java.io.File;
import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * Dispatch new Cell Broadcasts to receivers. Acquires a private wakelock until the broadcast
 * completes and our result receiver is called.
 */
public class CellBroadcastHandler extends WakeLockStateMachine {
    private static final String TAG = "CellBroadcastHandler";

    private static final boolean VDBG = false;

    /**
     * CellBroadcast apex name
     */
    private static final String CB_APEX_NAME = "com.android.cellbroadcast";

    /**
     * Path where CB apex is mounted (/apex/com.android.cellbroadcast)
     */
    private static final String CB_APEX_PATH = new File("/apex", CB_APEX_NAME).getAbsolutePath();

    private static final String EXTRA_MESSAGE = "message";

    /**
     * To disable cell broadcast duplicate detection for debugging purposes
     * <code>adb shell am broadcast -a com.android.cellbroadcastservice.action.DUPLICATE_DETECTION
     * --ez enable false</code>
     *
     * To enable cell broadcast duplicate detection for debugging purposes
     * <code>adb shell am broadcast -a com.android.cellbroadcastservice.action.DUPLICATE_DETECTION
     * --ez enable true</code>
     */
    private static final String ACTION_DUPLICATE_DETECTION =
            "com.android.cellbroadcastservice.action.DUPLICATE_DETECTION";

    /**
     * The extra for cell broadcast duplicate detection enable/disable
     */
    private static final String EXTRA_ENABLE = "enable";

    private final LocalLog mLocalLog = new LocalLog(100);

    private static final boolean IS_DEBUGGABLE = SystemProperties.getInt("ro.debuggable", 0) == 1;

    /** Uses to request the location update. */
    private final LocationRequester mLocationRequester;
    private @NonNull final CbSendMessageCalculatorFactory mCbSendMessageCalculatorFactory;

    /** Timestamp of last airplane mode on */
    protected long mLastAirplaneModeTime = 0;

    /** Resource cache */
    private final Map<Integer, Resources> mResourcesCache = new HashMap<>();

    /** Whether performing duplicate detection or not. Note this is for debugging purposes only. */
    private boolean mEnableDuplicateDetection = true;

    /**
     * Service category equivalent map. The key is the GSM service category, the value is the CDMA
     * service category.
     */
    private final Map<Integer, Integer> mServiceCategoryCrossRATMap;

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            switch (intent.getAction()) {
                case Intent.ACTION_AIRPLANE_MODE_CHANGED:
                    boolean airplaneModeOn = intent.getBooleanExtra("state", false);
                    if (airplaneModeOn) {
                        mLastAirplaneModeTime = System.currentTimeMillis();
                        log("Airplane mode on.");
                    }
                    break;
                case ACTION_DUPLICATE_DETECTION:
                    mEnableDuplicateDetection = intent.getBooleanExtra(EXTRA_ENABLE,
                            true);
                    log("Duplicate detection " + (mEnableDuplicateDetection
                            ? "enabled" : "disabled"));
                    break;
                default:
                    log("Unhandled broadcast " + intent.getAction());
            }
        }
    };

    private CellBroadcastHandler(Context context) {
        this(CellBroadcastHandler.class.getSimpleName(), context, Looper.myLooper(),
                new CbSendMessageCalculatorFactory());
    }

    /**
     * Allows tests to inject new calculators
     *
     * @hide
     */
    @VisibleForTesting
    public static class CbSendMessageCalculatorFactory {
        public CbSendMessageCalculatorFactory() {
        }

        /**
         * Creates new calculator
         * @param context context
         * @param fences the geo fences to use in the calculator
         * @return a new instance of the calculator
         */
        public CbSendMessageCalculator createNew(@NonNull final Context context,
                @NonNull final List<android.telephony.CbGeoUtils.Geometry> fences) {
            return new CbSendMessageCalculator(context, fences);
        }
    }

    @VisibleForTesting
    public CellBroadcastHandler(String debugTag, Context context, Looper looper,
            @NonNull final CbSendMessageCalculatorFactory cbSendMessageCalculatorFactory) {
        super(debugTag, context, looper);
        mCbSendMessageCalculatorFactory = cbSendMessageCalculatorFactory;
        mLocationRequester = new LocationRequester(
                context,
                (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE),
                getHandler());

        // Adding GSM / CDMA service category mapping.
        mServiceCategoryCrossRATMap = Stream.of(new Integer[][] {
                // Presidential alert
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT},

                // Extreme alert
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_OBSERVED,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_OBSERVED_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_LIKELY,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_LIKELY_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT},

                // Severe alert
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_OBSERVED,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_OBSERVED_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_LIKELY,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_LIKELY_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_IMMEDIATE_OBSERVED,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_IMMEDIATE_OBSERVED_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_IMMEDIATE_LIKELY,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_IMMEDIATE_LIKELY_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_OBSERVED,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_OBSERVED_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_LIKELY,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_LIKELY_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT},

                // Amber alert
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY},

                // Monthly test alert
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_REQUIRED_MONTHLY_TEST,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_TEST_MESSAGE},
                { SmsCbConstants.MESSAGE_ID_CMAS_ALERT_REQUIRED_MONTHLY_TEST_LANGUAGE,
                        CdmaSmsCbProgramData.CATEGORY_CMAS_TEST_MESSAGE},
        }).collect(Collectors.toMap(data -> data[0], data -> data[1]));

        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(Intent.ACTION_AIRPLANE_MODE_CHANGED);
        if (IS_DEBUGGABLE) {
            intentFilter.addAction(ACTION_DUPLICATE_DETECTION);
        }

        mContext.registerReceiver(mReceiver, intentFilter);
    }

    public void cleanup() {
        if (DBG) log("CellBroadcastHandler cleanup");
        mContext.unregisterReceiver(mReceiver);
    }

    /**
     * Create a new CellBroadcastHandler.
     * @param context the context to use for dispatching Intents
     * @return the new handler
     */
    public static CellBroadcastHandler makeCellBroadcastHandler(Context context) {
        CellBroadcastHandler handler = new CellBroadcastHandler(context);
        handler.start();
        return handler;
    }

    /**
     * Handle Cell Broadcast messages from {@code CdmaInboundSmsHandler}.
     * 3GPP-format Cell Broadcast messages sent from radio are handled in the subclass.
     *
     * @param message the message to process
     * @return true if need to wait for geo-fencing or an ordered broadcast was sent.
     */
    @Override
    protected boolean handleSmsMessage(Message message) {
        if (message.obj instanceof SmsCbMessage) {
            if (!isDuplicate((SmsCbMessage) message.obj)) {
                handleBroadcastSms((SmsCbMessage) message.obj);
                return true;
            } else {
                CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_FILTERED,
                        CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__TYPE__CDMA,
                        CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__FILTER__DUPLICATE_MESSAGE);
            }
            return false;
        } else {
            final String errorMessage =
                    "handleSmsMessage got object of type: " + message.obj.getClass().getName();
            loge(errorMessage);
            CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                    CELL_BROADCAST_MESSAGE_ERROR__TYPE__UNEXPECTED_CDMA_MESSAGE_TYPE_FROM_FWK,
                    errorMessage);
            return false;
        }
    }

    /**
     * Get the maximum time for waiting location.
     *
     * @param message Cell broadcast message
     * @return The maximum waiting time in second
     */
    protected int getMaxLocationWaitingTime(SmsCbMessage message) {
        int maximumTime = message.getMaximumWaitingDuration();
        if (maximumTime == SmsCbMessage.MAXIMUM_WAIT_TIME_NOT_SET) {
            Resources res = getResources(message.getSubscriptionId());
            maximumTime = res.getInteger(R.integer.max_location_waiting_time);
        }
        return maximumTime;
    }

    /**
     * Dispatch a Cell Broadcast message to listeners.
     * @param message the Cell Broadcast to broadcast
     */
    protected void handleBroadcastSms(SmsCbMessage message) {
        int slotIndex = message.getSlotIndex();

        // TODO: Database inserting can be time consuming, therefore this should be changed to
        // asynchronous.
        ContentValues cv = message.getContentValues();
        Uri uri = mContext.getContentResolver().insert(CellBroadcasts.CONTENT_URI, cv);

        if (message.needGeoFencingCheck()) {
            int maximumWaitingTime = getMaxLocationWaitingTime(message);
            if (DBG) {
                log("Requesting location for geo-fencing. serialNumber = "
                        + message.getSerialNumber() + ", maximumWaitingTime = "
                        + maximumWaitingTime);
            }

            requestLocationUpdate((location, accuracy) -> {
                if (location == null) {
                    // Broadcast the message directly if the location is not available.
                    broadcastMessage(message, uri, slotIndex);
                } else {
                    performGeoFencing(message, uri, message.getGeometries(), location, slotIndex,
                            accuracy);
                }
            }, maximumWaitingTime);
        } else {
            if (DBG) {
                log("Broadcast the message directly because no geo-fencing required, "
                        + "serialNumber = " + message.getSerialNumber()
                        + " needGeoFencing = " + message.needGeoFencingCheck());
            }
            broadcastMessage(message, uri, slotIndex);
        }
    }

    /**
     * Check the location based on geographical scope defined in 3GPP TS 23.041 section 9.4.1.2.1.
     *
     * The Geographical Scope (GS) indicates the geographical area over which the Message Code
     * is unique, and the display mode. The CBS message is not necessarily broadcast by all cells
     * within the geographical area. When two CBS messages are received with identical Serial
     * Numbers/Message Identifiers in two different cells, the Geographical Scope may be used to
     * determine if the CBS messages are indeed identical.
     *
     * @param message The current message
     * @param messageToCheck The older message in the database to be checked
     * @return {@code true} if within the same area, otherwise {@code false}, which should be
     * be considered as a new message.
     */
    private boolean isSameLocation(SmsCbMessage message,
            SmsCbMessage messageToCheck) {
        if (message.getGeographicalScope() != messageToCheck.getGeographicalScope()) {
            return false;
        }

        // only cell wide (which means that if a message is displayed it is desirable that the
        // message is removed from the screen when the UE selects the next cell and if any CBS
        // message is received in the next cell it is to be regarded as "new").
        if (message.getGeographicalScope() == SmsCbMessage.GEOGRAPHICAL_SCOPE_CELL_WIDE_IMMEDIATE
                || message.getGeographicalScope() == SmsCbMessage.GEOGRAPHICAL_SCOPE_CELL_WIDE) {
            return message.getLocation().isInLocationArea(messageToCheck.getLocation());
        }

        // Service Area wide (which means that a CBS message with the same Message Code and Update
        // Number may or may not be "new" in the next cell according to whether the next cell is
        // in the same Service Area as the current cell)
        if (message.getGeographicalScope() == SmsCbMessage.GEOGRAPHICAL_SCOPE_LOCATION_AREA_WIDE) {
            if (!message.getLocation().getPlmn().equals(messageToCheck.getLocation().getPlmn())) {
                return false;
            }

            return message.getLocation().getLac() != -1
                    && message.getLocation().getLac() == messageToCheck.getLocation().getLac();
        }

        // PLMN wide (which means that the Message Code and/or Update Number must change in the
        // next cell, of the PLMN, for the CBS message to be "new". The CBS message is only relevant
        // to the PLMN in which it is broadcast, so any change of PLMN (including a change to
        // another PLMN which is an ePLMN) means the CBS message is "new")
        if (message.getGeographicalScope() == SmsCbMessage.GEOGRAPHICAL_SCOPE_PLMN_WIDE) {
            return !TextUtils.isEmpty(message.getLocation().getPlmn())
                    && message.getLocation().getPlmn().equals(
                            messageToCheck.getLocation().getPlmn());
        }

        return false;
    }

    /**
     * Check if the message is a duplicate
     *
     * @param message Cell broadcast message
     * @return {@code true} if this message is a duplicate
     */
    @VisibleForTesting
    public boolean isDuplicate(SmsCbMessage message) {
        if (!mEnableDuplicateDetection) {
            log("Duplicate detection was disabled for debugging purposes.");
            return false;
        }

        // Find the cell broadcast message identify by the message identifier and serial number
        // and is not broadcasted.
        String where = CellBroadcasts.RECEIVED_TIME + ">?";

        Resources res = getResources(message.getSubscriptionId());

        // Only consider cell broadcast messages received within certain period.
        // By default it's 24 hours.
        long expirationDuration = res.getInteger(R.integer.message_expiration_time);
        long dupCheckTime = System.currentTimeMillis() - expirationDuration;

        // Some carriers require reset duplication detection after airplane mode or reboot.
        if (res.getBoolean(R.bool.reset_on_power_cycle_or_airplane_mode)) {
            dupCheckTime = Long.max(dupCheckTime, mLastAirplaneModeTime);
            dupCheckTime = Long.max(dupCheckTime,
                    System.currentTimeMillis() - SystemClock.elapsedRealtime());
        }

        List<SmsCbMessage> cbMessages = new ArrayList<>();

        try (Cursor cursor = mContext.getContentResolver().query(CellBroadcasts.CONTENT_URI,
                CellBroadcastProvider.QUERY_COLUMNS,
                where,
                new String[] {Long.toString(dupCheckTime)},
                null)) {
            if (cursor != null) {
                while (cursor.moveToNext()) {
                    cbMessages.add(SmsCbMessage.createFromCursor(cursor));
                }
            }
        }

        boolean compareMessageBody = res.getBoolean(R.bool.duplicate_compare_body);

        log("Found " + cbMessages.size() + " messages since "
                + DateFormat.getDateTimeInstance().format(dupCheckTime));
        for (SmsCbMessage messageToCheck : cbMessages) {
            // If messages are from different slots, then we only compare the message body.
            if (VDBG) log("Checking the message " + messageToCheck);
            if (message.getSlotIndex() != messageToCheck.getSlotIndex()) {
                if (TextUtils.equals(message.getMessageBody(), messageToCheck.getMessageBody())) {
                    log("Duplicate message detected from different slot. " + message);
                    return true;
                }
                if (VDBG) log("Not from a same slot.");
            } else {
                // Check serial number if message is from the same carrier.
                if (message.getSerialNumber() != messageToCheck.getSerialNumber()) {
                    if (VDBG) log("Serial number does not match.");
                    // Not a dup. Check next one.
                    continue;
                }

                // ETWS primary / secondary should be treated differently.
                if (message.isEtwsMessage() && messageToCheck.isEtwsMessage()
                        && message.getEtwsWarningInfo().isPrimary()
                        != messageToCheck.getEtwsWarningInfo().isPrimary()) {
                    if (VDBG) log("ETWS primary/secondary does not match.");
                    // Not a dup. Check next one.
                    continue;
                }

                // Check if the message category is different. Some carriers send cell broadcast
                // messages on different techs (i.e. GSM / CDMA), so we need to compare service
                // category cross techs.
                if (message.getServiceCategory() != messageToCheck.getServiceCategory()
                        && !Objects.equals(mServiceCategoryCrossRATMap.get(
                                message.getServiceCategory()), messageToCheck.getServiceCategory())
                        && !Objects.equals(mServiceCategoryCrossRATMap.get(
                                messageToCheck.getServiceCategory()),
                        message.getServiceCategory())) {
                    if (VDBG) log("GSM/CDMA category does not match.");
                    // Not a dup. Check next one.
                    continue;
                }

                // Check if the message location is different
                if (!isSameLocation(message, messageToCheck)) {
                    if (VDBG) log("Location does not match.");
                    // Not a dup. Check next one.
                    continue;
                }

                // Compare message body if needed.
                if (!compareMessageBody || TextUtils.equals(
                        message.getMessageBody(), messageToCheck.getMessageBody())) {
                    log("Duplicate message detected. " + message);
                    return true;
                } else {
                    if (VDBG) log("Body does not match.");
                }
            }
        }

        log("Not a duplicate message. " + message);
        return false;
    }

    /**
     * Perform a geo-fencing check for {@code message}. Broadcast the {@code message} if the
     * {@code location} is inside the {@code broadcastArea}.
     * @param message the message need to geo-fencing check
     * @param uri the message's uri
     * @param broadcastArea the broadcast area of the message
     * @param location current location
     * @param slotIndex the index of the slot
     * @param accuracy the accuracy of the coordinate given in meters
     */
    protected void performGeoFencing(SmsCbMessage message, Uri uri, List<Geometry> broadcastArea,
            LatLng location, int slotIndex, double accuracy) {

        if (DBG) {
            logd("Perform geo-fencing check for message identifier = "
                    + message.getServiceCategory()
                    + " serialNumber = " + message.getSerialNumber());
        }

        if (uri != null) {
            ContentValues cv = new ContentValues();
            cv.put(CellBroadcasts.LOCATION_CHECK_TIME, System.currentTimeMillis());
            mContext.getContentResolver().update(CellBroadcasts.CONTENT_URI, cv,
                    CellBroadcasts._ID + "=?", new String[] {uri.getLastPathSegment()});
        }

        // When fully implemented, #addCoordinate will be called multiple times and not just once.
        CbSendMessageCalculator calc =
                mCbSendMessageCalculatorFactory.createNew(mContext, broadcastArea);
        calc.addCoordinate(location, accuracy);

        if (calc.getAction() == SEND_MESSAGE_ACTION_SEND
                || calc.getAction() == SEND_MESSAGE_ACTION_AMBIGUOUS
                || calc.getAction() == SEND_MESSAGE_ACTION_NO_COORDINATES) {
            broadcastMessage(message, uri, slotIndex);
            if (DBG) {
                Log.d(TAG, "performGeoFencing: SENT.  action=" + calc.getActionString()
                        + ", loc=" + location.toString() + ", acc=" + accuracy);
                calc.getAction();
            }
            return;
        }

        if (DBG) {
            logd("Device location is outside the broadcast area "
                    + CbGeoUtils.encodeGeometriesToString(broadcastArea));
            Log.d(TAG, "performGeoFencing: OUTSIDE.  action=" + calc.getAction() + ", loc="
                    + location.toString() + ", acc=" + accuracy);
        }
        if (message.getMessageFormat() == SmsCbMessage.MESSAGE_FORMAT_3GPP) {
            CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_FILTERED,
                    CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__TYPE__GSM,
                    CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__FILTER__GEOFENCED_MESSAGE);
        } else if (message.getMessageFormat() == SmsCbMessage.MESSAGE_FORMAT_3GPP2) {
            CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_FILTERED,
                    CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__TYPE__CDMA,
                    CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__FILTER__GEOFENCED_MESSAGE);
        }

        sendMessage(EVENT_BROADCAST_NOT_REQUIRED);
    }

    /**
     * Request a single location update.
     * @param callback a callback will be called when the location is available.
     * @param maximumWaitTimeSec the maximum wait time of this request. If location is not updated
     * within the maximum wait time, {@code callback#onLocationUpadte(null)} will be called.
     */
    protected void requestLocationUpdate(LocationUpdateCallback callback, int maximumWaitTimeSec) {
        mLocationRequester.requestLocationUpdate(callback, maximumWaitTimeSec);
    }

    /**
     * Get the subscription ID for a phone ID, or INVALID_SUBSCRIPTION_ID if the phone does not
     * have an active sub
     * @param phoneId the phoneId to use
     * @return the associated sub id
     */
    protected static int getSubIdForPhone(Context context, int phoneId) {
        SubscriptionManager subMan =
                (SubscriptionManager) context.getSystemService(
                        Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        int[] subIds = subMan.getSubscriptionIds(phoneId);
        if (subIds != null) {
            return subIds[0];
        } else {
            return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
        }
    }

    /**
     * Put the phone ID and sub ID into an intent as extras.
     */
    public static void putPhoneIdAndSubIdExtra(Context context, Intent intent, int phoneId) {
        int subId = getSubIdForPhone(context, phoneId);
        if (subId != SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            intent.putExtra("subscription", subId);
            intent.putExtra(SubscriptionManager.EXTRA_SUBSCRIPTION_INDEX, subId);
        }
        intent.putExtra("phone", phoneId);
        intent.putExtra(SubscriptionManager.EXTRA_SLOT_INDEX, phoneId);
    }

    /**
     * Broadcast the {@code message} to the applications.
     * @param message a message need to broadcast
     * @param messageUri message's uri
     */
    protected void broadcastMessage(@NonNull SmsCbMessage message, @Nullable Uri messageUri,
            int slotIndex) {
        String msg;
        Intent intent;
        if (message.isEmergencyMessage()) {
            msg = "Dispatching emergency SMS CB, SmsCbMessage is: " + message;
            log(msg);
            mLocalLog.log(msg);
            intent = new Intent(Telephony.Sms.Intents.ACTION_SMS_EMERGENCY_CB_RECEIVED);
            //Emergency alerts need to be delivered with high priority
            intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);

            intent.putExtra(EXTRA_MESSAGE, message);
            putPhoneIdAndSubIdExtra(mContext, intent, slotIndex);

            if (IS_DEBUGGABLE) {
                // Send additional broadcast intent to the specified package. This is only for sl4a
                // automation tests.
                String[] testPkgs = mContext.getResources().getStringArray(
                        R.array.test_cell_broadcast_receiver_packages);
                if (testPkgs != null) {
                    Intent additionalIntent = new Intent(intent);
                    for (String pkg : testPkgs) {
                        additionalIntent.setPackage(pkg);
                        mContext.createContextAsUser(UserHandle.ALL, 0).sendOrderedBroadcast(
                                intent, null, (Bundle) null, null, getHandler(),
                                Activity.RESULT_OK, null, null);

                    }
                }
            }

            List<String> pkgs = new ArrayList<>();
            pkgs.add(getDefaultCBRPackageName(mContext, intent));
            pkgs.addAll(Arrays.asList(mContext.getResources().getStringArray(
                    R.array.additional_cell_broadcast_receiver_packages)));
            if (pkgs != null) {
                mReceiverCount.addAndGet(pkgs.size());
                for (String pkg : pkgs) {
                    // Explicitly send the intent to all the configured cell broadcast receivers.
                    intent.setPackage(pkg);
                    mContext.createContextAsUser(UserHandle.ALL, 0).sendOrderedBroadcast(
                            intent, null, (Bundle) null, mOrderedBroadcastReceiver, getHandler(),
                            Activity.RESULT_OK, null, null);
                }
            }
        } else {
            msg = "Dispatching SMS CB, SmsCbMessage is: " + message;
            log(msg);
            mLocalLog.log(msg);
            // Send implicit intent since there are various 3rd party carrier apps listen to
            // this intent.

            mReceiverCount.incrementAndGet();
            CellBroadcastIntents.sendSmsCbReceivedBroadcast(
                    mContext, UserHandle.ALL, message, mOrderedBroadcastReceiver, getHandler(),
                    Activity.RESULT_OK, slotIndex);
        }

        if (messageUri != null) {
            ContentValues cv = new ContentValues();
            cv.put(CellBroadcasts.MESSAGE_BROADCASTED, 1);
            mContext.getContentResolver().update(CellBroadcasts.CONTENT_URI, cv,
                    CellBroadcasts._ID + "=?", new String[] {messageUri.getLastPathSegment()});
        }
    }

    /**
     * Checks if the app's path starts with CB_APEX_PATH
     */
    private static boolean isAppInCBApex(ApplicationInfo appInfo) {
        return appInfo.sourceDir.startsWith(CB_APEX_PATH);
    }

    /**
     * Find the name of the default CBR package. The criteria is that it belongs to CB apex and
     * handles the given intent.
     */
    static String getDefaultCBRPackageName(Context context, Intent intent) {
        PackageManager packageManager = context.getPackageManager();
        List<ResolveInfo> cbrPackages = packageManager.queryBroadcastReceivers(intent, 0);

        // remove apps that don't live in the CellBroadcast apex
        cbrPackages.removeIf(info ->
                !isAppInCBApex(info.activityInfo.applicationInfo));

        if (cbrPackages.isEmpty()) {
            Log.e(TAG, "getCBRPackageNames: no package found");
            return null;
        }

        if (cbrPackages.size() > 1) {
            // multiple apps found, log an error but continue
            Log.e(TAG, "Found > 1 APK in CB apex that can resolve " + intent.getAction() + ": "
                    + cbrPackages.stream()
                    .map(info -> info.activityInfo.applicationInfo.packageName)
                    .collect(Collectors.joining(", ")));
        }

        // Assume the first ResolveInfo is the one we're looking for
        ResolveInfo info = cbrPackages.get(0);
        return info.activityInfo.applicationInfo.packageName;
    }

    /**
     * Get the device resource based on SIM
     *
     * @param subId Subscription index
     *
     * @return The resource
     */
    public @NonNull Resources getResources(int subId) {
        if (subId == SubscriptionManager.DEFAULT_SUBSCRIPTION_ID
                || !SubscriptionManager.isValidSubscriptionId(subId)) {
            return mContext.getResources();
        }

        if (mResourcesCache.containsKey(subId)) {
            return mResourcesCache.get(subId);
        }

        Resources res = SubscriptionManager.getResourcesForSubId(mContext, subId);
        mResourcesCache.put(subId, res);

        return res;
    }

    @Override
    public void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
        pw.println("CellBroadcastHandler:");
        mLocalLog.dump(fd, pw, args);
        pw.flush();
    }

    /** The callback interface of a location request. */
    public interface LocationUpdateCallback {
        /**
         * Call when the location update is available.
         * @param location a location in (latitude, longitude) format, or {@code null} if the
         * location service is not available.
         */
        void onLocationUpdate(@Nullable LatLng location, double accuracy);
    }

    private static final class LocationRequester {
        private static final String TAG = CellBroadcastHandler.class.getSimpleName();

        /**
         * Fused location provider, which means GPS plus network based providers (cell, wifi, etc..)
         */
        //TODO: Should make LocationManager.FUSED_PROVIDER system API in S.
        private static final String FUSED_PROVIDER = "fused";

        private final LocationManager mLocationManager;
        private final List<LocationUpdateCallback> mCallbacks;
        private final Context mContext;
        private final Handler mLocationHandler;

        private boolean mLocationUpdateInProgress;
        private final Runnable mTimeoutCallback;
        private CancellationSignal mCancellationSignal;

        LocationRequester(Context context, LocationManager locationManager, Handler handler) {
            mLocationManager = locationManager;
            mCallbacks = new ArrayList<>();
            mContext = context;
            mLocationHandler = handler;
            mLocationUpdateInProgress = false;
            mTimeoutCallback = this::onLocationTimeout;
        }

        /**
         * Request a single location update. If the location is not available, a callback with
         * {@code null} location will be called immediately.
         *
         * @param callback a callback to the response when the location is available
         * @param maximumWaitTimeS the maximum wait time of this request. If location is not
         * updated within the maximum wait time, {@code callback#onLocationUpadte(null)} will be
         * called.
         */
        void requestLocationUpdate(@NonNull LocationUpdateCallback callback,
                int maximumWaitTimeS) {
            mLocationHandler.post(() -> requestLocationUpdateInternal(callback, maximumWaitTimeS));
        }

        private void onLocationTimeout() {
            Log.e(TAG, "Location request timeout");
            if (mCancellationSignal != null) {
                mCancellationSignal.cancel();
            }
            onLocationUpdate(null);
        }

        private void onLocationUpdate(@Nullable Location location) {
            mLocationUpdateInProgress = false;
            mLocationHandler.removeCallbacks(mTimeoutCallback);
            LatLng latLng = null;
            float accuracy = 0;
            if (location != null) {
                Log.d(TAG, "Got location update");
                latLng = new LatLng(location.getLatitude(), location.getLongitude());
                accuracy = location.getAccuracy();
            } else {
                Log.e(TAG, "Location is not available.");
            }

            for (LocationUpdateCallback callback : mCallbacks) {
                callback.onLocationUpdate(latLng, accuracy);
            }
            mCallbacks.clear();
        }

        private void requestLocationUpdateInternal(@NonNull LocationUpdateCallback callback,
                int maximumWaitTimeS) {
            if (DBG) Log.d(TAG, "requestLocationUpdate");
            if (!hasPermission(ACCESS_FINE_LOCATION) && !hasPermission(ACCESS_COARSE_LOCATION)) {
                if (DBG) {
                    Log.e(TAG, "Can't request location update because of no location permission");
                }
                callback.onLocationUpdate(null, Float.NaN);
                return;
            }

            if (!mLocationUpdateInProgress) {
                LocationRequest request = LocationRequest.create()
                        .setProvider(FUSED_PROVIDER)
                        .setQuality(LocationRequest.ACCURACY_FINE)
                        .setInterval(0)
                        .setFastestInterval(0)
                        .setSmallestDisplacement(0)
                        .setNumUpdates(1)
                        .setExpireIn(TimeUnit.SECONDS.toMillis(maximumWaitTimeS));
                if (DBG) {
                    Log.d(TAG, "Location request=" + request);
                }
                try {
                    mCancellationSignal = new CancellationSignal();
                    mLocationManager.getCurrentLocation(request, mCancellationSignal,
                            new HandlerExecutor(mLocationHandler), this::onLocationUpdate);
                    // TODO: Remove the following workaround in S. We need to enforce the timeout
                    // before location manager adds the support for timeout value which is less
                    // than 30 seconds. After that we can rely on location manager's timeout
                    // mechanism.
                    mLocationHandler.postDelayed(mTimeoutCallback,
                            TimeUnit.SECONDS.toMillis(maximumWaitTimeS));
                } catch (IllegalArgumentException e) {
                    Log.e(TAG, "Cannot get current location. e=" + e);
                    callback.onLocationUpdate(null, 0.0);
                    return;
                }
                mLocationUpdateInProgress = true;
            }
            mCallbacks.add(callback);
        }

        private boolean hasPermission(String permission) {
            // TODO: remove the check. This will always return true because cell broadcast service
            // is running under the UID Process.NETWORK_STACK_UID, which is below 10000. It will be
            // automatically granted with all runtime permissions.
            return mContext.checkPermission(permission, Process.myPid(), Process.myUid())
                    == PackageManager.PERMISSION_GRANTED;
        }
    }
}
