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

package com.google.android.chre.test.crossvalidator;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.ContextHubTransaction;
import android.hardware.location.NanoAppBinary;
import android.hardware.location.NanoAppMessage;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.google.android.chre.nanoapp.proto.ChreCrossValidationWifi;
import com.google.android.chre.nanoapp.proto.ChreCrossValidationWifi.Step;
import com.google.android.chre.nanoapp.proto.ChreTestCommon;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Assert;
import org.junit.Assume;

import java.math.BigInteger;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

public class ChreCrossValidatorWifi extends ChreCrossValidatorBase {
    private static final long AWAIT_STEP_RESULT_MESSAGE_TIMEOUT_MS = 1000; // 1 sec
    private static final long AWAIT_WIFI_SCAN_RESULT_TIMEOUT_MS = 3000; // 3 sec

    private static final long NANO_APP_ID = 0x476f6f6754000005L;

    /**
     * Wifi capabilities flags listed in
     * //system/chre/chre_api/include/chre_api/chre/wifi.h
     */
    private static final int WIFI_CAPABILITIES_SCAN_MONITORING = 1;
    private static final int WIFI_CAPABILITIES_ON_DEMAND_SCAN = 2;

    AtomicReference<Step> mStep = new AtomicReference<Step>(Step.INIT);
    AtomicBoolean mDidReceiveNanoAppMessage = new AtomicBoolean(false);

    private AtomicBoolean mApWifiScanSuccess = new AtomicBoolean(false);
    private CountDownLatch mAwaitApWifiSetupScan = new CountDownLatch(1);

    private WifiManager mWifiManager;
    private BroadcastReceiver mWifiScanReceiver;

    private AtomicReference<ChreCrossValidationWifi.WifiCapabilities> mWifiCapabilities =
            new AtomicReference<ChreCrossValidationWifi.WifiCapabilities>(null);

    private AtomicBoolean mWifiScanResultsCompareFinalResult = new AtomicBoolean(false);
    private AtomicReference<String> mWifiScanResultsCompareFinalErrorMessage =
            new AtomicReference<String>(null);

