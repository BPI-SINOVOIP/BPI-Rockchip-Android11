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

import static com.android.cellbroadcastservice.CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_ERROR__TYPE__GSM_INVALID_PDU;
import static com.android.cellbroadcastservice.CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_ERROR__TYPE__UNEXPECTED_GSM_MESSAGE_TYPE_FROM_FWK;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.database.Cursor;
import android.net.Uri;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.os.UserHandle;
import android.provider.Telephony.CellBroadcasts;
import android.telephony.AccessNetworkConstants;
import android.telephony.CbGeoUtils.Geometry;
import android.telephony.CellBroadcastIntents;
import android.telephony.CellIdentity;
import android.telephony.CellIdentityGsm;
import android.telephony.CellIdentityLte;
import android.telephony.CellIdentityNr;
import android.telephony.CellIdentityTdscdma;
import android.telephony.CellIdentityWcdma;
import android.telephony.CellInfo;
import android.telephony.NetworkRegistrationInfo;
import android.telephony.ServiceState;
import android.telephony.SmsCbLocation;
import android.telephony.SmsCbMessage;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Pair;
import android.util.SparseArray;

import com.android.cellbroadcastservice.GsmSmsCbMessage.GeoFencingTriggerMessage;
import com.android.cellbroadcastservice.GsmSmsCbMessage.GeoFencingTriggerMessage.CellBroadcastIdentity;
import com.android.internal.annotations.VisibleForTesting;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

/**
 * Handler for 3GPP format Cell Broadcasts. Parent class can also handle CDMA Cell Broadcasts.
 */
public class GsmCellBroadcastHandler extends CellBroadcastHandler {
    private static final boolean VDBG = false;  // log CB PDU data

    /** Indicates that a message is not displayed. */
    private static final String MESSAGE_NOT_DISPLAYED = "0";

    private final SparseArray<String> mAreaInfos = new SparseArray<>();

    /** This map holds incomplete concatenated messages waiting for assembly. */
    private final HashMap<SmsCbConcatInfo, byte[][]> mSmsCbPageMap =
            new HashMap<>(4);

    @VisibleForTesting
    public GsmCellBroadcastHandler(Context context, Looper looper,
            CbSendMessageCalculatorFactory cbSendMessageCalculatorFactory) {
        super("GsmCellBroadcastHandler", context, looper, cbSendMessageCalculatorFactory);
    }

    @Override
    protected void onQuitting() {
        super.onQuitting();     // release wakelock
    }

    /**
     * Handle a GSM cell broadcast message passed from the telephony framework.
     * @param message
     */
    public void onGsmCellBroadcastSms(int slotIndex, byte[] message) {
        sendMessage(EVENT_NEW_SMS_MESSAGE, slotIndex, -1, message);
    }

    /**
     * Get the area information
     *
     * @param slotIndex SIM slot index
     * @return The area information
     */
    @NonNull
    public String getCellBroadcastAreaInfo(int slotIndex) {
        String info;
        synchronized (mAreaInfos) {
            info = mAreaInfos.get(slotIndex);
        }
        return info == null ? "" : info;
    }

    /**
     * Create a new CellBroadcastHandler.
     * @param context the context to use for dispatching Intents
     * @return the new handler
     */
    public static GsmCellBroadcastHandler makeGsmCellBroadcastHandler(Context context) {
        GsmCellBroadcastHandler handler = new GsmCellBroadcastHandler(context, Looper.myLooper(),
                new CbSendMessageCalculatorFactory());
        handler.start();
        return handler;
    }

