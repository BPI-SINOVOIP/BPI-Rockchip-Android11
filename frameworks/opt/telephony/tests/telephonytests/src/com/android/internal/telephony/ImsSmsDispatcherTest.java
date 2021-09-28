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
 * limitations under the License
 */

package com.android.internal.telephony;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Looper;
import android.telephony.SmsMessage;
import android.telephony.ims.stub.ImsSmsImplBase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import com.android.internal.util.HexDump;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;

import java.util.HashMap;

public class ImsSmsDispatcherTest extends TelephonyTest {
    @Mock private SmsDispatchersController mSmsDispatchersController;
    @Mock private SMSDispatcher.SmsTracker mSmsTracker;
    private HashMap<String, Object> mTrackerData;
    private ImsSmsDispatcher mImsSmsDispatcher;

    @Before
    public void setUp() throws Exception {
        super.setUp(getClass().getSimpleName());
        if (Looper.myLooper() == null) {
            Looper.prepare();
        }

        mImsSmsDispatcher = spy(new ImsSmsDispatcher(mPhone, mSmsDispatchersController));
        when(mSmsDispatchersController.isIms()).thenReturn(true);

        mTrackerData = new HashMap<>(1);
        when(mSmsTracker.getData()).thenReturn(mTrackerData);
    }

    /**
     * Send an SMS and verify that the token and PDU is correct.
     */
    @Test
    @SmallTest
    public void testSendSms() throws Exception {
        int token = mImsSmsDispatcher.mNextToken.get();
        int trackersSize = mImsSmsDispatcher.mTrackers.size();

        byte[] pdu = com.android.internal.telephony.gsm.SmsMessage.getSubmitPdu(null,
                "+15555551212", "Test", false).encodedMessage;
        mTrackerData.put("pdu", pdu);
        when(mImsManager.getSmsFormat()).thenReturn(SmsMessage.FORMAT_3GPP);
        when(mPhone.getPhoneType()).thenReturn(PhoneConstants.PHONE_TYPE_GSM);

        //Send an SMS
        mImsSmsDispatcher.sendSms(mSmsTracker);

        assertEquals(token + 1, mImsSmsDispatcher.mNextToken.get());
        assertEquals(trackersSize + 1, mImsSmsDispatcher.mTrackers.size());
        verify(mImsManager).sendSms(eq(token + 1), anyInt(), eq(SmsMessage.FORMAT_3GPP),
                nullable(String.class), eq(false), eq(pdu));
    }

    /**
     * Ensure that when sending a GSM text fails with SEND_STATUS_ERROR_FALLBACK, retry with
     * a non-zero retry count (set TP-RD).
     */
    @Test
    @SmallTest
    public void testFallbackGsmRetry() throws Exception {
        int token = mImsSmsDispatcher.mNextToken.get();
        mTrackerData.put("pdu", com.android.internal.telephony.gsm.SmsMessage.getSubmitPdu(null,
                "+15555551212", "Test", false).encodedMessage);
        mImsSmsDispatcher.mTrackers.put(token, mSmsTracker);
        when(mPhone.getPhoneType()).thenReturn(PhoneConstants.PHONE_TYPE_GSM);

        // Fallback over GSM
        mImsSmsDispatcher.getSmsListener().onSendSmsResult(token, 0,
                ImsSmsImplBase.SEND_STATUS_ERROR_FALLBACK, 0, SmsResponse.NO_ERROR_CODE);
        ArgumentCaptor<SMSDispatcher.SmsTracker> captor =
                ArgumentCaptor.forClass(SMSDispatcher.SmsTracker.class);
        // Ensure GsmSmsDispatcher calls sendSms
        verify(mSmsDispatchersController).sendRetrySms(captor.capture());

        assertNotNull(captor.getValue());
        assertTrue(captor.getValue().mRetryCount > 0);

    }

