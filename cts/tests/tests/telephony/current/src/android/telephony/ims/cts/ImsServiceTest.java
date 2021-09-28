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

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;
import static junit.framework.Assert.assertTrue;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.app.UiAutomation;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.PersistableBundle;
import android.telephony.AccessNetworkConstants;
import android.telephony.CarrierConfigManager;
import android.telephony.SmsManager;
import android.telephony.SmsMessage;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.cts.AsyncSmsMessageListener;
import android.telephony.cts.SmsReceiverHelper;
import android.telephony.ims.ImsException;
import android.telephony.ims.ImsManager;
import android.telephony.ims.ImsMmTelManager;
import android.telephony.ims.ImsRcsManager;
import android.telephony.ims.ImsReasonInfo;
import android.telephony.ims.ProvisioningManager;
import android.telephony.ims.RegistrationManager;
import android.telephony.ims.feature.ImsFeature;
import android.telephony.ims.feature.MmTelFeature;
import android.telephony.ims.feature.RcsFeature.RcsImsCapabilities;
import android.telephony.ims.stub.ImsConfigImplBase;
import android.telephony.ims.stub.ImsFeatureConfiguration;
import android.telephony.ims.stub.ImsRegistrationImplBase;
import android.util.Base64;
import android.util.Pair;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.ShellIdentityUtils;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * CTS tests for ImsService API.
 */
@RunWith(AndroidJUnit4.class)
public class ImsServiceTest {

    private static ImsServiceConnector sServiceConnector;

    private static final int RCS_CAP_NONE = RcsImsCapabilities.CAPABILITY_TYPE_NONE;
    private static final int RCS_CAP_OPTIONS = RcsImsCapabilities.CAPABILITY_TYPE_OPTIONS_UCE;
    private static final int RCS_CAP_PRESENCE = RcsImsCapabilities.CAPABILITY_TYPE_PRESENCE_UCE;

    private static final String MSG_CONTENTS = "hi";
    private static final String EXPECTED_RECEIVED_MESSAGE = "foo5";
    private static final String DEST_NUMBER = "5555554567";
    private static final String SRC_NUMBER = "5555551234";
    private static final byte[] EXPECTED_PDU =
            new byte[]{1, 0, 10, -127, 85, 85, 85, 33, 67, 0, 0, 2, -24, 52};
    private static final String RECEIVED_MESSAGE = "B5EhYBMDIPgEC5FhBWKFkPEAAEGQQlGDUooE5ve7Bg==";
    private static final byte[] STATUS_REPORT_PDU =
            hexStringToByteArray("0006000681214365919061800000639190618000006300");

    private static int sTestSlot = 0;
    private static int sTestSub = SubscriptionManager.INVALID_SUBSCRIPTION_ID;

    private static final int TEST_CONFIG_KEY = 1000;
    private static final int TEST_CONFIG_VALUE_INT = 0xDEADBEEF;
    private static final String TEST_CONFIG_VALUE_STRING = "DEADBEEF";
    private static final String TEST_AUTOCONFIG_CONTENT = "<?xml version=\"1.0\"?>\n"
            + "<wap-provisioningdoc version=\"1.1\">\n"
            + "<characteristic type=\"VERS\">\n"
            + "<parm name=\"version\" value=\"1\"/>\n"
            + "<parm name=\"validity\" value=\"1728000\"/>\n"
            + "</characteristic>"
            + "</wap-provisioningdoc>";

    private static CarrierConfigReceiver sReceiver;

    private static class CarrierConfigReceiver extends BroadcastReceiver {
        private CountDownLatch mLatch = new CountDownLatch(1);
        private final int mSubId;

        CarrierConfigReceiver(int subId) {
            mSubId = subId;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED.equals(intent.getAction())) {
                int subId = intent.getIntExtra(CarrierConfigManager.EXTRA_SUBSCRIPTION_INDEX, -1);
                if (mSubId == subId) {
                    mLatch.countDown();
                }
            }
        }

        void clearQueue() {
            mLatch = new CountDownLatch(1);
        }

