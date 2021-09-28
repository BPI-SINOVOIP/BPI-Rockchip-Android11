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

package com.android.cellbroadcastservice;

import static com.android.cellbroadcastservice.CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_ERROR__TYPE__CDMA_DECODING_ERROR;

import android.annotation.NonNull;
import android.content.Context;
import android.os.Bundle;
import android.telephony.CellBroadcastService;
import android.telephony.SmsCbLocation;
import android.telephony.SmsCbMessage;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.cdma.CdmaSmsCbProgramData;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

/**
 * The default implementation of CellBroadcastService, which is used for handling GSM and CDMA cell
 * broadcast messages.
 */
public class DefaultCellBroadcastService extends CellBroadcastService {
    private GsmCellBroadcastHandler mGsmCellBroadcastHandler;
    private CellBroadcastHandler mCdmaCellBroadcastHandler;
    private CdmaServiceCategoryProgramHandler mCdmaScpHandler;

    private static final String TAG = "DefaultCellBroadcastService";

    private static final char[] HEX_DIGITS = {'0', '1', '2', '3', '4', '5', '6', '7',
            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    @Override
    public void onCreate() {
        super.onCreate();
        mGsmCellBroadcastHandler =
                GsmCellBroadcastHandler.makeGsmCellBroadcastHandler(getApplicationContext());
        mCdmaCellBroadcastHandler =
                CellBroadcastHandler.makeCellBroadcastHandler(getApplicationContext());
        mCdmaScpHandler =
                CdmaServiceCategoryProgramHandler.makeScpHandler(getApplicationContext());
    }

    @Override
    public void onDestroy() {
        mGsmCellBroadcastHandler.cleanup();
        mCdmaCellBroadcastHandler.cleanup();
        super.onDestroy();
    }

    @Override
    public void onGsmCellBroadcastSms(int slotIndex, byte[] message) {
        Log.d(TAG, "onGsmCellBroadcastSms received message on slotId=" + slotIndex);
        CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_REPORTED,
                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_REPORTED__TYPE__GSM,
                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_REPORTED__SOURCE__CB_SERVICE);
        mGsmCellBroadcastHandler.onGsmCellBroadcastSms(slotIndex, message);
    }

    @Override
    public void onCdmaCellBroadcastSms(int slotIndex, byte[] bearerData, int serviceCategory) {
        Log.d(TAG, "onCdmaCellBroadcastSms received message on slotId=" + slotIndex);
        CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_REPORTED,
                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_REPORTED__TYPE__CDMA,
                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_REPORTED__SOURCE__CB_SERVICE);
        int[] subIds =
                ((SubscriptionManager) getSystemService(
                        Context.TELEPHONY_SUBSCRIPTION_SERVICE)).getSubscriptionIds(slotIndex);
        String plmn;
        if (subIds != null && subIds.length > 0) {
            int subId = subIds[0];
            plmn = ((TelephonyManager) getSystemService(
                    Context.TELEPHONY_SERVICE)).createForSubscriptionId(
                    subId).getNetworkOperator();
        } else {
            plmn = "";
        }
        SmsCbMessage message = parseCdmaBroadcastSms(getApplicationContext(), slotIndex, plmn,
                bearerData, serviceCategory);
        if (message != null) {
            mCdmaCellBroadcastHandler.onCdmaCellBroadcastSms(message);
        }
    }

    @Override
    public void onCdmaScpMessage(int slotIndex, List<CdmaSmsCbProgramData> programData,
            String originatingAddress, Consumer<Bundle> callback) {
        Log.d(TAG, "onCdmaScpMessage received message on slotId=" + slotIndex);
        CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_REPORTED,
                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_REPORTED__TYPE__CDMA_SPC,
                CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_REPORTED__SOURCE__CB_SERVICE);
        mCdmaScpHandler.onCdmaScpMessage(slotIndex, new ArrayList<>(programData),
                originatingAddress, callback);
    }

    @Override
    public @NonNull String getCellBroadcastAreaInfo(int slotIndex) {
        Log.d(TAG, "getCellBroadcastAreaInfo on slotId=" + slotIndex);
        return mGsmCellBroadcastHandler.getCellBroadcastAreaInfo(slotIndex);
    }

    /**
     * Parses a CDMA broadcast SMS
     *
     * @param slotIndex       the slotIndex the SMS was received on
     * @param plmn            the PLMN for a broadcast SMS or "" if unknown
     * @param bearerData      the bearerData of the SMS
     * @param serviceCategory the service category of the broadcast
     */
    @VisibleForTesting
    public static SmsCbMessage parseCdmaBroadcastSms(Context context, int slotIndex, String plmn,
            byte[] bearerData,
            int serviceCategory) {
        BearerData bData;
        try {
            bData = BearerData.decode(context, bearerData, serviceCategory);
        } catch (Exception e) {
            final String errorMessage = "Error decoding bearer data e=" + e.toString();
            Log.e(TAG, errorMessage);
            CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                    CELL_BROADCAST_MESSAGE_ERROR__TYPE__CDMA_DECODING_ERROR, errorMessage);
            return null;
        }
        Log.d(TAG, "MT raw BearerData = " + toHexString(bearerData, 0, bearerData.length));
        SmsCbLocation location = new SmsCbLocation(plmn, -1, -1);

        SubscriptionManager sm = (SubscriptionManager) context.getSystemService(
                Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        int subId = SubscriptionManager.DEFAULT_SUBSCRIPTION_ID;
        int[] subIds = sm.getSubscriptionIds(slotIndex);
        if (subIds != null && subIds.length > 0) {
            subId = subIds[0];
        }

        return new SmsCbMessage(SmsCbMessage.MESSAGE_FORMAT_3GPP2,
                SmsCbMessage.GEOGRAPHICAL_SCOPE_PLMN_WIDE, bData.messageId, location,
                serviceCategory, bData.getLanguage(), bData.userData.msgEncoding,
                bData.userData.payloadStr, bData.priority, null, bData.cmasWarningInfo, 0, null,
                System.currentTimeMillis(), slotIndex, subId);
    }

    private static String toHexString(byte[] array, int offset, int length) {
        char[] buf = new char[length * 2];
        int bufIndex = 0;
        for (int i = offset; i < offset + length; i++) {
            byte b = array[i];
            buf[bufIndex++] = HEX_DIGITS[(b >>> 4) & 0x0F];
            buf[bufIndex++] = HEX_DIGITS[b & 0x0F];
        }
        return new String(buf);
    }
}
