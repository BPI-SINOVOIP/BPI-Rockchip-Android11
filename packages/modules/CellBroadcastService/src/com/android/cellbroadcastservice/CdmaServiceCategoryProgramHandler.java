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

import static com.android.cellbroadcastservice.CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_ERROR__TYPE__CDMA_SCP_HANDLING_ERROR;
import static com.android.cellbroadcastservice.CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_ERROR__TYPE__UNEXPECTED_CDMA_SCP_MESSAGE_TYPE_FROM_FWK;

import android.Manifest;
import android.app.Activity;
import android.app.AppOpsManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Looper;
import android.os.Message;
import android.provider.Telephony.Sms.Intents;
import android.telephony.cdma.CdmaSmsCbProgramData;

import java.util.ArrayList;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.function.Consumer;

/**
 * Handle CDMA Service Category Program Data requests and responses.
 */
public final class CdmaServiceCategoryProgramHandler extends WakeLockStateMachine {

    // hold the callbacks provided from the framework, executed after SCP messages are broadcast
    private ConcurrentLinkedQueue<Consumer<Bundle>> mScpCallback = new ConcurrentLinkedQueue<>();

    /**
     * Create a new CDMA inbound SMS handler.
     */
    CdmaServiceCategoryProgramHandler(Context context) {
        super("CdmaServiceCategoryProgramHandler", context, Looper.myLooper());
        mContext = context;
    }

    /**
     * Create a new State machine for SCPD requests.
     *
     * @param context the context to use
     * @return the new SCPD handler
     */
    static CdmaServiceCategoryProgramHandler makeScpHandler(Context context) {
        CdmaServiceCategoryProgramHandler handler = new CdmaServiceCategoryProgramHandler(
                context);
        handler.start();
        return handler;
    }

    /**
     * Handle a CDMA SCP message.
     *
     * @param slotIndex          the index of the slot which received the message
     * @param programData        the SMS CB program data of the message
     * @param originatingAddress the originating address of the message
     * @param callback           a callback to run after each cell broadcast receiver has handled
     *                           the SCP message
     */
    public void onCdmaScpMessage(int slotIndex, ArrayList<CdmaSmsCbProgramData> programData,
            String originatingAddress, Consumer<Bundle> callback) {
        onCdmaCellBroadcastSms(new CdmaScpMessage(slotIndex, programData, originatingAddress,
                callback));
    }

    /**
     * Class for holding parts of a CDMA Service Program Category message.
     */
    private class CdmaScpMessage {
        int mSlotIndex;
        ArrayList<CdmaSmsCbProgramData> mProgamData;
        String mOriginatingAddress;
        Consumer<Bundle> mCallback;

        CdmaScpMessage(int slotIndex, ArrayList<CdmaSmsCbProgramData> programData,
                String originatingAddress, Consumer<Bundle> callback) {
            mSlotIndex = slotIndex;
            mProgamData = programData;
            mOriginatingAddress = originatingAddress;
            mCallback = callback;
        }
    }


    /**
     * Handle Cell Broadcast messages from {@code CdmaInboundSmsHandler}.
     * 3GPP-format Cell Broadcast messages sent from radio are handled in the subclass.
     *
     * @param message the message to process
     * @return true if an ordered broadcast was sent; false on failure
     */
    @Override
    protected boolean handleSmsMessage(Message message) {
        if (message.obj instanceof CdmaScpMessage) {
            CdmaScpMessage cdmaScpMessage = (CdmaScpMessage) message.obj;
            return handleServiceCategoryProgramData(cdmaScpMessage.mProgamData,
                    cdmaScpMessage.mOriginatingAddress, cdmaScpMessage.mSlotIndex,
                    cdmaScpMessage.mCallback);
        } else {
            final String errorMessage =
                    "handleMessage got object of type: " + message.obj.getClass().getName();
            loge(errorMessage);
            CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                    CELL_BROADCAST_MESSAGE_ERROR__TYPE__UNEXPECTED_CDMA_SCP_MESSAGE_TYPE_FROM_FWK,
                    errorMessage);
            return false;
        }
    }

    /**
     * Send SCPD request to CellBroadcastReceiver as an ordered broadcast.
     *
     * @param programData        the program data of the SCP message
     * @param originatingAddress the address of the sender
     * @param phoneId            the phoneId that the message was received on
     * @param callback           a callback to run after each broadcast
     * @return true if an ordered broadcast was sent; false on failure
     */
    private boolean handleServiceCategoryProgramData(ArrayList<CdmaSmsCbProgramData> programData,
            String originatingAddress, int phoneId, Consumer<Bundle> callback) {
        if (programData == null) {
            final String errorMessage =
                    "handleServiceCategoryProgramData: program data list is null!";
            loge(errorMessage);
            CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                    CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_ERROR__TYPE__CDMA_SCP_EMPTY,
                    errorMessage);
            return false;
        }

        Intent intent = new Intent(Intents.SMS_SERVICE_CATEGORY_PROGRAM_DATA_RECEIVED_ACTION);
        intent.putExtra("sender", originatingAddress);
        intent.putParcelableArrayListExtra("program_data", programData);
        CellBroadcastHandler.putPhoneIdAndSubIdExtra(mContext, intent, phoneId);

        String pkg = CellBroadcastHandler.getDefaultCBRPackageName(mContext, intent);
        mReceiverCount.incrementAndGet();
        intent.setPackage(pkg);
        mContext.sendOrderedBroadcast(intent, Manifest.permission.RECEIVE_SMS,
                AppOpsManager.OPSTR_RECEIVE_SMS, mScpResultsReceiver,
                getHandler(), Activity.RESULT_OK, null, null);
        mScpCallback.add(callback);
        return true;
    }

    /**
     * Broadcast receiver to handle results of ordered broadcast. Sends the results back to the
     * framework through a provided callback.
     */
    private final BroadcastReceiver mScpResultsReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            int resultCode = getResultCode();
            if ((resultCode != Activity.RESULT_OK) && (resultCode != Intents.RESULT_SMS_HANDLED)) {
                final String errorMessage = "SCP results error: result code = " + resultCode;
                loge(errorMessage);
                CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                        CELL_BROADCAST_MESSAGE_ERROR__TYPE__CDMA_SCP_HANDLING_ERROR,
                        errorMessage);
                return;
            }
            Bundle extras = getResultExtras(false);
            Consumer<Bundle> callback = mScpCallback.poll();
            callback.accept(extras);
            if (DBG) log("mScpResultsReceiver finished");
            if (mReceiverCount.decrementAndGet() == 0) {
                sendMessage(EVENT_BROADCAST_COMPLETE);
            }
        }
    };
}