    /**
     * Ensure that when an outgoing SMS has failed over IMS with SEND_STATUS_ERROR_RETRY, it is
     * sent over the IMS channel again with the TP-RD bit set.
     */
    @Test
    @SmallTest
    public void testErrorImsRetry() throws Exception {
        int token = mImsSmsDispatcher.mNextToken.get();
        mTrackerData.put("pdu", com.android.internal.telephony.gsm.SmsMessage.getSubmitPdu(null,
                "+15555551212", "Test", false).encodedMessage);
        when(mImsManager.getSmsFormat()).thenReturn(SmsMessage.FORMAT_3GPP);
        mImsSmsDispatcher.mTrackers.put(token, mSmsTracker);
        when(mPhone.getPhoneType()).thenReturn(PhoneConstants.PHONE_TYPE_GSM);

        // Fallback over GSM
        mImsSmsDispatcher.getSmsListener().onSendSmsResult(token, 0,
                ImsSmsImplBase.SEND_STATUS_ERROR_RETRY, 0, SmsResponse.NO_ERROR_CODE);

        // Make sure retry bit set
        ArgumentCaptor<byte[]> byteCaptor = ArgumentCaptor.forClass(byte[].class);
        verify(mImsManager).sendSms(eq(token + 1), anyInt(), nullable(String.class),
                nullable(String.class), eq(true), byteCaptor.capture());
        byte[] pdu = byteCaptor.getValue();
        // Make sure that TP-RD is set for this message
        assertNotNull(pdu);
        assertEquals(0x04, (pdu[0] & 0x04));
    }

    /**
     * Ensure that when a GSM status report is received, it calls acknowledgeSmsReport with correct
     * token and message reference.
     */
    @Test
    @SmallTest
    public void testReceiveGsmSmsStatusReport() throws Exception {
        int sentSmsToken = mImsSmsDispatcher.mNextToken.get();
        int statusReportToken = 456; // Generated by IMS providers
        int messageRef = 123; // TP-MR for sent SMS
        int trackersSize = mImsSmsDispatcher.mTrackers.size();
        // PDU for SMS-STATUS-REPORT
        byte[] pdu = HexDump.hexStringToByteArray("0006000681214365919061800000639190618000006300");

        // Set TP-MR
        pdu[2] = (byte) messageRef;
        mSmsTracker.mMessageRef = messageRef;

        mImsSmsDispatcher.mTrackers.put(sentSmsToken, mSmsTracker);
        when(mPhone.getPhoneType()).thenReturn(PhoneConstants.PHONE_TYPE_GSM);
        when(mSmsDispatchersController.handleSmsStatusReport(
                    eq(mSmsTracker), eq(SmsMessage.FORMAT_3GPP), eq(pdu)))
                .thenReturn(new Pair(true, true));

        // Receive the status report
        mImsSmsDispatcher
                .getSmsListener()
                .onSmsStatusReportReceived(statusReportToken, SmsMessage.FORMAT_3GPP, pdu);

        // Ensure it calls acknowledgeSmsReport with correct token and message reference
        verify(mImsManager)
                .acknowledgeSmsReport(
                        eq(statusReportToken),
                        eq(messageRef),
                        eq(ImsSmsImplBase.STATUS_REPORT_STATUS_OK));
        assertEquals(trackersSize, mImsSmsDispatcher.mTrackers.size());
    }

    /**
     * Ensure that when an outgoing SMS has failed over IMS with SEND_STATUS_ERROR and an associated
     * networkErrorCode, the error is sent to the tracker properly.
     */
    @Test
    @SmallTest
    public void testNetworkError() throws Exception {
        int token = mImsSmsDispatcher.mNextToken.get();
        mTrackerData.put("pdu", com.android.internal.telephony.gsm.SmsMessage.getSubmitPdu(null,
                "+15555551212", "Test", false).encodedMessage);
        when(mImsManager.getSmsFormat()).thenReturn(SmsMessage.FORMAT_3GPP);
        mImsSmsDispatcher.mTrackers.put(token, mSmsTracker);
        when(mPhone.getPhoneType()).thenReturn(PhoneConstants.PHONE_TYPE_GSM);

        // network error 41
        mImsSmsDispatcher.getSmsListener().onSendSmsResult(token, 0,
                ImsSmsImplBase.SEND_STATUS_ERROR, 0, 41);
        verify(mSmsTracker).onFailed(any(Context.class), anyInt(), eq(41));
    }

    @After
    public void tearDown() throws Exception {
        mImsSmsDispatcher = null;
        super.tearDown();
    }
}