        void waitForCarrierConfigChanged() throws Exception {
            mLatch.await(5000, TimeUnit.MILLISECONDS);
        }
    }

    @BeforeClass
    public static void beforeAllTests() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        TelephonyManager tm = (TelephonyManager) getContext()
                .getSystemService(Context.TELEPHONY_SERVICE);
        sTestSub = ImsUtils.getPreferredActiveSubId();
        sTestSlot = SubscriptionManager.getSlotIndex(sTestSub);
        if (tm.getSimState(sTestSlot) != TelephonyManager.SIM_STATE_READY) {
            return;
        }
        sServiceConnector = new ImsServiceConnector(InstrumentationRegistry.getInstrumentation());
        // Remove all live ImsServices until after these tests are done
        sServiceConnector.clearAllActiveImsServices(sTestSlot);
        // Configure SMS receiver based on the Android version.
        sServiceConnector.setDefaultSmsApp();

        sReceiver = new CarrierConfigReceiver(sTestSub);
        IntentFilter filter = new IntentFilter(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED);
        // ACTION_CARRIER_CONFIG_CHANGED is sticky, so we will get a callback right away.
        InstrumentationRegistry.getInstrumentation().getContext()
                .registerReceiver(sReceiver, filter);
    }

    @AfterClass
    public static void afterAllTests() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Restore all ImsService configurations that existed before the test.
        if (sServiceConnector != null) {
            sServiceConnector.disconnectServices();
        }
        sServiceConnector = null;

        // Ensure there are no CarrierConfig overrides as well as reset the ImsResolver in case the
        // ImsService override changed in CarrierConfig while we were overriding it.
        overrideCarrierConfig(null);

        if (sReceiver != null) {
            InstrumentationRegistry.getInstrumentation().getContext().unregisterReceiver(sReceiver);
            sReceiver = null;
        }
    }

    @Before
    public void beforeTest() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        TelephonyManager tm = (TelephonyManager) InstrumentationRegistry.getInstrumentation()
                .getContext().getSystemService(Context.TELEPHONY_SERVICE);
        if (tm.getSimState(sTestSlot) != TelephonyManager.SIM_STATE_READY) {
            fail("This test requires that there is a SIM in the device!");
        }
        // Sanity check: ensure that the subscription hasn't changed between tests.
        int[] subs = SubscriptionManager.getSubId(sTestSlot);

        if (subs == null) {
            fail("This test requires there is an active subscription in slot " + sTestSlot);
        }
        boolean isFound = false;
        for (int sub : subs) {
            isFound |= (sTestSub == sub);
        }
        if (!isFound) {
            fail("Invalid state found: the test subscription in slot " + sTestSlot + " changed "
                    + "during this test.");
        }
    }

    @After
    public void afterTest() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Unbind the GTS ImsService after the test completes.
        if (sServiceConnector != null) {
            sServiceConnector.disconnectCarrierImsService();
            sServiceConnector.disconnectDeviceImsService();
        }
    }

    @Test
    public void testCarrierImsServiceBindRcsFeature() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to the ImsService with the RCS feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_RCS)
                .build()));
        // The RcsFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_RCS);
        assertNotNull("ImsService created, but ImsService#createRcsFeature was not called!",
                sServiceConnector.getCarrierService().getRcsFeature());
    }

    @Test
    public void testCarrierImsServiceBindMmTelFeature() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to the ImsService with the MmTel feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                .build()));
        // The MmTelFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL);
        assertNotNull("ImsService created, but ImsService#createMmTelFeature was not called!",
                sServiceConnector.getCarrierService().getMmTelFeature());
    }

    @Test
    public void testCarrierImsServiceBindRcsFeatureEnableDisableIms() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to the ImsService with the RCS feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_RCS)
                .build()));
        // The RcsFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_RCS));

        //Enable IMS and ensure that we receive the call to enable IMS in the ImsService.
        sServiceConnector.enableImsService(sTestSlot);
        // Wait for command in ImsService
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_ENABLE_IMS));
        assertTrue(sServiceConnector.getCarrierService().isEnabled());

        //Disable IMS and ensure that we receive the call to enable IMS in the ImsService.
        sServiceConnector.disableImsService(sTestSlot);
        // Wait for command in ImsService
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_DISABLE_IMS));
        assertFalse(sServiceConnector.getCarrierService().isEnabled());
    }

    @Test
    public void testCarrierImsServiceBindRcsChangeToMmtel() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to the ImsService with the RCS feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_RCS)
                .build()));
        // The RcsFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_RCS));

        // Change the supported feature to MMTEl
        sServiceConnector.getCarrierService().getImsService().onUpdateSupportedImsFeatures(
                new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL).build());

        // createMmTelFeature should be called.
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL));
    }

    @Test
    public void testCarrierImsServiceBindMmTelNoEmergency() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to the ImsService with the MMTEL feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                .build()));
        // The MmTelFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL));
    }

    @Test
    public void testCarrierImsServiceBindMmTelEmergencyEnabled() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to the ImsService with the MMTEL feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                .addFeature(sTestSlot, ImsFeature.FEATURE_EMERGENCY_MMTEL)
                .build()));
        // The MmTelFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL));
    }

    @Test
    public void testDeviceImsServiceBindRcsFeature() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to the ImsService with the RCS feature.
        assertTrue(sServiceConnector.connectDeviceImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_RCS)
                .build()));
        // The RcsFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        assertTrue(sServiceConnector.getExternalService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_RCS));
        // Make sure the RcsFeature was created in the test service.
        assertTrue("Device ImsService created, but TestDeviceImsService#createRcsFeature was not"
                        + "called!", sServiceConnector.getExternalService().isRcsFeatureCreated());
    }

    @Test
    public void testBindDeviceAndCarrierDifferentFeatures() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to Device the ImsService with the MMTEL/EMERGENCY_MMTEL feature.
        assertTrue(sServiceConnector.connectDeviceImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_EMERGENCY_MMTEL)
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                .build()));
        // Connect to Device the ImsService with the RCS feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_RCS)
                .build()));
        // The MmTelFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        assertTrue(sServiceConnector.getExternalService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL));
        // Make sure the MmTelFeature was created in the test service.
        assertTrue("Device ImsService created, but TestDeviceImsService#createMmTelFeature was"
                + "not called!", sServiceConnector.getExternalService().isMmTelFeatureCreated());

        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_RCS));
        assertNotNull("ImsService created, but ImsService#createRcsFeature was not called!",
                sServiceConnector.getCarrierService().getRcsFeature());
    }

    @Test
    public void testBindDeviceAndCarrierSameFeature() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to Device the ImsService with the RCS feature.
        assertTrue(sServiceConnector.connectDeviceImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                .build()));

        //First MMTEL feature is created on device ImsService.
        assertTrue(sServiceConnector.getExternalService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL));
        assertTrue("Device ImsService created, but TestDeviceImsService#createMmTelFeature was "
                + "not called!", sServiceConnector.getExternalService().isMmTelFeatureCreated());

        // Connect to Device the ImsService with the MMTEL feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                .addFeature(sTestSlot, ImsFeature.FEATURE_EMERGENCY_MMTEL)
                .build()));

        // Next MMTEL feature is created on carrier ImsService (and unbound on device)
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL));
        assertNotNull("ImsService created, but ImsService#createRcsFeature was not called!",
                sServiceConnector.getCarrierService().getMmTelFeature());

        // Ensure that the MmTelFeature was removed on the device ImsService.
        assertTrue(sServiceConnector.getExternalService().waitForLatchCountdown(
                TestImsService.LATCH_REMOVE_MMTEL));
        assertFalse("Device ImsService was never removed when carrier ImsService took MMTEL."
                + "feature.", sServiceConnector.getExternalService().isMmTelFeatureCreated());
    }

    @Test
    public void testBindDeviceAndCarrierUpdateToSameFeature() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Connect to Device the ImsService with the MMTEL feature.
        assertTrue(sServiceConnector.connectDeviceImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                .build()));

        //First MMTEL feature is created on device ImsService.
        assertTrue(sServiceConnector.getExternalService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL));
        assertTrue("Device ImsService created, but TestDeviceImsService#createMmTelFeature was"
                + "not called!", sServiceConnector.getExternalService().isMmTelFeatureCreated());

        // Connect to Device the ImsService with the RCS feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_RCS)
                .build()));

        // Next Rcs feature is created on carrier ImsService
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_RCS));
        assertNotNull("ImsService created, but ImsService#createRcsFeature was not called!",
                sServiceConnector.getCarrierService().getRcsFeature());

        // Change the supported feature to MMTEl
        sServiceConnector.getCarrierService().getImsService().onUpdateSupportedImsFeatures(
                new ImsFeatureConfiguration.Builder()
                        .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                        .addFeature(sTestSlot, ImsFeature.FEATURE_EMERGENCY_MMTEL)
                        .build());

        // MMTEL feature is created on carrier ImsService
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL));
        assertNotNull("ImsService created, but ImsService#createMmTelFeature was not called!",
                sServiceConnector.getCarrierService().getMmTelFeature());

        // Ensure that the MmTelFeature was removed on the device ImsService.
        assertTrue(sServiceConnector.getExternalService().waitForLatchCountdown(
                TestImsService.LATCH_REMOVE_MMTEL));
        assertFalse("Device ImsService was never removed when carrier ImsService took MMTEL."
                + "feature.", sServiceConnector.getExternalService().isMmTelFeatureCreated());

        // Ensure that the RcsFeature was removed on the carrier ImsService.
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_REMOVE_RCS));
        assertNull(sServiceConnector.getCarrierService().getRcsFeature());
    }

    @Test
    public void testMmTelSendSms() throws Exception {
        if (!ImsUtils.shouldRunSmsImsTests(sTestSub)) {
            return;
        }

        setupImsServiceForSms();
        // Send Message with sent PendingIntent requested
        SmsManager.getSmsManagerForSubscriptionId(sTestSub).sendTextMessage(SRC_NUMBER,
                DEST_NUMBER, MSG_CONTENTS, SmsReceiverHelper.getMessageSentPendingIntent(
                        InstrumentationRegistry.getInstrumentation().getTargetContext()), null);
        assertTrue(sServiceConnector.getCarrierService().getMmTelFeature()
                .getSmsImplementation().waitForMessageSentLatch());

        // Wait for send PendingIntent
        Intent intent = AsyncSmsMessageListener.getInstance().waitForMessageSentIntent(
                ImsUtils.TEST_TIMEOUT_MS);
        assertNotNull("SMS send PendingIntent never received", intent);
        assertEquals("SMS send PendingIntent should have result RESULT_OK",
                Activity.RESULT_OK, intent.getIntExtra(SmsReceiverHelper.EXTRA_RESULT_CODE,
                        Activity.RESULT_CANCELED));

        // Ensure we receive correct PDU on the other side.
        Assert.assertArrayEquals(EXPECTED_PDU, sServiceConnector.getCarrierService()
                .getMmTelFeature().getSmsImplementation().sentPdu);
    }

    @Test
    public void testMmTelSendSmsDeliveryReportQCompat() throws Exception {
        if (!ImsUtils.shouldRunSmsImsTests(sTestSub)) {
            return;
        }

        setupImsServiceForSms();
        // Send Message with sent PendingIntent requested
        SmsManager.getSmsManagerForSubscriptionId(sTestSub).sendTextMessage(SRC_NUMBER,
                DEST_NUMBER, MSG_CONTENTS, null, SmsReceiverHelper.getMessageDeliveredPendingIntent(
                        InstrumentationRegistry.getInstrumentation().getTargetContext()));
        assertTrue(sServiceConnector.getCarrierService().getMmTelFeature()
                .getSmsImplementation().waitForMessageSentLatch());

        // Ensure we receive correct PDU on the other side.
        // Set TP-Status-Report-Request bit as well for this case.
        byte[] pduWithStatusReport = EXPECTED_PDU.clone();
        pduWithStatusReport[0] |= 0x20;
        Assert.assertArrayEquals(pduWithStatusReport, sServiceConnector.getCarrierService()
                .getMmTelFeature().getSmsImplementation().sentPdu);

        // Ensure the API works on Q as well as in R+, where it was deprecated.
        sServiceConnector.getCarrierService().getMmTelFeature().getSmsImplementation()
                .sendReportWaitForAcknowledgeSmsReportPQ(0, SmsMessage.FORMAT_3GPP,
                        STATUS_REPORT_PDU);

        // Wait for delivered PendingIntent
        Intent intent = AsyncSmsMessageListener.getInstance().waitForMessageDeliveredIntent(
                ImsUtils.TEST_TIMEOUT_MS);
        assertNotNull("SMS delivered PendingIntent never received", intent);
        assertEquals("SMS delivered PendingIntent should have result RESULT_OK",
                Activity.RESULT_OK, intent.getIntExtra(SmsReceiverHelper.EXTRA_RESULT_CODE,
                        Activity.RESULT_CANCELED));
    }

    @Test
    public void testMmTelSendSmsDeliveryReportR() throws Exception {
        if (!ImsUtils.shouldRunSmsImsTests(sTestSub)) {
            return;
        }

        setupImsServiceForSms();
        // Send Message with sent PendingIntent requested
        SmsManager.getSmsManagerForSubscriptionId(sTestSub).sendTextMessage(SRC_NUMBER,
                DEST_NUMBER, MSG_CONTENTS, null, SmsReceiverHelper.getMessageDeliveredPendingIntent(
                        InstrumentationRegistry.getInstrumentation().getTargetContext()));
        assertTrue(sServiceConnector.getCarrierService().getMmTelFeature()
                .getSmsImplementation().waitForMessageSentLatch());

        // Ensure we receive correct PDU on the other side.
        // Set TP-Status-Report-Request bit as well for this case.
        byte[] pduWithStatusReport = EXPECTED_PDU.clone();
        pduWithStatusReport[0] |= 0x20;
        Assert.assertArrayEquals(pduWithStatusReport, sServiceConnector.getCarrierService()
                .getMmTelFeature().getSmsImplementation().sentPdu);

        sServiceConnector.getCarrierService().getMmTelFeature().getSmsImplementation()
                .sendReportWaitForAcknowledgeSmsReportR(123456789, SmsMessage.FORMAT_3GPP,
                        STATUS_REPORT_PDU);

        // Wait for delivered PendingIntent
        Intent intent = AsyncSmsMessageListener.getInstance().waitForMessageDeliveredIntent(
                ImsUtils.TEST_TIMEOUT_MS);
        assertNotNull("SMS delivered PendingIntent never received", intent);
        assertEquals("SMS delivered PendingIntent should have result RESULT_OK",
                Activity.RESULT_OK, intent.getIntExtra(SmsReceiverHelper.EXTRA_RESULT_CODE,
                        Activity.RESULT_CANCELED));
    }

    @Test
    public void testMmTelSendSmsRSuccess() throws Exception {
        if (!ImsUtils.shouldRunSmsImsTests(sTestSub)) {
            return;
        }

        setupImsServiceForSms();

        // Send Message
        SmsManager.getSmsManagerForSubscriptionId(sTestSub).sendTextMessage(SRC_NUMBER,
                DEST_NUMBER, MSG_CONTENTS, SmsReceiverHelper.getMessageSentPendingIntent(
                        InstrumentationRegistry.getInstrumentation().getTargetContext()), null);
        // Use R specific API for sending SMS result
        assertTrue(sServiceConnector.getCarrierService().getMmTelFeature()
                .getSmsImplementation().waitForMessageSentLatchSuccess());
        Intent intent = AsyncSmsMessageListener.getInstance().waitForMessageSentIntent(
                ImsUtils.TEST_TIMEOUT_MS);
        assertNotNull(intent);
        assertEquals(Activity.RESULT_OK, intent.getIntExtra(SmsReceiverHelper.EXTRA_RESULT_CODE,
                    Activity.RESULT_CANCELED));

        // Ensure we receive correct PDU on the other side.
        Assert.assertArrayEquals(EXPECTED_PDU, sServiceConnector.getCarrierService()
                .getMmTelFeature().getSmsImplementation().sentPdu);
    }

    @Test
    public void testMmTelSendSmsNetworkError() throws Exception {
        if (!ImsUtils.shouldRunSmsImsTests(sTestSub)) {
            return;
        }

        setupImsServiceForSms();

        // Send Message
        SmsManager.getSmsManagerForSubscriptionId(sTestSub).sendTextMessage(SRC_NUMBER,
                DEST_NUMBER, MSG_CONTENTS, SmsReceiverHelper.getMessageSentPendingIntent(
                        InstrumentationRegistry.getInstrumentation().getContext()), null);
        assertTrue(sServiceConnector.getCarrierService().getMmTelFeature()
                .getSmsImplementation().waitForMessageSentLatchError(
                        SmsManager.RESULT_ERROR_GENERIC_FAILURE, 41));
        Intent intent = AsyncSmsMessageListener.getInstance().waitForMessageSentIntent(
                ImsUtils.TEST_TIMEOUT_MS);
        assertNotNull(intent);
        // In the case of error, the PendingIntent result will not report OK
        assertNotEquals(Activity.RESULT_OK, intent.getIntExtra(SmsReceiverHelper.EXTRA_RESULT_CODE,
                Activity.RESULT_OK));
        // make sure the "errorCode" extra contains the network error code returned by the
        // ImsService.
        assertEquals(41, intent.getIntExtra("errorCode", 0));

        // Ensure we receive correct PDU on the other side.
        Assert.assertArrayEquals(EXPECTED_PDU, sServiceConnector.getCarrierService()
                .getMmTelFeature().getSmsImplementation().sentPdu);
    }

    @Test
    public void testMmTelReceiveSms() throws Exception {
        if (!ImsUtils.shouldRunSmsImsTests(sTestSub)) {
            return;
        }

        setupImsServiceForSms();

        // Message received
        sServiceConnector.getCarrierService().getMmTelFeature().getSmsImplementation()
                .receiveSmsWaitForAcknowledge(123456789, SmsMessage.FORMAT_3GPP,
                        Base64.decode(RECEIVED_MESSAGE, Base64.DEFAULT));

        // Wait for SMS received intent and ensure it is correct.
        String receivedMessage = AsyncSmsMessageListener.getInstance()
                .waitForSmsMessage(ImsUtils.TEST_TIMEOUT_MS);
        assertEquals(EXPECTED_RECEIVED_MESSAGE, receivedMessage);
    }

    @Test
    public void testGetFeatureState() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // This will set feature state to ready
        triggerFrameworkConnectToCarrierImsService();

        Integer result = getFeatureState();
        assertNotNull(result);
        assertEquals("ImsService state should be STATE_READY",
                sServiceConnector.getCarrierService().getMmTelFeature().getFeatureState(),
                ImsFeature.STATE_READY);
        assertTrue("ImsService state is ready, but STATE_READY is not reported.",
                ImsUtils.retryUntilTrue(() -> (getFeatureState() == ImsFeature.STATE_READY)));

        sServiceConnector.getCarrierService().getMmTelFeature().setFeatureState(
                ImsFeature.STATE_INITIALIZING);
        result = getFeatureState();
        assertNotNull(result);
        assertEquals("ImsService state should be STATE_INITIALIZING",
                sServiceConnector.getCarrierService().getMmTelFeature().getFeatureState(),
                ImsFeature.STATE_INITIALIZING);
        assertTrue("ImsService state is initializing, but STATE_INITIALIZING is not reported.",
                ImsUtils.retryUntilTrue(
                        () -> (getFeatureState() == ImsFeature.STATE_INITIALIZING)));

        sServiceConnector.getCarrierService().getMmTelFeature().setFeatureState(
                ImsFeature.STATE_UNAVAILABLE);
        result = getFeatureState();
        assertNotNull(result);
        assertEquals("ImsService state should be STATE_UNAVAILABLE",
                sServiceConnector.getCarrierService().getMmTelFeature().getFeatureState(),
                ImsFeature.STATE_UNAVAILABLE);
        assertTrue("ImsService state is unavailable, but STATE_UNAVAILABLE is not reported.",
                ImsUtils.retryUntilTrue(
                        () -> (getFeatureState() == ImsFeature.STATE_UNAVAILABLE)));
    }

    private Integer getFeatureState() throws Exception {
        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mmTelManager = imsManager.getImsMmTelManager(sTestSub);
        LinkedBlockingQueue<Integer> state = new LinkedBlockingQueue<>(1);
        ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(mmTelManager,
                (m) -> m.getFeatureState(Runnable::run, state::offer), ImsException.class);
        return state.poll(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    @Test
    public void testMmTelManagerRegistrationCallback() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        triggerFrameworkConnectToCarrierImsService();

        // Start deregistered
        sServiceConnector.getCarrierService().getImsRegistration().onDeregistered(
                new ImsReasonInfo(ImsReasonInfo.CODE_LOCAL_NOT_REGISTERED,
                        ImsReasonInfo.CODE_UNSPECIFIED, ""));

        // This is a little bit gross looking, but on P devices, I can not define classes that
        // extend ImsMmTelManager.RegistrationCallback (because it doesn't exist), so this has to
        // happen as an anon class here.
        LinkedBlockingQueue<Integer> mQueue = new LinkedBlockingQueue<>();
        // Deprecated in R, see testMmTelManagerRegistrationCallbackR below.
        ImsMmTelManager.RegistrationCallback callback = new ImsMmTelManager.RegistrationCallback() {
            @Override
            public void onRegistered(int imsTransportType) {
                mQueue.offer(imsTransportType);
            }

            @Override
            public void onRegistering(int imsTransportType) {
                mQueue.offer(imsTransportType);
            }

            @Override
            public void onUnregistered(ImsReasonInfo info) {
                mQueue.offer(info.getCode());
            }

            @Override
            public void onTechnologyChangeFailed(int imsTransportType, ImsReasonInfo info) {
                mQueue.offer(imsTransportType);
                mQueue.offer(info.getCode());
            }
        };

        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            // First try without the correct permissions.
            ImsManager imsManager = getContext().getSystemService(ImsManager.class);
            ImsMmTelManager mmTelManager = imsManager.getImsMmTelManager(sTestSub);
            mmTelManager.registerImsRegistrationCallback(getContext().getMainExecutor(), callback);
            fail("registerImsRegistrationCallback requires READ_PRECISE_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }

        // Latch will count down here (we callback on the state during registration).
        try {
            automan.adoptShellPermissionIdentity();
            ImsManager imsManager = getContext().getSystemService(ImsManager.class);
            ImsMmTelManager mmTelManager = imsManager.getImsMmTelManager(sTestSub);
            mmTelManager.registerImsRegistrationCallback(getContext().getMainExecutor(), callback);
        } finally {
            automan.dropShellPermissionIdentity();
        }
        assertEquals(ImsReasonInfo.CODE_LOCAL_NOT_REGISTERED, waitForIntResult(mQueue));


        // Start registration
        sServiceConnector.getCarrierService().getImsRegistration().onRegistering(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WWAN, waitForIntResult(mQueue));

        // Complete registration
        sServiceConnector.getCarrierService().getImsRegistration().onRegistered(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WWAN, waitForIntResult(mQueue));

        // Fail handover to IWLAN
        sServiceConnector.getCarrierService().getImsRegistration().onTechnologyChangeFailed(
                ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN,
                new ImsReasonInfo(ImsReasonInfo.CODE_LOCAL_HO_NOT_FEASIBLE,
                        ImsReasonInfo.CODE_UNSPECIFIED, ""));
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WLAN, waitForIntResult(mQueue));
        assertEquals(ImsReasonInfo.CODE_LOCAL_HO_NOT_FEASIBLE, waitForIntResult(mQueue));

        // Ensure null ImsReasonInfo still results in non-null callback value.
        sServiceConnector.getCarrierService().getImsRegistration().onTechnologyChangeFailed(
                ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN, null);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WLAN, waitForIntResult(mQueue));
        assertEquals(ImsReasonInfo.CODE_UNSPECIFIED, waitForIntResult(mQueue));

        // Ensure null ImsReasonInfo still results in non-null callback.
        sServiceConnector.getCarrierService().getImsRegistration().onDeregistered(null);
        assertEquals(ImsReasonInfo.CODE_UNSPECIFIED, waitForIntResult(mQueue));

        try {
            automan.adoptShellPermissionIdentity();
            ImsManager imsManager = getContext().getSystemService(ImsManager.class);
            ImsMmTelManager mmTelManager = imsManager.getImsMmTelManager(sTestSub);
            mmTelManager.unregisterImsRegistrationCallback(callback);
        } finally {
            automan.dropShellPermissionIdentity();
        }

        try {
            ImsManager imsManager = getContext().getSystemService(ImsManager.class);
            ImsMmTelManager mmTelManager = imsManager.getImsMmTelManager(sTestSub);
            mmTelManager.unregisterImsRegistrationCallback(callback);
            fail("unregisterImsRegistrationCallback requires READ_PRECISE_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }
    }

    @Ignore("RCS APIs not public yet")
    @Test
    public void testRcsManagerRegistrationCallback() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        if (imsManager == null) {
            fail("Cannot find IMS service");
        }

        // Connect to device ImsService with RcsFeature
        triggerFrameworkConnectToLocalImsServiceBindRcsFeature();
        ImsRcsManager imsRcsManager = imsManager.getImsRcsManager(sTestSub);

        // Wait for the framework to set the capabilities on the ImsService
        sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_RCS_CAP_SET);

        // Start de-registered
        sServiceConnector.getCarrierService().getImsRegistration().onDeregistered(
                new ImsReasonInfo(ImsReasonInfo.CODE_LOCAL_NOT_REGISTERED,
                        ImsReasonInfo.CODE_UNSPECIFIED, ""));

        LinkedBlockingQueue<Integer> mQueue = new LinkedBlockingQueue<>();
        RegistrationManager.RegistrationCallback callback =
                new RegistrationManager.RegistrationCallback() {
                    @Override
                    public void onRegistered(int imsTransportType) {
                        mQueue.offer(imsTransportType);
                    }

                    @Override
                    public void onRegistering(int imsTransportType) {
                        mQueue.offer(imsTransportType);
                    }

                    @Override
                    public void onUnregistered(ImsReasonInfo info) {
                        mQueue.offer(info.getCode());
                    }

                    @Override
                    public void onTechnologyChangeFailed(int imsTransportType, ImsReasonInfo info) {
                        mQueue.offer(imsTransportType);
                        mQueue.offer(info.getCode());
                    }
                };

        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            automan.adoptShellPermissionIdentity();
            imsRcsManager.registerImsRegistrationCallback(getContext().getMainExecutor(), callback);
        } finally {
            automan.dropShellPermissionIdentity();
        }
        // Verify it's not registered
        assertEquals(ImsReasonInfo.CODE_LOCAL_NOT_REGISTERED, waitForIntResult(mQueue));

        // Start registration
        sServiceConnector.getCarrierService().getImsRegistration().onRegistering(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WWAN, waitForIntResult(mQueue));

        // Complete registration
        sServiceConnector.getCarrierService().getImsRegistration().onRegistered(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WWAN, waitForIntResult(mQueue));

        // Fail handover to IWLAN
        sServiceConnector.getCarrierService().getImsRegistration().onTechnologyChangeFailed(
                ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN,
                new ImsReasonInfo(ImsReasonInfo.CODE_LOCAL_HO_NOT_FEASIBLE,
                        ImsReasonInfo.CODE_UNSPECIFIED, ""));
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WLAN, waitForIntResult(mQueue));
        assertEquals(ImsReasonInfo.CODE_LOCAL_HO_NOT_FEASIBLE, waitForIntResult(mQueue));

        try {
            automan.adoptShellPermissionIdentity();
            imsRcsManager.unregisterImsRegistrationCallback(callback);
        } finally {
            automan.dropShellPermissionIdentity();
        }
    }

    @Test
    public void testMmTelManagerRegistrationStateR() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        RegistrationManager regManager = imsManager.getImsMmTelManager(sTestSub);
        LinkedBlockingQueue<Integer> mQueue = new LinkedBlockingQueue<>();

        triggerFrameworkConnectToCarrierImsService();

        // Start deregistered
        sServiceConnector.getCarrierService().getImsRegistration().onDeregistered(
                new ImsReasonInfo(ImsReasonInfo.CODE_LOCAL_NOT_REGISTERED,
                        ImsReasonInfo.CODE_UNSPECIFIED, ""));

        RegistrationManager.RegistrationCallback callback =
                new RegistrationManager.RegistrationCallback() {
                    @Override
                    public void onRegistered(int imsTransportType) {
                        mQueue.offer(imsTransportType);
                    }

                    @Override
                    public void onRegistering(int imsTransportType) {
                        mQueue.offer(imsTransportType);
                    }

                    @Override
                    public void onUnregistered(ImsReasonInfo info) {
                        mQueue.offer(info.getCode());
                    }

                    @Override
                    public void onTechnologyChangeFailed(int imsTransportType, ImsReasonInfo info) {
                        mQueue.offer(imsTransportType);
                        mQueue.offer(info.getCode());
                    }
                };

        ImsMmTelManager mmTelManager = imsManager.getImsMmTelManager(sTestSub);
        ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(mmTelManager,
                (m) -> m.registerImsRegistrationCallback(getContext().getMainExecutor(), callback),
                ImsException.class);
        assertEquals(ImsReasonInfo.CODE_LOCAL_NOT_REGISTERED, waitForIntResult(mQueue));

        // Ensure that the Framework reports Deregistered correctly
        verifyRegistrationState(regManager, RegistrationManager.REGISTRATION_STATE_NOT_REGISTERED);
        verifyRegistrationTransportType(regManager, AccessNetworkConstants.TRANSPORT_TYPE_INVALID);

        // Start registration
        sServiceConnector.getCarrierService().getImsRegistration().onRegistering(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WWAN, waitForIntResult(mQueue));
        verifyRegistrationState(regManager, RegistrationManager.REGISTRATION_STATE_REGISTERING);
        verifyRegistrationTransportType(regManager, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);

        // Complete registration
        sServiceConnector.getCarrierService().getImsRegistration().onRegistered(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WWAN, waitForIntResult(mQueue));
        verifyRegistrationState(regManager, RegistrationManager.REGISTRATION_STATE_REGISTERED);
        verifyRegistrationTransportType(regManager, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);


        // Fail handover to IWLAN
        sServiceConnector.getCarrierService().getImsRegistration().onTechnologyChangeFailed(
                ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN,
                new ImsReasonInfo(ImsReasonInfo.CODE_LOCAL_HO_NOT_FEASIBLE,
                        ImsReasonInfo.CODE_UNSPECIFIED, ""));
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WLAN, waitForIntResult(mQueue));
        assertEquals(ImsReasonInfo.CODE_LOCAL_HO_NOT_FEASIBLE, waitForIntResult(mQueue));
        verifyRegistrationTransportType(regManager, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);

        // handover to IWLAN
        sServiceConnector.getCarrierService().getImsRegistration().onRegistered(
                ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WLAN, waitForIntResult(mQueue));
        verifyRegistrationTransportType(regManager, AccessNetworkConstants.TRANSPORT_TYPE_WLAN);

        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mmTelManager,
                (m) -> m.unregisterImsRegistrationCallback(callback));
    }

    @Ignore("RCS APIs not public yet")
    @Test
    public void testRcsManagerRegistrationState() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        if (imsManager == null) {
            fail("Cannot find IMS service");
        }

        // Connect to device ImsService with RcsFeature
        triggerFrameworkConnectToLocalImsServiceBindRcsFeature();
        sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_RCS_CAP_SET);

        // Start de-registered
        sServiceConnector.getCarrierService().getImsRegistration().onDeregistered(
                new ImsReasonInfo(ImsReasonInfo.CODE_LOCAL_NOT_REGISTERED,
                        ImsReasonInfo.CODE_UNSPECIFIED, ""));

        LinkedBlockingQueue<Integer> mQueue = new LinkedBlockingQueue<>();
        RegistrationManager.RegistrationCallback callback =
                new RegistrationManager.RegistrationCallback() {
                    @Override
                    public void onRegistered(int imsTransportType) {
                        mQueue.offer(imsTransportType);
                    }

                    @Override
                    public void onRegistering(int imsTransportType) {
                        mQueue.offer(imsTransportType);
                    }

                    @Override
                    public void onUnregistered(ImsReasonInfo info) {
                        mQueue.offer(info.getCode());
                    }

                    @Override
                    public void onTechnologyChangeFailed(int imsTransportType, ImsReasonInfo info) {
                        mQueue.offer(imsTransportType);
                        mQueue.offer(info.getCode());
                    }
                };

        ImsRcsManager imsRcsManager = imsManager.getImsRcsManager(sTestSub);
        ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(imsRcsManager,
                (m) -> m.registerImsRegistrationCallback(getContext().getMainExecutor(), callback),
                ImsException.class);
        assertEquals(ImsReasonInfo.CODE_LOCAL_NOT_REGISTERED, waitForIntResult(mQueue));

        // Ensure that the Framework reports Deregistered correctly
        verifyRegistrationState(imsRcsManager,
                RegistrationManager.REGISTRATION_STATE_NOT_REGISTERED);
        verifyRegistrationTransportType(imsRcsManager,
                AccessNetworkConstants.TRANSPORT_TYPE_INVALID);

        // Start registration
        sServiceConnector.getCarrierService().getImsRegistration().onRegistering(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WWAN, waitForIntResult(mQueue));
        verifyRegistrationState(imsRcsManager, RegistrationManager.REGISTRATION_STATE_REGISTERING);
        verifyRegistrationTransportType(imsRcsManager, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);

        // Complete registration
        sServiceConnector.getCarrierService().getImsRegistration().onRegistered(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WWAN, waitForIntResult(mQueue));
        verifyRegistrationState(imsRcsManager, RegistrationManager.REGISTRATION_STATE_REGISTERED);
        verifyRegistrationTransportType(imsRcsManager, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);

        // Fail handover to IWLAN
        sServiceConnector.getCarrierService().getImsRegistration().onTechnologyChangeFailed(
                ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN,
                new ImsReasonInfo(ImsReasonInfo.CODE_LOCAL_HO_NOT_FEASIBLE,
                        ImsReasonInfo.CODE_UNSPECIFIED, ""));
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WLAN, waitForIntResult(mQueue));
        assertEquals(ImsReasonInfo.CODE_LOCAL_HO_NOT_FEASIBLE, waitForIntResult(mQueue));
        verifyRegistrationTransportType(imsRcsManager, AccessNetworkConstants.TRANSPORT_TYPE_WWAN);

        // handover to IWLAN
        sServiceConnector.getCarrierService().getImsRegistration().onRegistered(
                ImsRegistrationImplBase.REGISTRATION_TECH_IWLAN);
        assertEquals(AccessNetworkConstants.TRANSPORT_TYPE_WLAN, waitForIntResult(mQueue));
        verifyRegistrationTransportType(imsRcsManager, AccessNetworkConstants.TRANSPORT_TYPE_WLAN);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(imsRcsManager,
                (m) -> m.unregisterImsRegistrationCallback(callback));
    }

    @Test
    public void testCapabilityStatusCallback() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mmTelManager = imsManager.getImsMmTelManager(sTestSub);

        triggerFrameworkConnectToCarrierImsService();

        // Wait for the framework to set the capabilities on the ImsService
        sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_MMTEL_CAP_SET);
        MmTelFeature.MmTelCapabilities fwCaps = sServiceConnector.getCarrierService()
                .getMmTelFeature().getCapabilities();
        // Make sure we start off with every capability unavailable
        sServiceConnector.getCarrierService().getImsRegistration().onRegistered(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        sServiceConnector.getCarrierService().getMmTelFeature()
                .notifyCapabilitiesStatusChanged(new MmTelFeature.MmTelCapabilities());

        // Make sure the capabilities match the API getter for capabilities
        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        // Latch will count down here (we callback on the state during registration).
        try {
            automan.adoptShellPermissionIdentity();
            // Make sure we are tracking voice capability over LTE properly.
            assertEquals(fwCaps.isCapable(MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE),
                    mmTelManager.isCapable(MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE,
                            ImsRegistrationImplBase.REGISTRATION_TECH_LTE));
        } finally {
            automan.dropShellPermissionIdentity();
        }

        // This is a little bit gross looking, but on P devices, I can not define classes that
        // extend ImsMmTelManager.CapabilityCallback (because it doesn't exist), so this has to
        // happen as an anon class here.
        LinkedBlockingQueue<MmTelFeature.MmTelCapabilities> mQueue = new LinkedBlockingQueue<>();
        ImsMmTelManager.CapabilityCallback callback = new ImsMmTelManager.CapabilityCallback() {

            @Override
            public void onCapabilitiesStatusChanged(MmTelFeature.MmTelCapabilities capabilities) {
                mQueue.offer(capabilities);
            }
        };

        // Latch will count down here (we callback on the state during registration).
        try {
            automan.adoptShellPermissionIdentity();
            mmTelManager.registerMmTelCapabilityCallback(getContext().getMainExecutor(), callback);
        } finally {
            automan.dropShellPermissionIdentity();
        }

        try {
            mmTelManager.registerMmTelCapabilityCallback(getContext().getMainExecutor(), callback);
            fail("registerMmTelCapabilityCallback requires READ_PRECISE_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }

        // We should not have voice availability here, we notified the framework earlier.
        MmTelFeature.MmTelCapabilities capCb = waitForResult(mQueue);
        assertNotNull(capCb);
        assertFalse(capCb.isCapable(MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE));

        // Now enable voice availability
        sServiceConnector.getCarrierService().getMmTelFeature()
                .notifyCapabilitiesStatusChanged(new MmTelFeature.MmTelCapabilities(
                        MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE));
        capCb = waitForResult(mQueue);
        assertNotNull(capCb);
        assertTrue(capCb.isCapable(MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE));

        try {
            automan.adoptShellPermissionIdentity();
            assertTrue(ImsUtils.retryUntilTrue(() -> mmTelManager.isAvailable(
                    MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE,
                    ImsRegistrationImplBase.REGISTRATION_TECH_LTE)));

            mmTelManager.unregisterMmTelCapabilityCallback(callback);
        } finally {
            automan.dropShellPermissionIdentity();
        }

        try {
            mmTelManager.unregisterMmTelCapabilityCallback(callback);
            fail("unregisterMmTelCapabilityCallback requires READ_PRECISE_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }
    }

    /**
     * We are specifically testing a race case here such that IsAvailable returns the correct
     * capability status during the callback.
     */
    @Test
    public void testCapabilityStatusWithIsAvailableDuringCallback() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mmTelManager = imsManager.getImsMmTelManager(sTestSub);

        triggerFrameworkConnectToCarrierImsService();

        // Wait for the framework to set the capabilities on the ImsService
        sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_MMTEL_CAP_SET);


        // Make sure we start off with every capability unavailable
        sServiceConnector.getCarrierService().getImsRegistration().onRegistered(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        MmTelFeature.MmTelCapabilities stdCapabilities = new MmTelFeature.MmTelCapabilities();
        sServiceConnector.getCarrierService().getMmTelFeature()
                .notifyCapabilitiesStatusChanged(stdCapabilities);


        // Make sure the capabilities match the API getter for capabilities
        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();

        //This lock is to keep the shell permissions from being dropped on a different thread
        //causing a permission error.
        Object lockObj = new Object();

        synchronized (lockObj) {
            try {
                automan.adoptShellPermissionIdentity();
                boolean isAvailableBeforeStatusChange = mmTelManager.isAvailable(
                        MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE,
                        ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
                assertFalse(isAvailableBeforeStatusChange);
            } finally {
                automan.dropShellPermissionIdentity();
            }
        }

        LinkedBlockingQueue<Boolean> voiceIsAvailable = new LinkedBlockingQueue<>();
        ImsMmTelManager.CapabilityCallback verifyCapabilityStatusCallaback =
                new ImsMmTelManager.CapabilityCallback() {
            @Override
            public void onCapabilitiesStatusChanged(MmTelFeature.MmTelCapabilities capabilities) {
                synchronized (lockObj) {
                    try {
                        automan.adoptShellPermissionIdentity();
                        boolean isVoiceAvailable = mmTelManager
                                .isAvailable(MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE,
                                        ImsRegistrationImplBase.REGISTRATION_TECH_LTE);

                        voiceIsAvailable.offer(isVoiceAvailable);
                    } finally {
                        automan.dropShellPermissionIdentity();
                    }
                }
            }
        };

        synchronized (lockObj) {
            // Latch will count down here (we callback on the state during registration).
            try {
                automan.adoptShellPermissionIdentity();
                mmTelManager.registerMmTelCapabilityCallback(getContext().getMainExecutor(),
                        verifyCapabilityStatusCallaback);
            } finally {
                automan.dropShellPermissionIdentity();
            }
        }

        // Now enable voice availability
        Boolean isAvailableDuringRegister = waitForResult(voiceIsAvailable);
        assertNotNull(isAvailableDuringRegister);
        assertFalse(isAvailableDuringRegister);
        sServiceConnector.getCarrierService().getMmTelFeature()
                .notifyCapabilitiesStatusChanged(new MmTelFeature.MmTelCapabilities(
                        MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE));
        Boolean isAvailableAfterStatusChange = waitForResult(voiceIsAvailable);
        assertNotNull(isAvailableAfterStatusChange);
        assertTrue(isAvailableAfterStatusChange);

        synchronized (lockObj) {
            try {
                automan.adoptShellPermissionIdentity();
                mmTelManager.unregisterMmTelCapabilityCallback(verifyCapabilityStatusCallaback);
            } finally {
                automan.dropShellPermissionIdentity();
            }
        }
    }

    @Test
    public void testProvisioningManagerNotifyAutoConfig() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        // Trigger carrier config changed
        PersistableBundle bundle = new PersistableBundle();
        bundle.putBoolean(CarrierConfigManager.KEY_USE_RCS_SIP_OPTIONS_BOOL, true);
        bundle.putBoolean(CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL, true);
        overrideCarrierConfig(bundle);

        triggerFrameworkConnectToLocalImsServiceBindRcsFeature();

        ProvisioningManager provisioningManager =
                ProvisioningManager.createForSubscriptionId(sTestSub);

        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            automan.adoptShellPermissionIdentity();
            provisioningManager.notifyRcsAutoConfigurationReceived(
                    TEST_AUTOCONFIG_CONTENT.getBytes(), false);
            ImsConfigImplBase config = sServiceConnector.getCarrierService().getConfig();
            Assert.assertNotNull(config);
            assertEquals(TEST_AUTOCONFIG_CONTENT,
                    config.getConfigString(ImsUtils.ITEM_NON_COMPRESSED));

            provisioningManager.notifyRcsAutoConfigurationReceived(
                    TEST_AUTOCONFIG_CONTENT.getBytes(), true);
            assertEquals(TEST_AUTOCONFIG_CONTENT,
                    config.getConfigString(ImsUtils.ITEM_COMPRESSED));
        } finally {
            automan.dropShellPermissionIdentity();
        }
    }

    @Ignore("RCS APIs not public yet")
    @Test
    public void testRcsCapabilityStatusCallback() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        if (imsManager == null) {
            fail("Cannot find IMS service");
        }

        // Connect to device ImsService with RcsFeature
        triggerFrameworkConnectToLocalImsServiceBindRcsFeature();

        int registrationTech = ImsRegistrationImplBase.REGISTRATION_TECH_LTE;
        ImsRcsManager imsRcsManager = imsManager.getImsRcsManager(sTestSub);

        // Wait for the framework to set the capabilities on the ImsService
        sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_RCS_CAP_SET);
        // Make sure we start off with none-capability
        sServiceConnector.getCarrierService().getImsRegistration().onRegistered(
                ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
        RcsImsCapabilities noCapabilities = new RcsImsCapabilities(RCS_CAP_NONE);
        sServiceConnector.getCarrierService().getRcsFeature()
                .notifyCapabilitiesStatusChanged(noCapabilities);

        // Make sure the capabilities match the API getter for capabilities
        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        // Latch will count down here (we callback on the state during registration).
        try {
            automan.adoptShellPermissionIdentity();
            // Make sure we are tracking voice capability over LTE properly.
            RcsImsCapabilities availability = sServiceConnector.getCarrierService()
                    .getRcsFeature().queryCapabilityStatus();
            assertEquals(availability.isCapable(RCS_CAP_PRESENCE),
                    imsRcsManager.isAvailable(RCS_CAP_PRESENCE));
        } finally {
            automan.dropShellPermissionIdentity();
        }

        // Trigger carrier config changed
        PersistableBundle bundle = new PersistableBundle();
        bundle.putBoolean(CarrierConfigManager.KEY_USE_RCS_SIP_OPTIONS_BOOL, true);
        bundle.putBoolean(CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL, true);
        overrideCarrierConfig(bundle);

        // The carrier config changed should trigger RcsFeature#changeEnabledCapabilities
        try {
            automan.adoptShellPermissionIdentity();
            // Checked by isCapable api to make sure RcsFeature#changeEnabledCapabilities is called
            assertTrue(ImsUtils.retryUntilTrue(() ->
                    imsRcsManager.isCapable(RCS_CAP_OPTIONS, registrationTech)));
            assertTrue(ImsUtils.retryUntilTrue(() ->
                    imsRcsManager.isCapable(RCS_CAP_PRESENCE, registrationTech)));
        } finally {
            automan.dropShellPermissionIdentity();
        }

        // A queue to receive capability changed
        LinkedBlockingQueue<RcsImsCapabilities> mQueue = new LinkedBlockingQueue<>();
        ImsRcsManager.AvailabilityCallback callback = new ImsRcsManager.AvailabilityCallback() {
            @Override
            public void onAvailabilityChanged(RcsImsCapabilities capabilities) {
                mQueue.offer(capabilities);
            }
        };

        // Latch will count down here (we callback on the state during registration).
        try {
            automan.adoptShellPermissionIdentity();
            imsRcsManager.registerRcsAvailabilityCallback(getContext().getMainExecutor(), callback);
        } finally {
            automan.dropShellPermissionIdentity();
        }

        // We should not have any availabilities here, we notified the framework earlier.
        RcsImsCapabilities capCb = waitForResult(mQueue);

        // The SIP OPTIONS capability from onAvailabilityChanged should be disabled.
        // Moreover, ImsRcsManager#isAvailable also return FALSE with SIP OPTIONS
        assertTrue(capCb.isCapable(RCS_CAP_NONE));
        try {
            automan.adoptShellPermissionIdentity();
            assertFalse(imsRcsManager.isAvailable(RCS_CAP_OPTIONS));
        } finally {
            automan.dropShellPermissionIdentity();
        }

        // Notify the SIP OPTIONS capability status changed
        RcsImsCapabilities optionsCap = new RcsImsCapabilities(RCS_CAP_OPTIONS);
        sServiceConnector.getCarrierService().getRcsFeature()
                .notifyCapabilitiesStatusChanged(optionsCap);
        capCb = waitForResult(mQueue);

        // The SIP OPTIONS capability from onAvailabilityChanged should be enabled.
        // Verify ImsRcsManager#isAvailable also return true with SIP OPTIONS
        assertTrue(capCb.isCapable(RCS_CAP_OPTIONS));
        try {
            automan.adoptShellPermissionIdentity();
            assertTrue(imsRcsManager.isAvailable(RCS_CAP_OPTIONS));
        } finally {
            automan.dropShellPermissionIdentity();
        }

        overrideCarrierConfig(null);
    }

    @Test
    public void testProvisioningManagerSetConfig() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        triggerFrameworkConnectToCarrierImsService();

        ProvisioningManager provisioningManager =
                ProvisioningManager.createForSubscriptionId(sTestSub);

        // This is a little bit gross looking, but on P devices, I can not define classes that
        // extend ProvisioningManager.Callback (because it doesn't exist), so this has to
        // happen as an anon class here.
        LinkedBlockingQueue<Pair<Integer, Integer>> mIntQueue = new LinkedBlockingQueue<>();
        LinkedBlockingQueue<Pair<Integer, String>> mStringQueue = new LinkedBlockingQueue<>();
        ProvisioningManager.Callback callback = new ProvisioningManager.Callback() {
            @Override
            public void onProvisioningIntChanged(int item, int value) {
                mIntQueue.offer(new Pair<>(item, value));
            }

            @Override
            public void onProvisioningStringChanged(int item, String value) {
                mStringQueue.offer(new Pair<>(item, value));
            }
        };

        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            automan.adoptShellPermissionIdentity();
            provisioningManager.registerProvisioningChangedCallback(getContext().getMainExecutor(),
                    callback);

            provisioningManager.setProvisioningIntValue(TEST_CONFIG_KEY, TEST_CONFIG_VALUE_INT);
            assertTrue(waitForParam(mIntQueue, new Pair<>(TEST_CONFIG_KEY, TEST_CONFIG_VALUE_INT)));
            assertEquals(TEST_CONFIG_VALUE_INT,
                    provisioningManager.getProvisioningIntValue(TEST_CONFIG_KEY));

            provisioningManager.setProvisioningStringValue(TEST_CONFIG_KEY,
                    TEST_CONFIG_VALUE_STRING);
            assertTrue(waitForParam(mStringQueue,
                    new Pair<>(TEST_CONFIG_KEY, TEST_CONFIG_VALUE_STRING)));
            assertEquals(TEST_CONFIG_VALUE_STRING,
                    provisioningManager.getProvisioningStringValue(TEST_CONFIG_KEY));

            automan.adoptShellPermissionIdentity();
            provisioningManager.unregisterProvisioningChangedCallback(callback);
        } finally {
            automan.dropShellPermissionIdentity();
        }
    }

    @Ignore("The ProvisioningManager constants were moved back to @hide for now, don't want to "
            + "completely remove test.")
    @Test
    public void testProvisioningManagerConstants() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        triggerFrameworkConnectToCarrierImsService();

        ProvisioningManager provisioningManager =
                ProvisioningManager.createForSubscriptionId(sTestSub);

        // This is a little bit gross looking, but on P devices, I can not define classes that
        // extend ProvisioningManager.Callback (because it doesn't exist), so this has to
        // happen as an anon class here.
        LinkedBlockingQueue<Pair<Integer, Integer>> mIntQueue = new LinkedBlockingQueue<>();
        LinkedBlockingQueue<Pair<Integer, String>> mStringQueue = new LinkedBlockingQueue<>();
        ProvisioningManager.Callback callback = new ProvisioningManager.Callback() {
            @Override
            public void onProvisioningIntChanged(int item, int value) {
                mIntQueue.offer(new Pair<>(item, value));
            }

            @Override
            public void onProvisioningStringChanged(int item, String value) {
                mStringQueue.offer(new Pair<>(item, value));
            }
        };

        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            automan.adoptShellPermissionIdentity();
            provisioningManager.registerProvisioningChangedCallback(getContext().getMainExecutor(),
                    callback);

            verifyStringKey(provisioningManager, mStringQueue,
                    ProvisioningManager.KEY_AMR_CODEC_MODE_SET_VALUES, "1,2");
            verifyStringKey(provisioningManager, mStringQueue,
                    ProvisioningManager.KEY_AMR_WB_CODEC_MODE_SET_VALUES, "1,2");
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_SESSION_TIMER_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_MINIMUM_SIP_SESSION_EXPIRATION_TIMER_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_INVITE_CANCELLATION_TIMER_MS, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_TRANSITION_TO_LTE_DELAY_MS, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_ENABLE_SILENT_REDIAL, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_T1_TIMER_VALUE_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_T2_TIMER_VALUE_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_TF_TIMER_VALUE_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_VOLTE_PROVISIONING_STATUS, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_VT_PROVISIONING_STATUS, 0);
            verifyStringKey(provisioningManager, mStringQueue,
                    ProvisioningManager.KEY_REGISTRATION_DOMAIN_NAME, "test.com");
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SMS_FORMAT, ProvisioningManager.SMS_FORMAT_3GPP);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SMS_FORMAT, ProvisioningManager.SMS_FORMAT_3GPP2);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SMS_OVER_IP_ENABLED, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_PUBLISH_TIMER_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_PUBLISH_OFFLINE_AVAILABILITY_TIMER_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_CAPABILITY_DISCOVERY_ENABLED, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_CAPABILITIES_CACHE_EXPIRATION_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_CAPABILITIES_CACHE_EXPIRATION_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_AVAILABILITY_CACHE_EXPIRATION_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_CAPABILITIES_POLL_INTERVAL_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_PUBLISH_SOURCE_THROTTLE_MS, 1000);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_MAX_NUM_ENTRIES_IN_RCL, 50);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RCS_CAPABILITY_POLL_LIST_SUB_EXP_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_USE_GZIP_FOR_LIST_SUBSCRIPTION, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_EAB_PROVISIONING_STATUS, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_VOICE_OVER_WIFI_ROAMING_ENABLED_OVERRIDE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_VOICE_OVER_WIFI_MODE_OVERRIDE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_VOICE_OVER_WIFI_ENABLED_OVERRIDE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_MOBILE_DATA_ENABLED, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_VOLTE_USER_OPT_IN_STATUS, 0);
            verifyStringKey(provisioningManager, mStringQueue,
                    ProvisioningManager.KEY_LOCAL_BREAKOUT_PCSCF_ADDRESS, "local.fun.com");
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_KEEP_ALIVE_ENABLED, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_REGISTRATION_RETRY_BASE_TIME_SEC, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_REGISTRATION_RETRY_MAX_TIME_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RTP_SPEECH_START_PORT, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RTP_SPEECH_END_PORT, 600);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_INVITE_REQUEST_TRANSMIT_INTERVAL_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_INVITE_ACK_WAIT_TIME_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_INVITE_RESPONSE_RETRANSMIT_WAIT_TIME_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_NON_INVITE_TRANSACTION_TIMEOUT_TIMER_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_INVITE_RESPONSE_RETRANSMIT_INTERVAL_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_ACK_RECEIPT_WAIT_TIME_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_ACK_RETRANSMIT_WAIT_TIME_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_NON_INVITE_REQUEST_RETRANSMISSION_WAIT_TIME_MS, 500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SIP_NON_INVITE_RESPONSE_RETRANSMISSION_WAIT_TIME_MS,
                    500);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_AMR_WB_OCTET_ALIGNED_PAYLOAD_TYPE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_AMR_WB_BANDWIDTH_EFFICIENT_PAYLOAD_TYPE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_AMR_OCTET_ALIGNED_PAYLOAD_TYPE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_AMR_BANDWIDTH_EFFICIENT_PAYLOAD_TYPE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_DTMF_WB_PAYLOAD_TYPE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_DTMF_NB_PAYLOAD_TYPE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_AMR_DEFAULT_ENCODING_MODE, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_SMS_PUBLIC_SERVICE_IDENTITY, 0);
            verifyStringKey(provisioningManager, mStringQueue,
                    ProvisioningManager.KEY_SMS_PUBLIC_SERVICE_IDENTITY, "local.fun.com");
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_VIDEO_QUALITY, ProvisioningManager.VIDEO_QUALITY_HIGH);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_VIDEO_QUALITY, ProvisioningManager.VIDEO_QUALITY_LOW);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_LTE_THRESHOLD_1, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_LTE_THRESHOLD_2, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_LTE_THRESHOLD_3, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_1X_THRESHOLD, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_WIFI_THRESHOLD_A, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_WIFI_THRESHOLD_B, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_LTE_EPDG_TIMER_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_WIFI_EPDG_TIMER_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_1X_EPDG_TIMER_SEC, 5);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_MULTIENDPOINT_ENABLED, 0);
            verifyIntKey(provisioningManager, mIntQueue,
                    ProvisioningManager.KEY_RTT_ENABLED, 0);

            automan.adoptShellPermissionIdentity();
            provisioningManager.unregisterProvisioningChangedCallback(callback);
        } finally {
            automan.dropShellPermissionIdentity();
        }
    }

    @Test
    public void testProvisioningManagerProvisioningCaps() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        triggerFrameworkConnectToCarrierImsService();

        PersistableBundle bundle = new PersistableBundle();
        bundle.putBoolean(CarrierConfigManager.KEY_CARRIER_SUPPORTS_SS_OVER_UT_BOOL, true);
        bundle.putBoolean(CarrierConfigManager.KEY_CARRIER_UT_PROVISIONING_REQUIRED_BOOL, true);
        overrideCarrierConfig(bundle);

        ProvisioningManager provisioningManager =
                ProvisioningManager.createForSubscriptionId(sTestSub);

        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            automan.adoptShellPermissionIdentity();
            boolean provisioningStatus = provisioningManager.getProvisioningStatusForCapability(
                    MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_UT,
                    ImsRegistrationImplBase.REGISTRATION_TECH_LTE);
            provisioningManager.setProvisioningStatusForCapability(
                    MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_UT,
                    ImsRegistrationImplBase.REGISTRATION_TECH_LTE, !provisioningStatus);
            // Make sure the change in provisioning status is correctly returned.
            assertEquals(!provisioningStatus,
                    provisioningManager.getProvisioningStatusForCapability(
                            MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_UT,
                            ImsRegistrationImplBase.REGISTRATION_TECH_LTE));
            // TODO: Enhance test to make sure the provisioning change is also sent to the
            // ImsService

            // set back to current status
            provisioningManager.setProvisioningStatusForCapability(
                    MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_UT,
                    ImsRegistrationImplBase.REGISTRATION_TECH_LTE, provisioningStatus);
        } finally {
            automan.dropShellPermissionIdentity();
        }

        overrideCarrierConfig(null);
    }

    private void verifyIntKey(ProvisioningManager pm,
            LinkedBlockingQueue<Pair<Integer, Integer>> intQueue, int key, int value)
            throws Exception {
        pm.setProvisioningIntValue(key, value);
        assertTrue(waitForParam(intQueue, new Pair<>(key, value)));
        assertEquals(value, pm.getProvisioningIntValue(key));
    }

    private void verifyStringKey(ProvisioningManager pm,
            LinkedBlockingQueue<Pair<Integer, String>> strQueue, int key, String value)
            throws Exception {
        pm.setProvisioningStringValue(key, value);
        assertTrue(waitForParam(strQueue, new Pair<>(key, value)));
        assertEquals(value, pm.getProvisioningStringValue(key));
    }

    private void setupImsServiceForSms() throws Exception {
        MmTelFeature.MmTelCapabilities capabilities = new MmTelFeature.MmTelCapabilities(
                MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_SMS);
        // Set up MMTEL
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                .build()));
        // Wait until MMTEL is created and onFeatureReady is called
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_CREATE_MMTEL));
        assertTrue(sServiceConnector.getCarrierService().waitForLatchCountdown(
                TestImsService.LATCH_MMTEL_READY));
        int serviceSlot = sServiceConnector.getCarrierService().getMmTelFeature().getSlotIndex();
        assertEquals("The slot specified for the test (" + sTestSlot + ") does not match the "
                        + "assigned slot (" + serviceSlot + "+ for the associated MmTelFeature",
                sTestSlot, serviceSlot);
        // Wait until ImsSmsDispatcher connects and calls onReady.
        assertTrue(sServiceConnector.getCarrierService().getMmTelFeature().getSmsImplementation()
                .waitForOnReadyLatch());
        // Set Registered and SMS capable
        sServiceConnector.getCarrierService().getMmTelFeature().setCapabilities(capabilities);
        sServiceConnector.getCarrierService().getImsService().getRegistration(0).onRegistered(1);
        sServiceConnector.getCarrierService().getMmTelFeature()
                .notifyCapabilitiesStatusChanged(capabilities);

        // Wait a second for the notifyCapabilitiesStatusChanged indication to be processed on the
        // main telephony thread - currently no better way of knowing that telephony has processed
        // this command. SmsManager#isImsSmsSupported() is @hide and must be updated to use new API.
        Thread.sleep(1000);
    }

    private void triggerFrameworkConnectToLocalImsServiceBindRcsFeature() throws Exception {
        // Connect to the ImsService with the RCS feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_RCS)
                .build()));
        // The RcsFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        assertTrue("Did not receive createRcsFeature", sServiceConnector.getCarrierService()
                .waitForLatchCountdown(TestImsService.LATCH_CREATE_RCS));
        assertTrue("Did not receive RcsFeature#onReady", sServiceConnector.getCarrierService()
                .waitForLatchCountdown(TestImsService.LATCH_RCS_READY));
        // Make sure the RcsFeature was created in the test service.
        assertNotNull("Device ImsService created, but TestDeviceImsService#createRcsFeature was not"
                + "called!", sServiceConnector.getCarrierService().getRcsFeature());
        int serviceSlot = sServiceConnector.getCarrierService().getRcsFeature().getSlotIndex();
        assertEquals("The slot specified for the test (" + sTestSlot + ") does not match the "
                        + "assigned slot (" + serviceSlot + "+ for the associated RcsFeature",
                sTestSlot, serviceSlot);
    }

    private void triggerFrameworkConnectToCarrierImsService() throws Exception {
        // Connect to the ImsService with the MmTel feature.
        assertTrue(sServiceConnector.connectCarrierImsService(new ImsFeatureConfiguration.Builder()
                .addFeature(sTestSlot, ImsFeature.FEATURE_MMTEL)
                .build()));
        // The MmTelFeature is created when the ImsService is bound. If it wasn't created, then the
        // Framework did not call it.
        assertTrue("Did not receive createMmTelFeature", sServiceConnector.getCarrierService()
                .waitForLatchCountdown(TestImsService.LATCH_CREATE_MMTEL));
        assertTrue("Did not receive MmTelFeature#onReady", sServiceConnector.getCarrierService()
                .waitForLatchCountdown(TestImsService.LATCH_MMTEL_READY));
        assertNotNull("ImsService created, but ImsService#createMmTelFeature was not called!",
                sServiceConnector.getCarrierService().getMmTelFeature());
        int serviceSlot = sServiceConnector.getCarrierService().getMmTelFeature().getSlotIndex();
        assertEquals("The slot specified for the test (" + sTestSlot + ") does not match the "
                        + "assigned slot (" + serviceSlot + "+ for the associated MmTelFeature",
                sTestSlot, serviceSlot);
    }

    // Waiting for ImsRcsManager to become public before implementing RegistrationManager,
    // Duplicate these methods for now.
    private void verifyRegistrationState(ImsRcsManager regManager, int expectedState)
            throws Exception {
        LinkedBlockingQueue<Integer> mQueue = new LinkedBlockingQueue<>();
        assertTrue(ImsUtils.retryUntilTrue(() -> {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(regManager,
                    (m) -> m.getRegistrationState(getContext().getMainExecutor(), mQueue::offer));
            return waitForIntResult(mQueue) == expectedState;
        }));
    }

    // Waiting for ImsRcsManager to become public before implementing RegistrationManager,
    // Duplicate these methods for now.
    private void verifyRegistrationTransportType(ImsRcsManager regManager,
            int expectedTransportType) throws Exception {
        LinkedBlockingQueue<Integer> mQueue = new LinkedBlockingQueue<>();
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(regManager,
                (m) -> m.getRegistrationTransportType(getContext().getMainExecutor(),
                        mQueue::offer));
        assertEquals(expectedTransportType, waitForIntResult(mQueue));
    }

    private void verifyRegistrationState(RegistrationManager regManager, int expectedState)
            throws Exception {
        LinkedBlockingQueue<Integer> mQueue = new LinkedBlockingQueue<>();
        assertTrue(ImsUtils.retryUntilTrue(() -> {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(regManager,
                    (m) -> m.getRegistrationState(getContext().getMainExecutor(), mQueue::offer));
            return waitForIntResult(mQueue) == expectedState;
        }));
    }

    private void verifyRegistrationTransportType(RegistrationManager regManager,
            int expectedTransportType) throws Exception {
        LinkedBlockingQueue<Integer> mQueue = new LinkedBlockingQueue<>();
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(regManager,
                (m) -> m.getRegistrationTransportType(getContext().getMainExecutor(),
                        mQueue::offer));
        assertEquals(expectedTransportType, waitForIntResult(mQueue));
    }

    private <T> boolean waitForParam(LinkedBlockingQueue<T> queue, T waitParam) throws Exception {
        T result;
        while ((result = waitForResult(queue)) != null) {
            if (waitParam.equals(result)) {
                return true;
            }
        }
        return false;
    }

    private <T> T waitForResult(LinkedBlockingQueue<T> queue) throws Exception {
        return queue.poll(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private int waitForIntResult(LinkedBlockingQueue<Integer> queue) throws Exception {
        Integer result = queue.poll(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        return result != null ? result : Integer.MAX_VALUE;
    }

    private static void overrideCarrierConfig(PersistableBundle bundle) throws Exception {
        CarrierConfigManager carrierConfigManager = InstrumentationRegistry.getInstrumentation()
                .getContext().getSystemService(CarrierConfigManager.class);
        sReceiver.clearQueue();
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(carrierConfigManager,
                (m) -> m.overrideConfig(sTestSub, bundle));
        sReceiver.waitForCarrierConfigChanged();
    }

    private static Context getContext() {
        return InstrumentationRegistry.getInstrumentation().getContext();
    }

    // Copied from com.android.internal.util.HexDump
    private static byte[] hexStringToByteArray(String hexString) {
        int length = hexString.length();
        byte[] buffer = new byte[length / 2];

        for (int i = 0; i < length; i += 2) {
            buffer[i / 2] =
                    (byte) ((toByte(hexString.charAt(i)) << 4) | toByte(hexString.charAt(i + 1)));
        }

        return buffer;
    }

    // Copied from com.android.internal.util.HexDump
    private static int toByte(char c) {
        if (c >= '0' && c <= '9') return (c - '0');
        if (c >= 'A' && c <= 'F') return (c - 'A' + 10);
        if (c >= 'a' && c <= 'f') return (c - 'a' + 10);

        throw new RuntimeException("Invalid hex char '" + c + "'");
    }
}
