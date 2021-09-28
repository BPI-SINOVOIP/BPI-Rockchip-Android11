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

package android.telephony.ims.cts;

import static junit.framework.Assert.assertTrue;

import static org.junit.Assert.assertEquals;

import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import android.telephony.ims.stub.ImsSmsImplBase;
import android.util.Log;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class TestImsSmsImpl extends ImsSmsImplBase {
    private static final String TAG = "CtsTestImsService";

    private CountDownLatch mSentTriggeredLatch = new CountDownLatch(1);
    private CountDownLatch mOnReadyLatch = new CountDownLatch(1);
    private CountDownLatch mAckDeliveryLatch = new CountDownLatch(1);
    private CountDownLatch mSmsAckLatch = new CountDownLatch(1);
    // Expecting only one message at a time
    public byte[] sentPdu;
    private int mToken;
    private int mMessageRef;
    private int mResult;

    @Override
    public void sendSms(int token, int messageRef, String format, String smsc, boolean isRetry,
            byte[] pdu) {
        if (ImsUtils.VDBG) {
            Log.d(TAG, "ImsSmsImplBase.sendSms called");
        }
        sentPdu = pdu;
        mToken = token;
        mMessageRef = messageRef;

        mSentTriggeredLatch.countDown();
    }

    @Override
    public String getSmsFormat() {
        return SmsMessage.FORMAT_3GPP;
    }

    @Override
    public void onReady() {
        if (ImsUtils.VDBG) {
            Log.d(TAG, "ImsSmsImplBase.onReady called");
        }
        mOnReadyLatch.countDown();

    }

    @Override
    public void acknowledgeSms(int token, int messageRef, int result) {
        mToken = token;
        mMessageRef = messageRef;
        mResult = result;
        mSmsAckLatch.countDown();
    }

    @Override
    public void acknowledgeSmsReport(int token, int messageRef, int result) {
        mToken = token;
        mMessageRef = messageRef;
        mResult = result;
        mAckDeliveryLatch.countDown();
    }

    public void receiveSmsWaitForAcknowledge(int token, String format, byte[] pdu) {
        onSmsReceived(token, format, pdu);
        boolean complete = false;
        try {
            complete = mSmsAckLatch.await(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            // complete == false
        }

        assertTrue("Timed out waiting for acknowledgeSms.", complete);
        assertEquals("Token mismatch.", token, mToken);
        assertTrue("Invalid messageRef", mMessageRef >= 0);
        assertEquals("Invalid result in acknowledgeSms.", DELIVER_STATUS_OK, mResult);
    }

    public void sendReportWaitForAcknowledgeSmsReportR(int token, String format, byte[] pdu) {
        onSmsStatusReportReceived(token, format, pdu);
        boolean complete = false;
        try {
            complete = mAckDeliveryLatch.await(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            // complete = false
        }
        assertTrue("Timed out waiting for delivery report.", complete);
        assertEquals("Ttoken mismatch.", token, mToken);
        assertEquals("Status mismatch.", STATUS_REPORT_STATUS_OK, mResult);
    }


    // Deprecated method for P and Q, where mToken is expected to be the framework token
    public void sendReportWaitForAcknowledgeSmsReportPQ(int messageRef, String format,
            byte[] pdu) {
        onSmsStatusReportReceived(mToken, messageRef, format, pdu);
        boolean complete = false;
        try {
            complete = mAckDeliveryLatch.await(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            // complete = false
        }
        assertTrue("Timed out waiting for delivery report.", complete);
        assertEquals("MessageRef mismatch.", messageRef, mMessageRef);
        assertEquals("Status mismatch.", STATUS_REPORT_STATUS_OK, mResult);
    }

    // P-Q API
    public boolean waitForMessageSentLatch() {
        boolean complete = false;
        try {
            complete = mSentTriggeredLatch.await(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            onSendSmsResult(mToken, mMessageRef, ImsSmsImplBase.SEND_STATUS_OK,
                    SmsManager.RESULT_ERROR_NONE);
        } catch (InterruptedException e) {
            // complete = false
        }
        return complete;
    }

    // R+ API
    public boolean waitForMessageSentLatchSuccess() {
        boolean complete = false;
        try {
            complete = mSentTriggeredLatch.await(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            onSendSmsResultSuccess(mToken, mMessageRef);
        } catch (InterruptedException e) {
            // complete = false
        }
        return complete;
    }

    // R+ API
    public boolean waitForMessageSentLatchError(int resultCode, int networkErrorCode) {
        boolean complete = false;
        try {
            complete = mSentTriggeredLatch.await(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            onSendSmsResultError(mToken, mMessageRef, ImsSmsImplBase.SEND_STATUS_ERROR,
                        resultCode, networkErrorCode);
        } catch (InterruptedException e) {
            // complete = false
        }
        return complete;
    }

    public boolean waitForOnReadyLatch() {
        boolean complete = false;
        try {
            complete = mOnReadyLatch.await(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            // complete = false
        }

        return complete;
    }

}