    /**
     * Find the cell broadcast messages specify by the geo-fencing trigger message and perform a
     * geo-fencing check for these messages.
     * @param geoFencingTriggerMessage the trigger message
     *
     * @return {@code True} if geo-fencing is need for some cell broadcast message.
     */
    private boolean handleGeoFencingTriggerMessage(
            GeoFencingTriggerMessage geoFencingTriggerMessage, int slotIndex) {
        final List<SmsCbMessage> cbMessages = new ArrayList<>();
        final List<Uri> cbMessageUris = new ArrayList<>();

        SubscriptionManager subMgr = (SubscriptionManager) mContext.getSystemService(
                Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        int[] subIds = subMgr.getSubscriptionIds(slotIndex);
        Resources res;
        if (subIds != null) {
            res = getResources(subIds[0]);
        } else {
            res = getResources(SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
        }

        // Only consider the cell broadcast received within 24 hours.
        long lastReceivedTime = System.currentTimeMillis() - DateUtils.DAY_IN_MILLIS;

        // Some carriers require reset duplication detection after airplane mode or reboot.
        if (res.getBoolean(R.bool.reset_on_power_cycle_or_airplane_mode)) {
            lastReceivedTime = Long.max(lastReceivedTime, mLastAirplaneModeTime);
            lastReceivedTime = Long.max(lastReceivedTime,
                    System.currentTimeMillis() - SystemClock.elapsedRealtime());
        }

        // Find the cell broadcast message identify by the message identifier and serial number
        // and was not displayed.
        String where = CellBroadcasts.SERVICE_CATEGORY + "=? AND "
                + CellBroadcasts.SERIAL_NUMBER + "=? AND "
                + CellBroadcasts.MESSAGE_DISPLAYED + "=? AND "
                + CellBroadcasts.RECEIVED_TIME + ">?";

        ContentResolver resolver = mContext.getContentResolver();
        for (CellBroadcastIdentity identity : geoFencingTriggerMessage.cbIdentifiers) {
            try (Cursor cursor = resolver.query(CellBroadcasts.CONTENT_URI,
                    CellBroadcastProvider.QUERY_COLUMNS,
                    where,
                    new String[] { Integer.toString(identity.messageIdentifier),
                            Integer.toString(identity.serialNumber), MESSAGE_NOT_DISPLAYED,
                            Long.toString(lastReceivedTime) },
                    null /* sortOrder */)) {
                if (cursor != null) {
                    while (cursor.moveToNext()) {
                        cbMessages.add(SmsCbMessage.createFromCursor(cursor));
                        cbMessageUris.add(ContentUris.withAppendedId(CellBroadcasts.CONTENT_URI,
                                cursor.getInt(cursor.getColumnIndex(CellBroadcasts._ID))));
                    }
                }
            }
        }

        log("Found " + cbMessages.size() + " not broadcasted messages since "
                + DateFormat.getDateTimeInstance().format(lastReceivedTime));

        List<Geometry> commonBroadcastArea = new ArrayList<>();
        if (geoFencingTriggerMessage.shouldShareBroadcastArea()) {
            for (SmsCbMessage msg : cbMessages) {
                if (msg.getGeometries() != null) {
                    commonBroadcastArea.addAll(msg.getGeometries());
                }
            }
        }

        // ATIS doesn't specify the geo fencing maximum wait time for the cell broadcasts specified
        // in geo fencing trigger message. We will pick the largest maximum wait time among these
        // cell broadcasts.
        int maxWaitingTimeSec = 0;
        for (SmsCbMessage msg : cbMessages) {
            maxWaitingTimeSec = Math.max(maxWaitingTimeSec, getMaxLocationWaitingTime(msg));
        }

        if (DBG) {
            logd("Geo-fencing trigger message = " + geoFencingTriggerMessage);
            for (SmsCbMessage msg : cbMessages) {
                logd(msg.toString());
            }
        }

        if (cbMessages.isEmpty()) {
            if (DBG) logd("No CellBroadcast message need to be broadcasted");
            return false;
        }

        requestLocationUpdate((location, accuracy) -> {
            if (location == null) {
                // If the location is not available, broadcast the messages directly.
                for (int i = 0; i < cbMessages.size(); i++) {
                    broadcastMessage(cbMessages.get(i), cbMessageUris.get(i), slotIndex);
                }
            } else {
                for (int i = 0; i < cbMessages.size(); i++) {
                    List<Geometry> broadcastArea = !commonBroadcastArea.isEmpty()
                            ? commonBroadcastArea : cbMessages.get(i).getGeometries();
                    if (broadcastArea == null || broadcastArea.isEmpty()) {
                        broadcastMessage(cbMessages.get(i), cbMessageUris.get(i), slotIndex);
                    } else {
                        performGeoFencing(cbMessages.get(i), cbMessageUris.get(i), broadcastArea,
                                location, slotIndex, accuracy);
                    }
                }
            }
        }, maxWaitingTimeSec);
        return true;
    }

    /**
     * Process area info message.
     *
     * @param slotIndex SIM slot index
     * @param message Cell broadcast message
     * @return {@code true} if the mssage is an area info message and got processed correctly,
     * otherwise {@code false}.
     */
    private boolean handleAreaInfoMessage(int slotIndex, SmsCbMessage message) {
        Resources res = getResources(message.getSubscriptionId());
        int[] areaInfoChannels = res.getIntArray(R.array.area_info_channels);

        // Check area info message
        if (IntStream.of(areaInfoChannels).anyMatch(
                x -> x == message.getServiceCategory())) {
            synchronized (mAreaInfos) {
                String info = mAreaInfos.get(slotIndex);
                if (TextUtils.equals(info, message.getMessageBody())) {
                    // Message is a duplicate
                    return true;
                }
                mAreaInfos.put(slotIndex, message.getMessageBody());
            }

            String[] pkgs = mContext.getResources().getStringArray(
                    R.array.config_area_info_receiver_packages);
            for (String pkg : pkgs) {
                Intent intent = new Intent(CellBroadcastIntents.ACTION_AREA_INFO_UPDATED);
                intent.putExtra(SubscriptionManager.EXTRA_SLOT_INDEX, slotIndex);
                intent.setPackage(pkg);
                mContext.sendBroadcastAsUser(intent, UserHandle.ALL,
                        android.Manifest.permission.READ_PRIVILEGED_PHONE_STATE);
            }
            return true;
        }

        // This is not an area info message.
        return false;
    }

    /**
     * Handle 3GPP-format Cell Broadcast messages sent from radio.
     *
     * @param message the message to process
     * @return true if need to wait for geo-fencing or an ordered broadcast was sent.
     */
    @Override
    protected boolean handleSmsMessage(Message message) {
        // For GSM, message.obj should be a byte[]
        int slotIndex = message.arg1;
        if (message.obj instanceof byte[]) {
            byte[] pdu = (byte[]) message.obj;
            SmsCbHeader header = createSmsCbHeader(pdu);
            if (header == null) return false;

            log("header=" + header);
            if (header.getServiceCategory() == SmsCbConstants.MESSAGE_ID_CMAS_GEO_FENCING_TRIGGER) {
                GeoFencingTriggerMessage triggerMessage =
                        GsmSmsCbMessage.createGeoFencingTriggerMessage(pdu);
                if (triggerMessage != null) {
                    return handleGeoFencingTriggerMessage(triggerMessage, slotIndex);
                }
            } else {
                SmsCbMessage cbMessage = handleGsmBroadcastSms(header, pdu, slotIndex);
                if (cbMessage != null) {
                    if (isDuplicate(cbMessage)) {
                        CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_FILTERED,
                                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__TYPE__GSM,
                                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__FILTER__DUPLICATE_MESSAGE);
                        return false;
                    }

                    if (handleAreaInfoMessage(slotIndex, cbMessage)) {
                        log("Channel " + cbMessage.getServiceCategory() + " message processed");
                        CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_FILTERED,
                                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__TYPE__GSM,
                                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_FILTERED__FILTER__AREA_INFO_MESSAGE);
                        return false;
                    }

                    handleBroadcastSms(cbMessage);
                    return true;
                }
                if (VDBG) log("Not handled GSM broadcasts.");
            }
        } else {
            final String errorMessage = "handleSmsMessage for GSM got object of type: "
                    + message.obj.getClass().getName();
            loge(errorMessage);
            CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                    CELL_BROADCAST_MESSAGE_ERROR__TYPE__UNEXPECTED_GSM_MESSAGE_TYPE_FROM_FWK,
                    errorMessage);
        }
        if (message.obj instanceof SmsCbMessage) {
            return super.handleSmsMessage(message);
        } else {
            return false;
        }
    }

    /**
     * Get LAC (location area code for GSM/UMTS) / TAC (tracking area code for LTE/NR) and CID
     * (Cell id) from the cell identity
     *
     * @param ci Cell identity
     * @return Pair of LAC and CID. {@code null} if not available.
     */
    private @Nullable Pair<Integer, Integer> getLacAndCid(CellIdentity ci) {
        if (ci == null) return null;
        int lac = CellInfo.UNAVAILABLE;
        int cid = CellInfo.UNAVAILABLE;
        if (ci instanceof CellIdentityGsm) {
            lac = ((CellIdentityGsm) ci).getLac();
            cid = ((CellIdentityGsm) ci).getCid();
        } else if (ci instanceof CellIdentityWcdma) {
            lac = ((CellIdentityWcdma) ci).getLac();
            cid = ((CellIdentityWcdma) ci).getCid();
        } else if ((ci instanceof CellIdentityTdscdma)) {
            lac = ((CellIdentityTdscdma) ci).getLac();
            cid = ((CellIdentityTdscdma) ci).getCid();
        } else if (ci instanceof CellIdentityLte) {
            lac = ((CellIdentityLte) ci).getTac();
            cid = ((CellIdentityLte) ci).getCi();
        } else if (ci instanceof CellIdentityNr) {
            lac = ((CellIdentityNr) ci).getTac();
            cid = ((CellIdentityNr) ci).getPci();
        }

        if (lac != CellInfo.UNAVAILABLE || cid != CellInfo.UNAVAILABLE) {
            return Pair.create(lac, cid);
        }

        // When both LAC and CID are not available.
        return null;
    }

    /**
     * Get LAC (location area code for GSM/UMTS) / TAC (tracking area code for LTE/NR) and CID
     * (Cell id) of the registered network.
     *
     * @param slotIndex SIM slot index
     *
     * @return lac and cid. {@code null} if cell identity is not available from the registered
     * network.
     */
    private @Nullable Pair<Integer, Integer> getLacAndCid(int slotIndex) {
        TelephonyManager tm = mContext.getSystemService(TelephonyManager.class);
        tm.createForSubscriptionId(getSubIdForPhone(mContext, slotIndex));

        ServiceState serviceState = tm.getServiceState();

        if (serviceState == null) return null;

        // The list of cell identity to extract LAC and CID. The higher priority one will be added
        // into the top of list.
        List<CellIdentity> cellIdentityList = new ArrayList<>();

        // CS network
        NetworkRegistrationInfo nri = serviceState.getNetworkRegistrationInfo(
                NetworkRegistrationInfo.DOMAIN_CS, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        if (nri != null) {
            cellIdentityList.add(nri.getCellIdentity());
        }

        // PS network
        nri = serviceState.getNetworkRegistrationInfo(
                NetworkRegistrationInfo.DOMAIN_PS, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        if (nri != null) {
            cellIdentityList.add(nri.getCellIdentity());
        }

        // When SIM is not inserted, we use the cell identity from the nearby cell. This is
        // best effort.
        List<CellInfo> infos = tm.getAllCellInfo();
        if (infos != null) {
            cellIdentityList.addAll(
                    infos.stream().map(CellInfo::getCellIdentity).collect(Collectors.toList()));
        }

        // Return the first valid LAC and CID from the list.
        return cellIdentityList.stream()
                .map(this::getLacAndCid)
                .filter(Objects::nonNull)
                .findFirst()
                .orElse(null);
    }


    /**
     * Handle 3GPP format SMS-CB message.
     * @param header the cellbroadcast header.
     * @param receivedPdu the received PDUs as a byte[]
     */
    private SmsCbMessage handleGsmBroadcastSms(SmsCbHeader header, byte[] receivedPdu,
            int slotIndex) {
        try {
            if (VDBG) {
                int pduLength = receivedPdu.length;
                for (int i = 0; i < pduLength; i += 8) {
                    StringBuilder sb = new StringBuilder("SMS CB pdu data: ");
                    for (int j = i; j < i + 8 && j < pduLength; j++) {
                        int b = receivedPdu[j] & 0xff;
                        if (b < 0x10) {
                            sb.append('0');
                        }
                        sb.append(Integer.toHexString(b)).append(' ');
                    }
                    log(sb.toString());
                }
            }

            if (VDBG) log("header=" + header);
            TelephonyManager tm =
                    (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
            tm.createForSubscriptionId(getSubIdForPhone(mContext, slotIndex));
            String plmn = tm.getNetworkOperator();
            int lac = -1;
            int cid = -1;
            // Get LAC and CID of the current camped cell.
            Pair<Integer, Integer> lacAndCid = getLacAndCid(slotIndex);
            if (lacAndCid != null) {
                lac = lacAndCid.first;
                cid = lacAndCid.second;
            }

            SmsCbLocation location = new SmsCbLocation(plmn, lac, cid);

            byte[][] pdus;
            int pageCount = header.getNumberOfPages();
            if (pageCount > 1) {
                // Multi-page message
                SmsCbConcatInfo concatInfo = new SmsCbConcatInfo(header, location);

                // Try to find other pages of the same message
                pdus = mSmsCbPageMap.get(concatInfo);

                if (pdus == null) {
                    // This is the first page of this message, make room for all
                    // pages and keep until complete
                    pdus = new byte[pageCount][];

                    mSmsCbPageMap.put(concatInfo, pdus);
                }

                if (VDBG) log("pdus size=" + pdus.length);
                // Page parameter is one-based
                pdus[header.getPageIndex() - 1] = receivedPdu;

                for (byte[] pdu : pdus) {
                    if (pdu == null) {
                        // Still missing pages, exit
                        log("still missing pdu");
                        return null;
                    }
                }

                // Message complete, remove and dispatch
                mSmsCbPageMap.remove(concatInfo);
            } else {
                // Single page message
                pdus = new byte[1][];
                pdus[0] = receivedPdu;
            }

            // Remove messages that are out of scope to prevent the map from
            // growing indefinitely, containing incomplete messages that were
            // never assembled
            Iterator<SmsCbConcatInfo> iter = mSmsCbPageMap.keySet().iterator();

            while (iter.hasNext()) {
                SmsCbConcatInfo info = iter.next();

                if (!info.matchesLocation(plmn, lac, cid)) {
                    iter.remove();
                }
            }

            return GsmSmsCbMessage.createSmsCbMessage(mContext, header, location, pdus, slotIndex);

        } catch (RuntimeException e) {
            final String errorMessage = "Error in decoding SMS CB pdu: " + e.toString();
            e.printStackTrace();
            loge(errorMessage);
            CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                    CELL_BROADCAST_MESSAGE_ERROR__TYPE__GSM_INVALID_PDU, errorMessage);
            return null;
        }
    }

    private SmsCbHeader createSmsCbHeader(byte[] bytes) {
        try {
            return new SmsCbHeader(bytes);
        } catch (Exception ex) {
            loge("Can't create SmsCbHeader, ex = " + ex.toString());
            return null;
        }
    }

    /**
     * Holds all info about a message page needed to assemble a complete concatenated message.
     */
    private static final class SmsCbConcatInfo {

        private final SmsCbHeader mHeader;
        private final SmsCbLocation mLocation;

        SmsCbConcatInfo(SmsCbHeader header, SmsCbLocation location) {
            mHeader = header;
            mLocation = location;
        }

        @Override
        public int hashCode() {
            return (mHeader.getSerialNumber() * 31) + mLocation.hashCode();
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof SmsCbConcatInfo) {
                SmsCbConcatInfo other = (SmsCbConcatInfo) obj;

                // Two pages match if they have the same serial number (which includes the
                // geographical scope and update number), and both pages belong to the same
                // location (PLMN, plus LAC and CID if these are part of the geographical scope).
                return mHeader.getSerialNumber() == other.mHeader.getSerialNumber()
                        && mLocation.equals(other.mLocation);
            }

            return false;
        }

        /**
         * Compare the location code for this message to the current location code. The match is
         * relative to the geographical scope of the message, which determines whether the LAC
         * and Cell ID are saved in mLocation or set to -1 to match all values.
         *
         * @param plmn the current PLMN
         * @param lac the current Location Area (GSM) or Service Area (UMTS)
         * @param cid the current Cell ID
         * @return true if this message is valid for the current location; false otherwise
         */
        public boolean matchesLocation(String plmn, int lac, int cid) {
            return mLocation.isInLocationArea(plmn, lac, cid);
        }
    }
}