    public ChreCrossValidatorWifi(
            ContextHubManager contextHubManager, ContextHubInfo contextHubInfo,
            NanoAppBinary nanoAppBinary) {
        super(contextHubManager, contextHubInfo, nanoAppBinary);
        Assert.assertTrue("Nanoapp given to cross validator is not the designated chre cross"
                + " validation nanoapp.",
                nanoAppBinary.getNanoAppId() == NANO_APP_ID);
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        mWifiManager = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
        mWifiScanReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context c, Intent intent) {
                Log.i(TAG, "onReceive called");
                boolean success = intent.getBooleanExtra(
                        WifiManager.EXTRA_RESULTS_UPDATED, false);
                mApWifiScanSuccess.set(success);
                mAwaitApWifiSetupScan.countDown();
            }
        };
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION);
        context.registerReceiver(mWifiScanReceiver, intentFilter);
    }

    @Override public void validate() throws AssertionError {
        mCollectingData.set(true);
        sendStepStartMessage(Step.CAPABILITIES);
        waitForMessageFromNanoapp();
        mCollectingData.set(false);
        Assume.assumeTrue("Chre wifi is not enabled",
                          chreWifiHasCapabilities(mWifiCapabilities.get()));

        mCollectingData.set(true);
        sendStepStartMessage(Step.SETUP);
        waitForMessageFromNanoapp();
        mCollectingData.set(false);

        mCollectingData.set(true);
        sendStepStartMessage(Step.VALIDATE);

        Assert.assertTrue("Wifi manager start scan failed", mWifiManager.startScan());
        waitForApScanResults();
        sendWifiScanResultsToChre();

        waitForMessageFromNanoapp();
        mCollectingData.set(false);
    }


    /**
     * Send step start message to nanoapp.
     */
    private void sendStepStartMessage(Step step) {
        mStep.set(step);
        sendMessageToNanoApp(makeStepStartMessage(step));
    }

    /**
     * Send message to the nanoapp.
     */
    private void sendMessageToNanoApp(NanoAppMessage message) {
        int result = mContextHubClient.sendMessageToNanoApp(message);
        if (result != ContextHubTransaction.RESULT_SUCCESS) {
            Assert.fail("Collect data from CHRE failed with result "
                    + contextHubTransactionResultToString(result)
                    + " while trying to send start message during "
                    + getCurrentStepName() + " phase.");
        }
    }

    /**
    * @return The nanoapp message used to start the data collection in chre.
    */
    private NanoAppMessage makeStepStartMessage(Step step) {
        int messageType = ChreCrossValidationWifi.MessageType.STEP_START_VALUE;
        ChreCrossValidationWifi.StepStartCommand stepStartCommand =
                ChreCrossValidationWifi.StepStartCommand.newBuilder()
                .setStep(step).build();
        return NanoAppMessage.createMessageToNanoApp(
                mNappBinary.getNanoAppId(), messageType, stepStartCommand.toByteArray());
    }

    /**
     * Wait for a messaage from the nanoapp.
     */
    private void waitForMessageFromNanoapp() {
        try {
            mAwaitDataLatch.await(AWAIT_STEP_RESULT_MESSAGE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Assert.fail("Interrupted while awaiting " + getCurrentStepName() + " step");
        }
        mAwaitDataLatch = new CountDownLatch(1);
        Assert.assertTrue("Timed out while waiting for step result in " + getCurrentStepName()
                + " step", mDidReceiveNanoAppMessage.get());
        mDidReceiveNanoAppMessage.set(false);
        if (mErrorStr.get() != null) {
            Assert.fail(mErrorStr.get());
        }
    }

    /**
     * @param capabilities The wifi capabilities message from CHRE.
     * @return true if CHRE wifi has the necessary capabilities to run the test.
     */
    private boolean chreWifiHasCapabilities(ChreCrossValidationWifi.WifiCapabilities capabilities) {
        return (capabilities.getWifiCapabilities() & WIFI_CAPABILITIES_SCAN_MONITORING) != 0
            && (capabilities.getWifiCapabilities() & WIFI_CAPABILITIES_ON_DEMAND_SCAN) != 0;
    }

    private void waitForApScanResults() {
        try {
            mAwaitApWifiSetupScan.await(AWAIT_WIFI_SCAN_RESULT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Assert.fail("Interrupted while awaiting ap wifi scan result");
        }
        Assert.assertTrue("AP wifi scan result failed asynchronously", mApWifiScanSuccess.get());
    }

    private void sendWifiScanResultsToChre() {
        List<ScanResult> results = mWifiManager.getScanResults();
        Assert.assertTrue("No wifi scan results returned from AP", !results.isEmpty());
        for (int i = 0; i < results.size(); i++) {
            sendMessageToNanoApp(makeWifiScanResultMessage(results.get(i), results.size(), i));
        }
    }

    private NanoAppMessage makeWifiScanResultMessage(ScanResult result, int totalNumResults,
                                                     int resultIndex) {
        int messageType = ChreCrossValidationWifi.MessageType.SCAN_RESULT_VALUE;
        ChreCrossValidationWifi.WifiScanResult scanResult = ChreCrossValidationWifi.WifiScanResult
                .newBuilder().setSsid(result.SSID)
                .setBssid(ByteString.copyFrom(bssidToBytes(result.BSSID)))
                .setTotalNumResults(totalNumResults).setResultIndex(resultIndex).build();
        NanoAppMessage message = NanoAppMessage.createMessageToNanoApp(
                mNappBinary.getNanoAppId(), messageType, scanResult.toByteArray());
        return message;
    }

    @Override
    protected void parseDataFromNanoAppMessage(NanoAppMessage message) {
        mDidReceiveNanoAppMessage.set(true);
        if (message.getMessageType()
                == ChreCrossValidationWifi.MessageType.STEP_RESULT_VALUE) {
            ChreTestCommon.TestResult testResult = null;
            try {
                testResult = ChreTestCommon.TestResult.parseFrom(message.getMessageBody());
            } catch (InvalidProtocolBufferException e) {
                setErrorStr("Error parsing protobuff: " + e);
                mAwaitDataLatch.countDown();
                return;
            }
            boolean success = getSuccessFromTestResult(testResult);
            if (mStep.get() == Step.SETUP || mStep.get() == Step.VALIDATE) {
                if (success) {
                    Log.i(TAG, getCurrentStepName() + " step success");
                } else {
                    setErrorStr(getCurrentStepName() + " step failed: "
                            + testResult.getErrorMessage().toStringUtf8());
                }
            } else {
                setErrorStr("Received a step result message during step " + getCurrentStepName());
            }
        } else if (message.getMessageType()
                == ChreCrossValidationWifi.MessageType.WIFI_CAPABILITIES_VALUE) {
            if (mStep.get() != Step.CAPABILITIES) {
                setErrorStr("Received a capabilities message during step " + getCurrentStepName());
            }
            ChreCrossValidationWifi.WifiCapabilities capabilities = null;
            try {
                capabilities = ChreCrossValidationWifi.WifiCapabilities.parseFrom(
                        message.getMessageBody());
            } catch (InvalidProtocolBufferException e) {
                setErrorStr("Error parsing protobuff: " + e);
                mAwaitDataLatch.countDown();
                return;
            }
            mWifiCapabilities.set(capabilities);
        } else {
            setErrorStr(String.format("Received message with unexpected type: %d",
                                      message.getMessageType()));
        }
        // Each message should countdown the latch no matter success or fail
        mAwaitDataLatch.countDown();
    }

    /**
     * @return The boolean indicating test result success or failure from TestResult proto message.
     */
    private boolean getSuccessFromTestResult(ChreTestCommon.TestResult testResult) {
        return testResult.getCode() == ChreTestCommon.TestResult.Code.PASSED;
    }

    /**
     * @return The string name of the current phase.
     */
    private String getCurrentStepName() {
        switch (mStep.get()) {
            case INIT:
                return "INIT";
            case SETUP:
                return "SETUP";
            case VALIDATE:
                return "VALIDATE";
            default:
                return "UNKNOWN";
        }
    }

    private static byte[] bssidToBytes(String bssid) {
        // the ScanResult.BSSID field comes in format ff:ff:ff:ff:ff and needs to be converted to
        // bytes in order to be compared to CHRE bssid
        return (new BigInteger(bssid.replace(":" , ""), 16)).toByteArray();
    }

    // TODO: Implement this method
    @Override
    protected void unregisterApDataListener() {}
}
